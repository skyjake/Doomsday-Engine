/** @file qtguiapp.h  Application based on Qt GUI widgets.
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

#include "qtguiapp.h"
#include <QMessageBox>
#include <de/LogBuffer>
#include <de/Error>
#include <de/Clock>
#include <de/Animation>

using namespace de;

DENG2_PIMPL(QtGuiApp)
{
    LogBuffer logBuffer;
    Clock clock;

    Instance(Public *i) : Base(i)
    {
        LogBuffer::setAppBuffer(logBuffer);
        Clock::setAppClock(&clock);
        Animation::setClock(&clock);
    }

    ~Instance()
    {
        Clock::setAppClock(0);
        Animation::setClock(0);
    }
};

QtGuiApp::QtGuiApp(int &argc, char **argv)
    : QApplication(argc, argv), d(new Instance(this))
{}

QtGuiApp::~QtGuiApp()
{
    delete d;
}

bool QtGuiApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch(Error const &er)
    {
        QMessageBox::critical(NULL, "Uncaught Exception", er.asText());
    }
    return false;
}
