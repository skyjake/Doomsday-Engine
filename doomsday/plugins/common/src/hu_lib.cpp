/** @file hu_lib.cpp  HUD widget library.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <de/memory.h>

#include "hu_chat.h"
#include "hu_lib.h"
#include "hu_log.h"
#include "hu_automap.h"
#include "m_ctrl.h"

// @todo Remove external dependencies
#include "hu_menu.h" // For the menu sound ids.

using namespace de;
using namespace common;

void Hu_MenuDrawFocusCursor(int x, int y, int focusObjectHeight, float alpha);

static dd_bool inited = false;

static int numWidgets;
static uiwidget_t *widgets;

static ui_rendstate_t uiRS;
const ui_rendstate_t *uiRendState = &uiRS;

static patchid_t pSliderLeft;
static patchid_t pSliderRight;
static patchid_t pSliderMiddle;
static patchid_t pSliderHandle;
static patchid_t pEditLeft;
static patchid_t pEditRight;
static patchid_t pEditMiddle;

static void errorIfNotInited(char const *callerName)
{
    if(inited) return;
    Con_Error("%s: GUI module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static void lerpColor(float *dst, float const *a, float const *b, float t, dd_bool rgbaMode)
{
    if(t <= 0)
    {
        dst[CR] = a[CR];
        dst[CG] = a[CG];
        dst[CB] = a[CB];
        if(rgbaMode) dst[CA] = a[CA];
        return;
    }
    if(t >= 1)
    {
        dst[CR] = b[CR];
        dst[CG] = b[CG];
        dst[CB] = b[CB];
        if(rgbaMode) dst[CA] = b[CA];
        return;
    }
    dst[CR] = (1 - t) * a[CR] + t * b[CR];
    dst[CG] = (1 - t) * a[CG] + t * b[CG];
    dst[CB] = (1 - t) * a[CB] + t * b[CB];
    if(rgbaMode) dst[CA] = (1 - t) * a[CA] + t * b[CA];
}

static uiwidgetid_t nextUnusedId()
{
    return uiwidgetid_t(numWidgets);
}

static uiwidget_t *allocateWidget(guiwidgettype_t type, uiwidgetid_t id, int player, void *typedata)
{
    widgets = (uiwidget_t *) M_Realloc(widgets, sizeof(*widgets) * ++numWidgets);

    uiwidget_t *ob = &widgets[numWidgets-1];

    de::zapPtr(ob);
    ob->geometry = Rect_New();
    ob->type     = type;
    ob->id       = id;
    ob->player   = player;

    switch(ob->type)
    {
    case GUI_GROUP: {
        ob->typedata = (guidata_group_t *) M_Calloc(sizeof(guidata_group_t));
        break; }

    default:
        ob->typedata = typedata;
        break;
    }

    switch(ob->type)
    {
    case GUI_AUTOMAP: {
        guidata_automap_t *am = (guidata_automap_t*)ob->typedata;
        am->mcfg                 = ST_AutomapConfig();
        am->followPlayer         = player;
        am->oldViewScale         = 1;
        am->maxViewPositionDelta = 128;
        am->alpha = am->targetAlpha = am->oldAlpha = 0;

        /// Set initial geometry size.
        /// @todo Should not be necessary...
        Rect_SetWidthHeight(ob->geometry, SCREENWIDTH, SCREENHEIGHT);
        break; }

    default: break;
    }

    return ob;
}

static uiwidget_t *createWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity, void (*updateGeometry) (uiwidget_t *ob),
    void (*drawer) (uiwidget_t *ob, Point2Raw const *offset),
    void (*ticker) (uiwidget_t *ob, timespan_t ticLength), void *typedata)
{
    DENG2_ASSERT(updateGeometry != 0);

    uiwidget_t *ob = allocateWidget(type, nextUnusedId(), player, typedata);

    ob->alignFlags     = alignFlags;
    ob->font           = fontId;
    ob->opacity        = opacity;
    ob->updateGeometry = updateGeometry;
    ob->drawer         = drawer;
    ob->ticker         = ticker;

    return ob;
}

static void clearWidgets()
{
    if(!numWidgets) return;

    for(int i = 0; i < numWidgets; ++i)
    {
        uiwidget_t *ob = &widgets[i];
        if(ob->type == GUI_GROUP)
        {
            guidata_group_t *grp = (guidata_group_t *)ob->typedata;
            M_Free(grp->widgetIds);
            M_Free(grp);
        }
        Rect_Delete(ob->geometry);
    }
    M_Free(widgets); widgets = 0;
    numWidgets = 0;
}

uiwidget_t *GUI_FindObjectById(uiwidgetid_t id)
{
    if(!inited) return 0; // GUI not available.
    if(id >= 0)
    {
        for(int i = 0; i < numWidgets; ++i)
        {
            uiwidget_t *ob = &widgets[i];
            if(ob->id == id)
            {
                return ob;
            }
        }
    }
    return 0; // Not found.
}

uiwidget_t *GUI_MustFindObjectById(uiwidgetid_t id)
{
    uiwidget_t *ob = GUI_FindObjectById(id);
    if(!ob)
    {
        Con_Error("GUI_MustFindObjectById: Failed to locate object with id %i.", (int) id);
    }
    return ob;
}

void GUI_Register()
{
    UIAutomap_Register();
    UIChat_Register();
    UILog_Register();
}

void GUI_Init()
{
    if(inited) return;

    numWidgets = 0;
    widgets    = 0;
    UIChat_LoadMacros();

    inited = true;

    GUI_LoadResources();
}

void GUI_Shutdown()
{
    if(!inited) return;
    clearWidgets();
    inited = false;
}

void GUI_LoadResources()
{
    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_LoadResources();
    menu::LineEditWidget::loadResources();
    menu::SliderWidget::loadResources();
}

void GUI_ReleaseResources()
{
    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_ReleaseResources();

    for(int i = 0; i < numWidgets; ++i)
    {
        uiwidget_t *ob = widgets + i;
        switch(ob->type)
        {
        case GUI_AUTOMAP: UIAutomap_Reset(ob); break;
        default: break;
        }
    }
}

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity,
    void (*updateGeometry) (uiwidget_t *ob), void (*drawer) (uiwidget_t *ob, Point2Raw const *offset),
    void (*ticker) (uiwidget_t *ob, timespan_t ticLength), void *typedata)
{
    errorIfNotInited("GUI_CreateWidget");
    uiwidget_t *ob = createWidget(type, player, alignFlags, fontId, opacity, updateGeometry, drawer, ticker, typedata);
    return ob->id;
}

uiwidgetid_t GUI_CreateGroup(int groupFlags, int player, int alignFlags, order_t order, int padding)
{
    errorIfNotInited("GUI_CreateGroup");

    uiwidget_t *ob = createWidget(GUI_GROUP, player, alignFlags, 1, 0, UIGroup_UpdateGeometry, NULL, NULL, NULL);
    guidata_group_t *grp = (guidata_group_t *)ob->typedata;
    grp->flags   = groupFlags;
    grp->order   = order;
    grp->padding = padding;

    return ob->id;
}

void UIGroup_AddWidget(uiwidget_t *ob, uiwidget_t *other)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);

    guidata_group_t *grp = (guidata_group_t *)ob->typedata;

    if(!other || other == ob)
    {
        DENG2_ASSERT(!"UIGroup::AddWidget: Attempt to add invalid widget");
        return;
    }

    // Ensure widget is not already in this grp.
    for(int i = 0; i < grp->widgetIdCount; ++i)
    {
        if(grp->widgetIds[i] == other->id)
            return; // Already present. Ignore.
    }

    // Must add to this grp.
    grp->widgetIds = (uiwidgetid_t *) M_Realloc(grp->widgetIds, sizeof(*grp->widgetIds) * ++grp->widgetIdCount);
    grp->widgetIds[grp->widgetIdCount - 1] = other->id;
}

int UIGroup_Flags(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);
    return static_cast<guidata_group_t *>(ob->typedata)->flags;
}

void UIGroup_SetFlags(uiwidget_t *ob, int flags)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);
    static_cast<guidata_group_t *>(ob->typedata)->flags = flags;
}

static void applyAlignmentOffset(uiwidget_t *ob, int *x, int *y)
{
    DENG2_ASSERT(ob != 0);

    if(x)
    {
        if(ob->alignFlags & ALIGN_RIGHT)
            *x += UIWidget_MaximumWidth(ob);
        else if(!(ob->alignFlags & ALIGN_LEFT))
            *x += UIWidget_MaximumWidth(ob)/2;
    }
    if(y)
    {
        if(ob->alignFlags & ALIGN_BOTTOM)
            *y += UIWidget_MaximumHeight(ob);
        else if(!(ob->alignFlags & ALIGN_TOP))
            *y += UIWidget_MaximumHeight(ob)/2;
    }
}

static void updateWidgetGeometry(uiwidget_t *ob)
{
    Rect_SetXY(ob->geometry, 0, 0);
    ob->updateGeometry(ob);

    if(Rect_Width(ob->geometry) <= 0 || Rect_Height(ob->geometry) <= 0) return;

    if(ob->alignFlags & ALIGN_RIGHT)
        Rect_SetX(ob->geometry, Rect_X(ob->geometry) - Rect_Width(ob->geometry));
    else if(!(ob->alignFlags & ALIGN_LEFT))
        Rect_SetX(ob->geometry, Rect_X(ob->geometry) - Rect_Width(ob->geometry)/2);

    if(ob->alignFlags & ALIGN_BOTTOM)
        Rect_SetY(ob->geometry, Rect_Y(ob->geometry) - Rect_Height(ob->geometry));
    else if(!(ob->alignFlags & ALIGN_TOP))
        Rect_SetY(ob->geometry, Rect_Y(ob->geometry) - Rect_Height(ob->geometry)/2);
}

void UIGroup_UpdateGeometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);

    guidata_group_t *grp = (guidata_group_t*)ob->typedata;

    Rect_SetWidthHeight(ob->geometry, 0, 0);

    if(!grp->widgetIdCount) return;

    int x = 0, y = 0;
    applyAlignmentOffset(ob, &x, &y);

    for(int i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t *child = GUI_MustFindObjectById(grp->widgetIds[i]);
        Rect const *childGeometry;

        if(UIWidget_MaximumWidth(child) > 0 && UIWidget_MaximumHeight(child) > 0 &&
           UIWidget_Opacity(child) > 0)
        {
            updateWidgetGeometry(child);

            Rect_SetX(child->geometry, Rect_X(child->geometry) + x);
            Rect_SetY(child->geometry, Rect_Y(child->geometry) + y);

            childGeometry = UIWidget_Geometry(child);
            if(Rect_Width(childGeometry) > 0 && Rect_Height(childGeometry) > 0)
            {
                if(grp->order == ORDER_RIGHTTOLEFT)
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                        x -= Rect_Width(childGeometry)  + grp->padding;
                    else
                        y -= Rect_Height(childGeometry) + grp->padding;
                }
                else if(grp->order == ORDER_LEFTTORIGHT)
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                        x += Rect_Width(childGeometry)  + grp->padding;
                    else
                        y += Rect_Height(childGeometry) + grp->padding;
                }

                Rect_Unite(ob->geometry, childGeometry);
            }
        }
    }
}

#if 0 //def DENG2_DEBUG
static void drawWidgetGeometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);

    RectRaw geometry;
    Rect_Raw(ob->geometry, &geometry);

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

static void drawWidgetAvailableSpace(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    DGL_Color4f(0, .4f, 0, .1f);
    DGL_DrawRectf2(Rect_X(ob->geometry), Rect_Y(ob->geometry), ob->maxSize.width, ob->maxSize.height);
}
#endif

static void drawWidget2(uiwidget_t *ob, Point2Raw const *offset)
{
    DENG2_ASSERT(ob != 0);

/*#if _DEBUG
    drawWidgetAvailableSpace(ob);
#endif*/

    if(ob->drawer && ob->opacity > .0001f)
    {
        Point2Raw origin;
        Point2_Raw(Rect_Origin(ob->geometry), &origin);

        // Configure the page render state.
        uiRS.pageAlpha = ob->opacity;

        FR_PushAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin.x, origin.y, 0);

        // Do not pass a zero length offset.
        ob->drawer(ob, ((offset && (offset->x || offset->y))? offset : NULL));

        FR_PopAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin.x, -origin.y, 0);
    }

