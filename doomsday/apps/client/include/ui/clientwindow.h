/** @file clientwindow.h  Top-level window with UI widgets.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef CLIENT_CLIENTWINDOW_H
#define CLIENT_CLIENTWINDOW_H

#include <de/PersistentGLWindow>
#include <de/BaseWindow>
#include <de/NotificationAreaWidget>
#include <de/FadeToBlackWidget>

#include "ui/clientrootwidget.h"
#include "resource/image.h"
#include "ui/widgets/gamewidget.h"

#undef main

#define DE_GAMEVIEW_X         ClientWindow::main().game().rule().left().valuei()
#define DE_GAMEVIEW_Y         ClientWindow::main().game().rule().top().valuei()
#define DE_GAMEVIEW_WIDTH     ClientWindow::main().game().rule().width().valuei()
#define DE_GAMEVIEW_HEIGHT    ClientWindow::main().game().rule().height().valuei()

/**
 * A helpful macro that changes the origin of the window space coordinate system.
 */
#define FLIP(y)             (ClientWindow::main().height() - ((y)+1))

class ConsoleWidget;
class TaskBarWidget;
class BusyWidget;
class AlertDialog;
class HomeWidget;

/**
 * Top-level window that contains UI widgets. @ingroup gui
 */
class ClientWindow : public de::BaseWindow
{
public:
    enum Mode { Normal, Busy };
    enum SidebarLocation { RightEdge };
    enum FadeDirection { FadeFromBlack, FadeToBlack };

public:
    ClientWindow(de::String const &id = "main");

    ClientRootWidget &root();
    TaskBarWidget &   taskBar();
    de::GuiWidget &   taskBarBlur();
    ConsoleWidget &   console();
    HomeWidget &      home();
    GameWidget &      game();
    BusyWidget &      busy();
    AlertDialog &     alerts();
    de::NotificationAreaWidget &notifications();

    /**
     * Adds a widget to the widget tree so that it will be displayed over
     * other widgets.
     *
     * @param widget  Widget to add on top of others. Ownership of the
     *                widget taken by the new parent.
     */
    void addOnTop(de::GuiWidget *widget);

    /**
     * Installs a sidebar widget into the window. If there is an existing
     * sidebar, it will be deleted. Sidebar widgets are expected to control
     * their own width (on the right/left edges) or height (on the top/bottom
     * edges).
     *
     * @param location  Location to attach the sidebar. Window takes ownership
     *                  of the widget.
     * @param sidebar   Widget to install, or @c NULL to remove the sidebar.
     */
    void setSidebar(SidebarLocation location, de::GuiWidget *sidebar);

    void unsetSidebar(SidebarLocation location) { setSidebar(location, 0); }

    bool hasSidebar(SidebarLocation location = RightEdge) const;

    de::GuiWidget &sidebar(SidebarLocation location = RightEdge) const;

    /**
     * Sets the operating mode of the window. In Busy mode, the normal
     * widgets of the window will be replaced with a single BusyWidget.
     *
     * @param mode  Mode.
     */
    void setMode(Mode const &mode);

    /**
     * Minimizes or restores the game to full size. While minimized, the Home is
     * moved partially onscreen to be accessible while the game is still running and
     * loaded.
     *
     * @param minimize  @c true to minimize game, @c false to restore.
     */
    void setGameMinimized(bool minimize);

    bool isGameMinimized() const;

    /**
     * Must be called before any canvas windows are created. Defines the
     * default OpenGL format settings for the contained canvases.
     *
     * @return @c true, if the new format was applied. @c false, if the new
     * format remains the same because none of the settings have changed.
     */
    static bool setDefaultGLFormat();

    /**
     * Grab the contents of the window into the supplied @a image. Ownership of
     * the image passes to the window for the duration of this call.
     *
     * @param image      Image to fill with the grabbed frame contents.
     * @param halfSized  If @c true, scales the image to half the full size.
     */
    void grab(image_t &image, bool halfSized = false) const;

    void fadeContent(FadeDirection fadeDirection, de::TimeSpan const &duration);

    de::FadeToBlackWidget *contentFade();

    void fadeInTaskBarBlur(de::TimeSpan span);
    void fadeOutTaskBarBlur(de::TimeSpan span);
    void toggleFPSCounter();
    void showColorAdjustments();
    void hideTaskBarBlur();

    void updateRootSize();

    // Notifications.
    bool isFPSCounterVisible() const;

    // Implements BaseWindow.
    de::Vec2f windowContentSize() const override;
    void drawWindowContent() override;
    void preDraw() override;
    void postDraw() override;
    bool handleFallbackEvent(de::Event const &event) override;

    static ClientWindow &main();
    static bool mainExists();

protected:
    void windowAboutToClose() override;

private:
    DE_PRIVATE(d)
};

#endif // CANVASWINDOW_H
