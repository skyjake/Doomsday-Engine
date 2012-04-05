/**
 * @file bspbuilder.cpp
 * Public interface to the BspBuilder class. @ingroup map
 *
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include <BspBuilder>
#include "portable/src/map/bspbuilder/bspbuilder_instance.h"

using namespace de;

BspBuilder::BspBuilder(GameMap* map, int splitCostFactor)
{
    d = new bspbuilder::BspBuilderImp(map, splitCostFactor);
}

BspBuilder::~BspBuilder()
{
    delete d;
}

BspBuilder& BspBuilder::setSplitCostFactor(int factor)
{
    d->setSplitCostFactor(factor);
    return *this;
}

bool BspBuilder::build()
{
    return d->build();
}

de::BinaryTree<void*>* BspBuilder::root() const
{
    return d->root();
}
