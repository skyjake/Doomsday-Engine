/** @file map/hedge.cpp World Map Geometry Half-Edge.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <QtAlgorithms>

#include <de/Log>

#include "BspLeaf"
#include "Line"
#include "Vertex"
#include "map/lineowner.h"
#include "map/r_world.h" /// R_GetVtxLineOwner @todo remove me

#include "map/hedge.h"

using namespace de;

DENG2_PIMPL(HEdge)
{
    /// Map Line::Side attributed to the half-edge. Can be @c 0 (mini-edge).
    Line::Side *lineSide;

    Instance(Public *i)
        : Base(i),
          lineSide(0)
    {}
};

HEdge::HEdge(Vertex &from, Line::Side *lineSide)
    : MapElement(DMU_HEDGE), d(new Instance(this))
{
    _from = &from;
    _to = 0;
    _next = 0;
    _prev = 0;
    _twin = 0;
    _bspLeaf = 0;
    _angle = 0;
    _length = 0;
    _lineOffset = 0;
    std::memset(_bsuf, 0, sizeof(_bsuf));
    _frameFlags = 0;

    d->lineSide = lineSide;
}

HEdge::HEdge(HEdge const &other)
    : MapElement(DMU_HEDGE), d(new Instance(this))
{
    _from = other._from;
    _to = other._to;
    _next = other._next;
    _prev = other._prev;
    _twin = other._twin;
    _bspLeaf = other._bspLeaf;
    _angle = other._angle;
    _length = other._length;
    _lineOffset = other._lineOffset;
    std::memcpy(_bsuf, other._bsuf, sizeof(_bsuf));
    _frameFlags = other._frameFlags;

    d->lineSide = other.d->lineSide;
}

HEdge::~HEdge()
{
#ifdef __CLIENT__
    for(uint i = 0; i < 3; ++i)
    {
        if(_bsuf[i])
        {
            SB_DestroySurface(_bsuf[i]);
        }
    }
#endif
}

Vertex &HEdge::vertex(int to)
{
    DENG_ASSERT((to? _to : _from) != 0);
    return to? *_to : *_from;
}

Vertex const &HEdge::vertex(int to) const
{
    return const_cast<Vertex const &>(const_cast<HEdge *>(this)->vertex(to));
}

HEdge &HEdge::next() const
{
    DENG2_ASSERT(_next != 0);
    return *_next;
}

HEdge &HEdge::prev() const
{
    DENG2_ASSERT(_prev != 0);
    return *_prev;
}

bool HEdge::hasTwin() const
{
    return _twin != 0;
}

HEdge &HEdge::twin() const
{
    if(_twin)
    {
        return *_twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("HEdge::twin", "No twin half-edge is associated");
}

bool HEdge::hasBspLeaf() const
{
    return _bspLeaf != 0;
}

BspLeaf &HEdge::bspLeaf() const
{
    if(_bspLeaf)
    {
        return *_bspLeaf;
    }
    /// @throw MissingBspLeafError Attempted with no BSP leaf associated.
    throw MissingBspLeafError("HEdge::bspLeaf", "No BSP leaf is associated");
}

bool HEdge::hasLineSide() const
{
    return d->lineSide != 0;
}

Line::Side &HEdge::lineSide() const
{
    if(d->lineSide)
    {
        return *d->lineSide;
    }
    /// @throw MissingLineError Attempted with no line attributed.
    throw MissingLineSideError("HEdge::lineSide", "No line.side is attributed");
}

coord_t HEdge::lineOffset() const
{
    if(d->lineSide)
    {
        return _lineOffset;
    }
    /// @throw MissingLineError Attempted with no line attributed.
    throw MissingLineSideError("HEdge::lineOffset", "No line.side is attributed");
}

angle_t HEdge::angle() const
{
    return _angle;
}

coord_t HEdge::length() const
{
    return _length;
}

static walldivnode_t *findWallDivNodeByZOrigin(walldivs_t *wallDivs, coord_t height)
{
    DENG2_ASSERT(wallDivs != 0);
    for(uint i = 0; i < wallDivs->num; ++i)
    {
        walldivnode_t *node = &wallDivs->nodes[i];
        if(node->height == height)
            return node;
    }
    return 0;
}

static void addWallDivNodesForPlaneIntercepts(HEdge const *hedge, walldivs_t *wallDivs,
    int section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    bool const clockwise = !doRight;

    // Polyobj edges are never split.
    if(!hedge->hasLineSide() || hedge->line().isFromPolyobj()) return;

    bool const isTwoSided = (hedge->line().hasFrontSections() && hedge->line().hasBackSections())? true:false;

    // Check for neighborhood division?
    if(section == Line::Side::Middle && isTwoSided) return;

    // Only edges at line ends can/should be split.
    if(!((hedge == hedge->lineSide().leftHEdge()  && !doRight) ||
         (hedge == hedge->lineSide().rightHEdge() &&  doRight)))
        return;

    if(bottomZ >= topZ) return; // Obviously no division.

    Sector const *frontSec = hedge->lineSide().sectorPtr();

    // Retrieve the start owner node.
    LineOwner *base = R_GetVtxLineOwner(&hedge->lineSide().vertex(doRight), &hedge->line());
    LineOwner *own = base;
    bool stopScan = false;
    do
    {
        own = own->_link[clockwise];

        if(own == base)
        {
            stopScan = true;
        }
        else
        {
            Line *iter = &own->line();

            if(iter->isSelfReferencing())
                continue;

            uint i = 0;
            do
            {   // First front, then back.
                Sector *scanSec = NULL;
                if(!i && iter->hasFrontSections() && iter->frontSectorPtr() != frontSec)
                    scanSec = iter->frontSectorPtr();
                else if(i && iter->hasBackSections() && iter->backSectorPtr() != frontSec)
                    scanSec = iter->backSectorPtr();

                if(scanSec)
                {
                    if(scanSec->ceiling().visHeight() - scanSec->floor().visHeight() > 0)
                    {
                        for(int j = 0; j < scanSec->planeCount() && !stopScan; ++j)
                        {
                            Plane const &plane = scanSec->plane(j);

                            if(plane.visHeight() > bottomZ && plane.visHeight() < topZ)
                            {
                                if(!findWallDivNodeByZOrigin(wallDivs, plane.visHeight()))
                                {
                                    WallDivs_Append(wallDivs, plane.visHeight());

                                    // Have we reached the div limit?
                                    if(wallDivs->num == WALLDIVS_MAX_NODES)
                                        stopScan = true;
                                }
                            }

                            if(!stopScan)
                            {
                                // Clip a range bound to this height?
                                if(plane.type() == Plane::Floor && plane.visHeight() > bottomZ)
                                    bottomZ = plane.visHeight();
                                else if(plane.type() == Plane::Ceiling && plane.visHeight() < topZ)
                                    topZ = plane.visHeight();

                                // All clipped away?
                                if(bottomZ >= topZ)
                                    stopScan = true;
                            }
                        }
                    }
                    else
                    {
                        /**
                         * A zero height sector is a special case. In this
                         * instance, the potential division is at the height
                         * of the back ceiling. This is because elsewhere
                         * we automatically fix the case of a floor above a
                         * ceiling by lowering the floor.
                         */
                        coord_t z = scanSec->ceiling().visHeight();

                        if(z > bottomZ && z < topZ)
                        {
                            if(!findWallDivNodeByZOrigin(wallDivs, z))
                            {
                                WallDivs_Append(wallDivs, z);

                                // All clipped away.
                                stopScan = true;
                            }
                        }
                    }
                }
            } while(!stopScan && ++i < 2);

            // Stop the scan when a single sided line is reached.
            if(!iter->hasFrontSections() || !iter->hasBackSections())
                stopScan = true;
        }
    } while(!stopScan);
}

