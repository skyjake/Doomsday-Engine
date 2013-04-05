/** @file persistentcanvaswindow.cpp  Canvas window with persistent state.
 * @ingroup gui
 *
 * @todo Platform-specific behavior should be encapsulated in subclasses, e.g.,
 * MacWindowBehavior. This would make the code easier to follow and more adaptable
 * to the quirks of each platform.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "de/PersistentCanvasWindow"
#include "de/GuiApp"
#include "de/DisplayMode"
#include <de/ArrayValue>
#include <de/NumberValue>
#include <QDesktopWidget>
#include <QTimer>
#include <QVector>
#include <QList>

namespace de {

static String const MAIN_WINDOW_ID = "main";

int const PersistentCanvasWindow::MIN_WIDTH  = 320;
int const PersistentCanvasWindow::MIN_HEIGHT = 240;

static QRect desktopRect()
{
    /// @todo Multimonitor? This checks the default screen.
    return QApplication::desktop()->screenGeometry();
}

static Rectanglei centeredRect(Vector2i size)
{
    QSize screenSize = desktopRect().size();

    LOG_DEBUG("centeredGeometry: Current desktop rect %i x %i")
            << screenSize.width() << screenSize.height();

    QRect rect(desktopRect().topLeft() +
               QPoint((screenSize.width()  - size.x) / 2,
                      (screenSize.height() - size.y) / 2),
               QSize(size.x, size.y));

    return Rectanglei(rect.left(), rect.top(), rect.width(), rect.height());
}

static PersistentCanvasWindow *mainWindow = 0;

static void notifyAboutModeChange()
{
    /// @todo This should be done using an observer.

    LOG_MSG("Display mode has changed.");
    DENG2_GUI_APP->notifyDisplayModeChanged();
}

DENG2_PIMPL(PersistentCanvasWindow)
{
    /**
     * Logical state of a window.
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

        String winId;
        Rectanglei windowRect;  ///< Window geometry in windowed mode.
        Size fullSize;          ///< Dimensions in a fullscreen mode.
        int colorDepthBits;
        Flags flags;

        State(String const &id)
            : winId(id), colorDepthBits(0), flags(None)
        {}

        bool operator == (State const &other) const
        {
            return (winId          == other.winId &&
                    windowRect     == other.windowRect &&
                    fullSize       == other.fullSize &&
                    colorDepthBits == other.colorDepthBits &&
                    flags          == other.flags);
        }

        bool operator != (State const &other) const
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

        void setFlag(Flags const &f, bool set = true)
        {
            if(set)
            {
                flags |= f;

                if(f & Maximized)
                    LOG_DEBUG("Setting State::Maximized");
            }
            else
            {
                flags &= ~f;

                if(f & Centered)
                    LOG_DEBUG("Clearing State::Centered");
                if(f & Maximized)
                    LOG_DEBUG("Clearing State::Maximized");
            }
        }

        QString configName(String const &key) const
        {
            return QString("window.$1.$2").arg(winId).arg(key);
        }

        void saveToConfig()
        {
            Config &config = App::config();

            ArrayValue *array = new ArrayValue;
            *array << NumberValue(windowRect.left())
                   << NumberValue(windowRect.top())
                   << NumberValue(windowRect.width())
                   << NumberValue(windowRect.height());
            config.names()[configName("rect")] = array;

            array = new ArrayValue;
            *array << NumberValue(fullSize.x)
                   << NumberValue(fullSize.y);
            config.names()[configName("fullSize")] = array;

            config.names()[configName("center")]     = new NumberValue(isCentered());
            config.names()[configName("maximize")]   = new NumberValue(isMaximized());
            config.names()[configName("fullscreen")] = new NumberValue(isFullscreen());
            config.names()[configName("colorDepth")] = new NumberValue(colorDepthBits);
            config.names()[configName("fsaa")]       = new NumberValue(isAntialiased());
            config.names()[configName("vsync")]      = new NumberValue(isVSync());
        }

        void restoreFromConfig()
        {
            Config &config = App::config();

            // The default state of the window is determined by these values.
            ArrayValue &rect = config.geta(configName("rect"));
            if(rect.size() >= 4)
            {
                windowRect = Rectanglei(rect.at(0).asNumber(),
                                        rect.at(1).asNumber(),
                                        rect.at(2).asNumber(),
                                        rect.at(3).asNumber());
            }

            ArrayValue &fs = config.geta(configName("fullSize"));
            if(fs.size() >= 2)
            {
                fullSize = Vector2i(fs.at(0).asNumber(), fs.at(1).asNumber());
            }

            colorDepthBits =    config.geti(configName("colorDepth"));
            setFlag(Centered,   config.getb(configName("center")));
            setFlag(Maximized,  config.getb(configName("maximize")));
            setFlag(Fullscreen, config.getb(configName("fullscreen")));
            setFlag(FSAA,       config.getb(configName("fsaa")));
            setFlag(VSync,      config.getb(configName("vsync")));
        }

        /**
         * Determines if the window will overtake the entire screen.
         */
        bool shouldCaptureScreen() const
        {
            return isFullscreen() &&
                   !DisplayMode_IsEqual(displayMode(), DisplayMode_OriginalMode());
        }

        /**
         * Determines the display mode that this state will use in
         * fullscreen mode.
         */
        DisplayMode const *displayMode() const
        {
            if(isFullscreen())
            {
                return DisplayMode_FindClosest(fullSize.x, fullSize.y, colorDepthBits, 0);
            }
            return DisplayMode_OriginalMode();
        }
    };

    struct Task
    {
        enum Type
        {
            ShowNormal,
            ShowFullscreen,
            ShowMaximized,
            SetGeometry,
            NotifyModeChange,
            MacRaiseOverShield
        };

        Type type;
        Rectanglei rect;
        TimeDelta delay; ///< How long to wait before doing this.

        Task(Type t, TimeDelta defer = 0)
            : type(t), delay(defer) {}
        Task(Rectanglei const &r, TimeDelta defer = 0)
            : type(SetGeometry), rect(r), delay(defer) {}
    };

    String id;

    // Logical state.
    State state;

    typedef QList<Task> Tasks;
    Tasks queue;

    Instance(Public *i, String const &windowId)
        : Base(i),
          id(windowId),
          state(windowId)
    {
        // Keep a global pointer to the main window.
        if(id == MAIN_WINDOW_ID)
        {
            DENG2_ASSERT(mainWindow == 0);
            mainWindow = thisPublic;
        }
    }

    ~Instance()
    {
        self.saveToConfig();

        if(id == MAIN_WINDOW_ID)
        {
            mainWindow = 0;
        }
    }

    /**
     * Parse the attributes array and check the values.
     * @param attribs  Array of attributes, terminated by Attribute::End.
     */
    bool validateAttributes(int const *attribs)
    {
        DENG2_ASSERT(attribs);

        for(int i = 0; attribs[i]; ++i)
        {
            switch(attribs[i++])
            {
            case Width:
            case FullscreenWidth:
                if(attribs[i] < MIN_WIDTH)
                    return false;
                break;

            case Height:
            case FullscreenHeight:
                if(attribs[i] < MIN_HEIGHT)
                    return false;
                break;

            /*case Fullscreen:
                // Can't go to fullscreen when downloading.
                if(attribs[i] && Updater_IsDownloadInProgress())
                    return false;
                break;*/

            case ColorDepthBits:
                if(attribs[i] < 8 || attribs[i] > 32)
                    return false; // Illegal value.
                break;

            default:
                // Unknown attribute.
                LOG_WARNING("Unknown attribute %i, aborting...") << attribs[i];
                return false;
            }
        }

        // Seems ok.
        return true;
    }

    /**
     * Checks all command line options that affect window geometry and applies
     * them to this Window.
     */
    void modifyAccordingToOptions()
    {
        CommandLine const &cmdLine = DENG2_APP->commandLine();

        // We will compose a set of attributes based on the options.
        QVector<int> attribs;

        if(cmdLine.has("-nofullscreen") || cmdLine.has("-window"))
        {
            attribs << Fullscreen << false;
        }

        if(cmdLine.has("-fullscreen") || cmdLine.has("-nowindow"))
        {
            attribs << Fullscreen << true;
        }

        if(int arg = cmdLine.check("-width", 1))
        {
            attribs << FullscreenWidth << cmdLine.at(arg + 1).toInt();
        }

        if(int arg = cmdLine.check("-height", 1))
        {
            attribs << FullscreenHeight << cmdLine.at(arg + 1).toInt();
        }

        if(int arg = cmdLine.check("-winwidth", 1))
        {
            attribs << Width << cmdLine.at(arg + 1).toInt();
        }

        if(int arg = cmdLine.check("-winheight", 1))
        {
            attribs << Height << cmdLine.at(arg + 1).toInt();
        }

        if(int arg = cmdLine.check("-winsize", 2))
        {
            attribs << Width  << cmdLine.at(arg + 1).toInt()
                    << Height << cmdLine.at(arg + 2).toInt();
        }

        if(int arg = cmdLine.check("-colordepth", 1))
        {
            attribs << ColorDepthBits << de::clamp(8, cmdLine.at(arg+1).toInt(), 32);
        }
        if(int arg = cmdLine.check("-bpp", 1))
        {
            attribs << ColorDepthBits << de::clamp(8, cmdLine.at(arg+1).toInt(), 32);
        }

        if(int arg = cmdLine.check("-xpos", 1))
        {
            attribs << Left << cmdLine.at(arg+ 1 ).toInt()
                    << Centered << false
                    << Maximized << false;
        }

        if(int arg = cmdLine.check("-ypos", 1))
        {
            attribs << Top << cmdLine.at(arg + 1).toInt()
                    << Centered << false
                    << Maximized << false;
        }

        if(cmdLine.check("-center"))
        {
            attribs << Centered << true;
        }

        if(cmdLine.check("-nocenter"))
        {
            attribs << Centered << false;
        }

        if(cmdLine.check("-maximize"))
        {
            attribs << Maximized << true;
        }

        if(cmdLine.check("-nomaximize"))
        {
            attribs << Maximized << false;
        }

        if(cmdLine.check("-nofsaa"))
        {
            attribs << FullSceneAntialias << false;
        }

        if(cmdLine.check("-fsaa"))
        {
            attribs << FullSceneAntialias << true;
        }

        if(cmdLine.check("-novsync"))
        {
            attribs << VerticalSync << false;
        }

        if(cmdLine.check("-vsync"))
        {
            attribs << VerticalSync << true;
        }

        attribs << End;

        applyAttributes(attribs.constData());
    }

    void applyAttributes(int const *attribs)
    {
        LOG_AS("applyAttributes");

        // Parse the attributes array and check the values.
        DENG2_ASSERT(attribs);

        // The new modified state.
        State mod = state;

        for(int i = 0; attribs[i]; ++i)
        {
            switch(attribs[i++])
            {
            case Left:
                mod.windowRect.moveTopLeft(Vector2i(attribs[i], mod.windowRect.topLeft.y));
                break;

            case Top:
                mod.windowRect.moveTopLeft(Vector2i(mod.windowRect.topLeft.x, attribs[i]));
                break;

            case Width:
                DENG2_ASSERT(attribs[i] >= MIN_WIDTH);
                mod.windowRect.setWidth(attribs[i]);
                break;

            case Height:
                DENG2_ASSERT(attribs[i] >= MIN_HEIGHT);
                mod.windowRect.setHeight(attribs[i]);
                break;

            case Centered:
                mod.setFlag(State::Centered, attribs[i]);
                break;

            case Maximized:
                mod.setFlag(State::Maximized, attribs[i]);
                break;

            case Fullscreen:
                mod.setFlag(State::Fullscreen, attribs[i]);
                break;

            case FullscreenWidth:
                mod.fullSize.x = attribs[i];
                break;

            case FullscreenHeight:
                mod.fullSize.y = attribs[i];
                break;

            case ColorDepthBits:
                mod.colorDepthBits = attribs[i];
                DENG2_ASSERT(mod.colorDepthBits >= 8 && mod.colorDepthBits <= 32);
                break;

            case FullSceneAntialias:
                mod.setFlag(State::FSAA, attribs[i]);
                break;

            case VerticalSync:
                mod.setFlag(State::VSync, attribs[i]);
                break;

            default:
                // Unknown attribute.
                DENG2_ASSERT(false);
            }
        }

        // Apply them.
        if(mod != state)
        {
            applyToWidget(mod);
        }
        else
        {
            LOG_VERBOSE("New window attributes are the same as before");
        }
    }

    void applyToWidget(State const &newState)
    {
        // If the display mode needs to change, we will have to defer the rest
        // of the state changes.
        TimeDelta defer = 0;
        DisplayMode const *newMode = newState.displayMode();
        bool modeChanged = false;

        if(!DisplayMode_IsEqual(DisplayMode_Current(), newMode))
        {
            modeChanged = DisplayMode_Change(newMode, newState.shouldCaptureScreen());
            state.colorDepthBits = newMode->depth;
#ifdef MACOSX
            defer = .1;
#else
            defer = .01;
#endif
        }

        if(self.isVisible())
        {
//#ifndef MACOSX
#if 1
            if(state.isFullscreen() && newState.isFullscreen() && modeChanged)
            {
                // Switch back to normal and then again to fullscreen.
                queue << Task(Task::ShowNormal, defer);
                defer = 0;
            }
#endif
            // Geometry needs to be applied in a deferred way.
            queue << Task(newState.isFullscreen()? Task::ShowFullscreen :
                          newState.isMaximized()?  Task::ShowMaximized :
                                                   Task::ShowNormal, defer);
        }
        if(newState.isWindow())
        {
            queue << Task(newState.windowRect);
        }

        if(modeChanged)
        {
            if(newState.isFullscreen())
            {
                queue << Task(Task::MacRaiseOverShield);
            }
            queue << Task(Task::NotifyModeChange, .1);
        }

        state.fullSize = newState.fullSize;
        state.flags    = newState.flags;

        checkQueue();
    }

    void checkQueue()
    {
        while(!queue.isEmpty())
        {
            Task &next = queue[0];
            if(next.delay > 0.0)
            {
                QTimer::singleShot(next.delay.asMilliSeconds(), thisPublic, SLOT(performQueuedTasks()));
                next.delay = 0;
                break;
            }
            else
            {
                // Do it now.
                switch(next.type)
                {
                case Task::ShowNormal:
                    LOG_DEBUG("Showing window as normal");
                    self.showNormal();
                    break;

                case Task::ShowMaximized:
                    LOG_DEBUG("Showing window as maximized");
                    self.showMaximized();
                    break;

                case Task::ShowFullscreen:
                    LOG_DEBUG("Showing window as fullscreen");
                    self.showFullScreen();
                    break;

                case Task::SetGeometry:
                    if(state.isCentered())
                    {
                        LOG_DEBUG("Centering window with size ") << next.rect.size().asText();
                        next.rect = centeredRect(next.rect.size());
                    }
                    LOG_DEBUG("Setting window geometry to ") << next.rect.asText();
                    self.setGeometry(next.rect.left(),  next.rect.top(),
                                     next.rect.width(), next.rect.height());
                    break;

                case Task::NotifyModeChange:
                    LOG_DEBUG("Display mode change notification");
                    notifyAboutModeChange();
                    break;

                case Task::MacRaiseOverShield:
#ifdef MACOSX
                    // Pull the window again over the shield after the mode change.
                    DisplayMode_Native_Raise(self.nativeHandle());
#endif
                    break;
                }

                queue.takeFirst();
            }
        }
    }

    /**
     * Gets the current state of the Qt widget.
     */
    State widgetState() const
    {
        State st(id);

        st.windowRect     = self.windowRect();
        st.fullSize       = state.fullSize;
        st.colorDepthBits = DisplayMode_Current()->depth;

        st.flags =
                (self.isMaximized()?  State::Maximized  : State::None) |
                (self.isFullScreen()? State::Fullscreen : State::None) |
                (state.isCentered()?  State::Centered   : State::None);

        return st;
    }
};

