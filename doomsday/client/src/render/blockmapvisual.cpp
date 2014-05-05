/** @file blockmapvisual.cpp  Graphical Blockmap Visual.
 * @ingroup world
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

#include <de/aabox.h>
#include <de/concurrency.h>

#include <de/Vector>

#include "de_base.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_ui.h"

#include "Face"
#include "HEdge"

#include "gl/gl_texmanager.h"

#include "world/blockmap.h"
#include "world/lineblockmap.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "ConvexSubspace"

#include "render/blockmapvisual.h"

using namespace de;

byte bmapShowDebug; // 1 = mobjs, 2 = lines, 3 = BSP leafs, 4 = polyobjs. cvar
float bmapDebugSize = 1.5f; // cvar

static void drawMobj(mobj_t const &mobj)
{
    AABoxd const bounds = Mobj_AABox(mobj);

    glVertex2f(bounds.minX, bounds.minY);
    glVertex2f(bounds.maxX, bounds.minY);
    glVertex2f(bounds.maxX, bounds.maxY);
    glVertex2f(bounds.minX, bounds.maxY);
}

static int drawMobjWorker(void *mobjPtr, void * /*context*/)
{
    mobj_t &mobj = *static_cast<mobj_t *>(mobjPtr);
    if(mobj.validCount != validCount)
    {
        mobj.validCount = validCount;
        drawMobj(mobj);
    }
    return false; // Continue iteration.
}

static void drawLine(Line const &line)
{
    glVertex2f(line.fromOrigin().x, line.fromOrigin().y);
    glVertex2f(  line.toOrigin().x,   line.toOrigin().y);
}

static int drawLineWorker(void *linePtr, void * /*context*/)
{
    Line &line = *static_cast<Line *>(linePtr);
    if(line.validCount() != validCount)
    {
        line.setValidCount(validCount);
        drawLine(line);
    }
    return false; // Continue iteration.
}

static void drawSubspace(ConvexSubspace const &subspace)
{
    float const scale = de::max(bmapDebugSize, 1.f);
    float const width = (DENG_GAMEVIEW_WIDTH / 16) / scale;

    Face const &poly = subspace.poly();
    HEdge *base = poly.hedge();
    HEdge *hedge = base;
    do
    {
        Vector2d start = hedge->origin();
        Vector2d end   = hedge->twin().origin();

        glBegin(GL_LINES);
            glVertex2f(start.x, start.y);
            glVertex2f(end.x, end.y);
        glEnd();

        ddouble length = (end - start).length();
        if(length > 0)
        {
            Vector2d const unit = (end - start) / length;
            Vector2d const normal(-unit.y, unit.x);

            GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC));
            glEnable(GL_TEXTURE_2D);
            GL_BlendMode(BM_ADD);

            glBegin(GL_QUADS);
                glTexCoord2f(0.75f, 0.5f);
                glVertex2f(start.x, start.y);
                glTexCoord2f(0.75f, 0.5f);
                glVertex2f(end.x, end.y);
                glTexCoord2f(0.75f, 1);
                glVertex2f(end.x - normal.x * width, end.y - normal.y * width);
                glTexCoord2f(0.75f, 1);
                glVertex2f(start.x - normal.x * width, start.y - normal.y * width);
            glEnd();

            glDisable(GL_TEXTURE_2D);
            GL_BlendMode(BM_NORMAL);
        }

        // Draw a bounding box for the leaf's face geometry.
        start = Vector2d(poly.aaBox().minX, poly.aaBox().minY);
        end   = Vector2d(poly.aaBox().maxX, poly.aaBox().maxY);

        glBegin(GL_LINES);
            glVertex2f(start.x, start.y);
            glVertex2f(  end.x, start.y);
            glVertex2f(  end.x, start.y);
            glVertex2f(  end.x,   end.y);
            glVertex2f(  end.x,   end.y);
            glVertex2f(start.x,   end.y);
            glVertex2f(start.x,   end.y);
            glVertex2f(start.x, start.y);
        glEnd();

    } while((hedge = &hedge->next()) != base);
}

