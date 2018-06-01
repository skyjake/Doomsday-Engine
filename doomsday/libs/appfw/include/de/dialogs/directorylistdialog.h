/** @file directorylistdialog.h  Dialog for editing a list of directories.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_DIRECTORYLISTDIALOG_H
#define LIBAPPFW_DIRECTORYLISTDIALOG_H

#include "../MessageDialog"
#include "../DirectoryArrayWidget"
#include <de/Id>

namespace de {

/**
 * Dialog for editing a list of directories.
 */
class LIBAPPFW_PUBLIC DirectoryListDialog : public MessageDialog
{
    Q_OBJECT

public:
    DirectoryListDialog(String const &name = String());

    Id addGroup(String const &title, String const &description);

    /**
     * Sets the list elements.
     * @param elements  Array of text strings, or a single TextValue.
     */
    void setValue(Id const &group, Value const &elements);

    /**
     * Returns the contents of the directory list.
     * Array of text strings, or a single TextValue.
     */
    Value const &value(Id const &group) const;

signals:
    void arrayChanged();

protected:
    void prepare() override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_DIRECTORYLISTDIALOG_H
