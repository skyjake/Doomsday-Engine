/** @file rend_clip.cpp Angle Clipper (clipnodes and oranges). 
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

#include <cstdlib>

#include <de/Log>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"

#include "render/rend_clip.h"

using namespace de;

struct RoverNode
{
    RoverNode *prev, *next;
};

struct Rover
{
    RoverNode *first;
    RoverNode *last;
    RoverNode *rover;
};

struct ClipNode
{
    RoverNode rover;

    /// Previous and next nodes.
    ClipNode *prev, *next;

    /// The start and end angles (start < end).
    binangle_t start, end;
};

/**
 * @defgroup occlussionNodeFlags  OcclussionNodeFlags
 * @ingroup flags
 */
///@{
#define OCNF_TOPHALF                0x1 ///< Otherwise bottom half.
///@}

struct OccNode
{
    RoverNode rover;

    /// Previous and next nodes.
    OccNode *prev, *next;

    /// @ref occlussionNodeFlags
    byte flags;

    /// Start and end angles of the segment (start < end).
    binangle_t start, end;

    /// The normal of the occlusion plane.
    Vector3f normal;
};

static void C_CutOcclusionRange(binangle_t startAngle, binangle_t endAngle);
static int C_SafeCheckRange(binangle_t startAngle, binangle_t endAngle);

#if _DEBUG
static void C_OrangeRanger(int mark);
static void C_OcclusionLister(void);
#endif

int devNoCulling = 0; ///< cvar. Set to 1 to fully disable angle based culling.

/// The list of clipnodes.
static Rover clipNodes;

/// Head of the clipped regions list.
static ClipNode *clipHead; // The head node.

/// The list of occlusion nodes.
static Rover occNodes;

/// Head of the occlusion range list.
static OccNode *occHead; // The head occlusion node.

static int anglistSize;
static binangle_t *anglist;

/**
 * @note The point should be view-relative!
 */
static inline binangle_t pointToAngle(Vector2d const &point)
{
    // Shift for more accuracy;
    return bamsAtan2(int(point.y * 100), int(point.x * 100));
}

static void C_RoverInit(Rover *rover)
{
    std::memset(rover, 0, sizeof(*rover));
}

static void C_RoverRewind(Rover *r)
{
    r->rover = r->first;
}

static void *C_RoverGet(Rover *r)
{
    if(!r->rover) return 0;

    // We'll use this.
    void *node = r->rover;
    r->rover = r->rover->next;
    return node;
}

static void C_RoverAdd(Rover *r, RoverNode *node)
{
    // Link it to the start of the rover's list.
    if(!r->last)
        r->last = node;
    if(r->first)
        r->first->prev = node;

    node->next = r->first;
    node->prev = 0;
    r->first = node;
}

static void C_RoverRemove(Rover *r, RoverNode *node)
{
    DENG_ASSERT(r->last);

    if(node == r->last)
    {
        DENG_ASSERT(!r->rover);

        // We can only remove the last if all nodes are already in use.
        r->rover = node;
    }
    else
    {
        // Unlink from the list entirely.
        node->next->prev = node->prev;
        if(node->prev)
        {
            node->prev->next = node->next;
        }
        else
        {
            r->first = r->first->next;
            r->first->prev = 0;
        }

        // Put it back to the end of the list.
        r->last->next = node;
        node->prev = r->last;
        node->next = 0;
        r->last = node;

        // If all were in use, set the rover here. Otherwise the rover
        // can stay where it is.
        if(!r->rover)
            r->rover = r->last;
    }
}

/**
 * Finds the first unused clip node.
 */
static ClipNode *C_NewRange(binangle_t stAng, binangle_t endAng)
{
    ClipNode *node = reinterpret_cast<ClipNode *>(C_RoverGet(&clipNodes));
    if(!node)
    {
        // Allocate a new node and add it to head the list.
        node = static_cast<ClipNode *>(Z_Malloc(sizeof(ClipNode), PU_APPSTATIC, 0));
        C_RoverAdd(&clipNodes, reinterpret_cast<RoverNode *>(node));
    }

    // Initialize the node.
    node->start = stAng;
    node->end = endAng;
    node->prev = node->next = 0;
    return node;
}

