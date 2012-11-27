/**
 * @file m_linkedlist.c
 * Linked list. @ingroup base

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

#include "m_linkedlist.h"

#include <string.h>
#include <stdlib.h>

#ifndef NDEBUG
#  include <stdio.h> // For debug message output.
#endif

#define COMPARETYPERELATIVE(type) \
    ((*(type*)a < *(type*)b) - (*(type*)a > *(type*)b))

#define COMPAREFUNCTYPERELATIVE(name, type) \
    COMPAREFUNC(name) { \
        return COMPARETYPERELATIVE(type); \
    }

typedef struct listnode_s {
    struct listnode_s *next, *prev;
    void       *data;
} listnode_t;

typedef struct liststate_s {
    int         flags;
    listindex_t numElements;
    int         lastError;
} liststate_t;

struct linklist_s {
    // List state:
    liststate_t *state;

    // List structure:
    listnode_t *headptr, *tailptr;
    listnode_t *unused;

    // Misc:
    comparefunc funcCompare;
};

static __inline listnode_t *allocNode(void)
{
    return malloc(sizeof(listnode_t));
}

static __inline void freeNode(listnode_t *node)
{
    free(node);
}

static __inline listnode_t *newNode(LinkList *list)
{
    listnode_t *n;

    // Do we have any unused nodes we can reuse?
    if(list->unused != NULL)
    {
        n = list->unused;
        list->unused = n->next;
    }
    else
    {   // Allocate another node.
        n = allocNode();
    }

    if(!n)
    {
        list->state->lastError = LL_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    n->next = n->prev = n->data = NULL;
    return n;
}

static __inline void moveNodeForReuse(LinkList *list, listnode_t *node)
{
    if(list->unused != NULL)
        node->next = list->unused;
    else
        node->next = NULL;

    node->prev = NULL;
    list->unused = node;
}

static __inline void insertNodeAfter(LinkList *list, listnode_t *node,
                                     listnode_t *newnode)
{
    newnode->prev = node;
    newnode->next = node->next;
    if(list->tailptr == node)
    {
        list->tailptr = newnode;
        if(list->state->flags & LCF_CIRCULAR)
            list->headptr->prev = newnode;
    }
    else
        node->next->prev = newnode;

    node->next = newnode;
}

static __inline void insertNodeBefore(LinkList *list, listnode_t *node,
                                      listnode_t *newnode)
{
    newnode->prev = node->prev;
    newnode->next = node;
    if(list->headptr == node)
    {
        list->headptr = newnode;
        if(list->state->flags & LCF_CIRCULAR)
            list->tailptr->next = newnode;
    }
    else
        node->prev->next = newnode;

    node->prev = newnode;
}

static __inline void insertNodeAtStart(LinkList *list, listnode_t *newnode)
{
    if(list->headptr == NULL)
    {
        list->headptr = list->tailptr = newnode;
        if(list->state->flags & LCF_CIRCULAR)
            newnode->prev = newnode->next = newnode;
        else
            newnode->prev = newnode->next = NULL;
    }
    else
        insertNodeBefore(list, list->headptr, newnode);
}

static __inline void insertNodeAtEnd(LinkList *list, listnode_t *newnode)
{
    if(list->tailptr == NULL)
    {
        list->headptr = list->tailptr = newnode;
        if(list->state->flags & LCF_CIRCULAR)
            newnode->prev = newnode->next = newnode;
        else
            newnode->prev = newnode->next = NULL;
    }
    else
        insertNodeAfter(list, list->tailptr, newnode);
}

static __inline void removeNode(LinkList *list, listnode_t *node)
{
    if(list->state->flags & LCF_CIRCULAR)
    {
        if(node->next == node)
        {
            list->headptr = list->tailptr = NULL;
        }
        else
        {
            node->next->prev = node->prev;
            node->prev->next = node->next;
            if(node == list->tailptr)
                list->tailptr = node->prev;
            if(node == list->headptr)
                list->headptr = node->next;
        }
    }
    else
    {
        if(node->prev == NULL)
            list->headptr = node->next;
        else
            node->prev->next = node->next;

        if(node->next == NULL)
            list->tailptr = node->prev;
        else
            node->next->prev = node->prev;
    }

    moveNodeForReuse(list, node);
}

static listindex_t findByData(const LinkList *list, const void *data)
{
    if(list->state->numElements > 0)
    {
        listnode_t *n;
        listindex_t idx;
        int         found;

        n = list->headptr;
        idx = 0;
        found = 0;
        do
        {
            if(!list->funcCompare(n->data, data))
            {
                found = 1;
            }
            else
            {
                n = n->next;
                idx++;
            }
        } while(n != ((list->state->flags & LCF_CIRCULAR)? list->headptr : NULL) &&
                !found);

        if(found)
            return idx;
    }

    list->state->lastError = LL_ERROR_UNDERFLOW;
    return -1;
}

static listnode_t *findAtPosition(const LinkList *list, listindex_t position)
{
    if(list->state->numElements > 0)
    {
        listindex_t id;
        listnode_t *n;
        int         found = 0;

        if(position < 0)
        {
            list->state->lastError = LL_ERROR_UNDERFLOW;
            return NULL;
        }

        if(position >= list->state->numElements)
        {
            list->state->lastError = LL_ERROR_OVERFLOW;
            return NULL;
        }

        if(position == 0)
            return list->headptr;

        if(position == list->state->numElements - 1)
            return list->tailptr;

        id = 0;
        n = list->headptr;
        found = 0;
        do
        {
            if(id == position)
            {
                found = 1;
            }
            else
            {
                id++;
                n = n->next;
            }
        } while(n != ((list->state->flags & LCF_CIRCULAR)? list->headptr : NULL) &&
                !found);

        if(found)
            return n;
    }

    list->state->lastError = LL_ERROR_UNDERFLOW;
    return NULL;
}

static __inline void* getAt(const LinkList *list, listindex_t position)
{
    void       *data = NULL;
    listnode_t *n;

    if((n = findAtPosition(list, position)) != NULL)
    {
        data = n->data;
    }

    return data;
}

static __inline void* extractAt(LinkList *list, listindex_t position)
{
    void       *data = NULL;
    listnode_t *n;

    if((n = findAtPosition(list, position)) != NULL)
    {
        data = n->data;
        removeNode(list, n);
        list->state->numElements--;
    }

    return data;
}

static __inline int exchangeElements(LinkList *list, listindex_t positionA,
                                     listindex_t positionB)
{
    listnode_t *a, *b;
    void       *tmp;

    if((a = findAtPosition(list, positionA)) == NULL)
        return 0;

    if((b = findAtPosition(list, positionB)) == NULL)
        return 0;

    // Do the exchange.
    tmp = a->data;
    a->data = b->data;
    b->data = tmp;

    return 1; // Success!
}

static __inline void clear(LinkList *list)
{
    if(list->state->numElements > 0)
    {
        listnode_t *n, *np;

        // Free the list contents.
        n = list->headptr;
        do
        {
            np = n->next;
            moveNodeForReuse(list, n);
            n = np;
        } while(n != ((list->state->flags & LCF_CIRCULAR)? list->headptr : NULL));

        list->state->numElements = 0;
        list->headptr = list->tailptr = NULL;
    }
}

static __inline void clearUnused(LinkList *list)
{
    listnode_t *n, *np;

    // Free any unused nodes.
    n = list->unused;
    while(n != NULL)
    {
        np = n->next;
        freeNode(n);
        n = np;
    }
    list->unused = NULL;
}

/**
 * Merge left and right lists into a new list.
 */
