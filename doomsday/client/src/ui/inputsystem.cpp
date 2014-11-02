/** @file inputsystem.cpp  Input subsystem.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h" // strdup macro
#include "ui/inputsystem.h"

#include <QList>
#include <QtAlgorithms>
#include <de/timer.h> // SECONDSPERTIC
#include <de/Record>
#include <de/NumberValue>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>

#include "clientapp.h"
#include "dd_def.h"  // MAXEVENTS, ASSERT_NOT_64BIT
#include "dd_main.h" // App_GameLoaded()
#include "dd_loop.h" // DD_IsFrameTimeAdvancing()

#include "render/vr.h"

#include "ui/dd_input.h"
#include "ui/b_main.h"
#include "ui/clientwindow.h"
#include "ui/clientwindowsystem.h"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"
#include "ui/sys_input.h"

#include "sys_system.h" // novideo

using namespace de;

#define DEFAULT_JOYSTICK_DEADZONE  .05f  ///< 5%

#define MAX_AXIS_FILTER  40

static InputDevice *makeKeyboard(String const &name, String const &title = "")
{
    InputDevice *keyboard = new InputDevice(name);

    keyboard->setTitle(title);

    // DDKEYs are used as button indices.
    for(int i = 0; i < 256; ++i)
    {
        keyboard->addButton(new InputDeviceButtonControl);
    }

    return keyboard;
}

static InputDevice *makeMouse(String const &name, String const &title = "")
{
    InputDevice *mouse = new InputDevice(name);

    mouse->setTitle(title);

    for(int i = 0; i < IMB_MAXBUTTONS; ++i)
    {
        mouse->addButton(new InputDeviceButtonControl);
    }

    // Some of the mouse buttons have symbolic names.
    mouse->button(IMB_LEFT       ).setName("left");
    mouse->button(IMB_MIDDLE     ).setName("middle");
    mouse->button(IMB_RIGHT      ).setName("right");
    mouse->button(IMB_MWHEELUP   ).setName("wheelup");
    mouse->button(IMB_MWHEELDOWN ).setName("wheeldown");
    mouse->button(IMB_MWHEELLEFT ).setName("wheelleft");
    mouse->button(IMB_MWHEELRIGHT).setName("wheelright");

    // The mouse wheel is translated to keys, so there is no need to
    // create an axis for it.
    InputDeviceAxisControl *axis;
    mouse->addAxis(axis = new InputDeviceAxisControl("x", InputDeviceAxisControl::Pointer));
    //axis->setFilter(1); // On by default.
    axis->setScale(1.f/1000);

    mouse->addAxis(axis = new InputDeviceAxisControl("y", InputDeviceAxisControl::Pointer));
    //axis->setFilter(1); // On by default.
    axis->setScale(1.f/1000);

    return mouse;
}

static InputDevice *makeJoystick(String const &name, String const &title = "")
{
    InputDevice *joy = new InputDevice(name);

    joy->setTitle(title);

    for(int i = 0; i < IJOY_MAXBUTTONS; ++i)
    {
        joy->addButton(new InputDeviceButtonControl);
    }

    for(int i = 0; i < IJOY_MAXAXES; ++i)
    {
        char name[32];
        if(i < 4)
        {
            strcpy(name, i == 0? "x" : i == 1? "y" : i == 2? "z" : "w");
        }
        else
        {
            sprintf(name, "axis%02i", i + 1);
        }
        auto *axis = new InputDeviceAxisControl(name, InputDeviceAxisControl::Stick);
        joy->addAxis(axis);
        axis->setScale(1.0f / IJOY_AXISMAX);
        axis->setDeadZone(DEFAULT_JOYSTICK_DEADZONE);
    }

    for(int i = 0; i < IJOY_MAXHATS; ++i)
    {
        joy->addHat(new InputDeviceHatControl);
    }

    return joy;
}

static InputDevice *makeHeadTracker(String const &name, String const &title)
{
    InputDevice *head = new InputDevice(name);

    head->setTitle(title);

    auto *axis = new InputDeviceAxisControl("yaw", InputDeviceAxisControl::Stick);
    head->addAxis(axis);
    axis->setRawInput();

    head->addAxis(axis = new InputDeviceAxisControl("pitch", InputDeviceAxisControl::Stick));
    axis->setRawInput();

    head->addAxis(axis = new InputDeviceAxisControl("roll", InputDeviceAxisControl::Stick));
    axis->setRawInput();

    return head;
}

static Value *Function_InputSystem_BindEvent(Context &, Function::ArgumentValues const &args)
{
    String eventDesc = args[0]->asText();
    String command   = args[1]->asText();

    if(B_BindCommand(eventDesc.toLatin1(), command.toLatin1()))
    {
        // Success.
        return new NumberValue(true);
    }

    // Failed to create the binding...
    return new NumberValue(false);
}

#if 0
struct repeater_t
{
    int key;           ///< The DDKEY code (@c 0 if not in use).
    int native;        ///< Used to determine which key is repeating.
    char text[8];      ///< Text to insert.
    timespan_t timer;  ///< How's the time?
    int count;         ///< How many times has been repeated?
};
// The initial and secondary repeater delays (tics).
int repWait1 = 15;
int repWait2 = 3;
#endif

bool shiftDown;
bool altDown;
#ifdef OLD_FILTER
static uint mouseFreq;
#endif
static float oldPOV = IJOY_POV_CENTER;

static char *eventStrings[MAXEVENTS];

/**
 * Returns a copy of the string @a str. The caller does not get ownership of
 * the string. The string is valid until it gets overwritten by a new
 * allocation. There are at most MAXEVENTS strings allocated at a time.
 *
 * These are intended for strings in ddevent_t that are valid during the
 * processing of an event.
 */
