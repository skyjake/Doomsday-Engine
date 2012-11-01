/**
 * @file thinker.h
 * Thinkers. @ingroup thinker
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_THINKER_H
#define LIBDENG_THINKER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup thinker Thinkers
 * @ingroup playsim
 */
///@{

/// Function pointer to a function to handle an actor's thinking.
typedef void    (*think_t) ();

/**
 * Base for all thinker objects.
 */
typedef struct thinker_s {
    struct thinker_s *prev, *next;
    think_t         function;
    boolean         inStasis;
    thid_t          id; ///< Only used for mobjs (zero is not an ID).
} thinker_t;

void DD_InitThinkers(void);
void DD_RunThinkers(void);
void DD_ThinkerAdd(thinker_t* th);
void DD_ThinkerRemove(thinker_t* th);
void DD_ThinkerSetStasis(thinker_t* th, boolean on);
int DD_IterateThinkers(think_t func, int (*callback) (thinker_t*, void*), void* context);

///@}

/// Not part of the public API.
///@{
boolean P_IsMobjThinker(think_t thinker);
///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_THINKER_H
