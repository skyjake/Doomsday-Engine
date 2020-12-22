/** @file textdrawable.cpp  High-level GL text drawing utility.
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

#include "de/textdrawable.h"

#include <de/task.h>
#include <de/taskpool.h>

namespace de {

DE_PIMPL(TextDrawable)
{
    DE_DEFINE_AUDIENCE(Deletion, void ownerDeleted())

    template <typename Type>
    class LockablePointer : public Lockable
    {
    public:
        LockablePointer(Type *p = nullptr) : _ptr(p) {}

        LockablePointer &operator = (Type *p)
        {
            DE_GUARD(this);
            _ptr = p;
            return *this;
        }

        explicit operator bool () const
        {
            DE_GUARD(this);
            return _ptr != nullptr;
        }

        operator Type * ()
        {
            DE_GUARD(this);
            return _ptr;
        }

        Type *operator -> () { return _ptr; } // Lock beforehand!

    private:
        Type *_ptr;
    };

    template <typename Type>
    class LockableUniquePointer : public Lockable
    {
    public:
        void reset(Type *p)
        {
            DE_GUARD(this);
            _ptr.reset(p);
        }

        Type *take()
        {
            DE_GUARD(this);
            return _ptr.release();
        }

        explicit operator bool () const
        {
            DE_GUARD(this);
            return bool(_ptr);
        }

    private:
        std::unique_ptr<Type> _ptr;
    };

    /// Counter used for keeping track of the latest wrapping task.
    class SyncId : public Lockable
    {
    public:
        operator duint32 () const
        {
            DE_GUARD(this);
            return _id;
        }

        void invalidate()
        {
            DE_GUARD(this);
            _id++;
        }

        bool isValid(duint32 value) const
        {
            DE_GUARD(this);
            return value == _id;
        }

    private:
        duint32 _id { 0 };
    };

    struct Wrapper : public FontLineWrapping
    {
//        String text;
        Font::RichFormat format;
    };

    /**
     * Background task for wrapping text onto lines and figuring out the
     * formatting/tab stops. Observes the Instance for deletion so that if it has been
     * destroyed while the task is running, we'll know to discard the results.
     */
    class WrapTask : public Task, public Impl::IDeletionObserver
    {
    public:
        WrapTask(Impl *inst, const String &styledText, int toWidth, const Font &font,
                 const Font::RichFormat::IStyle *style)
            : d(inst)
            , _text(styledText)
            , _width(toWidth)
            , _font(font)
            , _style(style)
            , _valid(inst->sync)
            , _wrapper(nullptr)
        {
            d->audienceForDeletion += this;
        }

        void runTask()
        {
            try
            {
                runWrapTask();
            }
            catch (const Error &)
            {
                // Cancelled.
            }
        }

        void runWrapTask()
        {
            // Check that it's okay if we start the operation now.
            {
                DE_GUARD(d);
                if (!d) return; // Owner has been deleted.
                if (!d->sync.isValid(_valid))
                {
                    // No longer the latest task, so ignore this one.
                    d->audienceForDeletion -= this;
                    return;
                }
            }

            // Ok, we have a go. Set up the wrapper first.
            _wrapper.reset(new Wrapper);
            _wrapper->setFont(_font);
            if (_style)
            {
                _wrapper->format.setStyle(*_style);
            }
//            _wrapper->text = _text;
            _wrapper->format.initFromStyledText(_text);

            // This is where most of the time will be spent:
            //qDebug() << "wrapping" << _wrapper->plainText << "to" << _width;
            _wrapper->wrapTextToWidth(_text, _wrapper->format, _width);

            // Pre-rasterize the first lines of the text. The assumption is that
            // longer text will only be visible after scrolling, so it will be
            // rasterized as needed.
            _wrapper->rasterizeLines(Rangei(0, 10)); // May also take a while...

            // Pass the finished wrapping to the owner.
            {
                DE_GUARD(d);
                if (d) d->audienceForDeletion -= this;
                if (d && d->sync.isValid(_valid))
                {
                    d->incoming.reset(_wrapper.release());
                }
            }
        }

        void ownerDeleted()
        {
            d = nullptr;
            if (_wrapper) _wrapper->cancel();
        }

    private:
        LockablePointer<Impl> d;
        String _text;
        int _width;
        const Font &_font;
        const Font::RichFormat::IStyle *_style;
        duint32 _valid;
        std::unique_ptr<Wrapper> _wrapper;
    };

    bool inited { false };
    const Font::RichFormat::IStyle *style { nullptr };
    String styledText;
    const Font *font { nullptr };
    float fontHeight = 0.f;
    int wrapWidth { 0 };
    Wrapper *visibleWrap; ///< For drawing.
    LockableUniquePointer<Wrapper> incoming; ///< Latest finished wrapping.
    SyncId sync;
    TaskPool tasks;

    Impl(Public *i) : Base(i)
    {
        // The visible wrapper is replaced when new ones are produced by workers.
        // There always needs to be a visible wrapper, though, so create an empty one.
        visibleWrap = new Wrapper;
    }

    ~Impl()
    {
        // All ongoing tasks will be skipped/discarded.
        sync.invalidate();

        // Let the background tasks know that we are gone.
        DE_NOTIFY_VAR(Deletion, i) i->ownerDeleted();
        audienceForDeletion.clear();

        tasks.waitForDone();

        delete visibleWrap;
    }

    void beginWrapTask()
    {
        if (inited && wrapWidth > 0 && font)
        {
            sync.invalidate();

            // Check if the wrapping can be done immediately. Background tasks unavoidably
            // bring some extra latency before the job is finished, especially if a large
            // number of tasks is queued.
            if (styledText.size() <= 20)
            {
                // Looks quick enough, just do it now.
                WrapTask(this, styledText, wrapWidth, *font, style).runTask();
            }
            else
            {
                // Queue the task to be run when there's time.
                tasks.start(new WrapTask(this, styledText, wrapWidth, *font, style));
            }
        }
    }

    /**
     * Replaces the front wrapper with the latest finished line wrapping created by
     * background tasks.
     *
     * @return @c true, if a swap occurred; otherwise @c false.
     */
    bool swap()
    {
        if (!incoming) return false;

        delete visibleWrap;
        visibleWrap = incoming.take();

        DE_ASSERT(visibleWrap != nullptr);

        self().setWrapping(*visibleWrap);
        self().GLTextComposer::setText(visibleWrap->format);

        return true;
    }
};

