/** @file blockmapvisual.cpp  Graphical Blockmap Visual.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "render/blockmapvisual.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"

#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/convexsubspace.h"
#include "api_fontrender.h"
#include "render/rend_font.h"
#include "ui/ui_main.h"

#include <doomsday/world/blockmap.h>
#include <doomsday/world/lineblockmap.h>
#include <doomsday/mesh/face.h>
#include <doomsday/mesh/hedge.h>
#include <de/legacy/aabox.h>
#include <de/legacy/concurrency.h>
#include <de/glinfo.h>
#include <de/vector.h>

using namespace de;
using world::World;

byte  bmapShowDebug; // 1 = mobjs, 2 = lines, 3 = BSP leafs, 4 = polyobjs. cvar
float bmapDebugSize = 1.5f; // cvar

static void drawMobj(const mobj_t &mob)
{
    const AABoxd bounds = Mobj_Bounds(mob);

    DGL_Vertex2f(bounds.minX, bounds.minY);
    DGL_Vertex2f(bounds.maxX, bounds.minY);
    DGL_Vertex2f(bounds.maxX, bounds.maxY);
    DGL_Vertex2f(bounds.minX, bounds.maxY);
}

static void drawLine(const world::Line &line)
{
    DGL_Vertex2f(line.from().x(), line.from().y());
    DGL_Vertex2f(line.to  ().x(), line.to  ().y());
}

static void drawSubspace(const ConvexSubspace &subspace)
{
    const dfloat scale = de::max(bmapDebugSize, 1.f);
    const dfloat width = (DE_GAMEVIEW_WIDTH / 16) / scale;

    const mesh::Face &poly = subspace.poly();
    mesh::HEdge *base = poly.hedge();
    mesh::HEdge *hedge = base;
    do
    {
        Vec2d start = hedge->origin();
        Vec2d end   = hedge->twin().origin();

        DGL_Begin(DGL_LINES);
            DGL_Vertex2f(start.x, start.y);
            DGL_Vertex2f(end.x, end.y);
        DGL_End();

        ddouble length = (end - start).length();
        if (length > 0)
        {
            const Vec2d unit = (end - start) / length;
            Vec2d const normal(-unit.y, unit.x);

            GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC));
            DGL_Enable(DGL_TEXTURE_2D);
            GL_BlendMode(BM_ADD);

            DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0.75f, 0.5f);
                DGL_Vertex2f(start.x, start.y);
                DGL_TexCoord2f(0, 0.75f, 0.5f);
                DGL_Vertex2f(end.x, end.y);
                DGL_TexCoord2f(0, 0.75f, 1);
                DGL_Vertex2f(end.x - normal.x * width, end.y - normal.y * width);
                DGL_TexCoord2f(0, 0.75f, 1);
                DGL_Vertex2f(start.x - normal.x * width, start.y - normal.y * width);
            DGL_End();

            DGL_Disable(DGL_TEXTURE_2D);
            GL_BlendMode(BM_NORMAL);
        }

        // Draw a bounding box for the leaf's face geometry.
        start = Vec2d(poly.bounds().minX, poly.bounds().minY);
        end   = Vec2d(poly.bounds().maxX, poly.bounds().maxY);

        DGL_Begin(DGL_LINES);
            DGL_Vertex2f(start.x, start.y);
            DGL_Vertex2f(  end.x, start.y);
            DGL_Vertex2f(  end.x, start.y);
            DGL_Vertex2f(  end.x,   end.y);
            DGL_Vertex2f(  end.x,   end.y);
            DGL_Vertex2f(start.x,   end.y);
            DGL_Vertex2f(start.x,   end.y);
            DGL_Vertex2f(start.x, start.y);
        DGL_End();

    } while ((hedge = &hedge->next()) != base);
}

static int drawCellLines(const world::Blockmap &bmap, const world::BlockmapCell &cell, void *)
{
    DGL_Begin(DGL_LINES);
        bmap.forAllInCell(cell, [] (void *object)
        {
            Line &line = *(Line *)object;
            if (line.validCount() != World::validCount)
            {
                line.setValidCount(World::validCount);
                drawLine(line);
            }
            return LoopContinue;
        });
    DGL_End();
    return false; // Continue iteration.
}

static int drawCellPolyobjs(const world::Blockmap &bmap, const world::BlockmapCell &cell, void * /*context*/)
{
    DGL_Begin(DGL_LINES);
        bmap.forAllInCell(cell, [] (void *object)
        {
            Polyobj &pob = *(Polyobj *)object;
            for (auto *line : pob.lines())
            {
                if (line->validCount() != World::validCount)
                {
                    line->setValidCount(World::validCount);
                    drawLine(*line);
                }
            }
            return LoopContinue;
        });
    DGL_End();
    return false; // Continue iteration.
}

static int drawCellMobjs(const world::Blockmap &bmap, const world::BlockmapCell &cell, void *)
{
    DGL_Begin(DGL_QUADS);
        bmap.forAllInCell(cell, [] (void *object)
        {
            mobj_t &mob = *(mobj_t *)object;
            if (mob.validCount != World::validCount)
            {
                mob.validCount = World::validCount;
                drawMobj(mob);
            }
            return LoopContinue;
        });
    DGL_End();
    return false; // Continue iteration.
}