/*#if _DEBUG
    drawWidgetGeometry(ob);
#endif*/
}

static void drawWidget(uiwidget_t *ob, Point2Raw const *origin)
{
    DENG2_ASSERT(ob != 0);

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    // First we draw ourself.
    drawWidget2(ob, NULL/*no origin offset*/);

    if(ob->type == GUI_GROUP)
    {
        // Now our children.
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            uiwidget_t *child = GUI_MustFindObjectById(grp->widgetIds[i]);
            drawWidget(child, NULL/*no origin offset*/);
        }
    }

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin->x, -origin->y, 0);
    }
}

void GUI_DrawWidget(uiwidget_t *ob, Point2Raw const *offset)
{
    if(!ob) return;
    if(UIWidget_MaximumWidth(ob) < 1 || UIWidget_MaximumHeight(ob) < 1) return;
    if(UIWidget_Opacity(ob) <= 0) return;

    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    updateWidgetGeometry(ob);

    FR_PopAttrib();

    // Draw.
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    // Do not pass a zero length offset.
    drawWidget(ob, ((offset && (offset->x || offset->y))? offset : NULL));

    FR_PopAttrib();
}

void GUI_DrawWidgetXY(uiwidget_t *ob, int x, int y)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    GUI_DrawWidget(ob, &origin);
}

void UIWidget_RunTic(uiwidget_t *ob, timespan_t ticLength)
{
    DENG2_ASSERT(ob != 0);
    switch(ob->type)
    {
    case GUI_GROUP: {
        // First our children then self.
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_RunTic(GUI_MustFindObjectById(grp->widgetIds[i]), ticLength);
        }
        /* Fallthrough*/ }

    default:
        if(ob->ticker)
        {
            ob->ticker(ob, ticLength);
        }
        break;
    }
}

int UIWidget_Player(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->player;
}

Point2 const *UIWidget_Origin(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return Rect_Origin(ob->geometry);
}

Rect const *UIWidget_Geometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->geometry;
}

int UIWidget_MaximumHeight(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->maxSize.height;
}

Size2Raw const *UIWidget_MaximumSize(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return &ob->maxSize;
}

int UIWidget_MaximumWidth(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->maxSize.width;
}

void UIWidget_SetMaximumHeight(uiwidget_t *ob, int height)
{
    DENG2_ASSERT(ob != 0);

    if(ob->maxSize.height == height) return;
    ob->maxSize.height = height;

    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumHeight(GUI_MustFindObjectById(grp->widgetIds[i]), height);
        }
    }
}

void UIWidget_SetMaximumSize(uiwidget_t *ob, Size2Raw const *size)
{
    DENG2_ASSERT(ob != 0);
    if(ob->maxSize.width == size->width &&
       ob->maxSize.height == size->height) return;
    ob->maxSize.width = size->width;
    ob->maxSize.height = size->height;

    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumSize(GUI_MustFindObjectById(grp->widgetIds[i]), size);
        }
    }
}

void UIWidget_SetMaximumWidth(uiwidget_t *ob, int width)
{
    DENG2_ASSERT(ob != 0);
    if(ob->maxSize.width == width) return;
    ob->maxSize.width = width;

    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumWidth(GUI_MustFindObjectById(grp->widgetIds[i]), width);
        }
    }
}

int UIWidget_Alignment(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->alignFlags;
}

void UIWidget_SetAlignment(uiwidget_t *ob, int alignFlags)
{
    DENG2_ASSERT(ob != 0);
    ob->alignFlags = alignFlags;
}

float UIWidget_Opacity(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->opacity;
}

void UIWidget_SetOpacity(uiwidget_t *ob, float opacity)
{
    DENG2_ASSERT(ob != 0);
    ob->opacity = opacity;
    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            uiwidget_t *child = GUI_MustFindObjectById(grp->widgetIds[i]);
            UIWidget_SetOpacity(child, opacity);
        }
    }
}

