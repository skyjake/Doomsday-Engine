/** @file directoryarraywidget.cpp  Widget for an array of native directories.
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

#include "de/DirectoryArrayWidget"
#include "de/BaseGuiApp"
#include "de/BaseWindow"

#include <de/NativePath>
#include <de/TextValue>
#include <QFileDialog>

namespace de {

DENG2_PIMPL_NOREF(DirectoryArrayWidget)
{};

DirectoryArrayWidget::DirectoryArrayWidget(Variable &variable, String const &name)
    : VariableArrayWidget(variable, name)
    , d(new Impl)
{
    addButton().setText(tr("Add Folder..."));
    addButton().setActionFn([this] ()
    {
        // Use a native dialog to select the IWAD folder.
        DENG2_BASE_GUI_APP->beginNativeUIMode();

        QFileDialog dlg(nullptr,
                        tr("Select Folder"),
                        ".", "");
        dlg.setFileMode(QFileDialog::Directory);
        dlg.setReadOnly(true);
        //dlg.setNameFilter("*.wad");
        dlg.setLabelText(QFileDialog::Accept, tr("Select"));
        if (dlg.exec())
        {
            elementsMenu().items() << makeItem(TextValue(dlg.selectedFiles().at(0)));
            setVariableFromWidget();
        }

        DENG2_BASE_GUI_APP->endNativeUIMode();
    });

    updateFromVariable();
}

String DirectoryArrayWidget::labelForElement(Value const &value) const
{
    return NativePath(value.asText()).pretty();
}

} // namespace de
