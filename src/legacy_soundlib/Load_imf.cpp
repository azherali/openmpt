/*
 * Purpose: Load IMF (Imago Orpheus) modules
 * Authors: Storlek (Original author - http://schismtracker.org/)
 *                    Johannes Schultz (OpenMPT Port, tweaks)
 *
 * Thanks to Storlek for allowing me to use this code!
 */

#include "stdafx.h"
#include "Loaders.h"
#ifdef MODPLUG_TRACKER
#include "../moddoc.h"
#endif // MODPLUG_TRACKER

#pragma pack(1)

struct IMFCHANNEL
{
    char name[12];    // Channel name (ASCIIZ-String, bad_max 11 chars)
    uint8_t chorus;    // Default chorus
    uint8_t reverb;    // Default reverb
    uint8_t panning;    // Pan positions 00-FF
    uint8_t status;    // Channel status: 0 = enabled, 1 = mute, 2 = disabled (ignore effects!)
};

struct IMFHEADER
{
    char title[32];                            // Songname (ASCIIZ-String, bad_max. 31 chars)
    uint16_t ordnum;                            // Number of orders saved
    uint16_t patnum;                            // Number of patterns saved
    uint16_t insnum;                            // Number of instruments saved
    uint16_t flags;                            // Module flags (&1 => linear)
    uint8_t unused1[8];
    uint8_t tempo;                            // Default tempo (Axx, 1..255)
    uint8_t bpm;                                    // Default beats per minute (BPM) (Txx, 32..255)
    uint8_t master;                            // Default mastervolume (Vxx, 0..64)
    uint8_t amp;                                    // Amplification factor (mixing volume, 4..127)
    uint8_t unused2[8];
    char im10[4];                            // 'IM10'
    IMFCHANNEL channels[32];    // Channel settings
    uint8_t orderlist[256];            // Order list (0xff = +++; blank out anything beyond ordnum)
};

enum
{
    IMF_ENV_VOL = 0,
    IMF_ENV_PAN = 1,
    IMF_ENV_FILTER = 2,
};

struct IMFENVELOPE
{
    uint8_t points;            // Number of envelope points
    uint8_t sustain;            // Envelope sustain point
    uint8_t loop_start;    // Envelope loop start point
    uint8_t loop_end;            // Envelope loop end point
    uint8_t flags;            // Envelope flags
    uint8_t unused[3];
};

struct IMFENVNODES
{
    uint16_t tick;
    uint16_t value;
};

struct IMFINSTRUMENT
{
    char name[32];            // Inst. name (ASCIIZ-String, bad_max. 31 chars)
    uint8_t map[120];            // Multisample settings
    uint8_t unused[8];
    IMFENVNODES nodes[3][16];
    IMFENVELOPE env[3];
    uint16_t fadeout;            // Fadeout rate (0...0FFFH)
    uint16_t smpnum;            // Number of samples in instrument
    char ii10[4];            // 'II10'
};

struct IMFSAMPLE
{
    char filename[13];    // Sample legacy_filename (12345678.ABC) */
    uint8_t unused1[3];
    uint32_t length;            // Length
    uint32_t loop_start;    // Loop start
    uint32_t loop_end;    // Loop end
    uint32_t C5Speed;            // Samplerate
    uint8_t volume;            // Default volume (0...64)
    uint8_t panning;            // Default pan (0...255)
    uint8_t unused2[14];
    uint8_t flags;            // Sample flags
    uint8_t unused3[5];
    uint16_t ems;                    // Reserved for internal usage
    uint32_t dram;            // Reserved for internal usage
    char is10[4];            // 'IS10'
};
#pragma pack()

