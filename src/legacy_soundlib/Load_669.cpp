/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
 *                    OpenMPT dev(s)        (miscellaneous modifications)
*/

////////////////////////////////////////////////////////////
// 669 Composer / UNIS 669 module loader
////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Loaders.h"

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

typedef struct tagFILEHEADER669
{
    uint16_t sig;                            // 'if' or 'JN'
    char songmessage[108];    // Song Message
    uint8_t samples;                    // number of samples (1-64)
    uint8_t patterns;                    // number of patterns (1-128)
    uint8_t restartpos;
    uint8_t orders[128];
    uint8_t tempolist[128];
    uint8_t breaks[128];
} FILEHEADER669;


typedef struct tagSAMPLE669
{
    uint8_t filename[13];
    uint8_t length[4];    // when will somebody think about uint32_t align ???
    uint8_t loopstart[4];
    uint8_t loopend[4];
} SAMPLE669;


bool module_renderer::Read669(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    BOOL b669Ext;
    const FILEHEADER669 *pfh = (const FILEHEADER669 *)lpStream;
    const SAMPLE669 *psmp = (const SAMPLE669 *)(lpStream + 0x1F1);
    uint32_t dwMemPos = 0;

    if ((!lpStream) || (dwMemLength < sizeof(FILEHEADER669))) return false;
    if ((LittleEndianW(pfh->sig) != 0x6669) && (LittleEndianW(pfh->sig) != 0x4E4A)) return false;
    b669Ext = (LittleEndianW(pfh->sig) == 0x4E4A) ? TRUE : FALSE;
    if ((!pfh->samples) || (pfh->samples > 64) || (pfh->restartpos >= 128)
     || (!pfh->patterns) || (pfh->patterns > 128)) return false;
    uint32_t dontfuckwithme = 0x1F1 + pfh->samples * sizeof(SAMPLE669) + pfh->patterns * 0x600;
    if (dontfuckwithme > dwMemLength) return false;
    for (UINT ichk=0; ichk<pfh->samples; ichk++)
    {
        uint32_t len = LittleEndian(*((uint32_t *)(&psmp[ichk].length)));
        dontfuckwithme += len;
    }
    if (dontfuckwithme  - 0x1F1 > dwMemLength) return false;
    // That should be enough checking: this must be a 669 module.
    m_nType = MOD_TYPE_669;
    m_dwSongFlags |= SONG_LINEARSLIDES;
    m_nMinPeriod = 28 << 2;
    m_nMaxPeriod = 1712 << 3;
    m_nDefaultTempo = 125;
    m_nDefaultSpeed = 6;
    m_nChannels = 8;

    assign_without_padding(this->song_name, pfh->songmessage, 16);

    m_nSamples = pfh->samples;
    for (modplug::tracker::sampleindex_t nSmp = 1; nSmp <= m_nSamples; nSmp++, psmp++)
    {
        uint32_t len = LittleEndian(*((uint32_t *)(&psmp->length)));
        uint32_t loopstart = LittleEndian(*((uint32_t *)(&psmp->loopstart)));
        uint32_t loopend = LittleEndian(*((uint32_t *)(&psmp->loopend)));
        if (len > MAX_SAMPLE_LENGTH) len = MAX_SAMPLE_LENGTH;
        if ((loopend > len) && (!loopstart)) loopend = 0;
        if (loopend > len) loopend = len;
        if (loopstart + 4 >= loopend) loopstart = loopend = 0;
        Samples[nSmp].length = len;
        Samples[nSmp].loop_start = loopstart;
        Samples[nSmp].loop_end = loopend;
        if (loopend) Samples[nSmp].flags |= CHN_LOOP;
        memcpy(m_szNames[nSmp], psmp->filename, 13);
        SpaceToNullStringFixed<13>(m_szNames[nSmp]);
        Samples[nSmp].default_volume = 256;
        Samples[nSmp].global_volume = 64;
        Samples[nSmp].default_pan = 128;
    }

    // Song Message
    ReadFixedLineLengthMessage((uint8_t *)(&pfh->songmessage), 108, 36, 0);

    // Reading Orders
    Order.ReadAsByte(pfh->orders, 128, 128);
    m_nRestartPos = pfh->restartpos;
    if (Order[m_nRestartPos] >= pfh->patterns) m_nRestartPos = 0;
    // Reading Pattern Break Locations
    for (UINT npan=0; npan<8; npan++)
    {
        ChnSettings[npan].nPan = (npan & 1) ? 0x30 : 0xD0;
        ChnSettings[npan].nVolume = 64;
    }

    // Reading Patterns
    dwMemPos = 0x1F1 + pfh->samples * 25;
    for (UINT npat=0; npat<pfh->patterns; npat++)
    {
        if(Patterns.Insert(npat, 64))
            break;

        modplug::tracker::modevent_t *m = Patterns[npat];
        const uint8_t *p = lpStream + dwMemPos;
        for (UINT row=0; row<64; row++)
        {
            modplug::tracker::modevent_t *mspeed = m;
            if ((row == pfh->breaks[npat]) && (row != 63))
            {
                for (UINT i=0; i<8; i++)
                {
                    m[i].command = CmdPatternBreak;
                    m[i].param = 0;
                }
            }
            for (UINT n=0; n<8; n++, m++, p+=3)
            {
                UINT note = p[0] >> 2;
                UINT instr = ((p[0] & 0x03) << 4) | (p[1] >> 4);
                UINT vol = p[1] & 0x0F;
                if (p[0] < 0xFE)
                {
                    m->note = note + 37;
                    m->instr = instr + 1;
                }
                if (p[0] <= 0xFE)
                {
                    m->volcmd = VolCmdVol;
                    m->vol = (vol << 2) + 2;
                }
                if (p[2] != 0xFF)
                {
                    UINT command = p[2] >> 4;
                    UINT param = p[2] & 0x0F;
                    switch(command)
                    {
                    case 0x00:    command = CmdPortaUp; break;
                    case 0x01:    command = CmdPortaDown; break;
                    case 0x02:    command = CmdPorta; break;
                    case 0x03:    command = CmdModCmdEx; param |= 0x50; break;
                    case 0x04:    command = CmdVibrato; param |= 0x40; break;
                    case 0x05:    if (param) command = CmdSpeed; else command = 0; param += 2; break;
                    case 0x06:    if (param == 0) { command = CmdPanningSlide; param = 0xFE; } else
                                if (param == 1) { command = CmdPanningSlide; param = 0xEF; } else
                                command = 0;
                                break;
                    default:    command = 0;
                    }
                    if (command)
                    {
                        if (command == CmdSpeed) mspeed = NULL;
                        //XXXih: gross
                        m->command = (modplug::tracker::cmd_t) command;
                        m->param = param;
                    }
                }
            }
            if ((!row) && (mspeed))
            {
                for (UINT i=0; i<8; i++) if (!mspeed[i].command)
                {
                    mspeed[i].command = CmdSpeed;
                    mspeed[i].param = pfh->tempolist[npat] + 2;
                    break;
                }
            }
        }
        dwMemPos += 0x600;
    }

    // Reading Samples
    for (UINT n=1; n<=m_nSamples; n++)
    {
        UINT len = Samples[n].length;
        if (dwMemPos >= dwMemLength) break;
        if (len > 4) ReadSample(&Samples[n], RS_PCM8U, (LPSTR)(lpStream+dwMemPos), dwMemLength - dwMemPos);
        dwMemPos += len;
    }
    return true;
}