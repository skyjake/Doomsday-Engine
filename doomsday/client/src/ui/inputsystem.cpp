/** @file inputsystem.cpp  Input subsystem.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_BINDING

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
#include "m_misc.h"  // M_WriteTextEsc

#include "render/vr.h"
#include "world/p_players.h"

#include "BindContext"
#include "ui/ddevent.h"
#include "ui/b_main.h"
#include "ui/b_util.h"
#include "ui/clientwindow.h"
#include "ui/clientwindowsystem.h"
#include "ui/inputdebug.h"  // Debug visualization.
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"
#include "ui/playerimpulse.h"
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

    if(ClientApp::inputSystem().bindCommand(eventDesc.toLatin1(), command.toLatin1()))
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
, DENG2_OBSERVES(BindContext, ActiveChange)
, DENG2_OBSERVES(BindContext, AcquireDeviceChange)
, DENG2_OBSERVES(BindContext, BindingAddition)
{
    Binder binder;
    SettingsRegister settings;
    Record *scriptBindings = nullptr;
    bool ignoreInput = false;

    typedef QList<InputDevice *> Devices;
    Devices devices;

    typedef QList<BindContext *> BindContexts;
    BindContexts contexts;  ///< Ordered from highest to lowest priority.

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
    }

    ~Instance()
    {
        self.clearAllContexts();
        clearAllDevices();
        I_ShutdownInterfaces();
    }

    void clearAllDevices()
    {
        qDeleteAll(devices);
        devices.clear();
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

    void echoSymbolicEvent(ddevent_t const &ev)
    {
        // Disabled?
        if(!symbolicEchoMode) return;

        // Some event types are never echoed.
        if(ev.type == E_SYMBOLIC || ev.type == E_FOCUS) return;

        // Input device axis controls can be pretty sensitive (or misconfigured)
        // so we need to do some filtering to determine if the motion is strong
        // enough for an echo.
        if(ev.type == E_AXIS)
        {
            InputDevice const &device = self.device(ev.device);
            float const pos = device.axis(ev.axis.id).translateRealPosition(ev.axis.pos);

            if((ev.axis.type == EAXIS_ABSOLUTE && fabs(pos) < .5f) ||
               (ev.axis.type == EAXIS_RELATIVE && fabs(pos) < .02f))
            {
                return; // Not significant enough.
            }
        }

        // Echo the event.
        String const name = "echo-" + B_EventToString(ev);

        ddevent_t echo; de::zap(echo);
        echo.device = ev.device;
        echo.type   = E_SYMBOLIC;
        echo.symbolic.id   = 0;
        echo.symbolic.name = name.toUtf8().constData();

        LOG_INPUT_XVERBOSE("Symbolic echo: %s") << echo.symbolic.name;
        self.postEvent(&echo);
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
            self.trackEvent(*ddev);

            if(ignoreInput && ddev->type != E_FOCUS)
                continue;

            event_t ev;
            bool validGameEvent = self.convertEvent(*ddev, ev);

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

            // Generate a symbolic event if echo mode enabled.
            echoSymbolicEvent(*ddev);

            // Try the binding system to see if we need to respond to the event
            // and if so, trigger any associated actions.
            if(self.tryEvent(*ddev))
            {
                continue;
            }

            // The "fallback" responder. Gets the event if no one else is interested.
            if(validGameEvent && callGameResponders && gx.FallbackResponder)
            {
                gx.FallbackResponder(&ev);
            }
        }

        if(updateAxes)
        {
            // An event may have modified device-control state: update the axis positions.
            for(InputDevice *device : devices)
            {
                device->forAllControls([&ticLength] (InputDeviceControl &ctrl)
                {
                    if(auto *axis = ctrl.maybeAs<InputDeviceAxisControl>())
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
        // These values are for the input subsystem and gameplay. The renderer will
        // check the head orientation independently, with as little latency as possible.

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

    /**
     * Mark all device states with the highest-priority binding context to which they
     * have a connection via device bindings. This ensures that if a high-priority context
     * is using a particular device state, lower-priority contexts will not be using the
     * same state for their own controls.
     *
     * Called automatically whenever a context is activated or deactivated.
     */
    void updateAllDeviceStateAssociations()
    {
        auto &_self = self;

        // Clear all existing associations.
        for(InputDevice *device : devices)
        {
            device->forAllControls([] (InputDeviceControl &ctrl)
            {
                ctrl.clearBindContextAssociation();
                return LoopContinue;
            });
        }

        // Mark all bindings in all contexts.
        for(BindContext *context : contexts)
        {
            // Skip inactive contexts.
            if(!context->isActive())
                continue;

            context->forAllCommandBindings([&_self, &context] (CommandBinding &bind)
            {
                InputDevice &dev = _self.device(bind.deviceId);

                InputDeviceControl *ctrl = nullptr;
                switch(bind.type)
                {
                case E_TOGGLE: ctrl = &dev.button(bind.controlId); break;
                case E_AXIS:   ctrl = &dev.axis(bind.controlId);   break;
                case E_ANGLE:  ctrl = &dev.hat(bind.controlId);    break;

                case E_SYMBOLIC: break;

                default:
                    DENG2_ASSERT(!"InputSystem::updateAllDeviceStateAssociations: Invalid bind.type");
                    break;
                }

                if(ctrl && !ctrl->hasBindContext())
                {
                    ctrl->setBindContext(context);
                }

                return LoopContinue;
            });

            // Associate all the device bindings.
            context->forAllImpulseBindings([&_self, &context] (ImpulseBinding &bind)
            {
                InputDevice &dev = _self.device(bind.deviceId);

                InputDeviceControl *ctrl = nullptr;
                switch(bind.type)
                {
                case IBD_TOGGLE: ctrl = &dev.button(bind.controlId); break;
                case IBD_AXIS:   ctrl = &dev.axis(bind.controlId);   break;
                case IBD_ANGLE:  ctrl = &dev.hat(bind.controlId);    break;

                default:
                    DENG2_ASSERT(!"InputSystem::updateAllDeviceStateAssociations: Invalid bind.type");
                    break;
                }

                if(ctrl && !ctrl->hasBindContext())
                {
                    ctrl->setBindContext(context);
                }

                return LoopContinue;
            });

            // If the context have made a broad device acquisition, mark all relevant states.
            for(int i = 0; i < devices.count(); ++i)
            {
                InputDevice *device = devices.at(i);
                int const deviceId = i;

                if(device->isActive() && context->willAcquire(deviceId))
                {
                    device->forAllControls([&context] (InputDeviceControl &ctrl)
                    {
                        if(!ctrl.hasBindContext())
                        {
                            ctrl.setBindContext(context);
                        }
                        return LoopContinue;
                    });
                }
            };
        }

        // Now that we know what are the updated context associations, let's check
        // the devices and see if any of the states need to be expired.
        for(InputDevice *device : devices)
        {
            device->forAllControls([] (InputDeviceControl &ctrl)
            {
                if(!ctrl.inDefaultState())
                {
                    ctrl.expireBindContextAssociationIfChanged();
                }
                return LoopContinue;
            });
        };
    }

    /**
     * When the active state of a binding context changes we'll re-evaluate the
     * effective bindings to avoid wasting time by looking them up repeatedly.
     */
    void bindContextActiveChanged(BindContext &context)
    {
        updateAllDeviceStateAssociations();

        for(int i = 0; i < devices.count(); ++i)
        {
            InputDevice *device = devices.at(i);
            int const deviceId  = i;

            if(context.willAcquire(deviceId))
            {
                device->reset();
            }
        }
    }

    void bindContextAcquireDeviceChanged(BindContext &)
    {
        updateAllDeviceStateAssociations();
    }

    void bindContextBindingAdded(BindContext &, void * /*binding*/, bool /*isCommand*/)
    {
        updateAllDeviceStateAssociations();
    }
};

