/** @file widget.h  Base class for widgets.
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

#ifndef LIBCOMMON_UI_WIDGET
#define LIBCOMMON_UI_WIDGET

#include "doomsday.h"

#include <QFlags>
#include <QVariant>
#include <de/Rectangle>
#include <de/String>
#include "common.h"
#include "hu_lib.h"

namespace common {
namespace menu {

class Page;

/**
 * Base class from which all menu widgets must be derived.
 *
 * @ingroup menu
 */
class Widget
{
public:
    /// Required Page is presently missing. @ingroup errors
    DENG2_ERROR(MissingPageError);

    enum Flag
    {
        Hidden        = 0x1,
        Disabled      = 0x2,    ///< Currently disabled (non-interactive).
        Paused        = 0x4,    ///< Paused widgets do not tick.

        Active        = 0x10,   ///< In the active state (meaning is widget specific).
        Focused       = 0x20,   ///< Currently focused.
        NoFocus       = 0x40,   ///< Can never receive focus.
        DefaultFocus  = 0x80,   ///< Has focus by default.
        PositionFixed = 0x100,  ///< XY position is fixed and predefined; automatic layout does not apply.
        LayoutOffset  = 0x200,  ///< Predefined XY position is applied to the dynamic layout origin.

        /// @todo We need a new dynamic id mechanism.
        Id7           = 0x1000000,
        Id6           = 0x2000000,
        Id5           = 0x4000000,
        Id4           = 0x8000000,
        Id3           = 0x10000000,
        Id2           = 0x20000000,
        Id1           = 0x40000000,
        Id0           = 0x80000000,

        DefaultFlags  = 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Logical Action identifiers. Associated with/to events which trigger user-definable
     * callbacks according to widget-specific logic.
     */
    enum Action
    {
        Modified,     ///< The internal "modified" status was changed.
        Deactivated,  ///< Deactivated i.e., no longer active.
        Activated,    ///< Becomes "active".
        Closed,       ///< Normally means changed-state to be discarded.
        FocusLost,    ///< Loses selection "focus".
        FocusGained   ///< Gains selection "focus".
    };

    typedef void (*ActionCallback) (Widget &, Action);
    typedef void (*OnTickCallback) (Widget &);
    typedef int (*CommandResponder) (Widget &, menucommand_e);

public:
    Widget();
    virtual ~Widget();

    DENG2_AS_IS_METHODS()

    virtual void draw() const {}

    /// Update the geometry for this widget.
    virtual void updateGeometry() {}

    /// Respond to the given (input) event @a ev.
    /// @return  @c true if the event is eaten.
    virtual int handleEvent(event_t *ev);

    /// Respond to the given (input) event @a ev.
    /// @return  @c true if the event is eaten.
    virtual int handleEvent_Privileged(event_t *ev);

    /// Respond to the given menu @a command.
    /// @return  @c true if the command was eaten.
    virtual int handleCommand(menucommand_e command);

    /// Configure a custom command responder to override the default mechanism.
    Widget &setCommandResponder(CommandResponder newResponder);

    /// Delegate handling of @a command to the relevant responder.
    /// @return  @c true if the command was eaten.
    int cmdResponder(menucommand_e command);

    /// Process time (the "tick") for this object.
    virtual void tick();

    Widget &setOnTickCallback(OnTickCallback newCallback);

    /**
     * Returns @c true if a Page is presently attributed to the widget.
     * @see page(), setPage()
     */
    bool hasPage() const;

    /**
     * Change the Page attributed to the widget to @a newPage. Not that this will only
     * affect the Widget > Page side of the relationship.
     *
     * @param newPage  New Page to attribute. Use @c 0 to clear. Ownership unaffected.
     *
     * @see page(), hasPage()
     */
    Widget &setPage(Page *newPage);

    /**
     * Returns a reference to the Page presently attributed to the widget.
     * @see hasPage()
     */
    Page &page() const;

    /// Convenient method of returning a pointer to the presently attributed Page, if any.
    inline Page *pagePtr() const { return hasPage()? &page() : 0; }

    /**
     * Sets or clears one or more flags.
     *
     * @param flags      Flags to modify.
     * @param operation  Operation to perform on the flags.
     *
     * @return  Reference to this Widget.
     */
    Widget &setFlags(Flags flagsToChange, de::FlagOp operation = de::SetFlags);
    Flags flags() const;

    inline bool isActive()   const { return flags() & Active;   }
    inline bool isFocused()  const { return flags() & Focused;  }
    inline bool isHidden()   const { return flags() & Hidden;   }
    inline bool isDisabled() const { return flags() & Disabled; }
    inline bool isPaused()   const { return flags() & Paused;   }

    /**
     * Retrieve the current geometry of widget within the two-dimensioned
     * coordinate space of the owning object.
     *
     * @return  Rectangluar region of the parent space.
     */
    de::Rectanglei &geometry();
    de::Rectanglei const &geometry() const;

    /**
     * Retreive the current fixed origin coordinates.
     *
     * @param ob  MNObject-derived instance.
     * @return  Fixed origin.
     */
    de::Vector2i fixedOrigin() const;
    inline int fixedX() const { return fixedOrigin().x; }
    inline int fixedY() const { return fixedOrigin().y; }

    Widget &setFixedOrigin(de::Vector2i const &newOrigin);
    Widget &setFixedX(int x);
    Widget &setFixedY(int y);

    Widget &setGroup(int newGroup);
    int group() const;

    Widget &setShortcut(int ddkey);
    int shortcut();

    Widget &setColor(int newPageColor);
    int color() const;

    Widget &setFont(int newPageFont);
    int font() const;

    Widget &setHelpInfo(de::String newHelpInfo);
    de::String const &helpInfo() const;

    /**
     * Returns @c true if a triggerable action is defined for the specified @a id.
     */
    bool hasAction(Action id) const;

    Widget &setAction(Action id, ActionCallback callback);

    /**
     * Trigger the ActionCallback associated with @a id, if any.
     * @param id  Unique identifier of the action.
     */
    void execAction(Action id);

    Widget &setUserValue(QVariant const &newValue);
    QVariant const &userValue() const;

    Widget &setUserValue2(QVariant const &newValue);
    QVariant const &userValue2() const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Widget::Flags)

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_WIDGET
