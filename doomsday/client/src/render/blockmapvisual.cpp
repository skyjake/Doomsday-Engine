/** @file render/blockmapvisual.cpp Graphical Blockmap Visual.
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

#include <cmath>

#include <de/concurrency.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_play.h"
#include "de_ui.h"

#include "HEdge"

#include "world/blockmap.h"
#include "world/map.h"

#include "render/blockmapvisual.h"

using namespace de;

byte bmapShowDebug = 0; // 1 = mobjs, 2 = lines, 3 = BSP leafs, 4 = polyobjs. cvar
float bmapDebugSize = 1.5f; // cvar

static int rendMobj(mobj_t *mo, void * /*parameters*/)
{
    if(mo->validCount != validCount)
    {
        vec2f_t start; V2f_Set(start, mo->origin[VX] - mo->radius, mo->origin[VY] - mo->radius);
        vec2f_t end;   V2f_Set(end,   mo->origin[VX] + mo->radius, mo->origin[VY] + mo->radius);

        glVertex2f(start[VX], start[VY]);
        glVertex2f(  end[VX], start[VY]);
        glVertex2f(  end[VX],   end[VY]);
        glVertex2f(start[VX],   end[VY]);

        mo->validCount = validCount;
    }
    return false; // Continue iteration.
}

static int rendLine(Line *line, void * /*parameters*/)
{
    if(line->validCount() != validCount)
    {
        glVertex2f(line->fromOrigin().x, line->fromOrigin().y);
        glVertex2f(  line->toOrigin().x,   line->toOrigin().y);

        line->setValidCount(validCount);
    }
    return false; // Continue iteration.
}

static int rendBspLeaf(BspLeaf *bspLeaf, void * /*parameters*/)
{
    if(!bspLeaf->isDegenerate() && bspLeaf->validCount() != validCount)
    {
        float const scale = de::max(bmapDebugSize, 1.f);
        float const width = (DENG_WINDOW->width() / 16) / scale;
        float length, dx, dy, normal[2], unit[2];
        vec2f_t start, end;

        Face const &face = bspLeaf->face();

        HEdge *hedge = face.hedge();
        do
        {
            V2f_Set(start, hedge->origin().x, hedge->origin().y);
            V2f_Set(end,   hedge->twin().origin().x, hedge->twin().origin().y);

            glBegin(GL_LINES);
                glVertex2fv(start);
                glVertex2fv(end);
            glEnd();

            dx = end[VX] - start[VX];
            dy = end[VY] - start[VY];
            length = sqrt(dx * dx + dy * dy);
            if(length > 0)
            {
                unit[VX] = dx / length;
                unit[VY] = dy / length;
                normal[VX] = -unit[VY];
                normal[VY] = unit[VX];

                GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC));
                glEnable(GL_TEXTURE_2D);

                GL_BlendOp(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);

                glBegin(GL_QUADS);
                    glTexCoord2f(0.75f, 0.5f);
                    glVertex2fv(start);
                    glTexCoord2f(0.75f, 0.5f);
                    glVertex2fv(end);
                    glTexCoord2f(0.75f, 1);
                    glVertex2f(end[VX] - normal[VX] * width,
                               end[VY] - normal[VY] * width);
                    glTexCoord2f(0.75f, 1);
                    glVertex2f(start[VX] - normal[VX] * width,
                               start[VY] - normal[VY] * width);
                glEnd();

                glDisable(GL_TEXTURE_2D);
                GL_BlendMode(BM_NORMAL);
            }

            // Draw a bounding box for the leaf's face geometry.
            V2f_Set(start, face.aaBox().minX, face.aaBox().minY);
            V2f_Set(end,   face.aaBox().maxX, face.aaBox().maxY);

            glBegin(GL_LINES);
                glVertex2f(start[VX], start[VY]);
                glVertex2f(  end[VX], start[VY]);
                glVertex2f(  end[VX], start[VY]);
                glVertex2f(  end[VX],   end[VY]);
                glVertex2f(  end[VX],   end[VY]);
                glVertex2f(start[VX],   end[VY]);
                glVertex2f(start[VX],   end[VY]);
                glVertex2f(start[VX], start[VY]);
            glEnd();

        } while((hedge = &hedge->next()) != face.hedge());

        bspLeaf->setValidCount(validCount);
    }
    return false; // Continue iteration.
}

static int rendCellLines(Blockmap const &bmap, Blockmap::Cell const &cell, void *parameters)
{
    glBegin(GL_LINES);
        bmap.iterate(cell, (int (*)(void*,void*)) rendLine, parameters);
    glEnd();
    return false; // Continue iteration.
}

