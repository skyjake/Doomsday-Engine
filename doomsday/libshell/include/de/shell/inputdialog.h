/** @file inputdialog.h  Dialog for querying text from the user.
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

#ifndef LIBSHELL_INPUTDIALOG_H
#define LIBSHELL_INPUTDIALOG_H

#include "DialogWidget"

namespace de {
namespace shell {

class LabelWidget;
class LineEditWidget;
class MenuWidget;

/**
 * Dialog for querying text from the user.
 */
class InputDialog : public de::shell::DialogWidget
{
public:
    InputDialog(de::String const &name = "");

    LabelWidget &label();
    LineEditWidget &lineEdit();
    MenuWidget &menu();

    /**
     * Sets the width of the dialog. The default width is 50.
     *
     * @param width  Width of the dialog.
     */
    void setWidth(int width);

    void setDescription(String const &desc);
    void setPrompt(String const &prompt);
    void setText(String const &text);
    void setAcceptLabel(String const &label);
    void setRejectLabel(String const &label);

    void prepare();
    void finish(int result);

    /**
     * Returns the text that the user entered in the dialog. If the dialog
     * was rejected, the returned string is empy.
     */
    de::String text() const;

    /**
     * Returns the result from the DialogWidget.
     */
    int result() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_INPUTDIALOG_H
