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

#include "de/shell/LogWidget"
#include "de/shell/KeyEvent"
#include "de/shell/TextRootWidget"
#include <de/MonospaceLogSinkFormatter>
#include <de/MemoryLogSink>
#include <de/LogBuffer>
#include <QList>

namespace de {
namespace shell {

/**
 * Log sink for incoming entries (local and remote). Rather than formatting the
 * entries immediately, we keep a copy of them for formatting prior to drawing.
 */
class Sink : public MemoryLogSink
{
public:
    Sink(LogWidget &widget) : MemoryLogSink(), _widget(widget) {}

    void addedNewEntry(LogEntry &)
    {
        _widget.root().requestDraw();
    }

private:
    LogWidget &_widget;
};

DENG2_PIMPL(LogWidget)
{
    Sink sink;
    MonospaceLogSinkFormatter formatter;
    unsigned int cacheWidth;
    QList<TextCanvas *> cache; ///< Indices match entry indices in sink.
    int maxEntries;
    int visibleOffset;
    bool showScrollIndicator;
    int lastMaxScroll;

    Instance(Public *inst)
        : Base(inst),
          sink(*inst),
          cacheWidth(0),
          maxEntries(1000),
          visibleOffset(0),
          showScrollIndicator(true),
          lastMaxScroll(0)
    {}

    ~Instance()
    {
        clearCache();
    }

    void clear()
    {
        sink.clear();
        clearCache();
    }

    void clearCache()
    {
        qDeleteAll(cache);
        cache.clear();
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
        }
    }

    int totalHeight()
    {
        int total = 0;
        for(int idx = sink.entryCount() - 1; idx >= 0; --idx)
        {
            total += cache[idx]->size().y;
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
        setVisibleOffset(de::min(visibleOffset, maxVisibleOffset(visibleHeight)));
    }

    void setVisibleOffset(int off)
    {
        if(visibleOffset != off)
        {
            visibleOffset = off;
            emit self.scrollPositionChanged(off);
        }
    }
};

LogWidget::LogWidget(String const &name) : TextWidget(name), d(new Instance(this))
{}

LogSink &LogWidget::logSink()
{
    return d->sink;
}

void LogWidget::clear()
{
    d->clear();
    redraw();
}

void LogWidget::setScrollIndicatorVisible(bool visible)
{
    d->showScrollIndicator = visible;
}

int LogWidget::scrollPosition() const
{
    return d->visibleOffset;
}

int LogWidget::scrollPageSize() const
{
    return de::max(1, de::floor(rule().height().value()) - 1);
}

int LogWidget::maximumScroll() const
{
    return d->lastMaxScroll;
}

void LogWidget::scroll(int to)
{
    d->visibleOffset = de::max(0, to);
    redraw();
}

void LogWidget::draw()
{
    Rectanglei pos = rule().recti();
    TextCanvas buf(pos.size());

    if(d->cacheWidth != pos.width())
    {
        d->cacheWidth = pos.width();
        d->formatter.setMaxLength(d->cacheWidth);

        // Width has changed, zap the cache.
        d->clearCache();
    }

    // While we're drawing, new entries shouldn't be added.
    d->sink.lock();

    // Cache entries we don't yet have. We'll do this in normal order so that
    // the formatter gets them chronologically.
    while(d->cache.size() < d->sink.entryCount())
    {
        int idx = d->cache.size();

        // No cached entry for this, generate one.
        LogEntry const &entry = d->sink.entry(idx);
        QList<String> lines = d->formatter.logEntryToTextLines(entry);

        TextCanvas *buf = new TextCanvas(Vector2ui(pos.width(), lines.size()));
        d->cache.append(buf);

        TextCanvas::Char::Attribs attribs = (entry.flags() & LogEntry::Remote?
                TextCanvas::Char::DefaultAttributes : TextCanvas::Char::Bold);

        // Draw the text.
        for(int i = 0; i < lines.size(); ++i)
        {
            buf->drawText(Vector2i(0, i), lines[i], attribs);
        }

        // Adjust visible offset.
        if(d->visibleOffset > 0)
        {
            d->setVisibleOffset(d->visibleOffset + lines.size());
        }
    }

    DENG2_ASSERT(d->cache.size() == d->sink.entryCount());

    d->clampVisibleOffset(buf.height());

    // Draw in reverse, as much as we need.
    int yBottom = buf.height() + d->visibleOffset;

    for(int idx = d->sink.entryCount() - 1; yBottom > 0 && idx >= 0; --idx)
    {
        TextCanvas *canvas = d->cache[idx];
        yBottom -= canvas->size().y;
        if(yBottom < buf.height())
        {
            buf.draw(*canvas, Vector2i(0, yBottom));
        }
    }

    // Draw the scroll indicator.
    int const maxScroll = d->maxVisibleOffset(buf.height());
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

    d->prune();
    d->sink.unlock();

    // Notify now that we know what the max scroll is.
    if(d->lastMaxScroll != maxScroll)
    {
        d->lastMaxScroll = maxScroll;
        emit scrollMaxChanged(maxScroll);
    }
}

bool LogWidget::handleEvent(Event const &event)
{
    if(event.type() != Event::KeyPress) return false;

    KeyEvent const &ev = static_cast<KeyEvent const &>(event);

    int pageSize = scrollPageSize();

    switch(ev.key())
    {
    case Qt::Key_PageUp:
        d->setVisibleOffset(d->visibleOffset + pageSize);
        redraw();
        return true;

    case Qt::Key_PageDown:
        d->setVisibleOffset(de::max(0, d->visibleOffset - pageSize));
        redraw();
        return true;

    default:
        break;
    }

    return TextWidget::handleEvent(event);
}

void LogWidget::scrollToBottom()
{
    d->setVisibleOffset(0);
    redraw();
}

} // namespace shell
} // namespace de