static char const *allocEventString(char const *str)
{
    DENG2_ASSERT(str);
    static int eventStringRover = 0;

    DENG2_ASSERT(eventStringRover >= 0 && eventStringRover < MAXEVENTS);
    M_Free(eventStrings[eventStringRover]);
    char const *returnValue = eventStrings[eventStringRover] = strdup(str);

    if(++eventStringRover >= MAXEVENTS)
    {
        eventStringRover = 0;
    }
    return returnValue;
}

static void clearEventStrings()
{
    for(int i = 0; i < MAXEVENTS; ++i)
    {
        M_Free(eventStrings[i]); eventStrings[i] = nullptr;
    }
}

struct eventqueue_t
{
    ddevent_t events[MAXEVENTS];
    int head;
    int tail;
};

/**
 * Gets the next event from an input event queue.
 * @param q  Event queue.
 * @return @c NULL if no more events are available.
 */
static ddevent_t *nextFromQueue(eventqueue_t *q)
{
    DENG2_ASSERT(q);

    if(q->head == q->tail)
        return nullptr;

    ddevent_t *ev = &q->events[q->tail];
    q->tail = (q->tail + 1) & (MAXEVENTS - 1);

    return ev;
}

static void clearQueue(eventqueue_t *q)
{
    DENG2_ASSERT(q);
    q->head = q->tail;
}

static void postToQueue(eventqueue_t *q, ddevent_t *ev)
{
    DENG2_ASSERT(q && ev);
    q->events[q->head] = *ev;

    if(ev->type == E_SYMBOLIC)
    {
        // Allocate a throw-away string from our buffer.
        q->events[q->head].symbolic.name = allocEventString(ev->symbolic.name);
    }

    q->head++;
    q->head &= MAXEVENTS - 1;
}

static eventqueue_t queue;
static eventqueue_t sharpQueue;

static byte useSharpInputEvents = true; ///< cvar

