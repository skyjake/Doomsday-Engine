/** @file shell/logwidget.h  Text widget for log message output.
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

#ifndef LIBSHELL_LOGTEDGET_H
#define LIBSHELL_LOGTEDGET_H

#include "widget.h"
#include "../logsink.h"

namespace de { namespace term {

/**
 * Text-based log output widget.
 *
 * @ingroup textUi
 */
class DE_PUBLIC LogWidget : public Widget
{
public:
    LogWidget(const String &name = {});

    /**
     * Returns the log sink that can be connected to a log buffer for receiving
     * log entries into the widget's buffer.
     */
    LogSink &logSink();

    /**
     * Removes all entries from the log.
     */
    void clear();

    void setScrollIndicatorVisible(bool visible);

    /**
     * Returns the current scroll position, with 0 being the bottom of the
     * history (present time) and maximumScroll() being the top of the history
     * (most distant past).
     */
    int scrollPosition() const;

    int scrollPageSize() const;

    /**
     * Returns the maximum scroll position. The scrollMaxChanged() signal
     * is emitted whenever the maximum changes.
     */
    int maximumScroll() const;

    /**
     * Scrolls the view to a specified position. Position 0 means the bottom of
     * the log entry buffer, while maximumScroll() is the top of the buffer
     * (the oldest entry).
     *
     * @param to  Scroll position.
     */
    void scroll(int to);

    void draw();
    bool handleEvent(const Event &event);

    /**
     * Moves the scroll offset of the widget to the bottom of the history.
     */
    void scrollToBottom();

    DE_AUDIENCE(Scroll, void scrollPositionChanged(int pos))
    DE_AUDIENCE(Maximum, void scrollMaxChanged(int maximum))

private:
    DE_PRIVATE(d)
};

}} // namespace de::term

#endif // LIBSHELL_LOGTEDGET_H
