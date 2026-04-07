/** @file hu_lib.cpp  HUD widget library.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "hu_lib.h"

#include <de/list.h>

/// @todo remove me:
#include "hud/widgets/automapwidget.h"
#include "hud/widgets/chatwidget.h"
#include "hud/widgets/groupwidget.h"
#include "hud/widgets/playerlogwidget.h"
#include "menu/widgets/lineeditwidget.h"
#include "menu/widgets/sliderwidget.h"

using namespace de;
using namespace common;

static bool inited;

static List<HudWidget *> widgets;

static ui_rendstate_t uiRS;
const ui_rendstate_t *uiRendState = &uiRS;

static uiwidgetid_t nextUnusedId()
{
    return uiwidgetid_t(widgets.count());
}

static void clearWidgets()
{
    deleteAll(widgets); widgets.clear();
}

void GUI_Init()
{
    if(inited) return;

    clearWidgets();
    ChatWidget::loadMacros();

    inited = true;

    GUI_LoadResources();
}

void GUI_Shutdown()
{
    if(!inited) return;
    clearWidgets();
    inited = false;
}

HudWidget *GUI_AddWidget(HudWidget *wi)
{
    DE_ASSERT(inited);
    if(wi)
    {
        wi->setId(nextUnusedId());
        widgets << wi;
    }
    return wi;
}

HudWidget *GUI_TryFindWidgetById(uiwidgetid_t id)
{
    if(!inited) return 0;  // GUI not available.
    if(id >= 0)
    {
        for(HudWidget *wi : widgets)
        {
            if(wi->id() == id) return wi;
        }
    }
    return nullptr;  // Not found.
}

HudWidget &GUI_FindWidgetById(uiwidgetid_t id)
{
    if(HudWidget *wi = GUI_TryFindWidgetById(id)) return *wi;
    throw Error("GUI_FindWidgetById", "Unknown widget id #" + String::asText(id));
}

void GUI_UpdateWidgetGeometry(HudWidget *wi)
{
    if(!wi) return;

    Rect_SetXY(&wi->geometry(), 0, 0);
    wi->updateGeometry(wi);

    if(Rect_Width (&wi->geometry()) <= 0 ||
       Rect_Height(&wi->geometry()) <= 0) return;

    if(wi->alignment() & ALIGN_RIGHT)
        Rect_SetX(&wi->geometry(), Rect_X(&wi->geometry()) - Rect_Width(&wi->geometry()));
    else if(!(wi->alignment() & ALIGN_LEFT))
        Rect_SetX(&wi->geometry(), Rect_X(&wi->geometry()) - Rect_Width(&wi->geometry()) / 2);

    if(wi->alignment() & ALIGN_BOTTOM)
        Rect_SetY(&wi->geometry(), Rect_Y(&wi->geometry()) - Rect_Height(&wi->geometry()));
    else if(!(wi->alignment() & ALIGN_TOP))
        Rect_SetY(&wi->geometry(), Rect_Y(&wi->geometry()) - Rect_Height(&wi->geometry()) / 2);
}

#if defined(UI_DEBUG)
static void drawWidgetGeometry(HudWidget *wi)
{
    DE_ASSERT(wi);

    RectRaw geometry;
    Rect_Raw(&wi->geometry(), &geometry);

    DGL_Color3f(1, 1, 1);
    DGL_Begin(DGL_LINES);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y);
    DGL_End();
}

static void drawWidgetAvailableSpace(HudWidget *wi)
{
    DE_ASSERT(wi);
    DGL_Color4f(0, .4f, 0, .1f);
    DGL_DrawRectf2(Rect_X(&wi->geometry()), Rect_Y(&wi->geometry()), wi->maximumSize().width, wi->maximumSize().height);
}
#endif

static void drawWidget2(HudWidget *wi, const Point2Raw *offset = nullptr)
{
    DE_ASSERT(wi);

#if defined(UI_DEBUG)
    drawWidgetAvailableSpace(wi);
#endif

    if(wi->drawer && wi->opacity() > .0001f)
    {
        Point2Raw origin;
        Point2_Raw(Rect_Origin(&wi->geometry()), &origin);

        // Configure the page render state.
        uiRS.pageAlpha = wi->opacity();

        FR_PushAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin.x, origin.y, 0);

        // Do not pass a zero length offset.
        wi->drawer(wi, ((offset && (offset->x || offset->y))? offset : nullptr));

        FR_PopAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin.x, -origin.y, 0);
    }

#if defined(UI_DEBUG)
    drawWidgetGeometry(wi);
#endif
}

static void drawWidget(HudWidget *wi, const Point2Raw *origin = nullptr)
{
    DE_ASSERT(wi);

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    // First ourself.
    drawWidget2(wi);

    if(auto *group = maybeAs<GroupWidget>(wi))
    {
        // Then our children.
        group->forAllChildren([] (HudWidget &child)
        {
            drawWidget(&child);
            return LoopContinue;
        });
    }

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin->x, -origin->y, 0);
    }
}

void GUI_DrawWidget(HudWidget *wi, const Point2Raw *offset)
{
    if(!wi) return;
    if(wi->maximumWidth() < 1 || wi->maximumHeight() < 1) return;
    if(wi->opacity() <= 0) return;

    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    GUI_UpdateWidgetGeometry(wi);

    FR_PopAttrib();

    // Draw.
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    // Do not pass a zero length offset.
    drawWidget(wi, ((offset && (offset->x || offset->y))? offset : nullptr));

    FR_PopAttrib();
}

void GUI_DrawWidgetXY(HudWidget *wi, int x, int y)
{
    Point2Raw origin = {{{x, y}}};
    GUI_DrawWidget(wi, &origin);
}

void GUI_SpriteSize(int sprite, float scale, int *width, int *height)
{
    spriteinfo_t info;
    if(!width && !height) return;
    if(!R_GetSpriteInfo(sprite, 0, &info)) return;

    if(width)  *width  = info.geometry.size.width  * scale;
    if(height) *height = info.geometry.size.height * scale;
}

void GUI_DrawSprite(int sprite, float x, float y, hotloc_t hotspot,
    float scale, float alpha, dd_bool flip, int* drawnWidth, int *drawnHeight)
{
    spriteinfo_t info;

    if(!(alpha > 0)) return;

    alpha = MINMAX_OF(0.f, alpha, 1.f);
    R_GetSpriteInfo(sprite, 0, &info);

    switch(hotspot)
    {
    case HOT_BRIGHT:
        y -= info.geometry.size.height * scale;
        // Fall through.
    case HOT_TRIGHT:
        x -= info.geometry.size.width * scale;
        break;

    case HOT_BLEFT:
        y -= info.geometry.size.height * scale;
        break;
    default: break;
    }

    DGL_SetPSprite(info.material);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, alpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, flip * info.texCoord[0], 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], 0);
        DGL_Vertex2f(x + info.geometry.size.width * scale, y);

        DGL_TexCoord2f(0, !flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x + info.geometry.size.width * scale, y + info.geometry.size.height * scale);

        DGL_TexCoord2f(0, flip * info.texCoord[0], info.texCoord[1]);
        DGL_Vertex2f(x, y + info.geometry.size.height * scale);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);

    if(drawnWidth)  *drawnWidth  = info.geometry.size.width  * scale;
    if(drawnHeight) *drawnHeight = info.geometry.size.height * scale;
}

void GUI_Register()
{
    AutomapWidget::consoleRegister();
    ChatWidget::consoleRegister();
    PlayerLogWidget::consoleRegister();
}

void GUI_LoadResources()
{
    if(Get(DD_NOVIDEO)) return;

    AutomapWidget::prepareAssets();
    menu::LineEditWidget::loadResources();
    menu::SliderWidget::loadResources();
}

void GUI_ReleaseResources()
{
    if(Get(DD_NOVIDEO)) return;

    AutomapWidget::prepareAssets();

    for(HudWidget *wi : widgets)
    {
        if(auto *automap = maybeAs<AutomapWidget>(wi))
        {
            automap->reset();
        }
    }
}
