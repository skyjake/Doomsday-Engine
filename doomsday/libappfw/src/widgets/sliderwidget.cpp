/** @file sliderwidget.cpp  Slider to pick a value within a range.
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

#include "de/SliderWidget"
#include "de/DialogWidget"
#include "de/LineEditWidget"
#include "de/TextDrawable"

#include <de/Drawable>
#include <de/MouseEvent>
#include <cmath>

namespace de {

using namespace ui;

/**
 * Popup for editing the value of a slider.
 */
class ValuePopup : public PopupWidget
{
public:
    ValuePopup(SliderWidget &slider)
    {
        setContent(_edit = new LineEditWidget);
        //_edit->setEmptyContentHint(tr("Enter value"));
        _edit->setSignalOnEnter(true);
        connect(_edit, SIGNAL(enterPressed(QString)), &slider, SLOT(setValueFromText(QString)));
        connect(_edit, SIGNAL(enterPressed(QString)), this, SLOT(close()));
        _edit->rule().setInput(Rule::Width, slider.style().rules().rule("slider.editor"));

        _edit->setText(QString::number(slider.value() * slider.displayFactor(), 'f',
                                       slider.precision()));
    }

    LineEditWidget &editor() const
    {
        return *_edit;
    }

    void preparePanelForOpening()
    {
        PopupWidget::preparePanelForOpening();
        root().setFocus(_edit);
    }

    void panelClosing()
    {
        PopupWidget::panelClosing();
        root().setFocus(0);
    }

private:
    LineEditWidget *_edit;
};

