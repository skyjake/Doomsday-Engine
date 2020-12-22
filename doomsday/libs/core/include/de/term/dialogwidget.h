/** @file shell/dialogwidget.h  Base class for modal dialogs.
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

#ifndef LIBSHELL_DIALOGTEDGET_H
#define LIBSHELL_DIALOGTEDGET_H

#include "widget.h"
#include "textrootwidget.h"

namespace de { namespace term {

/**
 * Base class for modal dialogs.
 *
 * @ingroup textUi
 */
class DE_PUBLIC DialogWidget : public Widget
{
public:
    DE_AUDIENCE(Accept, void accepted(int result))
    DE_AUDIENCE(Reject, void rejected(int result))

public:
    DialogWidget(const String &name = String());

    /**
     * Shows the dialog and gives it focus. Execution is blocked until the
     * dialog is closed. Another event loop is started for event processing.
     * Call either accept() or reject() to dismiss the dialog.
     *
     * @param root  Root where to execute the dialog.
     *
     * @return Result code.
     */
    int exec(TextRootWidget &root);

    // Events.
    void draw();
    bool handleEvent(const Event &event);

    void accept(int result = 1);
    void reject(int result = 0);

protected:
    /**
     * Derived classes can override this to do additional tasks before
     * execution of the dialog begins. DialogWidget::prepare() must be called
     * from the overridding methods.
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
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_DIALOGTEDGET_H
