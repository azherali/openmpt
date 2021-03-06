/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

//////////////////////////////////////////////
// DigiTracker (MDL) module loader          //
//////////////////////////////////////////////
#include "stdafx.h"
#include "Loaders.h"

//#define MDL_LOG

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

typedef struct MDLSONGHEADER
{
    uint32_t id;    // "DMDL" = 0x4C444D44
    uint8_t version;
} MDLSONGHEADER;


typedef struct MDLINFOBLOCK
{
    CHAR songname[32];
    CHAR composer[20];
    uint16_t norders;
    uint16_t repeatpos;
    uint8_t globalvol;
    uint8_t speed;
    uint8_t tempo;
    uint8_t channelinfo[32];
    uint8_t seq[256];
} MDLINFOBLOCK;


typedef struct MDLPATTERNDATA
{
    uint8_t channels;
    uint8_t lastrow;    // nrows = lastrow+1
    CHAR name[16];
    uint16_t data[1];
} MDLPATTERNDATA;


void ConvertMDLCommand(modplug::tracker::modevent_t *m, UINT eff, UINT data)
//--------------------------------------------------------
{
    UINT command = 0, param = data;
    switch(eff)
    {
    case 0x01:    command = CmdPortaUp; break;
    case 0x02:    command = CmdPortaDown; break;
    case 0x03:    command = CmdPorta; break;
    case 0x04:    command = CmdVibrato; break;
    case 0x05:    command = CmdArpeggio; break;
    case 0x07:    command = (param < 0x20) ? CmdSpeed : CmdTempo; break;
    case 0x08:    command = CmdPanning8; param <<= 1; break;
    case 0x0B:    command = CmdPositionJump; break;
    case 0x0C:    command = CmdGlobalVol; break;
    case 0x0D:    command = CmdPatternBreak; param = (data & 0x0F) + (data>>4)*10; break;
    case 0x0E:
        command = CmdS3mCmdEx;
        switch(data & 0xF0)
        {
        case 0x00:    command = 0; break; // What is E0x in MDL (there is a bunch) ?
        case 0x10:    if (param & 0x0F) { param |= 0xF0; command = CmdPanningSlide; } else command = 0; break;
        case 0x20:    if (param & 0x0F) { param = (param << 4) | 0x0F; command = CmdPanningSlide; } else command = 0; break;
        case 0x30:    param = (data & 0x0F) | 0x10; break; // glissando
        case 0x40:    param = (data & 0x0F) | 0x30; break; // vibrato waveform
        case 0x60:    param = (data & 0x0F) | 0xB0; break;
        case 0x70:    param = (data & 0x0F) | 0x40; break; // tremolo waveform
        case 0x90:    command = CmdRetrig; param &= 0x0F; break;
        case 0xA0:    param = (data & 0x0F) << 4; command = CmdGlobalVolSlide; break;
        case 0xB0:    param = data & 0x0F; command = CmdGlobalVolSlide; break;
        case 0xF0:    param = ((data >> 8) & 0x0F) | 0xA0; break;
        }
        break;
    case 0x0F:    command = CmdSpeed; break;
    case 0x10:
        if ((param & 0xF0) != 0xE0) {
            command = CmdVolSlide;
            if ((param & 0xF0) == 0xF0) {
                param = ((param << 4) | 0x0F);
            } else {
                param >>= 2;
                if (param > 0xF)
                    param = 0xF;
                param <<= 4;
            }
        }
        break;
    case 0x20:
        if ((param & 0xF0) != 0xE0) {
            command = CmdVolSlide;
            if ((param & 0xF0) != 0xF0) {
                param >>= 2;
                if (param > 0xF)
                    param = 0xF;
            }
        }
        break;

    case 0x30:    command = CmdRetrig; break;
    case 0x40:    command = CmdTremolo; break;
    case 0x50:    command = CmdTremor; break;
    case 0xEF:    if (param > 0xFF) param = 0xFF; command = CmdOffset; break;
    }
    if (command)
    {
        //XXXih: gross!
        m->command = (modplug::tracker::cmd_t) command;
        m->param = param;
    }
}