DENG_GUI_PIMPL(SliderWidget)
{
    ddouble value;
    Ranged range;
    ddouble step;
    int precision;
    ddouble displayFactor;
    String minLabel;
    String maxLabel;

    enum State {
        Inert,
        Hovering,
        Grabbed
    };
    State state;
    Vector2i grabFrom;
    ddouble grabValue;

    // Visualization.
    bool animating;
    Animation pos;
    int endLabelSize;
    Animation frameOpacity;
    ColorBank::Colorf textColor;
    ColorBank::Colorf invTextColor;

    // GL objects.
    enum Labels {
        Value,
        Start,
        End,
        NUM_LABELS
    };
    TextDrawable labels[NUM_LABELS];
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uColor;

    Instance(Public *i)
        : Base(i),
          value(0),
          range(0, 0),
          step(0),
          precision(0),
          displayFactor(1),
          state(Inert),
          animating(false),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uColor    ("uColor",     GLUniform::Vec4)
    {
        self.setFont("slider.label");

        frameOpacity.setValue(.25f);

        updateStyle();
    }

    void updateStyle()
    {
        textColor    = style().colors().colorf("text");
        invTextColor = style().colors().colorf("inverted.text");

        endLabelSize = style().rules().rule("slider.label").valuei();

        for(int i = 0; i < int(NUM_LABELS); ++i)
        {
            labels[i].setFont(i == Value? style().fonts().font("slider.value") : self.font());
            labels[i].setLineWrapWidth(endLabelSize);
        }
    }

    void glInit()
    {
        DefaultVertexBuf *buf = new DefaultVertexBuf;
        drawable.addBuffer(buf);

        shaders().build(drawable.program(), "generic.textured.color_ucolor")
                << uMvpMatrix << uColor << uAtlas();

        for(int i = 0; i < int(NUM_LABELS); ++i)
        {
            labels[i].init(atlas(), self.font());
        }

        updateValueLabel();
        updateRangeLabels();
    }

    void glDeinit()
    {
        drawable.clear();
        for(int i = 0; i < int(NUM_LABELS); ++i)
        {
            labels[i].deinit();
        }
    }

    Rectanglei contentRect() const
    {
        Vector4i margins = self.margins().toVector();
        return self.rule().recti().adjusted(margins.xy(), -margins.zw());
    }

    /// Determines the total area where the slider is moving.
    Rectanglei sliderRect() const
    {
        Rectanglei const rect = contentRect();
        return Rectanglei(Vector2i(rect.topLeft.x + endLabelSize,     rect.topLeft.y),
                          Vector2i(rect.bottomRight.x - endLabelSize, rect.bottomRight.y));
    }

    /// Determines the area where the slider currently is.
    Rectanglei sliderValueRect() const
    {
        Rectanglei const area = sliderRect();
        ddouble i = range.size() > 0? (pos - range.start) / range.size() : 0;
        return Rectanglei::fromSize(Vector2i(area.topLeft.x +
                                             (area.width() - endLabelSize) * i,
                                             area.topLeft.y),
                                    Vector2ui(endLabelSize, area.height()));
    }

    void updateGeometry()
    {
        Rectanglei rect;
        if(self.hasChangedPlace(rect))
        {
            self.requestGeometry();
        }

        // Update texts.
        for(int i = 0; i < int(NUM_LABELS); ++i)
        {
            if(labels[i].update())
            {
                //qDebug() << "label" << i << "updated";
                self.requestGeometry();
            }
        }

        if(!self.geometryRequested()) return;

        Vector4i const margin = self.margins().toVector();
        rect = rect.adjusted(margin.xy(), -margin.zw());

        DefaultVertexBuf::Builder verts;
        self.glMakeGeometry(verts);

        // Determine the area where the slider is moving.
        Rectanglei sliderArea = sliderRect();

        // Range dots.
        int numDots = de::clamp(5, round<int>(range.size() / step) + 1, 11);
        int dotSpace = sliderArea.width() - endLabelSize;
        int dotX = sliderArea.topLeft.x + endLabelSize / 2;
        float altAlpha = 0;
        if(dotSpace / numDots > 30)
        {
            altAlpha = .5f;
            numDots = 2 * numDots + 1;
        }
        Image::Size const dotSize = atlas().imageRect(root().tinyDot()).size();
        for(int i = 0; i < numDots; ++i)
        {
            Vector2i dotPos(dotX + dotSpace * float(i) / float(numDots - 1),
                            sliderArea.middle().y);

            Vector4f dotColor = textColor;
            dotColor.w *= .666f;
            if(altAlpha > 0 && i % 2)
            {
                // Dim alt dots.
                dotColor.w *= altAlpha;
            }
            verts.makeQuad(Rectanglei::fromSize(dotPos - dotSize.toVector2i()/2, dotSize),
                           dotColor, atlas().imageRectf(root().tinyDot()));
        }

        // Current slider position.
        Rectanglei slider = sliderValueRect();
        verts.makeQuad(slider.expanded(2), state == Grabbed? textColor : invTextColor,
                       atlas().imageRectf(root().solidWhitePixel()).middle());
        verts.makeFlexibleFrame(slider.expanded(5), 6, Vector4f(1, 1, 1, frameOpacity),
                                atlas().imageRectf(root().boldRoundCorners()));

        // Labels.        
        if(labels[Start].isReady())
        {
            labels[Start].makeVertices(verts,
                                       Rectanglei(rect.topLeft,
                                                  Vector2i(rect.topLeft.x + endLabelSize,
                                                           rect.bottomRight.y)),
                                       ui::AlignCenter, ui::AlignCenter,
                                       textColor);
        }
        if(labels[End].isReady())
        {
            labels[End].makeVertices(verts,
                                     Rectanglei(Vector2i(rect.bottomRight.x - endLabelSize,
                                                         rect.topLeft.y),
                                                rect.bottomRight),
                                     ui::AlignCenter, ui::AlignCenter,
                                     textColor);
        }
        if(labels[Value].isReady())
        {
            labels[Value].makeVertices(verts, slider,
                                       ui::AlignCenter, ui::AlignCenter,
                                       state == Grabbed? invTextColor : textColor);
        }

        drawable.buffer<DefaultVertexBuf>()
                .setVertices(gl::TriangleStrip, verts, animating? gl::Dynamic : gl::Static);

        self.requestGeometry(false);
    }

    void draw()
    {
        updateGeometry();

        uColor = Vector4f(1, 1, 1, self.visibleOpacity());
        drawable.draw();
    }

    void setState(State st)
    {
        if(state == st) return;

        //State const prev = state;
        state = st;
        animating = true;

        switch(st)
        {
        case Inert:
            //scale.setValue(1.f, .3f);
            //scale.setStyle(prev == Down? Animation::Bounce : Animation::EaseOut);
            frameOpacity.setValue(.25f, .6);
            break;

        case Hovering:
            //scale.setValue(1.1f, .15f);
            //scale.setStyle(Animation::EaseOut);
            frameOpacity.setValue(.5f, .15);
            break;

        case Grabbed:
            //scale.setValue(.95f);
            frameOpacity.setValue(.8f);
            break;
        }

        self.requestGeometry();
    }

    void updateHover(Vector2i const &pos)
    {
        if(state == Grabbed) return;

        if(self.hitTest(pos))
        {
            if(state == Inert) setState(Hovering);
        }
        else if(state == Hovering)
        {
            setState(Inert);
        }
    }

    void updateValueLabel()
    {
        if(!minLabel.isEmpty() && fequal(value, range.start))
        {
            labels[Value].setText(minLabel);
        }
        else if(!maxLabel.isEmpty() && fequal(value, range.end))
        {
            labels[Value].setText(maxLabel);
        }
        else
        {
            labels[Value].setText(QString::number(value * displayFactor, 'f', precision));
        }
    }

    void setValue(ddouble v)
    {
        // Round to nearest step.
        if(step > 0)
        {
            v = de::round<ddouble>((v - range.start) / step) * step + range.start;
        }

        v = range.clamp(v);

        if(!fequal(v, value))
        {
            value = v;

            updateValueLabel();

            animating = true;
            pos.setValue(float(value), 0.1);
            self.requestGeometry();

            emit self.valueChanged(v);
        }
    }

    void updateRangeLabels()
    {
        labels[Start].setText(minLabel.isEmpty()? QString::number(range.start * displayFactor, 'g', precision) : minLabel);
        labels[End].setText(maxLabel.isEmpty()?   QString::number(range.end * displayFactor, 'g', precision)   : maxLabel);
    }

    void startGrab(MouseEvent const &ev)
    {
        if(sliderValueRect().contains(ev.pos()))
        {
            setState(Grabbed);
            grabFrom = ev.pos();
            grabValue = value;
        }
    }

    void updateGrab(MouseEvent const &ev)
    {
        DENG2_ASSERT(state == Grabbed);

        //qDebug() << "delta" << (ev.pos() - grabFrom).asText();

        Rectanglei const area = sliderRect();
        ddouble unitsPerPixel = range.size() / (area.width() - endLabelSize);
        setValue(grabValue + (ev.pos().x - grabFrom.x) * unitsPerPixel);

        emit self.valueChangedByUser(value);
    }

    /// Amount to step when clicking a label.
    ddouble clickStep() const
    {
        if(step > 0)
        {
            return step;
        }
        return 1.0 / std::pow(10.0, precision) / displayFactor;
    }

    void endGrab(MouseEvent const &ev)
    {
        if(state == Grabbed)
        {
            setState(Inert);
            updateHover(ev.pos());
        }
        else
        {
            Rectanglei const rect = contentRect();

            //qDebug() << "click step:" << clickStep() << "value:" << value << "range:"
            //   << range.asText() << "new value:" << value - clickStep();

            // Maybe a click on the start/end label?
            if(rect.contains(ev.pos()))
            {
                if(ev.pos().x < rect.left() + endLabelSize)
                {
                    setValue(value - clickStep());
                    emit self.valueChangedByUser(value);
                }
                else if(ev.pos().x > rect.right() - endLabelSize)
                {
                    setValue(value + clickStep());
                    emit self.valueChangedByUser(value);
                }
            }
        }
    }
};