static uint8_t imf_efftrans[] =
{
    CmdNone,
    CmdSpeed,                    // 0x01 1xx Set Tempo
    CmdTempo,                    // 0x02 2xx Set BPM
    CmdPorta, // 0x03 3xx Tone Portamento
    CmdPortaVolSlide,    // 0x04 4xy Tone Portamento + Volume Slide
    CmdVibrato,            // 0x05 5xy Vibrato
    CmdVibratoVolSlide,            // 0x06 6xy Vibrato + Volume Slide
    CmdFineVibrato,    // 0x07 7xy Fine Vibrato
    CmdTremolo,            // 0x08 8xy Tremolo
    CmdArpeggio,            // 0x09 9xy Arpeggio
    CmdPanning8,            // 0x0A Axx Set Pan Position
    CmdPanningSlide,    // 0x0B Bxy Pan Slide
    CmdVol,                    // 0x0C Cxx Set Volume
    CmdVolSlide,    // 0x0D Dxy Volume Slide
    CmdVolSlide,    // 0x0E Exy Fine Volume Slide
    CmdS3mCmdEx,            // 0x0F Fxx Set Finetune
    CmdNoteSlideUp,    // 0x10 Gxy Note Slide Up
    CmdNoteSlideDown,    // 0x11 Hxy Note Slide Down
    CmdPortaUp,    // 0x12 Ixx Slide Up
    CmdPortaDown,    // 0x13 Jxx Slide Down
    CmdPortaUp,    // 0x14 Kxx Fine Slide Up
    CmdPortaDown,    // 0x15 Lxx Fine Slide Down
    CmdMidi,                    // 0x16 Mxx Set Filter Cutoff - XXX
    CmdNone,                    // 0x17 Nxy Filter Slide + Resonance - XXX
    CmdOffset,                    // 0x18 Oxx Set Sample Offset
    CmdNone,                    // 0x19 Pxx Set Fine Sample Offset - XXX
    CmdKeyOff,                    // 0x1A Qxx Key Off
    CmdRetrig,                    // 0x1B Rxy Retrig
    CmdTremor,                    // 0x1C Sxy Tremor
    CmdPositionJump,    // 0x1D Txx Position Jump
    CmdPatternBreak,    // 0x1E Uxx Pattern Break
    CmdGlobalVol,    // 0x1F Vxx Set Mastervolume
    CmdGlobalVolSlide,    // 0x20 Wxy Mastervolume Slide
    CmdS3mCmdEx,            // 0x21 Xxx Extended Effect
                            // X1x Set Filter
                            // X3x Glissando
                            // X5x Vibrato Waveform
                            // X8x Tremolo Waveform
                            // XAx Pattern Loop
                            // XBx Pattern Delay
                            // XCx Note Cut
                            // XDx Note Delay
                            // XEx Ignore Envelope
                            // XFx Invert Loop
    CmdNone,                    // 0x22 Yxx Chorus - XXX
    CmdNone,                    // 0x23 Zxx Reverb - XXX
};