PersistentCanvasWindow::PersistentCanvasWindow(String const &id)
    : d(new Instance(this, id))
{
    restoreFromConfig();
}

void PersistentCanvasWindow::saveToConfig()
{
    d->widgetState().saveToConfig();
}

void PersistentCanvasWindow::restoreFromConfig()
{
    // Restore the window's state.
    d->state.restoreFromConfig();
    d->applyToWidget(d->state);
}

bool PersistentCanvasWindow::isCentered() const
{
    return d->state.isCentered();
}

Rectanglei PersistentCanvasWindow::windowRect() const
{
    QRect geom = normalGeometry();
    return Rectanglei(geom.left(), geom.top(), geom.width(), geom.height());
}

CanvasWindow::Size PersistentCanvasWindow::fullscreenSize() const
{
    return d->state.fullSize;
}

int PersistentCanvasWindow::colorDepthBits() const
{
    return d->state.colorDepthBits;
}

void PersistentCanvasWindow::show(bool yes)
{
    if(yes)
    {
        if(d->state.isFullscreen())
        {
            showFullScreen();
        }
        else if(d->state.isMaximized())
        {
            showMaximized();
        }
        else
        {
            showNormal();
        }
    }
    else
    {
        hide();
    }
}

bool PersistentCanvasWindow::changeAttributes(int const *attribs)
{
    LOG_AS("PersistentCanvasWindow");

    if(d->validateAttributes(attribs))
    {
        d->applyAttributes(attribs);
        return true;
    }

    // These weren't good!
    return false;
}