static void C_RemoveRange(ClipNode *node)
{
    // If this is the head, move it.
    if(clipHead == node)
        clipHead = node->next;

    // Unlink from the clipper.
    if(node->prev)
        node->prev->next = node->next;
    if(node->next)
        node->next->prev = node->prev;
    node->prev = node->next = 0;

    // Move this node to the free node rover.
    C_RoverRemove(&clipNodes, reinterpret_cast<RoverNode *>(node));
}

static void C_AddRange(binangle_t startAngle, binangle_t endAngle)
{
    // This range becomes a solid segment: cut everything away from the
    // corresponding occlusion range.
    C_CutOcclusionRange(startAngle, endAngle);

    // If there is no head, this will be the first range.
    //LOG_AS("Clipper");
    if(!clipHead)
    {
        clipHead = C_NewRange(startAngle, endAngle);

        /*
        LOG_DEBUG(QString("New head node added: %1 => %2")
                  .arg(clipHead->start, 0, 16)
                  .arg(clipHead->end, 0, 16)); */
        return;
    }

    // There are previous ranges. Check that the new range isn't contained
    // by any of them.
    for(ClipNode *ci = clipHead; ci; ci = ci->next)
    {
        /*
        LOG_DEBUG(QString("%1: %2 => %3")
                  .arg(ci)
                  .arg(ci->start, 0, 16)
                  .arg(ci->end, 0, 16));*/

        if(startAngle >= ci->start && endAngle <= ci->end)
        {
            //LOG_DEBUG("Range already exists");
            return; // The new range already exists.
        }

#ifdef DENG_DEBUG
        if(ci == ci->next)
            Con_Error("C_AddRange: loop1 %p linked to itself: %x => %x\n", ci, ci->start, ci->end);
#endif
    }

    // Now check if any of the old ranges are contained by the new one.
    for(ClipNode *ci = clipHead; ci;)
    {
        if(ci->start >= startAngle && ci->end <= endAngle)
        {
            ClipNode *crange = ci;

            /*
            LOG_DEBUG(QString("Removing contained range %1 => %2")
                      .arg(crange->start, 0, 16)
                      .arg(crange->end, 0, 16));*/

            // We must do this in order to keep the loop from breaking.
            ci = ci->next;
            C_RemoveRange(crange);

            //if(!ci) ci = clipHead;
            //if(!ci) break;
            continue;
        }
        ci = ci->next;
    }

    // Now it is possible that the new range overlaps one or two old ranges.
    // If two are overlapped, they are consecutive. First we'll try to find
    // a range that overlaps the beginning.
    ClipNode *crange = 0;
    for(ClipNode *ci = clipHead; ci; ci = ci->next)
    {
        // In preparation for the next stage, find a good spot for the range.
        if(ci->start < endAngle)
        {
            // After this one.
            crange = ci;
        }

        if(ci->start >= startAngle && ci->start <= endAngle)
        {
            // New range's end and ci's beginning overlap. ci's end is outside.
            // Otherwise it would have been already removed.
            // It suffices to adjust ci.

            /*
            LOG_DEBUG(QString("Overlapping beginning with %1 => %2\n"
                              "adjusting to %3 => %4")
                      .arg(ci->start, 0, 16)
                      .arg(ci->end, 0, 16)
                      .arg(startAngle, 0, 16)
                      .arg(ci->end, 0, 16));*/

            ci->start = startAngle;
            return;
        }

        // Check an overlapping end.
        if(ci->end >= startAngle && ci->end <= endAngle)
        {
            // Now it's possible that the ci->next's beginning overlaps the new
            // range's end. In that case there will be a merger.

            /*
            LOG_DEBUG(QString("Overlapping end with %1 => %2")
                      .arg(ci->start, 0, 16)
                      .arg(ci->end, 0, 16));*/

            crange = ci->next;
            if(!crange)
            {
                ci->end = endAngle;

                /*
                LOG_DEBUG(QString("No next, adjusting end (now %1 => %2)")
                          .arg(ci->start, 0, 16)
                          .arg(ci->end, 0, 16));*/
            }
            else
            {
                if(crange->start <= endAngle)
                {
                    // A fusion will commence. Ci will eat the new range
                    // *and* crange.
                    ci->end = crange->end;

                    /*
                    LOG_DEBUG(QString("merging with the next (%1 => %2)")
                              .arg(crange->start, 0, 16)
                              .arg(crange->end, 0, 16));*/

                    C_RemoveRange(crange);
                }
                else
                {
                    // Not overlapping.
                    ci->end = endAngle;

                    /*
                    LOG_DEBUG(QString("Not merger w/next (now %1 => %2)")
                              .arg(ci->start, 0, 16)
                              .arg(ci->end, 0, 16));*/
                }
            }
            return;
        }
    }

    // Still here? Now we know for sure that the range is disconnected from
    // the others. We still need to find a good place for it. Crange will mark
    // the spot.

    if(!crange)
    {
        // We have a new head node.
        crange = clipHead;
        clipHead = C_NewRange(startAngle, endAngle);
        clipHead->next = crange;
        if(crange)
            crange->prev = clipHead;
    }
    else
    {
        // Add the new range after crange.
        ClipNode *ci = C_NewRange(startAngle, endAngle);
        ci->next = crange->next;
        if(ci->next)
            ci->next->prev = ci;
        ci->prev = crange;
        crange->next = ci;
    }
}

