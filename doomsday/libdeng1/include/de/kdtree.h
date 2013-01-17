/** @file kdtree.h Kd-Tree data structure.
 * @ingroup data
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#ifndef LIBDENG_KDTREE_H
#define LIBDENG_KDTREE_H

#include "aabox.h"

#ifdef __cplusplus
extern "C" {
#endif

struct kdtree_s; // opaque
struct kdtreenode_s; // opaque

typedef struct kdtree_s KdTree;
typedef struct kdtreenode_s KdTreeNode;

/**
 * Constructs a new KdTree. The kd-tree must be destroyed with KdTree_Delete()
 * when no longer needed.
 */
DENG_PUBLIC KdTree *KdTree_New(const AABox *bounds);

/**
 * Destroys the KdTree.
 *
 * @param kdTree  KdTree instance.
 */
DENG_PUBLIC void KdTree_Delete(KdTree *kdTree);

DENG_PUBLIC KdTreeNode *KdTree_Root(KdTree *kdTree);

DENG_PUBLIC int KdTree_PostTraverse2(KdTree *kdTree, int(*callback)(KdTreeNode *, void *), void *parameters);

DENG_PUBLIC int KdTree_PostTraverse(KdTree *kdTree, int(*callback)(KdTreeNode *, void *)/*, parameters=NULL*/);

DENG_PUBLIC void KdTreeNode_Delete(KdTreeNode *kdTreeNode);

DENG_PUBLIC KdTree *KdTreeNode_KdTree(KdTreeNode *kdTreeNode);

DENG_PUBLIC const AABox *KdTreeNode_Bounds(KdTreeNode *kdTreeNode);

DENG_PUBLIC void *KdTreeNode_UserData(KdTreeNode *kdTreeNode);

DENG_PUBLIC KdTreeNode *KdTreeNode_SetUserData(KdTreeNode *kdTreeNode, void *userData);

DENG_PUBLIC KdTreeNode *KdTreeNode_Parent(KdTreeNode *kdTreeNode);

DENG_PUBLIC KdTreeNode *KdTreeNode_Child(KdTreeNode *kdTreeNode, int left);

#define KdTreeNode_Right(kdTreeNode) KdTreeNode_Child((kdTreeNode), false)
#define KdTreeNode_Left(kdTreeNode)  KdTreeNode_Child((kdTreeNode), true)

DENG_PUBLIC KdTreeNode *KdTreeNode_AddChild(KdTreeNode *kdTreeNode, double distance, int vertical, int left, void *userData);

DENG_PUBLIC int KdTreeNode_Traverse2(KdTreeNode *kdTreeNode, int (*callback)(KdTreeNode *, void *), void *parameters);

DENG_PUBLIC int KdTreeNode_Traverse(KdTreeNode *kdTreeNode, int (*callback)(KdTreeNode *, void *));

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_KDTREE_H
