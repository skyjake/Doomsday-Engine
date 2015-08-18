/** @file m_mus2midi.cpp  MUS data format utilities.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h"
#include "audio/m_mus2midi.h"

#include <doomsday/filesys/fs_util.h>
#include <de/Log>
#include <de/NativePath>
#include <de/memory.h>
#include <cstdio>
#include <cstring>

using namespace de;

// MUS event types.
enum
{
    MUS_EV_RELEASE_NOTE,
    MUS_EV_PLAY_NOTE,
    MUS_EV_PITCH_WHEEL,
    MUS_EV_SYSTEM,      ///< Valueless controller.
    MUS_EV_CONTROLLER,
    MUS_EV_FIVE,
    MUS_EV_SCORE_END,
    MUS_EV_SEVEN
};

// MUS controllers.
enum
{
    MUS_CTRL_INSTRUMENT,
    MUS_CTRL_BANK,
    MUS_CTRL_MODULATION,
    MUS_CTRL_VOLUME,
    MUS_CTRL_PAN,
    MUS_CTRL_EXPRESSION,
    MUS_CTRL_REVERB,
    MUS_CTRL_CHORUS,
    MUS_CTRL_SUSTAIN_PEDAL,
    MUS_CTRL_SOFT_PEDAL,

    // The valueless controllers.
    MUS_CTRL_SOUNDS_OFF,
    MUS_CTRL_NOTES_OFF,
    MUS_CTRL_MONO,
    MUS_CTRL_POLY,
    MUS_CTRL_RESET_ALL,
    NUM_MUS_CTRLS
};

#pragma pack(1)
struct mus_header_t
{
    char ID[4];                 ///< Identifier "MUS" 0x1A.
    dushort scoreLen;
    dushort scoreStart;
    dushort channels;           ///< Number of primary channels.
    dushort secondaryChannels;  ///< Number of secondary channels.
    dushort instrCnt;
    dushort padding;
    // The instrument list begins here.
};
#pragma pack()

struct mus_event_t
{
    byte channel;
    byte ev; // event.
    byte last;
};

struct midi_event_t
{
    duint deltaTime;
    byte command;
    byte size;
    byte parms[2];
};

bool M_MusRecognize(File1 &file)
{
    char buf[4];
    file.read((duint8 *)buf, 0, 4);

    // ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
    return !qstrncmp(buf, "MUS\x01a", 4);
}

static dint readTime;  ///< In ticks.
static byte *readPos;
static byte chanVols[16];  ///< Last volume for each channel.

static char ctrlMus2Midi[NUM_MUS_CTRLS] = {
     0,  ///< Not used.
     0,  ///< Bank select.
     1,  ///< Modulation.
     7,  ///< Volume.
    10,  ///< Pan.
    11,  ///< Expression.
    91,  ///< Reverb.
    93,  ///< Chorus.
    64,  ///< Sustain pedal.
    67,  ///< Soft pedal.

    // The valueless controllers:
    120,  ///< All sounds off.
    123,  ///< All notes off.
    126,  ///< Mono.
    127,  ///< Poly.
    121   ///< Reset all controllers.
};

static bool getNextEvent(midi_event_t *ev)
{
    DENG2_ASSERT(ev);

    ev->deltaTime = ::readTime;
    ::readTime = 0;

    mus_event_t evDesc;
    {
        byte const musEvent = *::readPos++;
        evDesc.channel = musEvent & 0xf;
        evDesc.ev      = (musEvent >> 4) & 0x7;
        evDesc.last    = (musEvent >> 7) & 0x1;
    }

    ev->command = 0;
    ev->size    = 0;
    std::memset(ev->parms, 0, sizeof(ev->parms));

    // Construct the MIDI event.
    switch(evDesc.ev)
    {
    case MUS_EV_PLAY_NOTE:
        ev->command = 0x90;
        ev->size = 2;
        // Which note?
        ev->parms[0] = *::readPos++;
        // Is the volume there, too?
        if(ev->parms[0] & 0x80)
            ::chanVols[evDesc.channel] = *::readPos++;
        ev->parms[0] &= 0x7f;
        ev->parms[1] = de::min<byte>(::chanVols[evDesc.channel], 127);
        break;

    case MUS_EV_RELEASE_NOTE:
        ev->command  = 0x80;
        ev->size     = 2;
        // Which note?
        ev->parms[0] = *::readPos++;
        break;

    case MUS_EV_CONTROLLER:
        ev->command  = 0xb0;
        ev->size     = 2;
        ev->parms[0] = *::readPos++;
        ev->parms[1] = *::readPos++;
        // The instrument control is mapped to another kind of MIDI event.
        if(ev->parms[0] == MUS_CTRL_INSTRUMENT)
        {
            ev->command  = 0xc0;
            ev->size     = 1;
            ev->parms[0] = ev->parms[1];
        }
        else
        {
            // Use the conversion table.
            ev->parms[0] = ctrlMus2Midi[ev->parms[0]];
        }
        break;

    case MUS_EV_PITCH_WHEEL: {
        // 2 bytes, 14 bit value. 0x2000 is the center.
        // First seven bits go to parm1, the rest to parm2.
        ev->command = 0xe0;
        ev->size = 2;
        dint val = *::readPos++ << 6;
        ev->parms[0] = val & 0x7f;
        ev->parms[1] = val >> 7;
        break; }

    case MUS_EV_SYSTEM: // Is this ever used?
        ev->command  = 0xb0;
        ev->size     = 2;
        ev->parms[0] = ctrlMus2Midi[*::readPos++];
        break;

    case MUS_EV_SCORE_END:
        // We're done.
        return false;

    default:
        LOG_RES_WARNING("Invalid MUS format music data");
        LOGDEV_RES_WARNING("Unknown MUS event %d while converting MUS to MIDI") << evDesc.ev;
        return false;
    }

    // Choose the channel.
    byte ch = evDesc.channel;
    // Redirect MUS channel 16 to MIDI channel 10 (percussion).
    if(ch == 15)
        ch = 9;
    else if(ch == 9)
        ch = 15;
    ev->command |= ch;

    // Check if this was the last event in a group.
    if(!evDesc.last)
        return true;

    // Read the time delta.
    ::readTime = 0;
    dint i = 0;
    do
    {
        i = *::readPos++;
        ::readTime = (::readTime << 7) + (i & 0x7f);
    } while(i & 0x80);

    return true;
}

bool M_Mus2Midi(void *data, size_t /*length*/, char const *outFile)
{
    LOG_AS("M_Mus2Midi");

    if(!outFile || !outFile[0]) return false;

    ddstring_t nativePath; Str_Set(Str_Init(&nativePath), outFile);
    F_ToNativeSlashes(&nativePath, &nativePath);

    /// @todo Reimplement using higher level methods for file IO.
    FILE *file = fopen(Str_Text(&nativePath), "wb");
    if(!file)
    {
        LOG_RES_WARNING("Failed opening output file \"%s\"") << NativePath(Str_Text(&nativePath)).pretty();
        Str_Free(&nativePath);
        return false;
    }
    Str_Free(&nativePath);

    uchar buffer[80];

    // Start with the MIDI header.
    strcpy((char *)buffer, "MThd");
    fwrite(buffer, 4, 1, file);

    // Header size.
    std::memset(buffer, 0, 3);
    buffer[3] = 6;
    fwrite(buffer, 4, 1, file);

    // Format (single track).
    buffer[0] = 0;
    buffer[1] = 0;
    // Number of tracks.
    buffer[2] = 0;
    buffer[3] = 1;
    // Delta ticks per quarter note (140).
    buffer[4] = 0;
    buffer[5] = 140;
    fwrite(buffer, 6, 1, file);

    // Track header.
    strcpy((char *)buffer, "MTrk");
    fwrite(buffer, 4, 1, file);

    // Length of the track in bytes.
    std::memset(buffer, 0, 4);
    dint trackSizeOffset = ftell(file);
    fwrite(buffer, 4, 1, file);  // Updated later.

    // The first MIDI ev sets the tempo.
    buffer[0] = 0;  // No delta ticks.
    buffer[1] = 0xff;
    buffer[2] = 0x51;
    buffer[3] = 3;
    buffer[4] = 0xf;  // Exactly one second per quarter note.
    buffer[5] = 0x42;
    buffer[6] = 0x40;
    fwrite(buffer, 7, 1, file);

    auto header = (mus_header_t *) data;
    ::readPos  = (byte *)data + DD_USHORT(header->scoreStart);
    ::readTime = 0;
    // Init channel volumes.
    for(dint i = 0; i < 16; ++i)
    {
        ::chanVols[i] = 64;
    }

    midi_event_t ev;
    while(getNextEvent(&ev))
    {
        // Delta time. Split into 7-bit segments.
        if(ev.deltaTime == 0)
        {
            buffer[0] = 0;
            fwrite(buffer, 1, 1, file);
        }
        else
        {
            dint i = -1;
            while(ev.deltaTime > 0)
            {
                buffer[++i] = ev.deltaTime & 0x7f;
                if(i > 0) buffer[i] |= 0x80;
                ev.deltaTime >>= 7;
            }

            // The bytes are written starting from the MSB.
            for(; i >= 0; --i)
            {
                fwrite(&buffer[i], 1, 1, file);
            }
        }

        // The ev data.
        fwrite(&ev.command, 1, 1, file);
        fwrite(&ev.parms, 1, ev.size, file);
    }

    // End of track.
    buffer[0] = 0;
    buffer[1] = 0xff;
    buffer[2] = 0x2f;
    buffer[3] = 0;
    fwrite(buffer, 4, 1, file);

    // All the MIDI data has now been written. Update the track length.
    dint const trackSize = ftell(file) - trackSizeOffset - 4;
    fseek(file, trackSizeOffset, SEEK_SET);

    buffer[3] = trackSize & 0xff;
    buffer[2] = (trackSize >> 8) & 0xff;
    buffer[1] = (trackSize >> 16) & 0xff;
    buffer[0] = trackSize >> 24;
    fwrite(buffer, 4, 1, file);

    fclose(file);

    return true;  // Success!
}

bool M_Mus2Midi(File1 &file, char const *outFile)
{
    if(!M_MusRecognize(file)) return false;

    duint8 *buf = (duint8 *) M_Malloc(file.size());
    file.read(buf, 0, file.size());
    bool result = M_Mus2Midi((void *)buf, file.size(), outFile);
    M_Free(buf); buf = nullptr;
    return result;
}
