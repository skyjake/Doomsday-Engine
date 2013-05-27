/** @file logwidget.cpp  Widget for output message log.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/logwidget.h"
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/fontlinewrapping.h"
#include "ui/widgets/gltextcomposer.h"
#include "ui/widgets/styledlogsinkformatter.h"
#include "ui/style.h"
#include "clientapp.h"

#include <de/KeyEvent>
#include <de/MemoryLogSink>
#include <de/LogBuffer>
#include <de/AtlasTexture>
#include <de/Drawable>
#include <de/VertexBuilder>
#include <de/Task>
#include <de/TaskPool>

#include <QImage>
#include <QPainter>

using namespace de;

/**
 * @todo Pruning:
 * - during widget update()
 *  ...whenever the sink is idle and no rewrapping is ongoing
 *  ...excess entries are deleted from the sink
 * - the same excess entries are deleted from the cache
 * - if something is pruned, existing cache sink indices are adjusted to match the new sink indices
 * - update totalHeight
 */

DENG2_PIMPL(LogWidget), public Font::RichFormat::IStyle
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    /// Cache of drawable entries.
    struct CacheEntry : public Lockable
    {
        int sinkIndex;
        Font::RichFormat format;
        FontLineWrapping wraps;
        GLTextComposer composer;

        CacheEntry(int index, Font const &font, Font::RichFormat::IStyle &richStyle, Atlas &atlas)
            : sinkIndex(index), format(richStyle)
        {
            wraps.setFont(font);
            composer.setAtlas(atlas);
        }

        ~CacheEntry()
        {
            // Free atlas allocations.
            composer.release();
        }

        int height() const
        {
            DENG2_GUARD(this);
            return wraps.height() * wraps.font().lineSpacing().valuei();
        }

        bool needWrap() const
        {
            DENG2_GUARD(this);
            return wraps.isEmpty();
        }

        void wrap(String const &richText, int width)
        {
            DENG2_GUARD(this);
            String plain = format.initFromStyledText(richText);
            wraps.wrapTextToWidth(plain, format, width);
            composer.setText(plain, format);
            composer.setWrapping(wraps);
        }

        void rewrap(int width)
        {
            DENG2_GUARD(this);
            wraps.wrapTextToWidth(wraps.text(), format, width);
        }

        void make(GLTextComposer::Vertices &verts, int y)
        {
            DENG2_GUARD(this);
            composer.update();
            composer.makeVertices(verts, Vector2i(0, y), GLTextComposer::AlignLeft);
        }

        void clear()
        {
            composer.release();
        }
    };

    class WrappingMemoryLogSink : public MemoryLogSink
    {
    public:
        WrappingMemoryLogSink(LogWidget::Instance *wd)
            : d(wd),
              _maxEntries(1000),
              _next(0),
              _width(0)
        {}

        ~WrappingMemoryLogSink()
        {
            _pool.waitForDone();
            clear();
        }

        int maxEntries() const { return _maxEntries; }

        void clear()
        {
            DENG2_GUARD(_wrappedEntries);
            qDeleteAll(_wrappedEntries);
            _wrappedEntries.clear();
        }

        void setWidth(int wrapWidth)
        {
            _width = wrapWidth;
            beginWorkOnNext();
        }

        void addedNewEntry(LogEntry &entry)
        {
            beginWorkOnNext();
        }

        CacheEntry *nextCachedEntry()
        {
            DENG2_GUARD(_wrappedEntries);
            if(_wrappedEntries.isEmpty()) return 0;
            return _wrappedEntries.takeFirst();
        }

    protected:
        class WrapTask : public Task
        {
        public:
            WrapTask(WrappingMemoryLogSink &owner, int index, String const &styledText)
                : _sink(owner),
                  _index(index),
                  _styledText(styledText)
            {}

            void runTask()
            {
                CacheEntry *cached = new CacheEntry(_index, *_sink.d->font, *_sink.d,
                                                    *_sink.d->entryAtlas);
                cached->wrap(_styledText, _sink._width);

                usleep(75000); // TODO -- remove this testing aid

                DENG2_GUARD_FOR(_sink._wrappedEntries, G);
                _sink._wrappedEntries << cached;
            }

        private:
            WrappingMemoryLogSink &_sink;
            int _index;
            String _styledText;
        };

        void beginWorkOnNext()
        {
            while(_width > 0 && _next >= 0 && _next < entryCount())
            {
                LogEntry const &ent = entry(_next);
                String const styled = _formatter.logEntryToTextLines(ent).at(0);

                _pool.start(new WrapTask(*this, _next, styled));
                _next++;
            }
        }

    private:
        LogWidget::Instance *d;
        int _maxEntries;
        StyledLogSinkFormatter _formatter;
        int _next;
        struct WrappedEntries : public QList<CacheEntry *>, public Lockable {};
        WrappedEntries _wrappedEntries;
        TaskPool _pool;
        int _width;
    };

    // Log entries.
    WrappingMemoryLogSink sink;

    QList<CacheEntry *> cache; ///< Indices match entry indices in sink.
    int cacheWidth;

    class RewrapTask : public Task
    {
        LogWidget::Instance *d;
        duint32 _cancelLevel;
        int _next;
        int _width;

    public:
        RewrapTask(LogWidget::Instance *wd, int startFrom, int width)
            : d(wd), _cancelLevel(wd->cancelRewrap), _next(startFrom), _width(width)
        {}

        void runTask()
        {
            while(_next >= 0 && _cancelLevel == d->cancelRewrap)
            {
                d->cache[_next--]->rewrap(_width);
                //usleep(10000);
            }
        }
    };

    TaskPool rewrapPool; ///< Used when rewrapping existing cached entries.
    volatile duint32 cancelRewrap;

    enum { CancelAllRewraps = 0xffffffff };

    // State.
    Animation visibleOffset;
    int totalHeight;
    int maxScroll;
    int lastMaxScroll;
    Rangei visibleRange;
    Animation scrollOpacity;

    // Style.
    Font const *font;
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    int margin;
    int scrollBarWidth;

    // GL objects.
    VertexBuf *buf;
    VertexBuf *bgBuf;
    AtlasTexture *entryAtlas;
    Drawable contents;
    Drawable background;
    GLUniform uMvpMatrix;
    GLUniform uTex;
    GLUniform uShadowColor;
    GLUniform uColor;
    GLUniform uBgMvpMatrix;
    GLUniform uBgTex;
    Matrix4f projMatrix;
    Matrix4f viewMatrix;
    Id bgTex;
    Id scrollTex;

    Instance(Public *i)
        : Base(i),
          sink(this),
          cacheWidth(0),
          cancelRewrap(0),
          visibleOffset(0),
          totalHeight(0),
          maxScroll(0),
          lastMaxScroll(0),
          visibleRange(Rangei(-1, -1)),
          scrollOpacity(0, Animation::EaseBoth),
          font(0),
          buf(0),
          entryAtlas(0),
          uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4),
          uTex        ("uTex",       GLUniform::Sampler2D),
          uShadowColor("uColor",     GLUniform::Vec4),
          uColor      ("uColor",     GLUniform::Vec4),
          uBgMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uBgTex      ("uTex",       GLUniform::Sampler2D)
    {
        updateStyle();
    }

    ~Instance()
    {
        LogBuffer::appBuffer().removeSink(sink);
    }

    void clear()
    {
        sink.clear();
        clearCache();
    }

    void cancelRewraps()
    {
        cancelRewrap = CancelAllRewraps;
        rewrapPool.waitForDone();
        cancelRewrap = 0;
    }

    void clearCache()
    {
        cancelRewraps();

        entryAtlas->clear();
        cache.clear();
    }

    void updateStyle()
    {
        // TODO -- stop wrapping tasks in the sink

        Style const &st = self.style();

        font           = &st.fonts().font("log.normal");
        margin         = st.rules().rule("gap").valuei();
        scrollBarWidth = st.rules().rule("log.scrollbar").valuei();

        normalColor    = st.colors().color("log.normal");
        highlightColor = st.colors().color("log.highlight");
        dimmedColor    = st.colors().color("log.dimmed");
        accentColor    = st.colors().color("log.accent");
    }

    Font::RichFormat::IStyle::Color richStyleColor(int index) const
    {
        switch(index)
        {
        default:
        case Font::RichFormat::NormalColor:
            return normalColor;

        case Font::RichFormat::HighlightColor:
            return highlightColor;

        case Font::RichFormat::DimmedColor:
            return dimmedColor;

        case Font::RichFormat::AccentColor:
            return accentColor;
        }
    }

    void richStyleFormat(int contentStyle,
                         float &sizeFactor,
                         Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle,
                         int &colorIndex) const
    {
        switch(contentStyle)
        {
        default:
        case Font::RichFormat::NormalStyle:
            sizeFactor = 1.f;
            fontWeight = Font::RichFormat::OriginalWeight;
            fontStyle  = Font::RichFormat::OriginalStyle;
            colorIndex = Font::RichFormat::OriginalColor;
            break;

        case Font::RichFormat::MajorStyle:
            sizeFactor = 1.f;
            fontWeight = Font::RichFormat::Bold;
            fontStyle  = Font::RichFormat::Regular;
            colorIndex = Font::RichFormat::HighlightColor;
            break;

        case Font::RichFormat::MinorStyle:
            sizeFactor = .8f;
            fontWeight = Font::RichFormat::Normal;
            fontStyle  = Font::RichFormat::Regular;
            colorIndex = Font::RichFormat::DimmedColor;
            break;

        case Font::RichFormat::MetaStyle:
            sizeFactor = .9f;
            fontWeight = Font::RichFormat::Light;
            fontStyle  = Font::RichFormat::Italic;
            colorIndex = Font::RichFormat::AccentColor;
            break;

        case Font::RichFormat::MajorMetaStyle:
            sizeFactor = .9f;
            fontWeight = Font::RichFormat::Bold;
            fontStyle  = Font::RichFormat::Italic;
            colorIndex = Font::RichFormat::AccentColor;
            break;

        case Font::RichFormat::MinorMetaStyle:
            sizeFactor = .8f;
            fontWeight = Font::RichFormat::Light;
            fontStyle  = Font::RichFormat::Italic;
            colorIndex = Font::RichFormat::NormalColor;
            break;

        case Font::RichFormat::AuxMetaStyle:
            sizeFactor = .8f;
            fontWeight = Font::RichFormat::Light;
            fontStyle  = Font::RichFormat::OriginalStyle;
            colorIndex = Font::RichFormat::DimmedColor;
            break;
        }
    }

    void glInit()
    {
        entryAtlas = AtlasTexture::newWithRowAllocator(
                Atlas::BackingStore | Atlas::AllowDefragment,
                GLTexture::maximumSize().min(Atlas::Size(2048, 1024)));

        Image solidWhitePixel = Image::solidColor(Image::Color(255, 255, 255, 255),
                                                  Image::Size(1, 1));
        bgTex = self.root().atlas().alloc(solidWhitePixel);
        uBgTex = self.root().atlas();

        scrollTex = entryAtlas->alloc(solidWhitePixel);

        uTex = entryAtlas;
        uColor = Vector4f(1, 1, 1, 1);

        background.addBuffer(bgBuf = new VertexBuf);
        self.root().shaders().build(background.program(), "generic.textured.color")
                << uBgMvpMatrix
                << uBgTex;

        // Vertex buffer for the log entries.
        contents.addBuffer(buf = new VertexBuf);
        self.root().shaders().build(contents.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uShadowColor
                << uTex;
    }

    void glDeinit()
    {
        self.root().atlas().release(bgTex);

        clearCache();

        delete entryAtlas;
        entryAtlas = 0;
    }

    void prune()
    {
        /*
        int numPruned = 0;

        // Remove old entries if there are too many.
        int excess = sink.entryCount() - maxEntries;
        if(excess > 0)
        {
            sink.remove(0, excess);
        }
        while(excess-- > 0 && !cache.isEmpty())
        {
            delete cache.takeFirst();
            numPruned++;
        }
        */
    }

    duint contentWidth() const
    {
        return de::max(margin, self.rule().width().valuei() - 2 * margin);
    }

    int maxVisibleOffset(int visibleHeight)
    {
        // Determine total height of all entries.
        return de::max(0, totalHeight - visibleHeight);
    }

    void clampVisibleOffset(int visibleHeight)
    {
        setVisibleOffset(de::min(int(visibleOffset), maxVisibleOffset(visibleHeight)));
    }

    void setVisibleOffset(int off)
    {
        if(int(visibleOffset) != off)
        {
            visibleOffset.setValue(off, .3f);
            emit self.scrollPositionChanged(off);
        }
    }

    void fetchNewCachedEntries()
    {
        while(CacheEntry *cached = sink.nextCachedEntry())
        {
            // Find a suitable place according to the original index in the sink;
            // the task pool may output the entries slightly out of order as
            // multiple threads may be in use.
            int pos = cache.size();
            while(pos > 0 && cache.at(pos - 1)->sinkIndex > cached->sinkIndex)
            {
                --pos;
            }
            cache.insert(pos, cached);

            totalHeight += cached->height();

            // Adjust visible offset so it remains fixed in relation to
            // existing entries.
            if(visibleOffset.target() > 0)
            {
                visibleOffset.adjustTarget(visibleOffset.target() + cached->height());
                emit self.scrollPositionChanged(visibleOffset.target());
            }
        }
    }

    void rewrapCache()
    {
        if(cache.isEmpty()) return;

        if(isRewrapping())
        {
            // Cancel an existing rewrap.
            cancelRewrap++;
        }

        // Start a rewrapping task that goes through all the existing entries,
        // starting from the latest entry.
        rewrapPool.start(new RewrapTask(this, cache.size() - 1, contentWidth()));
    }

    bool isRewrapping() const
    {
        return !rewrapPool.isDone();
    }

    void releaseExcessComposedEntries()
    {
        if(visibleRange < 0) return;

        // Excess entries before the visible range.
        int excess = visibleRange.start - visibleRange.size();
        for(int i = 0; i <= excess; ++i)
        {
            cache[i]->clear();
        }

        // Excess entries after the visible range.
        excess = visibleRange.end + visibleRange.size();
        for(int i = excess; i < cache.size(); ++i)
        {
            cache[i]->clear();
        }
    }

    void updateProjection()
    {
        projMatrix = self.root().projMatrix2D();

        uBgMvpMatrix = projMatrix;
    }

    void restartScrollOpacityFade()
    {
        if(visibleOffset.target() == 0)
        {
            scrollOpacity.setValue(0, .5f);
        }
        else
        {
            scrollOpacity.setValueFrom(.4f, .025f, 5, 2);
        }
    }

    void updateGeometry()
    {
        Rectanglei pos = self.rule().recti();
        Vector2i const contentSize(contentWidth(), pos.height());

        // If the width of the widget changes, text needs to be reflowed with the
        // new width.
        if(cacheWidth != contentSize.x)
        {
            rewrapCache();
            cacheWidth = contentSize.x;
        }

        // Get new entries.
        fetchNewCachedEntries();

        // We won't keep an unlimited number of entries in memory; delete the
        // oldest ones if limit has been reached.
        prune();

        clampVisibleOffset(contentSize.y);

        // Draw in reverse, as much as we need.
        int yBottom = contentSize.y + visibleOffset;

        maxScroll    = maxVisibleOffset(contentSize.y);
        visibleRange = Rangei(-1, -1);

        VertexBuf::Builder verts;

        // Copy all visible entries to the buffer.
        for(int idx = cache.size() - 1; yBottom > 0 && idx >= 0; --idx)
        {
            CacheEntry *entry = cache[idx];

            yBottom -= entry->height();

            if(yBottom < contentSize.y)
            {
                // This entry is visible.
                entry->make(verts, yBottom);

                if(visibleRange.end == -1) visibleRange.end = idx;
                visibleRange.start = idx;
            }
        }

        // Draw the scroll indicator.
        if(scrollOpacity > 0)
        {
            int const indHeight = de::clamp(
                        margin * 2,
                        int(float(contentSize.y * contentSize.y) / float(totalHeight)),
                        contentSize.y / 2);
            float const indPos = float(visibleOffset) / float(maxScroll);
            float const avail = contentSize.y - indHeight;
            for(int i = 0; i < indHeight; ++i)
            {                
                verts.makeQuad(Rectanglef(Vector2f(contentSize.x + margin - 2*scrollBarWidth,
                                                   avail - indPos * avail + indHeight),
                                          Vector2f(contentSize.x + margin - scrollBarWidth,
                                                   avail - indPos * avail)),
                               Vector4f(1, 1, 1, scrollOpacity) * accentColor / 255.f,
                               entryAtlas->imageRectf(scrollTex).middle());
            }
        }

        buf->setVertices(gl::TriangleStrip, verts, gl::Dynamic);
    }

    void draw()
    {
        Rectanglei pos;
        if(self.checkPlace(pos) || !bgBuf->isReady())
        {
            // Update the background quad.
            bgBuf->setVertices(gl::TriangleStrip,
                               VertexBuf::Builder().makeQuad(pos,
                                                             self.style().colors().colorf("background"),
                                                             self.root().atlas().imageRectf(bgTex).middle()),
                               gl::Static);
        }

        updateGeometry();

        GLState &st = GLState::push();
        st.setScissor(pos);

        // Draw the background.
        background.draw();

        // First draw the shadow of the text.
        uMvpMatrix = projMatrix *
                     Matrix4f::translate(Vector2f(pos.topLeft + Vector2i(margin, 0)));
        uShadowColor = Vector4f(0, 0, 0, 1);
        contents.draw();

        // Draw the text itself.
        uMvpMatrix = projMatrix *
                     Matrix4f::translate(Vector2f(pos.topLeft + Vector2i(margin, -2)));
        uShadowColor = Vector4f(1, 1, 1, 1);
        contents.draw();

        GLState::pop();

        // We don't need to keep all entries ready for drawing immediately.
        releaseExcessComposedEntries();

        // Notify now that we know what the max scroll is.
        if(lastMaxScroll != maxScroll)
        {
            lastMaxScroll = maxScroll;
            emit self.scrollMaxChanged(maxScroll);
        }
    }
};

