/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

/*
 * m_linkedlist.h: Linked lists.
 */

#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

// The C_DECL macro, used with compare functions.
#ifndef C_DECL
#  if defined(WIN32)
#    define C_DECL __cdecl
#  elif defined(UNIX)
#    define C_DECL
#  endif
#endif

typedef int listindex_t;

// Helper macro for defining list compare functions.
#define COMPAREFUNC(name)   int C_DECL name(const void *a, const void *b)

typedef int (C_DECL *comparefunc) (const void *a, const void *b);

// Premade compare functions:
COMPAREFUNC( compareInt );
COMPAREFUNC( compareUInt );
COMPAREFUNC( compareFloat );
COMPAREFUNC( compareDouble );
COMPAREFUNC( compareString );
COMPAREFUNC( compareAddress );

//
// Linked lists.
//
typedef void* linklist_t;

#define PTR_NOT_LIST        -1

// Linked list errors:
enum {
    LL_NO_ERROR,
    LL_ERROR_UNKNOWN,
    LL_ERROR_OUT_OF_MEMORY,
    LL_ERROR_UNDERFLOW,
    LL_ERROR_OVERFLOW,
    LL_ERROR_LAST
};

// List_Create flags:
#define LCF_CIRCULAR        0x1 // The list forms a ring of items, as
                                // opposed to the default linear list.

// List_Iterate flags
#define LIF_REVERSE         0x1 // Iteration will be tail > head.

// Create/Destroy:
linklist_t     *List_Create(void);
linklist_t     *List_Create2(int createFlags, comparefunc func);
void            List_Destroy(linklist_t *list);

// Inserts:
int             List_InsertFront(linklist_t *list, const void *data);
int             List_InsertBack(linklist_t *list, const void *data);

// Retrieves:
void*           List_GetFront(const linklist_t *list);
void*           List_GetBack(const linklist_t *list);
void*           List_GetAt(const linklist_t *list, listindex_t position);

// Extracts:
void*           List_ExtractFront(linklist_t *list);
void*           List_ExtractBack(linklist_t *list);
void*           List_ExtractAt(linklist_t *list, listindex_t position);

// Other:
void            List_SetCompareFunc(linklist_t *list, comparefunc func);
void            List_Clear(linklist_t *list);
listindex_t     List_Find(const linklist_t *list, const void *data);
int             List_Exchange(linklist_t *list, listindex_t positionA,
                              listindex_t positionB);
// Sorts:
void            List_Sort(linklist_t *list);

// State retrieval:
listindex_t     List_Count(const linklist_t *list);
int             List_GetError(const linklist_t *list);

// Iteration:
int             List_Iterate(const linklist_t *list, int iterateFlags,
                             void *data,
                             int (*callback) (void *element, void *data));
// Debugging:
#ifndef NDEBUG
void            List_Test(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