static listnode_t *mergeLists(listnode_t *left, listnode_t *right,
                              comparefunc funcCompare)
{
    listnode_t tmp, *np;

    np = &tmp;
    tmp.next = np;
    while(left != NULL && right != NULL)
    {
        if(funcCompare(left->data, right->data) <= 0)
        {
            np->next = left;
            np = left;

            left = left->next;
        }
        else
        {
            np->next = right;
            np = right;

            right = right->next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->next = left;
    if(right)
        np->next = right;

    // Is the list empty?
    if(tmp.next == &tmp)
        return NULL;

    return tmp.next;
}

/**
 * Split the given list into two.
 *
 * @return          Ptr to the head of the new list.
 */
static listnode_t *splitList(listnode_t *list)
{
    listnode_t *lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->next;
        lista = lista->next;
        if(lista != NULL)
            lista = lista->next;
    } while(lista);

    listc->next = NULL;
    return listb;
}

static listnode_t *doMergeSort(listnode_t *list, comparefunc funcCompare)
{
    listnode_t *p;

    if(list->next)
    {
        p = splitList(list);

        // Sort both halves and merge them back.
        list = mergeLists(doMergeSort(list, funcCompare),
                          doMergeSort(p, funcCompare), funcCompare);
    }
    return list;
}

static void mergeSort(LinkList *list)
{
    if(list->state->numElements > 1)
    {
        listnode_t     *n, *last;

        if(list->state->flags & LCF_CIRCULAR)
        {   // Break the ring so we can sort more easily.
            list->tailptr->next = NULL;
            list->headptr->prev = NULL;
        }

        list->headptr = doMergeSort(list->headptr, list->funcCompare);

        // Only singly linked atm, we need to be doubly linked.
        last = list->headptr;
        n = last->next;
        do
        {
            n->prev = last;
            last = n;
            n = n->next;
        } while(n != NULL);

        list->tailptr = last;

        // Circularly linked?
        if(list->state->flags & LCF_CIRCULAR)
        {   // Link the ring back.
            list->tailptr->next = list->headptr;
            list->headptr->prev = list->tailptr;
        }
        else
        {
            list->tailptr->next = NULL;
            list->headptr->prev = NULL;
        }
    }
}

static LinkList *createList(int flags, comparefunc cFunc)
{
    LinkList *list = malloc(sizeof(LinkList));
    liststate_t *state = malloc(sizeof(liststate_t));

    list->headptr = list->tailptr = NULL;
    list->unused = NULL;
    list->funcCompare = cFunc;

    list->state = state;
    list->state->numElements = 0;
    list->state->flags = flags;
    list->state->lastError = LL_NO_ERROR;

    return (LinkList*) list;
}

/**
 * Create a new (empty) linked list, with default paramaters.
 *
 * @return          Ptr to the newly created list.
 */
LinkList *List_New(void)
{
    return createList(0, compareAddress);
}

/**
 * Create a new (empty) linked list, with paramaters.
 *
 * @param flags     Creation flags specify list behavior:
 *                  LCF_CIRCULAR:
 *                  The list forms a ring of items, as opposed to the
 *                  default linear list.
 * @param cFunc     If @c NULL, the default compare function (compare by
 *                  address) will be set.
 * @return          Ptr to the newly created list.
 */
LinkList *List_NewWithCompareFunc(int flags, comparefunc cFunc)
{
    return createList(flags, (!cFunc? compareAddress : cFunc));
}

/**
 * Destroy a linked list.
 *
 * @param llist     Ptr to the list to be destroyed.
 */
void List_Destroy(LinkList *list)
{
    if(list)
    {
        clear(list);
        clearUnused(list);

        free(list->state);
        free(list);
    }
}

/**
 * Insert data as a new element at the beginning of the given list.
 *
 * @param llist     Ptr to the list being added to.
 * @param data      Ptr to the element being added.
 * @return          Non-zero, iff successfull.
 */
int List_InsertFront(LinkList *llist, const void *data)
{
    if(llist)
    {
        LinkList     *list = (LinkList*) llist;
        listnode_t *n;

        n = newNode(list);
        n->data = (void*) data;
        list->state->numElements++;

        // Link it in.
        insertNodeAtStart(list, n);
        return 1;
    }

    return 0;
}

/**
 * Insert data as a new element at the end of the given list.
 *
 * @param llist     Ptr to the list being added to.
 * @param data      Ptr to the element being added.
 * @return          Non-zero iff successfull.
 */
int List_InsertBack(LinkList *llist, const void *data)
{
    if(llist)
    {
        LinkList     *list = (LinkList*) llist;
        listnode_t *n;

        n = newNode(list);
        n->data = (void *) data;
        list->state->numElements++;

        // Link it in.
        insertNodeAtEnd(list, n);
        return 1;
    }

    return 0;
}

/**
 * Extract the element currently at the front of the given list.
 *
 * @param llist     Ptr to the list to remove an element from.
 * @return          Ptr to the extracted element if found and removed
 *                  successfully.
 */
void *List_ExtractFront(LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return extractAt(list, 0);
    }

    return NULL;
}

