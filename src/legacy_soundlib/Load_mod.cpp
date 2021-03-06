/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 *                    OpenMPT dev(s)        (miscellaneous modifications)
*/

#include "stdafx.h"
#include "Loaders.h"

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

extern uint16_t ProTrackerPeriodTable[6*12];

//////////////////////////////////////////////////////////
// ProTracker / NoiseTracker MOD/NST file support

void module_renderer::ConvertModCommand(modplug::tracker::modevent_t *m) const
//-----------------------------------------------------
{
    modplug::tracker::cmd_t command = m->command;
    modplug::tracker::param_t param = m->param;

    switch(command)
    {
    case 0x00:    if (param) command = CmdArpeggio; break;
    case 0x01:    command = CmdPortaUp; break;
    case 0x02:    command = CmdPortaDown; break;
    case 0x03:    command = CmdPorta; break;
    case 0x04:    command = CmdVibrato; break;
    case 0x05:    command = CmdPortaVolSlide; if (param & 0xF0) param &= 0xF0; break;
    case 0x06:    command = CmdVibratoVolSlide; if (param & 0xF0) param &= 0xF0; break;
    case 0x07:    command = CmdTremolo; break;
    case 0x08:    command = CmdPanning8; break;
    case 0x09:    command = CmdOffset; break;
    case 0x0A:    command = CmdVolSlide; if (param & 0xF0) param &= 0xF0; break;
    case 0x0B:    command = CmdPositionJump; break;
    case 0x0C:    command = CmdVol; break;
    case 0x0D:    command = CmdPatternBreak; param = ((param >> 4) * 10) + (param & 0x0F); break;
    case 0x0E:    command = CmdModCmdEx; break;
    case 0x0F:    command = (param <= ((m_nType & (MOD_TYPE_MOD)) ? 0x20 : 0x1F)) ? CmdSpeed : CmdTempo;
                if ((param == 0xFF) && (m_nSamples == 15) && (m_nType & MOD_TYPE_MOD)) command = CmdNone; break; //<rewbs> what the hell is this?! :) //<jojo> it's the "stop tune" command! :-P
    // Extension for XM extended effects
    case 'G' - 55:    command = CmdGlobalVol; break;                //16
    case 'H' - 55:    command = CmdGlobalVolSlide; if (param & 0xF0) param &= 0xF0; break;
    case 'K' - 55:    command = CmdKeyOff; break;
    case 'L' - 55:    command = CmdSetEnvelopePosition; break;
    case 'M' - 55:    command = CmdChannelVol; break;
    case 'N' - 55:    command = CmdChannelVolSlide; break;
    case 'P' - 55:    command = CmdPanningSlide; if (param & 0xF0) param &= 0xF0; break;
    case 'R' - 55:    command = CmdRetrig; break;
    case 'T' - 55:    command = CmdTremor; break;
    case 'X' - 55:    command = CmdExtraFinePortaUpDown;        break;
    case 'Y' - 55:    command = CmdPanbrello; break;                        //34
    case 'Z' - 55:    command = CmdMidi;        break;                                //35
    case '\\' - 56:    command = CmdSmoothMidi;        break;                //rewbs.smoothVST: 36 - note: this is actually displayed as "-" in FT2, but seems to be doing nothing.
    //case ':' - 21:    command = CMD_DELAYCUT;        break;                //37
    case '#' + 3:    command = CmdExtendedParameter;        break;                        //rewbs.XMfixes - XParam is 38
    default:            command = CmdNone;
    }
    m->command = command;
    m->param = param;
}


