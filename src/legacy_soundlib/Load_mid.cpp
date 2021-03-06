/*
 * OpenMPT
 *
 * Load_mid.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

//////////////////////////////////////////////
// MIDI loader                              //
//////////////////////////////////////////////
#include "stdafx.h"
#include "Loaders.h"
#include "dlsbank.h"
#include "Wav.h"


#pragma warning(disable:4244)
//#define MIDI_LOG

#ifdef MIDI_LOG
//#define MIDI_DETAILED_LOG
extern void Log(LPCSTR, ...);
#endif

#define MIDI_DRUMCHANNEL    10
#define MIDI_MAXTRACKS            64

UINT gnMidiImportSpeed = 3;
UINT gnMidiPatternLen = 128;

#pragma pack(1)

typedef struct MIDIFILEHEADER
{
    uint32_t id;            // "MThd" = 0x6468544D
    uint32_t len;            // 6
    uint16_t w1;            // 1?
    uint16_t wTrks;            // 2?
    uint16_t wDivision;    // F0
} MIDIFILEHEADER;


typedef struct MIDITRACKHEADER
{
    uint32_t id;    // "MTrk" = 0x6B72544D
    uint32_t len;
} MIDITRACKHEADER;

//////////////////////////////////////////////////////////////////////
// Midi Loader Internal Structures

#define CHNSTATE_NOTEOFFPENDING            0x0001

// MOD Channel State description (current volume, panning, etc...)
typedef struct MODCHANNELSTATE
{
    uint32_t flags;    // Channel Flags
    uint16_t idlecount;
    uint16_t pitchsrc, pitchdest;    // Pitch Bend (current position/new position)
    uint8_t parent;    // Midi Channel parent
    uint8_t pan;            // Channel Panning                        0-255
    uint8_t note;            // Note On # (0=available)
} MODCHANNELSTATE;

// MIDI Channel State (Midi Channels 0-15)
typedef struct MIDICHANNELSTATE
{
    uint32_t flags;            // Channel Flags
    uint16_t pitchbend;            // Pitch Bend Amount (14-bits unsigned)
    uint8_t note_on[128];    // If note=on -> MOD channel # + 1 (0 if note=off)
    uint8_t program;            // Channel Midi Program
    uint16_t bank;                    // 0-16383
    // -- Controllers --------- function ---------- CC# --- range  --- init (midi) ---
    uint8_t pan;                    // Channel Panning                        CC10        [0-255]                128 (64)
    uint8_t expression;    // Channel Expression                CC11        0-128                128        (127)
    uint8_t volume;            // Channel Volume                        CC7                0-128                80        (100)
    uint8_t modulation;    // Modulation                                CC1                0-127                0
    uint8_t pitchbendrange;// Pitch Bend Range                                                            64
} MIDICHANNELSTATE;

typedef struct MIDITRACK
{
    const uint8_t *ptracks;
    const uint8_t *ptrmax;
    uint32_t status;
    LONG nexteventtime;
} MIDITRACK;

#pragma pack()



extern const LPCSTR szMidiGroupNames[17] =
{
    "Piano",
    "Chromatic Percussion",
    "Organ",
    "Guitar",
    "Bass",
    "Strings",
    "Ensemble",
    "Brass",
    "Reed",
    "Pipe",
    "Synth Lead",
    "Synth Pad",
    "Synth Effects",
    "Ethnic",
    "Percussive",
    "Sound Effects",
    "Percussions"
};


extern const LPCSTR szMidiProgramNames[128] =
{
    // 1-8: Piano
    "Acoustic Grand Piano",
    "Bright Acoustic Piano",
    "Electric Grand Piano",
    "Honky-tonk Piano",
    "Electric Piano 1",
    "Electric Piano 2",
    "Harpsichord",
    "Clavi",
    // 9-16: Chromatic Percussion
    "Celesta",
    "Glockenspiel",
    "Music Box",
    "Vibraphone",
    "Marimba",
    "Xylophone",
    "Tubular Bells",
    "Dulcimer",
    // 17-24: Organ
    "Drawbar Organ",
    "Percussive Organ",
    "Rock Organ",
    "Church Organ",
    "Reed Organ",
    "Accordion",
    "Harmonica",
    "Tango Accordion",
    // 25-32: Guitar
    "Acoustic Guitar (nylon)",
    "Acoustic Guitar (steel)",
    "Electric Guitar (jazz)",
    "Electric Guitar (clean)",
    "Electric Guitar (muted)",
    "Overdriven Guitar",
    "Distortion Guitar",
    "Guitar harmonics",
    // 33-40   Bass
    "Acoustic Bass",
    "Electric Bass (finger)",
    "Electric Bass (pick)",
    "Fretless Bass",
    "Slap Bass 1",
    "Slap Bass 2",
    "Synth Bass 1",
    "Synth Bass 2",
    // 41-48   Strings
    "Violin",
    "Viola",
    "Cello",
    "Contrabass",
    "Tremolo Strings",
    "Pizzicato Strings",
    "Orchestral Harp",
    "Timpani",
    // 49-56   Ensemble
    "String Ensemble 1",
    "String Ensemble 2",
    "SynthStrings 1",
    "SynthStrings 2",
    "Choir Aahs",
    "Voice Oohs",
    "Synth Voice",
    "Orchestra Hit",
    // 57-64   Brass
    "Trumpet",
    "Trombone",
    "Tuba",
    "Muted Trumpet",
    "French Horn",
    "Brass Section",
    "SynthBrass 1",
    "SynthBrass 2",
    // 65-72   Reed
    "Soprano Sax",
    "Alto Sax",
    "Tenor Sax",
    "Baritone Sax",
    "Oboe",
    "English Horn",
    "Bassoon",
    "Clarinet",
    // 73-80   Pipe
    "Piccolo",
    "Flute",
    "Recorder",
    "Pan Flute",
    "Blown Bottle",
    "Shakuhachi",
    "Whistle",
    "Ocarina",
    // 81-88   Synth Lead
    "Lead 1 (square)",
    "Lead 2 (sawtooth)",
    "Lead 3 (calliope)",
    "Lead 4 (chiff)",
    "Lead 5 (charang)",
    "Lead 6 (voice)",
    "Lead 7 (fifths)",
    "Lead 8 (bass + lead)",
    // 89-96   Synth Pad
    "Pad 1 (new age)",
    "Pad 2 (warm)",
    "Pad 3 (polysynth)",
    "Pad 4 (choir)",
    "Pad 5 (bowed)",
    "Pad 6 (metallic)",
    "Pad 7 (halo)",
    "Pad 8 (sweep)",
    // 97-104  Synth Effects
    "FX 1 (rain)",
    "FX 2 (soundtrack)",
    "FX 3 (crystal)",
    "FX 4 (atmosphere)",
    "FX 5 (brightness)",
    "FX 6 (goblins)",
    "FX 7 (echoes)",
    "FX 8 (sci-fi)",
    // 105-112 Ethnic
    "Sitar",
    "Banjo",
    "Shamisen",
    "Koto",
    "Kalimba",
    "Bag pipe",
    "Fiddle",
    "Shanai",
    // 113-120 Percussive
    "Tinkle Bell",
    "Agogo",
    "Steel Drums",
    "Woodblock",
    "Taiko Drum",
    "Melodic Tom",
    "Synth Drum",
    "Reverse Cymbal",
    // 121-128 Sound Effects
    "Guitar Fret Noise",
    "Breath Noise",
    "Seashore",
    "Bird Tweet",
    "Telephone Ring",
    "Helicopter",
    "Applause",
    "Gunshot"
};


// Notes 25-85
extern const LPCSTR szMidiPercussionNames[61] =
{
    "Seq Click",
    "Brush Tap",
    "Brush Swirl",
    "Brush Slap",
    "Brush Swirl W/Attack",
    "Snare Roll",
    "Castanet",
    "Snare Lo",
    "Sticks",
    "Bass Drum Lo",
    "Open Rim Shot",
    "Acoustic Bass Drum",
    "Bass Drum 1",
    "Side Stick",
    "Acoustic Snare",
    "Hand Clap",
    "Electric Snare",
    "Low Floor Tom",
    "Closed Hi Hat",
    "High Floor Tom",
    "Pedal Hi-Hat",
    "Low Tom",
    "Open Hi-Hat",
    "Low-Mid Tom",
    "Hi Mid Tom",
    "Crash Cymbal 1",
    "High Tom",
    "Ride Cymbal 1",
    "Chinese Cymbal",
    "Ride Bell",
    "Tambourine",
    "Splash Cymbal",
    "Cowbell",
    "Crash Cymbal 2",
    "Vibraslap",
    "Ride Cymbal 2",
    "Hi Bongo",
    "Low Bongo",
    "Mute Hi Conga",
    "Open Hi Conga",
    "Low Conga",
    "High Timbale",
    "Low Timbale",
    "High Agogo",
    "Low Agogo",
    "Cabasa",
    "Maracas",
    "Short Whistle",
    "Long Whistle",
    "Short Guiro",
    "Long Guiro",
    "Claves",
    "Hi Wood Block",
    "Low Wood Block",
    "Mute Cuica",
    "Open Cuica",
    "Mute Triangle",
    "Open Triangle",
    "Shaker",
    "Jingle Bell",
    "Bell Tree",
};


const uint16_t kMidiChannelPriority[16] =
{
    0xFFFE, 0xFFFC, 0xFFF8, 0xFFF0,    0xFFE0, 0xFFC0, 0xFF80, 0xFF00,
    0xFE00, 0xFDFF, 0xF800, 0xF000,    0xE000, 0xC000, 0x8000, 0x0000,
};


///////////////////////////////////////////////////////////////////////////
// Helper functions

static LONG __fastcall getmidilong(const uint8_t * &p, const uint8_t * pmax)
//----------------------------------------------------------
{
    uint32_t n;
    UINT a;

    a = (p < pmax) ? *(p++) : 0;
    n = 0;
    while (a&0x80)
    {
        n = (n<<7)|(a&0x7F);
        a = (p < pmax) ? *(p++) : 0;
    }
    return (n<<7)|(LONG)a;
}


// Returns MOD tempo and tick multiplier
static int ConvertMidiTempo(int tempo_us, int *pTickMultiplier)
//-------------------------------------------------------------
{
    int nBestModTempo = 120;
    int nBestError = 1000000; // 1s
    int nBestMultiplier = 1;
    int nSpeed = gnMidiImportSpeed;
    for (int nModTempo=110; nModTempo<=240; nModTempo++)
    {
        int tick_us = (2500000) / nModTempo;
        int nFactor = (tick_us+tempo_us/2) / tempo_us;
        if (!nFactor) nFactor = 1;
        int nError = tick_us - tempo_us * nFactor;
        if (nError < 0) nError = -nError;
        if (nError < nBestError)
        {
            nBestError = nError;
            nBestModTempo = nModTempo;
            nBestMultiplier = nFactor;
        }
        if ((!nError) || ((nError<=1) && (nFactor==64))) break;
    }
    *pTickMultiplier = nBestMultiplier * nSpeed;
    return nBestModTempo;
}


////////////////////////////////////////////////////////////////////////////////
// Maps a midi instrument - returns the instrument number in the file
UINT module_renderer::MapMidiInstrument(uint32_t dwBankProgram, UINT nChannel, UINT nNote)
//--------------------------------------------------------------------------------
{
    modinstrument_t *pIns;
    UINT nProgram = dwBankProgram & 0x7F;
    UINT nBank = dwBankProgram >> 7;

    nNote &= 0x7F;
    if (nNote >= NoteMax) return 0;
    for (UINT i=1; i<=m_nInstruments; i++) if (Instruments[i])
    {
        modinstrument_t *p = Instruments[i];
        // Drum Kit ?
        if (nChannel == MIDI_DRUMCHANNEL)
        {
            if (nNote == p->midi_drum_set) return i;
        } else
        // Melodic Instrument
        {
            if (nProgram == p->midi_program) return i;
        }
    }
    if ((m_nInstruments + 1 >= MAX_INSTRUMENTS) || (m_nSamples + 1 >= MAX_SAMPLES)) return 0;
    pIns = new modinstrument_t;
    if (!pIns) return 0;
    memset(pIns, 0, sizeof(modinstrument_t));
    pIns->pTuning = pIns->s_DefaultTuning;
    m_nSamples++;
    m_nInstruments++;
    Instruments[m_nInstruments] = pIns;
    pIns->midi_bank = nBank;
    pIns->midi_program = nProgram;
    pIns->midi_channel = nChannel;
    if (nChannel == MIDI_DRUMCHANNEL) pIns->midi_drum_set = nNote;
    pIns->global_volume = 64;
    pIns->fadeout = 1024;
    pIns->default_pan = 128;
    pIns->pitch_pan_center = 5*12;
    pIns->new_note_action = NNA_NOTEOFF;
    pIns->duplicate_check_type = (nChannel == MIDI_DRUMCHANNEL) ? DCT_SAMPLE : DCT_NOTE;
    pIns->duplicate_note_action = DNA_NOTEFADE;
    SetDefaultInstrumentValues(pIns);
    for (UINT j=0; j<NoteMax; j++)
    {
        int mapnote = j+1;
        if (nChannel == MIDI_DRUMCHANNEL)
        {
            mapnote = NoteMiddleC;
            /*mapnote = 61 + j - nNote;
            if (mapnote < 1) mapnote = 1;
            if (mapnote > 120) mapnote = 120;*/
        }
        pIns->Keyboard[j] = m_nSamples;
        pIns->NoteMap[j] = (uint8_t)mapnote;
    }
    pIns->volume_envelope.flags |= ENV_ENABLED;
    if (nChannel != MIDI_DRUMCHANNEL) pIns->volume_envelope.flags |= ENV_SUSTAIN;
    pIns->volume_envelope.num_nodes = 4;
    pIns->volume_envelope.Ticks[0] = 0;
    pIns->volume_envelope.Values[0] = ENVELOPE_MAX;
    pIns->volume_envelope.Ticks[1] = 10;
    pIns->volume_envelope.Values[1] = ENVELOPE_MAX;
    pIns->volume_envelope.Ticks[2] = 15;
    pIns->volume_envelope.Values[2] = (ENVELOPE_MAX + ENVELOPE_MID) / 2;
    pIns->volume_envelope.Ticks[3] = 20;
    pIns->volume_envelope.Values[3] = ENVELOPE_MIN;
    pIns->volume_envelope.sustain_start = pIns->volume_envelope.sustain_end = 1;
    // Sample
    Samples[m_nSamples].default_pan = 128;
    Samples[m_nSamples].default_volume = 256;
    Samples[m_nSamples].global_volume = 64;
    if (nChannel != MIDI_DRUMCHANNEL)
    {
        // GM Midi Name
        strcpy(pIns->name, szMidiProgramNames[nProgram]);
        strcpy(m_szNames[m_nSamples], szMidiProgramNames[nProgram]);
    } else
    {
        strcpy(pIns->name, "Percussions");
        if ((nNote >= 24) && (nNote <= 84))
            strcpy(m_szNames[m_nSamples], szMidiPercussionNames[nNote-24]);
        else
            strcpy(m_szNames[m_nSamples], "Percussions");
    }
    return m_nInstruments;
}


