/** @file foldpanelwidget.cpp  Folding panel.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/FoldPanelWidget"
#include "de/ProceduralImage"
#include "de/DialogContentStylist"
#include "de/SignalAction"

namespace de {

using namespace ui;

DENG2_PIMPL_NOREF(FoldPanelWidget)
{
    struct FoldImage : public ProceduralImage
    {
        FoldPanelWidget &fold;
        bool needUpdate;

        FoldImage(FoldPanelWidget &owner) : fold(owner), needUpdate(true)
        {}

        void update()
        {
            float h = fold.title().font().height().value();
            setSize(Vector2f(h, h));
        }

        void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
        {
            GuiRootWidget &root = fold.root();
            Atlas &atlas = root.atlas();
            ColorBank::Colorf const &textColor = fold.title().textColorf();

            verts.makeFlexibleFrame(rect.toRectanglei(), 5, textColor,
                                    atlas.imageRectf(root.roundCorners()));

            Rectanglef uv = atlas.imageRectf(root.fold());
            if(!fold.isOpen())
            {
                // Flip it.
                uv = Rectanglef(uv.bottomLeft(), uv.topRight());
            }
            verts.makeQuad(rect, textColor * Vector4f(1, 1, 1, .5f), uv);
        }
    };

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
    d->title->setAlignment(ui::AlignLeft);
    d->title->setTextLineAlignment(ui::AlignLeft);
    d->title->set(Background()); // no frame or background
    d->title->setAction(new SignalAction(this, SLOT(toggleFold())));
    d->title->setOpacity(.8f);

    // Fold indicator.
    d->title->setOverlayImage(new Instance::FoldImage(*this), ui::AlignRight);

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
