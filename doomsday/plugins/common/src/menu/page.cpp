/** @file page.cpp  UI menu page.
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
#include "menu/page.h"

#include "hu_menu.h"

/// @todo Page should not need knowledge of Widget specializations - remove all.
#include "menu/widgets/buttonwidget.h"
#include "menu/widgets/cvarcolorpreviewwidget.h"
#include "menu/widgets/cvarinlinelistwidget.h"
#include "menu/widgets/cvarlineeditwidget.h"
#include "menu/widgets/cvarsliderwidget.h"
#include "menu/widgets/cvartextualsliderwidget.h"
#include "menu/widgets/inlinelistwidget.h"
#include "menu/widgets/inputbindingwidget.h"
#include "menu/widgets/labelwidget.h"
#include "menu/widgets/mobjpreviewwidget.h"

using namespace de;

namespace common {
namespace menu {

// Page draw state.
static mn_rendstate_t rs;
mn_rendstate_t const *mnRendState = &rs;

DENG2_PIMPL(Page)
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

    fontid_t fonts[MENU_FONT_COUNT]; ///< Predefined. Used by all widgets.
    uint colors[MENU_COLOR_COUNT];   ///< Predefined. Used by all widgets.

    OnActiveCallback onActiveCallback = nullptr;
    OnDrawCallback drawer             = nullptr;
    CommandResponder cmdResponder     = nullptr;

    // User data values.
    QVariant userValue;

    Instance(Public *i) : Base(i)
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

    void updateAllWidgetGeometry()
    {
        // Update objects.
        for(Widget *wi : widgets)
        {
            FR_PushAttrib();
            Rect_SetXY(wi->geometry(), 0, 0);
            wi->updateGeometry(thisPublic);
            FR_PopAttrib();
        }
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
                oldFocused->setFlags(Widget::Focused, UnsetFlags);
            }
            else if(!allowRefocus)
            {
                return;
            }
        }

        focus = widgets.indexOf(wi);
        wi->setFlags(Widget::Focused);
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
    OnDrawCallback drawer, CommandResponder cmdResponder)
    : d(new Instance(this))
{
    std::memcpy(&d->origin, &origin, sizeof(d->origin));
    d->name         = name;
    d->flags        = flags;
    d->drawer       = drawer;
    d->cmdResponder = cmdResponder;
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

static inline bool widgetIsDrawable(Widget *wi)
{
    DENG2_ASSERT(wi);
    return !(wi->flags() & Widget::Hidden);
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
            if(!widgetIsDrawable(wi)) continue;

            Rect_SetXY(wi->geometry(), wi->fixedOrigin()->x, wi->fixedOrigin()->y);
            Rect_Unite(d->geometry, wi->geometry());
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

        if(!widgetIsDrawable(wi))
        {
            // Proceed to the next widget!
            i += 1;
            continue;
        }

        // If the widget has a fixed position, we will ignore it while doing
        // dynamic layout.
        if(wi->flags() & Widget::PositionFixed)
        {
            Rect_SetXY(wi->geometry(), wi->fixedOrigin()->x, wi->fixedOrigin()->y);
            Rect_Unite(d->geometry, wi->geometry());

            // To the next object.
            i += 1;
            continue;
        }

        // An additional offset requested?
        if(wi->flags() & Widget::LayoutOffset)
        {
            origin.x += wi->fixedOrigin()->x;
            origin.y += wi->fixedOrigin()->y;
        }

        Rect_SetXY(wi->geometry(), origin.x, origin.y);

        // Orient label plus button/inline-list/textual-slider pairs about a
        // vertical dividing line, with the label on the left, other widget
        // on the right.
        // @todo Do not assume pairing, a widget should designate it's label.
        if(wi->is<LabelWidget>() && nextWi)
        {
            if(widgetIsDrawable(nextWi) &&
               (nextWi->is<ButtonWidget>()         ||
                nextWi->is<InlineListWidget>()     ||
                nextWi->is<ColorPreviewWidget>()   ||
                nextWi->is<InputBindingWidget>()   ||
                nextWi->is<CVarTextualSliderWidget>()))
            {
                int const margin = lineOffset * 2;

                Rect_SetXY(nextWi->geometry(), margin + Rect_Width(wi->geometry()), origin.y);

                RectRaw united;
                origin.y += Rect_United(wi->geometry(), nextWi->geometry(), &united)
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

        Rect_Unite(d->geometry, wi->geometry());

        origin.y += Rect_Height(wi->geometry()) + lineOffset;

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

    FR_DrawTextXY3(buf, x, y, ALIGN_TOP, Hu_MenuMergeEffectWithDrawTextFlags(0));

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
    d->updateAllWidgetGeometry();

    // Back to default page render state.
    FR_PopAttrib();

    // We can now layout the widgets of this page.
    /// @todo Do not modify the page layout here.
    applyLayout();

    // Determine the origin of the focus object (this dictates the page scroll location).
    Widget *focused = focusWidget();
    if(focused && !widgetIsDrawable(focused))
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
        if(focused->flags() & Widget::Active)
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

        if(wi->flags() & Widget::Hidden) continue;

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
        if(wi->flags() & Widget::Active)
        {
            return;
        }
    }
    d->focus = -1;
    for(Widget *wi : d->widgets)
    {
        wi->setFlags(Widget::Focused, UnsetFlags);
    }
    refocus();
}

Widget &Page::findWidget(int flags, int group)
{
    if(Widget *wi = tryFindWidget(flags, group))
    {
        return *wi;
    }
    throw Error("Page::findWidget", QString("Failed to locate widget in group #%1 with flags %2").arg(group).arg(flags));
}

Widget *Page::tryFindWidget(int flags, int group)
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
    // widget and select it.
    if(0 > d->focus)
    {
        int i, giveFocus = -1;

        // First look for a default focus widget. There should only be one
        // but find the last with this flag...
        for(i = 0; i < d->widgets.count(); ++i)
        {
            Widget *wi = d->widgets[i];
            if((wi->flags() & Widget::DefaultFocus) && !(wi->isDisabled() || (wi->flags() & Widget::NoFocus)))
            {
                giveFocus = i;
            }
        }

        // No default focus? Find the first focusable widget.
        if(-1 == giveFocus)
        for(i = 0; i < d->widgets.count(); ++i)
        {
            Widget *wi = d->widgets[i];
            if(!(wi->isDisabled() || (wi->flags() & Widget::NoFocus)))
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
            LOGDEV_WARNING("No focusable widget");
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

    // (Re)init widgets.
    for(Widget *wi : d->widgets)
    {
        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            cvarbutton_t *cvb = (cvarbutton_t *) tog->data1;
            bool const activate = (cvb && cvb->active);
            tog->setFlags(Widget::Active, (activate? SetFlags : UnsetFlags));
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
        wi->setPage(this);
        wi->setFlags(Widget::Focused, UnsetFlags);
    }
}

/// Main task is to update objects linked to cvars.
void Page::updateWidgets()
{
    for(Widget *wi : d->widgets)
    {
        if(wi->is<LabelWidget>() || wi->is<MobjPreviewWidget>())
        {
            wi->setFlags(Widget::NoFocus);
        }
        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            if(tog->data1)
            {
                // This button has already been initialized.
                cvarbutton_t *cvb = (cvarbutton_t *) tog->data1;
                cvb->active = (Con_GetByte(tog->cvarPath()) & (cvb->mask? cvb->mask : ~0)) != 0;
                tog->setText(cvb->active? cvb->yes : cvb->no);
                continue;
            }

            // Find the cvarbutton representing this one.
            for(cvarbutton_t *cvb = mnCVarButtons; cvb->cvarname; cvb++)
            {
                if(!strcmp(tog->cvarPath(), cvb->cvarname) && tog->data2 == cvb->mask)
                {
                    cvb->active = (Con_GetByte(tog->cvarPath()) & (cvb->mask? cvb->mask : ~0)) != 0;
                    tog->data1 = (void *) cvb;
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
        if(CVarSliderWidget *sldr = wi->maybeAs<CVarSliderWidget>())
        {
            float value;
            if(sldr->floatMode())
                value = Con_GetFloat(sldr->cvarPath());
            else
                value = Con_GetInteger(sldr->cvarPath());
            sldr->setValue(value);
        }
        if(CVarColorPreviewWidget *cbox = wi->maybeAs<CVarColorPreviewWidget>())
        {
            cbox->setColor(Vector4f(Con_GetFloat(cbox->redCVarPath()),
                                    Con_GetFloat(cbox->greenCVarPath()),
                                    Con_GetFloat(cbox->blueCVarPath()),
                                    (cbox->rgbaMode()? Con_GetFloat(cbox->alphaCVarPath()) : 1.f)));
        }
    }
}

void Page::tick()
{
    // Call the ticker of each object.
    for(Widget *wi : d->widgets)
    {
        wi->tick();
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
        if(int result = focused->cmdResponder(cmd))
            return result;
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
        // Page navigation requires a focused widget.
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
            } while(++i < widgetCount() && (d->widgets[giveFocus]->flags() & (Widget::Disabled | Widget::NoFocus | Widget::Hidden)));

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

void Page::setUserValue(QVariant const &newValue)
{
    d->userValue = newValue;
}

QVariant const &Page::userValue() const
{
    return d->userValue;
}

} // namespace menu
} // namespace common
