/** @file messagedialog.h Dialog for showing a message.
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

#ifndef LIBAPPFW_MESSAGEDIALOG_H
#define LIBAPPFW_MESSAGEDIALOG_H

#include "de/dialogwidget.h"

namespace de {

/**
 * Dialog for showing a message.
 *
 * @ingroup dialogs
 */
class LIBGUI_PUBLIC MessageDialog : public DialogWidget
{
public:
    MessageDialog(const String &name = String());

    void useInfoStyle();

    LabelWidget &title();
    LabelWidget &message();

    void setLayoutWidth(const Rule &);

    enum LayoutBehavior { ExcludeHidden, IncludeHidden };

    /**
     * Derived classes should call this after they add or remove widgets in the
     * dialog content area.
     */
    void updateLayout(LayoutBehavior behavior = ExcludeHidden);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_MESSAGEDIALOG_H
