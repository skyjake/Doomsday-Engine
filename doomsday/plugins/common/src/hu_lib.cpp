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

#include "hu_menu.h" // For the menu sound ids.
#include "menu/page.h"
#include "menu/widgets/buttonwidget.h"
#include "menu/widgets/colorpreviewwidget.h"
#include "menu/widgets/cvarinlinelistwidget.h"
#include "menu/widgets/cvarlineeditwidget.h"
#include "menu/widgets/cvartogglewidget.h"
#include "menu/widgets/inlinelistwidget.h"
#include "menu/widgets/inputbindingwidget.h"
#include "menu/widgets/labelwidget.h"
#include "menu/widgets/lineeditwidget.h"
#include "menu/widgets/mobjpreviewwidget.h"
#include "menu/widgets/rectwidget.h"
#include "menu/widgets/sliderwidget.h"
#include "menu/widgets/textualsliderwidget.h"

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

DENG2_PIMPL_NOREF(Page)
{
    String name;               ///< Symbolic name/identifier.
    Widgets widgets;

    /// "Physical" geometry in fixed 320x200 screen coordinate space.
    Point2Raw origin;
    Rect *geometry = Rect_New();

    String title;              ///< Title of this page.
    Page *previous = nullptr;  ///< Previous page.
    int focus      = -1;       ///< Index of the currently focused widget else @c -1
    int flags      = 0;        ///< @ref menuPageFlags
    int timer      = 0;
    void *userData = nullptr;

    /// Predefined fonts objects on this page.
    fontid_t fonts[MENU_FONT_COUNT];

    /// Predefined colors for objects on this page.
    uint colors[MENU_COLOR_COUNT];

    OnActiveCallback onActiveCallback = nullptr;
    OnDrawCallback drawer             = nullptr;
    CommandResponder cmdResponder     = nullptr;

    Instance()
    {
        fontid_t fontId = FID(GF_FONTA);
        for(int i = 0; i < MENU_FONT_COUNT; ++i)
        {
            fonts[i] = fontId;
        }

        de::zap(colors);
        colors[1] = 1;
        colors[2] = 2;
    }

    ~Instance()
    {
        Rect_Delete(geometry);
        qDeleteAll(widgets);
    }

    /// @pre @a wi is a child of this page.
    void giveChildFocus(Widget *wi, bool allowRefocus = false)
    {
        DENG2_ASSERT(wi != 0);

        if(!(0 > focus))
        {
            if(wi != widgets[focus])
            {
                Widget *oldFocused = widgets[focus];
                if(oldFocused->hasAction(Widget::MNA_FOCUSOUT))
                {
                    oldFocused->execAction(Widget::MNA_FOCUSOUT);
                }
                oldFocused->setFlags(FO_CLEAR, MNF_FOCUS);
            }
            else if(!allowRefocus)
            {
                return;
            }
        }

        focus = widgets.indexOf(wi);
        wi->setFlags(FO_SET, MNF_FOCUS);
        if(wi->hasAction(Widget::MNA_FOCUS))
        {
            wi->execAction(Widget::MNA_FOCUS);
        }
    }

    /**
     * Determines the size of the menu cursor for a focused widget. If no widget is currently
     * focused the default cursor size (i.e., the effective line height for @c MENU_FONT1)
     * is used.
     *
     * (Which means this should @em not be called to determine whether the cursor is in use).
     */
    int cursorSizeFor(Widget *focused, int lineHeight)
    {
        int focusedHeight = focused? Size2_Height(focused->size()) : 0;

        // Ensure the cursor is at least as tall as the effective line height for
        // the page. This is necessary because some mods replace the menu button
        // graphics with empty and/or tiny images (e.g., Hell Revealed 2).
        /// @note Handling this correctly would mean separate physical/visual
        /// geometries for menu widgets.
        return de::max(focusedHeight, lineHeight);
    }
};

Page::Page(String name, Point2Raw const &origin, int flags,
    OnDrawCallback drawer, CommandResponder cmdResponder, void *userData)
    : d(new Instance)
{
    std::memcpy(&d->origin, &origin, sizeof(d->origin));
    d->name         = name;
    d->flags        = flags;
    d->drawer       = drawer;
    d->cmdResponder = cmdResponder;
    d->userData     = userData;
}

Page::~Page()
{}

String Page::name() const
{
    return d->name;
}

Page::Widgets &Page::widgets()
{
    return d->widgets;
}

