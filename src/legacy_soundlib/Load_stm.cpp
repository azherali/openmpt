/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *
*/

#include "stdafx.h"
#include "Loaders.h"

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

#pragma pack(1)

typedef struct tagSTMNOTE
{
    uint8_t note;
    uint8_t insvol;
    uint8_t volcmd;
    uint8_t cmdinf;
} STMNOTE;


// Raw STM sampleinfo struct:
typedef struct tagSTMSAMPLE
{
    CHAR filename[14];        // Can't have long comments - just legacy_filename comments :)
    uint16_t reserved;                // ISA in memory when in ST 2
    uint16_t length;                // Sample length
    uint16_t loopbeg;                // Loop start point
    uint16_t loopend;                // Loop end point
    uint8_t volume;                // Volume
    uint8_t reserved2;                // More reserved crap
    uint16_t c2spd;                        // Good old c2spd
    uint8_t reserved3[6];        // Yet more of PSi's reserved crap
} STMSAMPLE;


// Raw STM header struct:
typedef struct tagSTMHEADER
{
    char songname[20];
    char trackername[8];        // !SCREAM! for ST 2.xx
    CHAR unused;                        // 0x1A
    CHAR filetype;                        // 1=song, 2=module (only 2 is supported, of course) :)
    CHAR ver_major;                        // Like 2
    CHAR ver_minor;                        // "ditto"
    uint8_t inittempo;                        // initspeed= stm inittempo>>4
    uint8_t numpat;                        // number of patterns
    uint8_t globalvol;                        // <- WoW! a RiGHT TRiANGLE =8*)
    uint8_t reserved[13];                // More of PSi's internal crap
    STMSAMPLE sample[31];        // STM sample data
    uint8_t patorder[128];                // Docs say 64 - actually 128
} STMHEADER;

#pragma pack()