InputSystem::InputSystem() : d(new Instance(this))
{
    initAllDevices();
}

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

int InputSystem::deviceCount() const
{
    return d->devices.count();
}

void InputSystem::initAllDevices()
{
    d->clearAllDevices();

    // Initialize devices.
    d->addDevice(makeKeyboard("key", "Keyboard"))->activate(); // A keyboard is assumed to always be present.

    d->addDevice(makeMouse("mouse", "Mouse"))->activate(Mouse_IsPresent()); // A mouse may not be present.

    d->addDevice(makeJoystick("joy", "Joystick"))->activate(Joystick_IsPresent()); // A joystick may not be present.

    /// @todo: Add support for multiple joysticks (just some generics, for now).
    d->addDevice(new InputDevice("joy2"));
    d->addDevice(new InputDevice("joy3"));
    d->addDevice(new InputDevice("joy4"));

    d->addDevice(makeHeadTracker("head", "Head Tracker")); // Head trackers are activated later.

    // Register console variables for the controls of all devices.
    for(InputDevice *device : d->devices) device->consoleRegister();
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

bool InputSystem::tryEvent(ddevent_t const &event, String const &namedContext)
{
    if(namedContext.isEmpty())
    {
        // Try all active contexts in order.
        for(BindContext *context : d->contexts)
        {
            if(context->tryEvent(event)) return true;
        }
        return false;
    }

    // Check a specific binding context for an action (regardless of its activation status).
    if(hasContext(namedContext))
    {
        return context(namedContext).tryEvent(event, false/*this context only*/);
    }

    return false;
}

bool InputSystem::tryEvent(Event const &event, String const &context)
{
    ddevent_t ddev;
    convertEvent(event, ddev);
    return tryEvent(ddev, context);
}

void InputSystem::trackEvent(ddevent_t const &event)
{
    if(event.type == E_FOCUS || event.type == E_SYMBOLIC)
        return; // Not a tracked device state.

    InputDevice *dev = devicePtr(event.device);
    if(!dev || !dev->isActive()) return;

    // Track the state of Shift and Alt.
    if(event.device == IDEV_KEYBOARD && event.type == E_TOGGLE)
    {
        if(event.toggle.id == DDKEY_RSHIFT)
        {
            if(event.toggle.state == ETOG_DOWN)
                ::shiftDown = true;
            else if(event.toggle.state == ETOG_UP)
                ::shiftDown = false;
        }
        else if(event.toggle.id == DDKEY_RALT)
        {
            if(event.toggle.state == ETOG_DOWN)
            {
                ::altDown = true;
                //qDebug() << "Alt down";
            }
            else if(event.toggle.state == ETOG_UP)
            {
                ::altDown = false;
                //qDebug() << "Alt up";
            }
        }
    }

    // Update the state table.
    /// @todo Offer the event to each control in turn.
    if(event.type == E_AXIS)
    {
        dev->axis(event.axis.id).applyRealPosition(event.axis.pos);
    }
    else if(event.type == E_TOGGLE)
    {
        dev->button(event.toggle.id).setDown(event.toggle.state == ETOG_DOWN || event.toggle.state == ETOG_REPEAT);
    }
    else if(event.type == E_ANGLE)
    {
        dev->hat(event.angle.id).setPosition(event.angle.pos);
    }
}

void InputSystem::trackEvent(Event const &event)
{
    ddevent_t ddev;
    convertEvent(event, ddev);
    trackEvent(ddev);
}

void InputSystem::convertEvent(Event const &from, ddevent_t &to) // static
{
    de::zap(to);
    switch(from.type())
    {
    case Event::KeyPress:
    case Event::KeyRelease: {
        KeyEvent const &kev = from.as<KeyEvent>();

        to.device       = IDEV_KEYBOARD;
        to.type         = E_TOGGLE;
        to.toggle.id    = kev.ddKey();
        to.toggle.state = (kev.state() == KeyEvent::Pressed? ETOG_DOWN : ETOG_UP);
        strcpy(to.toggle.text, kev.text().toLatin1());
        break; }

    default: break;
    }
}

bool InputSystem::convertEvent(ddevent_t const &from, event_t &to) // static
{
    // Copy the essentials into a cutdown version for the game.
    // Ensure the format stays the same for future compatibility!
    //
    /// @todo This is probably broken! (DD_MICKEY_ACCURACY=1000 no longer used...)
    //
    de::zap(to);
    if(from.type == E_SYMBOLIC)
    {
        to.type = EV_SYMBOLIC;
#ifdef __64BIT__
        ASSERT_64BIT(from.symbolic.name);
        ev->data1 = (int)(((uint64_t) from.symbolic.name) & 0xffffffff); // low dword
        ev->data2 = (int)(((uint64_t) from.symbolic.name) >> 32); // high dword
#else
        ASSERT_NOT_64BIT(from.symbolic.name);
        to.data1 = (int) from.symbolic.name;
        to.data2 = 0;
#endif
    }
    else if(from.type == E_FOCUS)
    {
        to.type  = EV_FOCUS;
        to.data1 = from.focus.gained;
        to.data2 = from.focus.inWindow;
    }
    else
    {
        switch(from.device)
        {
        case IDEV_KEYBOARD:
            to.type = EV_KEY;
            if(from.type == E_TOGGLE)
            {
                to.state = (  from.toggle.state == ETOG_UP  ? EVS_UP
                            : from.toggle.state == ETOG_DOWN? EVS_DOWN
                            : EVS_REPEAT );
                to.data1 = from.toggle.id;
            }
            break;

        case IDEV_MOUSE:
            if(from.type == E_AXIS)
            {
                to.type = EV_MOUSE_AXIS;
            }
            else if(from.type == E_TOGGLE)
            {
                to.type  = EV_MOUSE_BUTTON;
                to.data1 = from.toggle.id;
                to.state = (  from.toggle.state == ETOG_UP  ? EVS_UP
                            : from.toggle.state == ETOG_DOWN? EVS_DOWN
                            : EVS_REPEAT );
            }
            break;

        case IDEV_JOY1:
        case IDEV_JOY2:
        case IDEV_JOY3:
        case IDEV_JOY4:
            if(from.type == E_AXIS)
            {
                int *data = &to.data1;

                to.type  = EV_JOY_AXIS;
                to.state = (evstate_t) 0;
                if(from.axis.id >= 0 && from.axis.id < 6)
                {
                    data[from.axis.id] = from.axis.pos;
                }
                /// @todo  The other dataN's must contain up-to-date information
                /// as well. Read them from the current joystick status.
            }
            else if(from.type == E_TOGGLE)
            {
                to.type  = EV_JOY_BUTTON;
                to.state = (  from.toggle.state == ETOG_UP  ? EVS_UP
                            : from.toggle.state == ETOG_DOWN? EVS_DOWN
                            : EVS_REPEAT );
                to.data1 = from.toggle.id;
            }
            else if(from.type == E_ANGLE)
            {
                to.type = EV_POV;
            }
            break;

        case IDEV_HEAD_TRACKER:
            // No game-side equivalent exists.
            return false;

        default:
            DENG2_ASSERT(!"InputSystem::convertEvent: Unknown device ID in ddevent_t");
            return false;
        }
    }
    return true;
}

bool InputSystem::shiftDown() const
{
    return ::shiftDown;
}

void InputSystem::initialContextActivations()
{
    // Deactivate all contexts.
    for(BindContext *context : d->contexts) context->deactivate();

    // These are the contexts active by default.
    context(GLOBAL_BINDING_CONTEXT_NAME ).activate();
    context(DEFAULT_BINDING_CONTEXT_NAME).activate();

    /*
    if(Con_IsActive())
    {
        context(CONSOLE_BINDING_CONTEXT_NAME).activate();
    }
    */
}

void InputSystem::clearAllContexts()
{
    if(d->contexts.isEmpty()) return;

    qDeleteAll(d->contexts);
    d->contexts.clear();
}

int InputSystem::contextCount() const
{
    return d->contexts.count();
}

bool InputSystem::hasContext(String const &name) const
{
    return contextPtr(name) != nullptr;
}

BindContext &InputSystem::context(String const &name) const
{
    if(BindContext *context = contextPtr(name))
    {
        return *context;
    }
    /// @throw MissingContextError  Specified name is unknown.
    throw MissingContextError("InputSystem::context", "Unknown name:" + name);
}

/// @todo: Optimize O(n) search...
BindContext *InputSystem::contextPtr(String const &name) const
{
    for(BindContext const *context : d->contexts)
    {
        if(!context->name().compareWithoutCase(name))
        {
            return const_cast<BindContext *>(context);
        }
    }
    return nullptr;
}

BindContext &InputSystem::contextAt(int position) const
{
    if(position >= 0 && position < d->contexts.count())
    {
        return *d->contexts.at(position);
    }
    /// @throw MissingContextError  Specified position is invalid.
    throw MissingContextError("InputSystem::contextAt", "Invalid position:" + String::number(position));
}

int InputSystem::contextPositionOf(BindContext *context) const
{
    return (context? d->contexts.indexOf(context) : -1);
}

BindContext *InputSystem::newContext(String const &name)
{
    BindContext *context = new BindContext(name);
    d->contexts.prepend(context);

    context->audienceForActiveChange()        += d;
    context->audienceForAcquireDeviceChange() += d;
    context->audienceForBindingAddition()     += d;

    return context;
}

LoopResult InputSystem::forAllContexts(std::function<LoopResult (BindContext &)> func) const
{
    for(BindContext *context : d->contexts)
    {
        if(auto result = func(*context)) return result;
    }
    return LoopContinue;
}

static char const *parseContext(char const *desc, String &context)
{
    if(!desc || !strchr(desc, ':'))
    {
        // No context defined.
        context = "";
        return desc;
    }

    AutoStr *str = AutoStr_NewStd();
    desc = Str_CopyDelim(str, desc, ':');
    context = Str_Text(str);

    return desc;
}

void InputSystem::writeAllBindingsTo(FILE *file)
{
    DENG2_ASSERT(file);
    LOG_AS("InputSystem");

    // Start with a clean slate when restoring the bindings.
    fprintf(file, "clearbindings\n\n");

    for(BindContext const *context : d->contexts)
    {
        // Commands.
        context->forAllCommandBindings([this, &context, &file] (CommandBinding &bind)
        {
            fprintf(file, "bindevent \"%s:%s\" \"", context->name().toUtf8().constData(),
                           composeBindsFor(bind).toUtf8().constData());
            M_WriteTextEsc(file, bind.command.toUtf8().constData());
            fprintf(file, "\"\n");
            return LoopContinue;
        });

        // Impulses.
        context->forAllImpulseBindings([this, &context, &file] (ImpulseBinding &bind)
        {
            PlayerImpulse const *impulse = P_ImpulseById(bind.impulseId);
            DENG2_ASSERT(impulse);

            fprintf(file, "bindcontrol local%i-%s \"%s\"\n",
                          bind.localPlayer + 1, impulse->name.toUtf8().constData(),
                          composeBindsFor(bind).toUtf8().constData());
            return LoopContinue;
        });
    }
}

// ---------------------------------------------------------------------------

CommandBinding *InputSystem::bindCommand(char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc && command);
    LOG_AS("InputSystem");

    // The event descriptor may begin with a symbolic binding context name.
    String contextName;
    eventDesc = parseContext(eventDesc, contextName);

    if(BindContext *context = contextPtr(contextName.isEmpty()? DEFAULT_BINDING_CONTEXT_NAME : contextName))
    {
        return context->bindCommand(eventDesc, command);
    }
    return nullptr;
}

