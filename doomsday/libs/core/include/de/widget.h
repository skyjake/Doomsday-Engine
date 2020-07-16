/** @file widget.h Base class for widgets.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_WIDGET_H
#define LIBCORE_WIDGET_H

/** @defgroup widgets  Widget Framework
 * @ingroup core */

#include "de/path.h"
#include "de/event.h"
#include "de/scripting/iobject.h"
#include "de/id.h"
#include "de/observers.h"
#include "de/string.h"

#include <functional>

namespace de {

class AssetGroup;
class Widget;
class RootWidget;

typedef List<Widget *> WidgetList;

/**
 * Base class for widgets.
 * @ingroup widgets
 */
class DE_PUBLIC Widget : public IObject
{
public:
    /// Widget that was expected to exist was not found. @ingroup errors
    DE_ERROR(NotFoundError);

    enum Behavior
    {
        /// Widget is invisible: not drawn. Hidden widgets also receive no events.
        Hidden = 0x1,

        /// Widget is disabled. The meaning of this is Widget-specific. Events
        /// will still be dispatched to the widget even though it's disabled.
        Disabled = 0x2,

        /// Widget will only receive events if it has focus.
        HandleEventsOnlyWhenFocused = 0x4,

        /// Widget cannot be hit by a pointer device.
        Unhittable = 0x8,

        /// Widget's content will not extend visually beyoud its boundaries.
        ContentClipping = 0x10,

        /// Children cannot be hit outside this widget's boundaries.
        ChildHitClipping = 0x20,

        /// No events will be dispatched to the widget (or its children).
        DisableEventDispatch = 0x40,

        /// No events will be dispatched to the children of the widget.
        DisableEventDispatchToChildren = 0x80,

        /// Children of the widget should be clipped when drawing.
        ChildVisibilityClipping = 0x100,

        /// Widget can receive input focus.
        Focusable = 0x200,

        DefaultBehavior = 0
    };
    using Behaviors = Flags;

    enum WalkDirection { Forward, Backward };

    typedef WidgetList Children;

    /**
     * Notified when the widget is about to be deleted.
     */
    DE_AUDIENCE(Deletion, void widgetBeingDeleted(Widget &widget))

    /**
     * Notified when the widget's parent changes.
     */
    DE_AUDIENCE(ParentChange, void widgetParentChanged(Widget &child, Widget *oldParent, Widget *newParent))

    /**
     * Notified when a child is added to the widget.
     */
    DE_AUDIENCE(ChildAddition, void widgetChildAdded(Widget &child))

    /**
     * Notified after a child has been removed from the widget.
     */
    DE_AUDIENCE(ChildRemoval, void widgetChildRemoved(Widget &child))

public:
    Widget(const String &name = String());
    virtual ~Widget();

    DE_CAST_METHODS()

    /**
     * Returns the automatically generated, unique identifier of the widget.
     */
    Id id() const;

    String name() const;
    void setName(const String &name);

    /**
     * Forms the dotted path of the widget. Assumes that the names of this
     * widget and its parents use dots to indicate hierarchy.
     */
    DotPath path() const;

    bool hasRoot() const;
    RootWidget &root() const;
    RootWidget *findRoot() const;
    void setRoot(RootWidget *root);

    bool hasFocus() const;
    virtual bool canBeFocused() const;

    void show(bool doShow = true);
    inline void hide() { show(false); }
    void enable(bool yes = true)   { setBehavior(Disabled, !yes); }
    void disable(bool yes = true)  { setBehavior(Disabled, yes); }

    inline bool isHidden() const   { return hasFamilyBehavior(Hidden); }
    inline bool isVisible() const  { return !isHidden(); }
    inline bool isDisabled() const { return hasFamilyBehavior(Disabled); }
    inline bool isEnabled() const  { return !isDisabled(); }

    /**
     * Determines if this widget or any of its parents have specific behavior
     * flags set.
     *
     * @param flags  Flags to check (all must be set).
     *
     * @return @c true, if behavior is set; otherwise @c false.
     */
    bool hasFamilyBehavior(const Behavior &flags) const;

    /**
     * Sets or clears one or more behavior flags.
     *
     * @param behavior   Flags to modify.
     * @param operation  Operation to perform on the flags.
     */
    void setBehavior(const Behaviors& behavior, FlagOpArg operation = SetFlags);

    /**
     * Clears one or more behavior flags.
     *
     * @param behavior   Flags to unset.
     */
    void unsetBehavior(const Behaviors& behavior);

    Behaviors behavior() const;

    /**
     * Sets the identifier of the widget that will receive focus when
     * a forward focus navigation is requested.
     *
     * @param name  Name of a widget for forwards navigation.
     */
    void setFocusNext(const String &name);

    /**
     * Sets the identifier of the widget that will receive focus when
     * a backwards focus navigation is requested.
     *
     * @param name  Name of a widget for backwards navigation.
     */
    void setFocusPrev(const String &name);

    String focusNext() const;
    String focusPrev() const;

    /**
     * Routines specific types of events to another widget. It will be
     * the only one offered those events.
     *
     * @param types    List of event types.
     * @param routeTo  Widget to route events to. Caller must ensure
     *                 that the widget is not destroyed while the
     *                 routing is in effect.
     */
    void setEventRouting(const List<int> &types, Widget *routeTo);

    void clearEventRouting();

    bool isEventRouted(int type, Widget *to) const;

    /**
     * Deletes all child widgets.
     */
    void clearTree();

    Widget &add(Widget *child);

    template <typename Type, typename... Args>
    inline Type &addNew(Args... args) {
        std::unique_ptr<Type> w(new Type(args...));
        add(w.get());
        return *w.release();
    }

