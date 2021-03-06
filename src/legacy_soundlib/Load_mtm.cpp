/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *                  OpenMPT Devs
 *
*/

#include "stdafx.h"
#include "Loaders.h"

//////////////////////////////////////////////////////////
// MTM file support (import only)

#pragma pack(1)

typedef struct tagMTMSAMPLE
{
    char   samplename[22];
    uint32_t length;
    uint32_t reppos;
    uint32_t repend;
    int8_t   finetune;
    uint8_t  volume;
    uint8_t  attribute;
} MTMSAMPLE;


typedef struct tagMTMHEADER
{
    char   id[3];                    // MTM file marker
    uint8_t  version;                    // Tracker version
    char   songname[20];    // ASCIIZ songname
    uint16_t numtracks;            // number of tracks saved
    uint8_t  lastpattern;            // last pattern number saved
    uint8_t  lastorder;            // last order number to play (songlength-1)
    uint16_t commentsize;            // length of comment field
    uint8_t  numsamples;            // number of samples saved
    uint8_t  attribute;            // attribute byte (unused)
    uint8_t  beatspertrack;    // numbers of rows in every pattern
    uint8_t  numchannels;            // number of channels used
    uint8_t  panpos[32];            // channel pan positions
} MTMHEADER;


#pragma pack()


bool module_renderer::ReadMTM(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
    uint32_t dwMemPos = 66;

    if ((!lpStream) || (dwMemLength < 0x100)) return false;

    MTMHEADER *pmh = (MTMHEADER *)lpStream;
    if ((memcmp(pmh->id, "MTM", 3)) || (pmh->numchannels > 32)
     || (pmh->numsamples >= MAX_SAMPLES) || (!pmh->numsamples)
     || (!pmh->numtracks) || (!pmh->numchannels)
     || (!pmh->lastpattern) || (pmh->lastpattern > MAX_PATTERNS)) return false;
    assign_without_padding(this->song_name, pmh->songname, 20);

    if (dwMemPos + 37 * pmh->numsamples + 128 + 192 * pmh->numtracks
     + 64 * (pmh->lastpattern+1) + pmh->commentsize >= dwMemLength) return false;
    m_nType = MOD_TYPE_MTM;
    m_nSamples = pmh->numsamples;
    m_nChannels = pmh->numchannels;
    // Reading instruments
    for    (modplug::tracker::sampleindex_t i = 1; i <= m_nSamples; i++)
    {
        MTMSAMPLE *pms = (MTMSAMPLE *)(lpStream + dwMemPos);
        memcpy(m_szNames[i], pms->samplename, 22);
        SpaceToNullStringFixed<22>(m_szNames[i]);
        Samples[i].default_volume = pms->volume << 2;
        Samples[i].global_volume = 64;
        UINT len = pms->length;
        if ((len > 2) && (len <= MAX_SAMPLE_LENGTH))
        {
            Samples[i].length = len;
            Samples[i].loop_start = pms->reppos;
            Samples[i].loop_end = pms->repend;
            if (Samples[i].loop_end > Samples[i].length) Samples[i].loop_end = Samples[i].length;
            if (Samples[i].loop_start + 4 >= Samples[i].loop_end) Samples[i].loop_start = Samples[i].loop_end = 0;
            if (Samples[i].loop_end) Samples[i].flags |= CHN_LOOP;
            Samples[i].nFineTune = MOD2XMFineTune(pms->finetune);
            if (pms->attribute & 0x01)
            {
                Samples[i].flags |= CHN_16BIT;
                Samples[i].length >>= 1;
                Samples[i].loop_start >>= 1;
                Samples[i].loop_end >>= 1;
            }
            Samples[i].default_pan = 128;
            Samples[i].c5_samplerate = TransposeToFrequency(0, Samples[i].nFineTune);
        }
        dwMemPos += 37;
    }
    // Setting Channel Pan Position
    for (modplug::tracker::chnindex_t ich = 0; ich < m_nChannels; ich++)
    {
        ChnSettings[ich].nPan = ((pmh->panpos[ich] & 0x0F) << 4) + 8;
        ChnSettings[ich].nVolume = 64;
    }
    // Reading pattern order
    Order.ReadAsByte(lpStream + dwMemPos, pmh->lastorder + 1, dwMemLength - dwMemPos);
    dwMemPos += 128;
    // Reading Patterns
    modplug::tracker::rowindex_t nPatRows = CLAMP(pmh->beatspertrack, 1, MAX_PATTERN_ROWS);
    const uint8_t * pTracks = lpStream + dwMemPos;
    dwMemPos += 192 * pmh->numtracks;
    LPWORD pSeq = (LPWORD)(lpStream + dwMemPos);
    for (modplug::tracker::patternindex_t pat = 0; pat <= pmh->lastpattern; pat++)
    {
        if(Patterns.Insert(pat, nPatRows)) break;
        for (UINT n=0; n<32; n++) if ((pSeq[n]) && (pSeq[n] <= pmh->numtracks) && (n < m_nChannels))
        {
            const uint8_t * p = pTracks + 192 * (pSeq[n]-1);
            modplug::tracker::modevent_t *m = Patterns[pat] + n;
            for (UINT i = 0; i < nPatRows; i++, m += m_nChannels, p += 3)
            {
                if (p[0] & 0xFC) m->note = (p[0] >> 2) + 37;
                m->instr = ((p[0] & 0x03) << 4) | (p[1] >> 4);
                uint8_t cmd = p[1] & 0x0F;
                uint8_t param = p[2];
                if (cmd == 0x0A)
                {
                    if (param & 0xF0) param &= 0xF0; else param &= 0x0F;
                }
                //XXXih: gross!
                m->command = (modplug::tracker::cmd_t) cmd;
                m->param = param;
                if ((cmd) || (param))
                {
                    ConvertModCommand(m);
                    ConvertCommand(m, MOD_TYPE_MOD, MOD_TYPE_S3M);
                }
            }
        }
        pSeq += 32;
    }
    dwMemPos += 64 * (pmh->lastpattern + 1);
    if ((pmh->commentsize) && (dwMemPos + pmh->commentsize < dwMemLength))
    {
        UINT n = pmh->commentsize;
        ReadFixedLineLengthMessage(lpStream + dwMemPos, n, 39, 1);
    }
    dwMemPos += pmh->commentsize;
    // Reading Samples
    for (UINT ismp=1; ismp<=m_nSamples; ismp++)
    {
        if (dwMemPos >= dwMemLength) break;
        dwMemPos += ReadSample(&Samples[ismp], (Samples[ismp].flags & CHN_16BIT) ? RS_PCM16U : RS_PCM8U,
                                (LPSTR)(lpStream + dwMemPos), dwMemLength - dwMemPos);
    }
    m_nMinPeriod = 64;
    m_nMaxPeriod = 32767;
    return true;
}