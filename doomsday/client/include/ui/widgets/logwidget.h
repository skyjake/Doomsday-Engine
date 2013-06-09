/** @file logwidget.h  Widget for output message log.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_LOGWIDGET_H
#define DENG_CLIENT_LOGWIDGET_H

#include <QObject>
#include <de/LogSink>
#include <de/Animation>

#include "guiwidget.h"

/**
 * Widget for output message log.
 *
 * @ingroup gui
 */
class LogWidget : public GuiWidget
{
    Q_OBJECT

public:
    LogWidget(de::String const &name = "");

    /**
     * Returns the log sink that can be connected to a log buffer for receiving
     * log entries into the widget's buffer.
     */
    de::LogSink &logSink();

    /**
     * Removes all entries from the log.
     */
    void clear();

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
     * Returns the amount of space between the top edge of the widget and the
     * topmost content line.
     */
    int topMargin() const;

    /**
     * Scrolls the view to a specified position. Position 0 means the bottom of
     * the log entry buffer, while maximumScroll() is the top of the buffer
     * (the oldest entry).
     *
     * @param to  Scroll position.
     */
    void scroll(int to);

    void setContentYOffset(de::Animation const &anim);

    /**
     * Enables or disables scrolling with Page Up/Down keys.
     *
     * @param enabled  @c true to enable Page Up/Down.
     */
    void enablePageKeys(bool enabled);

    // Events.
    void viewResized();
    void update();
    void draw();
    bool handleEvent(de::Event const &event);

public slots:
    /**
     * Moves the scroll offset of the widget to the bottom of the history.
     */
    void scrollToBottom();

protected slots:
    void pruneExcessEntries();

signals:
    void scrollPositionChanged(int pos);
    void scrollMaxChanged(int maximum);
    void contentHeightIncreased(int delta);

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_LOGWIDGET_H
