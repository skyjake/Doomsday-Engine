/** @file guiapp.cpp  Application with GUI support.
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

#include "de/guiapp.h"
#include "de/opengl.h"
#include "de/builtinfont.h"
#include "de/imagefile.h"
#include "de/glwindow.h"

#include <de/commandline.h>
#include <de/config.h>
#include <de/constantrule.h>
#include <de/eventloop.h>
#include <de/filesystem.h>
#include <de/log.h>
#include <de/nativefont.h>
#include <de/nativepath.h>
#include <de/dscript.h>
#include <de/thread.h>
#include <de/windowsystem.h>

#include <SDL.h>
#undef main

#include <fstream>
#include <the_Foundation/thread.h>

namespace de {

DE_STATIC_STRING(VAR_UI_SCALE_FACTOR, "ui.scaleFactor");
    
static Value *Function_DisplayMode_OriginalMode(Context &, const Function::ArgumentValues &)
{
    SDL_DisplayMode mode;
    SDL_GetDesktopDisplayMode(GLWindow::mainExists() ? GLWindow::getMain().displayIndex() : 0,
                              &mode);
    const auto dim = de::ratio({mode.w, mode.h});

    auto *dict = new DictionaryValue;
    dict->add(new TextValue("width"),       new NumberValue(mode.w));
    dict->add(new TextValue("height"),      new NumberValue(mode.h));
    dict->add(new TextValue("depth"),       new NumberValue(SDL_BITSPERPIXEL(mode.format)));
    dict->add(new TextValue("refreshRate"), new NumberValue(mode.refresh_rate));

    auto *ratio = new ArrayValue;
    *ratio << NumberValue(dim.x) << NumberValue(dim.y);
    dict->add(new TextValue("ratio"), ratio);

    return dict;
}

DE_PIMPL(GuiApp)
, DE_OBSERVES(Variable, Change)
{
    Binder        binder;
    EventLoop     eventLoop;
    GuiLoop       loop;
    thrd_t        renderThread{};
    float         windowPixelRatio = 1.0f; ///< Without user's Config.ui.scaleConfig
    ConstantRule *pixelRatio       = new ConstantRule;
    SDL_Cursor *  arrowCursor;
    SDL_Cursor *  vsizeCursor;
    SDL_Cursor *  hsizeCursor;

    std::unique_ptr<WindowSystem> windowSystem;

    Impl(Public *i) : Base(i)
    {
        // The default render thread is the main thread.
        renderThread = thrd_current();

        arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        vsizeCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
        hsizeCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);

        // Script bindings.
        {
            binder.initNew() << DE_FUNC_NOARG(DisplayMode_OriginalMode, "originalMode");
            self().scriptSystem().addNativeModule("DisplayMode", binder.module());
            binder.module().addNumber("PIXEL_RATIO", 1.0);
        }

        // Built-in font for immediate use.
        Font::load("Builtin-Regular", sourceSansProRegular());
    }

    ~Impl() override
    {
        windowSystem->closeAll();
        windowSystem.reset();
        releaseRef(pixelRatio);
        SDL_FreeCursor(arrowCursor);
        SDL_FreeCursor(vsizeCursor);
        SDL_FreeCursor(hsizeCursor);
        SDL_Quit();
    }

    void variableValueChanged(Variable &, const Value &) override
    {
        self().setPixelRatio(windowPixelRatio);
    }

    void determineDevicePixelRatio()
    {
#if 0 //defined (WIN32)
        // Use the Direct2D API to find out the desktop DPI factor.
        ID2D1Factory *d2dFactory = nullptr;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        if (SUCCEEDED(hr))
        {
            FLOAT dpiX = 96;
            FLOAT dpiY = 96;
            d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
            windowPixelRatio = dpiX / 96.0;
            d2dFactory->Release();
            d2dFactory = nullptr;
        }
#else
/*        // Use a hidden SDL window to determine pixel ratio.
        SDL_Window *temp =
            SDL_CreateWindow("",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             100,
                             100,
                             SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
        int points, pixels;
        SDL_GetWindowSize(temp, &points, nullptr);
        SDL_GL_GetDrawableSize(temp, &pixels, nullptr);
        SDL_DestroyWindow(temp);

        windowPixelRatio = float(pixels) / float(points);
*/
#endif
    }

    DE_PIMPL_AUDIENCE(DisplayModeChange)
};
DE_AUDIENCE_METHOD(GuiApp, DisplayModeChange)

