/**
 * @file m_linkedlist.h
 * Linked list. @ingroup base
 *
 * A pretty simple linked list implementation that supports
 * single, double and ring lists.

 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_LINKEDLIST_H
#define LIBDENG_LINKEDLIST_H

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

#ifdef __cplusplus
extern "C" {
#endif

// Premade compare functions:
COMPAREFUNC( compareInt );
COMPAREFUNC( compareUInt );
COMPAREFUNC( compareFloat );
COMPAREFUNC( compareDouble );
COMPAREFUNC( compareString );
COMPAREFUNC( compareAddress );

struct linklist_s;

/**
 * LinkList instance. Use List_New() to construct.
 */
typedef struct linklist_s LinkList;

#define PTR_NOT_LIST        -1

/// Linked list errors.
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

LinkList*       List_New(void);
LinkList*       List_NewWithCompareFunc(int createFlags, comparefunc func);
void            List_Destroy(LinkList *list);

// Inserts:
int             List_InsertFront(LinkList *list, const void *data);
int             List_InsertBack(LinkList *list, const void *data);

// Retrieves:
void*           List_GetFront(const LinkList *list);
void*           List_GetBack(const LinkList *list);
void*           List_GetAt(const LinkList *list, listindex_t position);

// Extracts:
void*           List_ExtractFront(LinkList *list);
void*           List_ExtractBack(LinkList *list);
void*           List_ExtractAt(LinkList *list, listindex_t position);

// Other:
void            List_SetCompareFunc(LinkList *list, comparefunc func);
void            List_Clear(LinkList *list);
listindex_t     List_Find(const LinkList *list, const void *data);
int             List_Exchange(LinkList *list, listindex_t positionA,
                              listindex_t positionB);
// Sorts:
void            List_Sort(LinkList *list);

// State retrieval:
listindex_t     List_Count(const LinkList *list);
int             List_GetError(const LinkList *list);

// Iteration:
int             List_Iterate(const LinkList *list, int iterateFlags,
                             void *data,
                             int (*callback) (void *element, void *data));
// Debugging:
#ifndef NDEBUG
void            List_Test(void);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_LINKEDLIST_H