DENG2_PIMPL(InputSystem)
{
    Binder binder;
    SettingsRegister settings;
    Record *scriptBindings = nullptr;
    bool ignoreInput = false;

    typedef QList<InputDevice *> Devices;
    Devices devices;

    Instance(Public *i) : Base(i)
    {
        // Initialize settings.
        settings.define(SettingsRegister::ConfigVariable, "input.mouse.syncSensitivity")
                .define(SettingsRegister::FloatCVar,      "input-mouse-x-scale", 1.f/1000.f)
                .define(SettingsRegister::FloatCVar,      "input-mouse-y-scale", 1.f/1000.f)
                .define(SettingsRegister::IntCVar,        "input-mouse-x-flags", 0)
                .define(SettingsRegister::IntCVar,        "input-mouse-y-flags", 0)
                .define(SettingsRegister::IntCVar,        "input-joy", 1)
                .define(SettingsRegister::IntCVar,        "input-sharp", 1);

        // Initialize script bindings.
        binder.initNew()
                << DENG2_FUNC(InputSystem_BindEvent, "bindEvent", "event" << "command");

        App::scriptSystem().addNativeModule("Input", binder.module());

        // Initialize system APIs.
        I_InitInterfaces();

        // Initialize devices.
        addDevice(makeKeyboard("key", "Keyboard"))->activate(); // A keyboard is assumed to always be present.

        addDevice(makeMouse("mouse", "Mouse"))->activate(Mouse_IsPresent()); // A mouse may not be present.

        addDevice(makeJoystick("joy", "Joystick"))->activate(Joystick_IsPresent()); // A joystick may not be present.

        /// @todo: Add support for multiple joysticks (just some generics, for now).
        addDevice(new InputDevice("joy2"));
        addDevice(new InputDevice("joy3"));
        addDevice(new InputDevice("joy4"));

        addDevice(makeHeadTracker("head", "Head Tracker")); // Head trackers are activated later.

        // Register console variables for the controls of all devices.
        for(InputDevice *device : devices) device->consoleRegister();
    }

    ~Instance()
    {
        qDeleteAll(devices);
        I_ShutdownInterfaces();
    }

    /**
     * @param device  InputDevice to add.
     * @return  Same as @a device for caller convenience.
     */
    InputDevice *addDevice(InputDevice *device)
    {
        if(device)
        {
            if(!devices.contains(device))
            {
                // Ensure the name is unique.
                for(InputDevice *otherDevice : devices)
                {
                    if(!otherDevice->name().compareWithoutCase(device->name()))
                    {
                        throw Error("InputSystem::addDevice", "Multiple devices with name:" + device->name() + " cannot coexist");
                    }
                }

                // Add this device to the collection.
                devices.append(device);
            }
        }
        return device;
    }

    /**
     * Send all the events of the given timestamp down the responder chain.
     */
    void dispatchEvents(eventqueue_t *q, timespan_t ticLength, bool updateAxes)
    {
        bool const callGameResponders = App_GameLoaded();

        ddevent_t *ddev;
        while((ddev = nextFromQueue(q)))
        {
            // Update the state of the input device tracking table.
            self.trackEvent(ddev);

            if(ignoreInput && ddev->type != E_FOCUS)
                continue;

            event_t ev;
            bool validGameEvent = self.convertEvent(ddev, &ev);

            if(validGameEvent && callGameResponders)
            {
                // Does the game's special responder use this event? This is
                // intended for grabbing events when creating bindings in the
                // Controls menu.
                if(gx.PrivilegedResponder && gx.PrivilegedResponder(&ev))
                {
                    continue;
                }
            }

            // The bindings responder.
            if(B_Responder(ddev)) continue;

            // The "fallback" responder. Gets the event if no one else is interested.
            if(validGameEvent && callGameResponders && gx.FallbackResponder)
            {
                gx.FallbackResponder(&ev);
            }
        }

        if(updateAxes)
        {
            // Input events have modified input device state: update the axis positions.
            for(InputDevice *dev : devices)
            {
                dev->forAllControls([&ticLength] (InputDeviceControl &control)
                {
                    if(auto *axis = control.maybeAs<InputDeviceAxisControl>())
                    {
                        axis->update(ticLength);
                    }
                    return LoopContinue;
                });
            }
        }
    }

    /**
     * Poll all event sources (i.e., input devices) and post events.
     */
    void postEventsForAllDevices()
    {
        // On the client may have have input devices.
        readKeyboard();
        readMouse();
        readJoystick();
        readHeadTracker();
    }

    /**
     * Check the current keyboard state, generate input events based on pressed/held
     * keys and poss them.
     *
     * @todo Does not belong at this level.
     */
    void readKeyboard()
    {
#define QUEUESIZE 32

        if(novideo) return;

        ddevent_t ev; de::zap(ev);
        ev.device = IDEV_KEYBOARD;
        ev.type   = E_TOGGLE;
        ev.toggle.state = ETOG_REPEAT;

        // Read the new keyboard events, convert to ddevents and post them.
        keyevent_t keyevs[QUEUESIZE];
        size_t const numkeyevs = Keyboard_GetEvents(keyevs, QUEUESIZE);
        for(size_t n = 0; n < numkeyevs; ++n)
        {
            keyevent_t *ke = &keyevs[n];

            // Check the type of the event.
            switch(ke->type)
            {
            case IKE_DOWN:   ev.toggle.state = ETOG_DOWN;   break;
            case IKE_REPEAT: ev.toggle.state = ETOG_REPEAT; break;
            case IKE_UP:     ev.toggle.state = ETOG_UP;     break;

            default: break;
            }

            ev.toggle.id = ke->ddkey;

            // Text content to insert?
            DENG2_ASSERT(sizeof(ev.toggle.text) == sizeof(ke->text));
            std::memcpy(ev.toggle.text, ke->text, sizeof(ev.toggle.text));

            LOG_INPUT_XVERBOSE("toggle.id: %i/%c [%s:%u]")
                    << ev.toggle.id << ev.toggle.id
                    << ev.toggle.text << strlen(ev.toggle.text);

            self.postEvent(&ev);
        }

#undef QUEUESIZE
    }

    /**
     * Check the current mouse state (axis, buttons and wheel).
     * Generates events and mickeys and posts them.
     *
     * @todo Does not belong at this level.
     */
    void readMouse()
    {
        if(!Mouse_IsPresent())
            return;

        mousestate_t mouse;

#ifdef OLD_FILTER
        // Should we test the mouse input frequency?
        if(mouseFreq > 0)
        {
            static uint lastTime = 0;
            uint nowTime = Timer_RealMilliseconds();

            if(nowTime - lastTime < 1000 / mouseFreq)
            {
                // Don't ask yet.
                std::memset(&mouse, 0, sizeof(mouse));
            }
            else
            {
                lastTime = nowTime;
                Mouse_GetState(&mouse);
            }
        }
        else
#endif
        {
            // Get the mouse state.
            Mouse_GetState(&mouse);
        }

        ddevent_t ev; de::zap(ev);
        ev.device = IDEV_MOUSE;
        ev.type   = E_AXIS;

        float xpos = mouse.axis[IMA_POINTER].x;
        float ypos = mouse.axis[IMA_POINTER].y;

        /*if(uiMouseMode)
        {
            ev.axis.type = EAXIS_ABSOLUTE;
        }
        else*/
        {
            ev.axis.type = EAXIS_RELATIVE;
            ypos = -ypos;
        }

        // Post an event per axis.
        // Don't post empty events.
        if(xpos)
        {
            ev.axis.id  = 0;
            ev.axis.pos = xpos;
            self.postEvent(&ev);
        }
        if(ypos)
        {
            ev.axis.id  = 1;
            ev.axis.pos = ypos;
            self.postEvent(&ev);
        }

        // Some very verbose output about mouse buttons.
        int i = 0;
        for(; i < IMB_MAXBUTTONS; ++i)
        {
            if(mouse.buttonDowns[i] || mouse.buttonUps[i])
                break;
        }
        if(i < IMB_MAXBUTTONS)
        {
            for(i = 0; i < IMB_MAXBUTTONS; ++i)
            {
                LOGDEV_INPUT_XVERBOSE("[%02i] %i/%i")
                        << i << mouse.buttonDowns[i] << mouse.buttonUps[i];
            }
        }

        // Post mouse button up and down events.
        ev.type = E_TOGGLE;
        for(i = 0; i < IMB_MAXBUTTONS; ++i)
        {
            ev.toggle.id = i;
            while(mouse.buttonDowns[i] > 0 || mouse.buttonUps[i] > 0)
            {
                if(mouse.buttonDowns[i]-- > 0)
                {
                    ev.toggle.state = ETOG_DOWN;
                    LOG_INPUT_XVERBOSE("Mouse button %i down") << i;
                    self.postEvent(&ev);
                }
                if(mouse.buttonUps[i]-- > 0)
                {
                    ev.toggle.state = ETOG_UP;
                    LOG_INPUT_XVERBOSE("Mouse button %i up") << i;
                    self.postEvent(&ev);
                }
            }
        }
    }

    /**
     * Check the current joystick state (axis, sliders, hat and buttons).
     * Generates events and posts them. Axis clamps and dead zone is done
     * here.
     *
     * @todo Does not belong at this level.
     */
    void readJoystick()
    {
        if(!Joystick_IsPresent())
            return;

        joystate_t state;
        Joystick_GetState(&state);

        // Joystick buttons.
        ddevent_t ev; de::zap(ev);
        ev.device = IDEV_JOY1;
        ev.type   = E_TOGGLE;

        for(int i = 0; i < state.numButtons; ++i)
        {
            ev.toggle.id = i;
            while(state.buttonDowns[i] > 0 || state.buttonUps[i] > 0)
            {
                if(state.buttonDowns[i]-- > 0)
                {
                    ev.toggle.state = ETOG_DOWN;
                    self.postEvent(&ev);
                    LOG_INPUT_XVERBOSE("Joy button %i down") << i;
                }
                if(state.buttonUps[i]-- > 0)
                {
                    ev.toggle.state = ETOG_UP;
                    self.postEvent(&ev);
                    LOG_INPUT_XVERBOSE("Joy button %i up") << i;
                }
            }
        }

        if(state.numHats > 0)
        {
            // Check for a POV change.
            /// @todo: Some day it would be nice to support multiple hats here. -jk
            if(state.hatAngle[0] != oldPOV)
            {
                ev.type = E_ANGLE;
                ev.angle.id = 0;

                if(state.hatAngle[0] < 0)
                {
                    ev.angle.pos = -1;
                }
                else
                {
                    // The new angle becomes active.
                    ev.angle.pos = ROUND(state.hatAngle[0] / 45);
                }
                self.postEvent(&ev);

                oldPOV = state.hatAngle[0];
            }
        }

        // Send joystick axis events, one per axis.
        ev.type = E_AXIS;

        for(int i = 0; i < state.numAxes; ++i)
        {
            ev.axis.id   = i;
            ev.axis.pos  = state.axis[i];
            ev.axis.type = EAXIS_ABSOLUTE;
            self.postEvent(&ev);
        }
    }

    /// @todo Does not belong at this level.
    void readHeadTracker()
    {
        // These values are for the input subsystem and gameplay. The renderer will check the head
        // orientation independently, with as little latency as possible.

        // If a head tracking device is connected, the device is marked active.
        if(!DD_GetInteger(DD_USING_HEAD_TRACKING))
        {
            self.device(IDEV_HEAD_TRACKER).deactivate();
            return;
        }

        self.device(IDEV_HEAD_TRACKER).activate();

        // Get the latest values.
        //vrCfg().oculusRift().allowUpdate();
        //vrCfg().oculusRift().update();

        ddevent_t ev; de::zap(ev);
        ev.device = IDEV_HEAD_TRACKER;
        ev.type   = E_AXIS;
        ev.axis.type = EAXIS_ABSOLUTE;

        Vector3f const pry = vrCfg().oculusRift().headOrientation();

        // Yaw (1.0 means 180 degrees).
        ev.axis.id  = 0; // Yaw.
        ev.axis.pos = de::radianToDegree(pry[2]) * 1.0 / 180.0;
        self.postEvent(&ev);

        ev.axis.id  = 1; // Pitch (1.0 means 85 degrees).
        ev.axis.pos = de::radianToDegree(pry[0]) * 1.0 / 85.0;
        self.postEvent(&ev);

        // So I'll assume that if roll ever gets used, 1.0 will mean 180 degrees there too.
        ev.axis.id  = 2; // Roll.
        ev.axis.pos = de::radianToDegree(pry[1]) * 1.0 / 180.0;
        self.postEvent(&ev);
    }
};