ImpulseBinding *InputSystem::bindImpulse(char const *ctrlDesc, char const *impulseDesc)
{
    DENG2_ASSERT(ctrlDesc && impulseDesc);
    LOG_AS("InputSystem");

    // The impulse descriptor may begin with the local player number.
    int localNum    = 0;
    AutoStr *str    = AutoStr_NewStd();
    char const *ptr = Str_CopyDelim(str, impulseDesc, '-');
    if(!strncasecmp(Str_Text(str), "local", 5) && Str_Length(str) > 5)
    {
        localNum = strtoul(Str_Text(str) + 5, nullptr, 10) - 1;
        if(localNum < 0 || localNum >= DDMAXPLAYERS)
        {
            LOG_INPUT_WARNING("Local player number %i is invalid") << localNum;
            return nullptr;
        }

        // Skip past it.
        impulseDesc = ptr;
    }

    // The next part must be the impulse name.
    impulseDesc = Str_CopyDelim(str, impulseDesc, '-');
    PlayerImpulse const *impulse = P_ImpulseByName(Str_Text(str));
    if(!impulse)
    {
        LOG_INPUT_WARNING("Player impulse \"%s\" not defined") << Str_Text(str);
        return nullptr;
    }

    BindContext *context = contextPtr(impulse->bindContextName);
    DENG2_ASSERT(context); // Should be known by now?
    if(!context)
    {
        context = contextPtr(DEFAULT_BINDING_CONTEXT_NAME);
    }
    DENG2_ASSERT(context);

    return context->bindImpulse(ctrlDesc, *impulse, localNum);
}

