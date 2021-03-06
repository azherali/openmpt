/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 *                    OpenMPT dev(s)        (miscellaneous modifications)
*/

//////////////////////////////////////////////
// PTM PolyTracker module loader            //
//////////////////////////////////////////////
#include "stdafx.h"
#include "Loaders.h"

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

#pragma pack(1)

typedef struct PTMFILEHEADER
{
    CHAR songname[28];            // name of song, asciiz string
    CHAR eof;                            // 26
    uint8_t version_lo;            // 03 version of file, currently 0203h
    uint8_t version_hi;            // 02
    uint8_t reserved1;                    // reserved, set to 0
    uint16_t norders;                    // number of orders (0..256)
    uint16_t nsamples;                    // number of instruments (1..255)
    uint16_t npatterns;                    // number of patterns (1..128)
    uint16_t nchannels;                    // number of channels (voices) used (1..32)
    uint16_t fileflags;                    // set to 0
    uint16_t reserved2;                    // reserved, set to 0
    uint32_t ptmf_id;                    // song identification, 'PTMF' or 0x464d5450
    uint8_t reserved3[16];            // reserved, set to 0
    uint8_t chnpan[32];            // channel panning settings, 0..15, 0 = left, 7 = middle, 15 = right
    uint8_t orders[256];            // order list, valid entries 0..nOrders-1
    uint16_t patseg[128];            // pattern offsets (*16)
} PTMFILEHEADER, *LPPTMFILEHEADER;

#define SIZEOF_PTMFILEHEADER    608


typedef struct PTMSAMPLE
{
    uint8_t sampletype;            // sample type (bit array)
    CHAR filename[12];            // name of external sample file
    uint8_t volume;                    // default volume
    uint16_t nC4Spd;                    // C4 speed
    uint16_t sampleseg;                    // sample segment (used internally)
    uint16_t fileofs[2];            // offset of sample data
    uint16_t length[2];                    // sample size (in bytes)
    uint16_t loopbeg[2];            // start of loop
    uint16_t loopend[2];            // end of loop
    uint16_t gusdata[8];
    char  samplename[28];    // name of sample, asciiz
    uint32_t ptms_id;                    // sample identification, 'PTMS' or 0x534d5450
} PTMSAMPLE;

#define SIZEOF_PTMSAMPLE    80

#pragma pack()