SliderWidget::SliderWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    /*
    // Testing.
    setRange(Rangef(0, 1));
    setPrecision(2);
    setValue(.5f);
    */

    // Default size.
    rule().setInput(Rule::Width,  style().rules().rule("slider.width"))
          .setInput(Rule::Height, OperatorRule::maximum(
                        style().fonts().font("default").height(),
                        font().height()) + margins().height());
}

void SliderWidget::setRange(Rangei const &intRange, int step)
{
    setRange(Ranged(intRange.start, intRange.end), ddouble(step));
}

void SliderWidget::setRange(Rangef const &floatRange, float step)
{
    setRange(Ranged(floatRange.start, floatRange.end), ddouble(step));
}

void SliderWidget::setRange(Ranged const &doubleRange, ddouble step)
{
    d->range = doubleRange;
    d->step = step;

    d->updateRangeLabels();
    d->setValue(d->value);
    d->pos.finish();
}

void SliderWidget::setPrecision(int precisionDecimals)
{
    d->precision = precisionDecimals;
    d->updateValueLabel();
    d->updateRangeLabels();
}

void SliderWidget::setValue(ddouble value)
{
    d->setValue(value);
}

void SliderWidget::setMinLabel(const String &labelText)
{
    d->minLabel = labelText;

    d->updateRangeLabels();
    d->updateValueLabel();
}