void PersistentCanvasWindow::performQueuedTasks()
{
    d->checkQueue();
}

PersistentCanvasWindow &PersistentCanvasWindow::main()
{
    DENG2_ASSERT(mainWindow != 0);
    if(mainWindow == 0)
    {
        throw InvalidIdError("PersistentCanvasWindow::main",
                             "No window found with id \"" + MAIN_WINDOW_ID + "\"");
    }
    return *mainWindow;
}

} // namespace de

#if 0

using namespace de;


uint mainWindowIdx;

#ifdef MACOSX
static const int WAIT_MILLISECS_AFTER_MODE_CHANGE = 100; // ms
#else
static const int WAIT_MILLISECS_AFTER_MODE_CHANGE = 10; // ms
#endif

DENG2_PIMPL(Window)
{
    /// Saved geometry for detecting when changes have occurred.
    QRect appliedGeometry;

    bool needShowFullscreen;
    bool needReshowFullscreen;
    bool needShowNormal;
    bool needRecreateCanvas;
    bool needWait;
    bool willUpdateWindowState;

    Instance(Public *i)
        : Base(i),
          widget(new CanvasWindow),
          drawFunc(0)
    {
        // Minimum possible size when resizing.
        widget->setMinimumSize(QSize(Window::MIN_WIDTH, Window::MIN_HEIGHT));

        widget->setCloseFunc(windowIsClosing);
        widget->setMoveFunc(windowWasMoved);
    }

    bool inline isBeingAdjusted() const {
        return needShowFullscreen || needReshowFullscreen || needShowNormal || needRecreateCanvas || needWait;
    }

    inline bool isFlagged(int flag) const {
        return (flags & flag) != 0;
    }

    bool applyDisplayMode()
    {
        DENG2_ASSERT(widget);

        if(!DisplayMode_Count()) return true; // No modes to change to.

        if(flags & WF_FULLSCREEN)
        {
            DisplayMode const *mode =
                DisplayMode_FindClosest(geometry.width(), geometry.height(), colorDepthBits, 0);

            if(mode && DisplayMode_Change(mode, true /* fullscreen: capture */))
            {
                geometry.setSize(QSize(DisplayMode_Current()->width,
                                       DisplayMode_Current()->height));
#if defined MACOSX
                // Pull the window again over the shield after the mode change.
                DisplayMode_Native_Raise(self.nativeHandle());
#endif
                self.trapMouse(true);
                return true;
            }
        }
        else
        {
            return DisplayMode_Change(DisplayMode_OriginalMode(),
                                      false /* windowed: don't capture */);
        }
        return false;
    }

    /**
     * Applies the information stored in the Window instance to the actual
     * widget geometry. Centering is applied in this stage (it only affects the
     * widget's geometry).
     */
    void applyGeometry()
    {
        LOG_AS("applyGeometry");

        DENG2_ASSERT(widget);

        // While we're adjusting the window, the window move/resizing callbacks
        // should've mess with the geometry values.
        needWait = true;
        LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE * 20, endWindowWait);

        bool modeChanged = applyDisplayMode();
        if(modeChanged)
        {
            // Others might be interested to hear about the mode change.
            LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, notifyAboutModeChange);
        }

        /*
         * The following is a bit convoluted. The core idea is this, though: on
         * some platforms, changes to the window's mode (normal, maximized,
         * fullscreen/frameless) do not occur immediately. Instead, control
         * needs to return to the event loop and the native window events need
         * to play out. Thus some of the operations have to be performed in a
         * deferred way, after a short wait. The ideal would be to listen to
         * the native events and trigger the necessary updates after they
         * occur; however, now we just use naive time-based delays.
         */

        if(flags & WF_FULLSCREEN)
        {
            LOG_DEBUG("fullscreen mode (mode changed? %b)") << modeChanged;

            if(!modeChanged) return; // We don't need to do anything.

            if(widget->isVisible())
            {
                needShowFullscreen = !widget->isFullScreen();

#if defined(WIN32) || defined(Q_OS_LINUX)
                if(widget->isFullScreen())
                {
                    needShowFullscreen = false;
                    needReshowFullscreen = true;
                }
#endif
                LOG_DEBUG("widget is visible, need showFS:%b reshowFS:%b")
                        << needShowFullscreen << needReshowFullscreen;

#ifdef MACOSX
                // Kludge! See updateMainWindowLayout().
                appliedGeometry = QRect(0, 0,
                                        DisplayMode_Current()->width,
                                        DisplayMode_Current()->height);
#endif

                // The window is already visible, so let's allow a mode change to
                // resolve itself before we go changing the window.
                LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, updateMainWindowLayout);
            }
            else
            {
                LOG_DEBUG("widget is not visible, setting geometry to %s")
                        << Vector2i(DisplayMode_Current()->width,
                                    DisplayMode_Current()->height).asText();

                widget->setGeometry(0, 0, DisplayMode_Current()->width, DisplayMode_Current()->height);
            }
        }
        else
        {
            // The window is in windowed mode (frames and window decoration visible).
            // We will restore it to its previous position and size.
            QRect geom = normalGeometry; // Previously stored normal geometry.

            if(flags & WF_CENTERED)
            {
                // We'll center the window.
                geom = centeredRect(normalGeometry.size());
            }

            if(flags & WF_MAXIMIZED)
            {
                // When a window is maximized, we'll let the native WM handle the sizing.
                if(widget->isVisible())
                {
                    LOG_DEBUG("now maximized.");
                    widget->showMaximized();
                }

                // Non-visible windows will be shown later
                // (as maximized, if the flag is set).
            }
            else
            {
                // The window is in normal mode: not maximized or fullscreen.

                // If the window is already visible, changes to it need to be
                // deferred so that the native counterpart can be updated, too
                if(widget->isVisible() && (modeChanged || widget->isMaximized()))
                {
                    if(modeChanged)
                    {
                        // We'll wait before the mode change takes full effect.
                        needShowNormal = true;
                        LegacyCore_Timer(WAIT_MILLISECS_AFTER_MODE_CHANGE, updateMainWindowLayout);
                    }
                    else
                    {
                        // Display mode was not changed, so we can immediately
                        // change the window state.
                        widget->showNormal();
                    }
                }

                appliedGeometry = geom;

                if(widget->isVisible())
                {
                    // The native window may not be ready to receive the updated
                    // geometry (e.g., window decoration not made visible yet).
                    // We'll apply the geometry after a delay.
                    LegacyCore_Timer(50 + WAIT_MILLISECS_AFTER_MODE_CHANGE, useAppliedGeometryForWindows);
                }
                else
                {
                    // The native window is not visible yet, so we can apply any
                    // number of changes we like.
                    widget->setGeometry(geom);
                }
            }
        }
    }

    /**
     * Retrieves the actual widget geometry and updates the information stored
     * in the Window instance.
     */
    void fetchGeometry()
    {
        DENG2_ASSERT(widget);

        setFlag(WF_MAXIMIZED, widget->isMaximized());

        geometry = widget->geometry();

        // If the window is presently maximized or fullscreen, we will not
        // store the actual coordinates.
        if(!widget->isMaximized() && !(flags & WF_FULLSCREEN) && !isBeingAdjusted())
        {
            normalGeometry = widget->geometry();
            DEBUG_Message(("ngw=%i [A]\n", normalGeometry.width()));
        }

        LOG_DEBUG("Current window geometry: %i,%i %s (max:%b)")
                << geometry.x() << geometry.y()
                << Vector2i(geometry.width(), geometry.height()).asText()
                << ((flags & WF_MAXIMIZED) != 0);
        LOG_DEBUG("Normal window geometry: %i,%i %s")
                << normalGeometry.x() << normalGeometry.y()
                << Vector2i(normalGeometry.width(), normalGeometry.height()).asText();
    }

    void applyAttributes(int const *attribs)
    {
        LOG_AS("applyAttributes");

        bool changed = false;

        // Parse the attributes array and check the values.
        DENG2_ASSERT(attribs);
        for(int i = 0; attribs[i]; ++i)
        {
            switch(attribs[i++])
            {
            case Window::X:
                if(geometry.x() != attribs[i])
                {
                    normalGeometry.setX(attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Y:
                if(geometry.y() != attribs[i])
                {
                    normalGeometry.setY(attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Width:
                if(geometry.width() != attribs[i])
                {
                    DENG2_ASSERT(attribs[i] >= Window::MIN_WIDTH);
                    geometry.setWidth(attribs[i]);
                    normalGeometry.setWidth(attribs[i]);
                    DEBUG_Message(("ngw=%i [B]\n", normalGeometry.width()));
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Height:
                if(geometry.height() != attribs[i])
                {
                    DENG2_ASSERT(attribs[i] >= Window::MIN_HEIGHT);
                    geometry.setHeight(attribs[i]);
                    normalGeometry.setHeight(attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Centered:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_CENTERED)))
                {
                    setFlag(WF_CENTERED, attribs[i]);
                    changed = true;
                }
                break;
            case Window::Maximized:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_MAXIMIZED)))
                {
                    setFlag(WF_MAXIMIZED, attribs[i]);
                    changed = true;
                }
                break;
            case Window::Fullscreen:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_FULLSCREEN)))
                {
                    DENG2_ASSERT(!(attribs[i] && Updater_IsDownloadInProgress()));
                    setFlag(WF_FULLSCREEN, attribs[i]);
                    setFlag(WF_MAXIMIZED, false);
                    changed = true;
                }
                break;
            case Window::Visible:
                if(IS_NONZERO(attribs[i]) != IS_NONZERO(isFlagged(WF_VISIBLE)))
                {
                    setFlag(WF_VISIBLE, attribs[i]);
                    changed = true;
                }
                break;
            case Window::ColorDepthBits:
                qDebug() << attribs[i] << colorDepthBits;
                if(attribs[i] != colorDepthBits)
                {
                    colorDepthBits = attribs[i];
                    DENG2_ASSERT(colorDepthBits >= 8 && colorDepthBits <= 32);
                    changed = true;
                }
                break;
            default:
                // Unknown attribute.
                DENG2_ASSERT(false);
            }
        }

        // No change?
        if(!changed)
        {
            VERBOSE(Con_Message("New window attributes same as before."));
            return;
        }

        // Apply them.
        applyGeometry();
    }

    void updateLayout()
    {
        setFlag(WF_MAXIMIZED, widget->isMaximized());

        geometry.setSize(widget->size());

        if(!(flags & WF_FULLSCREEN))
        {
            LOG_DEBUG("Updating current view geometry for window, fetched %s")
                    << Vector2i(geometry.width(), geometry.height()).asText();

            if(!(flags & WF_MAXIMIZED) && !isBeingAdjusted())
            {
                // Update the normal-mode geometry (not fullscreen, not maximized).
                normalGeometry.setSize(geometry.size());

                LOG_DEBUG("Updating normal view geometry for window, fetched %s")
                        << Vector2i(normalGeometry.width(), normalGeometry.height()).asText();
            }
        }
        else
        {
            LOG_DEBUG("Updating view geometry for fullscreen %s")
                    << Vector2i(geometry.width(), geometry.height()).asText();
        }
    }

    static void updateMainWindowLayout()
    {
        Window *wnd = Window::mainPtr();

        if(wnd->d->needReshowFullscreen)
        {
            LOG_DEBUG("Main window re-set to fullscreen mode.");
            wnd->d->needReshowFullscreen = false;
            wnd->d->widget->showNormal();
            wnd->d->widget->showFullScreen();
        }

        if(wnd->d->needShowFullscreen)
        {
            LOG_DEBUG("Main window to fullscreen mode.");
            wnd->d->needShowFullscreen = false;
            wnd->d->widget->showFullScreen();
        }

        if(wnd->d->flags & WF_FULLSCREEN)
        {
#if defined MACOSX
            // For some interesting reason, we have to scale the window twice
            // in fullscreen mode or the resulting layout won't be correct.
            wnd->d->widget->setGeometry(QRect(0, 0, 320, 240));
            wnd->d->widget->setGeometry(wnd->d->appliedGeometry);

            DisplayMode_Native_Raise(wnd->nativeHandle());
#endif
            wnd->trapMouse();
        }

        if(wnd->d->needShowNormal)
        {
            LOG_DEBUG("Main window to normal mode (center:%b).") << ((wnd->d->flags & WF_CENTERED) != 0);
            wnd->d->needShowNormal = false;
            wnd->d->widget->showNormal();
        }
    }

    static void useAppliedGeometryForWindows()
    {
        Window *wnd = Window::mainPtr();
        if(!wnd || !wnd->d->widget) return;

        if(wnd->d->flags & WF_CENTERED)
        {
            wnd->d->appliedGeometry = centeredRect(wnd->rect().size());
        }

        LOG_DEBUG("Using applied geometry: %i, %i %s")
            << wnd->d->appliedGeometry.x() << wnd->d->appliedGeometry.y()
            << Vector2i(wnd->d->appliedGeometry.width(), wnd->d->appliedGeometry.height()).asText();
        wnd->d->widget->setGeometry(wnd->d->appliedGeometry);
    }

    static void endWindowWait()
    {
        Window *wnd = Window::mainPtr();
        if(wnd)
        {
            DEBUG_Message(("Window is no longer waiting for geometry changes."));

            // This flag is used for protecting against mode change resizings.
            wnd->d->needWait = false;
        }
    }

    static Window *canvasToWindow(Canvas &DENG_DEBUG_ONLY(canvas))
    {
        DENG2_ASSERT(mainWindow->d->widget->ownsCanvas(&canvas)); /// @todo multiwindow

        return mainWindow;
    }

    static int getWindowIdx(Window const &wnd)
    {
        /// @todo  Multiple windows.
        if(&wnd == mainWindow)
            return mainWindowIdx;
        return 0;
    }

    static void windowFocusChanged(Canvas &canvas, bool focus)
    {
        Window *wnd = canvasToWindow(canvas);
        DENG2_ASSERT(wnd->d->widget);

        LOG_DEBUG("windowFocusChanged focus:%b fullscreen:%b hidden:%b minimized:%b")
                << focus << wnd->isFullscreen()
                << wnd->d->widget->isHidden() << wnd->d->widget->isMinimized();

        if(!focus)
        {
            DD_ClearEvents();
            I_ResetAllDevices();
            wnd->trapMouse(false);
        }
        else if(wnd->isFullscreen())
        {
            // Trap the mouse again in fullscreen mode.
            wnd->trapMouse();
        }

        // Generate an event about this.
        ddevent_t ev;
        ev.type           = E_FOCUS;
        ev.focus.gained   = focus;
        ev.focus.inWindow = getWindowIdx(*wnd);
        DD_PostEvent(&ev);
    }

    static void finishMainWindowInit(Canvas &canvas)
    {
        Window *wnd = canvasToWindow(canvas);
        DENG2_ASSERT(wnd == mainWindow);

#if defined MACOSX
        if(wnd->isFullscreen())
        {
            // The window must be manually raised above the shielding window put up by
            // the fullscreen display capture.
            DisplayMode_Native_Raise(wnd->nativeHandle());
        }
#endif

        wnd->d->widget->raise();
        wnd->d->widget->activateWindow();

        // Automatically grab the mouse from the get-go if in fullscreen mode.
        if(Mouse_IsPresent() && wnd->isFullscreen())
        {
            wnd->trapMouse();
        }

        wnd->d->widget->canvas().setFocusFunc(windowFocusChanged);

#ifdef WIN32
        if(wnd->isFullscreen())
        {
            // It would seem we must manually give our canvas focus. Bug in Qt?
            wnd->d->widget->canvas().setFocus();
        }
#endif

        DD_FinishInitializationAfterWindowReady();
    }

    static bool windowIsClosing(CanvasWindow &)
    {
        LOG_DEBUG("Window is about to close, executing 'quit'.");

        /// @todo autosave and quit?
        Con_Execute(CMDS_DDAY, "quit", true, false);

        // We are not authorizing immediate closing of the window;
        // engine shutdown will take care of it later.
        return false; // don't close
    }

    /**
     * See the todo notes. Duplicating state is not a good idea.
     */
    static void updateWindowStateAfterUserChange()
    {
        Window *wnd = Window::mainPtr();
        if(!wnd || !wnd->d->widget) return;

        wnd->d->fetchGeometry();
        wnd->d->willUpdateWindowState = false;
    }

    static void windowWasMoved(CanvasWindow &cw)
    {
        LOG_AS("windowWasMoved");

        Window *wnd = canvasToWindow(cw.canvas());
        DENG2_ASSERT(wnd != 0);

        if(!(wnd->d->flags & WF_FULLSCREEN) && !wnd->d->needWait)
        {
            // The window was moved from its initial position; it is therefore
            // not centered any more (most likely).
            wnd->d->setFlag(WF_CENTERED, false);
        }

        if(!wnd->d->willUpdateWindowState)
        {
            wnd->d->willUpdateWindowState = true;
            LegacyCore_Timer(500, updateWindowStateAfterUserChange);
        }
    }
};

