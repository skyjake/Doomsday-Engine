/** @file persistentcanvaswindow.cpp  Canvas window with persistent state.
 * @ingroup gui
 *
 * @todo Platform-specific behavior should be encapsulated in subclasses, e.g.,
 * MacWindowBehavior. This would make the code easier to follow and more adaptable
 * to the quirks of each platform.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#if !defined (DE_MOBILE)

#include "de/persistentglwindow.h"
#include "de/guiapp.h"

#include <de/arrayvalue.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/logbuffer.h>
#include <de/numbervalue.h>

namespace de {

//static const char *MAIN_WINDOW_ID            = "main";
static const int   BREAK_CENTERING_THRESHOLD = 5;

const int PersistentGLWindow::MIN_WIDTH  = 320;
const int PersistentGLWindow::MIN_HEIGHT = 240;

static Rectanglei desktopRect()
{
    //return QApplication::desktop()->screenGeometry();

    /// @todo Multimonitor? This checks the default screen.
#if defined (DE_QT_5_6_OR_NEWER)
    return QGuiApplication::primaryScreen()->geometry();
#else
    //return QGuiApplication::screens().at(0)->geometry();
    SDL_Rect vid;
    SDL_GetDisplayBounds(0, &vid);
    return Rectanglei(vid.x, vid.y, vid.w, vid.h);
#endif
}

static Rectanglei centeredRect(const Vec2ui &size)
{
    Vec2ui const screenSize(desktopRect().size().x, desktopRect().size().y);
    const Vec2ui clamped = size.min(screenSize);

    LOGDEV_GL_XVERBOSE("centeredGeometry: Current desktop rect %i x %i",
                       screenSize.x << screenSize.y);

    return Rectanglei::fromSize(desktopRect().topLeft + Vec2i((screenSize.x - clamped.x) / 2,
                                                              (screenSize.y - clamped.y) / 2),
                                clamped);
}

static void notifyAboutModeChange()
{
    /// @todo This should be done using an observer.

    LOG_GL_NOTE("Display mode has changed");
    DE_GUI_APP->notifyDisplayModeChanged();
}

DE_PIMPL(PersistentGLWindow)
{
    /**
     * State of a window.
     */
    struct State
    {
        enum Flag
        {
            None       = 0,
            Fullscreen = 0x1,
            Centered   = 0x2,
            Maximized  = 0x4,
            FSAA       = 0x8,
            VSync      = 0x10
        };
        typedef int Flags;

        String     winId;
        Rectanglei windowRect; ///< Window geometry in windowed mode.
        Size       fullSize;   ///< Dimensions in a fullscreen mode.
        duint      colorDepthBits = 0;
        float      refreshRate    = 0.f;
        Flags      flags{None};

        State(const String &id) : winId(id)
        {}

        bool operator==(const State &other) const
        {
            return (winId          == other.winId &&
                    windowRect     == other.windowRect &&
                    fullSize       == other.fullSize &&
                    colorDepthBits == other.colorDepthBits &&
                    flags          == other.flags &&
                    fequal(refreshRate, other.refreshRate));
        }

        bool operator != (const State &other) const
        {
            return !(*this == other);
        }

        bool isCentered() const
        {
            return (flags & Centered) != 0;
        }

        bool isWindow() const
        {
            return !isFullscreen() && !isMaximized();
        }

        bool isFullscreen() const
        {
            return (flags & Fullscreen) != 0;
        }

        bool isMaximized() const
        {
            return (flags & Maximized) != 0;
        }

        bool isAntialiased() const
        {
            return (flags & FSAA) != 0;
        }

        bool isVSync() const
        {
            return (flags & VSync) != 0;
        }

        void setFlag(const Flags &f, bool set = true)
        {
            if (set)
            {
                flags |= f;

                if (f & Maximized)
                    LOGDEV_GL_VERBOSE("Setting State::Maximized");
            }
            else
            {
                flags &= ~f;

                if (f & Centered)
                    LOGDEV_GL_VERBOSE("Clearing State::Centered");
                if (f & Maximized)
                    LOGDEV_GL_VERBOSE("Clearing State::Maximized");
            }
        }

        String configName(const String &key) const
        {
            return Stringf("window.%s.%s", winId.c_str(), key.c_str());
        }

        void saveToConfig()
        {
            Config &config = App::config();

            ArrayValue *array = new ArrayValue;
            *array << NumberValue(windowRect.left())
                   << NumberValue(windowRect.top())
                   << NumberValue(windowRect.width())
                   << NumberValue(windowRect.height());
            config.set(configName("rect"), array);

            array = new ArrayValue;
            *array << NumberValue(fullSize.x)
                   << NumberValue(fullSize.y);
            config.set(configName("fullSize"), array);

            config.set(configName("center"),      isCentered());
            config.set(configName("maximize"),    isMaximized());
            config.set(configName("fullscreen"),  isFullscreen());
            config.set(configName("colorDepth"),  colorDepthBits);
            config.set(configName("refreshRate"), refreshRate);

            // FSAA and vsync are saved as part of the Config.
            //config.set(configName("fsaa"),       isAntialiased());
            //config.set(configName("vsync"),      isVSync());
        }

        void restoreFromConfig()
        {
            Config &config = App::config();

            // The default state of the window is determined by these values.
            const ArrayValue &rect = config.geta(configName("rect"));
            if (rect.size() >= 4)
            {
                windowRect = Rectanglei(rect.at(0).asInt(),
                                        rect.at(1).asInt(),
                                        rect.at(2).asInt(),
                                        rect.at(3).asInt());
            }

            const ArrayValue &fs = config.geta(configName("fullSize"));
            if (fs.size() >= 2)
            {
                fullSize = Size(fs.at(0).asInt(), fs.at(1).asInt());
            }

            colorDepthBits    = config.getui(configName("colorDepth"));
            refreshRate       = config.getf(configName("refreshRate"));
            setFlag(Centered,   config.getb(configName("center")));
            setFlag(Maximized,  config.getb(configName("maximize")));
            setFlag(Fullscreen, config.getb(configName("fullscreen")));
            setFlag(FSAA,       config.getb(configName("fsaa"), false));
            setFlag(VSync,      config.getb(configName("vsync"), true));
        }

//        /**
//         * Determines if the window will overtake the entire screen.
//         */
//        bool shouldCaptureScreen() const
//        {
//            return isFullscreen() &&
//                   !DisplayMode_IsEqual(displayMode(), DisplayMode_OriginalMode());
//        }

        /**
         * Determines the display mode that this state will use in
         * fullscreen mode.
         */
        GLWindow::DisplayMode displayMode() const
        {            
            if (isFullscreen())
            {
                return {Vec2i(fullSize.x, fullSize.y), colorDepthBits, roundi(refreshRate)};
            }
            return {};
        }

        void applyAttributes(const int *attribs)
        {
            for (int i = 0; attribs[i]; ++i)
            {
                switch (attribs[i++])
                {
                case PersistentGLWindow::Left:
                    windowRect.moveTopLeft(Vec2i(attribs[i], windowRect.topLeft.y));
                    break;

                case PersistentGLWindow::Top:
                    windowRect.moveTopLeft(Vec2i(windowRect.topLeft.x, attribs[i]));
                    break;

                case PersistentGLWindow::Width:
                    windowRect.setWidth(de::max(attribs[i], MIN_WIDTH));
                    break;

                case PersistentGLWindow::Height:
                    windowRect.setHeight(de::max(attribs[i], MIN_HEIGHT));
                    break;

                case PersistentGLWindow::Centered:
                    setFlag(State::Centered, attribs[i]);
                    break;

                case PersistentGLWindow::Maximized:
                    setFlag(State::Maximized, attribs[i]);
                    if (attribs[i]) setFlag(State::Fullscreen, false);
                    break;

                case PersistentGLWindow::Fullscreen:
                    setFlag(State::Fullscreen, attribs[i]);
                    if (attribs[i]) setFlag(State::Maximized, false);
                    break;

                case PersistentGLWindow::FullscreenWidth:
                    fullSize.x = attribs[i];
                    break;

                case PersistentGLWindow::FullscreenHeight:
                    fullSize.y = attribs[i];
                    break;

                case PersistentGLWindow::ColorDepthBits:
                    colorDepthBits = attribs[i];
                    DE_ASSERT(colorDepthBits >= 8 && colorDepthBits <= 32);
                    break;

                case PersistentGLWindow::RefreshRate:
                    refreshRate = float(de::max(0, attribs[i])) / 1000.f;
                    break;

                case PersistentGLWindow::FullSceneAntialias:
                    setFlag(State::FSAA, attribs[i]);
                    break;

                case PersistentGLWindow::VerticalSync:
                    setFlag(State::VSync, attribs[i]);
                    break;

                default:
                    // Unknown attribute.
                    DE_ASSERT_FAIL("PersistentGLWindow: unknown attribute");
                }
            }
        }

        /**
         * Checks all command line options that affect window geometry and
         * applies them to this State.
         */
        void modifyAccordingToOptions()
        {
            const CommandLine &cmdLine = App::commandLine();

            // We will compose a set of attributes based on the options.
            List<int> attribs;

            if (cmdLine.has("-nofullscreen") || cmdLine.has("-window"))
            {
                attribs << PersistentGLWindow::Fullscreen << false;
            }

            if (cmdLine.has("-fullscreen") || cmdLine.has("-nowindow"))
            {
                attribs << PersistentGLWindow::Fullscreen << true;
            }

            if (int arg = cmdLine.check("-width", 1))
            {
                attribs << PersistentGLWindow::FullscreenWidth << cmdLine.at(arg + 1).toInt();
            }

            if (int arg = cmdLine.check("-height", 1))
            {
                attribs << PersistentGLWindow::FullscreenHeight << cmdLine.at(arg + 1).toInt();
            }

            if (int arg = cmdLine.check("-winwidth", 1))
            {
                attribs << PersistentGLWindow::Width << cmdLine.at(arg + 1).toInt();
            }

            if (int arg = cmdLine.check("-winheight", 1))
            {
                attribs << PersistentGLWindow::Height << cmdLine.at(arg + 1).toInt();
            }

            if (int arg = cmdLine.check("-winsize", 2))
            {
                attribs << PersistentGLWindow::Width  << cmdLine.at(arg + 1).toInt()
                        << PersistentGLWindow::Height << cmdLine.at(arg + 2).toInt();
            }

            if (int arg = cmdLine.check("-colordepth", 1))
            {
                attribs << PersistentGLWindow::ColorDepthBits << de::clamp(8, cmdLine.at(arg+1).toInt(), 32);
            }
            if (int arg = cmdLine.check("-bpp", 1))
            {
                attribs << PersistentGLWindow::ColorDepthBits << de::clamp(8, cmdLine.at(arg+1).toInt(), 32);
            }

            if (int arg = cmdLine.check("-refreshrate", 1))
            {
                attribs << PersistentGLWindow::RefreshRate
                    << int(cmdLine.at(arg + 1).toFloat() * 1000);
            }

            if (int arg = cmdLine.check("-xpos", 1))
            {
                attribs << PersistentGLWindow::Left << cmdLine.at(arg+ 1 ).toInt()
                        << PersistentGLWindow::Centered << false
                        << PersistentGLWindow::Maximized << false;
            }

            if (int arg = cmdLine.check("-ypos", 1))
            {
                attribs << PersistentGLWindow:: Top << cmdLine.at(arg + 1).toInt()
                        << PersistentGLWindow::Centered << false
                        << PersistentGLWindow::Maximized << false;
            }

            if (cmdLine.check("-center"))
            {
                attribs << PersistentGLWindow::Centered << true;
            }

            if (cmdLine.check("-nocenter"))
            {
                attribs << PersistentGLWindow::Centered << false;
            }

            if (cmdLine.check("-maximize"))
            {
                attribs << PersistentGLWindow::Maximized << true;
            }

            if (cmdLine.check("-nomaximize"))
            {
                attribs << PersistentGLWindow::Maximized << false;
            }

            if (cmdLine.check("-nofsaa"))
            {
                attribs << PersistentGLWindow::FullSceneAntialias << false;
            }

            if (cmdLine.check("-fsaa"))
            {
                attribs << PersistentGLWindow::FullSceneAntialias << true;
            }

            if (cmdLine.check("-novsync"))
            {
                attribs << PersistentGLWindow::VerticalSync << false;
            }

            if (cmdLine.check("-vsync"))
            {
                attribs << PersistentGLWindow::VerticalSync << true;
            }

            attribs << PersistentGLWindow::End;

            applyAttributes(attribs.data());
        }
    };

    struct Task
    {
        enum Type {
            ShowNormal,
            ShowFullscreen,
            ShowMaximized,
            SetGeometry,
            NotifyModeChange,
            TrapMouse,
            MacRaiseOverShield
        };

        Type       type;
        Rectanglei rect;
        TimeSpan   delay; ///< How long to wait before doing this.

        Task(Type t, TimeSpan defer = 0.0)
            : type(t)
            , delay(std::move(defer))
        {}
        Task(const Rectanglei &r, TimeSpan defer = 0.0)
            : type(SetGeometry)
            , rect(r)
            , delay(std::move(defer))
        {}
    };

    // Window state.
    State state;
    State savedState; // used by saveState(), restoreState()
    bool neverShown;

    typedef List<Task> Tasks;
    Tasks queue;

    Impl(Public *i)
        : Base(i)
        , state(i->id())
        , savedState(i->id())
        , neverShown(true)
    {
        // Keep a global pointer to the main window.
//        if (i->id() == MAIN_WINDOW_ID)
//        {
//            DE_ASSERT(!mainExists());
//            setMain(thisPublic);
//        }

        self().setMinimumSize(Size(MIN_WIDTH, MIN_HEIGHT));
    }

    ~Impl()
    {
        self().saveToConfig();
    }

    /**
     * Parse the attributes array and check the values.
     * @param attribs  Array of attributes, terminated by Attribute::End.
     */
    bool validateAttributes(const int *attribs)
    {
        DE_ASSERT(attribs);

        for (int i = 0; attribs[i]; ++i)
        {
            switch (attribs[i++])
            {
            case Width:
            case FullscreenWidth:
                if (attribs[i] < MIN_WIDTH)
                    return false;
                break;

            case Height:
            case FullscreenHeight:
                if (attribs[i] < MIN_HEIGHT)
                    return false;
                break;

            case Fullscreen:
                // Can't go to fullscreen when downloading.
                //if (attribs[i] && Updater_IsDownloadInProgress())
                //    return false;
                break;

            case ColorDepthBits:
                if (attribs[i] < 8 || attribs[i] > 32)
                    return false; // Illegal value.
                break;

            case RefreshRate:
            case Centered:
            case Maximized:
                break;

            default:
                // Unknown attribute.
                LOGDEV_GL_WARNING("Unknown attribute %i, aborting...") << attribs[i];
                return false;
            }
        }

        // Seems ok.
        return true;
    }

    /**
     * Parse attributes and apply the values to the widget.
     *
     * @param attribs  Zero-terminated array of attributes and values.
     */
    void applyAttributes(const int *attribs)
    {
        LOG_AS("applyAttributes");

        DE_ASSERT(attribs);

        // Update the cached state from the authoritative source:
        // the widget itself.
        state = currentState();

        // The new modified state.
        State mod = state;
        mod.applyAttributes(attribs);

        LOGDEV_GL_MSG("windowRect:%s fullSize:%s depth:%i refresh:%.1f flags:%x")
                << mod.windowRect.asText()
                << mod.fullSize.asText()
                << mod.colorDepthBits
                << mod.refreshRate
                << mod.flags;

        // Apply them.
        if (mod != state)
        {
            applyToWidget(mod);
        }
        else
        {
            LOGDEV_GL_VERBOSE("New window attributes are the same as before");
        }
    }

    /**
     * Apply a State to the concrete widget instance. All properties of the widget may
     * not be updated instantly during this method. Particularly a display mode change
     * will cause geometry changes to occur later.
     *
     * @param newState  State to apply.
     */
    void applyToWidget(const State &newState)
    {
        bool trapped = self().eventHandler().isMouseTrapped();

        // If the display mode needs to change, we will have to defer the rest
        // of the state changes so that everything catches up after the change.
        TimeSpan defer = 0.0;
        const DisplayMode newMode = newState.displayMode();
//        bool modeChanged = false;

        if (!self().isVisible())
        {
            // Update geometry for windowed mode right away.
            queue << Task(newState.windowRect);
        }

        // Change display mode, if necessary.
/*        if (!newMode.isDefault() && GLWindow::getMain().fullscreenDisplayMode() != newMode)
        {
            LOG_GL_NOTE("Changing display mode to %i x %i x %i (%.1f Hz)")
                    << newMode->width << newMode->height << newMode->depth << newMode->refreshRate;

            modeChanged = DisplayMode_Change(newMode, newState.shouldCaptureScreen());
            state.colorDepthBits = newMode->depth;
            state.refreshRate    = newMode->refreshRate;

            // Wait a while after the mode change to let changes settle in.
#ifdef MACOSX
            defer = .1;
#else
            defer = .01;
#endif
        }*/

        const auto oldWinMode = self().fullscreenDisplayMode();

        self().setFullscreenDisplayMode(newMode);

        if (self().isVisible())
        {
            // Possible actions:
            //
            // Window -> Window:    Geometry
            // Window -> Max:       ShowMax
            // Window -> Full:      ShowFull
            // Window -> Mode+Full: Mode, ShowFull
            // Max -> Window:       ShowNormal, Geometry
            // Max -> Max:          -
            // Max -> Full:         ShowFull
            // Max -> Mode+Full:    Mode, ShowFull
            // Full -> Window:      ShowNormal, Geometry
            // Full -> Max:         ShowMax
            // Full -> Full:        -
            // Full -> Mode+Full:   Mode, SnowNormal, ShowFull

            if (newState.isWindow())
            {
                queue << Task(Task::ShowNormal, defer) << Task(newState.windowRect);
            }
            else
            {
//                if (modeChanged)
//                {
//                    queue << Task(Task::ShowNormal, defer);
//                    defer = 0.01;
//                }

                if (newState.isMaximized())
                {
                    queue << Task(Task::ShowMaximized, defer);
                    state.windowRect = newState.windowRect;
                }
                else if (newState.isFullscreen())
                {
                    queue << Task(Task::ShowFullscreen, defer);
                    state.windowRect = newState.windowRect;
                }
            }

            defer = 0.0;
        }

        if (oldWinMode != newMode)
        {
//#ifdef MACOSX
//            if (newState.isFullscreen())
//            {
//                queue << Task(Task::MacRaiseOverShield);
//            }
//#endif
            queue << Task(Task::NotifyModeChange, .1);
        }

        if (trapped)
        {
            queue << Task(Task::TrapMouse);
        }

        state.fullSize = newState.fullSize;
        state.flags    = newState.flags;

        if (self().isVisible())
        {
            // Carry out queued operations after dropping back to the event loop.
            Loop::timer(0.010, [this]() { performQueuedTasks(); });
        }
        else
        {
            // Not visible yet so we can do anything we want.
            checkQueue();
        }
    }

    void checkQueue()
    {
        while (!queue.isEmpty())
        {
            Task &next = queue[0];
            if (next.delay > 0.0)
            {
                Loop::timer(next.delay, [this](){ performQueuedTasks(); });
                next.delay = 0.0;
                break;
            }
            else
            {
                // Do it now.
                switch (next.type)
                {
                case Task::ShowNormal:
                    LOG_GL_VERBOSE("Showing window as normal");
                    self().showNormal();
                    break;

                case Task::ShowMaximized:
                    LOG_GL_VERBOSE("Showing window as maximized");
                    self().showMaximized();
                    break;

                case Task::ShowFullscreen:
                    LOG_GL_VERBOSE("Showing window as fullscreen");
                    self().showFullScreen();
                    break;

                case Task::SetGeometry:
                    if (state.isCentered())
                    {
                        LOG_GL_VERBOSE("Centering window with size ") << next.rect.size().asText();
                        next.rect = centeredRect(next.rect.size());
                    }
                    LOG_GL_VERBOSE("Setting window geometry to ") << next.rect.asText();
                    self().setGeometry(next.rect.left(),  next.rect.top(),
                                       next.rect.width(), next.rect.height());
                    state.windowRect = next.rect;
                    break;

                case Task::NotifyModeChange:
                    LOGDEV_GL_VERBOSE("Display mode change notification");
                    notifyAboutModeChange();
                    break;

                case Task::MacRaiseOverShield:
//#ifdef MACOSX
//                    // Pull the window again over the shield after the mode change.
//                    LOGDEV_GL_VERBOSE("Raising window over shield");
//                    DisplayMode_Native_Raise(self().nativeHandle());
//#endif
                    break;

                case Task::TrapMouse:
                    self().eventHandler().trapMouse();
                    break;
                }

                queue.takeFirst();
            }
        }

        // The queue is now empty; all modifications to state have been applied.
        DE_NOTIFY_PUBLIC(AttributeChange, i)
        {
            i->windowAttributesChanged(self());
        }
    }

    /**
     * Gets the current state of the window.
     */
    State currentState() const
    {
        State st(self().id());

        st.windowRect     = self().windowRect();
        st.fullSize       = state.fullSize;
        st.colorDepthBits = self().fullscreenDisplayMode().bitDepth;
        st.refreshRate    = self().fullscreenDisplayMode().refreshRate;

        st.flags =
                (self().isMaximized()?  State::Maximized  : State::None) |
                (self().isFullScreen()? State::Fullscreen : State::None) |
                (state.isCentered()?    State::Centered   : State::None);

        return st;
    }

    void performQueuedTasks()
    {
        checkQueue();
    }

    void windowVisibilityChanged()
    {
        if (queue.isEmpty())
        {
            state = currentState();
        }

        DE_NOTIFY_PUBLIC(AttributeChange, i)
        {
            i->windowAttributesChanged(self());
        }
    }

    DE_PIMPL_AUDIENCE(AttributeChange)
};