uint16_t module_renderer::ModSaveCommand(const modplug::tracker::modevent_t *m, const bool bXM, const bool bCompatibilityExport) const
//---------------------------------------------------------------------------------------------------------
{
    UINT command = m->command, param = m->param;

    switch(command)
    {
    case CmdNone:                            command = param = 0; break;
    case CmdArpeggio:                    command = 0; break;
    case CmdPortaUp:
        if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_STM|MOD_TYPE_MPT))
        {
            if ((param & 0xF0) == 0xE0) { command = 0x0E; param = ((param & 0x0F) >> 2) | 0x10; break; }
            else if ((param & 0xF0) == 0xF0) { command = 0x0E; param &= 0x0F; param |= 0x10; break; }
        }
        command = 0x01;
        break;
    case CmdPortaDown:
        if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_STM|MOD_TYPE_MPT))
        {
            if ((param & 0xF0) == 0xE0) { command = 0x0E; param= ((param & 0x0F) >> 2) | 0x20; break; }
            else if ((param & 0xF0) == 0xF0) { command = 0x0E; param &= 0x0F; param |= 0x20; break; }
        }
        command = 0x02;
        break;
    case CmdPorta:    command = 0x03; break;
    case CmdVibrato:                    command = 0x04; break;
    case CmdPortaVolSlide:            command = 0x05; break;
    case CmdVibratoVolSlide:            command = 0x06; break;
    case CmdTremolo:                    command = 0x07; break;
    case CmdPanning8:
        command = 0x08;
        if(m_nType & MOD_TYPE_S3M)
        {
            if(param <= 0x80)
            {
                param = bad_min(param << 1, 0xFF);
            }
            else if(param == 0xA4)    // surround
            {
                if(bCompatibilityExport || !bXM)
                {
                    command = param = 0;
                }
                else
                {
                    command = 'X' - 55;
                    param = 91;
                }
            }
        }
        break;
    case CmdOffset:                    command = 0x09; break;
    case CmdVolSlide:            command = 0x0A; break;
    case CmdPositionJump:            command = 0x0B; break;
    case CmdVol:                    command = 0x0C; break;
    case CmdPatternBreak:            command = 0x0D; param = ((param / 10) << 4) | (param % 10); break;
    case CmdModCmdEx:                    command = 0x0E; break;
    case CmdSpeed:                            command = 0x0F; param = bad_min(param, (bXM) ? 0x1F : 0x20); break;
    case CmdTempo:                            command = 0x0F; param = bad_max(param, (bXM) ? 0x20 : 0x21); break;
    case CmdGlobalVol:            command = 'G' - 55; break;
    case CmdGlobalVolSlide:    command = 'H' - 55; break;
    case CmdKeyOff:                    command = 'K' - 55; break;
    case CmdSetEnvelopePosition:    command = 'L' - 55; break;
    case CmdChannelVol:            command = 'M' - 55; break;
    case CmdChannelVolSlide:    command = 'N' - 55; break;
    case CmdPanningSlide:            command = 'P' - 55; break;
    case CmdRetrig:                    command = 'R' - 55; break;
    case CmdTremor:                    command = 'T' - 55; break;
    case CmdExtraFinePortaUpDown:
        if(bCompatibilityExport && (param & 0xF0) > 2)    // X1x and X2x are legit, everything above are MPT extensions, which don't belong here.
            command = param = 0;
        else
            command = 'X' - 55;
        break;
    case CmdPanbrello:
        if(bCompatibilityExport)
            command = param = 0;
        else
            command = 'Y' - 55;
        break;
    case CmdMidi:
        if(bCompatibilityExport)
            command = param = 0;
        else
            command = 'Z' - 55;
        break;
    case CmdSmoothMidi: //rewbs.smoothVST: 36
        if(bCompatibilityExport)
            command = param = 0;
        else
            command = '\\' - 56;
        break;
    case CmdExtendedParameter: //rewbs.XMfixes - XParam is 38
        if(bCompatibilityExport)
            command = param = 0;
        else
            command = '#' + 3;
        break;
    case CmdS3mCmdEx:
        switch(param & 0xF0)
        {
        case 0x10:    command = 0x0E; param = (param & 0x0F) | 0x30; break;
        case 0x20:    command = 0x0E; param = (param & 0x0F) | 0x50; break;
        case 0x30:    command = 0x0E; param = (param & 0x0F) | 0x40; break;
        case 0x40:    command = 0x0E; param = (param & 0x0F) | 0x70; break;
        case 0x90:
            if(bCompatibilityExport)
                command = param = 0;
            else
                command = 'X' - 55;
            break;
        case 0xB0:    command = 0x0E; param = (param & 0x0F) | 0x60; break;
        case 0xA0:
        case 0x50:
        case 0x70:
        case 0x60:    command = param = 0; break;
        default:    command = 0x0E; break;
        }
        break;
    default:
        command = param = 0;
    }

    // don't even think about saving XM effects in MODs...
    if(command > 0x0F && !bXM)
        command = param = 0;

    return (uint16_t)((command << 8) | (param));
}


