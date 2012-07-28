/**
 * @file id1map_analyze.cpp @ingroup wadmapconverter
 *
 * id tech 1 post load map analyses.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "wadmapconverter.h"
#include <de/Log>
#include <list>

#define mapFormat               DENG_PLUGIN_GLOBAL(mapFormat)
#define map                     DENG_PLUGIN_GLOBAL(map)

static uint validCount = 0; // Used for Polyobj LineDef collection.

typedef std::list<uint> LineList;

/**
 * Create a temporary polyobj (read from the original map data).
 */
static mpolyobj_t* createPolyobj(LineList& lineList, int tag, int sequenceType,
    int16_t anchorX, int16_t anchorY)
{
    // Allocate the new polyobj.
    map->polyobjs.push_back(mpolyobj_t());
    mpolyobj_t* po = &map->polyobjs.back();

    po->idx = map->polyobjs.size()-1;
    po->tag = tag;
    po->seqType = sequenceType;
    po->anchor[VX] = anchorX;
    po->anchor[VY] = anchorY;

    po->lineCount = lineList.size();
    po->lineIndices = (uint*)malloc(sizeof(uint) * po->lineCount);
    uint n = 0;
    for(LineList::iterator i = lineList.begin(); i != lineList.end(); ++i, ++n)
    {
        uint lineIdx = *i;
        mline_t* line = &map->lines[lineIdx];

        line->aFlags |= LAF_POLYOBJ;
        /**
         * Due a logic error in hexen.exe, when the column drawer is
         * presented with polyobj segs built from two-sided linedefs;
         * clipping is always calculated using the pegging logic for
         * single-sided linedefs.
         *
         * Here we emulate this behavior by automatically applying
         * bottom unpegging for two-sided linedefs.
         */
        if(line->sides[LEFT] != 0)
            line->ddFlags |= DDLF_DONTPEGBOTTOM;

        po->lineIndices[n] = lineIdx + 1; // 1-based indices.
    }

    return po;
}

/**
 * @param lineList      @c NULL, will cause IterFindPolyLines to count
 *                      the number of lines in the polyobj.
 */
static void iterFindPolyLines(LineList& lineList, coord_t x, coord_t y)
{
    DENG2_FOR_EACH(i, map->lines, Lines::iterator)
    {
        if((i)->aFlags & LAF_POLYOBJ) continue;
        if((i)->validCount == validCount) continue;

        coord_t v1[2];
        v1[VX] = map->vertexes[( (i)->v[0] - 1 ) * 2];
        v1[VY] = map->vertexes[( (i)->v[0] - 1 ) * 2 + 1];

        coord_t v2[2];
        v2[VX] = map->vertexes[( (i)->v[1] - 1 ) * 2];
        v2[VY] = map->vertexes[( (i)->v[1] - 1 ) * 2 + 1];

        if(FEQUAL(v1[VX], x) && FEQUAL(v1[VY], y))
        {
            (i)->validCount = validCount;
            lineList.push_back( i - map->lines.begin() );
            iterFindPolyLines(lineList, v2[VX], v2[VY]);
        }
    }
}

/**
 * @todo This terribly inefficent (naive) algorithm may need replacing
 *       (it is far outside an acceptable polynomial range!).
 */
static void collectPolyobjLines(LineList& lineList, Lines::iterator lineIt)
{
    mline_t* line = &*lineIt;
    line->xType = 0;
    line->xArgs[0] = 0;

    coord_t v1[2];
    v1[VX] = map->vertexes[(line->v[0]-1) * 2];
    v1[VY] = map->vertexes[(line->v[0]-1) * 2 + 1];

    coord_t v2[2];
    v2[VX] = map->vertexes[(line->v[1]-1) * 2];
    v2[VY] = map->vertexes[(line->v[1]-1) * 2 + 1];

    validCount++;
    // Insert the first line.
    lineList.push_back(lineIt - map->lines.begin());
    line->validCount = validCount;
    iterFindPolyLines(lineList, v2[VX], v2[VY]);
}

/**
 * Find all linedefs marked as belonging to a polyobject with the given tag
 * and attempt to create a polyobject from them.
 *
 * @param tag           Line tag of linedefs to search for.
 *
 * @return @c true = successfully created polyobj.
 */
static bool findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY)
{
    LineList polyLines;

    // First look for a PO_LINE_START linedef with this tag.
    DENG2_FOR_EACH(i, map->lines, Lines::iterator)
    {
        if((i)->aFlags & LAF_POLYOBJ) continue;
        if(!((i)->xType == PO_LINE_START && (i)->xArgs[0] == tag)) continue;

        collectPolyobjLines(polyLines, i);
        if(!polyLines.empty())
        {
            int8_t sequenceType = (i)->xArgs[2];
            if(sequenceType >= SEQTYPE_NUMSEQ) sequenceType = 0;

            createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
            return true;
        }
        return false;
    }

    /**
     * Didn't find a polyobj through PO_LINE_START.
     * We'll try another approach...
     */
    for(uint n = 0; ; ++n)
    {
        bool foundAnotherLine = false;

        DENG2_FOR_EACH(i, map->lines, Lines::iterator)
        {
            if((i)->aFlags & LAF_POLYOBJ) continue;

            if((i)->xType == PO_LINE_EXPLICIT && (i)->xArgs[0] == tag)
            {
                if(!(i)->xArgs[1])
                {
                    LOG_WARNING("Linedef missing (probably #%d) in explicit polyobj (tag:%d).") << n + 1 << tag;
                    return false;
                }

                if((i)->xArgs[1] == n+1)
                {
                    // Add this line to the list.
                    polyLines.push_back( i - map->lines.begin() );
                    foundAnotherLine = true;

                    // Clear any special.
                    (i)->xType = 0;
                    (i)->xArgs[0] = 0;
                }
            }
        }

        if(foundAnotherLine)
        {
            // Check if an explicit line order has been skipped.
            // A line has been skipped if there are any more explicit lines with
            // the current tag value.
            DENG2_FOR_EACH(i, map->lines, Lines::iterator)
            {
                if((i)->xType == PO_LINE_EXPLICIT && (i)->xArgs[0] == tag)
                {
                    LOG_WARNING("Linedef missing (#%d) in explicit polyobj (tag:%d).") << n << tag;
                    return false;
                }
            }
        }
    }

    if(!polyLines.empty())
    {
        mline_t* line = &map->lines[ polyLines.front() ];
        const int8_t sequenceType = line->xArgs[3];

        // Setup the mirror if it exists.
        line->xArgs[1] = line->xArgs[2];

        createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
        return true;
    }

    return false;
}

static void findPolyobjs(void)
{
    LOG_AS("WadMapConverter");
    LOG_TRACE("Locating polyobjs...");

    DENG2_FOR_EACH(i, map->things, Things::iterator)
    {
        // A polyobj anchor?
        if((i)->doomEdNum == PO_ANCHOR_DOOMEDNUM)
        {
            const int tag = (i)->angle;
            findAndCreatePolyobj(tag, (i)->origin[VX], (i)->origin[VY]);
        }
    }
}

void AnalyzeMap(void)
{
    if(mapFormat == MF_HEXEN)
    {
        findPolyobjs();
    }
}
