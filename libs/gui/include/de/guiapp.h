/** @file guiapp.h  Application with GUI support.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GUIAPP_H
#define LIBGUI_GUIAPP_H

#include "de/libgui.h"
#include <de/app.h>
#include <de/guiloop.h>

/**
 * Macro for conveniently accessing the de::GuiApp singleton instance.
 */
#define DE_GUI_APP   (static_cast<de::GuiApp *>(DE_APP))

#define DE_ASSERT_IN_RENDER_THREAD()   DE_ASSERT(de::GuiApp::inRenderThread())

namespace de {

class NativePath;
class Rule;
class Thread;
class WindowSystem;

/**
 * Application with GUI support.
 *
 * The event loop is protected against uncaught exceptions. Catches the
 * exception and shuts down the app cleanly.
 *
 * @ingroup gui
 */
class LIBGUI_PUBLIC GuiApp : public App
{
public:
    enum MouseCursor { None, Arrow, ResizeHorizontal, ResizeVertical };

    /// Emitted when the display mode has changed.
    DE_AUDIENCE(DisplayModeChange, void displayModeChanged())

public:
    GuiApp(const StringList &args);

    void initSubsystems(SubsystemInitFlags subsystemInitFlags = DefaultSubsystems) override;

    /**
     * The ratio of pixels per point. For example, this is 2.0 if there are two pixels per point.
     * This value is affected by the UI scaling factor setting.
     */
    const Rule &pixelRatio() const;
    
    /**
     * Pixel ratio of the window. This is not affected by UI scaling.
     */
    float devicePixelRatio() const;

    /**
     * Sets a new pixel ratio. This replaces the initial automatically detected pixel ratio.
     *
     * @param pixelRatio  Pixel ratio.
     */
    void setPixelRatio(float pixelRatio);

    void setMetadata(const String &orgName,
                     const String &orgDomain,
                     const String &appName,
                     const String &appVersion);

    int exec(const std::function<void()> &startup = {});

    void quit(int code = 0);

    GuiLoop &loop();

    void setMouseCursor(MouseCursor cursor);

    /**
     * Emits the displayModeChanged() signal.
     *
     * @todo In the future when de::GuiApp (or a sub-object owned by it) is
     * responsible for display modes, this should be handled internally and
     * not via this public interface where anybody can call it.
     */
    void notifyDisplayModeChanged();

    /**
     * Called upon receiving a Quit event from the operating system.
     */
    virtual void quitRequested();

public:
    static WindowSystem &windowSystem();
    static bool hasWindowSystem();

    /**
     * Determines if the currently executing thread is the rendering thread.
     * This may be the same thread as the main thread.
     */
    static bool inRenderThread();

    /**
     * Marks the current thread as the rendering thread.
     */
    static void setRenderThread();

    /**
     * Shows a native file or folder in the operating system's file manager (macOS Finder
     * or Explorer in Windows).
     */
    static void revealFile(const NativePath &fileOrFolder);

    /**
     * Opens an URL in the operating system's default web browser.
     *
     * @param url  URL to open.
     */
    static void openBrowserUrl(const String &url);

protected:
    NativePath appDataPath() const override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GUIAPP_H