#pragma pack(1)

typedef struct _MODSAMPLEHEADER
{
    CHAR name[22];
    uint16_t length;
    uint8_t finetune;
    uint8_t volume;
    uint16_t loopstart;
    uint16_t looplen;
} MODSAMPLEHEADER, *PMODSAMPLEHEADER;

typedef struct _MODMAGIC
{
    uint8_t nOrders;
    uint8_t nRestartPos;
    uint8_t Orders[128];
    char Magic[4];
} MODMAGIC, *PMODMAGIC;

#pragma pack()

bool IsMagic(LPCSTR s1, LPCSTR s2)
{
    return ((*(uint32_t *)s1) == (*(uint32_t *)s2)) ? true : false;
}

// Functor for fixing VBlank MODs and MODs with 7-bit panning
struct FixMODPatterns
//===================
{
    FixMODPatterns(bool bVBlank, bool bPanning)
    {
        this->bVBlank = bVBlank;
        this->bPanning = bPanning;
    }

    void operator()(modplug::tracker::modevent_t& m)
    {
        // Fix VBlank MODs
        if(m.command == CmdTempo && this->bVBlank)
        {
            m.command = CmdSpeed;
        }
        // Fix MODs with 7-bit + surround panning
        if(m.command == CmdPanning8 && this->bPanning)
        {
            if(m.param == 0xA4)
            {
                m.command = CmdS3mCmdEx;
                m.param = 0x91;
            } else
            {
                m.param = bad_min(m.param * 2, 0xFF);
            }
        }
    }

    bool bVBlank, bPanning;
};