static void import_imf_effect(modplug::tracker::modevent_t *note)
//---------------------------------------------
{
    uint8_t n;
    // fix some of them
    switch (note->command)
    {
    case 0xe: // fine volslide
        // hackaround to get almost-right behavior for fine slides (i think!)
        if (note->param == 0)
            /* nothing */;
        else if (note->param == 0xf0)
            note->param = 0xef;
        else if (note->param == 0x0f)
            note->param = 0xfe;
        else if (note->param & 0xf0)
            note->param |= 0xf;
        else
            note->param |= 0xf0;
        break;
    case 0xf: // set finetune
        // we don't implement this, but let's at least import the value
        note->param = 0x20 | bad_min(note->param >> 4, 0xf);
        break;
    case 0x14: // fine slide up
    case 0x15: // fine slide down
        // this is about as close as we can do...
        if (note->param >> 4)
            note->param = 0xf0 | bad_min(note->param >> 4, 0xf);
        else
            note->param |= 0xe0;
        break;
    case 0x16: // cutoff
        note->param >>= 1;
        break;
    case 0x1f: // set global volume
        note->param = bad_min(note->param << 1, 0xff);
        break;
    case 0x21:
        n = 0;
        switch (note->param >> 4)
        {
        case 0:
            /* undefined, but since S0x does nothing in IT anyway, we won't care.
            this is here to allow S00 to pick up the previous value (assuming IMF
            even does that -- I haven't actually tried it) */
            break;
        default: // undefined
        case 0x1: // set filter
        case 0xf: // invert loop
            note->command = CmdNone;
            break;
        case 0x3: // glissando
            n = 0x20;
            break;
        case 0x5: // vibrato waveform
            n = 0x30;
            break;
        case 0x8: // tremolo waveform
            n = 0x40;
            break;
        case 0xa: // pattern loop
            n = 0xb0;
            break;
        case 0xb: // pattern delay
            n = 0xe0;
            break;
        case 0xc: // note cut
        case 0xd: // note delay
            // no change
            break;
        case 0xe: // ignore envelope
            /* predicament: we can only disable one envelope at a time.
            volume is probably most noticeable, so let's go with that.
            (... actually, orpheus doesn't even seem to implement this at all) */
            note->param = 0x77;
            break;
        case 0x18: // sample offset
            // O00 doesn't pick up the previous value
            if (!note->param)
                note->command = CmdNone;
            break;
        }
        if (n)
            note->param = n | (note->param & 0xf);
        break;
    }
    //XXXih: gross!
    note->command = (note->command < 0x24) ? (modplug::tracker::cmd_t) imf_efftrans[note->command] : CmdNone;
    if (note->command == CmdVol && note->volcmd == VolCmdNone)
    {
        note->volcmd = VolCmdVol;
        note->vol = note->param;
        note->command = CmdNone;
        note->param = 0;
    }
}

static void load_imf_envelope(modplug::tracker::modenvelope_t *env, const IMFINSTRUMENT *imfins, const int e)
//----------------------------------------------------------------------------------------------
{
    UINT bad_min = 0; // minimum tick value for next node
    const int shift = (e == IMF_ENV_VOL) ? 0 : 2;

    env->flags = ((imfins->env[e].flags & 1) ? ENV_ENABLED : 0) | ((imfins->env[e].flags & 2) ? ENV_SUSTAIN : 0) | ((imfins->env[e].flags & 4) ? ENV_LOOP : 0);
    env->num_nodes = CLAMP(imfins->env[e].points, 2, 25);
    env->loop_start = imfins->env[e].loop_start;
    env->loop_end = imfins->env[e].loop_end;
    env->sustain_start = env->sustain_end = imfins->env[e].sustain;
    env->release_node = ENV_RELEASE_NODE_UNSET;

    for(UINT n = 0; n < env->num_nodes; n++)
    {
        uint16_t nTick, nValue;
        nTick = LittleEndianW(imfins->nodes[e][n].tick);
        nValue = LittleEndianW(imfins->nodes[e][n].value) >> shift;
        env->Ticks[n] = (uint16_t)bad_max(bad_min, nTick);
        env->Values[n] = (uint8_t)bad_min(nValue, ENVELOPE_MAX);
        bad_min = nTick + 1;
    }
}

