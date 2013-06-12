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
#include <de/MouseEvent>
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

DENG2_PIMPL(LogWidget),
DENG2_OBSERVES(Atlas, Reposition),
public Font::RichFormat::IStyle
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
            DENG2_GUARD(this);
            if(composer.isReady())
            {
                composer.release();
            }
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
                d->self.modifyContentHeight(delta);

                /// @todo Adjust the scroll position if this entry is below it
                /// (would cause a visible scroll to occur).

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
    Rangei visibleRange;
    Animation contentOffset; ///< Additional vertical offset to apply when drawing content.

    // Style.
    Font const *font;
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;

    // GL objects.
    VertexBuf *buf;
    VertexBuf *bgBuf;
    AtlasTexture *entryAtlas;
    bool entryAtlasLayoutChanged;
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
          visibleRange(Rangei(-1, -1)),
          font(0),
          buf(0),
          entryAtlas(0),
          entryAtlasLayoutChanged(false),
          uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4),
          uTex        ("uTex",       GLUniform::Sampler2D),
          uShadowColor("uColor",     GLUniform::Vec4),
          uColor      ("uColor",     GLUniform::Vec4),
          uBgMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {
        self.setFont("log.normal");
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

        font           = &self.font();

        normalColor    = st.colors().color("log.normal");
        highlightColor = st.colors().color("log.highlight");
        dimmedColor    = st.colors().color("log.dimmed");
        accentColor    = st.colors().color("log.accent");
        dimAccentColor = st.colors().color("log.dimaccent");

        self.set(Background(st.colors().colorf("background"), Background::Blurred));
        //self.set(Background(Vector4f(1, 1, 1, 1), Background::Blurred));
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
        // Private atlas for the composed entry text lines.
        entryAtlas = AtlasTexture::newWithRowAllocator(
                Atlas::BackingStore | Atlas::AllowDefragment,
                GLTexture::maximumSize().min(Atlas::Size(2048, 1024)));
        entryAtlas->audienceForReposition += this;

        // Simple texture for the scroll indicator.
        Image solidWhitePixel = Image::solidColor(Image::Color(255, 255, 255, 255),
                                                  Image::Size(1, 1));
        scrollTex = entryAtlas->alloc(solidWhitePixel);
        self.setIndicatorUv(entryAtlas->imageRectf(scrollTex).middle());

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

        contents.clear();
        background.clear();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        if(entryAtlas == &atlas)
        {
            entryAtlasLayoutChanged = true;
            self.setIndicatorUv(entryAtlas->imageRectf(scrollTex).middle());
        }
    }

    duint contentWidth() const
    {
        return self.viewportSize().x;
    }

    int maxVisibleOffset()
    {
        return self.maximumScrollY().valuei();
    }

    void fetchNewCachedEntries()
    {
        int oldHeight = self.contentHeight();

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

            self.modifyContentHeight(cached->height());

            // Adjust visible offset so it remains fixed in relation to
            // existing entries.
            if(self.scrollPositionY().animation().target() > 0)
            {
                self.scrollPositionY().shift(cached->height());
                //emit self.scrollPositionChanged(visibleOffset.target());
            }
        }

        if(self.contentHeight() > oldHeight)
        {
            emit self.contentHeightIncreased(self.contentHeight() - oldHeight);
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

        int len = de::max(10, visibleRange.size());

        // Excess entries before the visible range.
        int excess = visibleRange.start - len;
        for(int i = 0; i <= excess; ++i)
        {
            cache[i]->clear();
        }

        // Excess entries after the visible range.
        excess = visibleRange.end + len;
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
                self.modifyContentHeight(-cache[0]->height());
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

    void updateGeometry()
    {
        Vector2i const contentSize = self.viewportSize();

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
        //prune();

        VertexBuf::Builder verts;

        for(int attempt = 0; attempt < 2; ++attempt)
        {
            // Draw in reverse, as much as we need.
            int yBottom = contentSize.y + self.scrollPositionY().valuei();
            visibleRange = Rangei(-1, -1);
            entryAtlasLayoutChanged = false;

            // Find the visible range and update all visible entries.
            for(int idx = cache.size() - 1; yBottom > -contentOffset && idx >= 0; --idx)
            {
                CacheEntry *entry = cache[idx];
                yBottom -= entry->height();

                if(yBottom + contentOffset < contentSize.y)
                {
                    // Rasterize and allocate if needed.
                    entry->make(verts, yBottom);

                    // Update the visible range.
                    if(visibleRange.end == -1)
                    {
                        visibleRange.end = idx;
                    }
                    visibleRange.start = idx;
                }
            }

            if(entryAtlasLayoutChanged)
            {
                // Oops, the atlas was optimized during the loop and some items'
                // positions are obsolete.
                verts.clear();
                continue;
            }

            break;
        }

        // Draw the scroll indicator, too.
        self.glMakeScrollIndicatorGeometry(verts);

        buf->setVertices(gl::TriangleStrip, verts, gl::Dynamic);
    }

    void draw()
    {
        Rectanglei pos;
        if(self.hasChangedPlace(pos) || !bgBuf->isReady())
        {
            // Update the background quad.
            VertexBuf::Builder bgVerts;
            self.glMakeGeometry(bgVerts);
            bgBuf->setVertices(gl::TriangleStrip, bgVerts, gl::Static);
        }

        //updateGeometry();

        // Draw the background.
        background.draw();

        Rectanglei vp = self.viewport();
        if(vp.height() > 0)
        {
            GLState &st = GLState::push();
            // Leave room for the indicator in the scissor.
            st.setScissor(vp.adjusted(Vector2i(), Vector2i(self.rightMargin(), 0)));

            // First draw the shadow of the text.
            uMvpMatrix = projMatrix * Matrix4f::translate(
                         Vector2f(vp.topLeft + Vector2i(0, contentOffset)));
            uShadowColor = Vector4f(0, 0, 0, 1);
            contents.draw();

            // Draw the text itself.
            uMvpMatrix = projMatrix * Matrix4f::translate(
                         Vector2f(vp.topLeft + Vector2i(0, contentOffset - 1)));
            uShadowColor = Vector4f(1, 1, 1, 1);
            contents.draw();

            GLState::pop();
        }

        // We don't need to keep all entries ready for drawing immediately.
        releaseExcessComposedEntries();
    }
};

LogWidget::LogWidget(String const &name)
    : ScrollAreaWidget(name), d(new Instance(this))
{
    setOrigin(Bottom);

    connect(&d->rewrapPool, SIGNAL(allTasksDone()), this, SLOT(pruneExcessEntries()));

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

void LogWidget::setContentYOffset(Animation const &anim)
{
    if(isAtBottom())
    {
        d->contentOffset = anim;
    }
    else
    {
        // When not at the bottom, the content is expected to stay fixed in place.
        d->contentOffset = 0;
    }
}

Animation const &LogWidget::contentYOffset() const
{
    return d->contentOffset;
}

void LogWidget::viewResized()
{
    d->updateProjection();
}

void LogWidget::update()
{
    ScrollAreaWidget::update();

    // The log widget's geometry is fully dynamic -- regenerated on every frame.
    d->updateGeometry();
}

void LogWidget::drawContent()
{
    d->sink.setWidth(d->contentWidth());
    d->draw();
}

bool LogWidget::handleEvent(Event const &event)
{
    return ScrollAreaWidget::handleEvent(event);
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
