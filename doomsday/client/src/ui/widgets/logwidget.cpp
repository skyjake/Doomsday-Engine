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

#include <QImage>
#include <QPainter>

using namespace de;

DENG2_PIMPL(LogWidget)
{
    typedef GLBufferT<Vertex2TexRgba> VertexBuf;

    // Log entries.
    MemoryLogSink sink;
    StyledLogSinkFormatter formatter;
    int maxEntries;

    /// Cache of drawable entries.
    struct CacheEntry
    {
        int y;
        FontLineWrapping wraps;
        GLTextComposer composer;

        CacheEntry(Font const &font, Atlas &atlas) : y(0)
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

        void wrap(String const &text, int width)
        {
            wraps.wrapTextToWidth(text, width);
            composer.setText(wraps.text());
            composer.setWrapping(wraps);
        }

        void make(GLTextComposer::Vertices &verts)
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

    // Style.
    Font const *font;
    int margin;

    // GL objects.
    VertexBuf *buf;
    VertexBuf *bgBuf;
    AtlasTexture *entryAtlas;
    Drawable drawable;
    GLUniform uMvpMatrix;
    GLUniform uTex;
    GLUniform uColor;
    GLUniform uBgMvpMatrix;
    GLUniform uBgTex;
    Matrix4f projMatrix;
    Matrix4f viewMatrix;
    Id bgTex;

    Instance(Public *i)
        : Base(i),
          maxEntries(1000),
          cacheWidth(0),
          visibleOffset(0),
          maxScroll(0),
          lastMaxScroll(0),
          firstVisibleIndex(-1),
          lastVisibleIndex(-1),
          font(0),
          buf(0),
          entryAtlas(0),
          uMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uTex      ("uTex",       GLUniform::Sampler2D),
          uColor    ("uColor",     GLUniform::Vec4),
          uBgMvpMatrix("uMvpMatrix", GLUniform::Mat4),
          uBgTex    ("uTex",       GLUniform::Sampler2D)
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
        font = &self.style().fonts().font("log.normal");
        margin = self.style().rules().rule("gap").valuei();
    }

    void glInit()
    {
        entryAtlas = AtlasTexture::newWithRowAllocator(
                Atlas::BackingStore | Atlas::AllowDefragment,
                GLTexture::maximumSize().min(Atlas::Size(4096, 2048)));

        bgTex = self.root().atlas().alloc(Image::solidColor(Image::Color(255, 255, 255, 255),
                                                            Image::Size(1, 1)));
        uBgTex = self.root().atlas();

        uTex = entryAtlas;
        uColor = Vector4f(1, 1, 1, 1);

        drawable.addBufferWithNewProgram(bgBuf = new VertexBuf, "bg");
        self.root().shaders().build(drawable.program("bg"), "generic.tex_color")
                << uBgMvpMatrix
                << uBgTex;

        // Vertex buffer for the log entries.
        drawable.addBuffer(buf = new VertexBuf);
        self.root().shaders().build(drawable.program(), "generic.tex_color")
                << uMvpMatrix
                //<< uColor
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
        // Remove old entries if there are too many.
        int excess = sink.entryCount() - maxEntries;
        if(excess > 0)
        {
            sink.remove(0, excess);
        }
        while(excess-- > 0 && !cache.isEmpty())
        {
            delete cache.takeFirst();

            firstVisibleIndex--;
            lastVisibleIndex--;
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
            CacheEntry *cached = new CacheEntry(*font, *entryAtlas);
            cached->wrap(styledTextForEntry(idx), contentWidth());
            if(idx > 0)
            {
                CacheEntry *previous = cache[idx - 1];
                cached->y = previous->y + previous->height();
            }
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

        // We don't need to keep all entries ready for drawing immediately.

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

    void drawEntries()
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

        // Scrolling is done using the matrix.
        maxScroll = maxVisibleOffset(contentSize.y);
        uMvpMatrix = projMatrix * Matrix4f::translate(
                    Vector2f(pos.topLeft + Vector2i(margin, visibleOffset - maxScroll)));

        firstVisibleIndex = -1;
        lastVisibleIndex = -1;

        // Copy all visible entries to the buffer.
        VertexBuf::Vertices verts;
        for(int idx = sink.entryCount() - 1; yBottom > 0 && idx >= 0; --idx)
        {
            CacheEntry *entry = cache[idx];

            yBottom -= entry->height();

            if(yBottom < contentSize.y)
            {
                // This entry is visible.
                entry->make(verts);

                if(lastVisibleIndex == -1) lastVisibleIndex = idx;
                firstVisibleIndex = idx;
            }
        }

        buf->setVertices(gl::TriangleStrip, verts, gl::Dynamic);

        // Draw the scroll indicator.

        /*
        if(d->showScrollIndicator && d->visibleOffset > 0)
        {
            int const indHeight = de::clamp(2, de::floor(float(buf.height() * buf.height()) /
                                                   float(d->totalHeight())), buf.height() / 2);
            float const indPos = float(d->visibleOffset) / float(maxScroll);
            int const avail = buf.height() - indHeight;
            for(int i = 0; i < indHeight; ++i)
            {
                buf.put(Vector2i(buf.width() - 1, i + avail - indPos * avail),
                        TextCanvas::Char(':', TextCanvas::Char::Reverse));
            }
        }
        targetCanvas().draw(buf, pos.topLeft);
        */

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
            v.rgba = Vector4f(0, 0, 0, .8f);
            v.texCoord = self.root().atlas().imageRectf(bgTex).middle();

            v.pos = pos.topLeft; verts << v;
            v.pos = pos.topRight(); verts << v;
            v.pos = pos.bottomLeft(); verts << v;
            v.pos = pos.bottomRight; verts << v;

            bgBuf->setVertices(gl::TriangleStrip, verts, gl::Static);
        }

        GLState &st = GLState::push();
        st.setScissor(pos);

        drawEntries();
        drawable.draw();

        GLState::pop();

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
    rule().setInput(Rule::Width, Const(400)); // TODO -- from rule defs
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
        return true;

    case DDKEY_PGDN:
        d->setVisibleOffset(de::max(0, int(d->visibleOffset.target()) - pageSize));
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