static int rendCellPolyobjLines(void *object, void *parameters)
{
    Polyobj *po = (Polyobj *)object;
    DENG_ASSERT(po != 0);
    foreach(Line *line, po->lines())
    {
        if(line->validCount() == validCount)
            continue;

        line->setValidCount(validCount);
        int result = rendLine(line, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

static int rendCellPolyobjs(Blockmap const &bmap, Blockmap::Cell const &cell, void *parameters)
{
    glBegin(GL_LINES);
        bmap.iterate(cell, (int (*)(void*,void*)) rendCellPolyobjLines, parameters);
    glEnd();
    return false; // Continue iteration.
}

static int rendCellMobjs(Blockmap const &bmap, Blockmap::Cell const &cell, void *parameters)
{
    glBegin(GL_QUADS);
        bmap.iterate(cell, (int (*)(void*,void*)) rendMobj, parameters);
    glEnd();
    return false; // Continue iteration.
}

static int rendCellBspLeafs(Blockmap const &bmap, Blockmap::Cell const &cell, void *parameters)
{
    bmap.iterate(cell, (int (*)(void*,void*)) rendBspLeaf, parameters);
    return false; // Continue iteration.
}

static void rendBlockmapBackground(Blockmap const &bmap)
{
    Blockmap::Cell const &bmapDimensions = bmap.dimensions();

    // Scale modelview matrix so we can express cell geometry
    // using a cell-sized unit coordinate space.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(bmap.cellWidth(), bmap.cellHeight(), 1);

    /*
     * Draw the translucent quad which represents the "used" cells.
     */
    glColor4f(.25f, .25f, .25f, .66f);
    glBegin(GL_QUADS);
        glVertex2f(0,                0);
        glVertex2f(bmapDimensions.x, 0);
        glVertex2f(bmapDimensions.x, bmapDimensions.y);
        glVertex2f(0,                bmapDimensions.y);
    glEnd();

    /*
     * Draw the "null cells" over the top.
     */
    glColor4f(0, 0, 0, .95f);
    Blockmap::Cell cell;
    for(cell.y = 0; cell.y < bmapDimensions.y; ++cell.y)
    for(cell.x = 0; cell.x < bmapDimensions.x; ++cell.x)
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

static void drawCellInfo(Point2Raw const *_origin, char const *info)
{
    DENG_ASSERT(_origin != 0);

    glEnable(GL_TEXTURE_2D);

    FR_SetFont(fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    FR_SetShadowStrength(UI_SHADOW_STRENGTH);

    Size2Raw size(16 + FR_TextWidth(info),
                  16 + FR_SingleLineHeight(info));

    Point2Raw origin = *_origin;

    origin.x -= size.width / 2;
    UI_GradientEx(&origin, &size, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(&origin, &size, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    origin.x += 8;
    origin.y += size.height / 2;
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(info, &origin, UI_Color(UIC_TITLE), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawBlockmapInfo(Point2Raw const *_origin, Blockmap const *blockmap)
{
    DENG_ASSERT(blockmap != 0);

    glEnable(GL_TEXTURE_2D);

    Point2Raw origin = *_origin;

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

    Vector2ui const &bmapDimensions = blockmap->dimensions();
    char buf[80];
    dd_snprintf(buf, 80, "Dimensions:(%u, %u) #%li", bmapDimensions.x, bmapDimensions.y,
                                                    (long) bmapDimensions.y * bmapDimensions.x);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "Cell dimensions:(%.3f, %.3f)", blockmap->cellWidth(), blockmap->cellHeight());
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);
    origin.y += th;

    dd_snprintf(buf, 80, "(%+06.0f, %+06.0f) (%+06.0f, %+06.0f)",
                         blockmap->bounds().minX, blockmap->bounds().minY,
                         blockmap->bounds().maxX, blockmap->bounds().maxY);
    UI_TextOutEx2(buf, &origin, UI_Color(UIC_TEXT), 1, ALIGN_LEFT, DTF_ONLY_SHADOW);

    glDisable(GL_TEXTURE_2D);
}

static void drawCellInfoBox(Blockmap const *blockmap, Point2Raw const *origin,
    char const *objectTypeName, Blockmap::Cell const &cell)
{
    uint count = blockmap->cellElementCount(cell);
    char info[160];

    dd_snprintf(info, 160, "Cell:(%u, %u) %s:#%u", cell.x, cell.y, objectTypeName, count);
    drawCellInfo(origin, info);
}

/**
 * @param bmap        Blockmap to be rendered.
 * @param followMobj  Mobj to center/focus the visual on. Can be @c NULL.
 * @param cellDrawer  Blockmap cell content drawing callback. Can be @a NULL.
 */
static void rendBlockmap(Blockmap const &bmap, mobj_t *followMobj,
    int (*cellDrawer) (Blockmap const &bmap, Blockmap::Cell const &cell, void *parameters))
{
    Blockmap::CellBlock vCellBlock;
    Blockmap::Cell vCell;

    Blockmap::Cell const &dimensions = bmap.dimensions();
    Vector2d const &cellDimensions   = bmap.cellDimensions();

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
            float const radius = followMobj->radius + DDMOBJ_RADIUS_MAX * 2;

            AABoxd aaBox;
            vec2d_t start; V2d_Set(start, followMobj->origin[VX] - radius, followMobj->origin[VY] - radius);
            vec2d_t end;   V2d_Set(end,   followMobj->origin[VX] + radius, followMobj->origin[VY] + radius);
            V2d_InitBox(aaBox.arvec2, start);
            V2d_AddToBox(aaBox.arvec2, end);

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
    rendBlockmapBackground(bmap);
    if(followMobj)
    {
        // Highlight cells the followed Mobj "touches".
        glBegin(GL_QUADS);

        Blockmap::Cell cell;
        for(cell.y = vCellBlock.min.y; cell.y <= vCellBlock.max.y; ++cell.y)
        for(cell.x = vCellBlock.min.x; cell.x <= vCellBlock.max.x; ++cell.x)
        {
            if(cell.x == vCell.x && cell.y == vCell.y)
            {
                // The cell the followed Mobj is actually in.
                glColor4f(.66f, .66f, 1, .66f);
            }
            else
            {
                // A cell within the followed Mobj's extended collision range.
                glColor4f(.33f, .33f, .66f, .33f);
            }

            vec2d_t start; V2d_Set(start, cell.x * cellDimensions.x, cell.y * cellDimensions.y);
            vec2d_t end;   V2d_Set(end, cellDimensions.x, cellDimensions.y);
            V2d_Sum(end, end, start);

            glVertex2d(start[VX], start[VY]);
            glVertex2d(  end[VX], start[VY]);
            glVertex2d(  end[VX],   end[VY]);
            glVertex2d(start[VX],   end[VY]);
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

    bmap.gridmap().drawDebugVisual();

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
            Blockmap::Cell cell;
            for(cell.y = 0; cell.y < dimensions.y; ++cell.y)
            for(cell.x = 0; cell.x < dimensions.x; ++cell.x)
            {
                if(cell.x >= vCellBlock.min.x && cell.x <= vCellBlock.max.x &&
                   cell.y >= vCellBlock.min.y && cell.y <= vCellBlock.max.y) continue;
                if(!bmap.cellElementCount(cell)) continue;

                cellDrawer(bmap, cell, 0/*no params*/);
            }

            // Next, the cells within the "touch" range (orange).
            validCount++;
            glColor3f(1, .5f, 0);
            for(cell.y = vCellBlock.min.y; cell.y <= vCellBlock.max.y; ++cell.y)
            for(cell.x = vCellBlock.min.x; cell.x <= vCellBlock.max.x; ++cell.x)
            {
                if(cell.x == vCell.x && cell.y == vCell.y) continue;
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
            Blockmap::Cell cell;
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
            rendMobj(followMobj, NULL/*no params*/);
        glEnd();
    }

    // Undo the map coordinate space translation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Rend_BlockmapDebug()
{
    int (*cellDrawer) (Blockmap const &blockmap, Blockmap::Cell const &cell, void *parameters);
    char const *objectTypeName;
    mobj_t *followMobj = 0;
    Blockmap const *blockmap;
    Point2Raw origin;
    float scale;

    // Disabled?
    if(!bmapShowDebug || bmapShowDebug > 4) return;

    if(!App_World().hasMap()) return;

    Map &map = App_World().map();
    switch(bmapShowDebug)
    {
    default: // MobjLinks.
        if(!map.mobjBlockmap()) return;

        blockmap = map.mobjBlockmap();
        cellDrawer = rendCellMobjs;
        objectTypeName = "Mobjs";
        break;

    case 2: // Lines.
        if(!map.lineBlockmap()) return;

        blockmap = map.lineBlockmap();
        cellDrawer = rendCellLines;
        objectTypeName = "Lines";
        break;

    case 3: // BspLeafs.
        if(!map.bspLeafBlockmap()) return;

        blockmap = map.bspLeafBlockmap();
        cellDrawer = rendCellBspLeafs;
        objectTypeName = "BSP Leafs";
        break;

    case 4: // PolyobjLinks.
        if(!map.polyobjBlockmap()) return;

        blockmap = map.polyobjBlockmap();
        cellDrawer = rendCellPolyobjs;
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
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);
    // Orient on the center of the window.
    glTranslatef((DENG_WINDOW->width() / 2), (DENG_WINDOW->height() / 2), 0);

    // Uniform scaling factor for this visual.
    scale = bmapDebugSize / de::max(DENG_WINDOW->height() / 100, 1);
    glScalef(scale, -scale, 1);

    // If possible we'll tailor what we draw relative to the viewPlayer.
    if(viewPlayer && viewPlayer->shared.mo)
        followMobj = viewPlayer->shared.mo;

    // Draw!
    rendBlockmap(*blockmap, followMobj, cellDrawer);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    /*
     * Draw HUD info.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_WINDOW->width(), DENG_WINDOW->height(), 0, -1, 1);

    if(followMobj)
    {
        // About the cell the followed Mobj is in.
        bool didClip;
        Blockmap::Cell cell = blockmap->toCell(followMobj->origin, &didClip);
        if(!didClip)
        {
            origin.x = DENG_WINDOW->width() / 2;
            origin.y = 30;
            drawCellInfoBox(blockmap, &origin, objectTypeName, cell);
        }
    }

    // About the Blockmap itself.
    origin.x = DENG_WINDOW->width()  - 10;
    origin.y = DENG_WINDOW->height() - 10;
    drawBlockmapInfo(&origin, blockmap);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
