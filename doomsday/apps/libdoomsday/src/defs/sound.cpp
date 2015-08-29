/** @file sound.cpp  Sound definition accessor.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/sound.h"
#include "doomsday/defs/ded.h"

using namespace de;

namespace defn {

void Sound::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText("id", "");           // ID of this sound, refered to by others.
    def().addText("name", "");         // A tag name for the sound.
    def().addText("lumpName", "");     // Actual lump name of the sound ("DS" not included).
    def().addText("ext", "");          // External sound file (WAV).
    def().addText("link", "");         // Link to another sound.
    def().addNumber("linkPitch", 0);   // Playback frequency (factor).
    def().addNumber("linkVolume", 0);  // Playback volume (factor).
    def().addNumber("priority", 0);    // Priority classification.
    def().addNumber("channels", 0);    // Max number of channels to occupy.
    def().addNumber("group", 0);       // Exclusion group.
    def().addNumber("flags", 0);       // sf_* flags.
}

}  // namespace defn
