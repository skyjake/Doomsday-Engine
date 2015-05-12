/** @file widgets/logwidget.h  Scrollable widget for log message history.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_LOGWIDGET_H
#define LIBAPPFW_LOGWIDGET_H

#include <QObject>
#include <de/LogSink>
#include <de/Animation>

#include "../ScrollAreaWidget"

namespace de {

/**
 * Scrollable widget for log message history.
 *
 * You must specify a log entry formatter using setLogFormatter() after creating the
 * widget. Otherwise the widget won't be able to show any entries.
 *
 * @ingroup gui
 */
class LIBAPPFW_PUBLIC LogWidget : public ScrollAreaWidget
{
    Q_OBJECT

public:
    LogWidget(String const &name = "");

    /**
     * Sets the formatter that will be used for formatting log entries for the widget.
     *
     * @param formatter  Formatter. Must exist as long as the LogWidget exists.
     */
    void setLogFormatter(LogSink::IFormatter &formatter);

    /**
     * Returns the log sink that can be connected to a log buffer for receiving
     * log entries into the widget's buffer.
     */
    LogSink &logSink();

    /**
     * Removes all entries from the log.
     */
    void clear();

    void setContentYOffset(Animation const &anim);
    Animation const &contentYOffset() const;

    // Events.
    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(Event const &event);

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

} // namespace de

#endif // LIBAPPFW_LOGWIDGET_H
