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
#include "menu/widgets/cvarcoloreditwidget.h"
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
    String name;                     ///< Symbolic name/identifier.
    Children children;

    Vector2i origin;
    Rectanglei geometry;             ///< "Physical" geometry, in fixed 320x200 screen coordinate space.

    String title;                    ///< Title of this page.
    Page *previous = nullptr;        ///< Previous page.
    int focus      = -1;             ///< Index of the currently focused widget else @c -1
    Flags flags    = DefaultFlags;
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
        qDeleteAll(children);
    }

    void updateAllChildGeometry()
    {
        for(Widget *wi : children)
        {
            wi->geometry().moveTopLeft(Vector2i(0, 0));
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
        fontid_t oldFont = FR_Font();

        /// @kludge We cannot yet query line height from the font...
        FR_SetFont(self.predefinedFont(MENU_FONT1));
        int lh = FR_TextHeight("{case}WyQ");
        if(lineOffset)
        {
            *lineOffset = de::max(1.f, .5f + lh * .34f);
        }
        // Restore the old font.
        FR_SetFont(oldFont);

        return lh;
    }

    void applyLayout()
    {
        geometry.topLeft = Vector2i(0, 0);
        geometry.setSize(Vector2ui(0, 0));

        if(flags & FixedLayout)
        {
            for(Widget *wi : children)
            {
                if(wi->isHidden()) continue;

                wi->geometry().moveTopLeft(wi->fixedOrigin());
                geometry |= wi->geometry();
            }
            return;
        }

        // This page uses a dynamic layout.
        int lineOffset;
        int const lh = lineHeight(&lineOffset);

        Vector2i origin;

        for(int i = 0; i < children.count(); )
        {
            Widget *wi = children[i];
            Widget *nextWi = i + 1 < children.count()? children[i + 1] : 0;

            if(wi->isHidden())
            {
                // Proceed to the next widget.
                i += 1;
                continue;
            }

            // If the widget has a fixed position, we will ignore it while doing
            // dynamic layout.
            if(wi->flags() & Widget::PositionFixed)
            {
                wi->geometry().moveTopLeft(wi->fixedOrigin());
                geometry |= wi->geometry();

                // Proceed to the next widget.
                i += 1;
                continue;
            }

            // An additional offset requested?
            if(wi->flags() & Widget::LayoutOffset)
            {
                origin += wi->fixedOrigin();
            }

            wi->geometry().moveTopLeft(origin);

            // Orient label plus button/inline-list/textual-slider pairs about a
            // vertical dividing line, with the label on the left, other widget
            // on the right.
            // @todo Do not assume pairing, a widget should designate it's label.
            if(wi->is<LabelWidget>() && nextWi)
            {
                if(!nextWi->isHidden() &&
                   (nextWi->is<ButtonWidget>()         ||
                    nextWi->is<InlineListWidget>()     ||
                    nextWi->is<ColorEditWidget>()      ||
                    nextWi->is<InputBindingWidget>()   ||
                    nextWi->is<CVarTextualSliderWidget>()))
                {
                    int const margin = lineOffset * 2;

                    nextWi->geometry().moveTopLeft(Vector2i(margin + wi->geometry().width(), origin.y));

                    Rectanglei const united = wi->geometry() | nextWi->geometry();
                    geometry |= united;
                    origin.y += united.height() + lineOffset;

                    // Extra spacing between object groups.
                    if(i + 2 < children.count() && nextWi->group() != children[i + 2]->group())
                    {
                        origin.y += lh;
                    }

                    // Proceed to the next object!
                    i += 2;
                    continue;
                }
            }

            geometry |= wi->geometry();
            origin.y += wi->geometry().height() + lineOffset;

            // Extra spacing between object groups.
            if(nextWi && nextWi->group() != wi->group())
            {
                origin.y += lh;
            }

            // Proceed to the next object!
            i += 1;
        }
    }

    /// @pre @a wi is a child of this page.
    void giveChildFocus(Widget *newFocus, bool allowRefocus = false)
    {
        DENG2_ASSERT(newFocus != 0);

        if(Widget *focused = self.focusWidget())
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

        focus = self.indexOf(newFocus);
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
            if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
            {
                int value = Con_GetByte(tog->cvarPath()) & (tog->cvarValueMask()? tog->cvarValueMask() : ~0);
                tog->setState(value? CVarToggleWidget::Down : CVarToggleWidget::Up);
                tog->setText(tog->isDown()? tog->downText() : tog->upText());
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
            if(CVarColorEditWidget *cbox = wi->maybeAs<CVarColorEditWidget>())
            {
                cbox->setColor(Vector4f(Con_GetFloat(cbox->redCVarPath()),
                                        Con_GetFloat(cbox->greenCVarPath()),
                                        Con_GetFloat(cbox->blueCVarPath()),
                                        (cbox->rgbaMode()? Con_GetFloat(cbox->alphaCVarPath()) : 1.f)));
            }
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
        int focusedHeight = focused? focused->geometry().height() : 0;

        // Ensure the cursor is at least as tall as the effective line height for
        // the page. This is necessary because some mods replace the menu button
        // graphics with empty and/or tiny images (e.g., Hell Revealed 2).
        /// @note Handling this correctly would mean separate physical/visual
        /// geometries for menu widgets.
        return de::max(focusedHeight, lineHeight);
    }
};

Page::Page(String name, Vector2i const &origin, Flags const &flags,
    OnDrawCallback drawer, CommandResponder cmdResponder)
    : d(new Instance(this))
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

    DENG2_ASSERT(widget);
    d->children << widget;
    widget->setPage(this)
           .setFlags(Widget::Focused, UnsetFlags); // Not focused initially.
    return *widget;
}