// Convert MDL envelope data (env points and flags)
void ConvertMDLEnvelope(const unsigned char *pMDLEnv, modplug::tracker::modenvelope_t *pMPTEnv)
//--------------------------------------------------------------------------------
{
    uint16_t nCurTick = 1;
    pMPTEnv->num_nodes = 15;
    for (UINT nTick = 0; nTick < 15; nTick++)
    {
        if (nTick) nCurTick += pMDLEnv[nTick * 2 + 1];
        pMPTEnv->Ticks[nTick] = nCurTick;
        pMPTEnv->Values[nTick] = pMDLEnv[nTick * 2 + 2];
        if (!pMDLEnv[nTick * 2 + 1]) // last point reached
        {
            pMPTEnv->num_nodes = nTick + 1;
            break;
        }
    }
    pMPTEnv->sustain_start = pMPTEnv->sustain_end = pMDLEnv[31] & 0x0F;
    pMPTEnv->flags |= ((pMDLEnv[31] & 0x10) ? ENV_SUSTAIN : 0) | ((pMDLEnv[31] & 0x20) ? ENV_LOOP : 0);
    pMPTEnv->loop_start = pMDLEnv[32] & 0x0F;
    pMPTEnv->loop_end = pMDLEnv[32] >> 4;
}


void UnpackMDLTrack(modplug::tracker::modevent_t *pat, UINT nChannels, UINT nRows, UINT nTrack, const uint8_t *lpTracks)
//-------------------------------------------------------------------------------------------------
{
    modplug::tracker::modevent_t cmd, *m = pat;
    UINT len = *((uint16_t *)lpTracks);
    UINT pos = 0, row = 0, i;
    lpTracks += 2;
    for (UINT ntrk=1; ntrk<nTrack; ntrk++)
    {
        lpTracks += len;
        len = *((uint16_t *)lpTracks);
        lpTracks += 2;
    }
    cmd.note = cmd.instr = 0;
    cmd.volcmd = VolCmdNone;
    cmd.vol = 0;
    cmd.command = CmdNone;
    cmd.param = 0;
    while ((row < nRows) && (pos < len))
    {
        UINT xx;
        uint8_t b = lpTracks[pos++];
        xx = b >> 2;
        switch(b & 0x03)
        {
        case 0x01:
            for (i=0; i<=xx; i++)
            {
                if (row) *m = *(m-nChannels);
                m += nChannels;
                row++;
                if (row >= nRows) break;
            }
            break;

        case 0x02:
            if (xx < row) *m = pat[nChannels*xx];
            m += nChannels;
            row++;
            break;

        case 0x03:
            {
                cmd.note = (xx & 0x01) ? lpTracks[pos++] : 0;
                cmd.instr = (xx & 0x02) ? lpTracks[pos++] : 0;
                cmd.volcmd = VolCmdNone;
                cmd.vol = 0;
                cmd.command = CmdNone;
                cmd.param = 0;
                if ((cmd.note < NoteMax-12) && (cmd.note)) cmd.note += 12;
                UINT volume = (xx & 0x04) ? lpTracks[pos++] : 0;
                UINT commands = (xx & 0x08) ? lpTracks[pos++] : 0;
                UINT command1 = commands & 0x0F;
                UINT command2 = commands & 0xF0;
                UINT param1 = (xx & 0x10) ? lpTracks[pos++] : 0;
                UINT param2 = (xx & 0x20) ? lpTracks[pos++] : 0;
                if ((command1 == 0x0E) && ((param1 & 0xF0) == 0xF0) && (!command2))
                {
                    param1 = ((param1 & 0x0F) << 8) | param2;
                    command1 = 0xEF;
                    command2 = param2 = 0;
                }
                if (volume)
                {
                    cmd.volcmd = VolCmdVol;
                    cmd.vol = (volume+1) >> 2;
                }
                ConvertMDLCommand(&cmd, command1, param1);
                if ((cmd.command != CmdSpeed)
                 && (cmd.command != CmdTempo)
                 && (cmd.command != CmdPatternBreak))
                    ConvertMDLCommand(&cmd, command2, param2);
                *m = cmd;
                m += nChannels;
                row++;
            }
            break;

        // Empty Slots
        default:
            row += xx+1;
            m += (xx+1)*nChannels;
            if (row >= nRows) break;
        }
    }
}