TextDrawable::TextDrawable() : d(new Impl(this))
{
    setWrapping(*d->visibleWrap);
}

void TextDrawable::init(Atlas &atlas, const Font &font, const Font::RichFormat::IStyle *style)
{
    d->inited = true;

    setAtlas(atlas);
    d->style = style;
    d->font = &font;

    if (!d->styledText.isEmpty())
    {
        // Update the wrapping, if possible.
        d->beginWrapTask();
    }
}

void TextDrawable::deinit()
{
    clear();

    d->inited = false;
}

void TextDrawable::clear()
{
    // Ignore whatever the background task(s) are doing.
    d->sync.invalidate();
    d->incoming.reset(nullptr);
    d->visibleWrap->clear();

    release();
}

void TextDrawable::setLineWrapWidth(int maxLineWidth)
{
    if (d->wrapWidth != maxLineWidth)
    {
        d->wrapWidth = maxLineWidth;
        d->beginWrapTask();
    }
}

void TextDrawable::setText(const String &styledText)
{
    if (d->styledText != styledText)
    {
        d->styledText = styledText;
        d->beginWrapTask();
    }
}

void TextDrawable::setFont(const Font &font)
{
    if (d->font != &font || !fequal(d->fontHeight, font.height().value()))
    {
        d->font       = &font;
        d->fontHeight = font.height().value();
        d->beginWrapTask(); // Redo the contents.
    }
}

void TextDrawable::setRange(const Rangei &lineRange)
{
    GLTextComposer::setRange(lineRange);
    releaseLinesOutsideRange();
}

bool TextDrawable::update()
{
    if (!d->inited || !d->font) return false;

    // Check for a completed background task.
    bool swapped = d->swap();
    const bool wasNotReady = !isReady();
    return GLTextComposer::update() || swapped || (isReady() && wasNotReady);
}

const FontLineWrapping &TextDrawable::wraps() const
{
    return *d->visibleWrap;
}

Vec2ui TextDrawable::wrappedSize() const
{
    return Vec2ui(d->visibleWrap->width(), d->visibleWrap->totalHeightInPixels());
}

String TextDrawable::text() const
{
    // The latest text that is either pending or currently being shown.
    return d->styledText;
}

bool TextDrawable::isBeingWrapped() const
{
    return !d->tasks.isDone();
}

const Font &TextDrawable::font() const
{
    DE_ASSERT(d->font != nullptr);
    return *d->font;
}

} // namespace de
