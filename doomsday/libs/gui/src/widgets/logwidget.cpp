/** @file widgets/logwidget.cpp  Widget for output message log.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/logwidget.h"
#include "de/textdrawable.h"
#include "de/ui/style.h"

#include <de/keyevent.h>
#include <de/mouseevent.h>
#include <de/memorylogsink.h>
#include <de/logbuffer.h>
#include <de/atlastexture.h>
#include <de/drawable.h>
#include <de/vertexbuilder.h>
#include <de/app.h>
#include <de/image.h>

namespace de {

using namespace ui;

DE_GUI_PIMPL(LogWidget)
, DE_OBSERVES(Atlas, OutOfSpace)
, public Font::RichFormat::IStyle
{
    typedef GLBufferT<GuiVertex> VertexBuf;

    /**
     * Cached log entry ready for drawing. TextDrawable takes the styled text of the
     * entry and wraps it onto multiple lines according to the available content width.
     *
     * The height of the entry is initially zero. When TextDrawable has finished
     * laying out and preparing the text, the real height is then updated and the
     * content height of the log increases.
     *
     * CacheEntry is accessed only from the main thread. However, instances may be
     * initially created also in background threads (if they happen to flush the log).
     */
    class CacheEntry
    {
        bool _needWrap { true };
        int _wrapWidth { 0 };
        int _height;    ///< Current height of the entry, in pixels.
        int _oldHeight; ///< Previous height, before calling updateVisibility().

    public:
        TextDrawable drawable;

        CacheEntry(const Font &font, Font::RichFormat::IStyle &richStyle, Atlas &atlas)
            : _height(0)
            , _oldHeight(0)
        {
            drawable.init(atlas, font, &richStyle);
            drawable.setRange(Rangei()); // Determined later.
        }

        ~CacheEntry()
        {
            // Free atlas allocations.
            drawable.deinit();
        }

        int height() const
        {
            return _height;
        }

        int oldHeight() const
        {
            return _oldHeight;
        }

        bool isReady() const
        {
            return drawable.isReady();
        }

        void setupWrap(const String &richText, int width)
        {
            drawable.setText(richText);
            _needWrap = true;
            _wrapWidth = width;
        }

        void rewrap(int width)
        {
            drawable.setLineWrapWidth(width);
        }

        /**
         * Returns the possible delta in the height of the entry.
         * Does not block even though a long wrapping task is in progress.
         */
        int update()
        {
            const int old = _height;
            if (drawable.update())
            {
                _height = drawable.wraps().height() * drawable.font().lineSpacing().valuei();
                return _height - old;
            }
            return 0;
        }

        void beginWrap()
        {
            if (_needWrap)
            {
                drawable.setLineWrapWidth(_wrapWidth);
                _needWrap = false;
            }
        }

        /**
         * Updates the entry's visibility: which lines might be visible to the user
         * and thus need to be allocated on an atlas and ready to draw.
         *
         * @param yBottom        Bottom coordinate for the entry.
         * @param visiblePixels  Full range of currently visible pixels in the widget.
         *
         * @return Possible change in the height of the entry.
         */
        int updateVisibility(int yBottom, const Rangei &visiblePixels)
        {
            // If the wrapping hasn't been started yet for this item, do so now.
            beginWrap();

            int heightDelta = 0;

            // Remember the height we had prior to any updating.
            _oldHeight = _height;

            /*
             * At this point:
             * - we may have no content ready yet (_height is 0)
             * - wrapping may have completed for the first time
             * - wrapping may be ongoing for rewrapping, but we can still update the
             *   current content's visibility
             * - wrapping may have completed for an updated content
             */

            if (!drawable.isBeingWrapped())
            {
                // We may now have the number of wrapped lines.
                heightDelta += update();
            }
            if (!_height)
            {
                // Content not ready yet.
                return 0;
            }

            // Determine which lines might be visible.
            const int lineSpacing = drawable.font().lineSpacing().value();
            const int yTop = yBottom - _height;
            Rangei range;

            if (yBottom < visiblePixels.start || yTop > visiblePixels.end)
            {
                // Completely outside.
            }
            else if (yTop >= visiblePixels.start && yBottom <= visiblePixels.end)
            {
                // Completely inside.
                range = Rangei(0, drawable.wraps().height());
            }
            else if (yTop < visiblePixels.start && yBottom > visiblePixels.end)
            {
                // Extends over the whole range and beyond.
                range = Rangei::fromSize((visiblePixels.start - yTop) / lineSpacing,
                                         (visiblePixels.end - visiblePixels.start) / lineSpacing + 1);
            }
            else if (yBottom > visiblePixels.end)
            {
                DE_ASSERT(yTop >= visiblePixels.start);

                // Partially inside.
                range = Rangei(0, (visiblePixels.end - yTop) / lineSpacing);
            }
            else
            {
                DE_ASSERT(yBottom <= visiblePixels.end);

                // Partially inside.
                int count = (yBottom - visiblePixels.start) / lineSpacing;
                range = Rangei(drawable.wraps().height() - count,
                               drawable.wraps().height());
            }

            drawable.setRange(range);

            // Updating will prepare the visible lines for drawing.
            return update() + heightDelta;
        }

        void make(GuiVertexBuilder &verts, int y)
        {
            DE_ASSERT(isReady());
            drawable.makeVertices(verts, Vec2i(0, y), AlignLeft);
        }

        void releaseFromAtlas()
        {
            drawable.setRange(Rangei()); // Nothing visible.
        }
    };

    /**
     * Log sink that where all entries that will be visible in the widget will
     * be received. For each entry, a CacheEntry is created and its TextDrawable
     * will start processing the entry contents in the background.
     *
     * LogWidget will periodically check if excess entries should be removed. Entries are
     * only removed from the sink (and cache) during a prune, in the main thread,
     * during which the sink is locked.
     */
    class WrappingMemoryLogSink : public MemoryLogSink
    {
    public:
        WrappingMemoryLogSink(LogWidget::Impl *wd)
            : d(wd)
            , _maxEntries(1000)
            , _next(0)
            , _width(0)
        {}

        ~WrappingMemoryLogSink()
        {
            clear();
        }

        int maxEntries() const { return _maxEntries; }

        void clear()
        {
            DE_GUARD(_wrappedEntries);
            deleteAll(_wrappedEntries);
            _wrappedEntries.clear();
        }

        void remove(int pos, int n = 1)
        {
            DE_GUARD(this);
            DE_ASSERT(pos + n <= _next);
            MemoryLogSink::remove(pos, n);
            _next -= n;
        }

        void setWidth(int wrapWidth)
        {
            _width = wrapWidth;
            beginWorkOnNext();
        }

        void addedNewEntry(LogEntry &)
        {
            beginWorkOnNext();
        }

        CacheEntry *nextCachedEntry()
        {
            DE_GUARD(_wrappedEntries);
            if (_wrappedEntries.isEmpty()) return 0;
            return _wrappedEntries.takeFirst();
        }

        /**
         * Schedules wrapping tasks for all incoming entries.
         */
        void beginWorkOnNext()
        {
            if (!d->formatter) return; // Must have a formatter.

            DE_GUARD(this);

            while (_width > 0 && _next >= 0 && _next < entryCount())
            {
                const LogEntry &ent = entry(_next);
                const String styled = d->formatter->logEntryToTextLines(ent).at(0);

                CacheEntry *cached = new CacheEntry(*d->font, *d, *d->entryAtlas);
                cached->setupWrap(styled, _width);

                // The cached entry will be passed to the widget when it's ready to
                // receive new ones.
                {
                    DE_GUARD(_wrappedEntries);
                    _wrappedEntries << cached;
                }

                _next++;
            }
        }

    private:
        LogWidget::Impl *d;
        int _maxEntries;
        int _next;
        int _width;

        struct WrappedEntries : public List<CacheEntry *>, public Lockable {};
        WrappedEntries _wrappedEntries; ///< New entries possibly created in background threads.
    };

    WrappingMemoryLogSink sink;

    List<CacheEntry *> cache; ///< Cached entries in use when drawing.
    int cacheWidth;

    // State.
    Rangei visibleRange;
    Animation contentOffset; ///< Additional vertical offset to apply when drawing content.
    int contentOffsetForDrawing; ///< Set when geometry updated.

    // Style.
    LogSink::IFormatter *formatter;
    const Font *font;
    ColorBank::Color normalColor;
    ColorBank::Color highlightColor;
    ColorBank::Color dimmedColor;
    ColorBank::Color accentColor;
    ColorBank::Color dimAccentColor;
    ColorBank::Color altAccentColor;

    // GL objects.
    VertexBuf *buf;
    VertexBuf *bgBuf;
    AtlasTexture *entryAtlas;
    bool entryAtlasLayoutChanged;
    bool entryAtlasFull;
    Drawable contents;
    Drawable background;
    GLUniform uMvpMatrix;
    GLUniform uTex;
    GLUniform uShadowColor;
    GLUniform uColor;
    GLUniform uBgMvpMatrix;
    Mat4f projMatrix;
    Mat4f viewMatrix;
    Id scrollTex;

    Impl(Public *i)
        : Base(i)
        , sink(this)
        , cacheWidth(0)
        , visibleRange(Rangei(-1, -1))
        , formatter(nullptr)
        , font(nullptr)
        , buf(nullptr)
        , entryAtlas(nullptr)
        , entryAtlasLayoutChanged(false)
        , entryAtlasFull(false)
        , uMvpMatrix  ("uMvpMatrix", GLUniform::Mat4)
        , uTex        ("uTex",       GLUniform::Sampler2D)
        , uShadowColor("uColor",     GLUniform::Vec4)
        , uColor      ("uColor",     GLUniform::Vec4)
        , uBgMvpMatrix("uMvpMatrix", GLUniform::Mat4)
    {
        self().setFont("log.normal");
        updateStyle();
    }

    ~Impl()
    {
        LogBuffer::get().removeSink(sink);
        DE_ASSERT(cache.isEmpty());
    }

    void clear()
    {
        sink.clear();
        clearCache();
        self().setContentHeight(0);
    }

    void clearCache()
    {
        deleteAll(cache); // Ongoing text wrapping cancelled automatically.
        cache.clear();
    }

    void updateStyle()
    {
        const Style &st = style();

        font           = &self().font();

        normalColor    = st.colors().color("log.normal");
        highlightColor = st.colors().color("log.highlight");
        dimmedColor    = st.colors().color("log.dimmed");
        accentColor    = st.colors().color("log.accent");
        dimAccentColor = st.colors().color("log.dimaccent");
        altAccentColor = st.colors().color("log.altaccent");

        self().set(Background(st.colors().colorf("background")));
    }

    Font::RichFormat::IStyle::Color richStyleColor(int index) const
    {
        switch (index)
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

        case Font::RichFormat::AltAccentColor:
            return altAccentColor;
        }
    }

    void richStyleFormat(int contentStyle,
                         float &sizeFactor,
                         Font::RichFormat::Weight &fontWeight,
                         Font::RichFormat::Style &fontStyle,
                         int &colorIndex) const
    {
        return style().richStyleFormat(contentStyle, sizeFactor, fontWeight, fontStyle, colorIndex);
    }

    const Font *richStyleFont(Font::RichFormat::Style fontStyle) const
    {
        return style().richStyleFont(fontStyle);
    }

    void glInit()
    {
        // Private atlas for the composed entry text lines.
        entryAtlas = AtlasTexture::newWithRowAllocator(
                Atlas::BackingStore | Atlas::AllowDefragment,
                GLTexture::maximumSize().min(Atlas::Size(4096, 2048)));

        entryAtlas->audienceForReposition() += this;
        entryAtlas->audienceForOutOfSpace() += this;

        // Simple texture for the scroll indicator.
        /*Image solidWhitePixel = Image::solidColor(Image::Color(255, 255, 255, 255),
                                                  Image::Size(1, 1));
        scrollTex = entryAtlas->alloc(solidWhitePixel);
        self().setIndicatorUv(entryAtlas->imageRectf(scrollTex).middle());*/
        //self().setIndicatorUv(root().atlas().imageRectf(root().solidWhitePixel()).middle());

        uTex = entryAtlas;
        uColor = Vec4f(1);

        background.addBuffer(bgBuf = new VertexBuf);
        shaders().build(background.program(), "generic.textured.color")
                << uBgMvpMatrix
                << uAtlas();

        // Vertex buffer for the log entries.
        contents.addBuffer(buf = new VertexBuf);
        shaders().build(contents.program(), "generic.textured.color_ucolor")
                << uMvpMatrix
                << uShadowColor
                << uTex;
    }

    void glDeinit()
    {
        clearCache();

        delete entryAtlas;
        entryAtlas = 0;
        scrollTex = Id::None;

        contents.clear();
        background.clear();
    }

    void atlasContentRepositioned(Atlas &atlas)
    {
        if (entryAtlas == &atlas)
        {
            entryAtlasLayoutChanged = true;
            self().setIndicatorUv(entryAtlas->imageRectf(scrollTex).middle());
        }
    }

    void atlasOutOfSpace(Atlas &atlas)
    {
        if (entryAtlas == &atlas)
        {
            entryAtlasFull = true;
        }
    }

    duint contentWidth() const
    {
        return self().viewportSize().x;
    }

    int maxVisibleOffset()
    {
        return self().maximumScrollY().valuei();
    }

    void modifyContentHeight(float delta)
    {
        self().modifyContentHeight(delta);

        // TODO: If content height changes below the visible range, we should adjust
        // the current scroll position so that the entries don't change position
        // inside the view.

        if (!self().isAtBottom())
        {
            self().scrollPositionY().shift(delta);
        }
    }

    void fetchNewCachedEntries()
    {
        while (CacheEntry *cached = sink.nextCachedEntry())
        {
            cache << cached;
        }
    }

    void rewrapCache()
    {
        int startFrom = max(0, visibleRange.start);

        // Resize entries starting from the first visible entry, continue down to the
        // most recent entry.
        for (int idx = startFrom; idx < cache.sizei(); ++idx)
        {
            cache[idx]->rewrap(contentWidth());
        }

        // Resize the rest of the items (above the visible range).
        for (int idx = startFrom - 1; idx >= 0; --idx)
        {
            cache[idx]->rewrap(contentWidth());
        }
    }

    void releaseExcessComposedEntries()
    {
        if (visibleRange < 0) return;

        int len = de::max(10, visibleRange.size());

        // Excess entries before the visible range.
        int excess = visibleRange.start - len;
        for (int i = 0; i <= excess && i < cache.sizei(); ++i)
        {
            cache[i]->releaseFromAtlas();
        }

        // Excess entries after the visible range.
        excess = visibleRange.end + len;
        for (int i = excess; i < cache.sizei(); ++i)
        {
            cache[i]->releaseFromAtlas();
        }
    }

    /**
     * Releases all entries currently stored in the entry atlas.
     */
    void releaseAllNonVisibleEntries()
    {
        for (int i = 0; i < cache.sizei(); ++i)
        {
            if (!visibleRange.contains(i))
            {
                cache[i]->releaseFromAtlas();
            }
        }
    }

    /**
     * Removes entries from the sink and the cache.
     */
    void prune()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        // Remove oldest excess entries.
        int num = cache.sizei() - sink.maxEntries();
        if (num > 0)
        {
            // There is one sink entry and one cached entry for each log entry.
            sink.remove(0, num);
            for (int i = 0; i < num; ++i)
            {
                self().modifyContentHeight(-cache.first()->height());
                delete cache.takeFirst();
            }
        }
    }

    void updateProjection()
    {
        projMatrix = root().projMatrix2D();

        uBgMvpMatrix = projMatrix;
    }

    Rangei extendPixelRangeWithPadding(const Rangei &range)
    {
        const int padding = range.size() / 2;
        return Rangei(range.start - padding, range.end + padding);
    }

    void updateGeometry()
    {
        bool needHeightNotify = false; // if changed as entries are updated
        int heightDelta = 0;
        const Vec2i contentSize = self().viewportSize();

        // If the width of the widget changes, text needs to be reflowed with the
        // new width.
        if (cacheWidth != contentSize.x)
        {
            rewrapCache();
            cacheWidth = contentSize.x;
        }

        // If the atlas becomes full, we'll retry once.
        entryAtlasFull = false;

        VertexBuf::Builder verts;

        // Draw in reverse, as much as we need.
        int initialYBottom = contentSize.y + self().scrollPositionY().valuei();
        contentOffsetForDrawing = std::ceil(contentOffset.value());

        Rangei visiblePixelRange = extendPixelRangeWithPadding(
                    Rangei(-contentOffsetForDrawing, contentSize.y - contentOffsetForDrawing));
        if (!isVisible())
        {
            // The widget is hidden, so there's no point in loading anything into the atlas.
            visiblePixelRange = Rangei(); // Nothing to be seen.
        }

        for (int attempt = 0; attempt < 2; ++attempt)
        {
            if (entryAtlasFull)
            {
                // Hopefully releasing some entries will make it possible to fit the
                // new entries.
                releaseAllNonVisibleEntries();
                entryAtlasFull = false;
            }

            int yBottom = initialYBottom;
            visibleRange = Rangei(-1, -1);
            entryAtlasLayoutChanged = false;

            // Find the visible range and update all visible entries.
            for (int idx = cache.sizei() - 1; yBottom >= -contentOffsetForDrawing && idx >= 0; --idx)
            {
                CacheEntry *entry = cache[idx];

                if (int delta = entry->updateVisibility(yBottom, visiblePixelRange))
                {
                    heightDelta += delta;

                    if (!entry->oldHeight())
                    {
                        // A bit of the kludge: height was changed due to a first-time
                        // update of content (new content appears rather than being a rewrap).
                        needHeightNotify = true;

                        // Don't draw this entry yet; we'll need to fire the notification
                        // first to ensure that offsets are properly set so that the
                        // new entry's height is taken into account. The new entry will
                        // be visible on the next frame.
                        continue;
                    }
                }

                yBottom -= entry->height();

                if (entry->isReady() && yBottom + contentOffsetForDrawing <= contentSize.y)
                {
                    entry->make(verts, yBottom);

                    // Update the visible range.
                    if (visibleRange.end == -1)
                    {
                        visibleRange.end = idx;
                    }
                    visibleRange.start = idx;
                }

                if (entryAtlasLayoutChanged || entryAtlasFull)
                {
                    goto nextAttempt;
                }
            }

            // Successfully completed.
            break;

nextAttempt:
            // Oops, the atlas was optimized during the loop and some items'
            // positions are obsolete.
            verts.clear();
        }

        // Draw the scroll indicator, too.
        //self().glMakeScrollIndicatorGeometry(verts);

        buf->setVertices(gfx::TriangleStrip, verts, gfx::Dynamic);

        // Apply changes to content height that may have occurred as text becomes
        // available for drawing.
        if (heightDelta)
        {
            modifyContentHeight(heightDelta);
            if (needHeightNotify && heightDelta > 0)
            {
                DE_NOTIFY_PUBLIC(ContentHeight, i)
                {
                    i->contentHeightIncreased(self(), heightDelta);
                }
            }
        }

        // We don't need to keep all entries ready for drawing immediately.
        releaseExcessComposedEntries();
    }

    bool isVisible() const
    {
        Rectanglei vp = self().viewport();
        return vp.height() > 0 && vp.right() >= 0;
    }

    void draw()
    {
        Rectanglei pos;
        if (self().hasChangedPlace(pos) || !bgBuf->isReady())
        {
            // Update the background quad.
            VertexBuf::Builder bgVerts;
            self().glMakeGeometry(bgVerts);
            bgBuf->setVertices(gfx::TriangleStrip, bgVerts, gfx::Static);
        }

        background.draw();

        Rectanglei vp = self().viewport();
        if (vp.height() > 0)
        {
            GLState &st = GLState::push();

            // Leave room for the indicator in the scissor.
            st.setNormalizedScissor(
                    self().normalizedRect(
                            vp.adjusted(Vec2i(), Vec2i(self().margins().right().valuei(), 0))));

            // First draw the shadow of the text.
            uMvpMatrix = projMatrix * Mat4f::translate(
                         Vec2f(vp.topLeft + Vec2i(0, contentOffsetForDrawing)));
            uShadowColor = Vec4f(0, 0, 0, 1);
            contents.draw();

            // Draw the text itself.
            uMvpMatrix = projMatrix * Mat4f::translate(
                         Vec2f(vp.topLeft + Vec2i(0, contentOffsetForDrawing - 1)));
            uShadowColor = Vec4f(1);
            contents.draw();

            GLState::pop();
        }
    }

    DE_PIMPL_AUDIENCE(ContentHeight)
};

