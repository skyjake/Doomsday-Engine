/** @file textdrawable.cpp  High-level GL text drawing utility.
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

#include "ui/widgets/textdrawable.h"

#include <de/Task>
#include <de/TaskPool>

using namespace de;

DENG2_PIMPL(TextDrawable)
{
    /**
     * Background task for wrapping text onto lines and figuring out the
     * formatting/tab stops.
     */
    class WrapTask : public Task
    {
    public:
        WrapTask(Instance *inst, int toWidth) : d(inst), _width(toWidth) {}
        void runTask() {
            DENG2_GUARD_FOR(d->wraps, G);
            qDebug() << "running WrapTask" << _width;
            TimeDelta(2).sleep();
            d->wraps.wrapTextToWidth(d->plainText, d->format, _width);
        }
    private:
        Instance *d;
        int _width;
    };

    bool inited;
    int lineWidth;
    FontLineWrapping wraps;
    String text;
    String plainText;
    Font::RichFormat format;
    TaskPool tasks;

    Instance(Public *i) : Base(i), inited(false), lineWidth(0)
    {}

    ~Instance()
    {
        tasks.waitForDone();
    }

    void beginWrapTask(int toWidth)
    {
        if(inited && toWidth > 0)
        {
            tasks.start(new WrapTask(this, toWidth));
        }
    }
};

TextDrawable::TextDrawable() : d(new Instance(this))
{
    setWrapping(d->wraps);
}

void TextDrawable::init(Atlas &atlas, Font const &font, Font::RichFormat::IStyle const *style)
{
    d->inited = true;
    setAtlas(atlas);
    if(style)
    {
        d->format.setStyle(*style);
    }
    GLTextComposer::setText(d->plainText, d->format);
}

void TextDrawable::deinit()
{
    d->wraps.clear();
    release();
    d->inited = false;
}

void TextDrawable::setLineWrapWidth(int maxLineWidth)
{
    if(d->lineWidth != maxLineWidth)
    {
        setState(false);
        d->lineWidth = maxLineWidth;
        d->beginWrapTask(d->lineWidth);
    }

    /*
    if(maxLineWidth != d->wraps.maximumWidth())
    {
        // Needs rewrap.
        d->wraps.clear();
        setState(false);

        d->beginWrapTask(maxLineWidth);
    }*/
}

void TextDrawable::setText(String const &styledText)
{
    d->wraps.clear();
    release();

    d->text = styledText;
    d->plainText = d->format.initFromStyledText(styledText);
    GLTextComposer::setText(d->plainText, d->format);

    d->beginWrapTask(d->lineWidth);
}

void TextDrawable::setFont(Font const &font)
{
    d->wraps.setFont(font);
    d->wraps.clear();
    forceUpdate();
    setState(false);

    d->beginWrapTask(d->lineWidth);
}

void TextDrawable::setRange(Rangei const &lineRange)
{
    GLTextComposer::setRange(lineRange);
    releaseLinesOutsideRange();
}

bool TextDrawable::update()
{
    // Are we still wrapping?
    if(isBeingWrapped())
    {
        setState(false);
        return false;
    }
    return GLTextComposer::update();
}

FontLineWrapping const &TextDrawable::wraps() const
{
    return d->wraps;
}

Vector2i TextDrawable::wrappedSize() const
{
    if(isBeingWrapped())
    {
        // Don't block.
        return Vector2i(0, 0);
    }
    return Vector2i(d->wraps.width(), d->wraps.totalHeightInPixels());
}

String TextDrawable::text() const
{
    return d->text;
}

String TextDrawable::plainText() const
{
    return d->plainText;
}

bool TextDrawable::isBeingWrapped() const
{
    return !d->tasks.isDone();
}
