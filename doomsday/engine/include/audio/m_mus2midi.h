/** @file m_mus2midi.h MUS to MIDI conversion.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_MUS2MIDI_H__
#define __DOOMSDAY_MUS2MIDI_H__

/**
 * Converts DOOM MUS format music into MIDI music. The output is written to a
 * native file.
 *
 * @param data     The MUS data to convert.
 * @param length   The length of the data in bytes.
 * @param outFile  Name of the file the resulting MIDI data will be written to.
 */
boolean M_Mus2Midi(void* data, size_t length, const char* outFile);

#endif
