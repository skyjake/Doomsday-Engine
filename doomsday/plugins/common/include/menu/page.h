/** @file page.h  UI menu page.
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

#ifndef LIBCOMMON_UI_PAGE
#define LIBCOMMON_UI_PAGE

#include <QList>
#include <QVariant>
#include <de/String>
#include "widgets/widget.h"

namespace common {
namespace menu {

/**
 * @defgroup menuPageFlags  Menu Page Flags
 */
///@{
#define MPF_LAYOUT_FIXED            0x1 ///< Page uses a fixed layout.
#define MPF_NEVER_SCROLL            0x2 ///< Page scrolling is disabled.
///@}

/// @todo refactor away.
struct mn_rendstate_t
{
    float pageAlpha;
    float textGlitter;
    float textShadow;
    float textColors[MENU_COLOR_COUNT][4];
    fontid_t textFonts[MENU_FONT_COUNT];
};
extern mn_rendstate_t const *mnRendState;

/**
 * UI menu page (dialog).
 *
 * @ingroup menu
 */
class Page
{
public:
    typedef QList<Widget *> Widgets;

    typedef void (*OnActiveCallback) (Page *);
    typedef void (*OnDrawCallback) (Page *, Point2Raw const *);

    typedef int (*CommandResponder) (Page *, menucommand_e);

public:
    /**
     * Construct a new menu Page.
     *
     * @param name    Symbolic name/identifier for the page.
     * @param origin  Origin of the page in fixed 320x200 space.
     * @param flags   @ref menuPageFlags.
     * ---
     * @param drawer
     * @param cmdResponder
     */
    Page(de::String name, Point2Raw const &origin = Point2Raw(), int flags = 0,
         OnDrawCallback drawer = 0,
         CommandResponder cmdResponder = 0);

    virtual ~Page();

    /**
     * Returns the symbolic name/identifier of the page.
     */
    de::String name() const;

    /**
     * Adds a widget object as a child widget of the Page and sets up the Widget -> Page
     * relationship. The object must be an instance of a class derived from Widget.
     *
     * @param widget  Widget object to add to the Page. The Page takes ownership.
     *
     * @return  Reference to @a widget, for caller convenience.
     */
    template <typename WidgetType>
    inline WidgetType &addWidget(WidgetType *widget) {
        DENG2_ASSERT(widget != 0);
        addWidget(static_cast<Widget *>(widget));
        return *widget;
    }

    /**
     * Adds a Widget instance as a child widget of the Page and sets up the Widget -> Page
     * relationship.
     *
     * @param widget  Widget to add to the Page. The Page takes ownership.
     *
     * @return  Reference to @a widget, for caller convenience.
     */
    Widget &addWidget(Widget *widget);

    /**
     * Provides access to the list of child widgets of the Page, for efficient traversal.
     */
    Widgets const &widgets() const;

    /**
     * Returns the total number of child widgets of the Page.
     */
    inline int widgetCount() const { return widgets().count(); }

    void setTitle(de::String const &newTitle);
    de::String title() const;

    void setX(int x);
    void setY(int y);

    void setPreviousPage(Page *newPreviousPage);
    Page *previousPage() const;

    /**
     * Locate a widget on the page in the specified @a group.
     *
     * @param flags  @ref mnobjectFlags used to locate the widget. All flags specified
     *               must be set.
     * @param group  Widget group identifier.
     *
     * @return  Found widget.
     */
    Widget &findWidget(int flags, int group = 0);

    Widget *tryFindWidget(int flags, int group = 0);

    /**
     * Returns the in-page index of the given @a widget; otherwise @c -1
     */
    inline int indexOf(Widget *widget) {
        return widgets().indexOf(widget);
    }

    /**
     * Attempt to give focus to the given widget which is thought to be on the page.
     * If @a newFocusWidget is present and is not currently in-focus, an out-focus
     * action is first sent to the presently focused widget, then this page's focused
     * widget is set before finally executing an in-focus action on the new widget.
     * If the widget is not found on this page then nothing will happen.
     *
     * @param newFocusWidget  Widget to be given focus.
     */
    void setFocus(Widget *newFocusWidget);

    /**
     * Returns a pointer to the currently focused widget, if any (may be @c nullptr).
     */
    Widget *focusWidget();

    void clearFocusWidget();
    void refocus();

    /**
     * Returns the current time in tics since last page activation.
     */
    int timer();

    /// Call the ticker routine for each widget.
    void tick();

    /**
     * Draw this menu page.
     */
    void draw(float opacity = 1.f, bool showFocusCursor = true);

    /**
     * Change the function to callback on page activation to @a newCallback.
     *
     * @param newCallback  Function to callback on page activation. Use @c 0 to clear.
     */
    void setOnActiveCallback(OnActiveCallback newCallback);

    /**
     * Retrieve a predefined color triplet associated with this page by it's logical
     * page color identifier.
     *
     * @param id   Unique identifier of the predefined color being retrieved.
     * @param rgb  Found color values are written here, else set to white.
     */
    void predefinedColor(mn_page_colorid_t id, float rgb[3]);

    void setPredefinedFont(mn_page_fontid_t id, fontid_t fontId);

    /**
     * Retrieve a predefined Doomsday font-identifier associated with this page
     * by it's logical page font identifier.
     *
     * @param id  Unique identifier of the predefined font being retrieved.
     *
     * @return  Identifier of the found font else @c 0
     */
    fontid_t predefinedFont(mn_page_fontid_t id);

    /**
     * Returns the effective line height for the predefined @c MENU_FONT1.
     *
     * @param lineOffset  If not @c 0 the line offset is written here.
     */
    int lineHeight(int *lineOffset = 0);

    void initialize();
    void initWidgets();
    void updateWidgets();

    void applyLayout();

    int handleCommand(menucommand_e cmd);

    void setUserValue(QVariant const &newValue);
    QVariant const &userValue() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_PAGE
