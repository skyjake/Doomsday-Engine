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

#include "de/FoldPanelWidget"
#include "de/ProceduralImage"
#include "de/DialogContentStylist"
#include "de/SignalAction"

namespace de {

using namespace ui;

DENG2_PIMPL_NOREF(FoldPanelWidget)
{
    /*
    struct FoldImage : public ProceduralImage
    {
        FoldPanelWidget &fold;
        bool needUpdate;

        FoldImage(FoldPanelWidget &owner) : fold(owner), needUpdate(true)
        {}

        void update()
        {
            setSize(fold.root().atlas().imageRect(fold.root().roundCorners()).size());
        }

        void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
        {
            GuiRootWidget &root = fold.root();
            Atlas &atlas = root.atlas();

            ColorBank::Colorf const &textColor   = fold.style().colors().colorf("text");
            ColorBank::Colorf accentColor = fold.style().colors().colorf("accent")
                    * Vector4f(1, 1, 1, fold.isOpen()? .5f : 1);

            verts.makeQuad(rect, accentColor,
                           atlas.imageRectf(root.roundCorners()));

            Vector2ui dotSize = atlas.imageRect(root.tinyDot()).size();
            verts.makeQuad(Rectanglef::fromSize(rect.middle() - dotSize/2,
                                                dotSize),
                           fold.isOpen()? accentColor : textColor,
                           atlas.imageRectf(root.tinyDot()));
        }
    };*/

    ButtonWidget *title; // not owned
    GuiWidget *container; ///< Held here while not part of the widget tree.
    DialogContentStylist stylist;    

    Instance() : title(0), container(0) {}
};

FoldPanelWidget::FoldPanelWidget(String const &name) : PanelWidget(name), d(new Instance)
{}

ButtonWidget *FoldPanelWidget::makeTitle(String const &text)
{
    d->title = new ButtonWidget;

    d->title->setSizePolicy(Expand, Expand);
    d->title->setText(text);
    d->title->setTextColor("accent");
    d->title->setHoverTextColor("text", ButtonWidget::ReplaceColor);
    d->title->setFont("heading");
    d->title->setTextLineAlignment(ui::AlignLeft);
    d->title->set(Background()); // no frame or background
    d->title->setAction(new SignalAction(this, SLOT(toggleFold())));
    d->title->setOpacity(.8f);

    // Icon is disabled for now, doesn't look quite right.
    //d->title->setImage(new Instance::FoldImage(*this));
    //d->title->setTextAlignment(ui::AlignRight); // Text is on the right from the image.

    return d->title;
}

ButtonWidget &FoldPanelWidget::title()
{
    DENG2_ASSERT(d->title != 0);
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
            d->container->guiDeleteLater();
        }
        d->container = content;
        return;
    }

    PanelWidget::setContent(content);
}

GuiWidget &FoldPanelWidget::content() const
{
    if(d->container)
    {
        return *d->container;
    }
    return PanelWidget::content();
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

    if(d->title)
    {
        d->title->setOpacity(1);
    }

    PanelWidget::preparePanelForOpening();
}

void FoldPanelWidget::panelDismissed()
{
    PanelWidget::panelDismissed();

    if(d->title)
    {
        d->title->setOpacity(.8f, .5f);
    }

    content().notifySelfAndTree(&Widget::deinitialize);

    DENG2_ASSERT(d->container == 0);
    d->container = takeContent();
}

} // namespace de
