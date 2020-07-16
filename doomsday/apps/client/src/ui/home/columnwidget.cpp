/** @file columnwidget.cpp  Home column.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/app.h>
#include <de/glprogram.h>
#include <de/image.h>
#include <de/labelwidget.h>
#include <de/range.h>
#include <de/styleproceduralimage.h>
#include <de/math.h>

using namespace de;

DE_GUI_PIMPL(ColumnWidget)
{
    /**
     * Procedural image for drawing the background of a column.
     */
    struct BackgroundImage : public StyleProceduralImage
    {
        AnimationVector3 colorAnim{Animation::Linear};
        bool needUpdate = false;

        BackgroundImage(const DotPath &styleImageId, ColumnWidget &owner)
            : StyleProceduralImage(styleImageId, owner)
        {}

        void setColor(const Color &color)
        {
            StyleProceduralImage::setColor(color);
            colorAnim.setValue(color, 0.5);
        }

        bool update() override
        {
            StyleProceduralImage::update();
            const Size newPtSize = GuiWidget::pixelsToPoints(owner().rule().size());
            if (newPtSize != pointSize())
            {
                setPointSize(newPtSize);
                return true;
            }
            bool update = !colorAnim.done() || needUpdate;
            // Make sure one more update happens after the animation is done.
            if (!colorAnim.done()) needUpdate = true;
            return update;
        }

        void glMakeGeometry(GuiVertexBuilder &verts, const Rectanglef &rect) override
        {
            if (!allocId().isNone())
            {
                Rectanglef uv = root().atlas().imageRectf(allocId());
                Vec2f const reduction(uv.width() / 40, uv.height() / 40);
                uv = uv.adjusted(reduction, -reduction);

                const Rectanglef norm = owner().normalizedRect();
                verts.makeQuad(rect,
                               Vec4f(colorAnim.value(), 1.f),
                               Rectanglef(uv.topLeft + norm.topLeft     * uv.size(),
                                          uv.topLeft + norm.bottomRight * uv.size()));

                const int edgeWidth = GuiWidget::pointsToPixels(1);
                const auto edgeUv = owner().root().atlas().imageRectf(owner().root().solidWhitePixel());
                verts.makeQuad(Rectanglef(rect.left(), rect.top(),
                                          edgeWidth, rect.height()),
                               Vec4f(0, 0, 0, 1), edgeUv);
                verts.makeQuad(Rectanglef(rect.right() - edgeWidth, rect.top(),
                                          edgeWidth, rect.height()),
                               Vec4f(0, 0, 0, 1), edgeUv);
            }
        }
    };

    bool              highlighted = false;
    LabelWidget *     back;
    ScrollAreaWidget *scrollArea;
    HeaderWidget *    header;
    const Rule *      maxContentWidth = nullptr;
    Vec4f             backTintColor;
    Animation         backSaturation{0.f, Animation::Linear};

    Impl(Public *i) : Base(i)
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

    ~Impl()
    {
        releaseRef(maxContentWidth);
    }

    DE_PIMPL_AUDIENCE(Activity)
};

DE_AUDIENCE_METHOD(ColumnWidget, Activity)

ColumnWidget::ColumnWidget(const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{
    changeRef(d->maxContentWidth, rule("home.column.content.width"));

    AutoRef<Rule> contentMargin = (rule().width() - *d->maxContentWidth) / 2;
    d->scrollArea->margins()
            .setLeft(contentMargin)
            .setRight(contentMargin);

    d->back->rule().setRect(rule());
    d->scrollArea->rule().setRect(rule());

    add(d->back);
    add(d->scrollArea);

    updateStyle();

    setBackgroundImage("home.background.column");
    setBehavior(ChildVisibilityClipping);
}

void ColumnWidget::setBackgroundImage(const DotPath &imageId)
{
    auto *img = new Impl::BackgroundImage(imageId, *this);
    img->setColor(d->backTintColor);
    d->back->setImage(img);
}

ScrollAreaWidget &ColumnWidget::scrollArea()
{
    return *d->scrollArea;
}

HeaderWidget &ColumnWidget::header()
{
    return *d->header;
}

const Rule &ColumnWidget::maximumContentWidth() const
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
    if (name.isEmpty()) return nullptr;
    return &App::config(name);
}

int ColumnWidget::tabShortcut() const
{
    return 0;
}

void ColumnWidget::setHighlighted(bool highlighted)
{
    if (d->highlighted != highlighted)
    {
        d->highlighted = highlighted;

        auto &img = d->back->image()->as<Impl::BackgroundImage>();
        img.setColor(highlighted? Vec4f(1) : d->backTintColor);

        d->backSaturation.setValue(highlighted? 1.f : 0.f, 0.5);
    }
}

bool ColumnWidget::isHighlighted() const
{
    return d->highlighted;
}

void ColumnWidget::update()
{
    GuiWidget::update();

    d->back->setSaturation(d->backSaturation);
}

void ColumnWidget::updateStyle()
{
    GuiWidget::updateStyle();

    d->backTintColor = Vec4f(style().colors().colorf("home.background.tint"), 1.f);
}

bool ColumnWidget::dispatchEvent(const Event &event, bool (Widget::*memberFunc)(const Event &))
{
    // Observe mouse clicks occurring in the column.
    if (event.type() == Event::MouseButton ||
        event.type() == Event::MouseWheel)
    {
        const MouseEvent &mouse = event.as<MouseEvent>();
        if ((mouse.motion() == MouseEvent::Wheel || mouse.state() == MouseEvent::Pressed) &&
            rule().recti().contains(mouse.pos()))
        {
            DE_NOTIFY(Activity, i) i->mouseActivity(this);
        }
    }

    return GuiWidget::dispatchEvent(event, memberFunc);
}