Page::Widgets const &Page::widgets() const
{
    return d->widgets;
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

void Page::applyLayout()
{
    Rect_SetXY(d->geometry, 0, 0);
    Rect_SetWidthHeight(d->geometry, 0, 0);

    // Apply layout logic to this page.

    if(d->flags & MPF_LAYOUT_FIXED)
    {
        // This page uses a fixed layout.
        for(Widget *wi : d->widgets)
        {
            if(!MNObject_IsDrawable(wi)) continue;

            Rect_SetXY(wi->_geometry, wi->_origin.x, wi->_origin.y);
            Rect_Unite(d->geometry, wi->_geometry);
        }
        return;
    }

    // This page uses a dynamic layout.
    int lineOffset;
    int lh = lineHeight(&lineOffset);

    Point2Raw origin;

    for(int i = 0; i < d->widgets.count(); )
    {
        Widget *wi = d->widgets[i];
        Widget *nextWi = i + 1 < d->widgets.count()? d->widgets[i + 1] : 0;

        if(!MNObject_IsDrawable(wi))
        {
            // Proceed to the next widget!
            i += 1;
            continue;
        }

        // If the widget has a fixed position, we will ignore it while doing
        // dynamic layout.
        if(wi->flags() & MNF_POSITION_FIXED)
        {
            Rect_SetXY(wi->_geometry, wi->_origin.x, wi->_origin.y);
            Rect_Unite(d->geometry, wi->_geometry);

            // To the next object.
            i += 1;
            continue;
        }

        // An additional offset requested?
        if(wi->flags() & MNF_LAYOUT_OFFSET)
        {
            origin.x += wi->_origin.x;
            origin.y += wi->_origin.y;
        }

        Rect_SetXY(wi->_geometry, origin.x, origin.y);

        // Orient label plus button/inline-list/textual-slider pairs about a
        // vertical dividing line, with the label on the left, other widget
        // on the right.
        // @todo Do not assume pairing, a widget should designate it's label.
        if(wi->is<LabelWidget>() && nextWi)
        {
            if(MNObject_IsDrawable(nextWi) &&
               (nextWi->is<ButtonWidget>()         ||
                nextWi->is<InlineListWidget>()     ||
                nextWi->is<ColorPreviewWidget>()   ||
                nextWi->is<InputBindingWidget>()   ||
                nextWi->is<TextualSliderWidget>()))
            {
                int const margin = lineOffset * 2;

                Rect_SetXY(nextWi->_geometry, margin + Rect_Width(wi->_geometry), origin.y);

                RectRaw united;
                origin.y += Rect_United(wi->_geometry, nextWi->_geometry, &united)
                          ->size.height + lineOffset;

                Rect_UniteRaw(d->geometry, &united);

                // Extra spacing between object groups.
                if(i + 2 < d->widgets.count() && nextWi->group() != d->widgets[i + 2]->group())
                {
                    origin.y += lh;
                }

                // Proceed to the next object!
                i += 2;
                continue;
            }
        }

        Rect_Unite(d->geometry, wi->_geometry);

        origin.y += Rect_Height(wi->_geometry) + lineOffset;

        // Extra spacing between object groups.
        if(nextWi && nextWi->group() != wi->group())
        {
            origin.y += lh;
        }

        // Proceed to the next object!
        i += 1;
    }
}

void Page::setOnActiveCallback(Page::OnActiveCallback newCallback)
{
    d->onActiveCallback = newCallback;
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

static void drawPageHeading(Page *page, Point2Raw const *offset = nullptr)
{
    if(!page) return;
    if(page->title().isEmpty()) return;

    Point2Raw origin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) - ((SCREENHEIGHT / 2 - 5) / cfg.menuScale));
    if(offset)
    {
        origin.x += offset->x;
        origin.y += offset->y;
    }

    FR_PushAttrib();
    Hu_MenuDrawPageTitle(page->title(), origin.x, origin.y); origin.y += 16;
    drawPageNavigation(page, origin.x, origin.y);
    FR_PopAttrib();
}

