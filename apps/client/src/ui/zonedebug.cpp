/** @file zonedebug.cpp  Memory zone debug visualization.
 *
 * Shows the contents of the memory zone as on-screen visualization. This is
 * only available in debug builds and provides a view to the layout of the
 * allocated memory inside the zone.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#if defined (DE_DEBUG) && defined (DE_OPENGL)

#include <cmath>
#include <de/glstate.h>
#include <de/glinfo.h>
#include <de/legacy/concurrency.h>
#include <de/rectangle.h>
#include <de/vector.h>

/// @todo Find a better way to access the private data of the zone
/// (e.g., move this into the library and use an abstract graphics interface).
#include "../../../libs/core/src/legacy/memoryzone_private.h"

#include "gl/gl_main.h"
#include "gl/gl_draw.h"
#include "ui/clientwindow.h"

using namespace de;

static void drawRegion(memvolume_t &volume, const Rectanglei &rect, size_t start,
    size_t size, float const color[4])
{
    DE_ASSERT(start + size <= volume.size);

    const int bytesPerRow = (volume.size - sizeof(memzone_t)) / rect.height();
    const float toPixelScale = (float)rect.width() / (float)bytesPerRow;
    const size_t edge = rect.topLeft.x + rect.width();
    int x = (start % bytesPerRow) * toPixelScale + rect.topLeft.x;
    int y = start / bytesPerRow + rect.topLeft.y;
    int pixels = de::max<dint>(1, std::ceil(size * toPixelScale));

    while (pixels > 0)
    {
        const int availPixels = edge - x;
        const int usedPixels = de::min(availPixels, pixels);

        DGL_Color4fv(color);
        DGL_Vertex2f(x, y);
        DGL_Vertex2f(x + usedPixels, y);

        pixels -= usedPixels;

        // Move to the next row.
        y++;
        x = rect.topLeft.x;
    }
}

void Z_DebugDrawVolume(MemoryZonePrivateData *pd, memvolume_t *volume, const Rectanglei &rect)
{
    const float opacity = .85f;
    float const colAppStatic[4]   = { 1, 1, 1, .65f };
    float const colGameStatic[4]  = { 1, 0, 0, .65f };
    float const colMap[4]         = { 0, 1, 0, .65f };
    float const colMapStatic[4]   = { 0, .5f, 0, .65f };
    float const colCache[4]       = { 1, 0, 1, .65f };
    float const colOther[4]       = { 0, 0, 1, .65f };

    char *base = ((char *)volume->zone) + sizeof(memzone_t);

    // Clear the background.
    DGL_Color4f(0, 0, 0, opacity);
    GL_DrawRect(rect);

    // Outline.
//    GLInfo::setLineWidth(1);
    DGL_Color4f(1, 1, 1, opacity/2);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_DrawRect(rect);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    DGL_Begin(DGL_LINES);

    // Visualize each block.
    for (memblock_t *block = volume->zone->blockList.next;
        block != &volume->zone->blockList;
        block = block->next)
    {
        const float *color = colOther;
        if (!block->user) continue; // Free is black.

        // Choose the color for this block.
        switch (block->tag)
        {
        case PU_GAMESTATIC: color = colGameStatic; break;
        case PU_MAP:        color = colMap; break;
        case PU_MAPSTATIC:  color = colMapStatic; break;
        case PU_APPSTATIC:  color = colAppStatic; break;
        default:
            if (block->tag >= PU_PURGELEVEL)
                color = colCache;
            break;
        }

        drawRegion(*volume, rect, (char *)block - base, block->size, color);
    }

    DGL_End();

    if (pd->isVolumeTooFull(volume))
    {
//        GLInfo::setLineWidth(2);
        DGL_Color4f(1, 0, 0, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        GL_DrawRect(rect);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void Z_DebugDrawer(void)
{
    MemoryZonePrivateData pd;
    memvolume_t* volume;
    int i, volCount, h;

    if (!CommandLine_Exists("-zonedebug")) return;

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_PushState();
    DGL_CullFace(DGL_NONE);
    DGL_Disable(DGL_DEPTH_TEST);

    // Go into screen projection mode.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_LoadIdentity();

    Z_GetPrivateData(&pd);

    // Draw each volume.
    pd.lock();

    // Make sure all the volumes fit vertically.
    volCount = pd.volumeCount;
    h = 200;
    if (h * volCount + 10*(volCount - 1) > DE_GAMEVIEW_HEIGHT)
    {
        h = (DE_GAMEVIEW_HEIGHT - 10*(volCount - 1))/volCount;
    }

    i = 0;
    for (volume = pd.volumeRoot; volume; volume = volume->next, ++i)
    {
        int size = de::min(400, DE_GAMEVIEW_WIDTH);
        Z_DebugDrawVolume(&pd, volume,
                          Rectanglei::fromSize(Vec2i(DE_GAMEVIEW_WIDTH - size - 1,
                                                        DE_GAMEVIEW_HEIGHT - size * (i+1) - 10*i - 1),
                                               Vec2ui(size, size)));
    }

    pd.unlock();

    DGL_PopState();

    // Cleanup.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}

#endif // _DEBUG
