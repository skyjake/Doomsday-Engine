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

struct kdtree_s;

typedef struct kdtree_s KdTree;

KdTree* KdTree_New(const AABox* bounds);
KdTree* KdTree_NewWithUserData(const AABox* bounds, void* userData);

void KdTree_Delete(KdTree* kdTree);

const AABox* KdTree_Bounds(KdTree* kdTree);

void* KdTree_UserData(KdTree* kdTree);
KdTree* KdTree_SetUserData(KdTree* kdTree, void* userData);

KdTree* KdTree_Child(KdTree* kdTree, int left);

KdTree* KdTree_AddChild(KdTree* kdTree, const AABox* bounds, int left, void* userData);

int KdTree_Traverse2(KdTree* kdTree, int (*callback)(KdTree*, void*), void* parameters);
int KdTree_Traverse(KdTree* kdTree, int (*callback)(KdTree*, void*), void* parameters);

int KdTree_PostTraverse2(KdTree* kdTree, int(*callback)(KdTree*, void*), void* parameters);
int KdTree_PostTraverse(KdTree* kdTree, int(*callback)(KdTree*, void*)/*, parameters=NULL*/);

#endif /// LIBDENG_KDTREE