bool module_renderer::ReadSTM(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    STMHEADER *phdr = (STMHEADER *)lpStream;
    uint32_t dwMemPos = 0;

    if ((!lpStream) || (dwMemLength < sizeof(STMHEADER))) return false;
    if ((phdr->filetype != 2) || (phdr->unused != 0x1A)
     || ((_strnicmp(phdr->trackername, "!SCREAM!", 8))
      && (_strnicmp(phdr->trackername, "BMOD2STM", 8)))) return false;
    assign_without_padding(this->song_name, phdr->songname, 20);
    // Read STM header
    m_nType = MOD_TYPE_STM;
    m_nSamples = 31;
    m_nChannels = 4;
    m_nInstruments = 0;
    m_nMinPeriod = 64;
    m_nMaxPeriod = 0x7FFF;
    m_nDefaultSpeed = phdr->inittempo >> 4;
    if (m_nDefaultSpeed < 1) m_nDefaultSpeed = 1;
    m_nDefaultTempo = 125;
    m_nDefaultGlobalVolume = phdr->globalvol << 2;
    if (m_nDefaultGlobalVolume > MAX_GLOBAL_VOLUME) m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
    Order.ReadAsByte(phdr->patorder, 128, 128);
    // Setting up channels
    for (UINT nSet=0; nSet<4; nSet++)
    {
            ChnSettings[nSet].dwFlags = 0;
            ChnSettings[nSet].nVolume = 64;
            ChnSettings[nSet].nPan = (nSet & 1) ? 0x40 : 0xC0;
    }
    // Reading samples
    for (UINT nIns=0; nIns<31; nIns++)
    {
            modsample_t *pIns = &Samples[nIns+1];
            STMSAMPLE *pStm = &phdr->sample[nIns];  // STM sample data
            memcpy(pIns->legacy_filename, pStm->filename, 13);
            memcpy(m_szNames[nIns+1], pStm->filename, 12);
            SpaceToNullStringFixed<12>(pIns->legacy_filename);
            SpaceToNullStringFixed<12>(m_szNames[nIns + 1]);
            pIns->c5_samplerate = LittleEndianW(pStm->c2spd);
            pIns->global_volume = 64;
            pIns->default_volume = pStm->volume << 2;
            if (pIns->default_volume > 256) pIns->default_volume = 256;
            pIns->length = LittleEndianW(pStm->length);
            if ((pIns->length < 4) || (!pIns->default_volume)) pIns->length = 0;
            pIns->loop_start = LittleEndianW(pStm->loopbeg);
            pIns->loop_end = LittleEndianW(pStm->loopend);
            if ((pIns->loop_end > pIns->loop_start) && (pIns->loop_end != 0xFFFF))
            {
                    pIns->flags |= CHN_LOOP;
                    pIns->loop_end = bad_min(pIns->loop_end, pIns->length);
            }
    }
    dwMemPos = sizeof(STMHEADER);
    for (UINT nOrd = 0; nOrd < 128; nOrd++) if (Order[nOrd] >= 99) Order[nOrd] = Order.GetInvalidPatIndex();
    UINT nPatterns = phdr->numpat;
    for (UINT nPat=0; nPat<nPatterns; nPat++)
    {
            if (dwMemPos + 64*4*4 > dwMemLength) return true;
            if(Patterns.Insert(nPat, 64))
                    return true;
            modplug::tracker::modevent_t *m = Patterns[nPat];
            STMNOTE *p = (STMNOTE *)(lpStream + dwMemPos);
            for (UINT n=0; n<64*4; n++, p++, m++)
            {
                    UINT note,ins,vol,cmd;
                    // extract the various information from the 4 bytes that
                    // make up a single note
                    note = p->note;
                    ins = p->insvol >> 3;
                    vol = (p->insvol & 0x07) + (p->volcmd >> 1);
                    cmd = p->volcmd & 0x0F;
                    if ((ins) && (ins < 32)) m->instr = ins;
                    // special values of [SBYTE0] are handled here ->
                    // we have no idea if these strange values will ever be encountered
                    // but it appears as though stms sound correct.
                    if ((note == 0xFE) || (note == 0xFC)) m->note = 0xFE; else
                    // if note < 251, then all three bytes are stored in the file
                    if (note < 0xFC) m->note = (note >> 4)*12 + (note&0xf) + 37;
                    if (vol <= 64) { m->volcmd = VolCmdVol; m->vol = vol; }
                    m->param = p->cmdinf;
                    switch(cmd)
                    {
                    // Axx set speed to xx
                    case 1:        m->command = CmdSpeed; m->param >>= 4; break;
                    // Bxx position jump
                    case 2:        m->command = CmdPositionJump; break;
                    // Cxx patternbreak to row xx
                    case 3:        m->command = CmdPatternBreak; m->param = (m->param & 0xF0) * 10 + (m->param & 0x0F);        break;
                    // Dxy volumeslide
                    case 4:        m->command = CmdVolSlide; break;
                    // Exy toneslide down
                    case 5:        m->command = CmdPortaDown; break;
                    // Fxy toneslide up
                    case 6:        m->command = CmdPortaUp; break;
                    // Gxx Tone portamento,speed xx
                    case 7:        m->command = CmdPorta; break;
                    // Hxy vibrato
                    case 8:        m->command = CmdVibrato; break;
                    // Ixy tremor, ontime x, offtime y
                    case 9:        m->command = CmdTremor; break;
                    // Jxy arpeggio
                    case 10: m->command = CmdArpeggio; break;
                    // Kxy Dual command H00 & Dxy
                    case 11: m->command = CmdVibratoVolSlide; break;
                    // Lxy Dual command G00 & Dxy
                    case 12: m->command = CmdPortaVolSlide; break;
                    // Xxx amiga command 8xx
                    case 0x18:        m->command = CmdPanning8; break;
                    default:
                            m->command = CmdNone;
                            m->param = 0;
                    }
            }
            dwMemPos += 64*4*4;
    }
    // Reading Samples
    for (UINT nSmp=1; nSmp<=31; nSmp++)
    {
            modsample_t *pIns = &Samples[nSmp];
            dwMemPos = (dwMemPos + 15) & (~15);
            if (pIns->length)
            {
                    UINT nPos = ((UINT)phdr->sample[nSmp-1].reserved) << 4;
                    if ((nPos >= sizeof(STMHEADER)) && (nPos <= dwMemLength) && (pIns->length <= dwMemLength-nPos)) dwMemPos = nPos;
                    if (dwMemPos < dwMemLength)
                    {
                            dwMemPos += ReadSample(pIns, RS_PCM8S, (LPSTR)(lpStream+dwMemPos),dwMemLength-dwMemPos);
                    }
            }
    }
    return true;
}