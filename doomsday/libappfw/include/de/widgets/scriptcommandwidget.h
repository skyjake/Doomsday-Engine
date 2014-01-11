/** @file scriptwidget.h  Interactive Doomsday Script command line.
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

#ifndef LIBAPPFW_SCRIPTCOMMANDWIDGET_H
#define LIBAPPFW_SCRIPTCOMMANDWIDGET_H

#include "../CommandWidget"

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
 * @ingroup gui
 */
class LIBAPPFW_PUBLIC ScriptCommandWidget : public CommandWidget
{
public:
    ScriptCommandWidget(String const &name = "");

    bool handleEvent(Event const &event);

protected:
    bool isAcceptedAsCommand(String const &text);
    void executeCommand(String const &text);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_SCRIPTCOMMANDWIDGET_H
