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

#include <de/App>
#include <de/GLProgram>
#include <de/LabelWidget>
#include <de/Range>
#include <de/StyleProceduralImage>
#include <de/math.h>

#include <QColor>

using namespace de;

DENG_GUI_PIMPL(ColumnWidget)
{
    /**
     * Procedural image for drawing the background of a column.
     */
    struct BackgroundImage : public StyleProceduralImage
    {
        AnimationVector3 colorAnim { Animation::Linear };
        bool needUpdate = false;

        BackgroundImage(DotPath const &styleImageId, ColumnWidget &owner)
            : StyleProceduralImage(styleImageId, owner)
        {}

        void setColor(Color const &color)
        {
            StyleProceduralImage::setColor(color);
            colorAnim.setValue(color, 0.5);
        }

        bool update() override
        {
            StyleProceduralImage::update();
            Size const newSize = owner().rule().size();
            if (newSize != size())
            {
                setSize(newSize);
                return true;
            }
            bool update = !colorAnim.done() || needUpdate;
            // Make sure one more update happens after the animation is done.
            if (!colorAnim.done()) needUpdate = true;
            return update;
        }

        void glMakeGeometry(GuiVertexBuilder &verts, Rectanglef const &rect) override
        {
            if (!allocId().isNone())
            {
                Rectanglef uv = root().atlas().imageRectf(allocId());
                Vector2f const reduction(uv.width() / 40, uv.height() / 40);
                uv = uv.adjusted(reduction, -reduction);

                Rectanglef const norm = owner().normalizedRect();
                verts.makeQuad(rect,
                               Vector4f(colorAnim.value(), 1.f),
                               Rectanglef(uv.topLeft + norm.topLeft     * uv.size(),
                                          uv.topLeft + norm.bottomRight * uv.size()));

                int const edgeWidth = GuiWidget::toDevicePixels(1);
                auto const edgeUv = owner().root().atlas().imageRectf(owner().root().solidWhitePixel());
                verts.makeQuad(Rectanglef(rect.left(), rect.top(),
                                          edgeWidth, rect.height()),
                               Vector4f(0, 0, 0, 1), edgeUv);
                verts.makeQuad(Rectanglef(rect.right() - edgeWidth, rect.top(),
                                          edgeWidth, rect.height()),
                               Vector4f(0, 0, 0, 1), edgeUv);
            }
        }
    };

    bool highlighted = false;
    LabelWidget *back;
    ScrollAreaWidget *scrollArea;
    HeaderWidget *header;
    Rule const *maxContentWidth = nullptr;
    Vector4f backTintColor;

    //GLProgram bgProgram;
    //GLUniform uSaturation { "uSaturation", GLUniform::Float }; // background saturation
    //GLUniform uBgColor    { "uColor",      GLUniform::Vec4 };
    Animation backSaturation { 0.f, Animation::Linear };

    Impl(Public *i) : Base(i)
    {
        back = new LabelWidget;
        //back->setCustomShader(&bgProgram);
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

    /*void glInit()
    {
        root().shaders().build(bgProgram, "generic.textured.hsv.color_ucolor")
                << uSaturation
                << uBgColor
                << root().uAtlas();
    }

    void glDeinit()
    {
        bgProgram.clear();
    }*/
};

ColumnWidget::ColumnWidget(String const &name)
    : GuiWidget(name)
    , d(new Impl(this))
{
    changeRef(d->maxContentWidth, Const(toDevicePixels(400)));

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

void ColumnWidget::setBackgroundImage(DotPath const &imageId)
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
    if (name.isEmpty()) return nullptr;
    return &App::config(name);
}

String ColumnWidget::tabShortcut() const
{
    return String();
}

void ColumnWidget::setHighlighted(bool highlighted)
{
    if (d->highlighted != highlighted)
    {
        d->highlighted = highlighted;

        auto &img = d->back->image()->as<Impl::BackgroundImage>();
        img.setColor(highlighted? Vector4f(1, 1, 1, 1) : d->backTintColor);

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
    /*d->uSaturation = d->backSaturation;
    d->uBgColor    = Vector4f(1, 1, 1, visibleOpacity());*/
}

/*void ColumnWidget::glInit()
{
    d->glInit();
}

void ColumnWidget::glDeinit()
{
    d->bgProgram.clear();
}*/

void ColumnWidget::updateStyle()
{
    GuiWidget::updateStyle();

    d->backTintColor = Vector4f(style().colors().colorf("home.background.tint"), 1.f);
}

bool ColumnWidget::dispatchEvent(Event const &event, bool (Widget::*memberFunc)(Event const &))
{
    // Observe mouse clicks occurring in the column.
    if (event.type() == Event::MouseButton ||
        event.type() == Event::MouseWheel)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if ((mouse.motion() == MouseEvent::Wheel || mouse.state() == MouseEvent::Pressed) &&
            rule().recti().contains(mouse.pos()))
        {
            emit mouseActivity(this);
        }
    }

    return GuiWidget::dispatchEvent(event, memberFunc);
}