static OccNode *C_NewOcclusionRange(binangle_t stAng, binangle_t endAng,
    Vector3f const &normal, bool topHalf)
{
    OccNode *node = reinterpret_cast<OccNode *>(C_RoverGet(&occNodes));
    if(!node)
    {
        // Allocate a new node.
        node = static_cast<OccNode *>(Z_Malloc(sizeof(OccNode), PU_APPSTATIC, 0));
        C_RoverAdd(&occNodes, reinterpret_cast<RoverNode *>(node));
    }

    node->flags  = (topHalf ? OCNF_TOPHALF : 0);
    node->start  = stAng;
    node->end    = endAng;
    node->normal = normal;

    return node;
}

static void C_RemoveOcclusionRange(OccNode *orange)
{
    // If this is the head, move it to the next one.
    if(occHead == orange)
        occHead = orange->next;

    if(orange->prev)
        orange->prev->next = orange->next;
    if(orange->next)
        orange->next->prev = orange->prev;

    C_RoverRemove(&occNodes, reinterpret_cast<RoverNode*>(orange));
}

/**
 * The given range must be safe.
 */
static void C_AddOcclusionRange(binangle_t start, binangle_t end,
    Vector3f const &normal, bool topHalf)
{
    // Is the range valid?
    if(start > end) return;

    // A new range will be added.
    OccNode *newor = C_NewOcclusionRange(start, end, normal, topHalf);

    // Are there any previous occlusion nodes?
    if(!occHead)
    {
        // No; this is the first.
        occHead = newor;
        occHead->next = occHead->prev = 0;
        return;
    }

    /// @todo Optimize: Remove existing oranges that are fully contained by the
    /// new orange. But how to do the check efficiently?

    // Add the new occlusion range to the appropriate position.
    OccNode *orange = occHead;
    OccNode *last = 0;
    bool done = false;
    while(orange && !done)
    {
        // The list of oranges is sorted by the start angle.
        // Find the first range whose start is greater than the new one.
        if(orange->start > start)
        {
            // Add before this one.
            newor->next = orange;
            newor->prev = orange->prev;
            orange->prev = newor;

            if(newor->prev)
                newor->prev->next = newor;
            else
                occHead = newor; // We have a new head.

            done = true;
        }
        else
        {
            last = orange;
            orange = orange->next;
        }
    }

    if(done) return;

    // All right, add the new range to the end of the list.
    last->next = newor;
    newor->prev = last;
    newor->next = 0;
}