/// @todo: Don't format a string - just collect pointers.
static int getCommandBindings(String const &command, char *outBuf, size_t outBufSize)
{
    DENG2_ASSERT(outBuf);
    if(command.isEmpty()) return 0;
    if(!outBufSize) return 0;

    InputSystem &isys = ClientApp::inputSystem();

    String out;
    int found = 0;
    isys.forAllContexts([&] (BindContext &context)
    {
        context.forAllCommandBindings([&] (CommandBinding &bind)
        {
            if(!bind.command.compareWithCase(command))
            {
                if(found) out += " ";

                out += String::number(bind.id) + "@" + context.name() + ":" + isys.composeBindsFor(bind);
                found++;
            }
            return LoopContinue;
        });
        return LoopContinue;
    });

    // Copy the result to the return buffer.
    std::memset(outBuf, 0, outBufSize);
    strncpy(outBuf, out.toUtf8().constData(), outBufSize - 1);

    return found;
}

/// @todo: Don't format a string - just collect pointers.
static int getImpulseBindings(int localPlayer, char const *impulseName, int inverse,
    char *outBuf, size_t outBufSize)
{
    DENG2_ASSERT(impulseName && outBuf);
    InputSystem &isys = ClientApp::inputSystem();

    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return 0;

    if(!outBufSize) return 0;

    String out;
    int found = 0;
    isys.forAllContexts([&] (BindContext &context)
    {
        context.forAllImpulseBindings(localPlayer, [&] (ImpulseBinding &bind)
        {
            DENG2_ASSERT(bind.localPlayer == localPlayer);

            PlayerImpulse const *impulse = P_ImpulseById(bind.impulseId);
            DENG2_ASSERT(impulse);

            if(!impulse->name.compareWithoutCase(impulseName))
            {
                if(inverse == BFCI_BOTH ||
                   (inverse == BFCI_ONLY_NON_INVERSE && !(bind.flags & IBDF_INVERSE)) ||
                   (inverse == BFCI_ONLY_INVERSE     &&  (bind.flags & IBDF_INVERSE)))
                {
                    if(found) out += " ";

                    out += String::number(bind.id) + "@" + context.name() + ":" + isys.composeBindsFor(bind);
                    found++;
                }
            }
            return LoopContinue;
        });
        return LoopContinue;
    });

    // Copy the result to the return buffer.
    std::memset(outBuf, 0, outBufSize);
    strncpy(outBuf, out.toUtf8().constData(), outBufSize - 1);

    return found;
}

