/**
 * @file zonedebug.c
 * Memory zone debug visualization. @ingroup memzone
 *
 * Shows the contents of the memory zone as on-screen visualization. This is
 * only available in debug builds and provides a view to the layout of the
 * allocated memory inside the zone.
 *
 * @authors Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de_graphics.h"

#include <math.h>
#include <de/concurrency.h>

#ifdef _DEBUG

/// @todo Find a better way to access the private data of the zone
/// (e.g., move this into the library and use an abstract graphics interface).
#include "../../libdeng1/src/memoryzone_private.h"

void Z_DrawRegion(memvolume_t* volume, RectRaw* rect, size_t start, size_t size, const float* color)
{
    const int bytesPerRow = (volume->size - sizeof(memzone_t)) / rect->size.height;
    const float toPixelScale = (float)rect->size.width / (float)bytesPerRow;
    const size_t edge = rect->origin.x + rect->size.width;
    int x = (start % bytesPerRow)*toPixelScale + rect->origin.x;
    int y = start / bytesPerRow + rect->origin.y;
    int pixels = MAX_OF(1, ceil(size * toPixelScale));

    assert(start + size <= volume->size);

    while(pixels > 0)
    {
        const int availPixels = edge - x;
        const int usedPixels = MIN_OF(availPixels, pixels);

        glColor4fv(color);
        glVertex2i(x, y);
        glVertex2i(x + usedPixels, y);

        pixels -= usedPixels;

        // Move to the next row.
        y++;
        x = rect->origin.x;
    }
}

void Z_DebugDrawVolume(MemoryZonePrivateData* pd, memvolume_t* volume, RectRaw* rect)
{
    memblock_t* block;
    char* base = ((char*)volume->zone) + sizeof(memzone_t);
    float opacity = .85f;
    float colAppStatic[4]   = { 1, 1, 1, .65f };
    float colGameStatic[4]  = { 1, 0, 0, .65f };
    float colMap[4]         = { 0, 1, 0, .65f };
    float colMapStatic[4]   = { 0, .5f, 0, .65f };
    float colCache[4]       = { 1, 0, 1, .65f };
    float colOther[4]       = { 0, 0, 1, .65f };

    // Clear the background.
    glColor4f(0, 0, 0, opacity);
    GL_DrawRect(rect);

    // Outline.
    glLineWidth(1);
    glColor4f(1, 1, 1, opacity/2);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    GL_DrawRect(rect);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBegin(GL_LINES);

    // Visualize each block.
    for(block = volume->zone->blockList.next;
        block != &volume->zone->blockList;
        block = block->next)
    {
        const float* color = colOther;
        if(!block->user) continue; // Free is black.

        // Choose the color for this block.
        switch(block->tag)
        {
        case PU_GAMESTATIC: color = colGameStatic; break;
        case PU_MAP:        color = colMap; break;
        case PU_MAPSTATIC:  color = colMapStatic; break;
        case PU_APPSTATIC:  color = colAppStatic; break;
        default:
            if(block->tag >= PU_PURGELEVEL)
                color = colCache;
            break;
        }

        Z_DrawRegion(volume, rect, (char*)block - base, block->size, color);
    }

    glEnd();

    if(pd->isVolumeTooFull(volume))
    {
        glLineWidth(2);
        glColor4f(1, 0, 0, 1);
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

    if(!CommandLine_Exists("-zonedebug")) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    Z_GetPrivateData(&pd);

    // Draw each volume.
    pd.lock();

    // Make sure all the volumes fit vertically.
    volCount = pd.volumeCount;
    h = 200;
    if(h * volCount + 10*(volCount - 1) > Window_Height(theWindow))
    {
        h = (Window_Height(theWindow) - 10*(volCount - 1))/volCount;
    }

    i = 0;
    for(volume = pd.volumeRoot; volume; volume = volume->next, ++i)
    {
        RectRaw rect;
        rect.size.width = MIN_OF(400, Window_Width(theWindow));
        rect.size.height = h;
        rect.origin.x = Window_Width(theWindow) - rect.size.width - 1;
        rect.origin.y = Window_Height(theWindow) - rect.size.height*(i+1) - 10*i - 1;
        Z_DebugDrawVolume(&pd, volume, &rect);
    }

    pd.unlock();

    // Cleanup.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

#endif // _DEBUG
