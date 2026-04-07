/** @file page.h  UI menu page.
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

#ifndef LIBCOMMON_UI_PAGE
#define LIBCOMMON_UI_PAGE

#include <de/list.h>
#include <de/string.h>
#include "widgets/widget.h"

namespace common {
namespace menu {

/// @todo refactor away.
struct mn_rendstate_t
{
    float pageAlpha;
    float textGlitter;
    float textShadow;
    de::Vec4f textColors[MENU_COLOR_COUNT];
    fontid_t textFonts[MENU_FONT_COUNT];
};
extern const mn_rendstate_t *mnRendState;

typedef de::List<Widget *> WidgetList;

/**
 * UI menu page (dialog).
 *
 * @todo Menu pages should not have a "fixed" previous page mechanism, this should
 * be described in a more flexible manner -ds
 *
 * @ingroup menu
 */
class Page
{
public:
    enum Flag
    {
        FixedLayout  = 0x1,  ///< Children are positioned using a fixed layout.
        NoScroll     = 0x2,  ///< Scrolling is disabled.

        DefaultFlags = 0
    };
    using Flags = de::Flags;

    using Children = WidgetList;

    using OnActiveCallback = std::function<void(Page &)>;
    using OnDrawCallback   = std::function<void(const Page &, const de::Vec2i &)>;
    using CommandResponder = std::function<int(Page &, menucommand_e)>;

public:
    /**
     * Construct a new menu Page.
     *
     * @param name    Symbolic name/identifier for the page.
     * @param origin  Origin of the page in fixed 320x200 space.
     * @param flags   Page flags.
     * @param drawer
     * @param cmdResponder
     */
    explicit Page(de::String              name,
                  const de::Vec2i &       origin       = {},
                  Flags                   flags        = DefaultFlags,
                  const OnDrawCallback &  drawer       = {},
                  const CommandResponder &cmdResponder = {});

    virtual ~Page();

    /**
     * Returns the symbolic name/identifier of the page.
     */
    de::String name() const;

    void setTitle(const de::String &newTitle);
    de::String title() const;

    void setOrigin(const de::Vec2i &newOrigin);
    de::Vec2i origin() const;

    Flags flags() const;
    de::Rectanglei viewRegion() const;

    void setX(int x);
    void setY(int y);

    void setLeftColumnWidth(float columnWidthPercentage = 0.6f);

    void setPreviousPage(Page *newPreviousPage);
    Page *previousPage() const;

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
        DE_ASSERT(widget != 0);
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
     * Provides access to the list of child widgets of the Page, for efficient traversal.
     */
    const Children &children() const;

    /**
     * Returns the in-page index of the given @a widget; otherwise @c -1
     */
    inline int indexOf(Widget *widget) {
        return children().indexOf(widget);
    }

    /**
     * Attempt to give focus to the widget specified. If @a newFocusWidget is not @c nullptr,
     * is present and is not currently in-focus, an out-focus action is first sent to the
     * presently focused widget, then this page's focused widget is set before finally
     * triggering an in-focus action on the new widget.
     *
     * @param newFocusWidget  Widget to be given focus. Use @c nullptr to clear.
     */
    void setFocus(Widget *newFocusWidget);

    /**
     * Returns a pointer to the currently focused widget, if any (may be @c nullptr).
     */
    Widget *focusWidget();

    /**
     * Returns the current time in tics since last page activation.
     */
    int timer();

    /// Call the ticker routine for each widget.
    void tick();

    /**
     * Draw this page.
     */
    void draw(float opacity = 1.f, bool showFocusCursor = true);

    /**
     * Change the function to callback on page activation to @a newCallback.
     *
     * @param newCallback  Function to callback on page activation. Use @c 0 to clear.
     */
    void setOnActiveCallback(const OnActiveCallback &newCallback);

    /**
     * Retrieve a predefined color triplet associated with this page by it's logical
     * page color identifier.
     *
     * @param id   Unique identifier of the predefined color being retrieved.
     */
    de::Vec3f predefinedColor(mn_page_colorid_t id);

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

    void activate();

    int handleCommand(menucommand_e cmd);

    void setUserValue(const de::Value &newValue);
    const de::Value &userValue() const;

private:
    DE_PRIVATE(d)
};

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_PAGE