InputSystem::InputSystem() : d(new Instance(this))
{}

void InputSystem::timeChanged(Clock const &)
{}

SettingsRegister &InputSystem::settings()
{
    return d->settings;
}

InputDevice &InputSystem::device(int id) const
{
    if(id >= 0 && id < d->devices.count())
    {
        return *d->devices.at(id);
    }
    throw Error("InputSystem::device", "Unknown id:" + String::number(id));
}

InputDevice *InputSystem::devicePtr(int id) const
{
    if(id >= 0 && id < d->devices.count())
    {
        return d->devices.at(id);
    }
    return nullptr;
}

LoopResult InputSystem::forAllDevices(std::function<LoopResult (InputDevice &)> func) const
{
    for(InputDevice *device : d->devices)
    {
        if(auto result = func(*device)) return result;
    }
    return LoopContinue;
}

void InputSystem::trackEvent(ddevent_t *ev)
{
    DENG2_ASSERT(ev);

    if(ev->type == E_FOCUS || ev->type == E_SYMBOLIC)
        return; // Not a tracked device state.

    InputDevice *dev = devicePtr(ev->device);
    if(!dev || !dev->isActive()) return;

    // Track the state of Shift and Alt.
    if(IS_KEY_TOGGLE(ev))
    {
        if(ev->toggle.id == DDKEY_RSHIFT)
        {
            if(ev->toggle.state == ETOG_DOWN)
                ::shiftDown = true;
            else if(ev->toggle.state == ETOG_UP)
                ::shiftDown = false;
        }
        else if(ev->toggle.id == DDKEY_RALT)
        {
            if(ev->toggle.state == ETOG_DOWN)
            {
                ::altDown = true;
                //qDebug() << "Alt down";
            }
            else if(ev->toggle.state == ETOG_UP)
            {
                ::altDown = false;
                //qDebug() << "Alt up";
            }
        }
    }

    // Update the state table.
    /// @todo Offer the event to each control in turn.
    if(ev->type == E_AXIS)
    {
        dev->axis(ev->axis.id).applyRealPosition(ev->axis.pos);
    }
    else if(ev->type == E_TOGGLE)
    {
        dev->button(ev->toggle.id).setDown(ev->toggle.state == ETOG_DOWN || ev->toggle.state == ETOG_REPEAT);
    }
    else if(ev->type == E_ANGLE)
    {
        dev->hat(ev->angle.id).setPosition(ev->angle.pos);
    }
}

