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

DENG2_PIMPL(LogWidget), public Font::RichFormat::IStyle
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    /**
     * Cached log entry ready for drawing. The styled text of the entry is
     * wrapped onto multiple lines according to the available content width.
     *
     * CacheEntry is Lockable because it is accessed both by the main thread
     * when drawing and by RewrapTask when wrapping the cached entries to a
     * resized content width.
     */
    class CacheEntry : public Lockable
    {
        int _height; ///< Current height of the entry, in pixels.

    public:
        int sinkIndex; ///< Index of the corresponding entry in the Sink (for sorting).
        Font::RichFormat format;
        FontLineWrapping wraps;
        GLTextComposer composer;

        CacheEntry(int index, Font const &font, Font::RichFormat::IStyle &richStyle, Atlas &atlas)
            : _height(0), sinkIndex(index), format(richStyle)
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
            return _height;
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
            recalculateHeight();
        }

        /// Returns the difference in the entry's height (in pixels).
        int rewrap(int width)
        {
            DENG2_GUARD(this);
            int oldHeight = _height;
            wraps.wrapTextToWidth(wraps.text(), format, width);
            recalculateHeight();
            return _height - oldHeight;
        }

        void recalculateHeight()
        {
            _height = wraps.height() * wraps.font().lineSpacing().valuei();
        }

        void make(GLTextComposer::Vertices &verts, int y)
        {
            DENG2_GUARD(this);
            composer.update();
            composer.makeVertices(verts, Vector2i(0, y), AlignLeft);
        }

        void clear()
        {
            composer.release();
        }
    };

    /**
     * Log sink that wraps log entries as rich text to multiple lines before
     * they are shown by LogWidget. The wrapping is done by a TaskPool in the
     * background.
     *
     * Whenever all the wrapping tasks are complete, LogWidget will be notified
     * and it will check if excess entries should be removed. Entries are only
     * removed from the sink (and cache) during a prune, in the main thread,
     * and during this no background tasks are running.
     */
    class WrappingMemoryLogSink : public MemoryLogSink
    {
    public:
        WrappingMemoryLogSink(LogWidget::Instance *wd)
            : d(wd),
              _maxEntries(1000),
              _next(0),
              _width(0)
        {
            // Whenever the pool is idle, we'll check if pruning should be done.
            QObject::connect(&_pool, SIGNAL(allTasksDone()),
                             d->thisPublic, SLOT(pruneExcessEntries()));
        }

        ~WrappingMemoryLogSink()
        {
            _pool.waitForDone();
            clear();
        }

        bool isBusy() const
        {
            return !_pool.isDone();
        }

        int maxEntries() const { return _maxEntries; }

        void clear()
        {
            DENG2_GUARD(_wrappedEntries);
            qDeleteAll(_wrappedEntries);
            _wrappedEntries.clear();
        }

        void remove(int pos, int n = 1)
        {
            DENG2_GUARD(this);
            MemoryLogSink::remove(pos, n);
            _next -= n;
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
        /**
         * Asynchronous task for wrapping an incoming entry as rich text in the
         * background. WrapTask is responsible for creating the CacheEntry
         * instances for the LogWidget's entry cache.
         */
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

                //usleep(75000); // TODO -- remove this testing aid

                DENG2_GUARD_FOR(_sink._wrappedEntries, G);
                _sink._wrappedEntries << cached;
            }

        private:
            WrappingMemoryLogSink &_sink;
            int _index;
            String _styledText;
        };

        /**
         * Schedules wrapping tasks for all incoming entries.
         */
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
        TaskPool _pool;
        int _width;

        struct WrappedEntries : public QList<CacheEntry *>, public Lockable {};
        WrappedEntries _wrappedEntries;
    };

    WrappingMemoryLogSink sink;

    QList<CacheEntry *> cache; ///< Indices match entry indices in sink.
    int cacheWidth;

    /**
     * Asynchronous task that iterates through the cached entries in reverse
     * order and rewraps their existing content to a new maximum width. The
     * task is cancellable because an existing wrap should be abandoned if the
     * widget content width changes again during a rewrap.
     *
     * The total height of the entries is updated as the entries are rewrapped.
     */
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
                CacheEntry *e = d->cache[_next--];

                // Rewrap and update total height.
                int delta = e->rewrap(_width);
                d->totalHeight += delta;

                if(_next < d->visibleRange.end)
                {
                    // Above the visible range, no need to rush.
                    TimeDelta(.001).sleep();
                }
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
    ColorBank::Color dimAccentColor;
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
    Matrix4f projMatrix;
    Matrix4f viewMatrix;
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
          uBgMvpMatrix("uMvpMatrix", GLUniform::Mat4)
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
        dimAccentColor = st.colors().color("log.dimaccent");
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

        case Font::RichFormat::DimAccentColor:
            return dimAccentColor;
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
            colorIndex = Font::RichFormat::DimAccentColor;
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
        scrollTex = entryAtlas->alloc(solidWhitePixel);

        uTex = entryAtlas;
        uColor = Vector4f(1, 1, 1, 1);

        background.addBuffer(bgBuf = new VertexBuf);
        self.root().shaders().build(background.program(), "generic.textured.color")
                << uBgMvpMatrix
                << self.root().uAtlas();

        // Vertex buffer for the log entries.
        contents.addBuffer(buf = new VertexBuf);
        self.root().shaders().build(contents.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uShadowColor
                << uTex;
    }

    void glDeinit()
    {
        clearCache();

        delete entryAtlas;
        entryAtlas = 0;
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
        setVisibleOffset(de::min(int(visibleOffset), maxVisibleOffset(visibleHeight)), 0);
    }

    void setVisibleOffset(int off, float span)
    {
        if(int(visibleOffset) != off)
        {
            visibleOffset.setValue(off, span);
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
                visibleOffset.shift(cached->height());
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

    /**
     * Removes entries from the sink and the cache.
     */
    void prune()
    {
        DENG2_ASSERT_IN_MAIN_THREAD();

        if(isRewrapping())
        {
            // Rewrapper is busy, let's not do this right now.
            return;
        }

        // We must lock the sink so no new entries are added.
        DENG2_GUARD(sink);

        if(sink.isBusy())
        {
            // New entries are being added, prune later.
            return;
        }

        fetchNewCachedEntries();

        DENG2_ASSERT(sink.entryCount() == cache.size());

        int num = sink.entryCount() - sink.maxEntries();
        if(num > 0)
        {
            sink.remove(0, num);
            for(int i = 0; i < num; ++i)
            {
                totalHeight -= cache[0]->height();
                delete cache.takeFirst();
            }
            // Adjust existing indices to match.
            for(int i = 0; i < cache.size(); ++i)
            {
                cache[i]->sinkIndex -= num;
            }
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
                                   self.root().atlas().imageRectf(self.root().solidWhitePixel()).middle()),
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
    connect(&d->rewrapPool, SIGNAL(allTasksDone()), this, SLOT(pruneExcessEntries()));

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

void LogWidget::update()
{
    GuiWidget::update();
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
        if(ev.modifiers().testFlag(KeyEvent::Shift))
        {
            d->setVisibleOffset(d->maxScroll, .3f);
        }
        else
        {
            d->setVisibleOffset(int(d->visibleOffset.target()) + pageSize, .3f);
        }
        d->restartScrollOpacityFade();
        return true;

    case DDKEY_PGDN:
        if(ev.modifiers().testFlag(KeyEvent::Shift))
        {
            scrollToBottom();
        }
        else
        {
            d->setVisibleOffset(de::max(0, int(d->visibleOffset.target()) - pageSize), .3f);
        }
        d->restartScrollOpacityFade();
        return true;

    default:
        break;
    }

    return GuiWidget::handleEvent(event);
}

void LogWidget::scrollToBottom()
{
    d->setVisibleOffset(0, .3f);
}

void LogWidget::pruneExcessEntries()
{
    d->prune();
}

void LogWidget::glInit()
{
    d->glInit();
}

void LogWidget::glDeinit()
{
    d->glDeinit();
}