/**
 * Parse the main part of the event descriptor, with no conditions included.
 */
static bool configureCommandBinding(CommandBinding &b, char const *eventDesc, char const *command)
{
    DENG2_ASSERT(eventDesc);
    InputSystem &isys = ClientApp::inputSystem();

    // Take a copy of the commands string.
    b.command = command;

    // Parse the event descriptor.
    AutoStr *str = AutoStr_NewStd();

    // First, we expect to encounter a device name.
    eventDesc = Str_CopyDelim(str, eventDesc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        b.deviceId = IDEV_KEYBOARD;
        b.type     = E_TOGGLE;      // Keyboards only have toggles (as far as we know).

        // Parse the key.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        if(!B_ParseKeyId(Str_Text(str), &b.controlId))
        {
            return false;
        }

        // The final part of a key event is the state of the key toggle.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        if(!B_ParseToggleState(Str_Text(str), &b.state))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        b.deviceId = IDEV_MOUSE;

        // Next comes a button or axis name.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        if(!B_ParseMouseTypeAndId(Str_Text(str), &b.type, &b.controlId))
        {
            return false;
        }

        // The last part determines the toggle state or the axis position.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        if(b.type == E_TOGGLE)
        {
            if(!B_ParseToggleState(Str_Text(str), &b.state))
            {
                return false;
            }
        }
        else // Axis position.
        {
            if(!B_ParseAxisPosition(Str_Text(str), &b.state, &b.pos))
            {
                return false;
            }
        }
    }
    else if(!Str_CompareIgnoreCase(str, "joy") ||
            !Str_CompareIgnoreCase(str, "head"))
    {
        b.deviceId = (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER);

        // Next part defined button, axis, or hat.
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        if(!B_ParseJoystickTypeAndId(isys.device(b.deviceId), Str_Text(str), &b.type, &b.controlId))
        {
            return false;
        }

        // What is the state of the toggle, axis, or hat?
        eventDesc = Str_CopyDelim(str, eventDesc, '-');
        if(b.type == E_TOGGLE)
        {
            if(!B_ParseToggleState(Str_Text(str), &b.state))
            {
                return false;
            }
        }
        else if(b.type == E_AXIS)
        {
            if(!B_ParseAxisPosition(Str_Text(str), &b.state, &b.pos))
            {
                return false;
            }
        }
        else // Angle.
        {
            if(!B_ParseAnglePosition(Str_Text(str), &b.pos))
            {
                return false;
            }
        }
    }
    else if(!Str_CompareIgnoreCase(str, "sym"))
    {
        // A symbolic event.
        b.type         = E_SYMBOLIC;
        b.deviceId     = 0;
        b.symbolicName = strdup(eventDesc);
        eventDesc = nullptr;
    }
    else
    {
        LOG_INPUT_WARNING("Unknown device \"%s\"") << Str_Text(str);
        return false;
    }

    // Anything left that wasn't used?
    if(eventDesc)
    {
        LOG_INPUT_WARNING("Unrecognized \"%s\"") << eventDesc;
        return false;
    }

    // No errors detected.
    return true;
}