bool InputSystem::ignoreEvents(bool yes)
{
    bool const oldIgnoreInput = d->ignoreInput;

    d->ignoreInput = yes;
    LOG_INPUT_VERBOSE("Ignoring input: %b") << yes;
    if(!yes)
    {
        // Clear all the event buffers.
        d->postEventsForAllDevices();
        clearEvents();
    }
    return oldIgnoreInput;
}

void InputSystem::clearEvents()
{
    clearQueue(&queue);
    clearQueue(&sharpQueue);

    clearEventStrings();
}

/// @note Called by the I/O functions when input is detected.
void InputSystem::postEvent(ddevent_t *ev)
{
    DENG2_ASSERT(ev);// && ev->device < NUM_INPUT_DEVICES);

    eventqueue_t *q = &queue;
    if(useSharpInputEvents &&
       (ev->type == E_TOGGLE || ev->type == E_AXIS || ev->type == E_ANGLE))
    {
        q = &sharpQueue;
    }

    // Cleanup: make sure only keyboard toggles can have a text insert.
    if(ev->type == E_TOGGLE && ev->device != IDEV_KEYBOARD)
    {
        std::memset(ev->toggle.text, 0, sizeof(ev->toggle.text));
    }

    postToQueue(q, ev);

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
    if(ev->device == IDEV_KEYBOARD && ev->type == E_TOGGLE && ev->toggle.state == ETOG_DOWN)
    {
        extern float devCameraMovementStartTime;
        extern float devCameraMovementStartTimeRealSecs;

        // Restart timer on each key down.
        devCameraMovementStartTime         = sysTime;
        devCameraMovementStartTimeRealSecs = Sys_GetRealSeconds();
    }
#endif
}