DE_AUDIENCE_METHODS(LogWidget, ContentHeight)

LogWidget::LogWidget(const String &name)
    : ScrollAreaWidget(name), d(new Impl(this))
{
    setOrigin(Bottom);
    enableIndicatorDraw(true);

    LogBuffer::get().addSink(d->sink);
}

void LogWidget::setLogFormatter(LogSink::IFormatter &formatter)
{
    d->formatter = &formatter;
}

void LogWidget::setPrivilegedEntries(bool onlyPrivileged)
{
    d->sink.setPrivileged(onlyPrivileged);
}

LogSink &LogWidget::logSink()
{
    return d->sink;
}

void LogWidget::clear()
{
    d->clear();
}

void LogWidget::setContentYOffset(const Animation &anim)
{
    if (isAtBottom())
    {
        d->contentOffset = anim;
    }
    else
    {
        // When not at the bottom, the content is expected to stay fixed in place.
        d->contentOffset = 0;
    }
}

const Animation &LogWidget::contentYOffset() const
{
    return d->contentOffset;
}

void LogWidget::viewResized()
{
    ScrollAreaWidget::viewResized();

    d->updateProjection();
}

void LogWidget::update()
{
    ScrollAreaWidget::update();

    d->sink.setWidth(d->contentWidth());
    d->fetchNewCachedEntries();
    d->prune();

    // The log widget's geometry is fully dynamic -- regenerated on every frame.
    d->updateGeometry();
}

void LogWidget::drawContent()
{
    root().painter().flush();
    d->draw();
    ScrollAreaWidget::drawContent();
}

bool LogWidget::handleEvent(const Event &event)
{
    return ScrollAreaWidget::handleEvent(event);
}

void LogWidget::glInit()
{
    ScrollAreaWidget::glInit();
    d->glInit();
}

void LogWidget::glDeinit()
{
    ScrollAreaWidget::glDeinit();
    d->glDeinit();
}

} // namespace de
