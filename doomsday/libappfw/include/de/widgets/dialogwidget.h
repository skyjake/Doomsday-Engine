/** @file widgets/dialogwidget.h  Popup dialog.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_DIALOGWIDGET_H
#define LIBAPPFW_DIALOGWIDGET_H

#include "popupwidget.h"
#include "scrollareawidget.h"
#include "menuwidget.h"

namespace de {

class GuiRootWidget;
class DialogContentStylist;

/**
 * Popup dialog.
 *
 * The content area of a dialog is scrollable. A menu with buttons is placed in
 * the bottom of the dialog, for the actions available to the user.
 *
 * The contents of the dialog should be placed under the scroll area returned
 * by DialogWidget::content() and positioned in relation to its content rule.
 * When the dialog is set up, one must define the size of the content scroll
 * area (width and height rules).
 *
 * Note that when a widget is added to the content area, the dialog
 * automatically applies certain common style parameters (margins, backgrounds,
 * etc.).
 *
 * @par Widget Structure
 *
 * Dialogs are composed of several child widgets:
 * <pre>
 * DialogWidget    (PopupWidget)
 *  +- container   (GuiWidget; the popup content widget)
 *      +- heading (LabelWidget; optional)
 *      +- area    (ScrollAreaWidget; contains actual dialog widgets)
 *      +- buttons (MenuWidget)
 *      +- extra   (MenuWidget; might be empty)
 * </pre>
 *
 * Scrolling is set up so that the dialog height doesn't surpass the view
 * rectangle's height. Contents of the "area" widget scroll while the other
 * elements remain static in relation to the container.
 */
class LIBAPPFW_PUBLIC DialogWidget : public PopupWidget
{
    Q_OBJECT

public:
    /**
     * Modality of the dialog. By default, dialogs are modal, meaning that
     * while they are open, no events can get past the dialog.
     */
    enum Modality {
        Modal,
        NonModal
    };

    enum Flag {
        DefaultFlags = 0,
        WithHeading  = 0x1      ///< Dialog has a heading above the content area.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum RoleFlag {
        None    = 0,
        Default = 0x1, ///< Pressing Space or Enter will activate this.
        Accept  = 0x2,
        Reject  = 0x4,
        Yes     = 0x8,
        No      = 0x10,
        Action  = 0x20,

        IdMask  = 0xff0000,
        Id1     = 0x010000,
        Id2     = 0x020000,
        Id3     = 0x030000,
        Id4     = 0x040000
    };
    Q_DECLARE_FLAGS(RoleFlags, RoleFlag)

    /**
     * All buttons in a dialog must be ButtonItem instances or instances of
     * derived classes.
     *
     * The DialogButtonItem typedef is provided for convenience.
     */
    class LIBAPPFW_PUBLIC ButtonItem : public ui::ActionItem
    {
    public:
        /**
         * Button with the role's default label and action.
         * @param flags  Role flags for the button.
         * @param label  Label for the button. If empty, the default label will be used.
         */
        ButtonItem(RoleFlags flags, String const &label = "");

        /**
         * Button with custom action.
         * @param flags   Role flags for the button.
         * @param label   Label for the button. If empty, the default label will be used.
         * @param action  Action for the button.
         */
        ButtonItem(RoleFlags flags, String const &label, RefArg<de::Action> action);

        ButtonItem(RoleFlags flags, Image const &image, RefArg<de::Action> action);

        ButtonItem(RoleFlags flags, Image const &image, String const &label, RefArg<de::Action> action);

        RoleFlags role() const { return _role; }

    private:
        RoleFlags _role;
    };

    /// Asked for a label that does not exist in the dialog. @ingroup errors
    DENG2_ERROR(UndefinedLabel);

public:
    DialogWidget(String const &name = "", Flags const &flags = DefaultFlags);

    Modality modality() const;

    /**
     * If the dialog was created using the WithHeading flag, this will return the
     * label used for the dialog heading.
     */
    LabelWidget &heading();

    ScrollAreaWidget &area();

    /**
     * Sets the rule for the minimum width of the dialog. The default is that the
     * dialog is at least as wide as the content area, or all the button widths
     * summed together.
     *
     * @param minWidth  Custom minimum width for the dialog.
     */
    void setMinimumContentWidth(Rule const &minWidth);

    MenuWidget &buttonsMenu();

    /**
     * Additional buttons of the dialog, laid out opposite to the normal dialog
     * buttons. These are used for functionality related to the dialog's content,
     * but they don't accept or reject the dialog. For instance, shortcut for
     * settings, showing what's new in an update, etc.
     *
     * @return Widget for dialog's extra buttons.
     */
    MenuWidget &extraButtonsMenu();

    ui::Data &buttons();

    ButtonWidget &buttonWidget(String const &label) const;

    ButtonWidget *buttonWidget(int roleId) const;

    /**
     * Sets the action that will be triggered if the dialog is accepted. The action
     * will be triggered after the dialog has started closing (called from
     * DialogWidget::finish()).
     *
     * @param action  Action to trigger after the dialog has been accepted.
     */
    void setAcceptanceAction(RefArg<de::Action> action);

    /**
     * Shows the dialog and blocks execution until the dialog is closed --
     * another event loop is started for event processing. Call either accept()
     * or reject() to dismiss the dialog.
     *
     * @param root  Root where to execute the dialog.
     *
     * @return Result code.
     */
    int exec(GuiRootWidget &root);

    /**
     * Opens the dialog as non-modal. The dialog must already be added to the
     * widget tree. Use accept() or reject() to close the dialog.
     */
    void open();

    ui::ActionItem *defaultActionItem();

    // Events.
    void update();
    bool handleEvent(Event const &event);

public slots:
    void accept(int result = 1);
    void reject(int result = 0);

signals:
    void accepted(int result);
    void rejected(int result);

protected:
    void preparePanelForOpening();

    /**
     * Derived classes can override this to do additional tasks before
     * execution of the dialog begins. DialogWidget::prepare() must be called
     * from the overridding methods. The focused widget is reset in the method.
     */
    virtual void prepare();

    /**
     * Handles any tasks needed when the dialog is closing.
     * DialogWidget::finish() must be called from overridding methods.
     *
     * @param result  Result code.
     */
    virtual void finish(int result);

private:
    DENG2_PRIVATE(d)
};

typedef DialogWidget::ButtonItem DialogButtonItem;

Q_DECLARE_OPERATORS_FOR_FLAGS(DialogWidget::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(DialogWidget::RoleFlags)

} // namespace de

#endif // LIBAPPFW_DIALOGWIDGET_H