void Window::initialize()
{
    LOG_AS("Window::initialize");

    // Already been here?
    if(winManagerInited)
        return;

    LOG_MSG("Using Qt window management.");

    CanvasWindow::setDefaultGLFormat();

    winManagerInited = true;
}

void Window::shutdown()
{
    // Presently initialized?
    if(!winManagerInited)
        return;

    /// @todo Delete all windows, not just the main one.

    // Get rid of the windows.
    delete mainWindow; mainWindow = 0;

    // Now off-line, no more window management will be possible.
    winManagerInited = false;
}

Window *Window::create(char const *title)
{
    if(mainWindow) return 0; /// @todo  Allow multiple.

    Window *wnd = new Window();

    // Is this the main window?
    if(!mainWindow)
    {
        mainWindow = wnd;
        mainWindowIdx = 1;

        // After the main window is created, we can finish with the engine init.
        wnd->d->widget->canvas().setInitFunc(Instance::finishMainWindowInit);
    }

    wnd->restoreState();
    wnd->setTitle(title);

    // Let's see if there are command line options overriding the previous state.
    wnd->d->modifyAccordingToOptions();

    // Make it so. (Not shown yet.)
    wnd->d->applyGeometry();

#ifdef WIN32
    // Set an icon for the window.
    Path iconPath = DENG2_APP->nativeBasePath() / ("data\\graphics\\doomsday.ico");
    LOG_DEBUG("Window icon: ") << NativePath(iconPath).pretty();
    wnd->d->widget->setWindowIcon(QIcon(iconPath));
#endif

    return wnd;
}