namespace common {
namespace menu {

// Menu (page) render state.
static mn_rendstate_t rs;
mn_rendstate_t const *mnRendState = &rs;

Widget *MN_MustFindObjectOnPage(Page *page, int group, int flags)
{
    DENG2_ASSERT(page != 0);
    Widget *ob = page->findObject(group, flags);
    if(!ob) Con_Error("MN_MustFindObjectOnPage: Failed to locate object in group #%i with flags %i on page %p.", group, flags, page);
    return ob;
}

short MN_MergeMenuEffectWithDrawTextFlags(short f)
{
    return ((~cfg.menuEffectFlags & DTF_NO_EFFECTS) | (f & ~DTF_NO_EFFECTS));
}

static void MN_DrawObject(Widget *wi, Point2Raw const *offset)
{
    if(!wi) return;
    wi->draw(offset);
}

static void setupRenderStateForPageDrawing(Page *page, float alpha)
{
    if(!page) return; // Huh?

    rs.pageAlpha   = alpha;
    rs.textGlitter = cfg.menuTextGlitter;
    rs.textShadow  = cfg.menuShadow;

    for(int i = 0; i < MENU_FONT_COUNT; ++i)
    {
        rs.textFonts[i] = page->predefinedFont(mn_page_fontid_t(i));
    }
    for(int i = 0; i < MENU_COLOR_COUNT; ++i)
    {
        page->predefinedColor(mn_page_colorid_t(i), rs.textColors[i]);
        rs.textColors[i][CA] = alpha; // For convenience.
    }

    // Configure the font renderer (assume state has already been pushed if necessary).
    FR_SetFont(rs.textFonts[0]);
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetShadowStrength(rs.textShadow);
    FR_SetGlitterStrength(rs.textGlitter);
}

static void updatePageObjectGeometries(Page *page)
{
    if(!page) return;

    // Update objects.
    foreach(Widget *wi, page->widgets())
    {
        FR_PushAttrib();
        Rect_SetXY(wi->_geometry, 0, 0);
        wi->updateGeometry(page);
        FR_PopAttrib();
    }
}

/// @return  @c true iff this object is drawable (potentially visible).
dd_bool MNObject_IsDrawable(Widget *wi)
{
    return !(wi->flags() & MNF_HIDDEN);
}

int Page::lineHeight(int *lineOffset)
{
    fontid_t oldFont = FR_Font();

    /// @kludge We cannot yet query line height from the font...
    FR_SetFont(predefinedFont(MENU_FONT1));
    int lh = FR_TextHeight("{case}WyQ");
    if(lineOffset)
    {
        *lineOffset = de::max(1.f, .5f + lh * .34f);
    }

    // Restore the old font.
    FR_SetFont(oldFont);

    return lh;
}

void Page::applyPageLayout()
{
    Rect_SetXY(geometry, 0, 0);
    Rect_SetWidthHeight(geometry, 0, 0);

    // Apply layout logic to this page.

    if(flags & MPF_LAYOUT_FIXED)
    {
        // This page uses a fixed layout.
        foreach(Widget *wi, _widgets)
        {
            if(!MNObject_IsDrawable(wi)) continue;

            Rect_SetXY(wi->_geometry, wi->_origin.x, wi->_origin.y);
            Rect_Unite(geometry, wi->_geometry);
        }
        return;
    }

    // This page uses a dynamic layout.
    int lineOffset;
    int lh = lineHeight(&lineOffset);

    Point2Raw origin;

    for(int i = 0; i < _widgets.count(); )
    {
        Widget *ob = _widgets[i];
        Widget *nextOb = i + 1 < _widgets.count()? _widgets[i + 1] : 0;

        if(!MNObject_IsDrawable(ob))
        {
            // Proceed to the next object!
            i += 1;
            continue;
        }

        // If the object has a fixed position, we will ignore it while doing
        // dynamic layout.
        if(ob->flags() & MNF_POSITION_FIXED)
        {
            Rect_SetXY(ob->_geometry, ob->_origin.x, ob->_origin.y);
            Rect_Unite(geometry, ob->_geometry);

            // To the next object.
            i += 1;
            continue;
        }

        // An additional offset requested?
        if(ob->flags() & MNF_LAYOUT_OFFSET)
        {
            origin.x += ob->_origin.x;
            origin.y += ob->_origin.y;
        }

        Rect_SetXY(ob->_geometry, origin.x, origin.y);

        // Orient label plus button/inline-list/textual-slider pairs about a
        // vertical dividing line, with the label on the left, other object
        // on the right.
        // @todo Do not assume pairing, an object should designate it's pair.
        if(ob->is<LabelWidget>() && nextOb)
        {
            if(MNObject_IsDrawable(nextOb) &&
               (nextOb->is<ButtonWidget>()     ||
                nextOb->is<InlineListWidget>() ||
                nextOb->is<ColorPreviewWidget>()   ||
                nextOb->is<InputBindingWidget>()   ||
                nextOb->is<TextualSliderWidget>()))
            {
                int const margin = lineOffset * 2;

                Rect_SetXY(nextOb->_geometry, margin + Rect_Width(ob->_geometry), origin.y);

                RectRaw united;
                origin.y += Rect_United(ob->_geometry, nextOb->_geometry, &united)
                          ->size.height + lineOffset;

                Rect_UniteRaw(geometry, &united);

                // Extra spacing between object groups.
                if(i + 2 < _widgets.count() && nextOb->_group != _widgets[i + 2]->_group)
                {
                    origin.y += lh;
                }

                // Proceed to the next object!
                i += 2;
                continue;
            }
        }

        Rect_Unite(geometry, ob->_geometry);

        origin.y += Rect_Height(ob->_geometry) + lineOffset;

        // Extra spacing between object groups.
        if(nextOb && nextOb->_group != ob->_group)
            origin.y += lh;

        // Proceed to the next object!
        i += 1;
    }
}

void Page::setOnActiveCallback(Page::OnActiveCallback newCallback)
{
    onActiveCallback = newCallback;
}

#if __JDOOM__ || __JDOOM64__
static void composeSubpageString(Page *page, char *buf, size_t bufSize)
{
    DENG2_ASSERT(page != 0);
    DENG2_UNUSED(page);
    if(!buf || 0 == bufSize) return;
    dd_snprintf(buf, bufSize, "Page %i/%i", 0, 0);
}
#endif

static void drawPageNavigation(Page *page, int x, int y)
{
    int const currentPage = 0;//(page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1;
    int const totalPages  = 1;//(int)ceil((float)page->objectsCount/page->numVisObjects);
#if __JDOOM__ || __JDOOM64__
    char buf[1024];

    DENG2_UNUSED(currentPage);
#endif

    if(!page || totalPages <= 1) return;

#if __JDOOM__ || __JDOOM64__
    composeSubpageString(page, buf, 1024);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorv(cfg.menuTextColors[1]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(buf, x, y, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    GL_DrawPatchXY2( pInvPageLeft[currentPage == 0 || (menuTime & 8)], x - 144, y, ALIGN_RIGHT);
    GL_DrawPatchXY2(pInvPageRight[currentPage == totalPages-1 || (menuTime & 8)], x + 144, y, ALIGN_LEFT);

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

static void drawPageHeading(Page *page, Point2Raw const *offset)
{
    Point2Raw origin;

    if(!page) return;

    /// @kludge no title = no heading.
    if(Str_IsEmpty(&page->title)) return;
    // kludge end.

    origin.x = SCREENWIDTH/2;
    origin.y = (SCREENHEIGHT/2) - ((SCREENHEIGHT/2-5)/cfg.menuScale);
    if(offset)
    {
        origin.x += offset->x;
        origin.y += offset->y;
    }

    FR_PushAttrib();
    Hu_MenuDrawPageTitle(Str_Text(&page->title), origin.x, origin.y); origin.y += 16;
    drawPageNavigation(page, origin.x, origin.y);
    FR_PopAttrib();
}

void MN_DrawPage(Page *page, float alpha, dd_bool showFocusCursor)
{
    if(!page) return;

    alpha = de::clamp(0.f, alpha, 1.f);
    if(alpha <= .0001f) return;

    int focusObjHeight = 0;
    Point2Raw cursorOrigin;

    // Object geometry is determined from properties defined in the
    // render state, so configure render state before we begin.
    setupRenderStateForPageDrawing(page, alpha);

    // Update object geometry. We'll push the font renderer state because
    // updating geometry may require changing the current values.
    FR_PushAttrib();
    updatePageObjectGeometries(page);

    // Back to default page render state.
    FR_PopAttrib();

    // We can now layout the widgets of this page.
    page->applyPageLayout();

    // Determine the origin of the focus object (this dictates the page scroll location).
    Widget *focusObj = page->focusObject();
    if(focusObj && !MNObject_IsDrawable(focusObj))
    {
        focusObj = 0;
    }

    // Are we focusing?
    if(focusObj)
    {
        focusObjHeight = page->cursorSize();

        // Determine the origin and dimensions of the cursor.
        /// @todo Each object should define a focus origin...
        cursorOrigin.x = -1;
        cursorOrigin.y = Point2_Y(focusObj->origin());

        /// @kludge
        /// We cannot yet query the subobjects of the list for these values
        /// so we must calculate them ourselves, here.
        if(focusObj->flags() & MNF_ACTIVE)
        {
            if(ListWidget const *list = focusObj->maybeAs<ListWidget>())
            {
                if(list->selectionIsVisible())
                {
                    FR_PushAttrib();
                    FR_SetFont(page->predefinedFont(mn_page_fontid_t(focusObj->font())));
                    focusObjHeight = FR_CharHeight('A') * (1+MNDATA_LIST_LEADING);
                    cursorOrigin.y += (list->_selection - list->first) * focusObjHeight;
                    FR_PopAttrib();
                }
            }
        }
        // kludge end
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(page->origin.x, page->origin.y, 0);

    // Apply page scroll?
    if(!(page->flags & MPF_NEVER_SCROLL) && focusObj)
    {
        RectRaw pageGeometry, viewRegion;
        Rect_Raw(page->geometry, &pageGeometry);

        // Determine available screen region for the page.
        viewRegion.origin.x = 0;
        viewRegion.origin.y = page->origin.y;
        viewRegion.size.width  = SCREENWIDTH;
        viewRegion.size.height = SCREENHEIGHT - 40/*arbitrary but enough for the help message*/;

        // Is scrolling in effect?
        if(pageGeometry.size.height > viewRegion.size.height)
        {
            int const minY = -viewRegion.origin.y/2 + viewRegion.size.height/2;
            if(cursorOrigin.y > minY)
            {
                int const scrollLimitY = pageGeometry.size.height - viewRegion.size.height/2;
                int const scrollOriginY = MIN_OF(cursorOrigin.y, scrollLimitY) - minY;
                DGL_Translatef(0, -scrollOriginY, 0);
            }
        }
    }

    // Draw child objects.
    foreach(Widget *wi, page->widgets())
    {
        RectRaw geometry;

        if(wi->flags() & MNF_HIDDEN) continue;

        Rect_Raw(wi->geometry(), &geometry);

        FR_PushAttrib();
        MN_DrawObject(wi, &geometry.origin);
        FR_PopAttrib();
    }

    // How about a focus cursor?
    /// @todo cursor should be drawn on top of the page drawer.
    if(showFocusCursor && focusObj)
    {
        Hu_MenuDrawFocusCursor(cursorOrigin.x, cursorOrigin.y, focusObjHeight, alpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    drawPageHeading(page, NULL/*no offset*/);

    // The page has its own drawer.
    if(page->drawer)
    {
        FR_PushAttrib();
        page->drawer(page, &page->origin);
        FR_PopAttrib();
    }
}

Page::Page(Point2Raw const &origin, int flags,
    void (*ticker) (Page *page),
    void (*drawer) (Page *page, Point2Raw const *origin),
    int (*cmdResponder) (Page *page, menucommand_e cmd),
    void *userData)
    : origin          (origin)
    , geometry        (Rect_New())
    , previous        (0)
    , focus           (-1) /// @todo Make this a page flag.
    , flags           (flags)
    , ticker          (ticker)
    , drawer          (drawer)
    , cmdResponder    (cmdResponder)
    , onActiveCallback(0)
    , userData        (userData)
{
    Str_InitStd(&title);

    fontid_t fontId = FID(GF_FONTA);
    for(int i = 0; i < MENU_FONT_COUNT; ++i)
    {
        fonts[i] = fontId;
    }

    de::zap(colors);
    colors[1] = 1;
    colors[2] = 2;
}

Page::~Page()
{
    Str_Free(&title);
    if(geometry) Rect_Delete(geometry);

    foreach(Widget *wi, _widgets)
    {
        if(wi->_geometry) Rect_Delete(wi->_geometry);
    }
    qDeleteAll(_widgets);
}

Page::Widgets const &Page::widgets() const
{
    return _widgets;
}

void Page::setTitle(char const *newTitle)
{
    Str_Set(&title, newTitle? newTitle : "");
}

void Page::setX(int x)
{
    origin.x = x;
}

void Page::setY(int y)
{
    origin.y = y;
}

void Page::setPreviousPage(Page *newPrevious)
{
    previous = newPrevious;
}

Widget *Page::focusObject()
{
    if(_widgets.isEmpty() || focus < 0) return 0;
    return _widgets[focus];
}

void Page::clearFocusObject()
{
    if(focus >= 0)
    {
        Widget *wi = _widgets[focus];
        if(wi->flags() & MNF_ACTIVE)
        {
            return;
        }
    }
    focus = -1;
    foreach(Widget *wi, _widgets)
    {
        wi->setFlags(FO_CLEAR, MNF_FOCUS);
    }
    refocus();
}

int Page::cursorSize()
{
    Widget *focusOb = focusObject();
    int focusObHeight = focusOb? Size2_Height(focusOb->size()) : 0;

    // Ensure the cursor is at least as tall as the effective line height for
    // the page. This is necessary because some mods replace the menu button
    // graphics with empty and/or tiny images (e.g., Hell Revealed 2).
    /// @note Handling this correctly would mean separate physical/visual
    /// geometries for menu widgets.
    return de::max(focusObHeight, lineHeight());
}

Widget *Page::findObject(int group, int flags)
{
    foreach(Widget *wi, _widgets)
    {
        if(wi->isGroupMember(group) && (wi->flags() & flags) == flags)
            return wi;
    }
    return 0; // Not found.
}

int Page::findObjectIndex(Widget *wi)
{
    return _widgets.indexOf(wi);
}

/// @pre @a ob is a child of @a page.
void Page::giveChildFocus(Widget *wi, dd_bool allowRefocus)
{
    DENG2_ASSERT(wi != 0);

    if(!(0 > focus))
    {
        if(wi != _widgets[focus])
        {
            Widget *oldFocusOb = _widgets[focus];
            if(oldFocusOb->hasAction(Widget::MNA_FOCUSOUT))
            {
                oldFocusOb->execAction(Widget::MNA_FOCUSOUT, NULL);
            }
            oldFocusOb->setFlags(FO_CLEAR, MNF_FOCUS);
        }
        else if(!allowRefocus)
        {
            return;
        }
    }

    focus = _widgets.indexOf(wi);
    wi->setFlags(FO_SET, MNF_FOCUS);
    if(wi->hasAction(Widget::MNA_FOCUS))
    {
        wi->execAction(Widget::MNA_FOCUS, NULL);
    }
}

void Page::setFocus(Widget *wi)
{
    int index = findObjectIndex(wi);
    if(index < 0)
    {
        DENG2_ASSERT(!"Page::Focus: Failed to determine index-in-page for widget.");
        return;
    }
    giveChildFocus(_widgets[index], false);
}

void Page::refocus()
{
    // If we haven't yet visited this page then find the first focusable
    // object and select it.
    if(0 > focus)
    {
        int i, giveFocus = -1;

        // First look for a default focus object. There should only be one
        // but find the last with this flag...
        for(i = 0; i < _widgets.count(); ++i)
        {
            Widget *ob = _widgets[i];
            if((ob->flags() & MNF_DEFAULT) && !(ob->flags() & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
            }
        }

        // No default focus? Find the first focusable object.
        if(-1 == giveFocus)
        for(i = 0; i < _widgets.count(); ++i)
        {
            Widget *ob = _widgets[i];
            if(!(ob->flags() & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
                break;
            }
        }

        if(-1 != giveFocus)
        {
            giveChildFocus(_widgets[giveFocus], false);
        }
        else
        {
            LOGDEV_WARNING("Page::refocus: No focusable object on page");
        }
    }
    else
    {
        // We've been here before; re-focus on the last focused object.
        giveChildFocus(_widgets[focus], true);
    }
}

void Page::initialize()
{
    // Reset page timer.
    _timer = 0;

    // (Re)init objects.
    foreach(Widget *wi, _widgets)
    {
        // Reset object timer.
        wi->timer = 0;

        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            dd_bool const activate = (*(char *) wi->data1);
            tog->setFlags((activate? FO_SET:FO_CLEAR), MNF_ACTIVE);
        }
        if(ListWidget *list = wi->maybeAs<ListWidget>())
        {
            // Determine number of potentially visible items.
            list->numvis = list->itemCount();
            if(list->_selection >= 0)
            {
                if(list->_selection < list->first)
                    list->first = list->_selection;
                if(list->_selection > list->first + list->numvis - 1)
                    list->first = list->_selection - list->numvis + 1;
            }
        }
    }

    if(_widgets.isEmpty())
    {
        // Presumably the widgets will be added later...
        return;
    }

    refocus();

    if(onActiveCallback)
    {
        onActiveCallback(this);
    }
}

void Page::initObjects()
{
    foreach(Widget *wi, _widgets)
    {
        wi->_page     = this;
        wi->_geometry = Rect_New();

        wi->timer = 0;
        wi->setFlags(FO_CLEAR, MNF_FOCUS);

        if(0 != wi->_shortcut)
        {
            int shortcut = wi->_shortcut;
            wi->_shortcut = 0; // Clear invalid defaults.
            wi->setShortcut(shortcut);
        }

        if(LabelWidget *txt = wi->maybeAs<LabelWidget>())
        {
            wi->setFlags(FO_SET, MNF_NO_FOCUS);

            if(txt->text && (PTR2INT(txt->text) > 0 && PTR2INT(txt->text) < NUMTEXT))
            {
                txt->text = GET_TXT(PTR2INT(txt->text));
            }
        }
        if(LineEditWidget *edit = wi->maybeAs<LineEditWidget>())
        {
            if(edit->emptyString && (PTR2INT(edit->emptyString) > 0 && PTR2INT(edit->emptyString) < NUMTEXT))
            {
                edit->emptyString = GET_TXT(PTR2INT(edit->emptyString));
            }
        }
        if(ListWidget *list = wi->maybeAs<ListWidget>())
        {
            foreach(mndata_listitem_t *item, list->items())
            {
                if(item->text && (PTR2INT(item->text) > 0 && PTR2INT(item->text) < NUMTEXT))
                {
                    item->text = GET_TXT(PTR2INT(item->text));
                }
            }
        }
        if(ColorPreviewWidget *cbox = wi->maybeAs<ColorPreviewWidget>())
        {
            if(!cbox->_rgbaMode)
                cbox->_a = 1.f;
            if(0 >= cbox->width)
                cbox->width = MNDATA_COLORBOX_WIDTH;
            if(0 >= cbox->height)
                cbox->height = MNDATA_COLORBOX_HEIGHT;
        }
        if(MobjPreviewWidget *mprev = wi->maybeAs<MobjPreviewWidget>())
        {
            mprev->setFlags(FO_SET, MNF_NO_FOCUS);
        }
    }
}

/// Main task is to update objects linked to cvars.
void Page::updateObjects()
{
    foreach(Widget *wi, _widgets)
    {
        if(LabelWidget *text = wi->maybeAs<LabelWidget>())
        {
            text->setFlags(FO_SET, MNF_NO_FOCUS);
        }
        if(MobjPreviewWidget *mprev = wi->maybeAs<MobjPreviewWidget>())
        {
            mprev->setFlags(FO_SET, MNF_NO_FOCUS);
        }
        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            if(wi->data1)
            {
                // This button has already been initialized.
                cvarbutton_t *cvb = (cvarbutton_t *) wi->data1;
                cvb->active = (Con_GetByte(cvb->cvarname) & (cvb->mask? cvb->mask : ~0)) != 0;
                tog->setText(cvb->active ? cvb->yes : cvb->no);
                continue;
            }

            // Find the cvarbutton representing this one.
            for(cvarbutton_t *cvb = mnCVarButtons; cvb->cvarname; cvb++)
            {
                if(!strcmp(tog->cvarPath(), cvb->cvarname) && wi->data2 == cvb->mask)
                {
                    cvb->active = (Con_GetByte(cvb->cvarname) & (cvb->mask? cvb->mask : ~0)) != 0;
                    wi->data1 = (void *) cvb;
                    tog->setText(cvb->active ? cvb->yes : cvb->no);
                    break;
                }
            }
        }
        if(ListWidget *list = wi->maybeAs<ListWidget>())
        {
            Widget::mn_actioninfo_t const *action = wi->action(Widget::MNA_MODIFIED);

            if(action && action->callback == CvarListWidget_UpdateCvar)
            {
                list->selectItemByValue(MNLIST_SIF_NO_ACTION, Con_GetInteger((char const *)list->data));
            }
        }
        if(LineEditWidget *edit = wi->maybeAs<LineEditWidget>())
        {
            Widget::mn_actioninfo_t const *action = wi->action(Widget::MNA_MODIFIED);

            if(action && action->callback == CvarLineEditWidget_UpdateCvar)
            {
                edit->setText(MNEDIT_STF_NO_ACTION, Con_GetString((char const *)edit->data1));
            }
        }
        if(SliderWidget *sldr = wi->maybeAs<SliderWidget>())
        {
            Widget::mn_actioninfo_t const *action = wi->action(Widget::MNA_MODIFIED);

            if(action && action->callback == CvarSliderWidget_UpdateCvar)
            {
                float value;
                if(sldr->floatMode)
                    value = Con_GetFloat((char const *)sldr->data1);
                else
                    value = Con_GetInteger((char const *)sldr->data1);
                sldr->setValue(MNSLIDER_SVF_NO_ACTION, value);
            }
        }
        if(ColorPreviewWidget *cbox = wi->maybeAs<ColorPreviewWidget>())
        {
            Widget::mn_actioninfo_t const *action = wi->action(Widget::MNA_MODIFIED);

            if(action && action->callback == CvarColorPreviewWidget_UpdateCvar)
            {
                float rgba[4];
                rgba[CR] = Con_GetFloat((char const *)cbox->data1);
                rgba[CG] = Con_GetFloat((char const *)cbox->data2);
                rgba[CB] = Con_GetFloat((char const *)cbox->data3);
                rgba[CA] = (cbox->rgbaMode()? Con_GetFloat((char const *)cbox->data4) : 1.f);
                cbox->setColor4fv(MNCOLORBOX_SCF_NO_ACTION, rgba);
            }
        }
    }
}

void Page::tick()
{
    // Call the ticker of each object, unless they're hidden or paused.
    foreach(Widget *wi, _widgets)
    {
        if((wi->flags() & MNF_PAUSED) || (wi->flags() & MNF_HIDDEN))
            continue;

        wi->tick();

        // Advance object timer.
        wi->timer++;
    }

    _timer++;
}

fontid_t Page::predefinedFont(mn_page_fontid_t id)
{
    DENG2_ASSERT(VALID_MNPAGE_FONTID(id));
    return fonts[id];
}

void Page::setPredefinedFont(mn_page_fontid_t id, fontid_t fontId)
{
    DENG2_ASSERT(VALID_MNPAGE_FONTID(id));
    fonts[id] = fontId;
}

void Page::predefinedColor(mn_page_colorid_t id, float rgb[3])
{
    DENG2_ASSERT(rgb != 0);
    DENG2_ASSERT(VALID_MNPAGE_COLORID(id));
    uint colorIndex = colors[id];
    rgb[CR] = cfg.menuTextColors[colorIndex][CR];
    rgb[CG] = cfg.menuTextColors[colorIndex][CG];
    rgb[CB] = cfg.menuTextColors[colorIndex][CB];
}

int Page::timer()
{
    return _timer;
}

Widget::Widget()
    : _group             (0)
    , _flags             (0)
    , _shortcut          (0)
    , _pageFontIdx       (0)
    , _pageColorIdx      (0)
    , onTickCallback     (0)
    , cmdResponder       (0)
    , data1              (0)
    , data2              (0)
    , _geometry          (0)
    , _page              (0)
    , timer              (0)
{
    de::zap(actions);
}

int Widget::handleEvent(event_t * /*ev*/)
{
    return 0; // Not handled.
}

int Widget::handleEvent_Privileged(event_t * /*ev*/)
{
    return 0; // Not handled.
}

void Widget::tick()
{
    if(onTickCallback)
    {
        onTickCallback(this);
    }
}

Page *Widget::page() const
{
    return _page;
}

int Widget::flags() const
{
    return _flags;
}

Rect const *Widget::geometry() const
{
    return _geometry;
}

Point2Raw const *Widget::fixedOrigin() const
{
    return &_origin;
}

Widget &Widget::setFixedOrigin(Point2Raw const *newOrigin)
{
    if(newOrigin)
    {
        _origin.x = newOrigin->x;
        _origin.y = newOrigin->y;
    }
    return *this;
}

Widget &Widget::setFixedX(int newX)
{
    _origin.x = newX;
    return *this;
}

Widget &Widget::setFixedY(int newY)
{
    _origin.y = newY;
    return *this;
}

Widget &Widget::setFlags(flagop_t op, int flagsToChange)
{
    switch(op)
    {
    case FO_CLEAR:  _flags &= ~flagsToChange;  break;
    case FO_SET:    _flags |= flagsToChange;   break;
    case FO_TOGGLE: _flags ^= flagsToChange;   break;

    default: DENG2_ASSERT(!"MNObject::SetFlags: Unknown op.");
    }
    return *this;
}

int Widget::shortcut()
{
    return _shortcut;
}

Widget &Widget::setShortcut(int ddkey)
{
    if(isalnum(ddkey))
    {
        _shortcut = tolower(ddkey);
    }
    return *this;
}

int Widget::font()
{
    return _pageFontIdx;
}

int Widget::color()
{
    return _pageColorIdx;
}

dd_bool Widget::isGroupMember(int group) const
{
    return (_group == group);
}

int Widget_DefaultCommandResponder(Widget *ob, menucommand_e cmd)
{
    DENG2_ASSERT(ob);
    if(MCMD_SELECT == cmd && (ob->_flags & MNF_FOCUS) && !(ob->_flags & MNF_DISABLED))
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!(ob->_flags & MNF_ACTIVE))
        {
            ob->_flags |= MNF_ACTIVE;
            if(ob->hasAction(Widget::MNA_ACTIVE))
            {
                ob->execAction(Widget::MNA_ACTIVE, NULL);
            }
        }

        ob->_flags &= ~MNF_ACTIVE;
        if(ob->hasAction(Widget::MNA_ACTIVEOUT))
        {
            ob->execAction(Widget::MNA_ACTIVEOUT, NULL);
        }
        return true;
    }
    return false; // Not eaten.
}

Widget::mn_actioninfo_t const *Widget::action(mn_actionid_t id)
{
    DENG2_ASSERT((id) >= MNACTION_FIRST && (id) <= MNACTION_LAST);
    return &actions[id];
}

dd_bool Widget::hasAction(mn_actionid_t id)
{
    mn_actioninfo_t const *info = action(id);
    return (info && info->callback != 0);
}

int Widget::execAction(mn_actionid_t id, void *parameters)
{
    if(hasAction(id))
    {
        return action(id)->callback(this, id, parameters);
    }
    DENG2_ASSERT(!"MNObject::ExecAction: Attempt to execute non-existent action.");
    /// @todo Need an error handling mechanic.
    return -1; // NOP
}

RectWidget::RectWidget()
    : Widget()
    , patch      (0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
}

void RectWidget::draw(Point2Raw const *origin)
{
    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    if(patch)
    {
        DGL_SetPatch(patch, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_Enable(DGL_TEXTURE_2D);
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    DGL_DrawRect2(0, 0, dimensions.width, dimensions.height);

    if(patch)
    {
        DGL_Disable(DGL_TEXTURE_2D);
    }

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin->x, -origin->y, 0);
    }
}

void RectWidget::updateGeometry(Page * /*page*/)
{
    if(dimensions.width == 0 && dimensions.height == 0)
    {
        // Inherit dimensions from the patch.
        patchinfo_t info;
        if(R_GetPatchInfo(patch, &info))
        {
            std::memcpy(&dimensions, &info.geometry.size, sizeof(dimensions));
        }
    }
    Rect_SetWidthHeight(_geometry, dimensions.width, dimensions.height);
}

void RectWidget::setBackgroundPatch(patchid_t newBackgroundPatch)
{
    patch = newBackgroundPatch;
}

LabelWidget::LabelWidget()
    : Widget()
    , text (0)
    , patch(0)
    , flags(0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
}

void LabelWidget::draw(Point2Raw const *origin)
{
    fontid_t fontId = rs.textFonts[_pageFontIdx];
    float color[4], t = (Widget::_flags & MNF_FOCUS)? 1 : 0;

    // Flash if focused?
    if((Widget::_flags & MNF_FOCUS) && cfg.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(_page->timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    lerpColor(color, rs.textColors[_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    color[CA] = rs.textColors[_pageColorIdx][CA];

    DGL_Color4f(1, 1, 1, color[CA]);
    FR_SetFont(fontId);
    FR_SetColorAndAlphav(color);

    if(patch)
    {
        char const *replacement = 0;
        if(!(flags & MNTEXT_NO_ALTTEXT))
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), *patch, text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(*patch, replacement, de::Vector2i(origin->x, origin->y),
                     ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(text, origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

void LabelWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);

    Size2Raw size;
    /// @todo What if patch replacement is disabled?
    if(patch != 0)
    {
        patchinfo_t info;
        R_GetPatchInfo(*patch, &info);
        Rect_SetWidthHeight(_geometry, info.geometry.size.width, info.geometry.size.height);
        return;
    }
    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));
    FR_TextSize(&size, text);
    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

void LabelWidget_SetFlags(Widget *wi, flagop_t op, int flags)
{
    DENG2_ASSERT(wi != 0);
    LabelWidget *txt = static_cast<LabelWidget *>(wi);

    switch(op)
    {
    case FO_CLEAR:  txt->flags &= ~flags;  break;
    case FO_SET:    txt->flags |= flags;   break;
    case FO_TOGGLE: txt->flags ^= flags;   break;

    default: DENG2_ASSERT(!"LabelWidget::SetFlags: Unknown op.");
    }
}

LineEditWidget::LineEditWidget()
    : Widget()
    , _maxLength     (0)
    , maxVisibleChars(0)
    , emptyString    (0)
    , data1          (0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
    Widget::cmdResponder  = LineEditWidget_CommandResponder;

    Str_InitStd(&_text);
    Str_InitStd(&oldtext);
}

LineEditWidget::~LineEditWidget()
{
    Str_Free(&_text);
    Str_Free(&oldtext);
}

void LineEditWidget::loadResources() // static
{
#if defined(MNDATA_EDIT_BACKGROUND_PATCH_LEFT)
    pEditLeft   = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_LEFT);
#else
    pEditLeft   = 0;
#endif
#if defined(MNDATA_EDIT_BACKGROUND_PATCH_RIGHT)
    pEditRight  = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_RIGHT);
#else
    pEditRight  = 0;
#endif
    pEditMiddle = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE);
}

static void drawEditBackground(Widget const *wi, int x, int y, int width, float alpha)
{
    DENG2_UNUSED(wi);

    patchinfo_t leftInfo, rightInfo, middleInfo;
    int leftOffset = 0, rightOffset = 0;

    DGL_Color4f(1, 1, 1, alpha);

    if(R_GetPatchInfo(pEditLeft, &leftInfo))
    {
        DGL_SetPatch(pEditLeft, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x, y, leftInfo.geometry.size.width, leftInfo.geometry.size.height);
        leftOffset = leftInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditRight, &rightInfo))
    {
        DGL_SetPatch(pEditRight, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + width - rightInfo.geometry.size.width, y, rightInfo.geometry.size.width, rightInfo.geometry.size.height);
        rightOffset = rightInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        DGL_SetPatch(pEditMiddle, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x + leftOffset, y, width - leftOffset - rightOffset, middleInfo.geometry.size.height, middleInfo.geometry.size.width, middleInfo.geometry.size.height);
    }
}

void LineEditWidget::draw(Point2Raw const *_origin)
{
    DENG2_ASSERT(_origin != 0);

    fontid_t fontId = rs.textFonts[_pageFontIdx];

    Point2Raw origin(_origin->x + MNDATA_EDIT_OFFSET_X, _origin->y + MNDATA_EDIT_OFFSET_Y);

    char const *string = 0;
    float light = 1, textAlpha = rs.pageAlpha;
    if(!Str_IsEmpty(&_text))
    {
        string = Str_Text(&_text);
    }
    else if(!((Widget::_flags & MNF_ACTIVE) && (Widget::_flags & MNF_FOCUS)))
    {
        string = emptyString;
        light *= .5f;
        textAlpha = rs.pageAlpha * .75f;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(fontId);

    uint numVisCharacters = string? strlen(string) : 0;
    if(maxVisibleChars > 0 && maxVisibleChars < numVisCharacters)
    {
        numVisCharacters = maxVisibleChars;
    }

    drawEditBackground(this, origin.x + MNDATA_EDIT_BACKGROUND_OFFSET_X,
                             origin.y + MNDATA_EDIT_BACKGROUND_OFFSET_Y,
                       Rect_Width(_geometry), rs.pageAlpha);

    //if(string)
    {
        float color[4], t = 0;

        // Flash if focused?
        if(!(Widget::_flags & MNF_ACTIVE) && (Widget::_flags & MNF_FOCUS) && cfg.menuTextFlashSpeed > 0)
        {
            float const speed = cfg.menuTextFlashSpeed / 2.f;
            t = (1 + sin(_page->timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
        }

        lerpColor(color, cfg.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX], cfg.menuTextFlashColor, t, false/*rgb mode*/);
        color[CA] = textAlpha;

        // Light the text.
        color[CR] *= light; color[CG] *= light; color[CB] *= light;

        // Draw the text:
        FR_SetColorAndAlphav(color);
        FR_DrawText3(string, &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

        // Are we drawing a cursor?
        if((Widget::_flags & MNF_ACTIVE) && (Widget::_flags & MNF_FOCUS) && (menuTime & 8) &&
           (!_maxLength || (unsigned)Str_Length(&_text) < _maxLength))
        {
            origin.x += FR_TextWidth(string);
            FR_DrawChar3('_', &origin, ALIGN_TOPLEFT,  MN_MergeMenuEffectWithDrawTextFlags(0));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int LineEditWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    LineEditWidget *edit = static_cast<LineEditWidget *>(wi);

    if(cmd == MCMD_SELECT)
    {
        if(!(wi->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            wi->_flags |= MNF_ACTIVE;
            wi->timer = 0;
            // Store a copy of the present text value so we can restore it.
            Str_Copy(&edit->oldtext, &edit->_text);
            if(wi->hasAction(Widget::MNA_ACTIVE))
            {
                wi->execAction(Widget::MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            Str_Copy(&edit->oldtext, &edit->_text);
            wi->_flags &= ~MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVEOUT))
            {
                wi->execAction(Widget::MNA_ACTIVEOUT, NULL);
            }
        }
        return true;
    }

    if(wi->_flags & MNF_ACTIVE)
    {
        switch(cmd)
        {
        case MCMD_NAV_OUT:
            Str_Copy(&edit->_text, &edit->oldtext);
            wi->_flags &= ~MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_CLOSE))
            {
                wi->execAction(Widget::MNA_CLOSE, NULL);
            }
            return true;

        // Eat all other navigation commands, when active.
        case MCMD_NAV_LEFT:
        case MCMD_NAV_RIGHT:
        case MCMD_NAV_DOWN:
        case MCMD_NAV_UP:
        case MCMD_NAV_PAGEDOWN:
        case MCMD_NAV_PAGEUP:
            return true;

        default: break;
        }
    }

    return false; // Not eaten.
}

uint LineEditWidget::maxLength() const
{
    return _maxLength;
}

void LineEditWidget::setMaxLength(uint newMaxLength)
{
    if(newMaxLength < _maxLength)
    {
        Str_Truncate(&_text, newMaxLength);
        Str_Truncate(&oldtext, newMaxLength);
    }
    _maxLength = newMaxLength;
}

ddstring_t const *LineEditWidget::text() const
{
    return &_text;
}

void LineEditWidget::setText(int flags, char const *newText)
{
    if(!_maxLength)
    {
        Str_Set(&_text, newText);
    }
    else
    {
        Str_Clear(&_text);
        Str_PartAppend(&_text, newText, 0, _maxLength);
    }

    if(flags & MNEDIT_STF_REPLACEOLD)
    {
        Str_Copy(&oldtext, &_text);
    }
    if(!(flags & MNEDIT_STF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED, NULL);
    }
}

/**
 * Responds to alphanumeric input for edit fields.
 */
int LineEditWidget::handleEvent(event_t *ev)
{
    DENG2_ASSERT(ev != 0);

    if(!(Widget::_flags & MNF_ACTIVE) || ev->type != EV_KEY)
        return false;

    if(DDKEY_RSHIFT == ev->data1)
    {
        shiftdown = (EVS_DOWN == ev->state || EVS_REPEAT == ev->state);
        return true;
    }

    if(!(EVS_DOWN == ev->state || EVS_REPEAT == ev->state))
        return false;

    if(DDKEY_BACKSPACE == ev->data1)
    {
        if(!Str_IsEmpty(&_text))
        {
            Str_Truncate(&_text, Str_Length(&_text)-1);
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED, NULL);
            }
        }
        return true;
    }

    int ch = ev->data1;
    if(ch >= ' ' && ch <= 'z')
    {
        if(shiftdown)
            ch = shiftXForm[ch];

        // Filter out nasty characters.
        if(ch == '%')
            return true;

        if(!_maxLength || (unsigned)Str_Length(&_text) < _maxLength)
        {
            Str_AppendChar(&_text, ch);
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED, NULL);
            }
        }
        return true;
    }

    return false;
}

void LineEditWidget::updateGeometry(Page * /*page*/)
{
    // @todo calculate visible dimensions properly.
    Rect_SetWidthHeight(_geometry, 170, 14);
}

int CvarLineEditWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action, void * /*context*/)
{
    DENG2_ASSERT(wi != 0);
    LineEditWidget const &edit = wi->as<LineEditWidget>();
    cvartype_t varType = Con_GetVariableType((char const *)edit.data1);

    if(Widget::MNA_MODIFIED != action) return 1;

    switch(varType)
    {
    case CVT_CHARPTR:
        Con_SetString2((char const *)edit.data1, Str_Text(edit.text()), SVF_WRITE_OVERRIDE);
        break;

    case CVT_URIPTR: {
        /// @todo Sanitize and validate against known schemas.
        de::Uri uri(Str_Text(edit.text()), RC_NULL);
        Con_SetUri2((char const *)edit.data1, reinterpret_cast<uri_s *>(&uri), SVF_WRITE_OVERRIDE);
        break; }

    default: break;
    }
    return 0;
}

mndata_listitem_t::mndata_listitem_t(char const *text, int data)
    : text(text), data(data)
{}

ListWidget::ListWidget()
    : Widget()
    , data     (0)
    , mask     (0)
    , _selection(0)
    , first    (0)
    , numvis   (0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
    Widget::cmdResponder  = ListWidget_CommandResponder;
}

ListWidget::~ListWidget()
{
    qDeleteAll(_items);
}

ListWidget::Items const &ListWidget::items() const
{
    return _items;
}

void ListWidget::draw(Point2Raw const *_origin)
{
    DENG2_ASSERT(_origin != 0);

    dd_bool const flashSelection = ((Widget::_flags & MNF_ACTIVE) && selectionIsVisible());
    float const *color = rs.textColors[_pageColorIdx];
    float dimColor[4], flashColor[4], t = flashSelection? 1 : 0;

    if(flashSelection && cfg.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(_page->timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    lerpColor(flashColor, rs.textColors[_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    flashColor[CA] = color[CA];

    memcpy(dimColor, color, sizeof(dimColor));
    dimColor[CR] *= MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CG] *= MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CB] *= MNDATA_LIST_NONSELECTION_LIGHT;

    if(first < _items.count() && numvis > 0)
    {
        int i = first;

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(rs.textFonts[_pageFontIdx]);

        Point2Raw origin(*_origin);
        do
        {
            mndata_listitem_t const *item = _items[i];

            if(_selection == i)
            {
                if(flashSelection)
                    FR_SetColorAndAlphav(flashColor);
                else
                    FR_SetColorAndAlphav(color);
            }
            else
            {
                FR_SetColorAndAlphav(dimColor);
            }

            FR_DrawText3(item->text, &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
            origin.y += FR_TextHeight(item->text) * (1+MNDATA_LIST_LEADING);
        } while(++i < _items.count() && i < first + numvis);

        DGL_Disable(DGL_TEXTURE_2D);
    }
}

int ListWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    ListWidget *list = static_cast<ListWidget *>(wi);

    switch(cmd)
    {
    case MCMD_NAV_DOWN:
    case MCMD_NAV_UP:
        if(wi->_flags & MNF_ACTIVE)
        {
            int oldSelection = list->_selection;
            if(MCMD_NAV_DOWN == cmd)
            {
                if(list->_selection < list->itemCount() - 1)
                    ++list->_selection;
            }
            else
            {
                if(list->_selection > 0)
                    --list->_selection;
            }

            if(list->_selection != oldSelection)
            {
                S_LocalSound(cmd == MCMD_NAV_DOWN? SFX_MENU_NAV_DOWN : SFX_MENU_NAV_UP, NULL);
                if(wi->hasAction(Widget::MNA_MODIFIED))
                {
                    wi->execAction(Widget::MNA_MODIFIED, NULL);
                }
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_NAV_OUT:
        if(wi->_flags & MNF_ACTIVE)
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            wi->_flags &= ~MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_CLOSE))
            {
                wi->execAction(Widget::MNA_CLOSE, NULL);
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_SELECT:
        if(!(wi->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            wi->_flags |= MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVE))
            {
                wi->execAction(Widget::MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            wi->_flags &= ~MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVEOUT))
            {
                wi->execAction(Widget::MNA_ACTIVEOUT, NULL);
            }
        }
        return true;

    default: return false; // Not eaten.
    }
}

int ListWidget::selection() const
{
    return _selection;
}

dd_bool ListWidget::selectionIsVisible() const
{
    return (_selection >= first && _selection < first + numvis);
}

int ListWidget::itemData(int index) const
{
    if(index < 0 || index >= _items.count()) return 0;

    return _items[index]->data;
}

int ListWidget::findItem(int dataValue) const
{
    for(int i = 0; i < _items.count(); ++i)
    {
        mndata_listitem_t *item = _items[i];
        if(mask)
        {
            if((dataValue & mask) == item->data)
                return i;
        }
        else if(dataValue == item->data)
        {
            return i;
        }
    }
    return -1;
}

dd_bool ListWidget::selectItem(int flags, int itemIndex)
{
    int oldSelection = _selection;

    if(0 > itemIndex || itemIndex >= _items.count()) return false;

    _selection = itemIndex;
    if(_selection == oldSelection) return false;

    if(!(flags & MNLIST_SIF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED, NULL);
    }
    return true;
}

dd_bool ListWidget::selectItemByValue(int flags, int dataValue)
{
    return selectItem(flags, findItem(dataValue));
}

int CvarListWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action, void * /*parameters*/)
{
    ListWidget const *list = &wi->as<ListWidget>();

    if(Widget::MNA_MODIFIED != action) return 1;

    if(list->selection() < 0) return 0; // Hmm?

    cvartype_t varType = Con_GetVariableType((char const *)list->data);
    if(CVT_NULL == varType) return 0;

    mndata_listitem_t const *item = list->items()[list->_selection];
    int value;
    if(list->mask)
    {
        value = Con_GetInteger((char const *)list->data);
        value = (value & ~list->mask) | (item->data & list->mask);
    }
    else
    {
        value = item->data;
    }

    switch(varType)
    {
    case CVT_INT:
        Con_SetInteger2((char const *)list->data, value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2((char const *)list->data, (byte) value, SVF_WRITE_OVERRIDE);
        break;
    default:
        Con_Error("Hu_MenuCvarList: Unsupported variable type %i", (int)varType);
        break;
    }
    return 0;
}

InlineListWidget::InlineListWidget()
    : ListWidget()
{
    ListWidget::cmdResponder = InlineListWidget_CommandResponder;
}

void InlineListWidget::draw(Point2Raw const *origin)
{
    mndata_listitem_t const *item = _items[_selection];

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(rs.textFonts[_pageFontIdx]);
    FR_SetColorAndAlphav(rs.textColors[_pageColorIdx]);
    FR_DrawText3(item->text, origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

int InlineListWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    ListWidget *list = static_cast<ListWidget *>(wi);

    switch(cmd)
    {
    case MCMD_SELECT: // Treat as @c MCMD_NAV_RIGHT
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        int oldSelection = list->_selection;

        if(MCMD_NAV_LEFT == cmd)
        {
            if(list->_selection > 0)
                --list->_selection;
            else
                list->_selection = list->itemCount() - 1;
        }
        else
        {
            if(list->_selection < list->itemCount() - 1)
                ++list->_selection;
            else
                list->_selection = 0;
        }

        // Adjust the first visible item.
        list->first = list->_selection;

        if(oldSelection != list->_selection)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            if(wi->hasAction(Widget::MNA_MODIFIED))
            {
                wi->execAction(Widget::MNA_MODIFIED, NULL);
            }
        }
        return true;
      }
    default:
        return false; // Not eaten.
    }
}

void ListWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);
    Rect_SetWidthHeight(_geometry, 0, 0);
    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));

    RectRaw itemGeometry;
    for(int i = 0; i < _items.count(); ++i)
    {
        mndata_listitem_t *item = _items[i];

        FR_TextSize(&itemGeometry.size, item->text);
        if(i != _items.count() - 1)
        {
            itemGeometry.size.height *= 1 + MNDATA_LIST_LEADING;
        }

        Rect_UniteRaw(_geometry, &itemGeometry);

        itemGeometry.origin.y += itemGeometry.size.height;
    }
}

void InlineListWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);
    mndata_listitem_t *item = _items[_selection];
    Size2Raw size;

    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));
    FR_TextSize(&size, item->text);
    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

