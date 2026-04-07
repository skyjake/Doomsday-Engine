/** @file scriptcommandwidget.h  Interactive Doomsday Script command line.
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

#ifndef LIBAPPFW_SCRIPTCOMMANDWIDGET_H
#define LIBAPPFW_SCRIPTCOMMANDWIDGET_H

#include "de/commandwidget.h"

namespace de {

/**
 * Interactive Doomsday Script command line.
 *
 * The widget has its own script Process in which all commands gets executed.
 * The namespace of this process persists across entered commands (but not
 * across engine shutdown).
 *
 * An entered command is not accepted until it parses successfully as a
 * Doomsday Script statement. In other words, it is possible to enter
 * multi-line scripts when there are open braces etc. If there is a true syntax
 * error in the entered script, a popup will open showing the exception from
 * the parser.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC ScriptCommandWidget : public CommandWidget
{
public:
    ScriptCommandWidget(const String &name = String());

    bool handleEvent(const Event &event);

    /**
     * Checks the native script modules and shell Lexicon for known words.
     */
    void updateCompletion();

protected:
    bool isAcceptedAsCommand(const String &text);
    void executeCommand(const String &text);
    void autoCompletionBegan(const String &prefix);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SCRIPTCOMMANDWIDGET_H
