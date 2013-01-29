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

#include "logwidget.h"
#include <de/MonospaceLogSinkFormatter>
#include <de/Lockable>
#include <de/LogBuffer>
#include <de/shell/TextRootWidget>
#include <QList>

using namespace de;
using namespace de::shell;

/**
 * Log sink for incoming entries (local and remote). Rather than formatting the
 * entries immediately, we keep a copy of them for formatting prior to drawing.
 */
class Sink : public LogSink, public Lockable
{
public:
    Sink(LogWidget &widget) : LogSink(), _widget(widget)
    {}

    ~Sink()
    {
        foreach(LogEntry *e, _entries) delete e;
    }

    LogSink &operator << (LogEntry const &entry)
    {
        DENG2_GUARD(this);

        _entries.append(new LogEntry(entry));
        _widget.root().requestDraw();
        return *this;
    }

    LogSink &operator << (String const &plainText)
    {
        DENG2_UNUSED(plainText);
        return *this;
    }

    void flush() {}

    int entryCount() const
    {
        return _entries.size();
    }

    LogEntry const &entry(int index) const
    {
        return *_entries[index];
    }

private:
    LogWidget &_widget;
    QList<LogEntry *> _entries;
};

struct LogWidget::Instance
{
    Sink sink;
    MonospaceLogSinkFormatter formatter;
    int cacheWidth;
    QList<TextCanvas *> cache; ///< Indices match entry indices in sink.

    Instance(LogWidget &inst) : sink(inst), cacheWidth(0)
    {}

    ~Instance()
    {
        clearCache();
    }

    void clearCache()
    {
        foreach(TextCanvas *cv, cache) delete cv;
        cache.clear();
    }
};

LogWidget::LogWidget(String const &name) : TextWidget(name), d(new Instance(*this))
{
}

LogWidget::~LogWidget()
{
    delete d;
}

LogSink &LogWidget::logSink()
{
    return d->sink;
}

void LogWidget::draw()
{
    if(!targetCanvas()) return;

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

        TextCanvas *buf = new TextCanvas(Vector2i(pos.width(), lines.size()));
        d->cache.append(buf);

        // Draw the text.
        for(int i = 0; i < lines.size(); ++i)
        {
            buf->drawText(Vector2i(0, i), lines[i]);
        }
    }

    // Draw in reverse, as much as we need.
    int yBottom = pos.size().y;

    for(int idx = d->sink.entryCount() - 1; yBottom > 0 && idx >= 0; --idx)
    {
        TextCanvas *canvas = d->cache[idx];
        yBottom -= canvas->size().y;
        buf.draw(*canvas, Vector2i(0, yBottom));
    }

    d->sink.unlock();

    targetCanvas()->draw(buf, pos.topLeft);
}

void LogWidget::redraw()
{
    drawAndShow();
}
