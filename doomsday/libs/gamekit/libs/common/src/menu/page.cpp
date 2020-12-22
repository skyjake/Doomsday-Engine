/** @file page.cpp  UI menu page.
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
#include "menu/page.h"

#include "hu_menu.h"
#include "hu_stuff.h"

/// @todo Page should not need knowledge of Widget specializations - remove all.
#include "menu/widgets/buttonwidget.h"
#include "menu/widgets/cvarcoloreditwidget.h"
#include "menu/widgets/cvarinlinelistwidget.h"
#include "menu/widgets/cvarlineeditwidget.h"
#include "menu/widgets/cvarsliderwidget.h"
#include "menu/widgets/cvartextualsliderwidget.h"
#include "menu/widgets/inlinelistwidget.h"
#include "menu/widgets/inputbindingwidget.h"
#include "menu/widgets/labelwidget.h"
#include "menu/widgets/mobjpreviewwidget.h"

#include <de/animation.h>

using namespace de;

namespace common {
namespace menu {

// Page draw state.
static mn_rendstate_t rs;
const mn_rendstate_t *mnRendState = &rs;

DE_PIMPL(Page)
{
    String   name; ///< Symbolic name/identifier.
    Children children;

    Vec2i      origin;
    Rectanglei geometry; ///< "Physical" geometry, in fixed 320x200 screen coordinate space.
    Animation  scrollOrigin;
    Rectanglei viewRegion;
    int        leftColumnWidth = SCREENWIDTH * 6 / 10;

    String title;              ///< Title of this page.
    Page * previous = nullptr; ///< Previous page.
    int    focus    = -1;      ///< Index of the currently focused widget else @c -1
    Flags  flags    = DefaultFlags;
    int    timer    = 0;

    fontid_t fonts[MENU_FONT_COUNT]; ///< Predefined. Used by all widgets.
    uint     colors[MENU_COLOR_COUNT]; ///< Predefined. Used by all widgets.

    OnActiveCallback onActiveCallback;
    OnDrawCallback   drawer;
    CommandResponder cmdResponder;

    // User data values.
    std::unique_ptr<Value> userValue;

    Impl(Public *i) : Base(i)
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

    ~Impl()
    {
        deleteAll(children);
    }

    void updateAllChildGeometry()
    {
        for(Widget *wi : children)
        {
            wi->geometry().moveTopLeft(Vec2i(0, 0));
            wi->updateGeometry();
        }
    }

    /**
     * Returns the effective line height for the predefined @c MENU_FONT1.
     *
     * @param lineOffset  If not @c 0 the line offset is written here.
     */
    int lineHeight(int *lineOffset = 0)
    {
        /// @todo Kludge: We cannot yet query line height from the font...
        const fontid_t oldFont = FR_Font();
        FR_SetFont(self().predefinedFont(MENU_FONT1));
        int lh = FR_TextHeight("{case}WyQ");
        if (lineOffset)
        {
            *lineOffset = de::max(1.f, .5f + lh * .34f);
        }
        // Restore the old font.
        FR_SetFont(oldFont);
        return lh;
    }

    void applyLayout()
    {
        geometry.topLeft = Vec2i(0, 0);
        geometry.setSize(Vec2ui(0, 0));

        if (children.empty()) return;

        if (flags & FixedLayout)
        {
            for (Widget *wi : children)
            {
                if (!wi->isHidden())
                {
                    wi->geometry().moveTopLeft(wi->fixedOrigin());
                    geometry |= wi->geometry();
                }
            }
            return;
        }

        // This page uses a dynamic layout.
        int       lineOffset;
        const int lh = lineHeight(&lineOffset);
        int       prevGroup = children.front()->group();
        Widget *  prevWidget = nullptr;
        int       usedColumns = 0; // column flags for current row
        Vec2i     origin;
        int       rowHeight = 0;

        for (auto *wi : children)
        {
            if (wi->isHidden())
            {
                continue;
            }

            // If the widget has a fixed position, we will ignore it while doing
            // dynamic layout.
            if (wi->flags() & Widget::PositionFixed)
            {
                wi->geometry().moveTopLeft(wi->fixedOrigin());
                geometry |= wi->geometry();
                continue;
            }

            // Extra spacing between object groups.
            if (wi->group() != prevGroup)
            {
                origin.y += lh;
                prevGroup = wi->group();
            }

            // An additional offset requested?
            if (wi->flags() & Widget::LayoutOffset)
            {
                origin += wi->fixedOrigin();
            }

            int widgetColumns = (wi->flags() & (Widget::LeftColumn | Widget::RightColumn));
            if (widgetColumns == 0)
            {
                // Use both columns if neither specified.
                widgetColumns = Widget::LeftColumn | Widget::RightColumn;
            }

            // If this column is already used, move to the next row.
            if ((usedColumns & widgetColumns) != 0)
            {
                origin.y    += rowHeight;
                usedColumns = 0;
                rowHeight   = 0;
            }
            usedColumns |= widgetColumns;

            wi->geometry().moveTopLeft(origin);
            rowHeight = MAX_OF(rowHeight, wi->geometry().height() + lineOffset);

            if (wi->flags() & Widget::RightColumn)
            {
                // Move widget to the right side.
                wi->geometry().move(Vec2i(leftColumnWidth, 0));

                if (prevWidget && prevWidget->flags() & Widget::LeftColumn)
                {
                    // Align the shorter widget vertically.
                    if (prevWidget->geometry().height() < wi->geometry().height())
                    {
                        prevWidget->geometry().move(Vec2i(
                            0, (wi->geometry().height() - prevWidget->geometry().height()) / 2));
                    }
                    else
                    {
                        wi->geometry().move(Vec2i(
                            0, (prevWidget->geometry().height() - wi->geometry().height()) / 2));
                    }
                }
            }

            geometry |= wi->geometry();

            prevWidget = wi;
        }

        // Center horizontally.
        this->origin.x = SCREENWIDTH / 2 - geometry.width() / 2;
    }

    /// @pre @a wi is a child of this page.
    void giveChildFocus(Widget *newFocus, bool allowRefocus = false)
    {
        DE_ASSERT(newFocus != 0);

        if(Widget *focused = self().focusWidget())
        {
            if(focused != newFocus)
            {
                focused->execAction(Widget::FocusLost);
                focused->setFlags(Widget::Focused, UnsetFlags);
            }
            else if(!allowRefocus)
            {
                return;
            }
        }

        focus = self().indexOf(newFocus);
        newFocus->setFlags(Widget::Focused);
        newFocus->execAction(Widget::FocusGained);
    }

    void refocus()
    {
        // If we haven't yet visited this page then find a child widget to give focus.
        if(focus < 0)
        {
            Widget *newFocus = nullptr;

            // First look for a child with the default focus flag. There should only be one
            // but we'll choose the last with this flag...
            for(Widget *wi : children)
            {
                if(wi->isDisabled()) continue;
                if(wi->flags() & Widget::NoFocus) continue;
                if(!(wi->flags() & Widget::DefaultFocus)) continue;

                newFocus = wi;
            }

            // No default focus?
            if(!newFocus)
            {
                // Find the first focusable child.
                for(Widget *wi : children)
                {
                    if(wi->isDisabled()) continue;
                    if(wi->flags() & Widget::NoFocus) continue;

                    newFocus = wi;
                    break;
                }
            }

            if(newFocus)
            {
                giveChildFocus(newFocus);
            }
            else
            {
                LOGDEV_WARNING("No focusable widget");
            }
        }
        else
        {
            // We've been here before; re-focus on the last focused object.
            giveChildFocus(children[focus], true);
        }
    }

    void fetch()
    {
        for(Widget *wi : children)
        {
            if(CVarToggleWidget *tog = maybeAs<CVarToggleWidget>(wi))
            {
                int value = Con_GetByte(tog->cvarPath()) & (tog->cvarValueMask()? tog->cvarValueMask() : ~0);
                tog->setState(value? CVarToggleWidget::Down : CVarToggleWidget::Up);
                tog->setText(tog->isDown()? tog->downText() : tog->upText());
            }
            if(CVarInlineListWidget *list = maybeAs<CVarInlineListWidget>(wi))
            {
                int itemValue = Con_GetInteger(list->cvarPath());
                if(int valueMask = list->cvarValueMask())
                    itemValue &= valueMask;
                list->selectItemByValue(itemValue);
            }
            if(CVarLineEditWidget *edit = maybeAs<CVarLineEditWidget>(wi))
            {
                edit->setText(Con_GetString(edit->cvarPath()));
            }
            if(CVarSliderWidget *sldr = maybeAs<CVarSliderWidget>(wi))
            {
                float value;
                if(sldr->floatMode())
                    value = Con_GetFloat(sldr->cvarPath());
                else
                    value = Con_GetInteger(sldr->cvarPath());
                sldr->setValue(value);
            }
            if(CVarColorEditWidget *cbox = maybeAs<CVarColorEditWidget>(wi))
            {
                cbox->setColor(Vec4f(Con_GetFloat(cbox->redCVarPath()),
                                        Con_GetFloat(cbox->greenCVarPath()),
                                        Con_GetFloat(cbox->blueCVarPath()),
                                        (cbox->rgbaMode()? Con_GetFloat(cbox->alphaCVarPath()) : 1.f)));
            }
        }
    }