DENG2_PIMPL_NOREF(ButtonWidget)
{
    de::String text;  ///< Label text.
    patchid_t patch;  ///< Used when drawing this instead of text, if set.
    bool noAltText;

    Instance() : patch(-1), noAltText(false) {}
};

ButtonWidget::ButtonWidget() : Widget(), d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT2;
    Widget::_pageColorIdx = MENU_COLOR1;
    Widget::cmdResponder  = ButtonWidget_CommandResponder;
}

ButtonWidget::~ButtonWidget()
{}

void ButtonWidget::draw(Point2Raw const *origin)
{
    DENG2_ASSERT(origin != 0);
    //int dis   = (_flags & MNF_DISABLED) != 0;
    //int act   = (_flags & MNF_ACTIVE)   != 0;
    //int click = (_flags & MNF_CLICKED)  != 0;
    //dd_bool down = act || click;
    fontid_t const fontId = rs.textFonts[_pageFontIdx];
    float color[4], t = (_flags & MNF_FOCUS)? 1 : 0;

    // Flash if focused?
    if((_flags & MNF_FOCUS) && cfg.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(_page->timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    lerpColor(color, rs.textColors[_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    color[CA] = rs.textColors[_pageColorIdx][CA];

    FR_SetFont(fontId);
    FR_SetColorAndAlphav(color);
    DGL_Color4f(1, 1, 1, color[CA]);

    Block textAsUtf8 = d->text.toUtf8();
    if(d->patch >= 0)
    {
        char const *replacement = 0;
        if(!d->noAltText)
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), d->patch, textAsUtf8.constData());
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(d->patch, replacement, Vector2i(origin->x, origin->y),
                     ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(textAsUtf8.constData(), origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

int ButtonWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    //ButtonWidget *btn = static_cast<ButtonWidget *>(wi);

    if(cmd == MCMD_SELECT)
    {
        dd_bool justActivated = false;
        if(!(wi->_flags & MNF_ACTIVE))
        {
            justActivated = true;

            wi->_flags |= MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVE))
            {
                wi->execAction(Widget::MNA_ACTIVE, NULL);
            }
        }

        // We are not going to receive an "up event" so action that now.
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        wi->_flags &= ~MNF_ACTIVE;
        if(wi->hasAction(Widget::MNA_ACTIVEOUT))
        {
            wi->execAction(Widget::MNA_ACTIVEOUT, NULL);
        }

        wi->timer = 0;
        return true;
    }

    return false; // Not eaten.
}

void ButtonWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);
    //int dis   = (_flags & MNF_DISABLED) != 0;
    //int act   = (_flags & MNF_ACTIVE)   != 0;
    //int click = (_flags & MNF_CLICKED)  != 0;
    //dd_bool down = act || click;
    Block textAsUtf8 = d->text.toUtf8();
    char const *useText = textAsUtf8.constData();
    Size2Raw size;

    // @todo What if patch replacement is disabled?
    if(d->patch >= 0)
    {
        if(!d->noAltText)
        {
            // Use the replacement string?
            useText = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), d->patch, d->text.toUtf8().constData());
        }

        if(!useText || !useText[0])
        {
            // Use the original patch.
            patchinfo_t info;
            R_GetPatchInfo(d->patch, &info);
            Rect_SetWidthHeight(_geometry, info.geometry.size.width,
                                           info.geometry.size.height);
            return;
        }
    }

    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));
    FR_TextSize(&size, useText);

    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