static int drawSubspaceWorker(void *subspacePtr, void * /*context*/)
{
    ConvexSubspace &subspace = *static_cast<ConvexSubspace *>(subspacePtr);
    if(subspace.validCount() != validCount)
    {
        subspace.setValidCount(validCount);
        drawSubspace(subspace);
    }
    return false; // Continue iteration.
}

static int drawCellLines(Blockmap const &bmap, BlockmapCell const &cell, void *context)
{
    glBegin(GL_LINES);
        bmap.iterate(cell, (int (*)(void*,void*)) drawLineWorker, context);
    glEnd();
    return false; // Continue iteration.
}

static int drawCellPolyobjLineWorker(void *object, void *context)
{
    Polyobj *po = (Polyobj *)object;
    foreach(Line *line, po->lines())
    {
        if(int result = drawLineWorker(line, context))
            return result;
    }
    return false; // Continue iteration.
}

static int drawCellPolyobjs(Blockmap const &bmap, BlockmapCell const &cell, void *context)
{
    glBegin(GL_LINES);
        bmap.iterate(cell, (int (*)(void*,void*)) drawCellPolyobjLineWorker, context);
    glEnd();
    return false; // Continue iteration.
}

static int drawCellMobjs(Blockmap const &bmap, BlockmapCell const &cell, void *context)
{
    glBegin(GL_QUADS);
        bmap.iterate(cell, (int (*)(void*,void*)) drawMobjWorker, context);
    glEnd();
    return false; // Continue iteration.
}

static int drawCellSubspaces(Blockmap const &bmap, BlockmapCell const &cell, void *context)
{
    bmap.iterate(cell, (int (*)(void*,void*)) drawSubspaceWorker, context);
    return false; // Continue iteration.
}