bool module_renderer::ReadPTM(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    if(lpStream == nullptr || dwMemLength < sizeof(PTMFILEHEADER))
        return false;

    PTMFILEHEADER pfh = *(LPPTMFILEHEADER)lpStream;
    uint32_t dwMemPos;
    UINT nOrders;

    pfh.norders = LittleEndianW(pfh.norders);
    pfh.nsamples = LittleEndianW(pfh.nsamples);
    pfh.npatterns = LittleEndianW(pfh.npatterns);
    pfh.nchannels = LittleEndianW(pfh.nchannels);
    pfh.fileflags = LittleEndianW(pfh.fileflags);
    pfh.reserved2 = LittleEndianW(pfh.reserved2);
    pfh.ptmf_id = LittleEndian(pfh.ptmf_id);
    for (UINT j = 0; j < 128; j++)
    {
        pfh.patseg[j] = LittleEndianW(pfh.patseg[j]);
    }

    if ((pfh.ptmf_id != 0x464d5450) || (!pfh.nchannels)
     || (pfh.nchannels > 32)
     || (pfh.norders > 256) || (!pfh.norders)
     || (!pfh.nsamples) || (pfh.nsamples > 255)
     || (!pfh.npatterns) || (pfh.npatterns > 128)
     || (SIZEOF_PTMFILEHEADER+pfh.nsamples*SIZEOF_PTMSAMPLE >= (int)dwMemLength)) return false;
    assign_without_padding(this->song_name, pfh.songname, 28);

    m_nType = MOD_TYPE_PTM;
    m_nChannels = pfh.nchannels;
    m_nSamples = (pfh.nsamples < MAX_SAMPLES) ? pfh.nsamples : MAX_SAMPLES-1;
    dwMemPos = SIZEOF_PTMFILEHEADER;
    nOrders = (pfh.norders < MAX_ORDERS) ? pfh.norders : MAX_ORDERS-1;
    Order.ReadAsByte(pfh.orders, nOrders, nOrders);

    for (modplug::tracker::chnindex_t ipan = 0; ipan < m_nChannels; ipan++)
    {
        ChnSettings[ipan].nVolume = 64;
        ChnSettings[ipan].nPan = ((pfh.chnpan[ipan] & 0x0F) << 4) + 4;
    }
    for (modplug::tracker::sampleindex_t ismp = 0; ismp < m_nSamples; ismp++, dwMemPos += SIZEOF_PTMSAMPLE)
    {
        modsample_t *pSmp = &Samples[ismp+1];
        PTMSAMPLE *psmp = (PTMSAMPLE *)(lpStream+dwMemPos);

        lstrcpyn(m_szNames[ismp+1], psmp->samplename, 28);
        memcpy(pSmp->legacy_filename, psmp->filename, 12);
        SpaceToNullStringFixed<28>(m_szNames[ismp + 1]);
        SpaceToNullStringFixed<12>(pSmp->legacy_filename);

        pSmp->global_volume = 64;
        pSmp->default_pan = 128;
        pSmp->default_volume = psmp->volume << 2;
        pSmp->c5_samplerate = LittleEndianW(psmp->nC4Spd) << 1;
        pSmp->flags = 0;
        if ((psmp->sampletype & 3) == 1)
        {
            UINT smpflg = RS_PCM8D;
            uint32_t samplepos;
            pSmp->length = LittleEndian(*(LPDWORD)(psmp->length));
            pSmp->loop_start = LittleEndian(*(LPDWORD)(psmp->loopbeg));
            pSmp->loop_end = LittleEndian(*(LPDWORD)(psmp->loopend));
            samplepos = LittleEndian(*(LPDWORD)(&psmp->fileofs));
            if (psmp->sampletype & 4) pSmp->flags |= CHN_LOOP;
            if (psmp->sampletype & 8) pSmp->flags |= CHN_PINGPONGLOOP;
            if (psmp->sampletype & 16)
            {
                pSmp->flags |= CHN_16BIT;
                pSmp->length >>= 1;
                pSmp->loop_start >>= 1;
                pSmp->loop_end >>= 1;
                smpflg = RS_PTM8DTO16;
            }
            if ((pSmp->length) && (samplepos) && (samplepos < dwMemLength))
            {
                ReadSample(pSmp, smpflg, (LPSTR)(lpStream+samplepos), dwMemLength-samplepos);
            }
        }
    }
    // Reading Patterns
    for (UINT ipat=0; ipat<pfh.npatterns; ipat++)
    {
        dwMemPos = ((UINT)pfh.patseg[ipat]) << 4;
        if ((!dwMemPos) || (dwMemPos >= dwMemLength)) continue;
        if(Patterns.Insert(ipat, 64))
            break;
        //
        modplug::tracker::modevent_t *m = Patterns[ipat];
        for (UINT row=0; ((row < 64) && (dwMemPos < dwMemLength)); )
        {
            UINT b = lpStream[dwMemPos++];

            if (dwMemPos >= dwMemLength) break;
            if (b)
            {
                UINT nChn = b & 0x1F;

                if (b & 0x20)
                {
                    if (dwMemPos + 2 > dwMemLength) break;
                    m[nChn].note = lpStream[dwMemPos++];
                    m[nChn].instr = lpStream[dwMemPos++];
                }
                if (b & 0x40)
                {
                    if (dwMemPos + 2 > dwMemLength) break;
                    //XXXih: gross
                    m[nChn].command = (modplug::tracker::cmd_t) lpStream[dwMemPos++];
                    m[nChn].param = lpStream[dwMemPos++];
                    if (m[nChn].command < 0x10)
                    {
                        ConvertModCommand(&m[nChn]);
                        MODExx2S3MSxx(&m[nChn]);
                        // Note cut does just mute the sample, not cut it. We have to fix that, if possible.
                        if(m[nChn].command == CmdS3mCmdEx && (m[nChn].param & 0xF0) == 0xC0 && m[nChn].volcmd == VolCmdNone)
                        {
                            // SCx => v00 + SDx
                            // This is a pretty dumb solution because many (?) PTM files make usage of the volume column + note cut at the same time.
                            m[nChn].param = 0xD0 | (m[nChn].param & 0x0F);
                            m[nChn].volcmd = VolCmdVol;
                            m[nChn].vol = 0;
                        }
                    } else
                    {
                        switch(m[nChn].command)
                        {
                        case 16:
                            m[nChn].command = CmdGlobalVol;
                            break;
                        case 17:
                            m[nChn].command = CmdRetrig;
                            break;
                        case 18:
                            m[nChn].command = CmdFineVibrato;
                            break;
                        default:
                            m[nChn].command = CmdNone;
                        }
                    }
                }
                if (b & 0x80)
                {
                    if (dwMemPos >= dwMemLength) break;
                    m[nChn].volcmd = VolCmdVol;
                    m[nChn].vol = lpStream[dwMemPos++];
                }
            } else
            {
                row++;
                m += m_nChannels;
            }
        }
    }
    return true;
}