void Page::draw(float alpha, bool showFocusCursor)
{
    alpha = de::clamp(0.f, alpha, 1.f);
    if(alpha <= .0001f) return;

    int focusedHeight = 0;
    Point2Raw cursorOrigin;

    // Object geometry is determined from properties defined in the
    // render state, so configure render state before we begin.
    setupRenderStateForPageDrawing(this, alpha);

    // Update object geometry. We'll push the font renderer state because
    // updating geometry may require changing the current values.
    FR_PushAttrib();
    updatePageObjectGeometries(this);

    // Back to default page render state.
    FR_PopAttrib();

    // We can now layout the widgets of this page.
    /// @todo Do not modify the page layout here.
    applyLayout();

    // Determine the origin of the focus object (this dictates the page scroll location).
    Widget *focused = focusWidget();
    if(focused && !MNObject_IsDrawable(focused))
    {
        focused = 0;
    }

    // Are we focusing?
    if(focused)
    {
        focusedHeight = d->cursorSizeFor(focused, lineHeight());

        // Determine the origin and dimensions of the cursor.
        /// @todo Each object should define a focus origin...
        cursorOrigin.x = -1;
        cursorOrigin.y = Point2_Y(focused->origin());

        /// @kludge
        /// We cannot yet query the subobjects of the list for these values
        /// so we must calculate them ourselves, here.
        if(focused->flags() & MNF_ACTIVE)
        {
            if(ListWidget const *list = focused->maybeAs<ListWidget>())
            {
                if(list->selectionIsVisible())
                {
                    FR_PushAttrib();
                    FR_SetFont(predefinedFont(mn_page_fontid_t(focused->font())));
                    focusedHeight = FR_CharHeight('A') * (1+MNDATA_LIST_LEADING);
                    cursorOrigin.y += (list->selection() - list->first()) * focusedHeight;
                    FR_PopAttrib();
                }
            }
        }
        // kludge end
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(d->origin.x, d->origin.y, 0);

    // Apply page scroll?
    if(!(d->flags & MPF_NEVER_SCROLL) && focused)
    {
        RectRaw pageGeometry, viewRegion;
        Rect_Raw(d->geometry, &pageGeometry);

        // Determine available screen region for the page.
        viewRegion.origin.x = 0;
        viewRegion.origin.y = d->origin.y;
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
    for(Widget *wi : d->widgets)
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
    if(showFocusCursor && focused)
    {
        Hu_MenuDrawFocusCursor(cursorOrigin.x, cursorOrigin.y, focusedHeight, alpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    drawPageHeading(this);

    // The page has its own drawer.
    if(d->drawer)
    {
        FR_PushAttrib();
        d->drawer(this, &d->origin);
        FR_PopAttrib();
    }

    // How about some additional help/information for the focused item?
    if(focused && focused->hasHelpInfo())
    {
        Point2Raw helpOrigin(SCREENWIDTH/2, (SCREENHEIGHT/2) + ((SCREENHEIGHT/2-5)/cfg.menuScale));
        Hu_MenuDrawPageHelp(focused->helpInfo().toUtf8().constData(), helpOrigin.x, helpOrigin.y);
    }
}

void Page::setTitle(String const &newTitle)
{
    d->title = newTitle;
}

String Page::title() const
{
    return d->title;
}

void Page::setX(int x)
{
    d->origin.x = x;
}

void Page::setY(int y)
{
    d->origin.y = y;
}

void Page::setPreviousPage(Page *newPrevious)
{
    d->previous = newPrevious;
}

Page *Page::previousPage() const
{
    return d->previous;
}

Widget *Page::focusWidget()
{
    if(d->widgets.isEmpty() || d->focus < 0) return 0;
    return d->widgets[d->focus];
}

void Page::clearFocusWidget()
{
    if(d->focus >= 0)
    {
        Widget *wi = d->widgets[d->focus];
        if(wi->flags() & MNF_ACTIVE)
        {
            return;
        }
    }
    d->focus = -1;
    for(Widget *wi : d->widgets)
    {
        wi->setFlags(FO_CLEAR, MNF_FOCUS);
    }
    refocus();
}

Widget &Page::findWidget(int group, int flags)
{
    if(Widget *wi = tryFindWidget(group, flags))
    {
        return *wi;
    }
    throw Error("Page::findWidget", QString("Failed to locate widget in group #%1 with flags %2").arg(group).arg(flags));
}

Widget *Page::tryFindWidget(int group, int flags)
{
    for(Widget *wi : d->widgets)
    {
        if(wi->group() == group && (wi->flags() & flags) == flags)
            return wi;
    }
    return 0; // Not found.
}

void Page::setFocus(Widget *wi)
{
    int index = indexOf(wi);
    if(index < 0)
    {
        DENG2_ASSERT(!"Page::Focus: Failed to determine index-in-page for widget.");
        return;
    }
    d->giveChildFocus(d->widgets[index]);
}

void Page::refocus()
{
    LOG_AS("Page");

    // If we haven't yet visited this page then find the first focusable
    // wodget and select it.
    if(0 > d->focus)
    {
        int i, giveFocus = -1;

        // First look for a default focus wodget. There should only be one
        // but find the last with this flag...
        for(i = 0; i < d->widgets.count(); ++i)
        {
            Widget *wi = d->widgets[i];
            if((wi->flags() & MNF_DEFAULT) && !(wi->flags() & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
            }
        }

        // No default focus? Find the first focusable widget.
        if(-1 == giveFocus)
        for(i = 0; i < d->widgets.count(); ++i)
        {
            Widget *wi = d->widgets[i];
            if(!(wi->flags() & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
                break;
            }
        }

        if(-1 != giveFocus)
        {
            d->giveChildFocus(d->widgets[giveFocus]);
        }
        else
        {
            LOGDEV_WARNING("No focusable object on page");
        }
    }
    else
    {
        // We've been here before; re-focus on the last focused object.
        d->giveChildFocus(d->widgets[d->focus], true);
    }
}

void Page::initialize()
{
    // Reset page timer.
    d->timer = 0;

    // (Re)init objects.
    for(Widget *wi : d->widgets)
    {
        // Reset object timer.
        wi->timer = 0;

        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            cvarbutton_t *cvb = (cvarbutton_t *) wi->data1;
            bool const activate = (cvb && cvb->active);
            tog->setFlags((activate? FO_SET:FO_CLEAR), MNF_ACTIVE);
        }
        if(ListWidget *list = wi->maybeAs<ListWidget>())
        {
            // Determine number of potentially visible items.
            list->updateVisibleSelection();
        }
    }

    if(d->widgets.isEmpty())
    {
        // Presumably the widgets will be added later...
        return;
    }

    refocus();

    if(d->onActiveCallback)
    {
        d->onActiveCallback(this);
    }
}

void Page::initWidgets()
{
    for(Widget *wi : d->widgets)
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

        if(wi->is<LabelWidget>() || wi->is<MobjPreviewWidget>())
        {
            wi->setFlags(FO_SET, MNF_NO_FOCUS);
        }
    }
}

/// Main task is to update objects linked to cvars.
void Page::updateWidgets()
{
    for(Widget *wi : d->widgets)
    {
        if(wi->is<LabelWidget>() || wi->is<MobjPreviewWidget>())
        {
            wi->setFlags(FO_SET, MNF_NO_FOCUS);
        }
        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            if(wi->data1)
            {
                // This button has already been initialized.
                cvarbutton_t *cvb = (cvarbutton_t *) wi->data1;
                cvb->active = (Con_GetByte(tog->cvarPath()) & (cvb->mask? cvb->mask : ~0)) != 0;
                tog->setText(cvb->active? cvb->yes : cvb->no);
                continue;
            }

            // Find the cvarbutton representing this one.
            for(cvarbutton_t *cvb = mnCVarButtons; cvb->cvarname; cvb++)
            {
                if(!strcmp(tog->cvarPath(), cvb->cvarname) && wi->data2 == cvb->mask)
                {
                    cvb->active = (Con_GetByte(tog->cvarPath()) & (cvb->mask? cvb->mask : ~0)) != 0;
                    wi->data1 = (void *) cvb;
                    tog->setText(cvb->active ? cvb->yes : cvb->no);
                    break;
                }
            }
        }
        if(CVarInlineListWidget *list = wi->maybeAs<CVarInlineListWidget>())
        {
            int itemValue = Con_GetInteger(list->cvarPath());
            if(int valueMask = list->cvarValueMask())
                itemValue &= valueMask;
            list->selectItemByValue(itemValue);
        }
        if(CVarLineEditWidget *edit = wi->maybeAs<CVarLineEditWidget>())
        {
            edit->setText(Con_GetString(edit->cvarPath()));
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
                cbox->setColor(Vector4f(Con_GetFloat((char const *)cbox->data1),
                                        Con_GetFloat((char const *)cbox->data2),
                                        Con_GetFloat((char const *)cbox->data3),
                                        (cbox->rgbaMode()? Con_GetFloat((char const *)cbox->data4) : 1.f)));
            }
        }
    }
}

void Page::tick()
{
    // Call the ticker of each object, unless they're hidden or paused.
    for(Widget *wi : d->widgets)
    {
        if((wi->flags() & MNF_PAUSED) || (wi->flags() & MNF_HIDDEN))
            continue;

        wi->tick();

        // Advance object timer.
        wi->timer++;
    }

    d->timer++;
}

fontid_t Page::predefinedFont(mn_page_fontid_t id)
{
    DENG2_ASSERT(VALID_MNPAGE_FONTID(id));
    return d->fonts[id];
}

void Page::setPredefinedFont(mn_page_fontid_t id, fontid_t fontId)
{
    DENG2_ASSERT(VALID_MNPAGE_FONTID(id));
    d->fonts[id] = fontId;
}

void Page::predefinedColor(mn_page_colorid_t id, float rgb[3])
{
    DENG2_ASSERT(rgb != 0);
    DENG2_ASSERT(VALID_MNPAGE_COLORID(id));
    uint colorIndex = d->colors[id];
    rgb[CR] = cfg.menuTextColors[colorIndex][CR];
    rgb[CG] = cfg.menuTextColors[colorIndex][CG];
    rgb[CB] = cfg.menuTextColors[colorIndex][CB];
}

int Page::timer()
{
    return d->timer;
}

int Page::handleCommand(menucommand_e cmd)
{
    // Maybe the currently focused widget will handle this?
    if(Widget *focused = focusWidget())
    {
        if(focused->cmdResponder)
        {
            if(int result = focused->cmdResponder(focused, cmd))
                return result;
        }
        else
        {
            if(int result = focused->handleCommand(cmd))
                return result;
        }
    }

    // Maybe a custom command responder for the page?
    if(d->cmdResponder)
    {
        if(int result = d->cmdResponder(this, cmd))
            return result;
    }

    // Default/fallback handling for the page:
    switch(cmd)
    {
    case MCMD_NAV_PAGEUP:
    case MCMD_NAV_PAGEDOWN:
        /// @todo Why is the sound played here?
        S_LocalSound(cmd == MCMD_NAV_PAGEUP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
        return true;

    case MCMD_NAV_UP:
    case MCMD_NAV_DOWN:
        // An object on this page must have focus in order to navigate.
        if(Widget *focused = focusWidget())
        {
            int i = 0, giveFocus = indexOf(focused);
            do
            {
                giveFocus += (cmd == MCMD_NAV_UP? -1 : 1);
                if(giveFocus < 0)
                    giveFocus = widgetCount() - 1;
                else if(giveFocus >= widgetCount())
                    giveFocus = 0;
            } while(++i < widgetCount() && (d->widgets[giveFocus]->flags() & (MNF_DISABLED | MNF_NO_FOCUS | MNF_HIDDEN)));

            if(giveFocus != indexOf(focusWidget()))
            {
                S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
                setFocus(d->widgets[giveFocus]);
            }
        }
        return true;

    case MCMD_NAV_OUT:
        if(!d->previous)
        {
            S_LocalSound(SFX_MENU_CLOSE, NULL);
            Hu_MenuCommand(MCMD_CLOSE);
        }
        else
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            Hu_MenuSetPage(d->previous);
        }
        return true;

    default: break;
    }

    return false; // Not handled.
}

void Page::setUserData(void *newUserData)
{
    d->userData = newUserData;
}

void *Page::userData() const
{
    return d->userData;
}

DENG2_PIMPL_NOREF(Widget)
{
    int group = 0;    ///< Object group identifier.
    String helpInfo;  ///< Additional help information displayed when the widget has focus.
};

Widget::Widget()
    : _flags             (0)
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
    , d(new Instance)
{
    de::zap(actions);
}

Widget::~Widget()
{
    Rect_Delete(_geometry);
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

bool Widget::hasPage() const
{
    return _page != 0;
}

Page &Widget::page() const
{
    if(_page)
    {
        return *_page;
    }
    throw Error("Widget::page", "No page is attributed");
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

    default: DENG2_ASSERT(!"Widget::SetFlags: Unknown op.");
    }
    return *this;
}

int Widget::group() const
{
    return d->group;
}

Widget &Widget::setGroup(int newGroup)
{
    d->group = newGroup;
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

String const &Widget::helpInfo() const
{
    return d->helpInfo;
}

Widget &Widget::setHelpInfo(String newHelpInfo)
{
    d->helpInfo = newHelpInfo;
    return *this;
}

int Widget::handleCommand(menucommand_e cmd)
{
    if(MCMD_SELECT == cmd && (_flags & MNF_FOCUS) && !(_flags & MNF_DISABLED))
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!(_flags & MNF_ACTIVE))
        {
            _flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        _flags &= ~MNF_ACTIVE;
        if(hasAction(MNA_ACTIVEOUT))
        {
            execAction(MNA_ACTIVEOUT);
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

void Widget::execAction(mn_actionid_t id)
{
    if(hasAction(id))
    {
        action(id)->callback(this, id);
        return;
    }
    DENG2_ASSERT(!"MNObject::ExecAction: Attempt to execute non-existent action.");
}

DENG2_PIMPL_NOREF(RectWidget)
{
    Size2Raw dimensions;  ///< Dimensions of the rectangle.
    patchid_t patch = 0;  ///< Background patch.
};

RectWidget::RectWidget(patchid_t backgroundPatch)
    : Widget()
    , d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;

    setBackgroundPatch(backgroundPatch);
}

RectWidget::~RectWidget()
{}

void RectWidget::draw(Point2Raw const *origin)
{
    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    if(d->patch)
    {
        DGL_SetPatch(d->patch, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_Enable(DGL_TEXTURE_2D);
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    DGL_DrawRect2(0, 0, d->dimensions.width, d->dimensions.height);

    if(d->patch)
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
    if(d->dimensions.width == 0 && d->dimensions.height == 0)
    {
        // Inherit dimensions from the patch.
        patchinfo_t info;
        if(R_GetPatchInfo(d->patch, &info))
        {
            std::memcpy(&d->dimensions, &info.geometry.size, sizeof(d->dimensions));
        }
    }
    Rect_SetWidthHeight(Widget::_geometry, d->dimensions.width, d->dimensions.height);
}

void RectWidget::setBackgroundPatch(patchid_t newBackgroundPatch)
{
    d->patch = newBackgroundPatch;
}

DENG2_PIMPL_NOREF(LabelWidget)
{
    String text;
    patchid_t *patch = 0;  ///< Used instead of text if Patch Replacement is in use.
    int flags = 0;         ///< @ref mnTextFlags
};

LabelWidget::LabelWidget(String const &text, patchid_t *patch) : Widget(), d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
    setText(text);
    setPatch(patch);
}

LabelWidget::~LabelWidget()
{}

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

    if(d->patch)
    {
        String replacement;
        if(!(d->flags & MNTEXT_NO_ALTTEXT))
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), *d->patch, d->text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(*d->patch, replacement, Vector2i(origin->x, origin->y),
                     ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(d->text.toUtf8().constData(), origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

void LabelWidget::updateGeometry(Page *page)
{
    DENG2_ASSERT(page != 0);

    /// @todo What if patch replacement is disabled?
    if(d->patch)
    {
        patchinfo_t info;
        R_GetPatchInfo(*d->patch, &info);
        Rect_SetWidthHeight(_geometry, info.geometry.size.width, info.geometry.size.height);
        return;
    }

    Size2Raw size;
    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));
    FR_TextSize(&size, d->text.toUtf8().constData());
    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

void LabelWidget::setPatch(patchid_t *newPatch)
{
    d->patch = newPatch;
}

void LabelWidget::setText(String const &newText)
{
    d->text = newText;
}

DENG2_PIMPL_NOREF(LineEditWidget)
{
    String text;
    String oldText;    ///< For restoring a canceled edit.
    String emptyText;  ///< Used when value is empty.
    int maxLength       = 0;
    int maxVisibleChars = 0;
};

LineEditWidget::LineEditWidget()
    : Widget()
    , d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
}

LineEditWidget::~LineEditWidget()
{}

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

    String useText;
    float light = 1, textAlpha = rs.pageAlpha;
    if(!d->text.isEmpty())
    {
        useText = d->text;
    }
    else if(!((Widget::_flags & MNF_ACTIVE) && (Widget::_flags & MNF_FOCUS)))
    {
        useText = d->emptyText;
        light *= .5f;
        textAlpha = rs.pageAlpha * .75f;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(fontId);

    //int const numVisCharacters = de::clamp(0, useText.isNull()? 0 : useText.length(), d->maxVisibleChars);

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
        FR_DrawText3(useText.toUtf8().constData(), &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

        // Are we drawing a cursor?
        if((Widget::_flags & MNF_ACTIVE) && (Widget::_flags & MNF_FOCUS) && (menuTime & 8) &&
           (!d->maxLength || d->text.length() < d->maxLength))
        {
            origin.x += FR_TextWidth(useText.toUtf8().constData());
            FR_DrawChar3('_', &origin, ALIGN_TOPLEFT,  MN_MergeMenuEffectWithDrawTextFlags(0));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int LineEditWidget::maxLength() const
{
    return d->maxLength;
}

void LineEditWidget::setMaxLength(int newMaxLength)
{
    newMaxLength = de::max(newMaxLength, 0);
    if(d->maxLength != newMaxLength)
    {
        if(newMaxLength < d->maxLength)
        {
            d->text.truncate(newMaxLength);
            d->oldText.truncate(newMaxLength);
        }
        d->maxLength = newMaxLength;
    }
}

String LineEditWidget::text() const
{
    return d->text;
}

void LineEditWidget::setText(String const &newText, int flags)
{
    d->text = newText;
    if(d->maxLength) d->text.truncate(d->maxLength);

    if(flags & MNEDIT_STF_REPLACEOLD)
    {
        d->oldText = d->text;
    }

    if(!(flags & MNEDIT_STF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED);
    }
}

void LineEditWidget::setEmptyText(String const &newEmptyText)
{
    d->emptyText = newEmptyText;
}

String LineEditWidget::emptyText() const
{
    return d->emptyText;
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
        if(!d->text.isEmpty())
        {
            d->text.truncate(d->text.length() - 1);
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }
        return true;
    }

    if(ev->data1 >= ' ' && ev->data1 <= 'z')
    {
        char ch = char(ev->data1);
        if(shiftdown)
        {
            ch = shiftXForm[ch];
        }

        // Filter out nasty characters.
        if(ch == '%')
            return true;

        if(!d->maxLength || d->text.length() < d->maxLength)
        {
            d->text += ch;
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }
        return true;
    }

    return false;
}

int LineEditWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            Widget::_flags |= MNF_ACTIVE;
            Widget::timer = 0;
            // Store a copy of the present text value so we can restore it.
            d->oldText = d->text;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            d->oldText = d->text;
            Widget::_flags &= ~MNF_ACTIVE;
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }
        return true;
    }

    if(Widget::_flags & MNF_ACTIVE)
    {
        switch(cmd)
        {
        case MCMD_NAV_OUT:
            d->text = d->oldText;
            Widget::_flags &= ~MNF_ACTIVE;
            if(hasAction(MNA_CLOSE))
            {
                execAction(MNA_CLOSE);
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

void LineEditWidget::updateGeometry(Page * /*page*/)
{
    // @todo calculate visible dimensions properly.
    Rect_SetWidthHeight(_geometry, 170, 14);
}

CVarLineEditWidget::CVarLineEditWidget(char const *cvarPath)
    : LineEditWidget()
    , _cvarPath(cvarPath)
{
    Widget::actions[MNA_MODIFIED].callback = CvarLineEditWidget_UpdateCvar;
    Widget::actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
}

CVarLineEditWidget::~CVarLineEditWidget()
{}

char const *CVarLineEditWidget::cvarPath() const
{
    return _cvarPath;
}

void CvarLineEditWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action)
{
    DENG2_ASSERT(wi != 0);
    CVarLineEditWidget const &edit = wi->as<CVarLineEditWidget>();
    cvartype_t varType = Con_GetVariableType((char const *)edit.cvarPath());

    if(Widget::MNA_MODIFIED != action) return;

    switch(varType)
    {
    case CVT_CHARPTR:
        Con_SetString2((char const *)edit.cvarPath(), edit.text().toUtf8().constData(), SVF_WRITE_OVERRIDE);
        break;

    case CVT_URIPTR: {
        /// @todo Sanitize and validate against known schemas.
        de::Uri uri(edit.text(), RC_NULL);
        Con_SetUri2((char const *)edit.cvarPath(), reinterpret_cast<uri_s *>(&uri), SVF_WRITE_OVERRIDE);
        break; }

    default: break;
    }
}

ListWidget::Item::Item(String const &text, int userValue)
{
    setText(text);
    setUserValue(userValue);
}

void ListWidget::Item::setText(String const &newText)
{
    _text = newText;
}

String ListWidget::Item::text() const
{
    return _text;
}

void ListWidget::Item::setUserValue(int newUserValue)
{
    _userValue = newUserValue;
}

int ListWidget::Item::userValue() const
{
    return _userValue;
}

DENG2_PIMPL_NOREF(ListWidget)
{
    Items items;
    int selection = 0;  ///< Selected item (-1 if none).
    int first     = 0;  ///< First visible item.
    int numvis    = 0;

    ~Instance() { qDeleteAll(items); }
};

ListWidget::ListWidget()
    : Widget()
    , d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;
}

ListWidget::~ListWidget()
{}

ListWidget::Items &ListWidget::items()
{
    return d->items;
}

ListWidget::Items const &ListWidget::items() const
{
    return d->items;
}

void ListWidget::draw(Point2Raw const *_origin)
{
    DENG2_ASSERT(_origin != 0);

    bool const flashSelection = ((Widget::_flags & MNF_ACTIVE) && selectionIsVisible());
    float const *color = rs.textColors[_pageColorIdx];
    float dimColor[4], flashColor[4], t = flashSelection? 1 : 0;

    if(flashSelection && cfg.menuTextFlashSpeed > 0)
    {
        float const speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(_page->timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
    }

    lerpColor(flashColor, rs.textColors[_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    flashColor[CA] = color[CA];

    std::memcpy(dimColor, color, sizeof(dimColor));
    dimColor[CR] *= MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CG] *= MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CB] *= MNDATA_LIST_NONSELECTION_LIGHT;

    if(d->first < d->items.count() && d->numvis > 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(rs.textFonts[_pageFontIdx]);

        Point2Raw origin(*_origin);
        int itemIdx = d->first;
        do
        {
            Item const *item = d->items[itemIdx];

            if(d->selection == itemIdx)
            {
                FR_SetColorAndAlphav(flashSelection? flashColor : color);
            }
            else
            {
                FR_SetColorAndAlphav(dimColor);
            }

            FR_DrawText3(item->text().toUtf8().constData(), &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
            origin.y += FR_TextHeight(item->text().toUtf8().constData()) * (1+MNDATA_LIST_LEADING);
        } while(++itemIdx < d->items.count() && itemIdx < d->first + d->numvis);

        DGL_Disable(DGL_TEXTURE_2D);
    }
}

int ListWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_NAV_DOWN:
    case MCMD_NAV_UP:
        if(Widget::_flags & MNF_ACTIVE)
        {
            int oldSelection = d->selection;
            if(MCMD_NAV_DOWN == cmd)
            {
                if(d->selection < itemCount() - 1)
                    selectItem(d->selection + 1);
            }
            else
            {
                if(d->selection > 0)
                    selectItem(d->selection - 1);
            }

            if(d->selection != oldSelection)
            {
                S_LocalSound(cmd == MCMD_NAV_DOWN? SFX_MENU_NAV_DOWN : SFX_MENU_NAV_UP, NULL);
                if(hasAction(MNA_MODIFIED))
                {
                    execAction(MNA_MODIFIED);
                }
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_NAV_OUT:
        if(Widget::_flags & MNF_ACTIVE)
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            Widget::_flags &= ~MNF_ACTIVE;
            if(hasAction(MNA_CLOSE))
            {
                execAction(MNA_CLOSE);
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_SELECT:
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            Widget::_flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            Widget::_flags &= ~MNF_ACTIVE;
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }
        return true;

    default: return false; // Not eaten.
    }
}

int ListWidget::selection() const
{
    return d->selection;
}

int ListWidget::first() const
{
    return d->first;
}

bool ListWidget::selectionIsVisible() const
{
    return (d->selection >= d->first && d->selection < d->first + d->numvis);
}

void ListWidget::updateVisibleSelection()
{
    d->numvis = itemCount();
    if(d->selection >= 0)
    {
        if(d->selection < d->first)
            d->first = d->selection;
        if(d->selection > d->first + d->numvis - 1)
            d->first = d->selection - d->numvis + 1;
    }
}

int ListWidget::itemData(int index) const
{
    if(index >= 0 && index < itemCount())
    {
        return d->items[index]->userValue();
    }
    return 0;
}

int ListWidget::findItem(int userValue) const
{
    for(int i = 0; i < d->items.count(); ++i)
    {
        Item *item = d->items[i];
        if(item->userValue() == userValue)
        {
            return i;
        }
    }
    return -1;
}

bool ListWidget::selectItem(int itemIndex, int flags)
{
    if(itemIndex >= 0 && itemIndex < itemCount())
    {
        if(d->selection != itemIndex)
        {
            d->selection = itemIndex;
            if(!(flags & MNLIST_SIF_NO_ACTION) && hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
            return true;
        }
    }
    return false;
}

bool ListWidget::selectItemByValue(int userValue, int flags)
{
    return selectItem(findItem(userValue), flags);
}

InlineListWidget::InlineListWidget()
    : ListWidget()
{}

void InlineListWidget::draw(Point2Raw const *origin)
{
    Item const *item = items()[selection()];

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(rs.textFonts[_pageFontIdx]);
    FR_SetColorAndAlphav(rs.textColors[_pageColorIdx]);
    FR_DrawText3(item->text().toUtf8().constData(), origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

int InlineListWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_SELECT: // Treat as @c MCMD_NAV_RIGHT
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        int oldSelection = selection();

        if(MCMD_NAV_LEFT == cmd)
        {
            if(selection() > 0)
                selectItem(selection() - 1);
            else
                selectItem(itemCount() - 1);
        }
        else
        {
            if(selection() < itemCount() - 1)
                selectItem(selection() + 1);
            else
                selectItem(0);
        }

        updateVisibleSelection();

        if(oldSelection != selection())
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
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
    for(int i = 0; i < itemCount(); ++i)
    {
        Item *item = d->items[i];

        FR_TextSize(&itemGeometry.size, item->text().toUtf8().constData());
        if(i != itemCount() - 1)
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
    Item *item = items()[selection()];
    Size2Raw size;

    FR_SetFont(page->predefinedFont(mn_page_fontid_t(_pageFontIdx)));
    FR_TextSize(&size, item->text().toUtf8().constData());
    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

CVarInlineListWidget::CVarInlineListWidget(char const *cvarPath, int cvarValueMask)
    : InlineListWidget()
    , _cvarPath(cvarPath)
    , _cvarValueMask(cvarValueMask)
{
    Widget::_pageColorIdx  = MENU_COLOR3;
    Widget::actions[Widget::MNA_MODIFIED].callback = CvarInlineListWidget_UpdateCvar;
    Widget::actions[Widget::MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
}

CVarInlineListWidget::~CVarInlineListWidget()
{}

char const *CVarInlineListWidget::cvarPath() const
{
    return _cvarPath;
}

int CVarInlineListWidget::cvarValueMask() const
{
    return _cvarValueMask;
}

void CvarInlineListWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action)
{
    CVarInlineListWidget const *list = &wi->as<CVarInlineListWidget>();

    if(Widget::MNA_MODIFIED != action) return;

    if(list->selection() < 0) return; // Hmm?

    cvartype_t varType = Con_GetVariableType((char const *)list->cvarPath());
    if(CVT_NULL == varType) return;

    ListWidget::Item const *item = list->items()[list->selection()];
    int value;
    if(list->cvarValueMask())
    {
        value = Con_GetInteger((char const *)list->cvarPath());
        value = (value & ~list->cvarValueMask()) | (item->userValue() & list->cvarValueMask());
    }
    else
    {
        value = item->userValue();
    }

    switch(varType)
    {
    case CVT_INT:
        Con_SetInteger2((char const *)list->cvarPath(), value, SVF_WRITE_OVERRIDE);
        break;
    case CVT_BYTE:
        Con_SetInteger2((char const *)list->cvarPath(), (byte) value, SVF_WRITE_OVERRIDE);
        break;

    default: Con_Error("Hu_MenuCvarList: Unsupported variable type %i", (int)varType);
    }
}

DENG2_PIMPL_NOREF(ButtonWidget)
{
    String text;              ///< Label text.
    patchid_t patch = -1;     ///< Used when drawing this instead of text, if set.
    bool noAltText  = false;
    QVariant data;
};

ButtonWidget::ButtonWidget(String const &text, patchid_t patch)
    : Widget()
    , d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT2;
    Widget::_pageColorIdx = MENU_COLOR1;

    setText(text);
    setPatch(patch);
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

    if(d->patch >= 0)
    {
        String replacement;
        if(!d->noAltText)
        {
            replacement = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), d->patch, d->text);
        }

        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch(d->patch, replacement, Vector2i(origin->x, origin->y),
                     ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);

        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(d->text.toUtf8().constData(), origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

int ButtonWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            Widget::_flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        // We are not going to receive an "up event" so action that now.
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        Widget::_flags &= ~MNF_ACTIVE;
        if(hasAction(MNA_ACTIVEOUT))
        {
            execAction(MNA_ACTIVEOUT);
        }

        Widget::timer = 0;
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
    String useText = d->text;
    Size2Raw size;

    // @todo What if patch replacement is disabled?
    if(d->patch >= 0)
    {
        if(!d->noAltText)
        {
            // Use the replacement string?
            useText = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.menuPatchReplaceMode), d->patch, d->text);
        }

        if(useText.isEmpty())
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
    FR_TextSize(&size, useText.toUtf8().constData());

    Rect_SetWidthHeight(_geometry, size.width, size.height);
}

String ButtonWidget::text() const
{
    return d->text;
}

ButtonWidget &ButtonWidget::setText(String const &newText)
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

void ButtonWidget::setData(QVariant const &v)
{
    d->data = v;
}

QVariant const &ButtonWidget::data() const
{
    return d->data;
}

CVarToggleWidget::CVarToggleWidget(char const *cvarPath)
    : ButtonWidget()
    , _cvarPath(cvarPath)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR3;
    Widget::actions[MNA_MODIFIED].callback = CvarToggleWidget_UpdateCvar;
    Widget::actions[MNA_FOCUS   ].callback = Hu_MenuDefaultFocusAction;
}

CVarToggleWidget::~CVarToggleWidget()
{}

char const *CVarToggleWidget::cvarPath() const
{
    return _cvarPath;
}

int CVarToggleWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        bool justActivated = false;
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            justActivated = true;
            S_LocalSound(SFX_MENU_CYCLE, NULL);

            Widget::_flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }

        if(!justActivated)
        {
            Widget::_flags ^= MNF_ACTIVE;
        }

        if(Widget::data1)
        {
            void *data = Widget::data1;

            *((char *)data) = (Widget::_flags & MNF_ACTIVE) != 0;
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }

        if(!justActivated && !(Widget::_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }

        Widget::timer = 0;
        return true;
    }

    return false; // Not eaten.
}

void CvarToggleWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action)
{
    CVarToggleWidget *tog = &wi->as<CVarToggleWidget>();
    cvarbutton_t const *cb = (cvarbutton_t *)wi->data1;
    cvartype_t varType = Con_GetVariableType(tog->cvarPath());
    int value;

    if(Widget::MNA_MODIFIED != action) return;

    tog->setText(cb->active? cb->yes : cb->no);

    if(CVT_NULL == varType) return;

    if(cb->mask)
    {
        value = Con_GetInteger(tog->cvarPath());
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

    Con_SetInteger2(tog->cvarPath(), value, SVF_WRITE_OVERRIDE);
}

DENG2_PIMPL_NOREF(ColorPreviewWidget)
{
    bool rgbaMode       = false;
    Vector4f color      = Vector4f(0, 0, 0, 1.f);
    Vector2i dimensions = Vector2i(MNDATA_COLORBOX_WIDTH, MNDATA_COLORBOX_HEIGHT);  ///< Inner dimensions in fixed 320x200 space.
};

ColorPreviewWidget::ColorPreviewWidget(Vector4f const &color, bool rgbaMode)
    : Widget()
    , data1(0)
    , data2(0)
    , data3(0)
    , data4(0)
    , d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;

    d->rgbaMode = rgbaMode;
    d->color    = color;
    if(!d->rgbaMode) d->color.w = 1.f;
}

ColorPreviewWidget::~ColorPreviewWidget()
{}

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
    int w = d->dimensions.x;
    int h = d->dimensions.y;

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
    DGL_DrawRectf2Color(x, y, w, h, d->color.x, d->color.y, d->color.z, d->color.w * rs.pageAlpha);
}

int ColorPreviewWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_SELECT:
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            Widget::_flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            Widget::_flags &= ~MNF_ACTIVE;
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }
        return true;

    default: return false; // Not eaten.
    }
}

void ColorPreviewWidget::updateGeometry(Page * /*page*/)
{
    patchinfo_t info;

    Rect_SetWidthHeight(_geometry, d->dimensions.x, d->dimensions.y);

    // Add bottom border?
    if(R_GetPatchInfo(borderPatches[2], &info))
    {
        info.geometry.size.width = d->dimensions.x;
        info.geometry.origin.y   = d->dimensions.y;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add right border?
    if(R_GetPatchInfo(borderPatches[1], &info))
    {
        info.geometry.size.height = d->dimensions.y;
        info.geometry.origin.x    = d->dimensions.x;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top border?
    if(R_GetPatchInfo(borderPatches[0], &info))
    {
        info.geometry.size.width = d->dimensions.x;
        info.geometry.origin.y   = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add left border?
    if(R_GetPatchInfo(borderPatches[3], &info))
    {
        info.geometry.size.height = d->dimensions.y;
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
        info.geometry.origin.x = d->dimensions.x;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add bottom-right corner?
    if(R_GetPatchInfo(borderPatches[6], &info))
    {
        info.geometry.origin.x = d->dimensions.x;
        info.geometry.origin.y = d->dimensions.y;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add bottom-left corner?
    if(R_GetPatchInfo(borderPatches[7], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = d->dimensions.y;
        Rect_UniteRaw(_geometry, &info.geometry);
    }
}

void ColorPreviewWidget::setPreviewDimensions(Vector2i const &newDimensions)
{
    d->dimensions = newDimensions;
}

Vector2i ColorPreviewWidget::previewDimensions() const
{
    return d->dimensions;
}

bool ColorPreviewWidget::rgbaMode() const
{
    return d->rgbaMode;
}

Vector4f ColorPreviewWidget::color() const
{
    if(!d->rgbaMode)
    {
        return Vector4f(d->color, 1.0f);
    }
    return d->color;
}

bool ColorPreviewWidget::setRed(float red, int flags)
{
    float const oldRed = d->color.x;

    d->color.x = red;
    if(d->color.x != oldRed)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }
        return true;
    }
    return false;
}

bool ColorPreviewWidget::setGreen(float green, int flags)
{
    float const oldGreen = d->color.y;

    d->color.y = green;
    if(d->color.y != oldGreen)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }
        return true;
    }
    return false;
}

bool ColorPreviewWidget::setBlue(float blue, int flags)
{
    float const oldBlue = d->color.z;

    d->color.z = blue;
    if(d->color.z != oldBlue)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }
        return true;
    }
    return false;
}

bool ColorPreviewWidget::setAlpha(float alpha, int flags)
{
    if(d->rgbaMode)
    {
        float const oldAlpha = d->color.w;
        d->color.w = alpha;
        if(d->color.w != oldAlpha)
        {
            if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
            return true;
        }
    }
    return false;
}

bool ColorPreviewWidget::setColor(Vector4f const &newColor, int flags)
{
    int setComps = 0;
    int const setCompFlags = (flags | MNCOLORBOX_SCF_NO_ACTION);

    if(setRed  (newColor.x, setCompFlags)) setComps |= 0x1;
    if(setGreen(newColor.y, setCompFlags)) setComps |= 0x2;
    if(setBlue (newColor.z, setCompFlags)) setComps |= 0x4;
    if(setAlpha(newColor.w, setCompFlags)) setComps |= 0x8;

    if(!setComps) return false;

    if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED);
    }
    return true;
}

void CvarColorPreviewWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action)
{
    ColorPreviewWidget *cbox = &wi->as<ColorPreviewWidget>();

    if(action != Widget::MNA_MODIFIED) return;

    // MNColorBox's current color has already been updated and we know
    // that at least one of the color components have changed.
    // So our job is to simply update the associated cvars.
    Con_SetFloat2((char const *)cbox->data1, cbox->red(),   SVF_WRITE_OVERRIDE);
    Con_SetFloat2((char const *)cbox->data2, cbox->green(), SVF_WRITE_OVERRIDE);
    Con_SetFloat2((char const *)cbox->data3, cbox->blue(),  SVF_WRITE_OVERRIDE);
    if(cbox->rgbaMode())
    {
        Con_SetFloat2((char const *)cbox->data4, cbox->alpha(), SVF_WRITE_OVERRIDE);
    }
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

int SliderWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        float const oldvalue = _value;

        if(MCMD_NAV_LEFT == cmd)
        {
            _value -= step;
            if(_value < min)
                _value = min;
        }
        else
        {
            _value += step;
            if(_value > max)
                _value = max;
        }

        // Did the value change?
        if(oldvalue != _value)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }
        return true;
      }

    default: return false; // Not eaten.
    }
}

void CvarSliderWidget_UpdateCvar(Widget *wi, Widget::mn_actionid_t action)
{
    if(Widget::MNA_MODIFIED != action) return;

    SliderWidget &sldr = wi->as<SliderWidget>();
    cvartype_t varType = Con_GetVariableType((char const *)sldr.data1);
    if(CVT_NULL == varType) return;

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