/**
 * Attempts to merge the two given occnodes.
 *
 * @return      0: Could not be merged.
 *              1: orange was merged into other.
 *              2: other was merged into orange.
 */
static int C_TryMergeOccludes(OccNode *orange, OccNode *other)
{
    // We can't test this steep planes.
    if(!orange->normal[VZ]) return 0;

    // Where do they cross?
    Vector3f cross = orange->normal.cross(other->normal);
    if(!cross.x && !cross.y && !cross.z)
    {
        // These two planes are exactly the same! Remove one.
        C_RemoveOcclusionRange(orange);
        return 1;
    }

    // The cross angle must be outside the range.
    binangle_t crossAngle = bamsAtan2(int(cross.y), int(cross.x));
    if(crossAngle >= orange->start && crossAngle <= orange->end)
        return 0; // Inside the range, can't do a thing.

    /// @todo Isn't it possible to consistently determine which direction
    ///        the cross vector is pointing to?
    crossAngle += BANG_180;
    if(crossAngle >= orange->start && crossAngle <= orange->end)
        return 0; // Inside the range, can't do a thing.

    // Now we must determine which plane occludes which.
    // Pick a point in the middle of the range.
    crossAngle = (orange->start + orange->end) >> (1 + BAMS_BITS - 13);
    cross.x = 100 * FIX2FLT(fineCosine[crossAngle]);
    cross.y = 100 * FIX2FLT(finesine[crossAngle]);
    // z = -(A*x+B*y)/C
    cross.z = -(orange->normal.x * cross.x +
                orange->normal.y * cross.y) / orange->normal.z;

    // Is orange occluded by the other one?
    if(cross.dot(other->normal) < 0)
    {
        // No; then the other one is occluded by us. Remove it instead.
        C_RemoveOcclusionRange(other);
        return 2;
    }
    else
    {
        C_RemoveOcclusionRange(orange);
        return 1;
    }
}

/**
 * Try to merge oranges with matching ranges. (Quite a number may be
 * produced as a result of the cuts.)
 */
static void C_MergeOccludes()
{
    OccNode *orange = occHead;
    bool stopScan = false;
    while(!stopScan)
    {
        if(orange && orange->next)
        {
            OccNode *next = orange->next;

            // Find a good one to test with.
            OccNode *other = orange->next;
            bool isDone = false;
            while(!isDone)
            {
                if(other && orange->start == other->start)
                {
                    if(orange->end == other->end &&
                       (other->flags & OCNF_TOPHALF) ==
                       (orange->flags & OCNF_TOPHALF))
                    {
                        // It is a candidate for merging.
                        int result = C_TryMergeOccludes(orange, other);
                        if(result == 2)
                            next = next->next;

                        isDone = true;
                    }
                    else
                    {
                        // Move on to the next candidate.
                        other = other->next;
                    }
                }
                else
                {
                    isDone = true;
                }
            }

            orange = next;
        }
        else
        {
            stopScan = true;
        }
    }
}

static int inline C_OcclusionRelationship(binangle_t start, binangle_t startAngle,
    binangle_t end, binangle_t endAngle)
{
    if(start >= startAngle && end   <= endAngle) return 0;
    if(start >= startAngle && start <  endAngle) return 1;
    if(end    > startAngle && end   <= endAngle) return 2;
    if(start <= startAngle && end   >= endAngle) return 3;
    return -1;
}

/**
 * Everything in the given range is removed from the occlusion nodes.
 */
