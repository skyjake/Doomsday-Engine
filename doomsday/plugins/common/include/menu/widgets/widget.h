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
     * Logical Menu (object) Action identifiers. Associated with/to events which
     * produce/result-in callbacks made either automatically by this subsystem,
     * or "actioned" through the type-specific event/command responders of the
     * various widgets, according to their own widget-specific logic.
     */
    enum mn_actionid_t
    {
        MNA_NONE = -1,
        MNACTION_FIRST = 0,
        MNA_MODIFIED = MNACTION_FIRST, /// Object's internal "modified" status changed.
        MNA_ACTIVEOUT,              /// Deactivated i.e., no longer active.
        MNA_ACTIVE,                 /// Becomes "active".
        MNA_CLOSE,                  /// Normally means changed-state to be discarded.
        MNA_FOCUSOUT,               /// Loses selection "focus".
        MNA_FOCUS,                  /// Gains selection "focus".
        MNACTION_LAST = MNA_FOCUS
    };
    static int const MNACTION_COUNT = (MNACTION_LAST + 1 - MNACTION_FIRST);

    /**
     * Menu Action Info (Record). Holds information about an "actionable" menu
     * event, such as an object being activated or upon receiving focus.
     */
    struct mn_actioninfo_t
    {
        /// Callback to be made when this action is executed. Can be @c NULL in
        /// which case attempts to action this will be NOPs.
        ///
        /// @param ob      Object being referenced for this callback.
        /// @param action  Identifier of the Menu Action to be processed.
        typedef void (*ActionCallback) (Widget *wi, mn_actionid_t action);
        ActionCallback callback;
    };

    typedef void (*OnTickCallback) (Widget *);
    typedef int (*CommandResponder) (Widget *, menucommand_e);

public:
    Widget();
    virtual ~Widget();

    DENG2_AS_IS_METHODS()

    /// Draw this at the specified offset within the owning view-space.
    /// Can be @c NULL in which case this will never be drawn.
    virtual void draw(Point2Raw const * /*origin*/) {}

    /// Calculate geometry for this when visible on the specified page.
    virtual void updateGeometry(Page * /*page*/) {}

    /// Respond to the given (input) event @a ev. Can be @c NULL.
    /// @return  @c true if the event is eaten.
    virtual int handleEvent(event_t *ev);

    /// Respond to the given (input) event @a ev. Can be @c NULL.
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

    bool hasPage() const;

    /**
     * Change the Page attributed to the widget to @a newPage. Not that this will only
     * affect the Widget > Page side of the relationship.
     *
     * @param newPage  New Page to attribute. Use @c 0 to clear. Ownership unaffected.
     */
    Widget &setPage(Page *newPage);

    Page &page() const;

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
     * Retrieve the current geometry of object within the two-dimensioned
     * coordinate space of the owning object.
     *
     * @return  Rectangluar region of the parent space.
     */
    Rect *geometry();
    Rect const *geometry() const;

    /**
     * Retrieve the origin of the object within the two-dimensioned coordinate
     * space of the owning object.
     * @return  Origin point within the parent space.
     */
    inline Point2 const *origin() const { return Rect_Origin(geometry()); }

    /**
     * Retrieve the boundary dimensions of the object expressed as units of
     * the coordinate space of the owning object.
     * @return  Size of this object in units of the parent's coordinate space.
     */
    inline Size2 const *size() const { return Rect_Size(geometry()); }

    /**
     * Retreive the current fixed origin coordinates.
     *
     * @param ob  MNObject-derived instance.
     * @return  Fixed origin.
     */
    Point2Raw const *fixedOrigin() const;
    inline int fixedX() const { return fixedOrigin()->x; }
    inline int fixedY() const { return fixedOrigin()->y; }

    /**
     * Change the current fixed origin coordinates.
     *
     * @param ob  MNObject-derived instance.
     * @param origin  New origin coordinates.
     * @return  Reference to this Widget.
     */
    Widget &setFixedOrigin(Point2Raw const *origin);
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

    de::String const &helpInfo() const;
    Widget &setHelpInfo(de::String newHelpInfo);
    inline bool hasHelpInfo() const { return !helpInfo().isEmpty(); }

    /// @return  @c true if this object has a registered executeable action
    /// associated with the unique identifier @a action.
    bool hasAction(mn_actionid_t action);

    Widget &setAction(mn_actionid_t action, mn_actioninfo_t::ActionCallback callback);

    /**
     * Lookup the unique ActionInfo associated with the identifier @a id.
     * @return  Associated info if found else @c NULL.
     */
    mn_actioninfo_t const *action(mn_actionid_t action);

    /**
     * Execute the action associated with @a id
     * @param action  Identifier of the action to be executed (if found).
     * @return  Return value of the executed action else @c -1 if NOP.
     */
    void execAction(mn_actionid_t action);

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