bool Window::haveMain()
{
    return mainWindow != 0;
}

Window &Window::main()
{
    if(mainWindow)
    {
        return *mainWindow;
    }
    /// @throw MissingWindowError Attempted to reference a non-existant window.
    throw MissingWindowError("Window::main", "No main window is presently available");
}

Window *Window::byIndex(uint idx)
{
    if(!winManagerInited)
        return 0; // Window manager is not initialized.

    if(idx == 1)
        return mainWindow;

    //DENG2_ASSERT(false); // We can only have window 1 (main window).
    return 0;
}

Window::Window(char const *title) : d(new Instance(this))
{
    setTitle(title);
}

Window::~Window()
{
    if(!d->widget) return;

    d->widget->canvas().setFocusFunc(0);

    // Make sure we'll remember the config.
    saveState();

    // As the color transfer table is owned by the window on some platforms
    // (e.g., Windows) we must shutdown DisplayMode first.
    if(this == mainWindow)
    {
        DisplayMode_Shutdown();
    }

    // Delete the CanvasWindow.
    delete d->widget;
}

void Window::updateAfterResize()
{
    DENG2_ASSERT(d->widget);
    d->updateLayout();
}

QRect Window::rect() const
{
    return d->geometry;
}

QRect Window::normalRect() const
{
    return d->normalGeometry;
}

