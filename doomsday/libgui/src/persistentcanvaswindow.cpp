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
#include <QResizeEvent>
#include <QTimer>
#include <QVector>
#include <QList>

namespace de {

static String const MAIN_WINDOW_ID = "main";

int const PersistentCanvasWindow::MIN_WIDTH  = 320;
int const PersistentCanvasWindow::MIN_HEIGHT = 240;

static int const BREAK_CENTERING_THRESHOLD = 5;

static QRect desktopRect()
{
    /// @todo Multimonitor? This checks the default screen.
    return QApplication::desktop()->screenGeometry();
}

static QRect centeredQRect(Vector2ui const &size)
{
    QSize screenSize = desktopRect().size();

    LOG_TRACE("centeredGeometry: Current desktop rect %i x %i")
            << screenSize.width() << screenSize.height();

    return QRect(desktopRect().topLeft() +
                 QPoint((screenSize.width()  - size.x) / 2,
                        (screenSize.height() - size.y) / 2),
                 QSize(size.x, size.y));
}

static Rectanglei centeredRect(Vector2ui const &size)
{
    QRect rect = centeredQRect(size);
    return Rectanglei(rect.left(), rect.top(), rect.width(), rect.height());
}

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
            return QString("window.%1.%2").arg(winId).arg(key);
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

            config.set(configName("center"),     isCentered());
            config.set(configName("maximize"),   isMaximized());
            config.set(configName("fullscreen"), isFullscreen());
            config.set(configName("colorDepth"), colorDepthBits);
            config.set(configName("fsaa"),       isAntialiased());
            config.set(configName("vsync"),      isVSync());
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
                fullSize = Size(fs.at(0).asNumber(), fs.at(1).asNumber());
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
            TrapMouse,
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
    bool neverShown;

    typedef QList<Task> Tasks;
    Tasks queue;

    Instance(Public *i, String const &windowId)
        : Base(i),
          id(windowId),
          state(windowId),
          neverShown(true)
    {
        // Keep a global pointer to the main window.
        if(id == MAIN_WINDOW_ID)
        {
            DENG2_ASSERT(!haveMain());
            setMain(thisPublic);
        }

        self.setMinimumSize(MIN_WIDTH, MIN_HEIGHT);
    }

    ~Instance()
    {
        self.saveToConfig();
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

            case Fullscreen:
                // Can't go to fullscreen when downloading.
                //if(attribs[i] && Updater_IsDownloadInProgress())
                //    return false;
                break;

            case ColorDepthBits:
                if(attribs[i] < 8 || attribs[i] > 32)
                    return false; // Illegal value.
                break;

            case Centered:
            case Maximized:
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

    /**
     * Parse attributes and apply the values to the widget.
     *
     * @param attribs  Zero-terminated array of attributes and values.
     */
    void applyAttributes(int const *attribs)
    {
        LOG_AS("applyAttributes");

        DENG2_ASSERT(attribs);

        // Update the cached state from the authoritative source:
        // the widget itself.
        state = widgetState();

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
                if(attribs[i]) mod.setFlag(State::Fullscreen, false);
                break;

            case Fullscreen:
                mod.setFlag(State::Fullscreen, attribs[i]);
                if(attribs[i]) mod.setFlag(State::Maximized, false);
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

        LOG_DEBUG("windowRect:%s fullSize:%s depth:%i flags:%x")
                << mod.windowRect.asText()
                << mod.fullSize.asText()
                << mod.colorDepthBits
                << mod.flags;

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

    /**
     * Apply a logical state to the concrete widget instance. All properties of
     * the widget may not be updated instantly during this method. Particularly
     * a display mode change will cause geometry changes to occur later.
     *
     * @param newState  State to apply.
     */
    void applyToWidget(State const &newState)
    {
        bool trapped = self.canvas().isMouseTrapped();

        // If the display mode needs to change, we will have to defer the rest
        // of the state changes so that everything catches up after the change.
        TimeDelta defer = 0;
        DisplayMode const *newMode = newState.displayMode();
        bool modeChanged = false;

        if(!self.isVisible())
        {
            // Change size immediately.
            queue << Task(newState.windowRect);
        }

        // Change display mode, if necessary.
        if(!DisplayMode_IsEqual(DisplayMode_Current(), newMode))
        {
            LOG_INFO("Changing display mode to %i x %i x %i (%.1f Hz)")
                    << newMode->width << newMode->height << newMode->depth << newMode->refreshRate;

            modeChanged = DisplayMode_Change(newMode, newState.shouldCaptureScreen());
            state.colorDepthBits = newMode->depth;

            // Wait a while after the mode change to let changes settle in.
#ifdef MACOSX
            defer = .1;
#else
            defer = .01;
#endif
        }

        if(self.isVisible())
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

            if(newState.isWindow())
            {
                queue << Task(Task::ShowNormal, defer) << Task(newState.windowRect);
            }
            else
            {
                if(modeChanged)
                {
                    queue << Task(Task::ShowNormal, defer);
                    defer = 0.01;
                }

                if(newState.isMaximized())
                {
                    queue << Task(Task::ShowMaximized, defer);
                    state.windowRect = newState.windowRect;
                }
                else if(newState.isFullscreen())
                {
                    queue << Task(Task::ShowFullscreen, defer);
                    state.windowRect = newState.windowRect;
                }
            }

            defer = 0;
        }

        if(modeChanged)
        {
#ifdef MACOSX
            if(newState.isFullscreen())
            {
                queue << Task(Task::MacRaiseOverShield);
            }
#endif
            queue << Task(Task::NotifyModeChange, .1);
        }

        if(trapped /*|| newState.isFullscreen()*/)
        {
            queue << Task(Task::TrapMouse);
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
                    state.windowRect = next.rect;
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

                case Task::TrapMouse:
                    self.canvas().trapMouse();
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
    try
    {
        restoreFromConfig();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to restore window state:\n%s") << er.asText();
    }
}

void PersistentCanvasWindow::saveToConfig()
{
    try
    {
        d->widgetState().saveToConfig();
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to save window state:\n%s") << er.asText();
    }
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
    if(d->neverShown)
    {
        // If the window hasn't been shown yet, it doesn't have a valid
        // normal geometry. Use the one defined in the logical state.
        return d->state.windowRect;
    }

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

        // Now it has been shown.
        d->neverShown = false;
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
    DENG2_ASSERT(haveMain() != 0);
    if(!haveMain())
    {
        throw InvalidIdError("PersistentCanvasWindow::main",
                             "No window found with id \"" + MAIN_WINDOW_ID + "\"");
    }
    return static_cast<PersistentCanvasWindow &>(CanvasWindow::main());
}

void PersistentCanvasWindow::moveEvent(QMoveEvent *)
{
    if(isCentered() && !isMaximized() && !isFullScreen())
    {
        int len = (geometry().topLeft() - centeredQRect(size()).topLeft()).manhattanLength();

        if(len > BREAK_CENTERING_THRESHOLD)
        {
            d->state.setFlag(Instance::State::Centered, false);
        }
        else
        {
            // Recenter.
            setGeometry(centeredQRect(size()));
        }
    }
}

void PersistentCanvasWindow::resizeEvent(QResizeEvent *ev)
{
    LOG_DEBUG("Window resized: maximized:%b old:%ix%i new:%ix%i")
            << isMaximized() << ev->oldSize().width() << ev->oldSize().height()
            << ev->size().width() << ev->size().height();

    /*
    if(!isMaximized() && !isFullScreen())
    {
        d->state.windowRect.setSize(Vector2i(ev->size().width(), ev->size().height()));
    }*/
}

} // namespace de