#if 0
    /**
     * Determines the size of the menu cursor for a focused widget. If no widget is currently
     * focused the default cursor size (i.e., the effective line height for @c MENU_FONT1)
     * is used.
     *
     * (Which means this should @em not be called to determine whether the cursor is in use).
     */
    int cursorSizeFor(Widget *focused, int lineHeight)
    {
        return lineHeight;
        /*
        int focusedHeight = focused? focused->geometry().height() : 0;

        // Ensure the cursor is at least as tall as the effective line height for
        // the page. This is necessary because some mods replace the menu button
        // graphics with empty and/or tiny images (e.g., Hell Revealed 2).
        /// @note Handling this correctly would mean separate physical/visual
        /// geometries for menu widgets.
        return de::max(focusedHeight, lineHeight);
        */
    }
#endif
};

Page::Page(String                  name,
           const Vec2i &           origin,
           Flags                   flags,
           const OnDrawCallback &  drawer,
           const CommandResponder &cmdResponder)
    : d(new Impl(this))
{
    d->origin       = origin;
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

Widget &Page::addWidget(Widget *widget)
{
    LOG_AS("Page");

    DE_ASSERT(widget);
    d->children << widget;
    widget->setPage(this)
           .setFlags(Widget::Focused, UnsetFlags); // Not focused initially.
    return *widget;
}

const Page::Children &Page::children() const
{
    return d->children;
}

void Page::setOnActiveCallback(const OnActiveCallback &newCallback)
{
    d->onActiveCallback = newCallback;
}

#if __JDOOM__ || __JDOOM64__
static inline String subpageText(int page = 0, int totalPages = 0)
{
    if(totalPages <= 0) return "";
    return Stringf("Page %i/%i", page, totalPages);
}
#endif

static void drawNavigation(Vec2i const origin)
{
    const int currentPage = 0;//(page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1;
    const int totalPages  = 1;//(int)ceil((float)page->objectsCount/page->numVisObjects);
#if __JDOOM__ || __JDOOM64__
    DE_UNUSED(currentPage);
#endif

    if(totalPages <= 1) return;

#if __JDOOM__ || __JDOOM64__

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorv(cfg.common.menuTextColors[1]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(subpageText(currentPage, totalPages), origin.x, origin.y,
                   ALIGN_TOP, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    GL_DrawPatch( pInvPageLeft[currentPage == 0 || (menuTime & 8)]           , origin - Vec2i(144, 0), ALIGN_RIGHT);
    GL_DrawPatch(pInvPageRight[currentPage == totalPages-1 || (menuTime & 8)], origin + Vec2i(144, 0), ALIGN_LEFT);

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

static void drawTitle(const String &title)
{
    if(title.isEmpty()) return;

    Vec2i origin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) - ((SCREENHEIGHT / 2 - 5) / cfg.common.menuScale));

    FR_PushAttrib();
    Hu_MenuDrawPageTitle(title, origin); origin.y += 16;
    drawNavigation(origin);
    FR_PopAttrib();
}

static void setupRenderStateForPageDrawing(Page &page, float alpha)
{
    rs.pageAlpha   = alpha;
    rs.textGlitter = cfg.common.menuTextGlitter;
    rs.textShadow  = cfg.common.menuShadow;

    for(int i = 0; i < MENU_FONT_COUNT; ++i)
    {
        rs.textFonts[i] = page.predefinedFont(mn_page_fontid_t(i));
    }
    for(int i = 0; i < MENU_COLOR_COUNT; ++i)
    {
        rs.textColors[i] = Vec4f(page.predefinedColor(mn_page_colorid_t(i)), alpha);
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

    // Object geometry is determined from properties defined in the
    // render state, so configure render state before we begin.
    setupRenderStateForPageDrawing(*this, alpha);

    d->updateAllChildGeometry();

    // We can now layout the widgets of this page.
    /// @todo Do not modify the page layout here.
    d->applyLayout();

    // Determine the origin of the cursor (this dictates the page scroll location).
    Widget *focused = focusWidget();
    if(focused && focused->isHidden())
    {
        focused = 0;
    }

    Vec2i cursorOrigin;
    if (focused)
    {
        // Determine the origin and dimensions of the cursor.
        /// @todo Each object should define a focus origin...
        cursorOrigin.x = -1;
        cursorOrigin.y = focused->geometry().middle().y;

        /*
        /// @kludge
        /// We cannot yet query the subobjects of the list for these values
        /// so we must calculate them ourselves, here.
        if (const ListWidget *list = maybeAs<ListWidget>(focused))
        {
            if (focused->isActive() && list->selectionIsVisible())
            {
                FR_PushAttrib();
                FR_SetFont(predefinedFont(mn_page_fontid_t(focused->font())));
                const int rowHeight = FR_CharHeight('A') * (1+MNDATA_LIST_LEADING);
                //cursorOrigin.y += (list->selection() - list->first()) * rowHeight + rowHeight/2;
                FR_PopAttrib();
            }
        }
        // kludge end
        */
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(d->origin.x, d->origin.y, 0);

    // Apply page scroll?
    if (!(d->flags & NoScroll) && focused)
    {
        // Determine available screen region for the page.
        d->viewRegion.topLeft = {0, 0}; //d->origin.y);
        d->viewRegion.setSize(Vec2ui(SCREENWIDTH, SCREENHEIGHT - d->origin.y - 35 /*arbitrary but enough for the help message*/));

        // Is scrolling in effect?
        if (d->geometry.height() > d->viewRegion.height())
        {
            d->scrollOrigin.setValue(
                        de::min(de::max(0, int(cursorOrigin.y - d->viewRegion.height() / 2)),
                                int(d->geometry.height() - d->viewRegion.height())), .35);

            DGL_Translatef(0, -d->scrollOrigin, 0);
        }
    }
    else
    {
        d->viewRegion = {{0, 0}, {SCREENWIDTH, SCREENHEIGHT}};
    }

    // Draw all child widgets that aren't hidden.
    for (Widget *wi : d->children)
    {
        if (!wi->isHidden())
        {
            FR_PushAttrib();
            wi->draw();
            FR_PopAttrib();
        }
    }

    // How about a focus cursor?
    /// @todo cursor should be drawn on top of the page drawer.
    if (showFocusCursor && focused)
    {
#if defined (__JDOOM__) || defined (__JDOOM64__)
        const float cursorScale = .75f;
#else
        const float cursorScale = 1.f;
#endif
        Hu_MenuDrawFocusCursor(cursorOrigin, cursorScale, alpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    drawTitle(d->title);

    // The page has its own drawer.
    if (d->drawer)
    {
        FR_PushAttrib();
        d->drawer(*this, d->origin);
        FR_PopAttrib();
    }

    // How about some additional help/information for the focused item?
    if (focused && !focused->helpInfo().isEmpty())
    {
        Vec2i helpOrigin(SCREENWIDTH / 2, SCREENHEIGHT - 5 / cfg.common.menuScale);
        Hu_MenuDrawPageHelp(focused->helpInfo(), helpOrigin);
    }
}

void Page::setTitle(const String &newTitle)
{
    d->title = newTitle;
}

String Page::title() const
{
    return d->title;
}

void Page::setOrigin(const Vec2i &newOrigin)
{
    d->origin = newOrigin;
}

Vec2i Page::origin() const
{
    return d->origin;
}

Page::Flags Page::flags() const
{
    return d->flags;
}

Rectanglei Page::viewRegion() const
{
    if (d->flags & NoScroll)
    {
        return {{0, 0}, {SCREENWIDTH, SCREENHEIGHT}};
    }
    return d->viewRegion.moved({0, int(d->scrollOrigin)});
}

void Page::setX(int x)
{
    d->origin.x = x;
}

void Page::setY(int y)
{
    d->origin.y = y;
}

void Page::setLeftColumnWidth(float columnWidthPercentage)
{
    d->leftColumnWidth = int(SCREENWIDTH * columnWidthPercentage);
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
    if(d->children.isEmpty() || d->focus < 0) return 0;
    return d->children[d->focus];
}

Widget &Page::findWidget(int flags, int group)
{
    if(Widget *wi = tryFindWidget(flags, group))
    {
        return *wi;
    }
    throw Error("Page::findWidget",
                stringf("Failed to locate widget in group #%i with flags %x", group, flags));
}

Widget *Page::tryFindWidget(int flags, int group)
{
    for(Widget *wi : d->children)
    {
        if(wi->group() == group && int(wi->flags() & flags) == flags)
            return wi;
    }
    return 0; // Not found.
}

void Page::setFocus(Widget *newFocus)
{
    // Are we clearing focus?
    if(!newFocus)
    {
        if(Widget *focused = focusWidget())
        {
            if(focused->isActive()) return;
        }

        d->focus = -1;
        for(Widget *wi : d->children)
        {
            wi->setFlags(Widget::Focused, UnsetFlags);
        }
        d->refocus();
        return;
    }

    int index = indexOf(newFocus);
    if(index < 0)
    {
        DE_ASSERT_FAIL("Page::Focus: Failed to determine index-in-page for widget.");
        return;
    }
    d->giveChildFocus(d->children[index]);
}

void Page::activate()
{
    LOG_AS("Page");

    d->fetch();

    // Reset page timer.
    d->timer = 0;

    if (d->children.empty())
    {
        return; // Presumably the widgets will be added later...
    }

    // Notify widgets on the page.
    for (Widget *wi : d->children)
    {
        wi->pageActivated();
    }

    d->refocus();

    if (d->onActiveCallback)
    {
        d->onActiveCallback(*this);
    }
}

void Page::tick()
{
    // Call the ticker of each child widget.
    for (Widget *wi : d->children)
    {
        wi->tick();
    }
    d->timer++;
}

fontid_t Page::predefinedFont(mn_page_fontid_t id)
{
    DE_ASSERT(VALID_MNPAGE_FONTID(id));
    return d->fonts[id];
}

void Page::setPredefinedFont(mn_page_fontid_t id, fontid_t fontId)
{
    DE_ASSERT(VALID_MNPAGE_FONTID(id));
    d->fonts[id] = fontId;
}

Vec3f Page::predefinedColor(mn_page_colorid_t id)
{
    DE_ASSERT(VALID_MNPAGE_COLORID(id));
    const uint colorIndex = d->colors[id];
    return Vec3f(cfg.common.menuTextColors[colorIndex]);
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
        if(int result = d->cmdResponder(*this, cmd))
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
        if (Widget *focused = focusWidget())
        {
            int i = 0, giveFocus = indexOf(focused);
            do
            {
                giveFocus += (cmd == MCMD_NAV_UP? -1 : 1);
                if(giveFocus < 0)
                    giveFocus = d->children.count() - 1;
                else if(giveFocus >= d->children.count())
                    giveFocus = 0;
            } while(++i < d->children.count() && (d->children[giveFocus]->flags() & (Widget::Disabled | Widget::NoFocus | Widget::Hidden)));

            if (giveFocus != indexOf(focusWidget()))
            {
                S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
                setFocus(d->children[giveFocus]);
                d->timer = 0;
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

void Page::setUserValue(const Value &newValue)
{
    d->userValue.reset(newValue.duplicate());
}

const Value &Page::userValue() const
{
    return *d->userValue;
}

} // namespace menu
} // namespace common
