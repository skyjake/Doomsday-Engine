/** @file bsptreenode.h BSP Builder BspTreeNode.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_BSPBUILDER_BSPTREENODE
#define LIBDENG_BSPBUILDER_BSPTREENODE

#include <de/BinaryTree>
#include "MapElement"

/**
 * Nodes in BspBuilder's internal tree are modelled with this type.
 *
 * @ingroup bsp
 */
typedef de::BinaryTree<de::MapElement *> BspTreeNode;

#endif // LIBDENG_BSPBUILDER_BSPTREENODE