bool module_renderer::ReadMod(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
    char s[1024];
    uint32_t dwMemPos, dwTotalSampleLen;
    PMODMAGIC pMagic;
    UINT nErr;

    if ((!lpStream) || (dwMemLength < 0x600)) return false;
    dwMemPos = 20;
    m_nSamples = 31;
    m_nChannels = 4;
    pMagic = (PMODMAGIC)(lpStream + dwMemPos + sizeof(MODSAMPLEHEADER) * 31);
    // Check Mod Magic
    memcpy(s, pMagic->Magic, 4);
    if ((IsMagic(s, "M.K.")) || (IsMagic(s, "M!K!"))
     || (IsMagic(s, "M&K!")) || (IsMagic(s, "N.T.")) || (IsMagic(s, "FEST"))) m_nChannels = 4; else
    if ((IsMagic(s, "CD81")) || (IsMagic(s, "OKTA"))) m_nChannels = 8; else
    if ((s[0]=='F') && (s[1]=='L') && (s[2]=='T') && (s[3]>='4') && (s[3]<='9')) m_nChannels = s[3] - '0'; else
    if ((s[0]>='4') && (s[0]<='9') && (s[1]=='C') && (s[2]=='H') && (s[3]=='N')) m_nChannels = s[0] - '0'; else
    if ((s[0]=='1') && (s[1]>='0') && (s[1]<='9') && (s[2]=='C') && (s[3]=='H')) m_nChannels = s[1] - '0' + 10; else
    if ((s[0]=='2') && (s[1]>='0') && (s[1]<='9') && (s[2]=='C') && (s[3]=='H')) m_nChannels = s[1] - '0' + 20; else
    if ((s[0]=='3') && (s[1]>='0') && (s[1]<='2') && (s[2]=='C') && (s[3]=='H')) m_nChannels = s[1] - '0' + 30; else
    if ((s[0]=='T') && (s[1]=='D') && (s[2]=='Z') && (s[3]>='4') && (s[3]<='9')) m_nChannels = s[3] - '0'; else
    if (IsMagic(s,"16CN")) m_nChannels = 16; else
    if (IsMagic(s,"32CN")) m_nChannels = 32; else m_nSamples = 15;
    // Startrekker 8 channel mod (needs special treatment, see below)
    bool bFLT8 = IsMagic(s, "FLT8") ? true : false;
    // Only apply VBlank tests to M.K. (ProTracker) modules.
    const bool bMdKd = IsMagic(s, "M.K.") ? true : false;

    // Load Samples
    nErr = 0;
    dwTotalSampleLen = 0;
    for    (UINT i=1; i<=m_nSamples; i++)
    {
        PMODSAMPLEHEADER pms = (PMODSAMPLEHEADER)(lpStream+dwMemPos);
        modsample_t *psmp = &Samples[i];
        UINT loopstart, looplen;

        memcpy(m_szNames[i], pms->name, 22);
        SpaceToNullStringFixed<22>(m_szNames[i]);
        psmp->flags = 0;
        psmp->length = BigEndianW(pms->length)*2;
        dwTotalSampleLen += psmp->length;
        psmp->nFineTune = MOD2XMFineTune(pms->finetune & 0x0F);
        psmp->default_volume = 4*pms->volume;
        if (psmp->default_volume > 256) { psmp->default_volume = 256; nErr++; }
        psmp->global_volume = 64;
        psmp->default_pan = 128;
        loopstart = BigEndianW(pms->loopstart)*2;
        looplen = BigEndianW(pms->looplen)*2;
        // Fix loops
        if ((looplen > 2) && (loopstart+looplen > psmp->length)
         && (loopstart/2+looplen <= psmp->length))
        {
            loopstart /= 2;
        }
        psmp->loop_start = loopstart;
        psmp->loop_end = loopstart + looplen;
        if (psmp->length < 4) psmp->length = 0;
        if (psmp->length)
        {
            UINT derr = 0;
            if (psmp->loop_start >= psmp->length) { psmp->loop_start = psmp->length-1; derr|=1; }
            if (psmp->loop_end > psmp->length) { psmp->loop_end = psmp->length; derr |= 1; }
            if (psmp->loop_start > psmp->loop_end) derr |= 1;
            if ((psmp->loop_start > psmp->loop_end) || (psmp->loop_end < 4)
             || (psmp->loop_end - psmp->loop_start < 4))
            {
                psmp->loop_start = 0;
                psmp->loop_end = 0;
            }
            // Fix for most likely broken sample loops. This fixes super_sufm_-_new_life.mod which has a long sample which is looped from 0 to 4.
            if(psmp->loop_end <= 8 && psmp->loop_start == 0 && psmp->length > psmp->loop_end)
            {
                psmp->loop_end = 0;
            }
            if (psmp->loop_end > psmp->loop_start)
            {
                psmp->flags |= CHN_LOOP;
            }
        }
        dwMemPos += sizeof(MODSAMPLEHEADER);
    }
    if ((m_nSamples == 15) && (dwTotalSampleLen > dwMemLength * 4)) return false;
    pMagic = (PMODMAGIC)(lpStream+dwMemPos);
    dwMemPos += sizeof(MODMAGIC);
    if (m_nSamples == 15) dwMemPos -= 4;
    Order.ReadAsByte(pMagic->Orders, 128, 128);

    UINT nbp, nbpbuggy, nbpbuggy2, norders;

    norders = pMagic->nOrders;
    if ((!norders) || (norders > 0x80))
    {
        norders = 0x80;
        while ((norders > 1) && (!Order[norders-1])) norders--;
    }
    nbpbuggy = 0;
    nbpbuggy2 = 0;
    nbp = 0;
    for (UINT iord=0; iord<128; iord++)
    {
        UINT i = Order[iord];
        if ((i < 0x80) && (nbp <= i))
        {
            nbp = i+1;
            if (iord<norders) nbpbuggy = nbp;
        }
        if (i >= nbpbuggy2) nbpbuggy2 = i+1;

        // from mikmod: if the file says FLT8, but the orderlist has odd numbers, it's probably really an FLT4
        if(bFLT8 && (Order[iord] & 1))
        {
            m_nChannels = 4;
            bFLT8 = false;
        }

        // chances are very high that we're dealing with a non-MOD file here.
        if(m_nSamples == 15 && i >= 0x80)
            return false;
    }

    if(bFLT8)
    {
        // FLT8 has only even order items, so divide by two.
        for(modplug::tracker::orderindex_t nOrd = 0; nOrd < Order.GetLength(); nOrd++)
            Order[nOrd] >>= 1;
    }

    for(UINT iend = norders; iend < 0x80; iend++) Order[iend] = Order.GetInvalidPatIndex();

    norders--;
    m_nRestartPos = pMagic->nRestartPos;
    if (m_nRestartPos >= 0x78) m_nRestartPos = 0;
    if (m_nRestartPos + 1 >= (UINT)norders) m_nRestartPos = 0;
    if (!nbp) return false;
    uint32_t dwWowTest = dwTotalSampleLen+dwMemPos;
    if ((IsMagic(pMagic->Magic, "M.K.")) && (dwWowTest + nbp*8*256 == dwMemLength)) m_nChannels = 8;
    if ((nbp != nbpbuggy) && (dwWowTest + nbp*m_nChannels*256 != dwMemLength))
    {
        if (dwWowTest + nbpbuggy*m_nChannels*256 == dwMemLength) nbp = nbpbuggy;
        else nErr += 8;
    } else
    if ((nbpbuggy2 > nbp) && (dwWowTest + nbpbuggy2*m_nChannels*256 == dwMemLength))
    {
        nbp = nbpbuggy2;
    }
    if ((dwWowTest < 0x600) || (dwWowTest > dwMemLength)) nErr += 8;
    if ((m_nSamples == 15) && (nErr >= 16)) return false;
    // Default settings
    m_nType = MOD_TYPE_MOD;
    m_nDefaultSpeed = 6;
    m_nDefaultTempo = 125;
    m_nMinPeriod = 14 << 2;
    m_nMaxPeriod = 3424 << 2;
    assign_without_padding(this->song_name, reinterpret_cast<const char *>(lpStream), 20);
    // Setup channel pan positions and volume
    SetupMODPanning();

    const modplug::tracker::chnindex_t nMaxChn = (bFLT8) ? 4 : m_nChannels; // 4 channels per pattern in FLT8 format.
    if(bFLT8) nbp++; // as one logical pattern consists of two real patterns in FLT8 format, the highest pattern number has to be increased by one.
    bool bHasTempoCommands = false;    // for detecting VBlank MODs
    bool bLeftPanning = false, bExtendedPanning = false;    // for detecting 800-880 panning

    // Reading patterns
    for (modplug::tracker::patternindex_t ipat = 0; ipat < nbp; ipat++)
    {
        if (ipat < MAX_PATTERNS)
        {
            if (dwMemPos + nMaxChn * 256 > dwMemLength) break;

            modplug::tracker::modevent_t *m;
            if(bFLT8)
            {
                if((ipat & 1) == 0)
                {
                    // only create "even" patterns
                    if(Patterns.Insert(ipat >> 1, 64)) break;
                }
                m = Patterns[ipat >> 1];
            } else
            {
                if(Patterns.Insert(ipat, 64)) break;
                m = Patterns[ipat];
            }

            size_t instrWithoutNoteCount = 0;    // For detecting PT1x mode
            vector<modplug::tracker::instr_t> lastInstrument(m_nChannels, 0);

            const uint8_t *p = lpStream + dwMemPos;

            for(modplug::tracker::rowindex_t nRow = 0; nRow < 64; nRow++)
            {
                if(bFLT8)
                {
                    // FLT8: either write to channel 1 to 4 (even patterns) or 5 to 8 (odd patterns).
                    m = Patterns[ipat >> 1] + nRow * 8 + ((ipat & 1) ? 4 : 0);
                }
                for(modplug::tracker::chnindex_t nChn = 0; nChn < nMaxChn; nChn++, m++, p += 4)
                {
                    uint8_t A0 = p[0], A1 = p[1], A2 = p[2], A3 = p[3];
                    UINT n = ((((UINT)A0 & 0x0F) << 8) | (A1));
                    if ((n) && (n != 0xFFF)) m->note = GetNoteFromPeriod(n << 2);
                    m->instr = ((UINT)A2 >> 4) | (A0 & 0x10);
                    //XXXih: gross!
                    m->command = (modplug::tracker::cmd_t) (A2 & 0x0F);
                    m->param = A3;
                    if ((m->command) || (m->param)) ConvertModCommand(m);

                    if (m->command == CmdTempo && m->param < 100)
                        bHasTempoCommands = true;
                    if (m->command == CmdPanning8 && m->param < 0x80)
                        bLeftPanning = true;
                    if (m->command == CmdPanning8 && m->param > 0x80 && m->param != 0xA4)
                        bExtendedPanning = true;
                    if (m->note == NoteNone && m->instr > 0 && !bFLT8)
                    {
                        if(lastInstrument[nChn] > 0 && lastInstrument[nChn] != m->instr)
                        {
                            instrWithoutNoteCount++;
                        }
                    }
                    if (m->instr != 0)
                    {
                        lastInstrument[nChn] = m->instr;
                    }
                }
            }
            // Arbitrary threshold for going into PT1x mode: 16 "sample swaps" in one pattern.
            if(instrWithoutNoteCount > 16)
            {
                m_dwSongFlags |= SONG_PT1XMODE;
            }
        }
        dwMemPos += nMaxChn * 256;
    }

    // Reading samples
    bool bSamplesPresent = false;
    for (UINT ismp = 1; ismp <= m_nSamples; ismp++) if (Samples[ismp].length)
    {
        LPSTR p = (LPSTR)(lpStream + dwMemPos);
        UINT flags = 0;
        if (dwMemPos + 5 <= dwMemLength)
        {
            if (!_strnicmp(p, "ADPCM", 5))
            {
                flags = 3;
                p += 5;
                dwMemPos += 5;
            }
        }
        uint32_t dwSize = ReadSample(&Samples[ismp], flags, p, dwMemLength - dwMemPos);
        if (dwSize)
        {
            dwMemPos += dwSize;
            bSamplesPresent = true;
        }
    }

    // Fix VBlank MODs. Arbitrary threshold: 10 minutes.
    // Basically, this just converts all tempo commands into speed commands
    // for MODs which are supposed to have VBlank timing (instead of CIA timing).
    // There is no perfect way to do this, since both MOD types look the same,
    // but the most reliable way is to simply check for extremely long songs
    // (as this would indicate that f.e. a F30 command was really meant to set
    // the ticks per row to 48, and not the tempo to 48 BPM).
    // In the pattern loader above, a second condition is used: Only tempo commands
    // below 100 BPM are taken into account. Furthermore, only M.K. (ProTracker)
    // modules are checked.
    // The same check is also applied to original Ultimate Soundtracker 15 sample mods.
    const bool bVBlank = ((bMdKd && bHasTempoCommands && GetSongTime() >= 10 * 60) || m_nSamples == 15) ? true : false;
    const bool b7BitPanning = bLeftPanning && !bExtendedPanning;
    if(bVBlank || b7BitPanning)
    {
        Patterns.ForEachModCommand(FixMODPatterns(bVBlank, b7BitPanning));
    }

#ifdef MODPLUG_TRACKER
    return true;
#else
    return bSamplesPresent;
#endif    // MODPLUG_TRACKER
}


