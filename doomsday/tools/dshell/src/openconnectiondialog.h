/** @file openconnectiondialog.h  Dialog for specifying address for opening a connection.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef OPENCONNECTIONDIALOG_H
#define OPENCONNECTIONDIALOG_H

#include <de/term/inputdialog.h>

/**
 * Dialog for specifying address for opening a connection.
 */
class OpenConnectionDialog : public de::term::InputDialogWidget
{
public:
    OpenConnectionDialog(const de::String &name = {});

    /**
     * Returns the address that the user entered in the dialog. If the dialog
     * was rejected, the returned string is empy.
     */
    de::String address() const;

protected:
    void finish(int result);
};

#endif // OPENCONNECTIONDIALOG_H
