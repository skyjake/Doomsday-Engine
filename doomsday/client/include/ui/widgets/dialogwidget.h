/** @file dialogwidget.h  Popup dialog.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef DENG_CLIENT_DIALOGWIDGET_H
#define DENG_CLIENT_DIALOGWIDGET_H

#include "popupwidget.h"
#include "scrollareawidget.h"
#include "menuwidget.h"

class GuiRootWidget;

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
 * </pre>
 *
 * Scrolling is set up so that the dialog height doesn't surpass the view
 * rectangle's height. Contents of the "area" widget scroll while the other
 * elements remain static in relation to the container.
 */
class DialogWidget : public PopupWidget
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
        Action  = 0x20
    };
    Q_DECLARE_FLAGS(RoleFlags, RoleFlag)

    /**
     * All buttons in a dialog must be ButtonItem instances or instances of
     * derived classes.
     *
     * The DialogButtonItem typedef is provided for convenience.
     */
    class ButtonItem : public ui::ActionItem
    {
    public:
        /**
         * Button with the role's default label and action.
         * @param flags  Role flags for the button.
         * @param label  Label for the button. If empty, the default label will be used.
         */
        ButtonItem(RoleFlags flags, de::String const &label = "");

        /**
         * Button with custom action.
         * @param flags  Role flags for the button.
         * @param label  Label for the button. If empty, the default label will be used.
         */
        ButtonItem(RoleFlags flags, de::String const &label, de::Action *action);

        RoleFlags role() const { return _role; }

    private:
        RoleFlags _role;
    };

public:
    DialogWidget(de::String const &name = "", Flags const &flags = DefaultFlags);

    Modality modality() const;

    /**
     * If the dialog was created using the WithHeading flag, this will return the
     * label used for the dialog heading.
     */
    LabelWidget &heading();

    ScrollAreaWidget &area();

    MenuWidget &buttons();

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

    // Events.
    void update();
    bool handleEvent(de::Event const &event);

public slots:
    void accept(int result = 1);
    void reject(int result = 0);

signals:
    void accepted(int result);
    void rejected(int result);

protected:
    void preparePopupForOpening();

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

#endif // DENG_CLIENT_DIALOGWIDGET_H