DE_AUDIENCE_METHOD(PersistentGLWindow, AttributeChange)

PersistentGLWindow::PersistentGLWindow(const String &id)
    : GLWindow(id)
    , d(new Impl(this))
{
    try
    {
        audienceForVisibility() += [this]() { d->windowVisibilityChanged(); };
        audienceForMove() += [this]()
        {
            if (isCentered() && !isMaximized() && !isFullScreen())
            {
                int len = int((geometry().topLeft - centeredRect(pointSize()).topLeft).length());

                if (len > BREAK_CENTERING_THRESHOLD)
                {
                    d->state.setFlag(Impl::State::Centered, false);

                    // Notify.
                    DE_NOTIFY(AttributeChange, i)
                    {
                        i->windowAttributesChanged(*this);
                    }
                }
                else
                {
                    // Recenter.
                    setGeometry(centeredRect(pointSize()));
                }
            }
        };
        restoreFromConfig();
    }
    catch (const Error &er)
    {
        LOG_WARNING("Failed to restore window state: %s") << er.asText();
    }
}

void PersistentGLWindow::saveToConfig()
{
    try
    {
        d->currentState().saveToConfig();
    }
    catch (const Error &er)
    {
        LOG_WARNING("Failed to save window state: %s") << er.asText();
    }
}

