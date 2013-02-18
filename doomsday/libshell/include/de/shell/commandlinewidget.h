/** @file commandlinewidget.h  Widget for command line input.
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

#ifndef LIBSHELL_COMMANDLINEWIDGET_H
#define LIBSHELL_COMMANDLINEWIDGET_H

#include "LineEditWidget"

namespace de {
namespace shell {

/**
 * Text editor with a history.
 */
class LIBSHELL_PUBLIC CommandLineWidget : public LineEditWidget
{
    Q_OBJECT

public:
    CommandLineWidget(String const &name = "");
    virtual ~CommandLineWidget();

    bool handleEvent(Event const &event);

signals:
    void commandEntered(de::String command);

private:
    DENG2_PRIVATE(d)
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_COMMANDLINEWIDGET_H