/**
 * Extract the element currently at the back of the given list.
 *
 * @param llist     Ptr to the list to remove an element from.
 * @return          Ptr to the extracted element if found and removed
 *                  successfully.
 */
void *List_ExtractBack(LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return extractAt(list, list->state->numElements - 1);
    }

    return NULL;
}

/**
 * Extract an element from the list at the given position.
 * \note An O(n) opperation.
 *
 * @param llist     Ptr to the list to extract an element from.
 * @param position  Position of the element to be extracted.
 * @return          Ptr to the extracted element if found and removed
 *                  successfully.
 */
void *List_ExtractAt(LinkList *llist, listindex_t position)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return extractAt(list, position);
    }

    return NULL;
}

/**
 * Retrieve a ptr to the element at the front of a linked list.
 *
 * @param llist     Ptr to the list to retrieve the element from.
 * @return          @c true, iff an element was found at front of the list.
 */
void *List_GetFront(const LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return getAt(list, 0);
    }

    return NULL;
}

/**
 * Retrieve a ptr to the element at the back of a linked list.
 *
 * @param llist     Ptr to the list to retrieve the element from.
 * @return          @c true, iff an element was found at back of the list.
 */
void *List_GetBack(const LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return getAt(list, list->state->numElements - 1);
    }

    return NULL;
}