static void C_CutOcclusionRange(binangle_t startAngle, binangle_t endAngle)
{
    DENG_DEBUG_ONLY(C_OrangeRanger(1));

    // Find the node after which it's OK to add oranges cut in half.
    // (Must preserve the ascending order of the start angles.)
    OccNode *after = 0;
    OccNode *orange = occHead;
    bool isDone = false;
    while(!isDone)
    {
        // We want the orange with the smallest start angle, but one that
        // starts after the cut range has ended.
        if(orange && orange->start < endAngle)
        {
            after = orange;
        }
        else
        {
            isDone = true;
        }

        if(!isDone)
            orange = orange->next;
    }

    OccNode *next = 0;
    orange = occHead;
    isDone = false;
    while(!isDone)
    {
        if(orange)
        {
            // In case orange is removed, take a copy of the next one.
            next = orange->next;

            // Does the cut range include this orange?
            if(startAngle <= orange->end)
            {
                if(orange->start < endAngle)
                {
                    // Four options:
                    switch(C_OcclusionRelationship(orange->start, startAngle,
                                                   orange->end, endAngle))
                    {
                    case 0: // The cut range completely includes this orange.

                        // Fully contained; this orange will be removed.
                        C_RemoveOcclusionRange(orange);
                        break;

                    case 1: // The cut range contains the beginning of the orange.

                        // Cut away the beginning of this orange.
                        orange->start = endAngle;
                        // Even thought the start angle is modified, we don't need to
                        // move this orange anywhere. This is because after the cut there
                        // will be no oranges beginning inside the cut range.
                        break;

                    case 2: // The cut range contains the end of the orange.

                        // Cut away the end of this orange.
                        orange->end = startAngle;
                        break;

                    case 3: { // The orange contains the whole cut range.

                        // The orange gets cut in two parts. Create a new orange that
                        // represents the end, and add it after the 'after' node, or to
                        // the head of the list.
                        OccNode *part = C_NewOcclusionRange(endAngle, orange->end, orange->normal,
                                                            (orange->flags & OCNF_TOPHALF) != 0);
                        part->prev = after;
                        if(after)
                        {
                            part->next = after->next;
                            after->next = part;
                        }
                        else
                        {
                            // Add to the head.
                            part->next = occHead;
                            occHead = part;
                        }

                        if(part->next)
                            part->next->prev = part;

                        // Modify the start part.
                        orange->end = startAngle;
                        break; }

                    default: // No meaningful relationship (in this context).
                        break;
                    }
                }
                else
                {
                    isDone = true; // No more possible cuts.
                }
            }

            if(!isDone)
                orange = next;
        }
        else
        {
            isDone = true;
        }
    }

    DENG_DEBUG_ONLY(C_OrangeRanger(2));

    C_MergeOccludes();

    DENG_DEBUG_ONLY(C_OrangeRanger(6));
}

void C_Init()
{
    C_RoverInit(&clipNodes);
    C_RoverInit(&occNodes);
}

void C_ClearRanges()
{
    clipHead = 0;

    // Rewind the rover.
    C_RoverRewind(&clipNodes);

    occHead = 0;

    // Rewind the rover.
    C_RoverRewind(&occNodes);
}

int C_SafeAddRange(binangle_t startAngle, binangle_t endAngle)
{
    // The range may wrap around.
    if(startAngle > endAngle)
    {
        // The range has to added in two parts.
        C_AddRange(startAngle, BANG_MAX);
        C_AddRange(0, endAngle);
    }
    else
    {
        // Add the range as usual.
        C_AddRange(startAngle, endAngle);
    }
    return true;
}

void C_AddRangeFromViewRelPoints(Vector2d const &from, Vector2d const &to)
{
    Vector2d const eyeOrigin(vOrigin.x, vOrigin.z);
    C_SafeAddRange(pointToAngle(to   - eyeOrigin),
                   pointToAngle(from - eyeOrigin));
}

/**
 * If necessary, cut the given range in two.
 */
static void C_SafeAddOcclusionRange(binangle_t startAngle, binangle_t endAngle,
    Vector3f const &normal, bool tophalf)
{
    // Is this range already clipped?
    if(!C_SafeCheckRange(startAngle, endAngle)) return;

    if(startAngle > endAngle)
    {
        // The range has to be added in two parts.
        C_AddOcclusionRange(startAngle, BANG_MAX, normal, tophalf);

        DENG_DEBUG_ONLY(C_OrangeRanger(3));

        C_AddOcclusionRange(0, endAngle, normal, tophalf);

        DENG_DEBUG_ONLY(C_OrangeRanger(4));
    }
    else
    {
        // Add the range as usual.
        C_AddOcclusionRange(startAngle, endAngle, normal, tophalf);

        DENG_DEBUG_ONLY(C_OrangeRanger(5));
    }
}

