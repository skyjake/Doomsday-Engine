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

#include "de/TextDrawable"

#include <de/Task>
#include <de/TaskPool>

namespace de {

DENG2_PIMPL(TextDrawable)
{
    /**
     * Background task for wrapping text onto lines and figuring out the
     * formatting/tab stops.
     */
    class WrapTask : public Task
    {
    public:
        WrapTask(Instance *inst, int toWidth) : d(inst), _width(toWidth), _valid(inst->validWrapId) {}
        void runTask() {
            if(_valid < d->validWrapId) {
                return; // There's a later one.
            }
            DENG2_GUARD_FOR(d->backWrap, G);

            //qDebug() << "running WrapTask" << _width;
            //TimeDelta(0.5).sleep();

            d->backWrap->wrapTextToWidth(d->backWrap->plainText, d->backWrap->format, _width);
            d->needSwap = true;

            //qDebug() << "WrapTask" << _width << "completed" << d->thisPublic << d->backWrap->text;
        }
    private:
        Instance *d;
        int _width;
        duint32 _valid;
    };

    bool inited;

    struct Wrapper : public FontLineWrapping
    {
        String text;
        String plainText;
        Font::RichFormat format;
        int lineWidth;

        Wrapper() : lineWidth(0) {}
    };

    Wrapper *frontWrap; ///< For drawing.
    Wrapper *backWrap;  ///< For background task.
    bool needSwap;
    bool needUpdate;
    TaskPool tasks;
    volatile duint32 validWrapId;

    Instance(Public *i) : Base(i), inited(false), needSwap(false), needUpdate(false), validWrapId(0)
    {
        frontWrap = new Wrapper;
        backWrap  = new Wrapper;
    }

    ~Instance()
    {
        tasks.waitForDone();

        delete frontWrap;
        delete backWrap;
    }

    void beginWrapTask(int toWidth)
    {
        if(inited && toWidth > 0)
        {
            // Should this be done immediately? Background tasks unavoidably
            // bring some extra latency before the job is finished, especially
            // if a large number of tasks is queued.
            if(backWrap->plainText.size() < 20)
            {
                // Looks quick enough, just do it now.
                WrapTask(this, toWidth).runTask();
            }
            else
            {
                // Queue the task to be run when there's time.
                ++validWrapId;
                tasks.start(new WrapTask(this, toWidth));
            }
        }
    }

    /**
     * Swaps the back wrapping used by the background task with the front
     * wrapping used for drawing.
     */
    void swap()
    {
        DENG2_ASSERT(tasks.isDone());

        if(!frontWrap->hasFont() || &backWrap->font() != &frontWrap->font())
        {
            frontWrap->setFont(backWrap->font());
        }

        frontWrap->lineWidth = backWrap->lineWidth;
        frontWrap->text      = backWrap->text;
        frontWrap->plainText = backWrap->plainText;
        frontWrap->format    = backWrap->format;

        std::swap(backWrap, frontWrap);

        self.setWrapping(*frontWrap);
        self.GLTextComposer::setText(frontWrap->plainText, frontWrap->format);

        if(needUpdate)
        {
            self.forceUpdate();
            needUpdate = false;
        }

        needSwap = false;
    }
};

TextDrawable::TextDrawable() : d(new Instance(this))
{
    setWrapping(*d->frontWrap);
}

void TextDrawable::init(Atlas &atlas, Font const &font, Font::RichFormat::IStyle const *style)
{
    d->inited = true;

    setAtlas(atlas);
    if(style)
    {        
        d->frontWrap->format.setStyle(*style);
        d->backWrap->format.setStyle(*style);

        // Previously defined text should be restyled, now.
        d->backWrap->plainText = d->backWrap->format.initFromStyledText(d->backWrap->text);
    }
    GLTextComposer::setText(d->backWrap->plainText, d->backWrap->format);
    setFont(font);
}

void TextDrawable::deinit()
{
    clear();

    d->inited = false;
}

void TextDrawable::clear()
{
    d->tasks.waitForDone();

    d->frontWrap->clear();
    d->backWrap->clear();

    release();
}

void TextDrawable::setLineWrapWidth(int maxLineWidth)
{
    if(d->backWrap->lineWidth != maxLineWidth)
    {
        // Start a new background task.
        d->backWrap->lineWidth = maxLineWidth;
        d->beginWrapTask(maxLineWidth);
    }
}

void TextDrawable::setText(String const &styledText)
{
    d->backWrap->clear();
    d->needUpdate = true;

    d->backWrap->text = styledText;
    d->backWrap->plainText = d->backWrap->format.initFromStyledText(styledText);

    d->beginWrapTask(d->backWrap->lineWidth);
}

void TextDrawable::setFont(Font const &font)
{
    d->backWrap->setFont(font);
    d->backWrap->clear();

    d->needUpdate = true;

    d->beginWrapTask(d->backWrap->lineWidth);
}

void TextDrawable::setRange(Rangei const &lineRange)
{
    GLTextComposer::setRange(lineRange);
    releaseLinesOutsideRange();
}

bool TextDrawable::update()
{
    bool swapped = false;

    // Has a background wrap completed?
    if(!isBeingWrapped() && d->needSwap)
    {
        d->swap();
        swapped = true;
    }

    if(!d->frontWrap->hasFont()) return false;

    bool wasNotReady = !isReady();
    bool changed = GLTextComposer::update() || swapped || (isReady() && wasNotReady);
    return changed && !isBeingWrapped();
}

FontLineWrapping const &TextDrawable::wraps() const
{
    return *d->frontWrap;
}

Vector2ui TextDrawable::wrappedSize() const
{
    return Vector2ui(d->frontWrap->width(), d->frontWrap->totalHeightInPixels());
}

String TextDrawable::text() const
{
    if(!d->frontWrap->hasFont())
    {
        return d->backWrap->text;
    }
    return d->frontWrap->text;
}

String TextDrawable::plainText() const
{
    return d->frontWrap->plainText;
}

bool TextDrawable::isBeingWrapped() const
{
    return !d->tasks.isDone();
}

} // namespace de