static int drawCellSubspaces(const world::Blockmap &bmap, const world::BlockmapCell &cell, void *)
{
    bmap.forAllInCell(cell, [] (void *object)
    {
        ConvexSubspace *sub = (ConvexSubspace *)object;
        if (sub->validCount() != World::validCount)
        {
            sub->setValidCount(World::validCount);
            drawSubspace(*sub);
        }
        return LoopContinue;
    });
    return false; // Continue iteration.
}

static void drawBackground(const world::Blockmap &bmap)
{
    const world::BlockmapCell &dimensions = bmap.dimensions();

    // Scale modelview matrix so we can express cell geometry
    // using a cell-sized unit coordinate space.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(bmap.cellSize(), bmap.cellSize(), 1);

    /*
     * Draw the translucent quad which represents the "used" cells.
     */
    DGL_Color4f(.25f, .25f, .25f, .66f);
    DGL_Begin(DGL_QUADS);
        DGL_Vertex2f(0,            0);
        DGL_Vertex2f(dimensions.x, 0);
        DGL_Vertex2f(dimensions.x, dimensions.y);
        DGL_Vertex2f(0,            dimensions.y);
    DGL_End();

    /*
     * Draw the "null cells" over the top.
     */
    DGL_Color4f(0, 0, 0, .95f);
    world::BlockmapCell cell;
    for (cell.y = 0; cell.y < dimensions.y; ++cell.y)
    {
        for (cell.x = 0; cell.x < dimensions.x; ++cell.x)
        {
            if (bmap.cellElementCount(cell)) continue;

            DGL_Begin(DGL_QUADS);
                DGL_Vertex2f(cell.x,     cell.y);
                DGL_Vertex2f(cell.x + 1, cell.y);
                DGL_Vertex2f(cell.x + 1, cell.y + 1);
                DGL_Vertex2f(cell.x,     cell.y + 1);
            DGL_End();
        }
    }

    // Restore previous GL state.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

static void drawCellInfo(const Vec2d &origin_, const char *info)
{
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size = {{{16 + FR_TextWidth(info),
                       16 + FR_SingleLineHeight(info)}}};

    Point2Raw origin = {{{int(origin_.x), int(origin_.y)}}};
    origin.x -= size.width / 2;
    //UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    //UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += size.height / 2;
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(info, &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawBlockmapInfo(const Vec2d &origin_, const world::Blockmap &blockmap)
{
    DGL_Enable(DGL_TEXTURE_2D);

    Point2Raw origin = {{{int(origin_.x), int(origin_.y)}}};

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
    //UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    //UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += 8 + th/2;

    UI_TextOutEx2("Blockmap", &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    const Vec2ui &bmapDimensions = blockmap.dimensions();
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

    DGL_Disable(DGL_TEXTURE_2D);
}

static void drawCellInfoBox(const Vec2d &origin, const world::Blockmap &blockmap,
    const char *objectTypeName, const world::BlockmapCell &cell)
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
static void drawBlockmap(const world::Blockmap &bmap, mobj_t *followMobj,
    int (*cellDrawer) (const world::Blockmap &, const world::BlockmapCell &, void *))
{
    world::BlockmapCellBlock vCellBlock;
    world::BlockmapCell vCell;

    const world::BlockmapCell &dimensions = bmap.dimensions();
    const Vec2d cellDimensions = bmap.cellDimensions();

    if (followMobj)
    {
        // Determine the followed Mobj's blockmap coords.
        bool didClip;
        vCell = bmap.toCell(Vec2d(followMobj->origin), &didClip);

        if (didClip)
            followMobj = 0; // Outside the blockmap.

        if (followMobj)
        {
            // Determine the extended blockmap coords for the followed
            // Mobj's "touch" range.
            AABoxd mobBox = Mobj_Bounds(*followMobj);

            mobBox.minX -= DDMOBJ_RADIUS_MAX * 2;
            mobBox.minY -= DDMOBJ_RADIUS_MAX * 2;
            mobBox.maxX += DDMOBJ_RADIUS_MAX * 2;
            mobBox.maxY += DDMOBJ_RADIUS_MAX * 2;

            vCellBlock = bmap.toCellBlock(mobBox);
        }
    }

    if (followMobj)
    {
        // Orient on the center of the followed Mobj.
        DGL_Translatef(-(vCell.x * cellDimensions.x), -(vCell.y * cellDimensions.y), 0);
    }
    else
    {
        // Orient on center of the Blockmap.
        DGL_Translatef(-(cellDimensions.x * dimensions.x)/2,
                       -(cellDimensions.y * dimensions.y)/2, 0);
    }

    // First we'll draw a background showing the "null" cells.
    drawBackground(bmap);
    if (followMobj)
    {
        // Highlight cells the followed Mobj "touches".
        DGL_Begin(DGL_QUADS);

        world::BlockmapCell cell;
        for (cell.y = vCellBlock.min.y; cell.y < vCellBlock.max.y; ++cell.y)
        for (cell.x = vCellBlock.min.x; cell.x < vCellBlock.max.x; ++cell.x)
        {
            if (cell == vCell)
            {
                // The cell the followed Mobj is actually in.
                DGL_Color4f(.66f, .66f, 1, .66f);
            }
            else
            {
                // A cell within the followed Mobj's extended collision range.
                DGL_Color4f(.33f, .33f, .66f, .33f);
            }

            const Vec2d start = cellDimensions * cell;
            const Vec2d end   = start + cellDimensions;

            DGL_Vertex2f(start.x, start.y);
            DGL_Vertex2f(  end.x, start.y);
            DGL_Vertex2f(  end.x,   end.y);
            DGL_Vertex2f(start.x,   end.y);
        }

        DGL_End();
    }

    /**
     * Draw the Gridmap visual.
     * @note Gridmap uses a cell unit size of [width:1,height:1], so we need to
     *       scale it up so it aligns correctly.
     */
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Scalef(cellDimensions.x, cellDimensions.y, 1);

//    bmap.drawDebugVisual();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    /*
     * Draw the blockmap-linked data.
     * Translate the modelview matrix so that objects can be drawn using
     * the map space coordinates directly.
     */
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(-bmap.origin().x, -bmap.origin().y, 0);

    if (cellDrawer)
    {
        if (followMobj)
        {
            /*
             * Draw cell contents color coded according to their range from the
             * followed Mobj.
             */

            // First, the cells outside the "touch" range (crimson).
            World::validCount++;
            DGL_Color4f(.33f, 0, 0, .75f);
            world::BlockmapCell cell;
            for (cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for (cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if (   cell.x >= vCellBlock.min.x && cell.x < vCellBlock.max.x
                    && cell.y >= vCellBlock.min.y && cell.y < vCellBlock.max.y)
                {
                    continue;
                }
                if (!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Next, the cells within the "touch" range (orange).
            World::validCount++;
            DGL_Color3f(1, .5f, 0);
            for (cell.y = vCellBlock.min.y; cell.y < vCellBlock.max.y; ++cell.y)
            for (cell.x = vCellBlock.min.x; cell.x < vCellBlock.max.x; ++cell.x)
            {
                if (cell == vCell) continue;
                if (!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Lastly, the cell the followed Mobj is in (yellow).
            World::validCount++;
            DGL_Color3f(1, 1, 0);
            if (bmap.cellElementCount(vCell))
            {
                cellDrawer(bmap, vCell, 0/*no params*/);
            }
        }
        else
        {
            // Draw all cells without color coding.
            World::validCount++;
            DGL_Color4f(.33f, 0, 0, .75f);
            world::BlockmapCell cell;
            for (cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for (cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if (!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }
        }
    }

    /*
     * Draw the followed mobj, if any.
     */
    if (followMobj)
    {
        World::validCount++;
        DGL_Color3f(0, 1, 0);
        DGL_Begin(DGL_QUADS);
            drawMobj(*followMobj);
        DGL_End();
    }

    // Undo the map coordinate space translation.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void Rend_BlockmapDebug()
{
    // Disabled?
    if (!bmapShowDebug || bmapShowDebug > 4) return;

    if (!world::World::get().hasMap()) return;
    auto &map = App_World().map();

    const world::Blockmap *blockmap;
    int (*cellDrawer) (const world::Blockmap &, const world::BlockmapCell &, void *);
    const char *objectTypeName;
    switch (bmapShowDebug)
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

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    /*
     * Draw the blockmap.
     */
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);
    // Orient on the center of the window.
    DGL_Translatef((DE_GAMEVIEW_WIDTH / 2), (DE_GAMEVIEW_HEIGHT / 2), 0);

    // Uniform scaling factor for this visual.
    float scale = bmapDebugSize / de::max(DE_GAMEVIEW_HEIGHT / 100, 1);
    DGL_Scalef(scale, -scale, 1);

    // If possible we'll tailor what we draw relative to the viewPlayer.
    mobj_t *followMobj = 0;
    if (viewPlayer && viewPlayer->publicData().mo)
    {
        followMobj = viewPlayer->publicData().mo;
    }

    // Draw!
    drawBlockmap(*blockmap, followMobj, cellDrawer);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    /*
     * Draw HUD info.
     */
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    if (followMobj)
    {
        // About the cell the followed Mobj is in.
        bool didClip;
        auto cell = blockmap->toCell(Vec2d(followMobj->origin), &didClip);
        if (!didClip)
        {
            drawCellInfoBox(Vec2d(DE_GAMEVIEW_WIDTH / 2, 30), *blockmap,
                            objectTypeName, cell);
        }
    }

    // About the Blockmap itself.
    drawBlockmapInfo(Vec2d(DE_GAMEVIEW_WIDTH - 10, DE_GAMEVIEW_HEIGHT - 10),
                     *blockmap);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}
