/** @file logwidget.h  Scrollable widget for log message history.
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

#include "scrollareawidget.h"

/**
 * Scrollable widget for log message history.
 *
 * @ingroup gui
 */
class LogWidget : public ScrollAreaWidget
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

    void setContentYOffset(de::Animation const &anim);
    de::Animation const &contentYOffset() const;

    // Events.
    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(de::Event const &event);

protected slots:
    void pruneExcessEntries();

signals:
    //void scrollPositionChanged(int pos);
    //void scrollMaxChanged(int maximum);
    void contentHeightIncreased(int delta);

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_LOGWIDGET_H