LogWidget::LogWidget(String const &name) : GuiWidget(name), d(new Instance(this))
{
    rule().setInput(Rule::Width, Const(600)); // TODO -- from rule defs
    LogBuffer::appBuffer().addSink(d->sink);
}

LogSink &LogWidget::logSink()
{
    return d->sink;
}

void LogWidget::clear()
{
    d->clear();
}

int LogWidget::scrollPosition() const
{
    return d->visibleOffset;
}

int LogWidget::scrollPageSize() const
{
    return de::max(1, rule().height().valuei() / 2);
}

int LogWidget::maximumScroll() const
{
    return d->lastMaxScroll;
}

void LogWidget::scroll(int to)
{
    d->visibleOffset = de::max(0, to);
}

void LogWidget::viewResized()
{
    d->updateProjection();
}

void LogWidget::draw()
{
    d->sink.setWidth(d->contentWidth());

    d->draw();
}

bool LogWidget::handleEvent(Event const &event)
{
    if(!event.isKeyDown()) return false;

    KeyEvent const &ev = static_cast<KeyEvent const &>(event);

    int pageSize = scrollPageSize();

    switch(ev.ddKey())
    {
    case DDKEY_PGUP:
        d->setVisibleOffset(int(d->visibleOffset.target()) + pageSize);
        d->restartScrollOpacityFade();
        return true;

    case DDKEY_PGDN:
        d->setVisibleOffset(de::max(0, int(d->visibleOffset.target()) - pageSize));
        d->restartScrollOpacityFade();
        return true;

    default:
        break;
    }

    return GuiWidget::handleEvent(event);
}

void LogWidget::scrollToBottom()
{
    d->setVisibleOffset(0);
}

void LogWidget::pruneExcessEntries()
{

}

void LogWidget::glInit()
{
    d->glInit();
}

void LogWidget::glDeinit()
{
    d->glDeinit();
}
