/** @file localserverdialog.h  Dialog for starting a local server.
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

#ifndef LOCALSERVERDIALOG_H
#define LOCALSERVERDIALOG_H

#include <de/libcore.h>
#include <de/term/inputdialog.h>

class LocalServerDialog : public de::term::InputDialogWidget
{
public:
    LocalServerDialog();

    de::duint16 port() const;
    de::String  gameMode() const;

protected:
    void prepare() override;
    void finish(int result) override;

private:
    DE_PRIVATE(d)
};

#endif // LOCALSERVERDIALOG_H