bool module_renderer::ReadIMF(const uint8_t * const lpStream, const uint32_t dwMemLength)
//-----------------------------------------------------------------------
{
    uint32_t dwMemPos = 0;
    IMFHEADER hdr;
    modsample_t *pSample = Samples + 1;
    uint16_t firstsample = 1; // first pSample for the current instrument
    uint32_t ignore_channels = 0; // bit set for each channel that's completely disabled

    ASSERT_CAN_READ(sizeof(IMFHEADER));
    memset(&hdr, 0, sizeof(IMFHEADER));
    memcpy(&hdr, lpStream, sizeof(IMFHEADER));
    dwMemPos = sizeof(IMFHEADER);

    hdr.ordnum = LittleEndianW(hdr.ordnum);
    hdr.patnum = LittleEndianW(hdr.patnum);
    hdr.insnum = LittleEndianW(hdr.insnum);
    hdr.flags = LittleEndianW(hdr.flags);

    if (memcmp(hdr.im10, "IM10", 4) != 0)
        return false;

    m_nType = MOD_TYPE_IMF;
    SetModFlag(MSF_COMPATIBLE_PLAY, true);

    // song name
    memset(m_szNames, 0, sizeof(m_szNames));
    assign_without_padding(this->song_name, hdr.title, 31);

    if (hdr.flags & 1)
        m_dwSongFlags |= SONG_LINEARSLIDES;
    m_nDefaultSpeed = hdr.tempo;
    m_nDefaultTempo = hdr.bpm;
    m_nDefaultGlobalVolume = CLAMP(hdr.master, 0, 64) << 2;
    m_nSamplePreAmp = CLAMP(hdr.amp, 4, 127);

    m_nSamples = 0; // Will be incremented later
    m_nInstruments = 0;

    m_nChannels = 0;
    for(modplug::tracker::chnindex_t nChn = 0; nChn < 32; nChn++)
    {
        ChnSettings[nChn].nPan = hdr.channels[nChn].panning * 64 / 255;
        ChnSettings[nChn].nPan *= 4;

        memcpy(ChnSettings[nChn].szName, hdr.channels[nChn].name, 12);
        SpaceToNullStringFixed<12>(ChnSettings[nChn].szName);

        // TODO: reverb/chorus?
        switch(hdr.channels[nChn].status)
        {
        case 0: // enabled; don't worry about it
            m_nChannels = nChn + 1;
            break;
        case 1: // mute
            ChnSettings[nChn].dwFlags |= CHN_MUTE;
            m_nChannels = nChn + 1;
            break;
        case 2: // disabled
            ChnSettings[nChn].dwFlags |= CHN_MUTE;
            ignore_channels |= (1 << nChn);
            break;
        default: // uhhhh.... freak out
            //fprintf(stderr, "imf: channel %d has unknown status %d\n", n, hdr.channels[n].status);
            return false;
        }
    }
    if(!m_nChannels) return false;

    //From mikmod: work around an Orpheus bug
    if (hdr.channels[0].status == 0)
    {
        modplug::tracker::chnindex_t nChn;
        for(nChn = 1; nChn < 16; nChn++)
            if(hdr.channels[nChn].status != 1)
                break;
        if (nChn == 16)
            for(nChn = 1; nChn < 16; nChn++)
                ChnSettings[nChn].dwFlags &= ~CHN_MUTE;
    }

    Order.resize(hdr.ordnum);
    for(modplug::tracker::orderindex_t nOrd = 0; nOrd < hdr.ordnum; nOrd++)
        Order[nOrd] = ((hdr.orderlist[nOrd] == 0xff) ? Order.GetIgnoreIndex() : (modplug::tracker::patternindex_t)hdr.orderlist[nOrd]);

    // read patterns
    for(modplug::tracker::patternindex_t nPat = 0; nPat < hdr.patnum; nPat++)
    {
        uint16_t length, nrows;
        uint8_t mask, channel;
        int row;
        unsigned int lostfx = 0;
        modplug::tracker::modevent_t *row_data, *note, junk_note;

        ASSERT_CAN_READ(4);
        length = LittleEndianW(*((uint16_t *)(lpStream + dwMemPos)));
        nrows = LittleEndianW(*((uint16_t *)(lpStream + dwMemPos + 2)));
        dwMemPos += 4;

        if(Patterns.Insert(nPat, nrows))
            break;

        row_data = Patterns[nPat];

        row = 0;
        while(row < nrows)
        {
            ASSERT_CAN_READ(1);
            mask = *((uint8_t *)(lpStream + dwMemPos));
            dwMemPos += 1;
            if (mask == 0) {
                row++;
                row_data += m_nChannels;
                continue;
            }

            channel = mask & 0x1f;

            if(ignore_channels & (1 << channel))
            {
                /* should do this better, i.e. not go through the whole process of deciding
                what to do with the effects since they're just being thrown out */
                //printf("disabled channel %d contains data\n", channel + 1);
                note = &junk_note;
            } else
            {
                note = row_data + channel;
            }

            if(mask & 0x20)
            {
                // read note/instrument
                ASSERT_CAN_READ(2);
                note->note = *((uint8_t *)(lpStream + dwMemPos));
                note->instr = *((uint8_t *)(lpStream + dwMemPos + 1));
                dwMemPos += 2;

                if (note->note == 160)
                {
                    note->note = NoteKeyOff; /* ??? */
                } else if (note->note == 255)
                {
                    note->note = NoteNone; /* ??? */
                } else
                {
                    note->note = (note->note >> 4) * 12 + (note->note & 0xf) + 12 + 1;
                    if(note->note > NoteMax)
                    {
                        /*printf("%d.%d.%d: funny note 0x%02x\n",
                            nPat, row, channel, fp->data[fp->pos - 1]);*/
                        note->note = NoteNone;
                    }
                }
            }
            if((mask & 0xc0) == 0xc0)
            {
                uint8_t e1c, e1d, e2c, e2d;

                // read both effects and figure out what to do with them
                ASSERT_CAN_READ(4);
                e1c = *((uint8_t *)(lpStream + dwMemPos));
                e1d = *((uint8_t *)(lpStream + dwMemPos + 1));
                e2c = *((uint8_t *)(lpStream + dwMemPos + 2));
                e2d = *((uint8_t *)(lpStream + dwMemPos + 3));
                dwMemPos += 4;

                if (e1c == 0xc)
                {
                    note->vol = bad_min(e1d, 0x40);
                    note->volcmd = VolCmdVol;
                    //XXXih: gross
                    note->command = (modplug::tracker::cmd_t) e2c;
                    note->param = e2d;
                } else if (e2c == 0xc)
                {
                    note->vol = bad_min(e2d, 0x40);
                    note->volcmd = VolCmdVol;
                    //XXXih: gross
                    note->command = (modplug::tracker::cmd_t) e1c;
                    note->param = e1d;
                } else if (e1c == 0xa)
                {
                    note->vol = e1d * 64 / 255;
                    note->volcmd = VolCmdPan;
                    //XXXih: gross
                    note->command = (modplug::tracker::cmd_t) e2c;
                    note->param = e2d;
                } else if (e2c == 0xa)
                {
                    note->vol = e2d * 64 / 255;
                    note->volcmd = VolCmdPan;
                    //XXXih: gross
                    note->command = (modplug::tracker::cmd_t) e1c;
                    note->param = e1d;
                } else
                {
                    /* check if one of the effects is a 'global' effect
                    -- if so, put it in some unused channel instead.
                    otherwise pick the most important effect. */
                    lostfx++;
                    //XXXih: gross
                    note->command = (modplug::tracker::cmd_t) e2c;
                    note->param = e2d;
                }
            } else if(mask & 0xc0)
            {
                // there's one effect, just stick it in the effect column
                ASSERT_CAN_READ(2);
                //XXXih: gross
                note->command = (modplug::tracker::cmd_t) *((uint8_t *)(lpStream + dwMemPos));
                note->param = *((uint8_t *)(lpStream + dwMemPos + 1));
                dwMemPos += 2;
            }
            if(note->command)
                import_imf_effect(note);
        }
    }

    // read instruments
    for (modplug::tracker::instrumentindex_t nIns = 0; nIns < hdr.insnum; nIns++)
    {
        IMFINSTRUMENT imfins;
        modinstrument_t *pIns;
        ASSERT_CAN_READ(sizeof(IMFINSTRUMENT));
        memset(&imfins, 0, sizeof(IMFINSTRUMENT));
        memcpy(&imfins, lpStream + dwMemPos, sizeof(IMFINSTRUMENT));
        dwMemPos += sizeof(IMFINSTRUMENT);
        m_nInstruments++;

        imfins.smpnum = LittleEndianW(imfins.smpnum);
        imfins.fadeout = LittleEndianW(imfins.fadeout);

        // Orpheus does not check this!
        //if(memcmp(imfins.ii10, "II10", 4) != 0)
        //    return false;

        pIns = new modinstrument_t;
        if(!pIns)
            continue;
        Instruments[nIns + 1] = pIns;
        memset(pIns, 0, sizeof(modinstrument_t));
        pIns->pitch_pan_center = 5 * 12;
        SetDefaultInstrumentValues(pIns);

        memcpy(pIns->name, imfins.name, 31);
        SpaceToNullStringFixed<31>(pIns->name);

        if(imfins.smpnum)
        {
            for(uint8_t cNote = 0; cNote < 120; cNote++)
            {
                pIns->NoteMap[cNote] = cNote + 1;
                pIns->Keyboard[cNote] = firstsample + imfins.map[cNote];
            }
        }

        pIns->fadeout = imfins.fadeout;
        pIns->global_volume = 64;

        load_imf_envelope(&pIns->volume_envelope, &imfins, IMF_ENV_VOL);
        load_imf_envelope(&pIns->panning_envelope, &imfins, IMF_ENV_PAN);
        load_imf_envelope(&pIns->pitch_envelope, &imfins, IMF_ENV_FILTER);
        if((pIns->pitch_envelope.flags & ENV_ENABLED) != 0)
            pIns->pitch_envelope.flags |= ENV_FILTER;

        // hack to get === to stop notes (from modplug's xm loader)
        if(!(pIns->volume_envelope.flags & ENV_ENABLED) && !pIns->fadeout)
            pIns->fadeout = 8192;

        // read this instrument's samples
        for(modplug::tracker::sampleindex_t nSmp = 0; nSmp < imfins.smpnum; nSmp++)
        {
            IMFSAMPLE imfsmp;
            uint32_t blen;
            ASSERT_CAN_READ(sizeof(IMFSAMPLE));
            memset(&imfsmp, 0, sizeof(IMFSAMPLE));
            memcpy(&imfsmp, lpStream + dwMemPos, sizeof(IMFSAMPLE));
            dwMemPos += sizeof(IMFSAMPLE);
            m_nSamples++;

            if(memcmp(imfsmp.is10, "IS10", 4) != 0)
                return false;

            memcpy(pSample->legacy_filename, imfsmp.filename, 12);
            SpaceToNullStringFixed<12>(pSample->legacy_filename);
            strcpy(m_szNames[m_nSamples], pSample->legacy_filename);

            blen = pSample->length = LittleEndian(imfsmp.length);
            pSample->loop_start = LittleEndian(imfsmp.loop_start);
            pSample->loop_end = LittleEndian(imfsmp.loop_end);
            pSample->c5_samplerate = LittleEndian(imfsmp.C5Speed);
            pSample->default_volume = imfsmp.volume * 4;
            pSample->global_volume = 256;
            pSample->default_pan = imfsmp.panning;
            if (imfsmp.flags & 1)
                pSample->flags |= CHN_LOOP;
            if (imfsmp.flags & 2)
                pSample->flags |= CHN_PINGPONGLOOP;
            if (imfsmp.flags & 4)
            {
                pSample->flags |= CHN_16BIT;
                pSample->length >>= 1;
                pSample->loop_start >>= 1;
                pSample->loop_end >>= 1;
            }
            if (imfsmp.flags & 8)
                pSample->flags |= CHN_PANNING;

            if(blen)
            {
                ASSERT_CAN_READ(blen);
                ReadSample(pSample, (imfsmp.flags & 4) ? RS_PCM16S : RS_PCM8S, reinterpret_cast<LPCSTR>(lpStream + dwMemPos), blen);
            }

            dwMemPos += blen;
            pSample++;
        }
        firstsample += imfins.smpnum;
    }

    return true;
}