String const &ButtonWidget::text() const
{
    return d->text;
}

ButtonWidget &ButtonWidget::setText(String newText)
{
    d->text = newText;
    return *this;
}

patchid_t ButtonWidget::patch() const
{
    return d->patch;
}

ButtonWidget &ButtonWidget::setPatch(patchid_t newPatch)
{
    d->patch = newPatch;
    return *this;
}

bool ButtonWidget::noAltText() const
{
    return d->noAltText;
}

ButtonWidget &ButtonWidget::setNoAltText(bool yes)
{
    d->noAltText = yes;
    return *this;
}

CVarToggleWidget::CVarToggleWidget(char const *cvarPath)
    : ButtonWidget()
    , _cvarPath(cvarPath)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR3;
    Widget::cmdResponder  = CVarToggleWidget_CommandResponder;
    Widget::actions[MNA_MODIFIED].callback = CvarToggleWidget_UpdateCvar;
    Widget::actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
}

CVarToggleWidget::~CVarToggleWidget()
{}

char const *CVarToggleWidget::cvarPath() const
{
    return _cvarPath;
}

int CVarToggleWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    //CVarToggleWidget *tog = static_cast<CVarToggleWidget *>(wi);

    if(cmd == MCMD_SELECT)
    {
        dd_bool justActivated = false;
        if(!(wi->_flags & MNF_ACTIVE))
        {
            justActivated = true;
            S_LocalSound(SFX_MENU_CYCLE, NULL);

            wi->_flags |= MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVE))
            {
                wi->execAction(Widget::MNA_ACTIVE, NULL);
            }
        }

        if(!justActivated)
            wi->_flags ^= MNF_ACTIVE;

        if(wi->data1)
        {
            void* data = wi->data1;

            *((char*)data) = (wi->_flags & MNF_ACTIVE) != 0;
            if(wi->hasAction(Widget::MNA_MODIFIED))
            {
                wi->execAction(Widget::MNA_MODIFIED, NULL);
            }
        }

        if(!justActivated && !(wi->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            if(wi->hasAction(Widget::MNA_ACTIVEOUT))
            {
                wi->execAction(Widget::MNA_ACTIVEOUT, NULL);
            }
        }

        wi->timer = 0;
        return true;
    }

    return false; // Not eaten.
}

int CvarToggleWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action, void * /*context*/)
{
    CVarToggleWidget *tog = &wi->as<CVarToggleWidget>();
    cvarbutton_t const *cb = (cvarbutton_t *)wi->data1;
    cvartype_t varType = Con_GetVariableType(cb->cvarname);
    int value;

    if(Widget::MNA_MODIFIED != action) return 1;

    tog->setText(cb->active? cb->yes : cb->no);

    if(CVT_NULL == varType) return 0;

    if(cb->mask)
    {
        value = Con_GetInteger(cb->cvarname);
        if(cb->active)
        {
            value |= cb->mask;
        }
        else
        {
            value &= ~cb->mask;
        }
    }
    else
    {
        value = cb->active;
    }

    Con_SetInteger2(cb->cvarname, value, SVF_WRITE_OVERRIDE);
    return 0;
}

ColorPreviewWidget::ColorPreviewWidget()
    : Widget()
    , width   (0)
    , height  (0)
    , _r       (0)
    , _g       (0)
    , _b       (0)
    , _a       (0)
    , _rgbaMode(false)
    , data1   (0)
    , data2   (0)
    , data3   (0)
    , data4   (0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
    Widget::cmdResponder  = ColorPreviewWidget_CommandResponder;
}

void ColorPreviewWidget::draw(Point2Raw const *offset)
{
    DENG2_ASSERT(offset != 0);

    patchinfo_t t, b, l, r, tl, tr, br, bl;
    int const up = 1;

    R_GetPatchInfo(borderPatches[0], &t);
    R_GetPatchInfo(borderPatches[2], &b);
    R_GetPatchInfo(borderPatches[3], &l);
    R_GetPatchInfo(borderPatches[1], &r);
    R_GetPatchInfo(borderPatches[4], &tl);
    R_GetPatchInfo(borderPatches[5], &tr);
    R_GetPatchInfo(borderPatches[6], &br);
    R_GetPatchInfo(borderPatches[7], &bl);

    int x = offset->x;
    int y = offset->y;
    int w = width;
    int h = height;

    if(t.id || tl.id || tr.id)
    {
        int height = 0;
        if(t.id)  height = t.geometry.size.height;
        if(tl.id) height = de::max(height, tl.geometry.size.height);
        if(tr.id) height = de::max(height, tr.geometry.size.height);

        y += height;
    }

    if(l.id || tl.id || bl.id)
    {
        int width = 0;
        if(l.id)  width = l.geometry.size.width;
        if(tl.id) width = de::max(width, tl.geometry.size.width);
        if(bl.id) width = de::max(width, bl.geometry.size.width);

        x += width;
    }

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_SetMaterialUI((Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString(borderGraphics[0])), DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectf2Tiled(x, y, w, h, 64, 64);

    // Top
    if(t.id)
    {
        DGL_SetPatch(t.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x, y - t.geometry.size.height, w, t.geometry.size.height, up * t.geometry.size.width, up * t.geometry.size.height);
    }

    // Bottom
    if(b.id)
    {
        DGL_SetPatch(b.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x, y + h, w, b.geometry.size.height, up * b.geometry.size.width, up * b.geometry.size.height);
    }

    // Left
    if(l.id)
    {
        DGL_SetPatch(l.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x - l.geometry.size.width, y, l.geometry.size.width, h, up * l.geometry.size.width, up * l.geometry.size.height);
    }

    // Right
    if(r.id)
    {
        DGL_SetPatch(r.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x + w, y, r.geometry.size.width, h, up * r.geometry.size.width, up * r.geometry.size.height);
    }

    // Top Left
    if(tl.id)
    {
        DGL_SetPatch(tl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x - tl.geometry.size.width, y - tl.geometry.size.height, tl.geometry.size.width, tl.geometry.size.height);
    }

    // Top Right
    if(tr.id)
    {
        DGL_SetPatch(tr.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + w, y - tr.geometry.size.height, tr.geometry.size.width, tr.geometry.size.height);
    }

    // Bottom Right
    if(br.id)
    {
        DGL_SetPatch(br.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + w, y + h, br.geometry.size.width, br.geometry.size.height);
    }

    // Bottom Left
    if(bl.id)
    {
        DGL_SetPatch(bl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x - bl.geometry.size.width, y + h, bl.geometry.size.width, bl.geometry.size.height);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(x, y, w, h, _r, _g, _b, _a * rs.pageAlpha);
}

int ColorPreviewWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    //ColorPreviewWidget* cbox = (ColorPreviewWidget*)ob->_typedata;

    switch(cmd)
    {
    case MCMD_SELECT:
        if(!(wi->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            wi->_flags |= MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVE))
            {
                wi->execAction(Widget::MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            wi->_flags &= ~MNF_ACTIVE;
            if(wi->hasAction(Widget::MNA_ACTIVEOUT))
            {
                wi->execAction(Widget::MNA_ACTIVEOUT, NULL);
            }
        }
        return true;

    default: return false; // Not eaten.
    }
}

void ColorPreviewWidget::updateGeometry(Page * /*page*/)
{
    patchinfo_t info;

    Rect_SetWidthHeight(_geometry, width, height);

    // Add bottom border?
    if(R_GetPatchInfo(borderPatches[2], &info))
    {
        info.geometry.size.width = width;
        info.geometry.origin.y   = height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add right border?
    if(R_GetPatchInfo(borderPatches[1], &info))
    {
        info.geometry.size.height = height;
        info.geometry.origin.x    = width;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top border?
    if(R_GetPatchInfo(borderPatches[0], &info))
    {
        info.geometry.size.width = width;
        info.geometry.origin.y   = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add left border?
    if(R_GetPatchInfo(borderPatches[3], &info))
    {
        info.geometry.size.height = height;
        info.geometry.origin.x    = -info.geometry.size.width;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top-left corner?
    if(R_GetPatchInfo(borderPatches[4], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top-right corner?
    if(R_GetPatchInfo(borderPatches[5], &info))
    {
        info.geometry.origin.x = width;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add bottom-right corner?
    if(R_GetPatchInfo(borderPatches[6], &info))
    {
        info.geometry.origin.x = width;
        info.geometry.origin.y = height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add bottom-left corner?
    if(R_GetPatchInfo(borderPatches[7], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }
}

dd_bool ColorPreviewWidget::rgbaMode() const
{
    return _rgbaMode;
}

float ColorPreviewWidget::redf() const
{
    return _r;
}

float ColorPreviewWidget::greenf() const
{
    return _g;
}

float ColorPreviewWidget::bluef() const
{
    return _b;
}

float ColorPreviewWidget::alphaf() const
{
    return (_rgbaMode? _a : 1.0f);
}

dd_bool ColorPreviewWidget::setRedf(int flags, float red)
{
    float oldRed = _r;

    _r = red;
    if(_r != oldRed)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED, NULL);
        }
        return true;
    }
    return false;
}

dd_bool ColorPreviewWidget::setGreenf(int flags, float green)
{
    float oldGreen = _g;

    _g = green;
    if(_g != oldGreen)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED, NULL);
        }
        return true;
    }
    return false;
}

dd_bool ColorPreviewWidget::setBluef(int flags, float blue)
{
    float oldBlue = _b;

    _b = blue;
    if(_b != oldBlue)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED, NULL);
        }
        return true;
    }
    return false;
}

dd_bool ColorPreviewWidget::setAlphaf(int flags, float alpha)
{
    if(_rgbaMode)
    {
        float oldAlpha = _a;
        _a = alpha;
        if(_a != oldAlpha)
        {
            if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED, NULL);
            }
            return true;
        }
    }
    return false;
}

