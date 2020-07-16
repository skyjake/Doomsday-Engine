/** @file shell/inputdialog.h  Dialog for querying text from the user.
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

#ifndef LIBSHELL_INPUTDIALOGTEDGET_H
#define LIBSHELL_INPUTDIALOGTEDGET_H

#include "dialogwidget.h"

namespace de { namespace term {

class LabelWidget;
class LineEditWidget;
class MenuWidget;

/**
 * Dialog for querying text from the user.
 *
 * @ingroup textUi
 */
class DE_PUBLIC InputDialogWidget : public DialogWidget
{
public:
    InputDialogWidget(const String &name = {});

    LabelWidget &   label();
    LineEditWidget &lineEdit();
    MenuWidget &    menu();

    /**
     * Sets the width of the dialog. The default width is 50.
     *
     * @param width  Width of the dialog.
     */
    void setWidth(int width);

    void setDescription(const String &desc);
    void setPrompt(const String &prompt);
    void setText(const String &text);
    void setAcceptLabel(const String &label);
    void setRejectLabel(const String &label);

    void prepare();
    void finish(int result);

    /**
     * Returns the text that the user entered in the dialog. If the dialog
     * was rejected, the returned string is empy.
     */
    String text() const;

    /**
     * Returns the result from the DialogWidget.
     */
    int result() const;

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_INPUTDIALOGTEDGET_H
