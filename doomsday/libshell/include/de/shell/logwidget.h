/** @file logwidget.h  Widget for output message log.
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

#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <de/shell/TextWidget>
#include <de/LogSink>

namespace de {
namespace shell {

class LIBSHELL_PUBLIC LogWidget : public TextWidget
{
    Q_OBJECT

public:
    LogWidget(String const &name = "");

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
     * is emitted whenever the maximum changed.
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
    bool handleEvent(Event const &event);

public slots:
    /**
     * Moves the scroll offset of the widget to the bottom of the history.
     */
    void scrollToBottom();

signals:
    void scrollPositionChanged(int pos);
    void scrollMaxChanged(int maximum);

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LOGWIDGET_H