void PersistentGLWindow::restoreFromConfig()
{
    // Restore the window's state.
    d->state.restoreFromConfig();
    d->state.modifyAccordingToOptions();
    d->applyToWidget(d->state);
}

void PersistentGLWindow::saveState()
{
    d->savedState = d->currentState();
}

void PersistentGLWindow::restoreState()
{
    d->applyToWidget(d->savedState);
}

bool PersistentGLWindow::isCentered() const
{
    return d->state.isCentered();
}

Rectanglei PersistentGLWindow::windowRect() const
{
    if (d->neverShown || isFullScreen() || isMaximized())
    {
        // If the window hasn't been shown yet, or it uses a maximized/fullscreen
        // size, it doesn't have a valid normal geometry. Use the one defined in
        // the State.
        return d->state.windowRect;
    }
    return geometry();
}

GLWindow::Size PersistentGLWindow::fullscreenSize() const
{
    return d->state.fullSize;
}

int PersistentGLWindow::colorDepthBits() const
{
    return d->state.colorDepthBits;
}

float PersistentGLWindow::refreshRate() const
{
    return d->state.refreshRate;
}

void PersistentGLWindow::show(bool yes)
{
    if (yes)
    {
        if (d->state.isFullscreen())
        {
            showFullScreen();
        }
        else if (d->state.isMaximized())
        {
            showMaximized();
        }
        else
        {
            showNormal();
        }

        // Now it has been shown.
        d->neverShown = false;
    }
    else
    {
        hide();
    }
}

bool PersistentGLWindow::changeAttributes(const int *attribs)
{
    LOG_AS("PersistentGLWindow");

    if (d->validateAttributes(attribs))
    {
        d->applyAttributes(attribs);
        return true;
    }

    // These weren't good!
    return false;
}

String PersistentGLWindow::configName(const String &key) const
{
    return d->state.configName(key);
}

#if 0
void PersistentGLWindow::resizeEvent(QResizeEvent *ev)
{
    GLWindow::resizeEvent(ev);

    LOGDEV_GL_XVERBOSE("Window resized: maximized:%b old:%ix%i new:%ix%i",
                       isMaximized() << ev->oldSize().width() << ev->oldSize().height()
                       << ev->size().width() << ev->size().height());

    /*
    if (!isMaximized() && !isFullScreen())
    {
        d->state.windowRect.setSize(Vec2i(ev->size().width(), ev->size().height()));
    }*/
}
#endif

} // namespace de

#endif
