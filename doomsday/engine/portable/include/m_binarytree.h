/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * m_binarytree.h: A fairly standard binary tree implementation.
 */

#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct binarytree_s;
typedef struct binarytree_s binarytree_t;

// Creation/destruction methods:
binarytree_t   *BinaryTree_Create(const void *data);
binarytree_t   *BinaryTree_Create2(const void *data, binarytree_t *initialRight,
                                   binarytree_t *initialLeft);
void            BinaryTree_Destroy(binarytree_t *node);

// State management methods:
binarytree_t   *BinaryTree_GetChild(binarytree_t *tree, boolean left);
void           *BinaryTree_GetData(binarytree_t *tree);

void            BinaryTree_SetChild(binarytree_t *tree, boolean left,
                                    binarytree_t *subtree);
void            BinaryTree_SetData(binarytree_t *tree, const void *data);

// Utility methods:
boolean         BinaryTree_IsLeaf(binarytree_t *tree);
size_t          BinaryTree_GetHeight(binarytree_t *tree);

// Standard traversals:
boolean         BinaryTree_PreOrder(binarytree_t *tree,
                                    boolean (C_DECL *callback) (binarytree_t *tree, void *data),
                                    void *data);
boolean         BinaryTree_InOrder(binarytree_t *tree,
                                   boolean (C_DECL *callback) (binarytree_t *tree, void *data),
                                   void *data);
boolean         BinaryTree_PostOrder(binarytree_t *tree,
                                     boolean (C_DECL *callback) (binarytree_t *tree, void *data),
                                     void *data);

#ifdef __cplusplus
}
#endif
#endif
