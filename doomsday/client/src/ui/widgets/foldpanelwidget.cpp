/** @file foldpanelwidget.cpp  Folding panel.
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

#include "ui/widgets/foldpanelwidget.h"
#include "DialogContentStylist"
#include "SignalAction"

using namespace de;
using namespace ui;

DENG2_PIMPL_NOREF(FoldPanelWidget)
{
    ButtonWidget *title;
    GuiWidget *container; ///< Held here while not part of the widget tree.
    DialogContentStylist stylist;

    Instance() : title(0), container(0) {}
};

FoldPanelWidget::FoldPanelWidget(String const &name) : PanelWidget(name), d(new Instance)
{
    d->title = new ButtonWidget;
    d->title->setSizePolicy(Expand, Expand);
    d->title->set(Background());
    d->title->setFont("heading");
    d->title->setAction(new SignalAction(this, SLOT(toggleFold())));
}

ButtonWidget &FoldPanelWidget::title()
{
    return *d->title;
}

void FoldPanelWidget::setContent(GuiWidget *content)
{
    d->stylist.setContainer(*content);

    if(!isOpen())
    {
        // We'll just take it and do nothing else yet.
        if(d->container)
        {
            d->container->deleteLater();
        }
        d->container = content;
        return;
    }

    PanelWidget::setContent(content);
}

void FoldPanelWidget::toggleFold()
{
    if(!isOpen())
    {
        open();
    }
    else
    {
        close(0);
    }
}

void FoldPanelWidget::preparePanelForOpening()
{
    if(d->container)
    {
        // Insert the content back into the panel.
        PanelWidget::setContent(d->container);
        d->container = 0;
    }

    PanelWidget::preparePanelForOpening();
}

void FoldPanelWidget::panelDismissed()
{
    PanelWidget::panelDismissed();

    content().notifySelfAndTree(&Widget::deinitialize);

    DENG2_ASSERT(d->container == 0);
    d->container = takeContent();
}
