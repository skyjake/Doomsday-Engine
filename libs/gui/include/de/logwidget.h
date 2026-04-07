/** @file widgets/logwidget.h  Scrollable widget for log message history.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/logsink.h>
#include <de/animation.h>

#include "de/scrollareawidget.h"

namespace de {

/**
 * Scrollable widget for log message history.
 *
 * You must specify a log entry formatter using setLogFormatter() after creating the
 * widget. Otherwise the widget won't be able to show any entries.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC LogWidget : public ScrollAreaWidget
{
public:
    DE_AUDIENCE(ContentHeight, void contentHeightIncreased(LogWidget &, int delta))

public:
    LogWidget(const String &name = {});

    /**
     * Sets the formatter that will be used for formatting log entries for the widget.
     *
     * @param formatter  Formatter. Must exist as long as the LogWidget exists.
     */
    void setLogFormatter(LogSink::IFormatter &formatter);

    /**
     * Enables the showing of privileged messages.
     *
     * @param onlyPrivileged  Only show privileged entries.
     */
    void setPrivilegedEntries(bool onlyPrivileged);

    /**
     * Returns the log sink that can be connected to a log buffer for receiving
     * log entries into the widget's buffer.
     */
    LogSink &logSink();

    /**
     * Removes all entries from the log.
     */
    void clear();

    void setContentYOffset(const Animation &anim);
    const Animation &contentYOffset() const;

    // Events.
    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(const Event &event);

protected:
    void glInit();
    void glDeinit();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_LOGWIDGET_H