Page::Children const &Page::children() const
{
    return d->children;
}

void Page::setOnActiveCallback(Page::OnActiveCallback newCallback)
{
    d->onActiveCallback = newCallback;
}

#if __JDOOM__ || __JDOOM64__
static inline String subpageText(int page = 0, int totalPages = 0)
{
    if(totalPages <= 0) return "";
    return String("Page %1/%2").arg(page).arg(totalPages);
}
#endif

static void drawNavigation(Vector2i const origin)
{
    int const currentPage = 0;//(page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1;
    int const totalPages  = 1;//(int)ceil((float)page->objectsCount/page->numVisObjects);
#if __JDOOM__ || __JDOOM64__
    DENG2_UNUSED(currentPage);
#endif

    if(totalPages <= 1) return;

#if __JDOOM__ || __JDOOM64__

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorv(cfg.common.menuTextColors[1]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(subpageText(currentPage, totalPages).toUtf8().constData(), origin.x, origin.y,
                   ALIGN_TOP, Hu_MenuMergeEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    GL_DrawPatch( pInvPageLeft[currentPage == 0 || (menuTime & 8)]           , origin - Vector2i(144, 0), ALIGN_RIGHT);
    GL_DrawPatch(pInvPageRight[currentPage == totalPages-1 || (menuTime & 8)], origin + Vector2i(144, 0), ALIGN_LEFT);

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

static void drawTitle(String const &title)
{
    if(title.isEmpty()) return;

    Vector2i origin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) - ((SCREENHEIGHT / 2 - 5) / cfg.common.menuScale));

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
        rs.textColors[i] = Vector4f(page.predefinedColor(mn_page_colorid_t(i)), alpha);
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

    Vector2i cursorOrigin;
    int focusedHeight = 0;
    if(focused)
    {
        focusedHeight = d->cursorSizeFor(focused, d->lineHeight());

        // Determine the origin and dimensions of the cursor.
        /// @todo Each object should define a focus origin...
        cursorOrigin.x = -1;
        cursorOrigin.y = focused->geometry().topLeft.y;

        /// @kludge
        /// We cannot yet query the subobjects of the list for these values
        /// so we must calculate them ourselves, here.
        if(ListWidget const *list = focused->maybeAs<ListWidget>())
        {
            if(focused->isActive() && list->selectionIsVisible())
            {
                FR_PushAttrib();
                FR_SetFont(predefinedFont(mn_page_fontid_t(focused->font())));
                focusedHeight = FR_CharHeight('A') * (1+MNDATA_LIST_LEADING);
                cursorOrigin.y += (list->selection() - list->first()) * focusedHeight;
                FR_PopAttrib();
            }
        }
        // kludge end
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(d->origin.x, d->origin.y, 0);

    // Apply page scroll?
    if(!(d->flags & NoScroll) && focused)
    {
        Rectanglei viewRegion;

        // Determine available screen region for the page.
        viewRegion.topLeft = Vector2i(0, d->origin.y);
        viewRegion.setSize(Vector2ui(SCREENWIDTH, SCREENHEIGHT - 40/*arbitrary but enough for the help message*/));

        // Is scrolling in effect?
        if(d->geometry.height() > viewRegion.height())
        {
            int const minY = -viewRegion.topLeft.y / 2 + viewRegion.height() / 2;
            if(cursorOrigin.y > minY)
            {
                int const scrollLimitY  = d->geometry.height() - viewRegion.height() / 2;
                int const scrollOriginY = de::min(cursorOrigin.y, scrollLimitY) - minY;
                DGL_Translatef(0, -scrollOriginY, 0);
            }
        }
    }

    // Draw all child widgets that aren't hidden.
    for(Widget *wi : d->children)
    {
        if(wi->isHidden()) continue;

        FR_PushAttrib();
        wi->draw();
        FR_PopAttrib();
    }

    // How about a focus cursor?
    /// @todo cursor should be drawn on top of the page drawer.
    if(showFocusCursor && focused)
    {
        Hu_MenuDrawFocusCursor(cursorOrigin, focusedHeight, alpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    drawTitle(d->title);

    // The page has its own drawer.
    if(d->drawer)
    {
        FR_PushAttrib();
        d->drawer(*this, d->origin);
        FR_PopAttrib();
    }

    // How about some additional help/information for the focused item?
    if(focused && !focused->helpInfo().isEmpty())
    {
        Vector2i helpOrigin(SCREENWIDTH / 2, (SCREENHEIGHT / 2) + ((SCREENHEIGHT / 2 - 5) / cfg.common.menuScale));
        Hu_MenuDrawPageHelp(focused->helpInfo(), helpOrigin);
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

void Page::setOrigin(Vector2i const &newOrigin)
{
    d->origin = newOrigin;
}

Vector2i Page::origin() const
{
    return d->origin;
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
    if(d->children.isEmpty() || d->focus < 0) return 0;
    return d->children[d->focus];
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
        DENG2_ASSERT(!"Page::Focus: Failed to determine index-in-page for widget.");
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

    if(d->children.isEmpty())
        return; // Presumably the widgets will be added later...

    // (Re)init widgets.
    for(Widget *wi : d->children)
    {
        if(CVarToggleWidget *tog = wi->maybeAs<CVarToggleWidget>())
        {
            tog->setFlags(Widget::Active, tog->isDown()? SetFlags : UnsetFlags);
        }
        if(ListWidget *list = wi->maybeAs<ListWidget>())
        {
            // Determine number of potentially visible items.
            list->updateVisibleSelection();
        }
    }

    d->refocus();

    if(d->onActiveCallback)
    {
        d->onActiveCallback(*this);
    }
}

void Page::tick()
{
    // Call the ticker of each child widget.
    for(Widget *wi : d->children)
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

Vector3f Page::predefinedColor(mn_page_colorid_t id)
{
    DENG2_ASSERT(VALID_MNPAGE_COLORID(id));
    uint const colorIndex = d->colors[id];
    return Vector3f(cfg.common.menuTextColors[colorIndex]);
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
        if(Widget *focused = focusWidget())
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

            if(giveFocus != indexOf(focusWidget()))
            {
                S_LocalSound(cmd == MCMD_NAV_UP? SFX_MENU_NAV_UP : SFX_MENU_NAV_DOWN, NULL);
                setFocus(d->children[giveFocus]);
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