    /**
     * Adds a child widget. It becomes the last child, meaning it is drawn on
     * top of other children but will get events first.
     *
     * @param child  Child widget.
     *
     * @return Reference to the added child widget. (Note that this is @em not
     * a "fluent API".)
     */
    Widget &addLast(Widget *child);

    Widget &addFirst(Widget *child);

    Widget &insertBefore(Widget *child, const Widget &otherChild);
    Widget *remove(Widget &child);
    Widget *find(const String &name);
    bool isInTree(const Widget &child) const;
    bool hasAncestor(const Widget &ancestorOrParent) const;
    const Widget *find(const String &name) const;
    void moveChildBefore(Widget *child, const Widget &otherChild);
    void moveChildToLast(Widget &child);
    Children children() const;
    dsize childCount() const;
    Widget *parent() const;
    inline Widget *parentWidget() const { return parent(); }    
    bool isFirstChild() const;
    bool isLastChild() const;

    template <typename Type>
    Type *ancestorOfType() const {
        for (Widget *w = parent(); w; w = w->parent())
        {
            if (auto *t = maybeAs<Type>(w)) return t;
        }
        return nullptr;
    }

    /**
     * Calls the given callback on each widget of the tree, starting from this widget.
     * The callback is not called for this widget.
     *
     * When walking forward, the callback is called on parents first, and then on
     * children in first-to-last order. When walking backward, the callback is first
     * called on children in last-to-first order, and finally the parent.
     *
     * The walk can be started at any widget in the tree, and it will descend and ascend
     * in the tree to find the next/previous widget.
     *
     * @param dir       Walk direction: Forward or Backward.
     * @param callback  Callback to call for each widget. Return LoopContinue to continue
     *                  the walk, anything else will abort.
     *
     * @return If the walk was aborted, returns the widget at which the callback returned
     * LoopAbort. Otherwise returns nullptr to indicate that all the available widgets
     * were handled.
     */
    Widget *walkInOrder(WalkDirection dir, const std::function<LoopResult (Widget &)>& callback);

    /**
     * Calls the given callback on each child of this widget. The full subtree of each
     * child is walked.
     *
     * @param dir       Walk direction: Forward or Backward.
     * @param callback  Callback to call for each widget. Return LoopContinue to continue
     *                  the walk, anything else will abort.
     *
     * @return If the walk was aborted, returns the widget at which the callback returned
     * LoopAbort. Otherwise returns nullptr to indicate that all the available children
     * were handled.
     */
    Widget *walkChildren(WalkDirection dir, const std::function<LoopResult (Widget &)>& callback);

    /**
     * Removes the widget from its parent, if it has a parent.
     */
    void orphan();

    // Utilities.
    String uniqueName(const String &name) const;

    /**
     * Arguments for notifyTree() and notifyTreeReversed().
     */
    struct NotifyArgs {
        enum Result {
            Abort,
            Continue
        };
        void (Widget::*notifyFunc)();
        bool (Widget::*conditionFunc)() const;
        void (Widget::*preNotifyFunc)(); ///< Pre and post callbacks must always be paired.
        void (Widget::*postNotifyFunc)();
        Widget *until;

        NotifyArgs(void (Widget::*notify)())
            : notifyFunc(notify)
            , conditionFunc(nullptr)
            , preNotifyFunc(nullptr)
            , postNotifyFunc(nullptr)
            , until(nullptr)
        {}
    };

    NotifyArgs notifyArgsForDraw() const;
    NotifyArgs::Result notifyTree(const NotifyArgs &args);
    NotifyArgs::Result notifySelfAndTree(const NotifyArgs &args);
    void notifyTreeReversed(const NotifyArgs &args);
    virtual bool dispatchEvent(const Event &event, bool (Widget::*memberFunc)(const Event &));

    enum class CollectMode { OnlyVisible, All };
    virtual void collectUnreadyAssets(AssetGroup &collected,
                                      CollectMode collectMode = CollectMode::OnlyVisible);

    /**
     * Blocks until all assets in the widget tree are Ready.
     */
    void waitForAssetsReady();

    // Events.
    virtual void initialize();
    virtual void deinitialize();
    virtual void viewResized();
    virtual void focusGained();
    virtual void focusLost();
    virtual void offerFocus();
    virtual void update();
    virtual void draw();
    virtual void preDrawChildren();
    virtual void postDrawChildren();
    virtual bool handleEvent(const Event &event);

    // Implements IObject.
    Record &      objectNamespace() override;
    const Record &objectNamespace() const override;

public:
    static void setFocusCycle(const WidgetList &order);

private:
    DE_PRIVATE(d)
};

/**
 * Auto-nulled pointer to a Widget. Does not own the target widget.
 */
template <typename WidgetType>
class SafeWidgetPtr : DE_OBSERVES(Widget, Deletion)
{
public:
    SafeWidgetPtr(WidgetType *ptr = nullptr) {
        reset(ptr);
    }
    ~SafeWidgetPtr() {
        reset(nullptr);
    }
    void reset(WidgetType *ptr = nullptr) {
        if (_ptr) _ptr->Widget::audienceForDeletion() -= this;
        _ptr = ptr;
        if (_ptr) _ptr->Widget::audienceForDeletion() += this;
    }
    WidgetType *operator -> () const {
        return _ptr;
    }
    operator const WidgetType * () const {
        return _ptr;
    }
    operator WidgetType * () {
        return _ptr;
    }
    WidgetType *get() const {
        return _ptr;
    }
    explicit operator bool() const {
        return _ptr != nullptr;
    }
    void widgetBeingDeleted(Widget &widget) {
        if (&widget == _ptr) {
            _ptr = nullptr;
        }
    }

private:
    WidgetType *_ptr = nullptr;
};

} // namespace de

#endif // LIBCORE_WIDGET_H