static bool configureImpulseBinding(ImpulseBinding &b, char const *ctrlDesc,
    int impulseId, int localPlayer)
{
    DENG2_ASSERT(ctrlDesc);
    InputSystem &isys = ClientApp::inputSystem();

    b.impulseId   = impulseId;
    b.localPlayer = localPlayer;

    // Parse the control descriptor.
    AutoStr *str = AutoStr_NewStd();

    // First, the device name.
    ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        b.deviceId = IDEV_KEYBOARD;
        b.type     = IBD_TOGGLE;

        // Parse the key.
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
        if(!B_ParseKeyId(Str_Text(str), &b.controlId))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        b.deviceId = IDEV_MOUSE;

        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
        ddeventtype_t type;
        if(!B_ParseMouseTypeAndId(Str_Text(str), &type, &b.controlId))
        {
            return false;
        }
        b.type = EVTYPE_TO_IBDTYPE(type);
    }
    else if(!Str_CompareIgnoreCase(str, "joy") ||
            !Str_CompareIgnoreCase(str, "head"))
    {
        b.deviceId = (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER);

        // Next part defined button, axis, or hat.
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
        ddeventtype_t type;
        if(!B_ParseJoystickTypeAndId(isys.device(b.deviceId), Str_Text(str), &type, &b.controlId))
        {
            return false;
        }
        b.type = EVTYPE_TO_IBDTYPE(type);

        // Hats include the angle.
        if(type == E_ANGLE)
        {
            ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
            if(!B_ParseAnglePosition(Str_Text(str), &b.angle))
            {
                return false;
            }
        }
    }

    // Finally, there may be some flags at the end.
    while(ctrlDesc)
    {
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
        if(!Str_CompareIgnoreCase(str, "inverse"))
        {
            b.flags |= IBDF_INVERSE;
        }
        else if(!Str_CompareIgnoreCase(str, "staged"))
        {
            b.flags |= IBDF_TIME_STAGED;
        }
        else
        {
            LOG_INPUT_WARNING("Unrecognized \"%s\"") << ctrlDesc;
            return false;
        }
    }

    return true;
}

void InputSystem::configure(CommandBinding &bind, char const *eventDesc,
    char const *command, bool newId)
{
    DENG2_ASSERT(eventDesc);
    LOG_AS("InputSystem");

    // The first part specifies the event condition.
    AutoStr *str = AutoStr_NewStd();
    eventDesc = Str_CopyDelim(str, eventDesc, '+');

    if(!configureCommandBinding(bind, Str_Text(str), command))
    {
        throw BindError("InputSystem::configure", "Descriptor parse error");
    }

    // Any conditions?
    while(eventDesc)
    {
        // A new condition.
        eventDesc = Str_CopyDelim(str, eventDesc, '+');

        bind.conditions.append(statecondition_t());
        statecondition_t *cond = &bind.conditions.last();
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            throw BindError("InputSystem::configure", "Descriptor parse error");
        }
    }

    // Assign a new binding ID?
    if(newId) bind.id = B_NewIdentifier();
}

void InputSystem::configure(ImpulseBinding &bind, char const *ctrlDesc, int impulseId,
    int localPlayer, bool newId)
{
    DENG2_ASSERT(ctrlDesc);
    DENG2_ASSERT(localPlayer >= 0 && localPlayer < DDMAXPLAYERS);
    LOG_AS("InputSystem");

    // The first part specifies the device-control condition.
    AutoStr *str = AutoStr_NewStd();
    ctrlDesc = Str_CopyDelim(str, ctrlDesc, '+');

    if(!configureImpulseBinding(bind, Str_Text(str), impulseId, localPlayer))
    {
        throw BindError("InputSystem::configure", "Descriptor parse error");
    }

    // Any conditions?
    while(ctrlDesc)
    {
        // A new condition.
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '+');

        bind.conditions.append(statecondition_t());
        statecondition_t *cond = &bind.conditions.last();
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            throw BindError("InputSystem::configure", "Descriptor parse error");
        }
    }

    // Assign a new binding ID?
    if(newId) bind.id = B_NewIdentifier();
}

String InputSystem::composeBindsFor(CommandBinding const &bind)
{
    LOG_AS("InputSystem");
    String str = B_ControlDescToString(device(bind.deviceId), bind.type, bind.controlId);

    switch(bind.type)
    {
    case E_TOGGLE:      str += B_ToggleStateToString(bind.state);            break;
    case E_AXIS:        str += B_AxisPositionToString(bind.state, bind.pos); break;
    case E_ANGLE:       str += B_AnglePositionToString(bind.pos);            break;
    case E_SYMBOLIC:    str += "-" + String(bind.symbolicName);              break;

    default: DENG2_ASSERT(!"InputSystem::composeBindsFor: Unknown bind.type"); break;
    }

    // Append any state conditions.
    for(statecondition_t const &cond : bind.conditions)
    {
        str += " + " + B_StateConditionToString(cond);
    }

    return str;
}

