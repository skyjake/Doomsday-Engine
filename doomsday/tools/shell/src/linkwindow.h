/** @file linkwindow.h  Window for a server link.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LINKWINDOW_H
#define LINKWINDOW_H

#include <de/basewindow.h>
#include <de/guirootwidget.h>
#include <de/nativepath.h>
#include <de/string.h>
#include <doomsday/network/link.h>

/**
 * Window for a server link.
 */
class LinkWindow : public de::BaseWindow
{
public:
    LinkWindow(const de::String &id);

    de::GuiRootWidget &root();

    de::Vec2f windowContentSize() const override;
    void      drawWindowContent() override;

    void setTitle(const de::String &title);

    bool isConnected() const;

    // Qt events.
//    void changeEvent(QEvent *);
//    void closeEvent(QCloseEvent *);

//signals:
//    void linkOpened(LinkWindow *window);
//    void linkClosed(LinkWindow *window);
//    void closed(LinkWindow *window);

    void openConnection(const de::String &address);
    void waitForLocalConnection(de::duint16 localPort, const de::NativePath &errorLogPath, const de::String &name);
    void openConnection(network::Link *link, const de::String &name = {});
    void closeConnection();
    void sendCommandToServer(const de::String& command);
    void sendCommandsToServer(const de::StringList &commands);
    void switchToStatus();
    void switchToOptions();
    void switchToConsole();
    void stopServer();

protected:
    void handleIncomingPackets();
    void addressResolved();
    void connected();
    void disconnected();
    void askForPassword();

    void windowAboutToClose() override;

private:
    DE_PRIVATE(d)
};

#endif // LINKWINDOW_H
