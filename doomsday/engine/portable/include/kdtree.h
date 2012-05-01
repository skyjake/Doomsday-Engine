/**
 * @file kdtree.h
 * Kd-Tree data structure. @ingroup data
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_KDTREE
#define LIBDENG_KDTREE

#include "de_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct kdtree_s;
struct kdtreenode_s;

typedef struct kdtree_s KdTree;
typedef struct kdtreenode_s KdTreeNode;

/**
 * Constructs a new KdTree. The kd-tree must be destroyed with KdTree_Delete()
 * when no longer needed.
 */
KdTree* KdTree_New(const AABox* bounds);

/**
 * Destroys the KdTree.
 *
 * @param kdTree  KdTree instance.
 */
void KdTree_Delete(KdTree* kdTree);

struct kdtreenode_s* KdTree_Root(KdTree* kdTree);

int KdTree_PostTraverse2(KdTree* kdTree, int(*callback)(KdTreeNode*, void*), void* parameters);
int KdTree_PostTraverse(KdTree* kdTree, int(*callback)(KdTreeNode*, void*)/*, parameters=NULL*/);

KdTree* KdTreeNode_KdTree(KdTreeNode* kdTreeNode);

const AABox* KdTreeNode_Bounds(KdTreeNode* kdTreeNode);

void* KdTreeNode_UserData(KdTreeNode* kdTreeNode);
KdTreeNode* KdTreeNode_SetUserData(KdTreeNode* kdTreeNode, void* userData);

KdTreeNode* KdTreeNode_Parent(KdTreeNode* kdTreeNode);

KdTreeNode* KdTreeNode_Child(KdTreeNode* kdTreeNode, int left);

#define KdTreeNode_Right(kdTreeNode) KdTreeNode_Child((kdTreeNode), false)
#define KdTreeNode_Left(kdTreeNode)  KdTreeNode_Child((kdTreeNode), true)

KdTreeNode* KdTreeNode_AddChild(KdTreeNode* kdTreeNode, double distance, int vertical, int left, void* userData);

#define KdTreeNode_AddRight(kdTreeNode), distance, vertical, userData) KdTreeNode_AddChild((kdTreeNode), (distance), (vertical), false, (userData))
#define KdTreeNode_AddLeft(kdTreeNode), distance, vertical, userData) KdTreeNode_AddChild((kdTreeNode), (distance), (vertical), true, (userData))

int KdTreeNode_Traverse2(KdTreeNode* kdTreeNode, int (*callback)(KdTreeNode*, void*), void* parameters);
int KdTreeNode_Traverse(KdTreeNode* kdTreeNode, int (*callback)(KdTreeNode*, void*), void* parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_KDTREE