GuiApp::GuiApp(const StringList &args)
    : App(args)
    , d(new Impl(this))
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
                          SDL_INIT_GAMECONTROLLER) != 0)
    {
        throw Error("GuiApp::GuiApp", stringf("Failed to initialize SDL: %s", SDL_GetError()));
    }
    const int displayCount = SDL_GetNumVideoDisplays();
    if (displayCount == 0)
    {
        throw Error("GuiApp::GuiApp", "No video displays available");
    }
    setLocale_Foundation();

    // List available display modes.
    for (int i = 0; i < displayCount; ++i)
    {
        LOG_GL_MSG("Display %d:") << i;
        for (const auto &mode : GLWindow::displayModes(i))
        {
            LOG_GL_MSG("  %d x %d @ %d Hz (%d-bit)")
                << mode.resolution.x << mode.resolution.y << mode.refreshRate << mode.bitDepth;
        }
    }

    d->determineDevicePixelRatio();
    NativeFont::setPixelRatio(d->windowPixelRatio);

    static ImageFile::Interpreter intrpImageFile;
    fileSystem().addInterpreter(intrpImageFile);

    // Core packages for GUI functionality.
    addInitPackage("net.dengine.stdlib.gui");
}

void GuiApp::initSubsystems(SubsystemInitFlags subsystemInitFlags)
{
    App::initSubsystems(subsystemInitFlags); // reads Config

    if (!Config::get().has(VAR_UI_SCALE_FACTOR()))
    {
        Config::get().set(VAR_UI_SCALE_FACTOR(), 1.0f);
    }
    
    // The "-dpi" option overrides the detected pixel ratio.
    if (auto dpi = commandLine().check("-dpi", 1))
    {
        d->windowPixelRatio = dpi.params.at(0).toFloat();
    }
    setPixelRatio(d->windowPixelRatio);

    Config::get(VAR_UI_SCALE_FACTOR()).audienceForChange() += d;

    d->windowSystem.reset(new WindowSystem);
    addSystem(*d->windowSystem);
}

const Rule &GuiApp::pixelRatio() const
{
    return *d->pixelRatio;
}
    
float GuiApp::devicePixelRatio() const
{
    return d->windowPixelRatio;
}

void GuiApp::setPixelRatio(float pixelRatio)
{
    d->windowPixelRatio = pixelRatio;

    // Apply the overall UI scale factor.
    pixelRatio *= config().getf(VAR_UI_SCALE_FACTOR(), 1.0f);

    NativeFont::setPixelRatio(pixelRatio);

    if (!fequal(d->pixelRatio->value(), pixelRatio))
    {
        LOG_VERBOSE("Pixel ratio changed to %.1f") << pixelRatio;

        d->pixelRatio->set(pixelRatio);
        scriptSystem()["DisplayMode"].set("PIXEL_RATIO", Value::Number(pixelRatio));
    }
}

void GuiApp::setMetadata(const String &orgName,
                         const String &orgDomain,
                         const String &appName,
                         const String &appVersion)
{
    Record &amd = metadata();

    amd.set(APP_NAME,    appName);
    amd.set(APP_VERSION, appVersion);
    amd.set(ORG_NAME,    orgName);
    amd.set(ORG_DOMAIN,  orgDomain);
}

