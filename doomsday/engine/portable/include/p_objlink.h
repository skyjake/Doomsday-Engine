/**\file p_objlink.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Object > Surface Contact Lists
 *
 * Implements subsector contact spreading.
 */

#ifndef LIBDENG_REFRESH_OBJLINK_H
#define LIBDENG_REFRESH_OBJLINK_H

typedef enum {
    OT_MOBJ,
    OT_LUMOBJ,
    NUM_OBJ_TYPES
} objtype_t;

#define VALID_OBJTYPE(v) ((v) >= OT_MOBJ && (v) <= OT_LUMOBJ)

void R_InitObjLinksForMap(void);

void R_DestroyObjLinks(void);

/**
 * Called at the begining of each frame (iff the render lists are not frozen)
 * by R_BeginWorldFrame().
 */
void R_ClearObjLinksForFrame(void);

/**
 * Initialize the obj > subsector contact lists ready for adding new
 * luminous objects. Called by R_BeginWorldFrame() at the beginning of a new
 * frame (if the render lists are not frozen).
 */
void R_InitForNewFrame(void);

/**
 * Called by R_BeginWorldFrame() at the beginning of render tic (iff the
 * render lists are not frozen) to link all objlinks into the objlink
 * blockmap.
 */
void R_LinkObjs(void);

void R_ObjLinkCreate(void* object, objtype_t type);

/**
 * Perform any processing needed before we can draw surfaces within the
 * specified subsector.
 *
 * @param ssec  Subsector to process.
 */
void R_InitForSubsector(subsector_t* ssec);

typedef struct {
    void* obj;
    objtype_t type;
} linkobjtossecparams_t;

int RIT_LinkObjToSubsector(subsector_t* ssec, void* paramaters);

/**
 * Iterate over subsector contacts of the specified type, making a callback for
 * each visited. Iteration ends when all selected contacts have been visited or
 * a callback returns non-zero.
 *
 * @param ssec  Subsector whoose contact list to process.
 * @param type  Type of objects to be processed.
 * @param callback  Callback to make for each visited contact.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int R_IterateSubsectorContacts2(subsector_t* ssec, objtype_t type,
    int (*callback) (void* object, void* paramaters), void* paramaters);
int R_IterateSubsectorContacts(subsector_t* ssec, objtype_t type,
    int (*callback) (void* object, void* paramaters)); /* paramaters = NULL */

#endif /* LIBDENG_REFRESH_OBJLINK_H */