String InputSystem::composeBindsFor(ImpulseBinding const &bind)
{
    LOG_AS("InputSystem");
    String str = B_ControlDescToString(device(bind.deviceId), IBDTYPE_TO_EVTYPE(bind.type), bind.controlId);

    if(bind.type == IBD_ANGLE)
    {
        str += B_AnglePositionToString(bind.angle);
    }

    // Additional flags.
    if(bind.flags & IBDF_TIME_STAGED)
    {
        str += "-staged";
    }
    if(bind.flags & IBDF_INVERSE)
    {
        str += "-inverse";
    }

    // Append any state conditions.
    for(statecondition_t const &cond : bind.conditions)
    {
        str += " + " + B_StateConditionToString(cond);
    }

    return str;
}

bool InputSystem::removeBinding(int id)
{
    LOG_AS("InputSystem");
    for(BindContext *context : d->contexts)
    {
        if(bool result = context->deleteBinding(id)) return result;
    }
    return false;
}

D_CMD(ListDevices)
{
    DENG2_UNUSED3(src, argc, argv);
    InputSystem &isys = ClientApp::inputSystem();

    LOG_INPUT_MSG(_E(b) "%i input devices initalized:") << isys.deviceCount();
    isys.forAllDevices([] (InputDevice &device)
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

D_CMD(ListContexts)
{
    DENG2_UNUSED3(src, argc, argv);
    InputSystem &isys = ClientApp::inputSystem();

    LOG_INPUT_MSG(_E(b) "%i binding contexts defined:") << isys.contextCount();
    int idx = 0;
    isys.forAllContexts([&idx] (BindContext &context)
    {
        LOG_INPUT_MSG("  [%2i] " _E(>) "%s%s" _E(.) _E(2) " (%s)")
                << (idx++) << (context.isActive()? _E(b) : _E(w))
                << context.name()
                << (context.isActive()? "active" : "inactive");
        return LoopContinue;
    });
    return true;
}

/*
D_CMD(ClearContexts)
{
    ClientApp::inputSystem().clearAllContexts();
    return true;
}
*/

D_CMD(ActivateContext)
{
    DENG2_UNUSED2(src, argc);
    InputSystem &isys = ClientApp::inputSystem();

    bool const doActivate = !stricmp(argv[0], "activatebcontext");
    String const name     = argv[1];

    if(!isys.hasContext(name))
    {
        LOG_INPUT_WARNING("Unknown binding context '%s'") << name;
        return false;
    }

    BindContext &context = isys.context(name);
    if(context.isProtected())
    {
        LOG_INPUT_ERROR("Binding context " _E(b) "'%s'" _E(.) " is " _E(2) "protected" _E(.)
                        " and cannot be manually %s")
                << context.name() << (doActivate? "activated" : "deactivated");
        return false;
    }

    context.activate(doActivate);
    return true;
}

D_CMD(BindCommand)
{
    DENG2_UNUSED2(src, argc);
    if(CommandBinding *bind = ClientApp::inputSystem().bindCommand(argv[1], argv[2]))
    {
        LOG_INPUT_VERBOSE("Binding " _E(b) "%i" _E(.) " created") << bind->id;
        return true;
    }
    return false;
}

D_CMD(BindImpulse)
{
    DENG2_UNUSED2(src, argc);
    if(ImpulseBinding *bind = ClientApp::inputSystem().bindImpulse(argv[2], argv[1]))
    {
        LOG_INPUT_VERBOSE("Binding " _E(b) "%i" _E(.) " created") << bind->id;
        return true;
    }
    return false;
}

D_CMD(ListBindings)
{
    DENG2_UNUSED3(src, argc, argv);
    InputSystem &isys = ClientApp::inputSystem();

    int totalBindCount = 0;
    isys.forAllContexts([&totalBindCount] (BindContext &context)
    {
        totalBindCount += context.commandBindingCount() + context.impulseBindingCount();
        return LoopContinue;
    });

    LOG_INPUT_MSG(_E(b) "Bindings");
    LOG_INPUT_MSG("There are " _E(b) "%i" _E(.) " bindings, in " _E(b) "%i" _E(.) " contexts")
            << totalBindCount << isys.contextCount();

    isys.forAllContexts([&isys] (BindContext &context)
    {
        int const cmdCount = context.commandBindingCount();
        int const impCount = context.impulseBindingCount();

        // Skip empty contexts.
        if(cmdCount + impCount == 0) return LoopContinue;

        LOG_INPUT_MSG(_E(D)_E(b) "%s" _E(.) " context: " _E(l) "(%i %s)")
                << context.name()
                << (cmdCount + impCount)
                << (context.isActive()? "active" : "inactive");

        // Commands.
        if(cmdCount)
        {
            LOG_INPUT_MSG("  " _E(b) "%i command bindings:") << cmdCount;
        }
        context.forAllCommandBindings([&isys] (CommandBinding &bind)
        {
            LOG_INPUT_MSG("    [%3i] " _E(>) _E(b) "%s" _E(.) " %s")
                    << bind.id << isys.composeBindsFor(bind) << bind.command;
            return LoopContinue;
        });

        // Impulses.
        if(impCount)
        {
            LOG_INPUT_MSG("  " _E(b) "%i impulse bindings:") << impCount;
        }
        for(int pl = 0; pl < DDMAXPLAYERS; ++pl)
        context.forAllImpulseBindings(pl, [&isys, &pl] (ImpulseBinding &bind)
        {
            PlayerImpulse const *impulse = P_ImpulseById(bind.impulseId);
            DENG2_ASSERT(impulse);

            LOG_INPUT_MSG("    [%3i] " _E(>) _E(b) "%s" _E(.) " player%i %s")
                    << bind.id << isys.composeBindsFor(bind) << (pl + 1) << impulse->name;

            return LoopContinue;
        });

        return LoopContinue;
    });

    return true;
}

D_CMD(ClearBindings)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientApp::inputSystem().forAllContexts([] (BindContext &context)
    {
        LOG_INPUT_VERBOSE("Clearing binding context " _E(b) "'%s'") << context.name();
        context.clearAllBindings();
        return LoopContinue;
    });

    // We can restart the id counter, all the old bindings were destroyed.
    B_ResetIdentifiers();
    return true;
}

D_CMD(RemoveBinding)
{
    DENG2_UNUSED2(src, argc);

    int id = strtoul(argv[1], nullptr, 10);
    if(ClientApp::inputSystem().removeBinding(id))
    {
        LOG_INPUT_MSG("Binding " _E(b) "%i" _E(.) " deleted") << id;
    }
    else
    {
        LOG_INPUT_ERROR("Unknown binding #%i") << id;
    }

    return true;
}

D_CMD(DefaultBindings)
{
    DENG2_UNUSED3(src, argc, argv);

    B_BindDefaults();
    B_BindGameDefaults();

    return true;
}

void InputSystem::consoleRegister() // static
{
#define PROTECTED_FLAGS     (CMDF_NO_DEDICATED | CMDF_DED | CMDF_CLIENT)

    // Variables:
    C_VAR_BYTE("input-conflict-zerocontrol", &zeroControlUponConflict, 0, 0, 1);
    C_VAR_BYTE("input-sharp",                &useSharpInputEvents, 0, 0, 1);

    // Commands:
    C_CMD_FLAGS("activatebcontext",     "s",        ActivateContext,    PROTECTED_FLAGS);
    C_CMD_FLAGS("bindevent",            "ss",       BindCommand,        PROTECTED_FLAGS);
    C_CMD_FLAGS("bindcontrol",          "ss",       BindImpulse,        PROTECTED_FLAGS);
    //C_CMD_FLAGS("clearbcontexts",       "",         ClearContexts,      PROTECTED_FLAGS);
    C_CMD_FLAGS("clearbindings",        "",         ClearBindings,      PROTECTED_FLAGS);
    C_CMD_FLAGS("deactivatebcontext",   "s",        ActivateContext,    PROTECTED_FLAGS);
    C_CMD_FLAGS("defaultbindings",      "",         DefaultBindings,    PROTECTED_FLAGS);
    C_CMD_FLAGS("delbind",              "i",        RemoveBinding,      PROTECTED_FLAGS);
    C_CMD_FLAGS("listbcontexts",        nullptr,    ListContexts,       PROTECTED_FLAGS);
    C_CMD_FLAGS("listbindings",         nullptr,    ListBindings,       PROTECTED_FLAGS);
    C_CMD      ("listinputdevices",     "",         ListDevices);
    C_CMD      ("releasemouse",         "",         ReleaseMouse);
    //C_CMD_FLAGS("setaxis",            "s",        AxisPrintConfig,  CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis",            "ss",       AxisChangeOption, CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis",            "sss",      AxisChangeValue,  CMDF_NO_DEDICATED);

    I_DebugDrawerConsoleRegister();

#undef PROTECTED_FLAGS
}

DENG_EXTERN_C void B_SetContextFallback(char const *name, int (*responderFunc)(event_t *))
{
    InputSystem &isys = ClientApp::inputSystem();
    if(isys.hasContext(name))
    {
        isys.context(name).setFallbackResponder(responderFunc);
    }
}

DENG_EXTERN_C int B_BindingsForCommand(char const *command, char *outBuf, size_t outBufSize)
{
    DENG2_ASSERT(command && outBuf);
    return getCommandBindings(command, outBuf, outBufSize);
}

DENG_EXTERN_C int B_BindingsForControl(int localPlayer, char const *impulseName, int inverse,
    char *outBuf, size_t outBufSize)
{
    DENG2_ASSERT(impulseName && outBuf);
    return getImpulseBindings(localPlayer, impulseName, inverse, outBuf, outBufSize);
}

DENG_EXTERN_C int DD_GetKeyCode(char const *key)
{
    DENG2_ASSERT(key);
    int code = B_KeyForShortName(key);
    return (code ? code : key[0]);
}

DENG_DECLARE_API(B) =
{
    { DE_API_BINDING },
    B_SetContextFallback,
    B_BindingsForCommand,
    B_BindingsForControl,
    DD_GetKeyCode
};