void GuiApp::notifyDisplayModeChanged()
{
    DE_NOTIFY(DisplayModeChange, i)
    {
        i->displayModeChanged();
    }
}

int GuiApp::exec(const std::function<void ()> &startup)
{
    LOGDEV_NOTE("Starting GuiApp event loop...");

    int code = d->eventLoop.exec([this, startup](){
        d->loop.start();
        if (startup) startup();
    });

    LOGDEV_NOTE("GuiApp event loop exited with code %i") << code;
    return code;
}

void GuiApp::quit(int code)
{
    LOGDEV_MSG("Stopping GuiApp event loop");

    d->loop.stop();
    d->eventLoop.quit(code);
}

GuiLoop &GuiApp::loop()
{
    return d->loop;
}

void GuiApp::setMouseCursor(MouseCursor cursor)
{
    SDL_ShowCursor(cursor != None ? SDL_ENABLE : SDL_DISABLE);

    switch (cursor)
    {
    case None:             break;
    case Arrow:            SDL_SetCursor(d->arrowCursor); break;
    case ResizeHorizontal: SDL_SetCursor(d->hsizeCursor); break;
    case ResizeVertical:   SDL_SetCursor(d->vsizeCursor); break;
    }
}

bool GuiApp::inRenderThread()
{
    if (!App::appExists())
    {
        return false;
    }
    return thrd_current() == DE_GUI_APP->d->renderThread;
}

void GuiApp::setRenderThread()
{
    DE_GUI_APP->d->renderThread = thrd_current();
}

WindowSystem &GuiApp::windowSystem()
{
    DE_ASSERT(DE_GUI_APP->d->windowSystem);
    return *DE_GUI_APP->d->windowSystem;
}

bool GuiApp::hasWindowSystem()
{
    return bool(DE_GUI_APP->d->windowSystem);
}

NativePath GuiApp::appDataPath() const
{
    const auto &amd = metadata();
    #if defined (DE_WINDOWS)
    {
        return NativePath::homePath() / "AppData/Local" / amd.gets(ORG_NAME) / amd.gets(APP_NAME);
    }
    #elif defined (MACOSX)
    {
        return NativePath::homePath() / "Library/Application Support" / amd.gets(APP_NAME);
    }
    #else
    {
        return NativePath::homePath() / amd.gets(UNIX_HOME);
    }
    #endif
}

void GuiApp::revealFile(const NativePath &fileOrFolder) // static
{
    #if defined (MACOSX)
    {
        using namespace std;

        const NativePath scriptPath = cachePath() / "reveal-path.scpt";
        if (ofstream f{scriptPath.toStdString()})
        {
            // Apple Script to select a specific file.
            f << "on run argv" << endl
              << "  tell application \"Finder\"" << endl
              << "    activate" << endl
              << "    reveal POSIX file (item 1 of argv) as text" << endl
              << "  end tell" << endl
              << "end run" << endl;
            f.close();

            CommandLine{{"/usr/bin/osascript", scriptPath, fileOrFolder}}.execute();
        }
    }
    #elif defined (DE_X11)
    {
        String path = (fileOrFolder.isDirectory() ? fileOrFolder.toString()
                                                  : fileOrFolder.fileNamePath().toString());
        CommandLine({"/usr/bin/xdg-open", path}).execute();
    }
    #else
    {
        DE_ASSERT_FAIL("File revealing not implemented on this platform");
    }
    #endif
}

void GuiApp::openBrowserUrl(const String &url)
{
    #if defined (DE_X11)
    {
        CommandLine({"/usr/bin/x-www-browser", url}).execute();
    }
    #elif defined (MACOSX)
    {
        CommandLine({"/usr/bin/open", url}).execute();
    }
    #else
    {
        DE_ASSERT_FAIL("Browser URL opening not implemented on this platform");
    }
    #endif
}

void GuiApp::quitRequested()
{
    quit(0);
}

} // namespace de
