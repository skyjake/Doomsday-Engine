/** @file documentpopupwidget.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_DOCUMENTPOPUPWIDGET_H
#define LIBAPPFW_DOCUMENTPOPUPWIDGET_H

#include "../DocumentWidget"
#include "../ButtonWidget"
#include "../PopupWidget"

namespace de {

/**
 * Utility widget that has a document inside a popup.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC DocumentPopupWidget : public PopupWidget
{
public:
    DocumentPopupWidget(String const &name = {});
    DocumentPopupWidget(ButtonWidget *actionButton, String const &name = {});

    void setPreferredHeight(Rule const &preferredHeight);
    void setPreferredHeight(Rule const &preferredHeight, Rule const &maxHeight);

    DocumentWidget &document();
    DocumentWidget const &document() const;

    ButtonWidget *button();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_DOCUMENTPOPUPWIDGET_H