#ifndef MODPLUG_NO_FILESAVE

#ifdef MODPLUG_TRACKER
#include "../moddoc.h"
#endif    // MODPLUG_TRACKER

bool module_renderer::SaveMod(LPCSTR lpszFileName, UINT nPacking, const bool bCompatibilityExport)
//-------------------------------------------------------------------------------------------
{
    uint8_t insmap[32];
    UINT inslen[32];
    uint8_t bTab[32];
    uint8_t ord[128];
    FILE *f;

    if ((!m_nChannels) || (!lpszFileName)) return false;
    if ((f = fopen(lpszFileName, "wb")) == NULL) return false;
    MemsetZero(ord);
    MemsetZero(inslen);
    if (m_nInstruments)
    {
        MemsetZero(insmap);
        for (UINT i=1; i<32; i++) if (Instruments[i])
        {
            for (UINT j=0; j<128; j++) if (Instruments[i]->Keyboard[j])
            {
                insmap[i] = Instruments[i]->Keyboard[j];
                break;
            }
        }
    } else
    {
        for (UINT i=0; i<32; i++) insmap[i] = (uint8_t)i;
    }
    // Writing song name
    fwrite(m_szNames, 20, 1, f);
    // Writing instrument definition
    for (UINT iins=1; iins<=31; iins++)
    {
        modsample_t *pSmp = &Samples[insmap[iins]];
        memcpy(bTab, m_szNames[iins],22);
        inslen[iins] = pSmp->length;
        // if the sample size is odd, we have to add a padding byte, as all sample sizes in MODs are even.
        if(inslen[iins] & 1)
            inslen[iins]++;
        if (inslen[iins] > 0x1fff0) inslen[iins] = 0x1fff0;
        bTab[22] = inslen[iins] >> 9;
        bTab[23] = inslen[iins] >> 1;
        if (pSmp->RelativeTone < 0) bTab[24] = 0x08; else
        if (pSmp->RelativeTone > 0) bTab[24] = 0x07; else
        bTab[24] = (uint8_t)XM2MODFineTune(pSmp->nFineTune);
        bTab[25] = pSmp->default_volume >> 2;
        UINT repstart = 0, replen = 2;
        if(pSmp->flags & CHN_LOOP)
        {
            repstart = pSmp->loop_start;
            replen = pSmp->loop_end - pSmp->loop_start;
        }
        bTab[26] = repstart >> 9;
        bTab[27] = repstart >> 1;
        if(replen < 2) // ensure PT will load it properly
            replen = 2;
        bTab[28] = replen >> 9;
        bTab[29] = replen >> 1;
        fwrite(bTab, 30, 1, f);
    }
    // Writing number of patterns
    UINT nbp = 0, norders = 0;
    for (UINT iord = 0; iord < 128; iord++)
    {
        // Ignore +++ and --- patterns in order list, as well as high patterns (MOD officially only supports up to 128 patterns)
        if (Order[iord] < 0x80)
        {
            if (nbp <= Order[iord]) nbp = Order[iord] + 1;
            ord[norders++] = Order[iord];
        }
    }
    bTab[0] = norders;
    bTab[1] = m_nRestartPos;
    fwrite(bTab, 2, 1, f);
    fwrite(ord, 128, 1, f);
    // Writing signature
    if (m_nChannels == 4)
    {
        if(nbp < 64)
            lstrcpy((LPSTR)&bTab, "M.K.");
        else    // more than 64 patterns
            lstrcpy((LPSTR)&bTab, "M!K!");
    } else
    {
        sprintf((LPSTR)&bTab, "%luCHN", m_nChannels);
    }
    fwrite(bTab, 4, 1, f);
    // Writing patterns
    for (UINT ipat=0; ipat<nbp; ipat++)            //for all patterns
    {
        uint8_t s[64*4];
        if (Patterns[ipat])                                    //if pattern exists
        {
            modplug::tracker::modevent_t *m = Patterns[ipat];
            for (UINT i=0; i<64; i++) {                            //for all rows
                if (i < Patterns[ipat].GetNumRows()) {                    //if row exists
                    LPBYTE p=s;
                    for (UINT c=0; c<m_nChannels; c++,p+=4,m++)
                    {
                        UINT param = ModSaveCommand(m, false, true);
                        UINT command = param >> 8;
                        param &= 0xFF;
                        if (command > 0x0F) command = param = 0;
                        if ((m->vol >= 0x10) && (m->vol <= 0x50) && (!command) && (!param)) { command = 0x0C; param = m->vol - 0x10; }
                        UINT period = m->note;
                        if (period)
                        {
                            if (period < 37) period = 37;
                            period -= 37;
                            if (period >= 6*12) period = 6*12-1;
                            period = ProTrackerPeriodTable[period];
                        }
                        UINT instr = (m->instr > 31) ? 0 : m->instr;
                        p[0] = ((period >> 8) & 0x0F) | (instr & 0x10);
                        p[1] = period & 0xFF;
                        p[2] = ((instr & 0x0F) << 4) | (command & 0x0F);
                        p[3] = param;
                    }
                    fwrite(s, m_nChannels, 4, f);
                } else {                                                            //if row does not exist
                    memset(s, 0, m_nChannels*4);            //invent blank row
                    fwrite(s, m_nChannels, 4, f);
                }
            }                                                                            //end for all rows
        } else    {
            memset(s, 0, m_nChannels*4);            //if pattern does not exist
            for (UINT i=0; i<64; i++) {                    //invent blank pattern
                fwrite(s, m_nChannels, 4, f);
            }
        }
    }                                                                            //end for all patterns

    //Check for unsaved patterns
#ifdef MODPLUG_TRACKER
    if(GetpModDoc() != nullptr)
    {
        for(UINT ipat = nbp; ipat < MAX_PATTERNS; ipat++)
        {
            if(Patterns[ipat])
            {
                GetpModDoc()->AddToLog(_T("Warning: This track contains at least one pattern after the highest pattern number referred to in the sequence. Such patterns are not saved in the .mod format.\n"));
                break;
            }
        }
    }
#endif

    // Writing instruments
    for (UINT ismpd = 1; ismpd <= 31; ismpd++) if (inslen[ismpd])
    {
        modsample_t *pSmp = &Samples[insmap[ismpd]];
        if(bCompatibilityExport == true) // first two bytes have to be 0 due to PT's one-shot loop ("no loop")
        {
            int iOverwriteLen = 2 * pSmp->GetBytesPerSample();
            memset(pSmp->sample_data, 0, bad_min(iOverwriteLen, pSmp->GetSampleSizeInBytes()));
        }
        UINT flags = RS_PCM8S;
#ifndef NO_PACKING
        if (!(pSmp->flags & (CHN_16BIT|CHN_STEREO)))
        {
            if ((nPacking) && (CanPackSample(pSmp->sample_data, inslen[ismpd], nPacking)))
            {
                fwrite("ADPCM", 1, 5, f);
                flags = RS_ADPCM4;
            }
        }
#endif
        WriteSample(f, pSmp, flags, inslen[ismpd]);
        // write padding byte if the sample size is odd.
        if((pSmp->length & 1) && !nPacking)
        {
            int8_t padding = 0;
            fwrite(&padding, 1, 1, f);
        }
    }
    fclose(f);
    return true;
}

#endif // MODPLUG_NO_FILESAVE