/**
 * Retrieve a ptr to an element at the given position in a linked list.
 * \note An O(n) opperation.
 *
 * @param llist     Ptr to the list to retrieve the element from.
 * @param position  Position of the element to be retrieved.
 * @return          Ptr to the element if found at the given position.
 */
void *List_GetAt(const LinkList *llist, listindex_t position)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return getAt(list, position);
    }

    return NULL;
}

/**
 * Exchange two elements in the linked list.
 * \note An O(n+n) opperation!
 *
 * @param llist     Ptr to the list to exchange the elements of.
 * @param positionA Position of the first element being exchanged.
 * @param positionB Position of the send element being exchanged.
 * @return          Non-zero if successfull.
 */
int List_Exchange(LinkList *llist, listindex_t positionA,
                  listindex_t positionB)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return exchangeElements(list, positionA, positionB);
    }

    return 0;
}

/**
 * Search the link list for a specific element.
 * \note An O(n) opperation.
 *
 * @param llist     Ptr to the list to be searched.
 * @return          Index of the element in the list if found, else @c <0.
 */
listindex_t List_Find(const LinkList *llist, const void *data)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return findByData(list, data);
    }

    return -1;
}

/**
 * Sort the elements of the given list using a recursive mergesort
 * algorithm.
 *
 * Time: Average O(nlogn), Best O(nlogn), Worst: O(nlogn)
 * Space: Constant
 * Stability: Stable
 *
 * @param llist     Ptr to the list to be sorted.
 */
void List_Sort(LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        mergeSort(list);
        return;
    }
}

/**
 * Clears the given linked list. All elements are removed from the list but
 * data items attached to elements will persist.
 *
 * @param llist     Ptr to the list to be cleared.
 */
void List_Clear(LinkList *llist)
{
    if(llist)
    {
        clear((LinkList*) llist);
        return;
    }
}

/**
 * How many elements are there in the given list?
 *
 * @param llist     Ptr to the list to test.
 * @return          @c >= 0, Number of elements in the list else,
 *                  @c <0, if error.
 */
listindex_t List_Count(const LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return list->state->numElements;
    }

    return -1;
}

/**
 * Set the compare func used when sorting and searching for list elements.
 *
 * @param llist     Ptr to the list to change the compare func of.
 * @param func      Ptr to the compare func to use. Note: If @ NULL, the
 *                  default compareAddress will be assigned.
 */
void List_SetCompareFunc(LinkList *llist, comparefunc func)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        if(!func)
            list->funcCompare = compareAddress;
        else
            list->funcCompare = func;
        return;
    }
}

/**
 * Calls func for all elements in the given list, if a callback returns
 * zero; iteration is stopped immediately.
 *
 * @param llist     Ptr to the list to process.
 * @param iterateFlags
 *                  LIF_REVERSE:
 *                  Iteration will proceed from tail to head.
 * @param data      Ptr to pass to the callback.
 * @param func      Callback to make for each object.
 *
 * @return          @c <0, if error else,
 *                  @c >0, iff every callback returns non-zero else,
 *                  @c == 0, if iteration was ended by the callback.
 */
int List_Iterate(const LinkList *llist, int iterateFlags, void *data,
                 int (*callback) (void *element, void *data))
{
    if(llist)
    {
        LinkList     *list = (LinkList*) llist;
        int         retVal = 1;

        if(list->state->numElements > 0)
        {
            listnode_t *n;
            int         isDone;

            if(iterateFlags & LIF_REVERSE)
                n = list->tailptr;
            else
                n = list->headptr;

            isDone = 0;
            do
            {
                if(!callback(n->data, data))
                {
                    retVal = 0;
                    isDone = 1;
                }
                else
                {
                    if(iterateFlags & LIF_REVERSE)
                        n = n->prev;
                    else
                        n = n->next;
                }
            } while(n && !isDone);
        }

        return retVal;
    }

    return -1;
}

