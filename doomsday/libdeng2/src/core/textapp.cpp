/** @file textapp.h  Application with text-based/console interface.
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

#include "de/TextApp"
#include <de/Log>
#include <QDir>

namespace de {

struct TextApp::Instance
{};

TextApp::TextApp(int &argc, char **argv)
    : QCoreApplication(argc, argv),
      App(applicationFilePath(), arguments()),
      d(new Instance)
{}

TextApp::~TextApp()
{
    delete d;
}

bool TextApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QCoreApplication::notify(receiver, event);
    }
    catch(std::exception const &error)
    {
        handleUncaughtException(error.what());
    }
    catch(...)
    {
        handleUncaughtException("de::TextApp caught exception of unknown type.");
    }
    return false;
}

int TextApp::execLoop()
{
    return QCoreApplication::exec();
}

void TextApp::stopLoop(int code)
{
    return QCoreApplication::exit(code);
}

NativePath TextApp::appDataPath() const
{
    return NativePath(QDir::homePath()) / ".doomsday";
}

} // namespace de

