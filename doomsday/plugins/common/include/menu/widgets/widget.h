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
#include <de/String>
#include "common.h"
#include "hu_lib.h"

namespace common {
namespace menu {

struct Page;

/**
 * @defgroup menuObjectFlags Menu Object Flags
 */
///@{
#define MNF_HIDDEN              0x1
#define MNF_DISABLED            0x2     ///< Can't be interacted with.
#define MNF_PAUSED              0x4     ///< Ticker not called.
#define MNF_CLICKED             0x8
#define MNF_ACTIVE              0x10    ///< Object active.
#define MNF_FOCUS               0x20    ///< Has focus.
#define MNF_NO_FOCUS            0x40    ///< Can't receive focus.
#define MNF_DEFAULT             0x80    ///< Has focus by default.
#define MNF_POSITION_FIXED      0x100   ///< XY position is fixed and predefined; automatic layout does not apply.
#define MNF_LAYOUT_OFFSET       0x200   ///< Predefined XY position is applied to the dynamic layout origin.
//#define MNF_LEFT_ALIGN          0x100
//#define MNF_FADE_AWAY           0x200 // Fade UI away while the control is active.
//#define MNF_NEVER_FADE          0x400

/// @todo We need a new dynamic id allocating mechanism.
#define MNF_ID7                 0x1000000
#define MNF_ID6                 0x2000000
#define MNF_ID5                 0x4000000
#define MNF_ID4                 0x8000000
#define MNF_ID3                 0x10000000
#define MNF_ID2                 0x20000000
#define MNF_ID1                 0x40000000
#define MNF_ID0                 0x80000000
///@}

enum flagop_t
{
    FO_CLEAR,
    FO_SET,
    FO_TOGGLE
};

/**
 * Base class from which all widgets must be derived.
 */
struct Widget
{
public:
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
        void (*callback) (Widget *wi, mn_actionid_t action);
    };

public:
    int _flags;        ///< @ref menuObjectFlags.

    /// Used with the fixed layout method for positioning this object in
    /// the owning page's coordinate space.
    Point2Raw _origin;

    /// DDKEY shortcut used to switch focus to this object directly.
    /// @c 0= no shortcut defined.
    int _shortcut;

    int _pageFontIdx;  ///< Index of the predefined page font to use when drawing this.
    int _pageColorIdx; ///< Index of the predefined page color to use when drawing this.

    mn_actioninfo_t actions[MNACTION_COUNT];

    void (*onTickCallback) (Widget *wi);

    /// Respond to the given (menu) @a command. Can be @c NULL.
    /// @return  @c true if the command is eaten.
    int (*cmdResponder) (Widget *wi, menucommand_e command);

    // Extra property values.
    void *data1;
    int data2;

    Rect *_geometry;  ///< Current geometry.
    Page *_page;      ///< MenuPage which owns this object (if any).

    int timer;

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

    /// Process time (the "tick") for this object.
    virtual void tick();

    bool hasPage() const;

    Page &page() const;

    inline Page *pagePtr() const { return hasPage()? &page() : 0; }

    int flags() const;

    Widget &setFlags(flagop_t op, int flags);

    /**
     * Retrieve the current geometry of object within the two-dimensioned
     * coordinate space of the owning object.
     *
     * @return  Rectangluar region of the parent space.
     */
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
     * @return  Same as @a ob for caller convenience.
     */
    Widget &setFixedOrigin(Point2Raw const *origin);
    Widget &setFixedX(int x);
    Widget &setFixedY(int y);

    int group() const;
    Widget &setGroup(int newGroup);

    int shortcut();
    Widget &setShortcut(int ddkey);

    /// @return  Index of the color used from the owning/active page.
    int color();

    /// @return  Index of the font used from the owning/active page.
    int font();

    de::String const &helpInfo() const;
    Widget &setHelpInfo(de::String newHelpInfo);
    inline bool hasHelpInfo() const { return !helpInfo().isEmpty(); }

    /// @return  @c true if this object has a registered executeable action
    /// associated with the unique identifier @a action.
    dd_bool hasAction(mn_actionid_t action);

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

private:
    DENG2_PRIVATE(d)
};

int Widget_DefaultCommandResponder(Widget *wi, menucommand_e command);

} // namespace menu
} // namespace common

#endif // LIBCOMMON_UI_WIDGET