static void drawBackground(Blockmap const &bmap)
{
    BlockmapCell const &dimensions = bmap.dimensions();

    // Scale modelview matrix so we can express cell geometry
    // using a cell-sized unit coordinate space.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(bmap.cellSize(), bmap.cellSize(), 1);

    /*
     * Draw the translucent quad which represents the "used" cells.
     */
    glColor4f(.25f, .25f, .25f, .66f);
    glBegin(GL_QUADS);
        glVertex2f(0,            0);
        glVertex2f(dimensions.x, 0);
        glVertex2f(dimensions.x, dimensions.y);
        glVertex2f(0,            dimensions.y);
    glEnd();

    /*
     * Draw the "null cells" over the top.
     */
    glColor4f(0, 0, 0, .95f);
    BlockmapCell cell;
    for(cell.y = 0; cell.y < dimensions.y; ++cell.y)
    for(cell.x = 0; cell.x < dimensions.x; ++cell.x)
    {
        if(bmap.cellElementCount(cell)) continue;

        glBegin(GL_QUADS);
            glVertex2f(cell.x,     cell.y);
            glVertex2f(cell.x + 1, cell.y);
            glVertex2f(cell.x + 1, cell.y + 1);
            glVertex2f(cell.x,     cell.y + 1);
        glEnd();
    }

    // Restore previous GL state.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

static void drawCellInfo(Vector2d const &origin_, char const *info)
{
    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size(16 + FR_TextWidth(info),
                  16 + FR_SingleLineHeight(info));

    Point2Raw origin(origin_.x, origin_.y);
    origin.x -= size.width / 2;
    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += size.height / 2;
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(info, &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawBlockmapInfo(Vector2d const &origin_, Blockmap const &blockmap)
{
    glEnable(GL_TEXTURE_2D);

    Point2Raw origin(origin_.x, origin_.y);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size;
    size.width = 16 + FR_TextWidth("(+000.0, +000.0) (+000.0, +000.0)");
    int th = FR_SingleLineHeight("Info");
    size.height = th * 4 + 16;

    origin.x -= size.width;
    origin.y -= size.height;
    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += 8 + th/2;

    UI_TextOutEx2("Blockmap", &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    Vector2ui const &bmapDimensions = blockmap.dimensions();
    char buf[80];
    dd_snprintf(buf, 80, "Dimensions:(%u, %u) #%li", bmapDimensions.x, bmapDimensions.y,
                                                    (long) bmapDimensions.y * bmapDimensions.x);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "Cell dimensions:(%.3f, %.3f)", blockmap.cellSize(), blockmap.cellSize());
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "(%+06.0f, %+06.0f) (%+06.0f, %+06.0f)",
                         blockmap.bounds().minX, blockmap.bounds().minY,
                         blockmap.bounds().maxX, blockmap.bounds().maxY);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawCellInfoBox(Vector2d const &origin, Blockmap const &blockmap,
    char const *objectTypeName, BlockmapCell const &cell)
{
    uint count = blockmap.cellElementCount(cell);
    char info[160]; dd_snprintf(info, 160, "Cell:(%u, %u) %s:#%u", cell.x, cell.y, objectTypeName, count);
    drawCellInfo(origin, info);
}

/**
 * @param bmap        Blockmap to be rendered.
 * @param followMobj  Mobj to center/focus the visual on. Can be @c 0.
 * @param cellDrawer  Blockmap cell content drawing callback. Can be @c 0.
 */
static void drawBlockmap(Blockmap const &bmap, mobj_t *followMobj,
    int (*cellDrawer) (Blockmap const &, BlockmapCell const &, void *))
{
    BlockmapCellBlock vCellBlock;
    BlockmapCell vCell;

    BlockmapCell const &dimensions = bmap.dimensions();
    Vector2d const cellDimensions = bmap.cellDimensions();

    if(followMobj)
    {
        // Determine the followed Mobj's blockmap coords.
        bool didClip;
        vCell = bmap.toCell(followMobj->origin, &didClip);

        if(didClip)
            followMobj = 0; // Outside the blockmap.

        if(followMobj)
        {
            // Determine the extended blockmap coords for the followed
            // Mobj's "touch" range.
            AABoxd aaBox = Mobj_AABox(*followMobj);

            aaBox.minX -= DDMOBJ_RADIUS_MAX * 2;
            aaBox.minY -= DDMOBJ_RADIUS_MAX * 2;
            aaBox.maxX += DDMOBJ_RADIUS_MAX * 2;
            aaBox.maxY += DDMOBJ_RADIUS_MAX * 2;

            vCellBlock = bmap.toCellBlock(aaBox);
        }
    }

    if(followMobj)
    {
        // Orient on the center of the followed Mobj.
        glTranslated(-(vCell.x * cellDimensions.x), -(vCell.y * cellDimensions.y), 0);
    }
    else
    {
        // Orient on center of the Blockmap.
        glTranslated(-(cellDimensions.x * dimensions.x)/2,
                     -(cellDimensions.y * dimensions.y)/2, 0);
    }

    // First we'll draw a background showing the "null" cells.
    drawBackground(bmap);
    if(followMobj)
    {
        // Highlight cells the followed Mobj "touches".
        glBegin(GL_QUADS);

        BlockmapCell cell;
        for(cell.y = vCellBlock.min.y; cell.y < vCellBlock.max.y; ++cell.y)
        for(cell.x = vCellBlock.min.x; cell.x < vCellBlock.max.x; ++cell.x)
        {
            if(cell == vCell)
            {
                // The cell the followed Mobj is actually in.
                glColor4f(.66f, .66f, 1, .66f);
            }
            else
            {
                // A cell within the followed Mobj's extended collision range.
                glColor4f(.33f, .33f, .66f, .33f);
            }

            Vector2d const start = cellDimensions * cell;
            Vector2d const end   = start + cellDimensions;

            glVertex2d(start.x, start.y);
            glVertex2d(  end.x, start.y);
            glVertex2d(  end.x,   end.y);
            glVertex2d(start.x,   end.y);
        }

        glEnd();
    }

    /**
     * Draw the Gridmap visual.
     * @note Gridmap uses a cell unit size of [width:1,height:1], so we need to
     *       scale it up so it aligns correctly.
     */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScaled(cellDimensions.x, cellDimensions.y, 1);

    bmap.drawDebugVisual();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    /*
     * Draw the blockmap-linked data.
     * Translate the modelview matrix so that objects can be drawn using
     * the map space coordinates directly.
     */
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslated(-bmap.origin().x, -bmap.origin().y, 0);

    if(cellDrawer)
    {
        if(followMobj)
        {
            /*
             * Draw cell contents color coded according to their range from the
             * followed Mobj.
             */

            // First, the cells outside the "touch" range (crimson).
            validCount++;
            glColor4f(.33f, 0, 0, .75f);
            BlockmapCell cell;
            for(cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for(cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if(cell.x >= vCellBlock.min.x && cell.x < vCellBlock.max.x &&
                   cell.y >= vCellBlock.min.y && cell.y < vCellBlock.max.y)
                {
                    continue;
                }
                if(!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Next, the cells within the "touch" range (orange).
            validCount++;
            glColor3f(1, .5f, 0);
            for(cell.y = vCellBlock.min.y; cell.y < vCellBlock.max.y; ++cell.y)
            for(cell.x = vCellBlock.min.x; cell.x < vCellBlock.max.x; ++cell.x)
            {
                if(cell == vCell) continue;
                if(!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Lastly, the cell the followed Mobj is in (yellow).
            validCount++;
            glColor3f(1, 1, 0);
            if(bmap.cellElementCount(vCell))
            {
                cellDrawer(bmap, vCell, 0/*no params*/);
            }
        }
        else
        {
            // Draw all cells without color coding.
            validCount++;
            glColor4f(.33f, 0, 0, .75f);
            BlockmapCell cell;
            for(cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for(cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if(!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }
        }
    }

    /*
     * Draw the followed mobj, if any.
     */
    if(followMobj)
    {
        validCount++;
        glColor3f(0, 1, 0);
        glBegin(GL_QUADS);
            drawMobj(*followMobj);
        glEnd();
    }

    // Undo the map coordinate space translation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Rend_BlockmapDebug()
{
    // Disabled?
    if(!bmapShowDebug || bmapShowDebug > 4) return;

    if(!App_WorldSystem().hasMap()) return;
    Map &map = App_WorldSystem().map();

    Blockmap const *blockmap;
    int (*cellDrawer) (Blockmap const &, BlockmapCell const &, void *);
    char const *objectTypeName;
    switch(bmapShowDebug)
    {
    default: // Mobjs.
        blockmap = &map.mobjBlockmap();
        cellDrawer = drawCellMobjs;
        objectTypeName = "Mobjs";
        break;

    case 2: // Lines.
        blockmap = &map.lineBlockmap();
        cellDrawer = drawCellLines;
        objectTypeName = "Lines";
        break;

    case 3: // BSP leafs.
        blockmap = &map.subspaceBlockmap();
        cellDrawer = drawCellSubspaces;
        objectTypeName = "Subspaces";
        break;

    case 4: // Polyobjs.
        blockmap = &map.polyobjBlockmap();
        cellDrawer = drawCellPolyobjs;
        objectTypeName = "Polyobjs";
        break;
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    /*
     * Draw the blockmap.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);
    // Orient on the center of the window.
    glTranslatef((DENG_GAMEVIEW_WIDTH / 2), (DENG_GAMEVIEW_HEIGHT / 2), 0);

    // Uniform scaling factor for this visual.
    float scale = bmapDebugSize / de::max(DENG_GAMEVIEW_HEIGHT / 100, 1);
    glScalef(scale, -scale, 1);

    // If possible we'll tailor what we draw relative to the viewPlayer.
    mobj_t *followMobj = 0;
    if(viewPlayer && viewPlayer->shared.mo)
        followMobj = viewPlayer->shared.mo;

    // Draw!
    drawBlockmap(*blockmap, followMobj, cellDrawer);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    /*
     * Draw HUD info.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    if(followMobj)
    {
        // About the cell the followed Mobj is in.
        bool didClip;
        BlockmapCell cell = blockmap->toCell(followMobj->origin, &didClip);
        if(!didClip)
        {
            drawCellInfoBox(Vector2d(DENG_GAMEVIEW_WIDTH / 2, 30), *blockmap,
                            objectTypeName, cell);
        }
    }

    // About the Blockmap itself.
    drawBlockmapInfo(Vector2d(DENG_GAMEVIEW_WIDTH - 10, DENG_GAMEVIEW_HEIGHT - 10),
                     *blockmap);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