void InputSystem::convertEvent(de::Event const &event, ddevent_t *ddEvent) // static
{
    DENG2_ASSERT(ddEvent);
    using de::KeyEvent;

    de::zapPtr(ddEvent);

    switch(event.type())
    {
    case de::Event::KeyPress:
    case de::Event::KeyRelease: {
        KeyEvent const &kev = event.as<KeyEvent>();

        ddEvent->device       = IDEV_KEYBOARD;
        ddEvent->type         = E_TOGGLE;
        ddEvent->toggle.id    = kev.ddKey();
        ddEvent->toggle.state = (kev.state() == KeyEvent::Pressed? ETOG_DOWN : ETOG_UP);
        strcpy(ddEvent->toggle.text, kev.text().toLatin1());
        break; }

    default: break;
    }
}

bool InputSystem::convertEvent(ddevent_t const *ddEvent, event_t *ev) // static
{
    DENG2_ASSERT(ddEvent && ev);
    // Copy the essentials into a cutdown version for the game.
    // Ensure the format stays the same for future compatibility!
    //
    /// @todo This is probably broken! (DD_MICKEY_ACCURACY=1000 no longer used...)
    //
    de::zapPtr(ev);
    if(ddEvent->type == E_SYMBOLIC)
    {
        ev->type = EV_SYMBOLIC;
#ifdef __64BIT__
        ASSERT_64BIT(ddEvent->symbolic.name);
        ev->data1 = (int)(((uint64_t) ddEvent->symbolic.name) & 0xffffffff); // low dword
        ev->data2 = (int)(((uint64_t) ddEvent->symbolic.name) >> 32); // high dword
#else
        ASSERT_NOT_64BIT(ddEvent->symbolic.name);

        ev->data1 = (int) ddEvent->symbolic.name;
        ev->data2 = 0;
#endif
    }
    else if(ddEvent->type == E_FOCUS)
    {
        ev->type  = EV_FOCUS;
        ev->data1 = ddEvent->focus.gained;
        ev->data2 = ddEvent->focus.inWindow;
    }
    else
    {
        switch(ddEvent->device)
        {
        case IDEV_KEYBOARD:
            ev->type = EV_KEY;
            if(ddEvent->type == E_TOGGLE)
            {
                ev->state = (  ddEvent->toggle.state == ETOG_UP  ? EVS_UP
                             : ddEvent->toggle.state == ETOG_DOWN? EVS_DOWN
                             : EVS_REPEAT );
                ev->data1 = ddEvent->toggle.id;
            }
            break;

        case IDEV_MOUSE:
            if(ddEvent->type == E_AXIS)
            {
                ev->type = EV_MOUSE_AXIS;
            }
            else if(ddEvent->type == E_TOGGLE)
            {
                ev->type = EV_MOUSE_BUTTON;
                ev->data1 = ddEvent->toggle.id;
                ev->state = (  ddEvent->toggle.state == ETOG_UP  ? EVS_UP
                             : ddEvent->toggle.state == ETOG_DOWN? EVS_DOWN
                             : EVS_REPEAT );
            }
            break;

        case IDEV_JOY1:
        case IDEV_JOY2:
        case IDEV_JOY3:
        case IDEV_JOY4:
            if(ddEvent->type == E_AXIS)
            {
                int *data = &ev->data1;
                ev->type = EV_JOY_AXIS;
                ev->state = (evstate_t) 0;
                if(ddEvent->axis.id >= 0 && ddEvent->axis.id < 6)
                {
                    data[ddEvent->axis.id] = ddEvent->axis.pos;
                }
                /// @todo  The other dataN's must contain up-to-date information
                /// as well. Read them from the current joystick status.
            }
            else if(ddEvent->type == E_TOGGLE)
            {
                ev->type = EV_JOY_BUTTON;
                ev->state = (  ddEvent->toggle.state == ETOG_UP  ? EVS_UP
                             : ddEvent->toggle.state == ETOG_DOWN? EVS_DOWN
                             : EVS_REPEAT );
                ev->data1 = ddEvent->toggle.id;
            }
            else if(ddEvent->type == E_ANGLE)
            {
                ev->type = EV_POV;
            }
            break;

        case IDEV_HEAD_TRACKER:
            // No game-side equivalent exists.
            return false;

        default:
#ifdef DENG2_DEBUG
            App_Error("InputSystem::convertEvent: Unknown deviceID in ddevent_t");
#endif
            return false;
        }
    }
    return true;
}

