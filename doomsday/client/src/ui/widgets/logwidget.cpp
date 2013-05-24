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

#include <QImage>
#include <QPainter>

using namespace de;

DENG2_PIMPL(LogWidget), public Font::RichFormat::IStyle
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    // Log entries.
    MemoryLogSink sink;
    StyledLogSinkFormatter formatter;
    int maxEntries;

    /// Cache of drawable entries.
    struct CacheEntry
    {
        Font::RichFormat format;
        FontLineWrapping wraps;
        GLTextComposer composer;

        CacheEntry(Font const &font, Font::RichFormat::IStyle &richStyle, Atlas &atlas)
            : format(richStyle)
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
            return wraps.height() * wraps.font().lineSpacing().valuei();
        }

        bool needWrap() const
        {
            return wraps.isEmpty();
        }

        void wrap(String const &richText, int width)
        {
            String plain = format.initFromStyledText(richText);
            wraps.wrapTextToWidth(plain, format, width);
            composer.setText(plain, format);
            composer.setWrapping(wraps);
        }

        void make(GLTextComposer::Vertices &verts, int y)
        {
            composer.update();
            composer.makeVertices(verts, Vector2i(0, y), GLTextComposer::AlignLeft);
        }

        void clear()
        {
            composer.release();
        }
    };

    QList<CacheEntry *> cache; ///< Indices match entry indices in sink.
    int cacheWidth;

    // State.
    Animation visibleOffset;
    int maxScroll;
    int lastMaxScroll;
    int firstVisibleIndex;
    int lastVisibleIndex;
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
          maxEntries(1000),
          cacheWidth(0),
          visibleOffset(0),
          maxScroll(0),
          lastMaxScroll(0),
          firstVisibleIndex(-1),
          lastVisibleIndex(-1),
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

    void clearCache()
    {
        entryAtlas->clear();
        cache.clear();
    }

    void updateStyle()
    {
        Style const &st = self.style();

        font = &st.fonts().font("log.normal");
        margin = st.rules().rule("gap").valuei();
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

        // Adjust the visible range appropriately.
        firstVisibleIndex -= numPruned;
        lastVisibleIndex  -= numPruned;
        if(lastVisibleIndex >= 0 && firstVisibleIndex < 0)
        {
            firstVisibleIndex = 0;
        }
    }

    duint contentWidth() const
    {
        return de::max(margin, self.rule().width().valuei() - 2 * margin);
    }

    int totalHeight()
    {
        int total = 0;
        for(int idx = sink.entryCount() - 1; idx >= 0; --idx)
        {
            total += cache[idx]->height();
        }
        return total;
    }

    int maxVisibleOffset(int visibleHeight)
    {
        // Determine total height of all entries.
        return de::max(0, totalHeight() - visibleHeight);
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

    String styledTextForEntry(int index)
    {
        // No cached entry for this, generate one.
        LogEntry const &entry = sink.entry(index);

        StyledLogSinkFormatter::Lines lines = formatter.logEntryToTextLines(entry);
        DENG2_ASSERT(lines.size() == 1);
        return lines[0];
    }

    void cacheEntries()
    {
        // Cache entries we don't yet have.
        while(cache.size() < sink.entryCount())
        {
            int idx = cache.size();            

            // No cached entry for this, generate one.
            CacheEntry *cached = new CacheEntry(*font, *this, *entryAtlas);
            cached->wrap(styledTextForEntry(idx), contentWidth());
            cache.append(cached);

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
        for(int i = 0; i < cache.size(); ++i)
        {
            CacheEntry *entry = cache[i];
            entry->wrap(styledTextForEntry(i), contentWidth());
        }
    }

    void releaseExcessComposedEntries()
    {
        if(lastVisibleIndex < 0 || firstVisibleIndex < 0) return;

        int const visRange = lastVisibleIndex - firstVisibleIndex;

        // Excess entries before the visible range.
        int excess = firstVisibleIndex - visRange;
        for(int i = 0; i <= excess; ++i)
        {
            cache[i]->clear();
        }

        // Excess entries after the visible range.
        excess = lastVisibleIndex + visRange;
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
            scrollOpacity = .4f;
            scrollOpacity.setValue(.03f, 8, 3);
        }
    }

    void updateGeometry()
    {
        // While we're drawing, new entries shouldn't be added.
        DENG2_GUARD(sink);

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
        cacheEntries();

        // We should now have a cached entry for each log entry. Not all of them
        // are composed, though.
        DENG2_ASSERT(cache.size() == sink.entryCount());

        clampVisibleOffset(contentSize.y);

        // Draw in reverse, as much as we need.
        int yBottom = contentSize.y + visibleOffset;

        maxScroll = maxVisibleOffset(contentSize.y);

        firstVisibleIndex = -1;
        lastVisibleIndex = -1;

        VertexBuf::Builder verts;

        // Copy all visible entries to the buffer.
        for(int idx = sink.entryCount() - 1; yBottom > 0 && idx >= 0; --idx)
        {
            CacheEntry *entry = cache[idx];

            yBottom -= entry->height();

            if(yBottom < contentSize.y)
            {
                // This entry is visible.
                entry->make(verts, yBottom);

                if(lastVisibleIndex == -1) lastVisibleIndex = idx;
                firstVisibleIndex = idx;
            }
        }

        // Draw the scroll indicator.
        if(scrollOpacity > 0)
        {
            int const indHeight = de::clamp(
                        margin * 2,
                        int(float(contentSize.y * contentSize.y) / float(totalHeight())),
                        contentSize.y / 2);
            float const indPos = float(visibleOffset) / float(maxScroll);
            float const avail = contentSize.y - indHeight;
            for(int i = 0; i < indHeight; ++i)
            {
                VertexBuf::Builder quad;
                VertexBuf::Type v;
                v.rgba = Vector4f(1, 1, 1, scrollOpacity) * accentColor / 255.f;
                v.texCoord = entryAtlas->imageRectf(scrollTex).middle();

                Rectanglef indRect(Vector2f(contentSize.x + margin - 2*scrollBarWidth, avail - indPos * avail + indHeight),
                                   Vector2f(contentSize.x + margin - scrollBarWidth, avail - indPos * avail));

                v.pos = indRect.topLeft; quad << v;
                v.pos = indRect.topRight(); quad << v;
                v.pos = indRect.bottomLeft(); quad << v;
                v.pos = indRect.bottomRight; quad << v;

                verts += quad;
            }
        }

        buf->setVertices(gl::TriangleStrip, verts, gl::Dynamic);

        // We won't keep an unlimited number of entries in memory; delete the
        // oldest ones if limit has been reached.
        prune();
    }

    void draw()
    {
        Rectanglei pos;
        if(self.checkPlace(pos) || !bgBuf->isReady())
        {
            // Update the background quad.
            VertexBuf::Vertices verts;

            VertexBuf::Type v;
            v.rgba = Vector4f(0, 0, 0, .5f);
            v.texCoord = self.root().atlas().imageRectf(bgTex).middle();

            v.pos = pos.topLeft; verts << v;
            v.pos = pos.topRight(); verts << v;
            v.pos = pos.bottomLeft(); verts << v;
            v.pos = pos.bottomRight; verts << v;

            bgBuf->setVertices(gl::TriangleStrip, verts, gl::Static);
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

void LogWidget::glInit()
{
    d->glInit();
}

void LogWidget::glDeinit()
{
    d->glDeinit();
}