void SliderWidget::setMaxLabel(const String &labelText)
{
    d->maxLabel = labelText;

    d->updateRangeLabels();
    d->updateValueLabel();
}

void SliderWidget::setDisplayFactor(ddouble factor)
{
    d->displayFactor = factor;
    d->updateRangeLabels();
    d->updateValueLabel();
}

Ranged SliderWidget::range() const
{
    return d->range;
}

ddouble SliderWidget::value() const
{
    return d->value;
}

int SliderWidget::precision() const
{
    return d->precision;
}

ddouble SliderWidget::displayFactor() const
{
    return d->displayFactor;
}

void SliderWidget::viewResized()
{
    GuiWidget::viewResized();

    d->uMvpMatrix = root().projMatrix2D();
}

void SliderWidget::update()
{
    GuiWidget::update();

    if(d->animating)
    {
        requestGeometry();

        d->animating = !d->pos.done() || !d->frameOpacity.done();
    }
}

void SliderWidget::drawContent()
{
    d->draw();
}

bool SliderWidget::handleEvent(Event const &event)
{
    if(event.type() == Event::MousePosition)
    {
        if(d->state == Instance::Grabbed)
        {
            d->updateGrab(event.as<MouseEvent>());
            return true;
        }

        d->updateHover(event.as<MouseEvent>().pos());
    }

    // Left mouse button can be used to drag/step the value.
    if(d->state != Instance::Inert)
    {
        switch(handleMouseClick(event, MouseEvent::Left))
        {
        case MouseClickStarted:
            d->startGrab(event.as<MouseEvent>());
            return true;

        case MouseClickAborted:
        case MouseClickFinished:
            d->endGrab(event.as<MouseEvent>());
            return true;

        default:
            break;
        }
    }

    // Right-click to edit the value as text.
    if(d->state != Instance::Grabbed)
    {
        switch(handleMouseClick(event, MouseEvent::Right))
        {
        case MouseClickFinished: {
            ValuePopup *pop = new ValuePopup(*this);
            pop->setAnchorAndOpeningDirection(rule(),
                    rule().recti().middle().y < root().viewHeight().valuei()/2? ui::Down : ui::Up);
            pop->setDeleteAfterDismissed(true);
            root().addOnTop(pop);
            pop->open();
            return true; }

        case MouseClickStarted:
        case MouseClickAborted:
            return true;

        default:
            break;
        }
    }

    return GuiWidget::handleEvent(event);
}

void SliderWidget::setValueFromText(QString text)
{
    setValue(text.toDouble() / d->displayFactor);
    emit valueChangedByUser(d->value);
}

void SliderWidget::glInit()
{
    d->glInit();
}

void SliderWidget::glDeinit()
{
    d->glDeinit();
}

void SliderWidget::updateStyle()
{
    d->updateStyle();
}

} // namespace de