void C_AddViewRelOcclusion(Vector2d const &from, Vector2d const &to, coord_t height, bool topHalf)
{
    /// @todo Optimize:: Check if the given line is already occluded?

    // Calculate the occlusion plane normal.
    // We'll use the game's coordinate system (left-handed, but Y and Z are swapped).
    Vector3d const eyeOrigin = vOrigin.xzy();
    Vector3d eyeToV1 = Vector3d(from, height) - eyeOrigin;
    Vector3d eyeToV2 = Vector3d(to,   height) - eyeOrigin;

    binangle_t startAngle = pointToAngle(eyeToV2);
    binangle_t endAngle   = pointToAngle(eyeToV1);

    // Do not attempt to occlude with a zero-length range.
    if(startAngle == endAngle)
        return;

    // The normal points to the half we want to occlude.
    Vector3f normal = (topHalf? eyeToV2 : eyeToV1).cross(topHalf? eyeToV1 : eyeToV2);

#ifdef DENG_DEBUG
    Vector3f testPos(0, 0, (topHalf ? 1000 : -1000));
    if(bool Failed_C_AddViewRelOcclusion_SideTest = testPos.dot(normal) < 0)
    {
        // Uh-oh.
        LOG_AS("C_AddViewRelOcclusion");
        LOGDEV_GL_WARNING("Wrong side v1:%s v2:%s eyeOrigin:%s!")
                << from.asText() << to.asText()
                << Vector2d(eyeOrigin).asText();
        DENG_ASSERT(!Failed_C_AddViewRelOcclusion_SideTest);
    }
#endif

    // Try to add this range.
    C_SafeAddOcclusionRange(startAngle, endAngle, normal, topHalf);
}

/**
 * @return  Non-zero if the view relative point is occluded by an occlusion range.
 */
static int C_IsPointOccluded(Vector3d const &viewRelPoint)
{
    binangle_t angle = pointToAngle(viewRelPoint);

    for(OccNode *orange = occHead; orange; orange = orange->next)
    {
        if(angle >= orange->start && angle <= orange->end)
        {
            if(orange->start > angle)
                return false; // No more possibilities.

            // On which side of the occlusion plane is it?
            // The positive side is the occluded one.
            if(viewRelPoint.dot(orange->normal) > 0)
                return true;
        }
    }

    // No orange occluded the point.
    return false;
}

int C_IsPointVisible(Vector3d const &point)
{
    if(devNoCulling) return true;

    Vector3d const eyeOrigin = vOrigin.xzy();
    Vector3d const viewRelPoint = point - eyeOrigin;

    binangle_t const angle = pointToAngle(viewRelPoint);
    if(!C_IsAngleVisible(angle))
        return false;

    // Not clipped by the clipnodes. Perhaps it's occluded by an orange.
    return !C_IsPointOccluded(viewRelPoint);
}

/**
 * The specified range must be safe!
 */
static int C_IsRangeVisible(binangle_t startAngle, binangle_t endAngle)
{
    for(ClipNode *ci = clipHead; ci; ci = ci->next)
    {
        if(startAngle >= ci->start && endAngle <= ci->end)
            return false;
    }
    // No node fully contained the specified range.
    return true;
}

/**
 * @return  Non-zero iff the range is not entirely clipped, else @c 0.
 */
static int C_SafeCheckRange(binangle_t startAngle, binangle_t endAngle)
{
    if(startAngle > endAngle)
    {
        // The range wraps around.
        return (C_IsRangeVisible(startAngle, BANG_MAX) ||
                C_IsRangeVisible(0, endAngle));
    }
    return C_IsRangeVisible(startAngle, endAngle);
}

