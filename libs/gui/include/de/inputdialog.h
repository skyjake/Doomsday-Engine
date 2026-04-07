/** @file dialogs/inputdialog.h  Dialog for querying a string of text from the user.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_INPUTDIALOG_H
#define LIBAPPFW_INPUTDIALOG_H

#include "de/messagedialog.h"
#include "de/lineeditwidget.h"

namespace de {

/**
 * Dialog for querying a string of text from the user.
 *
 * @ingroup dialogs
 */
class LIBGUI_PUBLIC InputDialog : public MessageDialog
{
public:
    InputDialog(const String &name = String());

    LineEditWidget &editor();
    const LineEditWidget &editor() const;

protected:
    void preparePanelForOpening();
    void panelClosing();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_INPUTDIALOG_H