/**
 * Retrieve the last error reported by the given list.
 *
 * @param llist     Ptr to the list to process.
 * @return          Last error reported by the list.
 */
int List_GetError(const LinkList *llist)
{
    if(llist)
    {
        LinkList *list = (LinkList*) llist;
        return list->state->lastError;
    }

    return PTR_NOT_LIST;
}

/**
 * Compare (int) elements by their numerical relativity.
 */
COMPAREFUNCTYPERELATIVE(compareInt, int)

/**
 * Compare (unsigned int) elements by their numerical relativity.
 */
COMPAREFUNCTYPERELATIVE(compareUInt, unsigned int)

/**
 * Compare (float) elements by their numerical relativity.
 */
COMPAREFUNCTYPERELATIVE(compareFloat, float)

/**
 * Compare (double) elements by their numerical relativity.
 */
COMPAREFUNCTYPERELATIVE(compareDouble, double)

/**
 * Compare ptr elements by their memory address relativity.
 */
COMPAREFUNCTYPERELATIVE(compareAddress, void*)

/**
 * Compare (c-string) elements by their lexical relativity (case-sensitive).
 */
COMPAREFUNC(compareString)
{
    return strcmp(b, a);
}

#ifndef NDEBUG
int printInt(void *element, void *data)
{
    int         value = *(const int*) element;
    printf("  value = %i\n", value);
    return 1; // Continue iteration.
}

static int checkError(LinkList *list)
{
    int         error;

    if((error = List_GetError(list)) != LL_NO_ERROR)
        printf("List error: %i!\n", error);
    return error;
}

static void testList(int cFlags)
{
    LinkList *list;
    int         *data, i, count;
    int         integers[10] =
        {2456, 12, 76889, 45, 2, 0, -45, 680, -4005, 89};

    if(cFlags < 0)
        list = List_New();
    else
        list = List_NewWithCompareFunc(cFlags, NULL);
    checkError(list);

    if(List_Count(list) != 0)
        printf("ERROR: List not empty on create!\n");

    for(i = 0; i < 10; ++i)
    {
        List_InsertFront(list, &integers[i]);
        checkError(list);
    }

    if((count = List_Count(list)) != 10)
        printf("Error: List miscounted!\n");

    data = List_GetFront(list);
    checkError(list);
    if(!data || *data != 89)
        printf("ERROR: Incorrect first element!\n");

    data = List_GetBack(list);
    checkError(list);
    if(!data || *data != 2456)
        printf("ERROR: Incorrect last element!\n");

    //printf("  Set list compare to: compareInt\n");
    List_SetCompareFunc(list, compareInt);
    checkError(list);

    //printf("  Sort list contents.\n");
    List_Sort(list);
    checkError(list);

    if((count = List_Count(list)) != 10)
        printf("Error: List miscounted!\n");

    data = List_GetFront(list);
    checkError(list);
    if(!data || *data != 76889)
        printf("ERROR: Incorrect first element!\n");

    data = List_GetBack(list);
    checkError(list);
    if(!data || *data != -4005)
        printf("ERROR: Incorrect last element!\n");

    data = List_ExtractFront(list);
    checkError(list);
    if(!data || *data != 76889)
        printf("ERROR: Incorrect first element!\n");

    data = List_ExtractBack(list);
    checkError(list);
    if(!data || *data != -4005)
        printf("ERROR: Incorrect last element!\n");

    if((count = List_Count(list)) != 8)
        printf("Error: List miscounted!\n");

    List_Exchange(list, 1, 7);
    checkError(list);

    data = List_GetAt(list, 1);
    checkError(list);
    if(!data || *data != -45)
        printf("ERROR: Incorrect element #1!\n");

    data = List_GetAt(list, 7);
    checkError(list);
    if(!data || *data != 680)
        printf("ERROR: Incorrect element #7!\n");

    //printf("  Clearing list\n");
    List_Clear(list);
    checkError(list);

    if((count = List_Count(list)) != 0)
        printf("Error: List miscounted!\n");

    List_Destroy(list);
}

/**
 * Dry run the basic list operations to test it is functioning expectedly.
 */
void List_Test(void)
{
    printf("\n\nTesting list operations...\n");

    printf("Linear list...\n");
    testList(-1); // Linear list.
    printf("Circular list...\n");
    testList(LCF_CIRCULAR); // Circular (ring) list.

    printf("...complete.\n");
}
#endif
