/*
 * This source code is public domain.
 *
 * Purpose: Load GDM (BWSB Soundsystem) modules
 * Authors: Johannes Schultz
 *
 * This code is partly based on zilym's original code / specs (which are utterly wrong :P).
 * Thanks to the MenTaLguY for gdm.txt and ajs for gdm2s3m and some hints.
 *
 * Hint 1: Most (all?) of the unsupported features were not supported in 2GDM / BWSB either.
 * Hint 2: Files will be played like their original formats would be played in MPT, so no
 *         BWSB quirks including crashes and freezes are supported. :-P
 */

#include "stdafx.h"
#include "Loaders.h"
#ifdef MODPLUG_TRACKER
#include "../moddoc.h"
#endif // MODPLUG_TRACKER

#pragma pack(1)

typedef struct _GDMHEADER
{
    uint32_t ID;                                            // ID: 'GDM�'
    char   SongTitle[32];                    // Music's title
    char   SongMusician[32];            // Name of music's composer
    char   DOSEOF[3];                            // 13, 10, 26
    uint32_t ID2;                                            // ID: 'GMFS'
    uint8_t  FormMajorVer;                    // Format major version
    uint8_t  FormMinorVer;                    // Format minor version
    uint16_t TrackID;                                    // Composing Tracker ID code (00 = 2GDM)
    uint8_t  TrackMajorVer;                    // Tracker's major version
    uint8_t  TrackMinorVer;                    // Tracker's minor version
    uint8_t  PanMap[32];                            // 0-Left to 15-Right, 255-N/U
    uint8_t  MastVol;                                    // Range: 0...64
    uint8_t  Tempo;                                    // Initial music tempo (6)
    uint8_t  BPM;                                            // Initial music BPM (125)
    uint16_t FormOrigin;                            // Original format ID:
        // 1-MOD, 2-MTM, 3-S3M, 4-669, 5-FAR, 6-ULT, 7-STM, 8-MED
        // (versions of 2GDM prior to v1.15 won't set this correctly)

    uint32_t OrdOffset;
    uint8_t  NOO;                                            // Number of orders in module - 1
    uint32_t PatOffset;
    uint8_t  NOP;                                            // Number of patterns in module - 1
    uint32_t SamHeadOffset;
    uint32_t SamOffset;
    uint8_t  NOS;                                            // Number of samples in module - 1
    uint32_t MTOffset;                            // Offset of song message
    uint32_t MTLength;
    uint32_t SSOffset;                            // Offset of scrolly script (huh?)
    uint16_t SSLength;
    uint32_t TGOffset;                            // Offset of text graphic (huh?)
    uint16_t TGLength;
} GDMHEADER, *PGDMHEADER;

typedef struct _GDMSAMPLEHEADER
{
    char   SamName[32];            // sample's name
    char   FileName[12];    // sample's legacy_filename
    uint8_t  EmsHandle;            // useless
    uint32_t Length;                    // length in bytes
    uint32_t LoopBegin;            // loop start in samples
    uint32_t LoopEnd;                    // loop end in samples
    uint8_t  Flags;                    // misc. flags
    uint16_t C4Hertz;                    // frequency
    uint8_t  Volume;                    // default volume
    uint8_t  Pan;                            // default pan
} GDMSAMPLEHEADER, *PGDMSAMPLEHEADER;

#pragma pack()

static MODTYPE GDMHeader_Origin[] =
{
    MOD_TYPE_NONE, MOD_TYPE_MOD, MOD_TYPE_MTM, MOD_TYPE_S3M, MOD_TYPE_669, MOD_TYPE_FAR, MOD_TYPE_ULT, MOD_TYPE_STM, MOD_TYPE_MED
};