/////////////////////////////////////////////////////////////////
// Loader Status
#define MIDIGLOBAL_SONGENDED            0x0001
#define MIDIGLOBAL_FROZEN                    0x0002
#define MIDIGLOBAL_UPDATETEMPO            0x0004
#define MIDIGLOBAL_UPDATEMASTERVOL    0x0008
// Midi Globals
#define MIDIGLOBAL_GMSYSTEMON            0x0100
#define MIDIGLOBAL_XGSYSTEMON            0x0200


bool module_renderer::ReadMID(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
    const MIDIFILEHEADER *pmfh = (const MIDIFILEHEADER *)lpStream;
    const MIDITRACKHEADER *pmth;
    MODCHANNELSTATE chnstate[MAX_BASECHANNELS];
    MIDICHANNELSTATE midichstate[16];
    MIDITRACK miditracks[MIDI_MAXTRACKS];
    uint32_t dwMemPos, dwGlobalFlags, tracks, tempo;
    UINT row, pat, midimastervol;
    short int division;
    int midi_clock, nTempoUsec, nPPQN, nTickMultiplier;

    // Fix import parameters
    if (gnMidiImportSpeed < 2) gnMidiImportSpeed = 2;
    if (gnMidiImportSpeed > 6) gnMidiImportSpeed = 6;
    if (gnMidiPatternLen < 64) gnMidiPatternLen = 64;
    if (gnMidiPatternLen > 256) gnMidiPatternLen = 256;
    // Detect RMI files
    if ((dwMemLength > 12)
     && (*(uint32_t *)(lpStream) == IFFID_RIFF)
     && (*(uint32_t *)(lpStream+8) == 0x44494D52))
    {
        lpStream += 12;
        dwMemLength -= 12;
        while (dwMemLength > 8)
        {
            uint32_t id = *(uint32_t *)lpStream;
            uint32_t len = *(uint32_t *)(lpStream+4);
            lpStream += 8;
            dwMemLength -= 8;
            if ((id == IFFID_data) && (len < dwMemLength))
            {
                dwMemLength = len;
                pmfh = (const MIDIFILEHEADER *)lpStream;
                break;
            }
            if (len >= dwMemLength) return false;
            lpStream += len;
            dwMemLength -= len;
        }
    }
    // MIDI File Header
    if ((dwMemLength < sizeof(MIDIFILEHEADER)+8) || (pmfh->id != 0x6468544D)) return false;
    dwMemPos = 8 + BigEndian(pmfh->len);
    if (dwMemPos >= dwMemLength - 8) return false;
    pmth = (MIDITRACKHEADER *)(lpStream+dwMemPos);
    tracks = BigEndianW(pmfh->wTrks);
    if ((pmth->id != 0x6B72544D) || (!tracks)) return false;
    if (tracks > MIDI_MAXTRACKS) tracks = MIDI_MAXTRACKS;
    // Reading File...
    m_nType = MOD_TYPE_MID;
    m_nChannels = 32;
    m_nSamples = 0;
    m_nInstruments = 0;
    m_dwSongFlags |= SONG_LINEARSLIDES;
    // MIDI->MOD Tempo Conversion
    division = BigEndianW(pmfh->wDivision);
    if (division < 0)
    {
        int nFrames = -(division>>8);
        int nSubFrames = (division & 0xff);
        nPPQN = nFrames * nSubFrames / 2;
        if (!nPPQN) nPPQN = 1;
    } else
    {
        nPPQN = (division) ? division : 96;
    }
    nTempoUsec = 500000 / nPPQN;
    tempo = ConvertMidiTempo(nTempoUsec, &nTickMultiplier);
    m_nDefaultTempo = tempo;
    m_nDefaultSpeed = gnMidiImportSpeed;
    m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
    midimastervol = m_nDefaultGlobalVolume;

#ifdef MIDI_LOG
    Log("%d tracks, tempo = %dus, division = %04X TickFactor=%d\n", tracks, nTempoUsec, ((UINT)division) & 0xFFFF, nTickMultiplier);
#endif
    // Initializing
    Order.resize(MAX_ORDERS, Order.GetInvalidPatIndex());
    memset(chnstate, 0, sizeof(chnstate));
    memset(miditracks, 0, sizeof(miditracks));
    memset(midichstate, 0, sizeof(midichstate));
    // Initializing Patterns
    Order[0] = 0;
    // Initializing Channels
    for (UINT ics=0; ics<MAX_BASECHANNELS; ics++)
    {
        // Channel settings
        ChnSettings[ics].nPan = 128;
        ChnSettings[ics].nVolume = 64;
        ChnSettings[ics].dwFlags = 0;
        // Channels state
        chnstate[ics].pan = 128;
        chnstate[ics].pitchsrc = 0x2000;
        chnstate[ics].pitchdest = 0x2000;
    }
    // Initializing Track Pointers
    for (UINT itrk=0; itrk<tracks; itrk++)
    {
        miditracks[itrk].nexteventtime = -1;
        miditracks[itrk].status = 0x2F;
        pmth = (MIDITRACKHEADER *)(lpStream+dwMemPos);
        if (dwMemPos + 8 >= dwMemLength) break;
        uint32_t len = BigEndian(pmth->len);
        if ((pmth->id == 0x6B72544D) && (len <= dwMemLength - (dwMemPos + 8)))
        {
#ifdef MIDI_DETAILED_LOG
            Log(" track%d at offset %d len=%d ", itrk, dwMemPos+8, len);
#endif
            // Initializing midi tracks
            miditracks[itrk].ptracks = lpStream+dwMemPos+8;
            miditracks[itrk].ptrmax = miditracks[itrk].ptracks + len;
            miditracks[itrk].nexteventtime = getmidilong(miditracks[itrk].ptracks, miditracks[itrk].ptrmax);
#ifdef MIDI_DETAILED_LOG
            Log(" init time=%d\n", miditracks[itrk].nexteventtime);
#endif
        }
        dwMemPos += 8 + len;
    }
#ifdef MIDI_LOG
    Log("\n");
#endif
    // Initializing midi channels state
    for (UINT imidi=0; imidi<16; imidi++)
    {
        midichstate[imidi].pan = 128;                    // middle
        midichstate[imidi].expression = 128;    // no attenuation
        midichstate[imidi].volume = 80;                    // GM specs defaults to 100
        midichstate[imidi].pitchbend = 0x2000;    // Pitch Bend Amount
        midichstate[imidi].pitchbendrange = 64;    // Pitch Bend Range: +/- 2 semitones
    }
    ////////////////////////////////////////////////////////////////////////////
    // Main Midi Sequencer Loop
    pat = 0;
    row = 0;
    midi_clock = 0;
    dwGlobalFlags = MIDIGLOBAL_UPDATETEMPO | MIDIGLOBAL_FROZEN;
    do
    {
        // Allocate current pattern if not allocated yet
        if (!Patterns[pat] && Patterns.Insert(pat, gnMidiPatternLen))
        {
            break;
        }
        dwGlobalFlags |= MIDIGLOBAL_SONGENDED;
        modplug::tracker::modevent_t *m = Patterns[pat] + row * m_nChannels;
        // Parse Tracks
        for (UINT trk=0; trk<tracks; trk++) if (miditracks[trk].ptracks)
        {
            MIDITRACK *ptrk = &miditracks[trk];
            dwGlobalFlags &= ~MIDIGLOBAL_SONGENDED;
            while ((ptrk->ptracks) && (ptrk->nexteventtime >= 0) && (midi_clock+(nTickMultiplier>>2) >= ptrk->nexteventtime))
            {
                if (ptrk->ptracks[0] & 0x80) ptrk->status = *(ptrk->ptracks++);
            #ifdef MIDI_DETAILED_LOG
                Log("status: %02X\n", ptrk->status);
            #endif
                switch(ptrk->status)
                {
                /////////////////////////////////////////////////////////////////////
                // End Of Track
                case 0x2F:
                // End Of Song
                case 0xFC:
#ifdef MIDI_LOG
                    Log("track %d: EOT code 0x%02X\n", trk, ptrk->status);
#endif
                    ptrk->ptracks = NULL;
                    break;

                /////////////////////////////////////////////////////////////////////
                // SYSEX messages
                case 0xF0:
                case 0xF7:
                    {
                        LONG len = getmidilong(ptrk->ptracks, ptrk->ptrmax);
                        if ((len > 1) && (ptrk->ptracks + len <ptrk->ptrmax) && (ptrk->ptracks[len-1] == 0xF7))
                        {
                            uint32_t dwSysEx1 = 0, dwSysEx2 = 0;
                            if (len >= 4) dwSysEx1 = (*((uint32_t *)(ptrk->ptracks))) & 0x7F7F7F7F;
                            if (len >= 8) dwSysEx2 = (*((uint32_t *)(ptrk->ptracks+4))) & 0x7F7F7F7F;
                            // GM System On
                            if ((len == 5) && (dwSysEx1 == 0x01097F7E))
                            {
                                dwGlobalFlags |= MIDIGLOBAL_GMSYSTEMON;
                            } else
                            // XG System On
                            if ((len == 8) && ((dwSysEx1 & 0xFFFFF0FF) == 0x004c1043) && (dwSysEx2 == 0x77007e00))
                            {
                                dwGlobalFlags |= MIDIGLOBAL_XGSYSTEMON;
                            } else
                            // Midi Master Volume
                            if ((len == 7) && (dwSysEx1 == 0x01047F7F))
                            {
                                midimastervol = CDLSBank::DLSMidiVolumeToLinear(ptrk->ptracks[5] & 0x7F) >> 8;
                                if (midimastervol < 16) midimastervol = 16;
                                dwGlobalFlags |= MIDIGLOBAL_UPDATEMASTERVOL;
                            }
#ifdef MIDI_LOG
                            else
                            {
                                Log("track %d: SYSEX len=%d: F0", trk, len);
                                for (UINT k=0; k<(UINT)len; k++)
                                {
                                    Log(".%02X", ptrk->ptracks[k]);
                                    if (k >= 40)
                                    {
                                        Log("..");
                                        break;
                                    }
                                }
                                Log("\n");
                            }
#endif
                        }
#ifdef MIDI_LOG
                        else Log("Invalid SYSEX received!\n");
#endif
                        ptrk->ptracks += len;
                    }
                    break;

                //////////////////////////////////////////////////////////////////////
                // META-events: FF.code.len.data[len]
                case 0xFF:
                    {
                        UINT i = *(ptrk->ptracks++);
                        LONG len = getmidilong(ptrk->ptracks, ptrk->ptrmax);
                        if (ptrk->ptracks+len > ptrk->ptrmax)
                        {
                            // EOF
                            ptrk->ptracks = NULL;
                        } else
                        switch(i)
                        {
                        // FF.01 [text]: Song Information
                        case 0x01:
                            if (!len) break;
                            if ((len < 32) && this->song_name.empty())
                            {
                                assign_without_padding(this->song_name, reinterpret_cast<const char *>(ptrk->ptracks), len);
                            } else
                            if ((!m_lpszSongComments) && (ptrk->ptracks[0]) && (ptrk->ptracks[0] < 0x7F))
                            {
                                ReadMessage(ptrk->ptracks, len, leAutodetect);
                            }
                            break;
                        // FF.02 [text]: Song Copyright
                        case 0x02:
                            if (!len) break;
                            if ((!m_lpszSongComments) && (ptrk->ptracks[0]) && (ptrk->ptracks[0] < 0x7F) && (len > 7))
                            {
                                ReadMessage(ptrk->ptracks, len, leAutodetect);
                            }
                            break;
                        // FF.03: Sequence Name
                        case 0x03:
                        // FF.06: Sequence Text (->Pattern names)
                        case 0x06:
                            if ((len > 1) && (!trk))
                            {
                                UINT k = (len < 32) ? len : 31;
                                CHAR s[32];
                                memcpy(s, ptrk->ptracks, k);
                                s[k] = 0;
                                if ((!_strnicmp(s, "Copyri", 6)) || (!s[0])) break;
                                if (i == 0x03)
                                {
                                    if (this->song_name.empty()) this->song_name.assign(s);
                                } else
                                if (!trk)
                                {
                                    Patterns[pat].SetName(s);
                                }
#ifdef MIDI_LOG
                                Log("Track #%d, META 0x%02X, Pattern %d: ", trk, i, pat);
                                Log("%s\n", (uint32_t)s);
#endif
                            }
                            break;
                        // FF.07: Cue Point (marker)
                        // FF.20: Channel Prefix
                        // FF.2F: End of Track
                        case 0x2F:
                            ptrk->status = 0x2F;
                            ptrk->ptracks = NULL;
                            break;
                        // FF.51 [tttttt]: Set Tempo
                        case 0x51:
                            {
                                LONG l = ptrk->ptracks[0];
                                l = (l << 8) | ptrk->ptracks[1];
                                l = (l << 8) | ptrk->ptracks[2];
                                if (l <= 0) break;
                                nTempoUsec = l / nPPQN;
                                if (nTempoUsec < 100) nTempoUsec = 100;
                                tempo = ConvertMidiTempo(nTempoUsec, &nTickMultiplier);
                                dwGlobalFlags |= MIDIGLOBAL_UPDATETEMPO;
#ifdef MIDI_LOG
                                Log("META Tempo: %d usec\n", nTempoUsec);
#endif
                            }
                            break;
                        // FF.58: Time Signature
                        // FF.7F: Sequencer-Specific
#ifdef MIDI_LOG
                        default:
                            if ((i != 0x58) && (i != 0x7F))
                                Log("track %d: META %02X len=%d\n", trk, i, len);
#endif
                        }
                        if (ptrk->ptracks) ptrk->ptracks += len;
                    }
                    break;

                //////////////////////////////////////////////////////////////////////////
                // Regular Voice Events
                default:
                {
                    UINT midich = (ptrk->status & 0x0F)+1;
                    UINT midist = ptrk->status & 0xF0;
                    MIDICHANNELSTATE *pmidich = &midichstate[midich-1];
                    UINT note, velocity;

                    switch(midist)
                    {
                    //////////////////////////////////
                    // Note Off:    80.note.velocity
                    case 0x80:
                    // Note On:            90.note.velocity
                    case 0x90:
                        note = ptrk->ptracks[0] & 0x7F;
                        velocity = (midist == 0x90) ? (ptrk->ptracks[1] & 0x7F) : 0;
                        ptrk->ptracks += 2;
                    #ifdef MIDI_DETAILED_LOG
                        Log("Ch %d: NoteOn(%d,%d)\n", midich, note, velocity);
                    #endif
                        // Note On: 90.note.velocity
                        if (velocity)
                        {
                            // Start counting rows
                            dwGlobalFlags &= ~MIDIGLOBAL_FROZEN;
                            // if the note is already playing, we reuse this channel
                            UINT nchn = pmidich->note_on[note];
                            if ((nchn) && (chnstate[nchn-1].parent != midich)) nchn = 0;
                            // or else, we look for an available child channel
                            if (!nchn)
                            {
                                for (UINT i=0; i<m_nChannels; i++) if (chnstate[i].parent == midich)
                                {
                                    if ((!chnstate[i].note) && ((!m[i].note) || (m[i].note & 0x80)))
                                    {
                                        // found an available channel
                                        nchn = i+1;
                                        break;
                                    }
                                }
                            }
                            // still nothing? in this case, we try to allocate a new mod channel
                            if (!nchn)
                            {
                                for (UINT i=0; i<m_nChannels; i++) if (!chnstate[i].parent)
                                {
                                    nchn = i+1;
                                    chnstate[i].parent = midich;
                                    break;
                                }
                            }
                            // still not? we have to steal a voice from another channel
#ifdef MIDI_LOG
                            if (!nchn)
                            {
                                Log("Not enough voices!\n");
                            }
#endif
                            // We found our channel: let's do the note on
                            if (nchn)
                            {
                                pmidich->note_on[note] = nchn;
                                nchn--;
                                chnstate[nchn].pitchsrc = pmidich->pitchbend;
                                chnstate[nchn].pitchdest = pmidich->pitchbend;
                                chnstate[nchn].flags &= ~CHNSTATE_NOTEOFFPENDING;
                                chnstate[nchn].idlecount = 0;
                                chnstate[nchn].note = note+1;
                                int realnote = note;
                                if (midich != 10)
                                {
                                    realnote += (((int)pmidich->pitchbend - 0x2000) * pmidich->pitchbendrange) / (0x2000*32);
                                    if (realnote < 0) realnote = 0;
                                    if (realnote > 119) realnote = 119;
                                }
                                m[nchn].note = realnote+1;
                                m[nchn].instr = MapMidiInstrument(pmidich->program + ((UINT)pmidich->bank << 7), midich, note);
                                m[nchn].volcmd = VolCmdVol;
                                LONG vol = CDLSBank::DLSMidiVolumeToLinear(velocity) >> 8;
                                vol = (vol * (LONG)pmidich->volume * (LONG)pmidich->expression) >> 13;
                                if (vol > 256) vol = 256;
                                if (vol < 4) vol = 4;
                                m[nchn].vol = (uint8_t)(vol>>2);
                                // Channel Panning
                                if ((!m[nchn].command) && (pmidich->pan != chnstate[nchn].pan))
                                {
                                    chnstate[nchn].pan = pmidich->pan;
                                    m[nchn].param = pmidich->pan;
                                    m[nchn].command = CmdPanning8;
                                }
                            }
                        } else
                        // Note Off; 90.note.00
                        if (!(dwGlobalFlags & MIDIGLOBAL_FROZEN))
                        {
                            UINT nchn = pmidich->note_on[note];
                            if (nchn)
                            {
                                nchn--;
                                chnstate[nchn].flags |= CHNSTATE_NOTEOFFPENDING;
                                chnstate[nchn].note = 0;
                                pmidich->note_on[note] = 0;
                            } else
                            {
                                for (UINT i=0; i<m_nChannels; i++)
                                {
                                    if ((chnstate[i].parent == midich) && (chnstate[i].note == note+1))
                                    {
                                        chnstate[i].note = 0;
                                        chnstate[i].flags |= CHNSTATE_NOTEOFFPENDING;
                                    }
                                }
                            }
                        }
                        break;

                    ///////////////////////////////////
                    // A0.xx.yy: Aftertouch
                    case 0xA0:
                        {
#ifdef MIDI_LOG
                            UINT a = ptrk->ptracks[0];
                            UINT b = ptrk->ptracks[1];
                            Log("track %d: %02X %04X\n", trk, midist, a*256+b);
#endif
                            ptrk->ptracks += 2;
                        }
                        break;

                    ///////////////////////////////////
                    // B0: Control Change
                    case 0xB0:
                        {
                            UINT controller = ptrk->ptracks[0];
                            UINT value = ptrk->ptracks[1] & 0x7F;
                            ptrk->ptracks += 2;
                            switch(controller)
                            {
                            // Bn.00.xx: Bank Select MSB (GS)
                            case 0x00:
                                pmidich->bank &= 0x7F;
                                pmidich->bank |= (value << 7);
                                break;
                            // Bn.01.xx: Modulation Depth
                            case 0x01:
                                pmidich->pitchbendrange = value;
                                break;
                            // Bn.07.xx: Volume
                            case 0x07:
                                pmidich->volume = (uint8_t)(CDLSBank::DLSMidiVolumeToLinear(value) >> 9);
                                break;
                            // Bn.0B.xx: Expression
                            case 0x0B:
                                pmidich->expression = (uint8_t)(CDLSBank::DLSMidiVolumeToLinear(value) >> 9);
                                break;
                            // Bn.0A.xx: Pan
                            case 0x0A:
                                pmidich->pan = value * 2;
                                break;
                            // Bn.20.xx: Bank Select LSB (GS)
                            case 0x20:
                                pmidich->bank &= (0x7F << 7);
                                pmidich->bank |= value;
                                break;
                            // Bn.79.00: Reset All Controllers (GM)
                            case 0x79:
                                pmidich->modulation = 0;
                                pmidich->expression = 128;
                                pmidich->pitchbend = 0x2000;
                                pmidich->pitchbendrange = 64;
                                // Should also reset pedals (40h-43h), NRP, RPN, aftertouch
                                break;
                            // Bn.78.00: All Sound Off (GS)
                            // Bn.7B.00: All Notes Off (GM)
                            case 0x78:
                            case 0x7B:
                                if (value == 0x00)
                                {
                                    // All Notes Off
                                    for (UINT k=0; k<m_nChannels; k++)
                                    {
                                        if (chnstate[k].note)
                                        {
                                            chnstate[k].flags |= CHNSTATE_NOTEOFFPENDING;
                                            chnstate[k].note = 0;
                                        }
                                    }
                                }
                                break;
                            ////////////////////////////////////
                            // Controller List
                            //
                            // Bn.02.xx: Breath Control
                            // Bn.04.xx: Foot Pedal
                            // Bn.05.xx: Portamento Time (Glissando Time)
                            // Bn.06.xx: Data Entry MSB
                            // Bn.08.xx: Balance
                            // Bn.10-13.xx: GP Control #1-#4
                            // Bn.20-3F.xx: Data LSB for controllers 0-31
                            // Bn.26.xx: Data Entry LSB
                            // Bn.40.xx: Hold Pedal #1
                            // Bn.41.xx: Portamento (GS)
                            // Bn.42.xx: Sostenuto (GS)
                            // Bn.43.xx: Soft Pedal (GS)
                            // Bn.44.xx: Legato Pedal
                            // Bn.45.xx: Hold Pedal #2
                            // Bn.46.xx: Sound Variation
                            // Bn.47.xx: Sound Timbre
                            // Bn.48.xx: Sound Release Time
                            // Bn.49.xx: Sound Attack Time
                            // Bn.4A.xx: Sound Brightness
                            // Bn.4B-4F.xx: Sound Control #6-#10
                            // Bn.50-53.xx: GP Control #5-#8
                            // Bn.54.xx: Portamento Control (GS)
                            // Bn.5B.xx: Reverb Level (GS)
                            // Bn.5C.xx: Tremolo Depth
                            // Bn.5D.xx: Chorus Level (GS)
                            // Bn.5E.xx: Celeste Depth
                            // Bn.5F.xx: Phaser Depth
                            // Bn.60.xx: Data Increment
                            // Bn.61.xx: Data Decrement
                            // Bn.62.xx: Non-RPN Parameter LSB (GS)
                            // Bn.63.xx: Non-RPN Parameter MSB (GS)
                            // Bn.64.xx: RPN Parameter LSB (GM)
                            // Bn.65.xx: RPN Parameter MSB (GM)
                            // Bn.7A.00: Local On/Off
                            // Bn.7C.00: Omni Mode Off
                            // Bn.7D.00: Omni Mode On
                            // Bn.7E.mm: Mono Mode On
                            // Bn.7F.00: Poly Mode On
#ifdef MIDI_LOG
                            default:
                                Log("Control Change %02X controller %02X = %02X\n", ptrk->status, controller, value);
#endif
                            }
                        }
                        break;

                    ////////////////////////////////
                    // C0.pp: Program Change
                    case 0xC0:
                        {
                            pmidich->program = ptrk->ptracks[0] & 0x7F;
                            ptrk->ptracks++;
#ifdef MIDI_DETAILED_LOG
                            Log("track %ld, channel %ld: program change %ld\n", trk, midich, pmidich->program);
#endif
                        }
                        break;

                    ////////////////////////////////
                    // D0: Channel Aftertouch (Polyphonic Key Pressure)
                    case 0xD0:
                        {
                            ptrk->ptracks++;
                        }
                        break;

                    ////////////////////////////////
                    // E0: Pitch Bend
                    case 0xE0:
                        {
                            pmidich->pitchbend = (uint16_t)(((UINT)ptrk->ptracks[1] << 7) + (ptrk->ptracks[0] & 0x7F));
                            for (UINT i=0; i<128; i++) if (pmidich->note_on[i])
                            {
                                UINT nchn = pmidich->note_on[i]-1;
                                if (chnstate[nchn].parent == midich)
                                {
                                    chnstate[nchn].pitchdest = pmidich->pitchbend;
                                }
                            }
#ifdef MIDI_DETAILED_LOG
                            Log("channel %ld: pitch bend = 0x%04X\n", midich, pmidich->pitchbend);
#endif
                            ptrk->ptracks+=2;
                        }
                        break;

                    //////////////////////////////////////
                    // F0 & Unsupported commands: skip it
                    default:
                        ptrk->ptracks++;
#ifdef MIDI_LOG
                        Log("track %d: unknown status byte: 0x%02X\n", trk, ptrk->status);
#endif
                    }
                }} // switch+default
                // Process to next event
                if (ptrk->ptracks)
                {
                    ptrk->nexteventtime += getmidilong(ptrk->ptracks, ptrk->ptrmax);
                }
                if (ptrk->ptracks >= ptrk->ptrmax) ptrk->ptracks = NULL;
            }
            // End reached?
            if (ptrk->ptracks >= ptrk->ptrmax) ptrk->ptracks = NULL;
        }

        ////////////////////////////////////////////////////////////////////
        // Move to next row
        if (!(dwGlobalFlags & MIDIGLOBAL_FROZEN))
        {
            // Check MOD channels status
            for (UINT ichn=0; ichn<m_nChannels; ichn++)
            {
                // Pending Global Effects ?
                if (!m[ichn].command)
                {
                    if ((chnstate[ichn].pitchsrc != chnstate[ichn].pitchdest) && (chnstate[ichn].parent))
                    {
                        int newpitch = chnstate[ichn].pitchdest;
                        int pitchbendrange = midichstate[chnstate[ichn].parent-1].pitchbendrange;
                        // +/- 256 for +/- pitch bend range
                        int slideamount = (newpitch - (int)chnstate[ichn].pitchsrc) / (int)32;
#ifdef MIDI_DETAILED_LOG
                        Log("chn%2d: portamento: src=%5d dest=%5d ", ichn, chnstate[ichn].pitchsrc, chnstate[ichn].pitchdest);
                        Log("bendamount=%5d range=%d slideamount=%d\n", pitchbendamount, pitchbendrange, slideamount);
#endif
                        if (slideamount)
                        {
                            const int ppdiv = (16 * 128 * (gnMidiImportSpeed-1));
                            newpitch = (int)chnstate[ichn].pitchsrc + slideamount;
                            if (slideamount < 0)
                            {
                                int param = (-slideamount * pitchbendrange + ppdiv/2) / ppdiv;
                                if (param >= 0x80) param = 0x80;
                                if (param > 0)
                                {
                                    m[ichn].param = (uint8_t)param;
                                    m[ichn].command = CmdPortaDown;
                                }
                            } else
                            {
                                int param = (slideamount * pitchbendrange + ppdiv/2) / ppdiv;
                                if (param >= 0x80) param = 0x80;
                                if (param > 0)
                                {
                                    m[ichn].param = (uint8_t)param;
                                    m[ichn].command = CmdPortaUp;
                                }
                            }
                        }
                        chnstate[ichn].pitchsrc = (uint16_t)newpitch;
#ifdef MIDI_DETAILED_LOG
                        Log("  newpitchsrc=%5d newpitchdest=%5d\n", chnstate[ichn].pitchsrc, chnstate[ichn].pitchdest);
#endif

                    } else
                    if (dwGlobalFlags & MIDIGLOBAL_UPDATETEMPO)
                    {
                        m[ichn].command = CmdTempo;
                        m[ichn].param = (uint8_t)tempo;
                        dwGlobalFlags &= ~MIDIGLOBAL_UPDATETEMPO;
                    } else
                    if (dwGlobalFlags & MIDIGLOBAL_UPDATEMASTERVOL)
                    {
                        m[ichn].command = CmdGlobalVol;
                        m[ichn].param = midimastervol >> 1; // 0-128
                        dwGlobalFlags &= ~MIDIGLOBAL_UPDATEMASTERVOL;
                    }
                }
                // Check pending noteoff events for m[ichn]
                if (!m[ichn].note)
                {
                    if (chnstate[ichn].flags & CHNSTATE_NOTEOFFPENDING)
                    {
                        chnstate[ichn].flags &= ~CHNSTATE_NOTEOFFPENDING;
                        m[ichn].note = 0xFF;
                    }
                    // Check State of channel
                    chnstate[ichn].idlecount++;
                    if ((chnstate[ichn].note) && (chnstate[ichn].idlecount >= 50))
                    {
                        chnstate[ichn].note = 0;
                        m[ichn].note = 0xFF;    // only if not drum channel ?
                    } else
                    if (chnstate[ichn].idlecount >= 500) // 20secs of inactivity
                    {
                        chnstate[ichn].idlecount = 0;
                        chnstate[ichn].parent = 0;
                    }
                }
            }

            if ((++row) >= Patterns[pat].GetNumRows())
            {
                pat++;
                if (pat >= MAX_PATTERNS-1) break;
                Order[pat] = pat;
                Order[pat+1] = Order.GetInvalidPatIndex();
                row = 0;
            }
        }

        // Increase midi clock
        midi_clock += nTickMultiplier;
    } while (!(dwGlobalFlags & MIDIGLOBAL_SONGENDED));
#ifdef MIDI_LOG
    Log("\n------------------ End Of File ---------------------------\n\n");
#endif
    return true;
}