int C_CheckRangeFromViewRelPoints(Vector2d const &from, Vector2d const &to)
{
    if(devNoCulling) return true;

    Vector2d const eyeOrigin(vOrigin.x, vOrigin.z);
    return C_SafeCheckRange(pointToAngle(to   - eyeOrigin) - BANG_45/90,
                            pointToAngle(from - eyeOrigin) + BANG_45/90);
}

int C_IsAngleVisible(binangle_t bang)
{
    if(devNoCulling) return true;

    for(ClipNode *ci = clipHead; ci; ci = ci->next)
    {
        if(bang > ci->start && bang < ci->end)
            return false;
    }
    // No one clipped this angle.
    return true;
}

int C_IsPolyVisible(Face const &poly)
{
    if(!poly.hedgeCount())
        return false;

    if(devNoCulling)
        return true;

    // Do we need to resize the angle list buffer?
    if(poly.hedgeCount() > anglistSize)
    {
        anglistSize *= 2;
        if(!anglistSize)
            anglistSize = 64;
        anglist = static_cast<binangle_t *>(Z_Realloc(anglist, sizeof(*anglist) * anglistSize, PU_APPSTATIC));
    }

    // Find angles to all corners.
    Vector2d const eyeOrigin(vOrigin.x, vOrigin.z);
    int n = 0;
    HEdge const *hedge = poly.hedge();
    do
    {
        anglist[n++] = pointToAngle(hedge->origin() - eyeOrigin);

    } while((hedge = &hedge->next()) != poly.hedge());

    // Check each of the ranges defined by the edges.
    for(int i = 0; i < poly.hedgeCount() - 1; ++i)
    {
        // The last edge won't be checked. This is because the edges
        // define a closed, convex polygon and the last edge's range is
        // always already covered by the previous edges.
        int end = i + 1;

        // If even one of the edges is not contained by a clipnode,
        // the leaf is at least partially visible.
        binangle_t angLen = anglist[end] - anglist[i];

        // The viewer is on an edge, the leaf should be visible.
        if(angLen == BANG_180) return true;

        // Choose the start and end points so that length is < 180.
        if(angLen < BANG_180)
        {
            if(C_SafeCheckRange(anglist[i], anglist[end]))
                return true;
        }
        else
        {
            if(C_SafeCheckRange(anglist[end], anglist[i]))
                return true;
        }
    }

    // All clipped away, the leaf cannot be seen.
    return false;
}

int C_IsFull()
{
    if(devNoCulling) return false;

    return clipHead && clipHead->start == 0 && clipHead->end == BANG_MAX;
}

#ifdef DENG_DEBUG
static void C_OrangeRanger(int mark)
{
    for(OccNode *orange = occHead; orange; orange = orange->next)
    {
        if(orange->prev && orange->prev->start > orange->start)
        {
            C_OcclusionLister();
            Con_Error("C_OrangeRanger(%i): Orange order has failed.\n", mark);
        }
    }
}

static void C_OcclusionLister()
{
    LOG_AS("Clipper");
    for(OccNode *orange = occHead; orange; orange = orange->next)
    {
        LOG_MSG(QString("%1 => %2 (%i)")
                .arg(orange->start, 0, 16)
                .arg(orange->end, 0, 16)
                .arg((orange->flags & OCNF_TOPHALF) != 0));
    }
}

void C_Ranger()
{
    for(ClipNode *ci = clipHead; ci; ci = ci->next)
    {
        if(ci == clipHead)
        {
            if(ci->prev != 0)
                Con_Error("Cliphead->prev != 0.\n");
        }

        // Confirm that the links to prev and next are OK.
        if(ci->prev)
        {
            if(ci->prev->next != ci)
                Con_Error("Prev->next != this\n");
        }
        else if(ci != clipHead)
        {
            Con_Error("prev == null, this isn't clipHead.\n");
        }

        if(ci->next)
        {
            if(ci->next->prev != ci)
                Con_Error("Next->prev != this\n");
        }
    }
}
#endif
