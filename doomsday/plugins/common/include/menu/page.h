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

struct Page
{
public:
    typedef QList<Widget *> Widgets;

public: /// @todo make private:
    de::String _name;
    Widgets _widgets;

    /// "Physical" geometry in fixed 320x200 screen coordinate space.
    Point2Raw origin;
    Rect *geometry;

    Page *previous;    ///< Previous page.
    ddstring_t title;  ///< Title of this page.
    int focus;         ///< Index of the currently focused widget else @c -1
    int flags;         ///< @ref menuPageFlags

    /// Predefined fonts objects on this page.
    fontid_t fonts[MENU_FONT_COUNT];

    /// Predefined colors for objects on this page.
    uint colors[MENU_COLOR_COUNT];

    /// Process time (the "tick") for this object.
    void (*ticker) (Page *page);

    /// Page drawing routine.
    void (*drawer) (Page *page, Point2Raw const *offset);

    /// Menu-command responder routine.
    int (*cmdResponder) (Page *page, menucommand_e cmd);

    /// Automatically called when the page is made activate/current.
    typedef void (*OnActiveCallback) (Page *);
    OnActiveCallback onActiveCallback;

    void *userData;
    int _timer;

public:
    Page(de::String name, Point2Raw const &origin = Point2Raw(), int flags = 0,
         void (*ticker) (Page *page) = 0,
         void (*drawer) (Page *page, Point2Raw const *origin) = 0,
         int (*cmdResponder) (Page *page, menucommand_e cmd) = 0,
         void *userData = 0);

    ~Page();

    /**
     * Returns the name of the page.
     */
    de::String name() const;

    Widgets const &widgets() const;

    inline int widgetCount() const { return widgets().count(); }

    void initialize();

    void initWidgets();
    void updateWidgets();

    /// Call the ticker routine for each widget.
    void tick();

    void setTitle(char const *title);

    void setX(int x);
    void setY(int y);

    void setPreviousPage(Page *newPreviousPage);

    void refocus();

    /// @return  Currently focused widget; otherwise @c 0.
    Widget *focusWidget();

    void clearFocusWidget();

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
     * Determines the size of the menu cursor for the currently focused widget. If
     * no widget is currently focused the default cursor size (i.e., the effective
     * line height for @c MENU_FONT1) is used. (Which means this should @em not be
     * called to determine whether the cursor is actually in use -- for that purpose,
     * use @ref focusWidget() instead).
     */
    int cursorSize();

    /**
     * Locate a widget on the page in the specified @a group.
     *
     * @param group  Widget group identifier.
     * @param flags  @ref mnobjectFlags used to locate the widget. All flags specified
     *               must be set.
     *
     * @return  Found widget.
     */
    Widget &findWidget(int group, int flags);

    Widget *tryFindWidget(int group, int flags);

    /**
     * Returns the in-page index of the given @a widget; otherwise @c -1
     */
    int indexOf(Widget *widget);

    /**
     * Retrieve a predefined color triplet associated with this page by it's logical
     * page color identifier.
     *
     * @param id   Unique identifier of the predefined color being retrieved.
     * @param rgb  Found color values are written here, else set to white.
     */
    void predefinedColor(mn_page_colorid_t id, float rgb[3]);

    /**
     * Retrieve a predefined Doomsday font-identifier associated with this page
     * by it's logical page font identifier.
     *
     * @param id  Unique identifier of the predefined font being retrieved.
     *
     * @return  Identifier of the found font else @c 0
     */
    fontid_t predefinedFont(mn_page_fontid_t id);

    void setPredefinedFont(mn_page_fontid_t id, fontid_t fontId);

    /**
     * Returns the effective line height for the predefined @c MENU_FONT1.
     *
     * @param lineOffset  If not @c 0 the line offset is written here.
     */
    int lineHeight(int *lineOffset = 0);

    /// @return  Current time in tics since page activation.
    int timer();

    void applyPageLayout();

    /**
     * Change the function to callback on page activation to @a newCallback.
     *
     * @param newCallback  Function to callback on page activation. Use @c 0 to clear.
     */
    void setOnActiveCallback(OnActiveCallback newCallback);

private:
    void giveChildFocus(Widget *wi, dd_bool allowRefocus);
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_PAGE
