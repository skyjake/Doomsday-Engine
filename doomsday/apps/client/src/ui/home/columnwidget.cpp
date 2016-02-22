/** @file columnwidget.cpp  Home column.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/columnwidget.h"

#include <de/LabelWidget>
#include <de/StyleProceduralImage>
#include <de/Range>
#include <de/App>
#include <de/math.h>

#include <QColor>

using namespace de;

DENG_GUI_PIMPL(ColumnWidget)
{
    struct BackgroundImage : public StyleProceduralImage
    {
        Animation colorAnim { 0, Animation::Linear };
        bool needUpdate = false;

        BackgroundImage(DotPath const &styleImageId, ColumnWidget &owner)
            : StyleProceduralImage(styleImageId, owner)
        {}

        void setColor(Color const &color)
        {
            StyleProceduralImage::setColor(color);
            colorAnim.setValue(color.x, 0.5);
        }

        bool update() override
        {
            StyleProceduralImage::update();
            Size const newSize = owner().rule().size();
            if(newSize != size())
            {
                setSize(newSize);
                return true;
            }
            bool update = !colorAnim.done() || needUpdate;
            // Make sure one more update happens after the animation is done.
            if(!colorAnim.done()) needUpdate = true;
            return update;
        }

        void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect) override
        {
            if(!allocId().isNone())
            {
                Rectanglef uv = root().atlas().imageRectf(allocId());
                Vector2f const reduction(uv.width() / 40, uv.height() / 40);
                uv = uv.adjusted(reduction, -reduction);

                Rectanglef const norm = owner().normalizedRect();
                verts.makeQuad(rect,
                               Vector4f(colorAnim, colorAnim, colorAnim, 1.f),
                               Rectanglef(uv.topLeft + norm.topLeft     * uv.size(),
                                          uv.topLeft + norm.bottomRight * uv.size()));
            }
        }
    };

    bool highlighted;
    LabelWidget *back;
    ScrollAreaWidget *scrollArea;
    HeaderWidget *header;
    Rule const *maxContentWidth = nullptr;

    Instance(Public *i) : Base(i)
    {
        back = new LabelWidget;
        back->margins().setZero();

        scrollArea = new ScrollAreaWidget;
        scrollArea->setBehavior(ChildVisibilityClipping, UnsetFlags);
        scrollArea->enableIndicatorDraw(true);

        header = new HeaderWidget;
        scrollArea->add(header);

        header->rule()
                .setInput(Rule::Left,  scrollArea->contentRule().left())
                .setInput(Rule::Top,   scrollArea->contentRule().top())
                .setInput(Rule::Width, scrollArea->contentRule().width());
    }

    ~Instance()
    {
        releaseRef(maxContentWidth);
    }
};

ColumnWidget::ColumnWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    changeRef(d->maxContentWidth, Const(800)/*rule().width() - style().rules().rule("gap") * 2*/);

    AutoRef<Rule> contentMargin = (rule().width() - *d->maxContentWidth) / 2;
    d->scrollArea->margins()
            .setLeft(contentMargin)
            .setRight(contentMargin);

    d->back->rule().setRect(rule());
    d->scrollArea->rule().setRect(rule());

    add(d->back);
    add(d->scrollArea);

    setBackgroundImage("home.background.column");
}

void ColumnWidget::setBackgroundImage(DotPath const &imageId)
{
    d->back->setImage(new Instance::BackgroundImage(imageId, *this));
    //d->back->setImageFit(ui::FitToSize);
    //d->back->setSizePolicy(ui::Filled, ui::Filled);
}

ScrollAreaWidget &ColumnWidget::scrollArea()
{
    return *d->scrollArea;
}

HeaderWidget &ColumnWidget::header()
{
    return *d->header;
}

Rule const &ColumnWidget::maximumContentWidth() const
{
    return *d->maxContentWidth;
}

String ColumnWidget::configVariableName() const
{
    return ""; // Defaults to none.
}

Variable *ColumnWidget::configVariable() const
{
    String name = configVariableName();
    if(name.isEmpty()) return nullptr;
    return &App::config(name);
}

void ColumnWidget::setHighlighted(bool highlighted)
{
    d->highlighted = highlighted;

    auto &img = d->back->image()->as<Instance::BackgroundImage>();
    img.setColor(highlighted? Vector4f(1, 1, 1, 1) : Vector4f(.5f, .5f, .5f, 1.f));
}

bool ColumnWidget::dispatchEvent(Event const &event, bool (Widget::*memberFunc)(Event const &))
{
    // Observe mouse clicks occurring in the column.
    if(event.isMouse() && event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.state() == MouseEvent::Pressed &&
           d->scrollArea->contentRect().contains(mouse.pos()))
        {
            emit mouseActivity(this);
        }
    }

    return GuiWidget::dispatchEvent(event, memberFunc);
}
