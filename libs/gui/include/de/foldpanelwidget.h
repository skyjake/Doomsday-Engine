/** @file foldpanelwidget.h  Folding panel.
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

#ifndef LIBAPPFW_FOLDPANELWIDGET_H
#define LIBAPPFW_FOLDPANELWIDGET_H

#include "de/panelwidget.h"
#include "de/buttonwidget.h"

namespace de {

/**
 * Folding panel.
 *
 * You should first set the container of the folding panel with setContent(). This
 * ensures that widgets added to the panel use the appropriate stylist.
 *
 * When the fold is closed, the panel contents are GL-deinitialized and removed from the
 * widget tree entirely.
 *
 * @note When the fold is closed, the content widget receives no update() notifications
 * or events because it is not part of the widget tree.
 *
 * If needed, FoldPanelWidget can create a title button for toggling the panel open and
 * closed. It is the user's responsibility to lay out this button appropriately.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC FoldPanelWidget : public PanelWidget
{
public:
    FoldPanelWidget(const String &name = {});

    /**
     * Creates a title button widget for toggling the fold open and closed.
     * The method does not add the title as a child to anything.
     *
     * @param text  Text.
     *
     * @return Button widget instance. Caller gets ownership.
     */
    ButtonWidget *makeTitle(const String &text = "");

    ButtonWidget &title();

    void setContent(GuiWidget *content);

    GuiWidget &content() const;
    
    void toggleFold();

public:
    static FoldPanelWidget *makeOptionsGroup(const String &name, const String &heading,
                                             GuiWidget *parent);

protected:
    void preparePanelForOpening();
    void panelDismissed();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_FOLDPANELWIDGET_H