static int sortWallDivNode(void const *e1, void const *e2)
{
    coord_t const h1 = ((walldivnode_t *)e1)->height;
    coord_t const h2 = ((walldivnode_t *)e2)->height;
    if(h1 > h2) return  1;
    if(h2 > h1) return -1;
    return 0;
}

static void buildWallDiv(walldivs_t *wallDivs, HEdge const *hedge,
   int section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    DENG_ASSERT(wallDivs->num == 0);

    // Nodes are arranged according to their Z axis height in ascending order.
    // The first node is the bottom.
    WallDivs_Append(wallDivs, bottomZ);

    // Add nodes for intercepts.
    addWallDivNodesForPlaneIntercepts(hedge, wallDivs, section, bottomZ, topZ, doRight);

    // The last node is the top.
    WallDivs_Append(wallDivs, topZ);

    if(!(wallDivs->num > 2)) return;

    // Sorting is required. This shouldn't take too long...
    // There seldom are more than two or three nodes.
    qsort(wallDivs->nodes, wallDivs->num, sizeof(*wallDivs->nodes), sortWallDivNode);

    WallDivs_AssertSorted(wallDivs);
    WallDivs_AssertInRange(wallDivs, bottomZ, topZ);
}

bool HEdge::prepareWallDivs(int section, walldivs_t *leftWallDivs,
    walldivs_t *rightWallDivs, Vector2f *materialOrigin) const
{
    DENG_ASSERT(hasLineSide());

    Sector const *frontSec, *backSec;

    if(!line().isSelfReferencing())
    {
        frontSec = bspLeafSectorPtr();
        backSec  = hasTwin()? twin().bspLeafSectorPtr() : 0;
    }
    else
    {
        frontSec = backSec = lineSide().sectorPtr();
    }

    coord_t bottom, top;
    bool visible = R_SideSectionCoords(lineSide(), section, frontSec, backSec,
                                       &bottom, &top, materialOrigin);

    if(materialOrigin)
    {
        materialOrigin->x += float(_lineOffset);
    }

    if(!visible) return false;

    buildWallDiv(leftWallDivs,  this, section, bottom, top, false/*is-left-edge*/);
    buildWallDiv(rightWallDivs, this, section, bottom, top, true/*is-right-edge*/);

    return true;
}