dd_bool ColorPreviewWidget::setColor4f(int flags, float red, float green,
    float blue, float alpha)
{
    int setComps = 0, setCompFlags = (flags | MNCOLORBOX_SCF_NO_ACTION);

    if(setRedf(  setCompFlags, red))   setComps |= 0x1;
    if(setGreenf(setCompFlags, green)) setComps |= 0x2;
    if(setBluef( setCompFlags, blue))  setComps |= 0x4;
    if(setAlphaf(setCompFlags, alpha)) setComps |= 0x8;

    if(0 == setComps) return false;

    if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED, NULL);
    }
    return true;
}

dd_bool ColorPreviewWidget::setColor4fv(int flags, float rgba[4])
{
    if(!rgba) return false;
    return setColor4f(flags, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
}

dd_bool ColorPreviewWidget::copyColor(int flags, ColorPreviewWidget const &other)
{
    return setColor4f(flags, other.redf(), other.greenf(), other.bluef(), other.alphaf());
}

int CvarColorPreviewWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action, void * /*context*/)
{
    ColorPreviewWidget *cbox = &wi->as<ColorPreviewWidget>();

    if(action != Widget::MNA_MODIFIED) return 1;

    // MNColorBox's current color has already been updated and we know
    // that at least one of the color components have changed.
    // So our job is to simply update the associated cvars.
    Con_SetFloat2((char const *)cbox->data1, cbox->redf(),   SVF_WRITE_OVERRIDE);
    Con_SetFloat2((char const *)cbox->data2, cbox->greenf(), SVF_WRITE_OVERRIDE);
    Con_SetFloat2((char const *)cbox->data3, cbox->bluef(),  SVF_WRITE_OVERRIDE);
    if(cbox->rgbaMode())
    {
        Con_SetFloat2((char const *)cbox->data4, cbox->alphaf(), SVF_WRITE_OVERRIDE);
    }

    return 0;
}

SliderWidget::SliderWidget()
    : Widget()
    , min      (0)
    , max      (0)
    , _value    (0)
    , step     (0)
    , floatMode(false)
    , data1    (0)
    , data2    (0)
    , data3    (0)
    , data4    (0)
    , data5    (0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
    Widget::cmdResponder  = SliderWidget_CommandResponder;
}

void SliderWidget::loadResources() // static
{
    pSliderLeft   = R_DeclarePatch(MNDATA_SLIDER_PATCH_LEFT);
    pSliderRight  = R_DeclarePatch(MNDATA_SLIDER_PATCH_RIGHT);
    pSliderMiddle = R_DeclarePatch(MNDATA_SLIDER_PATCH_MIDDLE);
    pSliderHandle = R_DeclarePatch(MNDATA_SLIDER_PATCH_HANDLE);
}

float SliderWidget::value() const
{
    if(floatMode)
    {
        return _value;
    }
    return (int) (_value + (_value > 0? .5f : -.5f));
}

void SliderWidget::setValue(int /*flags*/, float value)
{
    if(floatMode) _value = value;
    else          _value = (int) (value + (value > 0? + .5f : -.5f));
}

int SliderWidget::thumbPos() const
{
#define WIDTH           (middleInfo.geometry.size.width)

    patchinfo_t middleInfo;
    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo)) return 0;

    float range = max - min;
    if(!range) range = 1; // Should never happen.

    float useVal = value() - min;
    return useVal / range * MNDATA_SLIDER_SLOTS * WIDTH;

#undef WIDTH
}

