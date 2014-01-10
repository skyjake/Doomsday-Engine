/** @file foldpanelwidget.h  Folding panel.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_FOLDPANELWIDGET_H
#define LIBAPPFW_FOLDPANELWIDGET_H

#include "../PanelWidget"
#include "../ButtonWidget"

namespace de {

/**
 * Folding panel.
 *
 * You should first set the container of the folding panel with setContent().
 * This ensures that widgets added to the panel use the appropriate stylist.
 *
 * When dismissed, the panel contents are GL-deinitialized and removed from
 * the widget tree entirely.
 *
 * FoldPanelWidget creates a title button for toggling the panel open and
 * closed. It is the user's responsibility to lay out this button
 * appropriately.
 */
class LIBAPPFW_PUBLIC FoldPanelWidget : public PanelWidget
{
    Q_OBJECT

public:
    FoldPanelWidget(String const &name = "");

    ButtonWidget &title();

    void setContent(GuiWidget *content);

    GuiWidget &content() const;

public slots:
    void toggleFold();

protected:
    void preparePanelForOpening();
    void panelDismissed();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FOLDPANELWIDGET_H