int Window::colorDepthBits() const
{
    return d->colorDepthBits;
}

void Window::trapMouse(bool enable) const
{
    if(!d->widget || novideo) return;

    DENG2_ASSERT(d->widget);
    d->widget->canvas().trapMouse(enable);
}

bool Window::isMouseTrapped() const
{
    DENG2_ASSERT(d->widget);
    return d->widget->canvas().isMouseTrapped();
}

void Window::updateCanvasFormat()
{
    d->needRecreateCanvas = true;

    // Save the relevant format settings.
    App::config().names()["window.fsaa"] = new NumberValue(Con_GetByte("vid-fsaa") != 0);
}

#if defined(UNIX) && !defined(MACOSX)
void GL_AssertContextActive()
{
    //Window *wnd = Window::main();
    DENG2_ASSERT(QGLContext::currentContext() != 0);
}
#endif

void Window::glActivate()
{
    DENG2_ASSERT(d->widget);
    d->widget->canvas().makeCurrent();

    DENG2_ASSERT_GL_CONTEXT_ACTIVE();
}

void Window::glDone()
{
    DENG2_ASSERT(d->widget);
    d->widget->canvas().doneCurrent();
}

QWidget *Window::widgetPtr()
{
    return d->widget;
}

CanvasWindow &Window::canvasWindow()
{
    DENG2_ASSERT(d->widget);
    return *d->widget;
}
#endif