void SliderWidget::draw(Point2Raw const *origin)
{
#define WIDTH                   (middleInfo.geometry.size.width)
#define HEIGHT                  (middleInfo.geometry.size.height)

    DENG2_ASSERT(origin != 0);
    float x, y;// float range = max - min;
    patchinfo_t middleInfo, leftInfo;

    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo)) return;
    if(!R_GetPatchInfo(pSliderLeft, &leftInfo)) return;
    if(WIDTH <= 0 || HEIGHT <= 0) return;

    x = origin->x + MNDATA_SLIDER_SCALE * (MNDATA_SLIDER_OFFSET_X + leftInfo.geometry.size.width);
    y = origin->y + MNDATA_SLIDER_SCALE * (MNDATA_SLIDER_OFFSET_Y);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(MNDATA_SLIDER_SCALE, MNDATA_SLIDER_SCALE, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    if(cfg.menuShadow > 0)
    {
        float from[2], to[2];
        from[0] = 2;
        from[1] = 1+HEIGHT/2;
        to[0] = (MNDATA_SLIDER_SLOTS * WIDTH) - 2;
        to[1] = 1+HEIGHT/2;
        M_DrawGlowBar(from, to, HEIGHT*1.1f, true, true, true, 0, 0, 0, rs.pageAlpha * rs.textShadow);
    }

    DGL_Color4f(1, 1, 1, rs.pageAlpha);

    GL_DrawPatchXY3(pSliderLeft, 0, 0, ALIGN_TOPRIGHT, DPF_NO_OFFSETX);
    GL_DrawPatchXY(pSliderRight, MNDATA_SLIDER_SLOTS * WIDTH, 0);

    DGL_SetPatch(pSliderMiddle, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectf2Tiled(0, middleInfo.geometry.origin.y, MNDATA_SLIDER_SLOTS * WIDTH, HEIGHT, middleInfo.geometry.size.width, middleInfo.geometry.size.height);

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatchXY3(pSliderHandle, thumbPos(), 1, ALIGN_TOP, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef HEIGHT
#undef WIDTH
}

int SliderWidget_CommandResponder(Widget *wi, menucommand_e cmd)
{
    DENG2_ASSERT(wi != 0);
    SliderWidget *sldr = static_cast<SliderWidget *>(wi);

    switch(cmd)
    {
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        float oldvalue = sldr->_value;

        if(MCMD_NAV_LEFT == cmd)
        {
            sldr->_value -= sldr->step;
            if(sldr->_value < sldr->min)
                sldr->_value = sldr->min;
        }
        else
        {
            sldr->_value += sldr->step;
            if(sldr->_value > sldr->max)
                sldr->_value = sldr->max;
        }

        // Did the value change?
        if(oldvalue != sldr->_value)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            if(wi->hasAction(Widget::MNA_MODIFIED))
            {
                wi->execAction(Widget::MNA_MODIFIED, NULL);
            }
        }
        return true;
      }

    default: return false; // Not eaten.
    }
}

int CvarSliderWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action, void * /*context*/)
{
    if(Widget::MNA_MODIFIED != action) return 1;

    SliderWidget &sldr = wi->as<SliderWidget>();
    cvartype_t varType = Con_GetVariableType((char const *)sldr.data1);
    if(CVT_NULL == varType) return 0;

    float value = sldr.value();
    switch(varType)
    {
    case CVT_FLOAT:
        if(sldr.step >= .01f)
        {
            Con_SetFloat2((char const *)sldr.data1, (int) (100 * value) / 100.0f, SVF_WRITE_OVERRIDE);
        }
        else
        {
            Con_SetFloat2((char const *)sldr.data1, value, SVF_WRITE_OVERRIDE);
        }
        break;

    case CVT_INT:
        Con_SetInteger2((char const *)sldr.data1, (int) value, SVF_WRITE_OVERRIDE);
        break;

    case CVT_BYTE:
        Con_SetInteger2((char const *)sldr.data1, (byte) value, SVF_WRITE_OVERRIDE);
        break;

    default: break;
    }

    return 0;
}

static inline dd_bool valueIsOne(float value, dd_bool floatMode)
{
    if(floatMode)
    {
        return INRANGE_OF(1, value, .0001f);
    }
    return (value > 0 && 1 == (int)(value + .5f));
}

static char *composeTextualValue(float value, dd_bool floatMode, int precision,
    size_t bufSize, char* buf)
{
    DENG2_ASSERT(0 != bufSize && buf);
    precision = de::max(0, precision);
    if(floatMode && !valueIsOne(value, floatMode))
    {
        dd_snprintf(buf, bufSize, "%.*f", precision, value);
    }
    else
    {
        dd_snprintf(buf, bufSize, "%.*i", precision, (int)value);
    }
    return buf;
}

static char *composeValueString(float value, float defaultValue, dd_bool floatMode,
    int precision, char const *defaultString, char const *templateString,
    char const *onethSuffix, char const *nthSuffix, size_t bufSize, char *buf)
{
    DENG2_ASSERT(0 != bufSize && buf);

    dd_bool const haveTemplateString = (templateString && templateString[0]);
    dd_bool const haveDefaultString  = (defaultString && defaultString[0]);
    dd_bool const haveOnethSuffix    = (onethSuffix && onethSuffix[0]);
    dd_bool const haveNthSuffix      = (nthSuffix && nthSuffix[0]);
    char const *suffix = 0;
    char textualValue[11];

    // Is the default-value-string in use?
    if(haveDefaultString && INRANGE_OF(value, defaultValue, .0001f))
    {
        strncpy(buf, defaultString, bufSize);
        buf[bufSize] = '\0';
        return buf;
    }

    composeTextualValue(value, floatMode, precision, 10, textualValue);

    // Choose a suffix.
    if(haveOnethSuffix && valueIsOne(value, floatMode))
    {
        suffix = onethSuffix;
    }
    else if(haveNthSuffix)
    {
        suffix = nthSuffix;
    }
    else
    {
        suffix = "";
    }

    // Are we substituting the textual value into a template?
    if(haveTemplateString)
    {
        char const *c, *beginSubstring = 0;
        ddstring_t compStr;

        // Reserve a conservative amount of storage, we assume the caller
        // knows best and take the value given as the output buffer size.
        Str_Init(&compStr);
        Str_Reserve(&compStr, bufSize);

        // Composite the final string.
        beginSubstring = templateString;
        for(c = beginSubstring; *c; c++)
        {
            if(c[0] == '%' && c[1] == '1')
            {
                Str_PartAppend(&compStr, beginSubstring, 0, c - beginSubstring);
                Str_Appendf(&compStr, "%s%s", textualValue, suffix);
                // Next substring will begin from here.
                beginSubstring = c + 2;
                c += 1;
            }
        }
        // Anything remaining?
        if(beginSubstring != c)
            Str_Append(&compStr, beginSubstring);

        strncpy(buf, Str_Text(&compStr), bufSize);
        buf[bufSize] = '\0';
        Str_Free(&compStr);
    }
    else
    {
        dd_snprintf(buf, bufSize, "%s%s", textualValue, suffix);
    }

    return buf;
}

void SliderWidget::updateGeometry(Page * /*page*/)
{
    patchinfo_t info;
    if(!R_GetPatchInfo(pSliderMiddle, &info)) return;

    int middleWidth = info.geometry.size.width * MNDATA_SLIDER_SLOTS;
    Rect_SetWidthHeight(_geometry, middleWidth, info.geometry.size.height);

    if(R_GetPatchInfo(pSliderLeft, &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    if(R_GetPatchInfo(pSliderRight, &info))
    {
        info.geometry.origin.x += middleWidth;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    Rect_SetWidthHeight(_geometry, .5f + Rect_Width (_geometry) * MNDATA_SLIDER_SCALE,
                                   .5f + Rect_Height(_geometry) * MNDATA_SLIDER_SCALE);
}

TextualSliderWidget::TextualSliderWidget()
    : SliderWidget()
{}

void TextualSliderWidget::draw(Point2Raw const *origin)
{
    DENG2_ASSERT(origin != 0);

    float const value = de::clamp(min, _value, max);
    char textualValue[41];
    char const *str = composeValueString(value, 0, floatMode, 0,
        (char const *)data2, (char const *)data3, (char const *)data4, (char const *)data5, 40, textualValue);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin->x, origin->y, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(rs.textFonts[_pageFontIdx]);
    FR_SetColorAndAlphav(rs.textColors[_pageColorIdx]);
    FR_DrawTextXY3(str, 0, 0, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-origin->x, -origin->y, 0);
}

void TextualSliderWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);

    fontid_t const font = page->predefinedFont(mn_page_fontid_t(_pageFontIdx));
    float const value = de::clamp(min, _value, max);
    char textualValue[41];
    char const *str = composeValueString(value, 0, floatMode, 0,
        (char const *)data2, (char const *)data3, (char const *)data4, (char const *)data5, 40, textualValue);

    FR_SetFont(font);

    Size2Raw size; FR_TextSize(&size, str);

    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

MobjPreviewWidget::MobjPreviewWidget()
    : Widget()
    , mobjType(0)
    , tClass  (0)
    , tMap    (0)
    , plrClass(0)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
}

static void findSpriteForMobjType(int mobjType, spritetype_e *sprite, int *frame)
{
    DENG2_ASSERT(mobjType >= MT_FIRST && mobjType < NUMMOBJTYPES && sprite && frame);

    mobjinfo_t *info = &MOBJINFO[mobjType];
    int stateNum = info->states[SN_SPAWN];
    *sprite = spritetype_e(STATES[stateNum].sprite);
    *frame  = ((menuTime >> 3) & 3);
}

void MobjPreviewWidget::setMobjType(int newMobjType)
{
    mobjType = newMobjType;
}

void MobjPreviewWidget::setPlayerClass(int newPlayerClass)
{
    plrClass = newPlayerClass;
}

void MobjPreviewWidget::setTranslationClass(int newTranslationClass)
{
    tClass = newTranslationClass;
}

void MobjPreviewWidget::setTranslationMap(int newTranslationMap)
{
    tMap = newTranslationMap;
}

/// @todo We can do better - the engine should be able to render this visual for us.
void MobjPreviewWidget::draw(Point2Raw const *offset)
{
    DENG2_ASSERT(offset != 0);

    if(MT_NONE == mobjType) return;

    spritetype_e sprite;
    int spriteFrame;
    findSpriteForMobjType(mobjType, &sprite, &spriteFrame);

    spriteinfo_t info;
    if(!R_GetSpriteInfo(sprite, spriteFrame, &info)) return;

    Point2Raw origin(info.geometry.origin.x, info.geometry.origin.y);
    Size2Raw size(info.geometry.size.width, info.geometry.size.height);

    float scale = (size.height > size.width? (float)MNDATA_MOBJPREVIEW_HEIGHT / size.height :
                                             (float)MNDATA_MOBJPREVIEW_WIDTH  / size.width);

    float s = info.texCoord[0];
    float t = info.texCoord[1];

    int tClassCycled = tClass;
    int tMapCycled   = tMap;
    // Are we cycling the translation map?
    if(tMapCycled == NUMPLAYERCOLORS)
    {
        tMapCycled = menuTime / 5 % NUMPLAYERCOLORS;
    }
#if __JHEXEN__
    if(gameMode == hexen_v10)
    {
        // Cycle through the four available colors.
        if(tMap == NUMPLAYERCOLORS) tMapCycled = menuTime / 5 % 4;
    }
    if(plrClass >= PCLASS_FIGHTER)
    {
        R_GetTranslation(plrClass, tMapCycled, &tClassCycled, &tMapCycled);
    }
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(scale, scale, 1);
    // Translate origin to the top left.
    DGL_Translatef(-origin.x, -origin.y, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_SetPSprite2(info.material, tClassCycled, tMapCycled);
    DGL_Color4f(1, 1, 1, rs.pageAlpha);

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(0, 0);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(size.width, 0);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(size.width, size.height);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(0, size.height);
    DGL_End();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_Disable(DGL_TEXTURE_2D);
}

void MobjPreviewWidget::updateGeometry(Page * /*page*/)
{
    // @todo calculate visible dimensions properly!
    Rect_SetWidthHeight(_geometry, MNDATA_MOBJPREVIEW_WIDTH, MNDATA_MOBJPREVIEW_HEIGHT);
}

} // namespace menu
} // namespace common