bool module_renderer::ReadGDM(const uint8_t * const lpStream, const uint32_t dwMemLength)
//-----------------------------------------------------------------------
{
    if ((!lpStream) || (dwMemLength < sizeof(GDMHEADER))) return false;

    const PGDMHEADER pHeader = (PGDMHEADER)lpStream;

    // is it a valid GDM file?
    if(    (LittleEndian(pHeader->ID) != 0xFE4D4447) || //GDM�
        (pHeader->DOSEOF[0] != 13 || pHeader->DOSEOF[1] != 10 || pHeader->DOSEOF[2] != 26) || //CR+LF+EOF
        (LittleEndian(pHeader->ID2) != 0x53464D47)) return false; //GMFS

    // there are no other format versions...
    if(pHeader->FormMajorVer != 1 || pHeader->FormMinorVer != 0)
        return false;

    // 1-MOD, 2-MTM, 3-S3M, 4-669, 5-FAR, 6-ULT, 7-STM, 8-MED
    m_nType = GDMHeader_Origin[pHeader->FormOrigin % CountOf(GDMHeader_Origin)];
    if(m_nType == MOD_TYPE_NONE)
        return false;

    // interesting question: Is TrackID, TrackMajorVer, TrackMinorVer relevant? The only TrackID should be 0 - 2GDM.exe, so the answer would be no.

    // song name
    memset(m_szNames, 0, sizeof(m_szNames));
    assign_without_padding(this->song_name, pHeader->SongTitle, 32);

    // read channel pan map... 0...15 = channel panning, 16 = surround channel, 255 = channel does not exist
    m_nChannels = 32;
    for(modplug::tracker::chnindex_t i = 0; i < 32; i++)
    {
        if(pHeader->PanMap[i] < 16)
        {
            ChnSettings[i].nPan = bad_min((pHeader->PanMap[i] << 4) + 8, 256);
        }
        else if(pHeader->PanMap[i] == 16)
        {
            ChnSettings[i].nPan = 128;
            ChnSettings[i].dwFlags |= CHN_SURROUND;
        }
        else if(pHeader->PanMap[i] == 0xff)
        {
            m_nChannels = i;
            break;
        }
    }

    m_nDefaultGlobalVolume = bad_min(pHeader->MastVol << 2, 256);
    m_nDefaultSpeed = pHeader->Tempo;
    m_nDefaultTempo = pHeader->BPM;
    m_nRestartPos = 0; // not supported in this format, so use the default value
    m_nSamplePreAmp = 48; // dito
    m_nVSTiVolume = 48; // dito

    uint32_t iSampleOffset  = LittleEndian(pHeader->SamOffset),
           iPatternsOffset = LittleEndian(pHeader->PatOffset);

    const uint32_t iOrdOffset = LittleEndian(pHeader->OrdOffset), iSamHeadOffset = LittleEndian(pHeader->SamHeadOffset),
                 iMTOffset = LittleEndian(pHeader->MTOffset), iMTLength = LittleEndian(pHeader->MTLength),
                 iSSOffset = LittleEndian(pHeader->SSOffset), iSSLength = LittleEndianW(pHeader->SSLength),
                 iTGOffset = LittleEndian(pHeader->TGOffset), iTGLength = LittleEndianW(pHeader->TGLength);


    // check if offsets are valid. we won't read the scrolly text or text graphics, but invalid pointers would probably indicate a broken file...
    if(       dwMemLength < iOrdOffset || dwMemLength - iOrdOffset < pHeader->NOO
        || dwMemLength < iPatternsOffset
        || dwMemLength < iSamHeadOffset || dwMemLength - iSamHeadOffset < (pHeader->NOS + 1) * sizeof(GDMSAMPLEHEADER)
        || dwMemLength < iSampleOffset
        || dwMemLength < iMTOffset || dwMemLength - iMTOffset < iMTLength
        || dwMemLength < iSSOffset || dwMemLength - iSSOffset < iSSLength
        || dwMemLength < iTGOffset || dwMemLength - iTGOffset < iTGLength)
        return false;

    // read orders
    Order.ReadAsByte(lpStream + iOrdOffset, pHeader->NOO + 1, dwMemLength - iOrdOffset);

    // read samples
    m_nSamples = pHeader->NOS + 1;

    for(modplug::tracker::sampleindex_t iSmp = 1; iSmp <= m_nSamples; iSmp++)
    {
        const PGDMSAMPLEHEADER pSample = (PGDMSAMPLEHEADER)(lpStream + iSamHeadOffset + (iSmp - 1) * sizeof(GDMSAMPLEHEADER));

        // sample header

        memcpy(m_szNames[iSmp], pSample->SamName, 32);
        SpaceToNullStringFixed<31>(m_szNames[iSmp]);
        memcpy(Samples[iSmp].legacy_filename, pSample->FileName, 12);
        SpaceToNullStringFixed<12>(Samples[iSmp].legacy_filename);

        Samples[iSmp].c5_samplerate = LittleEndianW(pSample->C4Hertz);
        Samples[iSmp].global_volume = 256; // not supported in this format
        Samples[iSmp].length = bad_min(LittleEndian(pSample->Length), MAX_SAMPLE_LENGTH); // in bytes
        Samples[iSmp].loop_start = bad_min(LittleEndian(pSample->LoopBegin), Samples[iSmp].length); // in samples
        Samples[iSmp].loop_end = bad_min(LittleEndian(pSample->LoopEnd) - 1, Samples[iSmp].length); // dito
        FrequencyToTranspose(&Samples[iSmp]); // set transpose + finetune for mod files

        // fix transpose + finetune for some rare cases where transpose is not C-5 (e.g. sample 4 in wander2.gdm)
        if(m_nType == MOD_TYPE_MOD)
        {
            while(Samples[iSmp].RelativeTone != 0)
            {
                if(Samples[iSmp].RelativeTone > 0)
                {
                    Samples[iSmp].RelativeTone -= 1;
                    Samples[iSmp].nFineTune += 128;
                }
                else
                {
                    Samples[iSmp].RelativeTone += 1;
                    Samples[iSmp].nFineTune -= 128;
                }
            }
        }

        if(pSample->Flags & 0x01) Samples[iSmp].flags |= CHN_LOOP; // loop sample

        if(pSample->Flags & 0x04)
        {
            Samples[iSmp].default_volume = bad_min(pSample->Volume << 2, 256); // 0...64, 255 = no default volume
        }
        else
        {
            Samples[iSmp].default_volume = 256; // default volume
        }

        if(pSample->Flags & 0x08) // default panning is used
        {
            Samples[iSmp].flags |= CHN_PANNING;
            Samples[iSmp].default_pan = (pSample->Pan > 15) ? 128 : bad_min((pSample->Pan << 4) + 8, 256); // 0...15, 16 = surround (not supported), 255 = no default panning
        }
        else
        {
            Samples[iSmp].default_pan = 128;
        }

        /* note: apparently (and according to zilym), 2GDM doesn't handle 16 bit or stereo samples properly.
           so those flags are pretty much meaningless and we will ignore them... in fact, samples won't load as expected if we don't! */

        UINT iSampleFormat;
        if(pSample->Flags & 0x02) // 16 bit
        {
            if(pSample->Flags & 0x20) // stereo
                iSampleFormat = RS_PCM16U; // should be RS_STPCM16U but that breaks the sample reader
            else
                iSampleFormat = RS_PCM16U;
        }
        else // 8 bit
        {
            if(pSample->Flags & 0x20) // stereo
                iSampleFormat = RS_PCM8U; // should be RS_STPCM8U - dito
            else
                iSampleFormat = RS_PCM8U;
        }

        // according to zilym, LZW support has never been finished, so this is also practically useless. Just ignore the flag.
        // if(pSample->Flags & 0x10) {...}

        // read sample data
        ReadSample(&Samples[iSmp], iSampleFormat, reinterpret_cast<LPCSTR>(lpStream + iSampleOffset), dwMemLength - iSampleOffset);

        iSampleOffset += bad_min(LittleEndian(pSample->Length), dwMemLength - iSampleOffset);

    }

    // read patterns
    Patterns.ResizeArray(bad_max(MAX_PATTERNS, pHeader->NOP + 1));

    bool bS3MCommandSet = (GetBestSaveFormat() & (MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT)) != 0 ? true : false;

    // we'll start at position iPatternsOffset and decode all patterns
    for (modplug::tracker::patternindex_t iPat = 0; iPat < pHeader->NOP + 1; iPat++)
    {

        if(iPatternsOffset + 2 > dwMemLength) break;
        uint16_t iPatternLength = LittleEndianW(*(uint16_t *)(lpStream + iPatternsOffset)); // pattern length including the two "length" bytes
        if(iPatternLength > dwMemLength || iPatternsOffset > dwMemLength - iPatternLength) break;

        if(Patterns.Insert(iPat, 64))
            break;

        // position in THIS pattern
        uint32_t iPatternPos = iPatternsOffset + 2;

        modplug::tracker::modevent_t *p = Patterns[iPat];

        for(UINT iRow = 0; iRow < 64; iRow++)
        {
            while(true) // zero byte = next row
            {
                if(iPatternPos + 1 > dwMemLength) break;

                uint8_t bChannel = lpStream[iPatternPos++];

                if(bChannel == 0) break; // next row, please!

                UINT channel = bChannel & 0x1f;
                if(channel >= m_nChannels) break; // better safe than sorry!

                modplug::tracker::modevent_t *m = &p[iRow * m_nChannels + channel];

                if(bChannel & 0x20)
                {
                    // note and sample follows
                    if(iPatternPos + 2 > dwMemLength) break;
                    uint8_t bNote = lpStream[iPatternPos++];
                    uint8_t bSample = lpStream[iPatternPos++];

                    bNote = (bNote & 0x7F) - 1; // this format doesn't have note cuts
                    if(bNote < 0xF0) bNote = (bNote & 0x0F) + 12 * (bNote >> 4) + 13;
                    if(bNote == 0xFF) bNote = NoteNone;
                    m->note = bNote;
                    m->instr = bSample;

                }

                if(bChannel & 0x40)
                {
                    // effect(s) follow

                    m->command = CmdNone;
                    m->volcmd = VolCmdNone;

                    while(true)
                    {
                        if(iPatternPos + 2 > dwMemLength) break;
                        uint8_t bEffect = lpStream[iPatternPos++];
                        uint8_t bEffectData = lpStream[iPatternPos++];

                        uint8_t command = bEffect & 0x1F, param = bEffectData;
                        uint8_t volcommand = CmdNone, volparam = param;

                        switch(command)
                        {
                        case 0x01: command = CmdPortaUp; if(param >= 0xE0) param = 0xDF; break;
                        case 0x02: command = CmdPortaDown; if(param >= 0xE0) param = 0xDF; break;
                        case 0x03: command = CmdPorta; break;
                        case 0x04: command = CmdVibrato; break;
                        case 0x05: command = CmdPortaVolSlide; if (param & 0xF0) param &= 0xF0; break;
                        case 0x06: command = CmdVibratoVolSlide; if (param & 0xF0) param &= 0xF0; break;
                        case 0x07: command = CmdTremolo; break;
                        case 0x08: command = CmdTremor; break;
                        case 0x09: command = CmdOffset; break;
                        case 0x0A: command = CmdVolSlide; break;
                        case 0x0B: command = CmdPositionJump; break;
                        case 0x0C:
                            if(bS3MCommandSet)
                            {
                                command = CmdNone;
                                volcommand = VolCmdVol;
                                volparam = bad_min(param, 64);
                            }
                            else
                            {
                                command = CmdVol;
                                param = bad_min(param, 64);
                            }
                            break;
                        case 0x0D: command = CmdPatternBreak; break;
                        case 0x0E:
                            if(bS3MCommandSet)
                            {
                                command = CmdS3mCmdEx;
                                // need to do some remapping
                                switch(param >> 4)
                                {
                                case 0x0:
                                    // set filter
                                    break;
                                case 0x1:
                                    // fine porta up
                                    command = CmdPortaUp;
                                    param = 0xF0 | (param & 0x0F);
                                    break;
                                case 0x2:
                                    // fine porta down
                                    command = CmdPortaDown;
                                    param = 0xF0 | (param & 0x0F);
                                    break;
                                case 0x3:
                                    // glissando control
                                    param = 0x10 | (param & 0x0F);
                                    break;
                                case 0x4:
                                    // vibrato waveform
                                    param = 0x30 | (param & 0x0F);
                                    break;
                                case 0x5:
                                    // set finetune
                                    param = 0x20 | (param & 0x0F);
                                    break;
                                case 0x6:
                                    // pattern loop
                                    param = 0xB0 | (param & 0x0F);
                                    break;
                                case 0x7:
                                    // tremolo waveform
                                    param = 0x40 | (param & 0x0F);
                                    break;
                                case 0x8:
                                    // extra fine porta up
                                    command = CmdPortaUp;
                                    param = 0xE0 | (param & 0x0F);
                                    break;
                                case 0x9:
                                    // extra fine porta down
                                    command = CmdPortaDown;
                                    param = 0xE0 | (param & 0x0F);
                                    break;
                                case 0xA:
                                    // fine volume up
                                    command = CmdVolSlide;
                                    param = ((param & 0x0F) << 4) | 0x0F;
                                    break;
                                case 0xB:
                                    // fine volume down
                                    command = CmdVolSlide;
                                    param = 0xF0 | (param & 0x0F);
                                    break;
                                case 0xC:
                                    // note cut
                                    break;
                                case 0xD:
                                    // note delay
                                    break;
                                case 0xE:
                                    // pattern delay
                                    break;
                                case 0xF:
                                    command = CmdModCmdEx;
                                    // invert loop / funk repeat
                                    break;
                                }
                            }
                            else
                            {
                                command = CmdModCmdEx;
                            }
                            break;
                        case 0x0F: command = CmdSpeed; break;
                        case 0x10: command = CmdArpeggio; break;
                        case 0x11: command = CmdNone /* set internal flag */; break;
                        case 0x12:
                            if((!bS3MCommandSet) && ((param & 0xF0) == 0))
                            {
                                // retrig in "mod style"
                                command = CmdModCmdEx;
                                param = 0x90 | (param & 0x0F);
                            }
                            else
                            {
                                // either "s3m style" is required or this format is like s3m anyway
                                command = CmdRetrig;
                            }
                            break;
                        case 0x13: command = CmdGlobalVol; break;
                        case 0x14: command = CmdFineVibrato; break;
                        case 0x1E:
                            switch(param >> 4)
                            {
                            case 0x0:
                                switch(param & 0x0F)
                                {
                                case 0x0: command = CmdS3mCmdEx; param = 0x90; break;
                                case 0x1: command = CmdPanning8; param = 0xA4; break;
                                case 0x2: command = CmdNone /* set normal loop - not implemented in 2GDM */; break;
                                case 0x3: command = CmdNone /* set bidi loop - dito */; break;
                                case 0x4: command = CmdS3mCmdEx; param = 0x9E; break;
                                case 0x5: command = CmdS3mCmdEx; param = 0x9F; break;
                                case 0x6: command = CmdNone /* monaural sample - dito */; break;
                                case 0x7: command = CmdNone /* stereo sample - dito */; break;
                                case 0x8: command = CmdNone /* stop sample on end - dito */; break;
                                case 0x9: command = CmdNone /* loop sample on end - dito */; break;
                                default: command = CmdNone; break;
                                }
                                break;
                            case 0x8:
                                command = (bS3MCommandSet) ? CmdS3mCmdEx : CmdModCmdEx;
                                break;
                            case 0xD:
                                // adjust frequency (increment in hz) - not implemented in 2GDM
                                command = CmdNone;
                                break;
                            default: command = CmdNone; break;
                            }
                            break;
                        case 0x1F: command = CmdTempo; break;
                        default: command = CmdNone; break;
                        }

                        if(command != CmdNone)
                        {
                            // move pannings to volume column - should never happen
                            if(m->command == CmdS3mCmdEx && ((m->param >> 4) == 0x8) && volcommand == CmdNone)
                            {
                                volcommand = VolCmdPan;
                                volparam = ((param & 0x0F) << 2) + 2;
                            }

                            //XXXih: gross!
                            m->command = (modplug::tracker::cmd_t) command;
                            m->param = param;
                        }
                        if(volcommand != CmdNone)
                        {
                            //XXXih: gross!
                            m->volcmd = (modplug::tracker::volcmd_t) volcommand;
                            m->vol = volparam;
                        }

                        if(!(bEffect & 0x20)) break; // no other effect follows
                    }

                }

            }
        }

        iPatternsOffset += iPatternLength;
    }

    // read song comments
    if(iMTLength > 0)
    {
        ReadMessage(lpStream + iMTOffset, iMTLength, leAutodetect);
    }

    return true;

}