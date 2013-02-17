/** @file dialogwidget.h  Base class for modal dialogs.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_DIALOGWIDGET_H
#define LIBSHELL_DIALOGWIDGET_H

#include "TextWidget"

namespace de {
namespace shell {

/**
 * Base class for modal dialogs.
 */
class LIBSHELL_PUBLIC DialogWidget : public TextWidget
{
    Q_OBJECT

public:
    DialogWidget(String const &name = "");
    ~DialogWidget();

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
    bool handleEvent(Event const &event);

public slots:
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

signals:
    void accepted(int result);
    void rejected(int result);

private:
    struct Instance;
    Instance *d;
};

} // namespace shell
} // namespace shell

#endif // LIBSHELL_DIALOGWIDGET_H