biassurface_t &HEdge::biasSurfaceForGeometryGroup(uint groupId)
{
    if(groupId <= Line::Side::Top)
    {
        DENG2_ASSERT(_bsuf[groupId]);
        return *_bsuf[groupId];
    }
    /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
    throw UnknownGeometryGroupError("HEdge::biasSurfaceForGeometryGroup", QString("Invalid group id %1").arg(groupId));
}

coord_t HEdge::pointDistance(const_pvec2d_t point, coord_t *offset) const
{
    /// @todo Why are we calculating this every time?
    Vector2d direction = _to->origin() - _from->origin();

    coord_t fromOriginV1[2] = { fromOrigin().x, fromOrigin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointLineDistance(point, fromOriginV1, directionV1, offset);
}

coord_t HEdge::pointOnSide(const_pvec2d_t point) const
{
    DENG_ASSERT(point != 0);
    /// @todo Why are we calculating this every time?
    Vector2d direction = _to->origin() - _from->origin();

    coord_t fromOriginV1[2] = { fromOrigin().x, fromOrigin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(point, fromOriginV1, directionV1);
}

int HEdge::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_HEDGE_V, &_from, &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_HEDGE_V, &_to, &args, 0);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_HEDGE_LENGTH, &_length, &args, 0);
        break;
    case DMU_OFFSET: {
        coord_t offset = d->lineSide? _lineOffset : 0;
        DMU_GetValue(DMT_HEDGE_OFFSET, &offset, &args, 0);
        break; }
    case DMU_SIDE:
        DMU_GetValue(DMT_HEDGE_SIDE, &d->lineSide, &args, 0);
        break;
    case DMU_LINE: {
        Line *lineAdr = d->lineSide? &d->lineSide->line() : 0;
        DMU_GetValue(DMT_HEDGE_LINE, &lineAdr, &args, 0);
        break; }
    case DMU_FRONT_SECTOR: {
        Sector *sector = bspLeafSectorPtr();
        DMU_GetValue(DMT_HEDGE_SECTOR, &sector, &args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector *sector = _twin? _twin->bspLeafSectorPtr() : 0;
        DMU_GetValue(DMT_HEDGE_SECTOR, &sector, &args, 0);
        break; }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &_angle, &args, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

// WallDivs ----------------------------------------------------------------
/// @todo Move the following to another file

coord_t WallDivNode_Height(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    return node->height;
}

walldivnode_t *WallDivNode_Next(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx + 1 >= node->divs->num) return 0;
    return &node->divs->nodes[idx+1];
}

walldivnode_t *WallDivNode_Prev(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx == 0) return 0;
    return &node->divs->nodes[idx-1];
}

uint WallDivs_Size(walldivs_t const *wd)
{
    DENG2_ASSERT(wd);
    return wd->num;
}

walldivnode_t *WallDivs_First(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    return &wd->nodes[0];
}

walldivnode_t *WallDivs_Last(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    return &wd->nodes[wd->num-1];
}

walldivs_t *WallDivs_Append(walldivs_t *wd, coord_t height)
{
    DENG2_ASSERT(wd);
    struct walldivnode_s *node = &wd->nodes[wd->num++];
    node->divs = wd;
    node->height = height;
    return wd;
}

void WallDivs_AssertSorted(walldivs_t *wd)
{
#ifdef DENG_DEBUG
    walldivnode_t *node = WallDivs_First(wd);
    coord_t highest = WallDivNode_Height(node);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        DENG2_ASSERT(node->height >= highest);
        highest = node->height;
    }
#else
    DENG_UNUSED(wd);
#endif
}

void WallDivs_AssertInRange(walldivs_t *wd, coord_t low, coord_t hi)
{
#ifdef DENG_DEBUG
    DENG2_ASSERT(wd);
    walldivnode_t *node = WallDivs_First(wd);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        DENG2_ASSERT(node->height >= low && node->height <= hi);
    }
#else
    DENG2_UNUSED3(wd, ow, hi);
#endif
}

#ifdef DENG_DEBUG
void WallDivs_DebugPrint(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    LOG_DEBUG("WallDivs [%p]:") << wd;
    for(uint i = 0; i < wd->num; ++i)
    {
        walldivnode_t *node = &wd->nodes[i];
        LOG_DEBUG("  %i: %f") << i << node->height;
    }
}
#endif