void InputSystem::processEvents(timespan_t ticLength)
{
    // Poll all event sources (i.e., input devices) and post events.
    d->postEventsForAllDevices();

    // Dispatch all accumulated events down the responder chain.
    d->dispatchEvents(&queue, ticLength, !useSharpInputEvents);
}

void InputSystem::processSharpEvents(timespan_t ticLength)
{
    // Sharp ticks may have some events queued on the side.
    if(DD_IsSharpTick() || !DD_IsFrameTimeAdvancing())
    {
        d->dispatchEvents(&sharpQueue, DD_IsFrameTimeAdvancing()? SECONDSPERTIC : ticLength, true);
    }
}

bool InputSystem::shiftDown() const
{
    return ::shiftDown;
}

D_CMD(ListAllDevices)
{
    DENG2_UNUSED3(src, argc, argv);
    LOG_INPUT_MSG(_E(b) "Input Devices:");
    ClientApp::inputSystem().forAllDevices([] (InputDevice &device)
    {
        LOG_INPUT_MSG("") << device.description();
        return LoopContinue;
    });
    return true;
}

D_CMD(ReleaseMouse)
{
    DENG2_UNUSED3(src, argc, argv);
    if(WindowSystem::mainExists())
    {
        ClientWindowSystem::main().canvas().trapMouse(false);
        return true;
    }
    return false;
}

void InputSystem::consoleRegister() // static
{
    B_ConsoleRegister(); // for control bindings

    // Cvars
    C_VAR_BYTE("input-sharp", &useSharpInputEvents, 0, 0, 1);

    // Ccmds
    C_CMD("listinputdevices",   "",     ListAllDevices);
    C_CMD("releasemouse",       "",     ReleaseMouse);
    //C_CMD_FLAGS("setaxis",      "s",    AxisPrintConfig,  CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis",      "ss",   AxisChangeOption, CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis",      "sss",  AxisChangeValue,  CMDF_NO_DEDICATED);

    I_ConsoleRegister();
}
