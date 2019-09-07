/** @file g_eventsequence.h  Input (keyboard) event sequences.
 *
 * An "event sequence" is a chain of two or more keyboard input events which
 * when entered in-sequence trigger a callback when the last event of that
 * sequence is received.
 *
 * @ingroup libcommon
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 1999 Activision
 * @author Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBCOMMON_EVENTSEQUENCE_H
#define LIBCOMMON_EVENTSEQUENCE_H

#include "doomsday.h"

#if __cplusplus
extern "C" {
#endif

/**
 * Event sequence arguments (passed to the callback handler).
 */
typedef int EventSequenceArg;

/**
 * Event sequence callback handler.
 */
typedef int (*eventsequencehandler_t) (int player, const EventSequenceArg *args, int numArgs);

// Initialize this subsystem.
void G_InitEventSequences(void);

// Shutdown this subsystem.
void G_ShutdownEventSequences(void);

/**
 * Responds to an input event if determined to be part of a known event sequence.
 *
 * @param ev  Input event to be processed.
 * @return  @c true= input event @a ev was 'eaten'.
 */
int G_EventSequenceResponder(event_t *ev);

/**
 * Add a new event sequence.
 *
 * @param sequence  Text description of the sequence.
 * @param callback  Handler function to be called upon sequence completion.
 */
void G_AddEventSequence(const char *sequence, eventsequencehandler_t callback);

/**
 * Add a new event sequence.
 *
 * @param sequence     Text description of the sequence.
 * @param cmdTemplate  Templated console command to be executed upon sequence completion.
 */
void G_AddEventSequenceCommand(const char *sequence, const char *commandTemplate);

#if __cplusplus
} // extern "C"
#endif

#endif  // LIBCOMMON_EVENTSEQUENCE_H