bool module_renderer::ReadMDL(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    uint32_t dwMemPos, dwPos, blocklen, dwTrackPos;
    const MDLSONGHEADER *pmsh = (const MDLSONGHEADER *)lpStream;
    MDLINFOBLOCK *pmib;
    UINT i,j, norders = 0, npatterns = 0, ntracks = 0;
    UINT ninstruments = 0, nsamples = 0;
    uint16_t block;
    uint16_t patterntracks[MAX_PATTERNS*32];
    uint8_t smpinfo[MAX_SAMPLES];
    uint8_t insvolenv[MAX_INSTRUMENTS];
    uint8_t inspanenv[MAX_INSTRUMENTS];
    const uint8_t *pvolenv;
    const uint8_t *ppanenv;
    const uint8_t *ppitchenv;
    UINT nvolenv, npanenv, npitchenv;
    vector<modplug::tracker::rowindex_t> patternLength;

    if ((!lpStream) || (dwMemLength < 1024)) return false;
    if ((pmsh->id != 0x4C444D44) || ((pmsh->version & 0xF0) > 0x10)) return false;
#ifdef MDL_LOG
    Log("MDL v%d.%d\n", pmsh->version>>4, pmsh->version&0x0f);
#endif
    memset(patterntracks, 0, sizeof(patterntracks));
    memset(smpinfo, 0, sizeof(smpinfo));
    memset(insvolenv, 0, sizeof(insvolenv));
    memset(inspanenv, 0, sizeof(inspanenv));
    dwMemPos = 5;
    dwTrackPos = 0;
    pvolenv = ppanenv = ppitchenv = NULL;
    nvolenv = npanenv = npitchenv = 0;
    m_nSamples = m_nInstruments = 0;
    while (dwMemPos+6 < dwMemLength)
    {
        block = *((uint16_t *)(lpStream+dwMemPos));
        blocklen = *((uint32_t *)(lpStream+dwMemPos+2));
        dwMemPos += 6;
        if (blocklen > dwMemLength - dwMemPos)
        {
            if (dwMemPos == 11) return false;
            break;
        }
        switch(block)
        {
        // IN: infoblock
        case 0x4E49:
        #ifdef MDL_LOG
            Log("infoblock: %d bytes\n", blocklen);
        #endif
            pmib = (MDLINFOBLOCK *)(lpStream+dwMemPos);
            assign_without_padding(this->song_name, pmib->songname, 31);

            norders = pmib->norders;
            if (norders > MAX_ORDERS) norders = MAX_ORDERS;
            m_nRestartPos = pmib->repeatpos;
            m_nDefaultGlobalVolume = pmib->globalvol;
            m_nDefaultTempo = pmib->tempo;
            m_nDefaultSpeed = pmib->speed;
            m_nChannels = 4;
            for (i=0; i<32; i++)
            {
                ChnSettings[i].nVolume = 64;
                ChnSettings[i].nPan = (pmib->channelinfo[i] & 0x7F) << 1;
                if (pmib->channelinfo[i] & 0x80)
                    ChnSettings[i].dwFlags |= CHN_MUTE;
                else
                    m_nChannels = i+1;
            }
            Order.ReadAsByte(pmib->seq, norders, sizeof(pmib->seq));
            break;
        // ME: song message
        case 0x454D:
        #ifdef MDL_LOG
            Log("song message: %d bytes\n", blocklen);
        #endif
            if(blocklen)
            {
                ReadMessage(lpStream + dwMemPos, blocklen - 1, leCR);
            }
            break;
        // PA: Pattern Data
        case 0x4150:
        #ifdef MDL_LOG
            Log("pattern data: %d bytes\n", blocklen);
        #endif
            npatterns = lpStream[dwMemPos];
            if (npatterns > MAX_PATTERNS) npatterns = MAX_PATTERNS;
            dwPos = dwMemPos + 1;

            patternLength.assign(npatterns, 64);

            for (i=0; i<npatterns; i++)
            {
                const uint16_t *pdata;
                UINT ch;

                if (dwPos+18 >= dwMemLength) break;
                if (pmsh->version > 0)
                {
                    const MDLPATTERNDATA *pmpd = (const MDLPATTERNDATA *)(lpStream + dwPos);
                    if (pmpd->channels > 32) break;
                    patternLength[i] = pmpd->lastrow + 1;
                    if (m_nChannels < pmpd->channels) m_nChannels = pmpd->channels;
                    dwPos += 18 + 2*pmpd->channels;
                    pdata = pmpd->data;
                    ch = pmpd->channels;
                } else
                {
                    pdata = (const uint16_t *)(lpStream + dwPos);
                    //Patterns[i].Resize(64, false);
                    if (m_nChannels < 32) m_nChannels = 32;
                    dwPos += 2*32;
                    ch = 32;
                }
                for (j=0; j<ch; j++) if (j<m_nChannels)
                {
                    patterntracks[i*32+j] = pdata[j];
                }
            }
            break;
        // TR: Track Data
        case 0x5254:
        #ifdef MDL_LOG
            Log("track data: %d bytes\n", blocklen);
        #endif
            if (dwTrackPos) break;
            ntracks = *((uint16_t *)(lpStream+dwMemPos));
            dwTrackPos = dwMemPos+2;
            break;
        // II: Instruments
        case 0x4949:
        #ifdef MDL_LOG
            Log("instruments: %d bytes\n", blocklen);
        #endif
            ninstruments = lpStream[dwMemPos];
            dwPos = dwMemPos+1;
            for (i=0; i<ninstruments; i++)
            {
                UINT nins = lpStream[dwPos];
                if ((nins >= MAX_INSTRUMENTS) || (!nins)) break;
                if (m_nInstruments < nins) m_nInstruments = nins;
                if (!Instruments[nins])
                {
                    UINT note = 12;
                    if ((Instruments[nins] = new modinstrument_t) == NULL) break;
                    modinstrument_t *pIns = Instruments[nins];
                    memset(pIns, 0, sizeof(modinstrument_t));
                    memcpy(pIns->name, lpStream+dwPos+2, 32);
                    SpaceToNullStringFixed<31>(pIns->name);

                    pIns->global_volume = 64;
                    pIns->pitch_pan_center = 5*12;
                    SetDefaultInstrumentValues(pIns);
                    for (j=0; j<lpStream[dwPos+1]; j++)
                    {
                        const uint8_t *ps = lpStream+dwPos+34+14*j;
                        while ((note < (UINT)(ps[1]+12)) && (note < NoteMax))
                        {
                            pIns->NoteMap[note] = note+1;
                            if (ps[0] < MAX_SAMPLES)
                            {
                                int ismp = ps[0];
                                pIns->Keyboard[note] = ps[0];
                                Samples[ismp].default_volume = ps[2];
                                Samples[ismp].default_pan = ps[4] << 1;
                                Samples[ismp].vibrato_type = ps[11];
                                Samples[ismp].vibrato_sweep = ps[10];
                                Samples[ismp].vibrato_depth = ps[9];
                                Samples[ismp].vibrato_rate = ps[8];
                            }
                            pIns->fadeout = (ps[7] << 8) | ps[6];
                            if (pIns->fadeout == 0xFFFF) pIns->fadeout = 0;
                            note++;
                        }
                        // Use volume envelope ?
                        if (ps[3] & 0x80)
                        {
                            pIns->volume_envelope.flags |= ENV_ENABLED;
                            insvolenv[nins] = (ps[3] & 0x3F) + 1;
                        }
                        // Use panning envelope ?
                        if (ps[5] & 0x80)
                        {
                            pIns->panning_envelope.flags |= ENV_ENABLED;
                            inspanenv[nins] = (ps[5] & 0x3F) + 1;
                        }

                        // taken from load_xm.cpp - seems to fix wakingup.mdl
                        if (!(pIns->volume_envelope.flags & ENV_ENABLED) && !pIns->fadeout)
                            pIns->fadeout = 8192;
                    }
                }
                dwPos += 34 + 14*lpStream[dwPos+1];
            }
            for (j=1; j<=m_nInstruments; j++) if (!Instruments[j])
            {
                Instruments[j] = new modinstrument_t;
                if (Instruments[j]) memset(Instruments[j], 0, sizeof(modinstrument_t));
            }
            break;
        // VE: Volume Envelope
        case 0x4556:
        #ifdef MDL_LOG
            Log("volume envelope: %d bytes\n", blocklen);
        #endif
            if ((nvolenv = lpStream[dwMemPos]) == 0) break;
            if (dwMemPos + nvolenv*32 + 1 <= dwMemLength) pvolenv = lpStream + dwMemPos + 1;
            break;
        // PE: Panning Envelope
        case 0x4550:
        #ifdef MDL_LOG
            Log("panning envelope: %d bytes\n", blocklen);
        #endif
            if ((npanenv = lpStream[dwMemPos]) == 0) break;
            if (dwMemPos + npanenv*32 + 1 <= dwMemLength) ppanenv = lpStream + dwMemPos + 1;
            break;
        // FE: Pitch Envelope
        case 0x4546:
        #ifdef MDL_LOG
            Log("pitch envelope: %d bytes\n", blocklen);
        #endif
            if ((npitchenv = lpStream[dwMemPos]) == 0) break;
            if (dwMemPos + npitchenv*32 + 1 <= dwMemLength) ppitchenv = lpStream + dwMemPos + 1;
            break;
        // IS: Sample Infoblock
        case 0x5349:
        #ifdef MDL_LOG
            Log("sample infoblock: %d bytes\n", blocklen);
        #endif
            nsamples = lpStream[dwMemPos];
            dwPos = dwMemPos+1;
            for (i=0; i<nsamples; i++, dwPos += (pmsh->version > 0) ? 59 : 57)
            {
                UINT nins = lpStream[dwPos];
                if ((nins >= MAX_SAMPLES) || (!nins)) continue;
                if (m_nSamples < nins) m_nSamples = nins;
                modsample_t *pSmp = &Samples[nins];
                memcpy(m_szNames[nins], lpStream+dwPos+1, 31);
                memcpy(pSmp->legacy_filename, lpStream+dwPos+33, 8);
                SpaceToNullStringFixed<31>(m_szNames[nins]);
                SpaceToNullStringFixed<8>(pSmp->legacy_filename);
                const uint8_t *p = lpStream+dwPos+41;
                if (pmsh->version > 0)
                {
                    pSmp->c5_samplerate = *((uint32_t *)p);
                    p += 4;
                } else
                {
                    pSmp->c5_samplerate = *((uint16_t *)p);
                    p += 2;
                }
                pSmp->length = *((uint32_t *)(p));
                pSmp->loop_start = *((uint32_t *)(p+4));
                pSmp->loop_end = pSmp->loop_start + *((uint32_t *)(p+8));
                if (pSmp->loop_end > pSmp->loop_start) pSmp->flags |= CHN_LOOP;
                pSmp->global_volume = 64;
                if (p[13] & 0x01)
                {
                    pSmp->flags |= CHN_16BIT;
                    pSmp->length >>= 1;
                    pSmp->loop_start >>= 1;
                    pSmp->loop_end >>= 1;
                }
                if (p[13] & 0x02) pSmp->flags |= CHN_PINGPONGLOOP;
                smpinfo[nins] = (p[13] >> 2) & 3;
            }
            break;
        // SA: Sample Data
        case 0x4153:
        #ifdef MDL_LOG
            Log("sample data: %d bytes\n", blocklen);
        #endif
            dwPos = dwMemPos;
            for (i=1; i<=m_nSamples; i++) if ((Samples[i].length) && (!Samples[i].sample_data) && (smpinfo[i] != 3) && (dwPos < dwMemLength))
            {
                modsample_t *pSmp = &Samples[i];
                UINT flags = (pSmp->flags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S;
                if (!smpinfo[i])
                {
                    dwPos += ReadSample(pSmp, flags, (LPSTR)(lpStream+dwPos), dwMemLength - dwPos);
                } else
                {
                    uint32_t dwLen = *((uint32_t *)(lpStream+dwPos));
                    dwPos += 4;
                    if ( (dwLen <= dwMemLength) && (dwPos <= dwMemLength - dwLen) && (dwLen > 4) )
                    {
                        flags = (pSmp->flags & CHN_16BIT) ? RS_MDL16 : RS_MDL8;
                        ReadSample(pSmp, flags, (LPSTR)(lpStream+dwPos), dwLen);
                    }
                    dwPos += dwLen;
                }
            }
            break;
        #ifdef MDL_LOG
        default:
            Log("unknown block (%c%c): %d bytes\n", block&0xff, block>>8, blocklen);
        #endif
        }
        dwMemPos += blocklen;
    }
    // Unpack Patterns
    if ((dwTrackPos) && (npatterns) && (m_nChannels) && (ntracks))
    {
        for (UINT ipat=0; ipat<npatterns; ipat++)
        {
            if(Patterns.Insert(ipat, patternLength[ipat]))
            {
                break;
            }
            for (UINT chn=0; chn<m_nChannels; chn++) if ((patterntracks[ipat*32+chn]) && (patterntracks[ipat*32+chn] <= ntracks))
            {
                modplug::tracker::modevent_t *m = Patterns[ipat] + chn;
                UnpackMDLTrack(m, m_nChannels, Patterns[ipat].GetNumRows(), patterntracks[ipat*32+chn], lpStream+dwTrackPos);
            }
        }
    }
    // Set up envelopes
    for (UINT iIns=1; iIns<=m_nInstruments; iIns++) if (Instruments[iIns])
    {
        // Setup volume envelope
        if ((nvolenv) && (pvolenv) && (insvolenv[iIns]))
        {
            const uint8_t * pve = pvolenv;
            for (UINT nve = 0; nve < nvolenv; nve++, pve += 33)
            {
                if (pve[0] + 1 == insvolenv[iIns])
                    ConvertMDLEnvelope(pve, &Instruments[iIns]->volume_envelope);
            }
        }
        // Setup panning envelope
        if ((npanenv) && (ppanenv) && (inspanenv[iIns]))
        {
            const uint8_t * ppe = ppanenv;
            for (UINT npe = 0; npe < npanenv; npe++, ppe += 33)
            {
                if (ppe[0] + 1 == inspanenv[iIns])
                    ConvertMDLEnvelope(ppe, &Instruments[iIns]->panning_envelope);
            }
        }
    }
    m_dwSongFlags |= SONG_LINEARSLIDES;
    m_nType = MOD_TYPE_MDL;
    return true;
}


/////////////////////////////////////////////////////////////////////////
// MDL Sample Unpacking

// MDL Huffman ReadBits compression
uint16_t MDLReadBits(uint32_t &bitbuf, UINT &bitnum, LPBYTE &ibuf, CHAR n)
//-----------------------------------------------------------------
{
    uint16_t v = (uint16_t)(bitbuf & ((1 << n) - 1) );
    bitbuf >>= n;
    bitnum -= n;
    if (bitnum <= 24)
    {
        bitbuf |= (((uint32_t)(*ibuf++)) << bitnum);
        bitnum += 8;
    }
    return v;
}