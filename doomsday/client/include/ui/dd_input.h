/** @file dd_input.h  Input Subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_CORE_INPUT_H
#define CLIENT_CORE_INPUT_H

#include <functional>
#include <QFlags>
#include <de/smoother.h>
#include <de/Error>
#include <de/Event>
#include <de/String>

#include "api_event.h"

struct bcontext_t;

class InputDeviceAxisControl;
class InputDeviceButtonControl;
class InputDeviceHatControl;

/**
 * Base class for modelling a "physical" input device.
 *
 * @ingroup ui
 */
class InputDevice
{
public:
    /// Referenced control is missing. @ingroup errors
    DENG2_ERROR(MissingControlError);

    /**
     * Base class for all controls.
     */
    class Control
    {
    public:
        /// No InputDevice is associated with the control. @ingroup errors
        DENG2_ERROR(MissingDeviceError);

        /**
         * How the control state relates to binding contexts.
         */
        enum BindContextAssociationFlag {
            /// The state has expired. The control is considered to remain in default
            /// state until the flag gets cleared (which happens when the real control
            /// state returns to its default).
            Expired      = 0x1,

            /// The state has been triggered. This is cleared when someone checks
            /// the control state. (Only for toggles).
            Triggered    = 0x2,

            DefaultFlags = 0
        };
        Q_DECLARE_FLAGS(BindContextAssociation, BindContextAssociationFlag)

    public:
        explicit Control(InputDevice *device = nullptr);
        virtual ~Control();

        DENG2_AS_IS_METHODS()

        /**
         * Returns @c true if the control is presently in its default state.
         * (e.g., button is not pressed, axis is at center, etc...).
         */
        virtual bool inDefaultState() const = 0;

        /**
         * Reset the control back to its default state. Note that any attributed
         * property values (name, device and binding association) are unaffected.
         *
         * The default implementation does nothing.
         */
        virtual void reset() {}

        /**
         * Returns the symbolic name of the control.
         */
        de::String name() const;

        /**
         * Change the symbolic name of the control to @a newName.
         */
        void setName(de::String const &newName);

        /**
         * Compose the full symbolic name of the control including the device name
         * (if one is attributed), for example:
         *
         * <device-name>-<name> => "mouse-x"
         */
        de::String fullName() const;

        /**
         * Returns information about the control as styled text.
         */
        virtual de::String description() const = 0;

        /**
         * Returns the InputDevice attributed to the control.
         *
         * @see hasDevice(), setDevice()
         */
        InputDevice &device() const;

        /**
         * Returns @c true if an InputDevice is attributed to the control.
         *
         * @see device(), setDevice()
         */
        bool hasDevice() const;

        /**
         * Change the attributed InputDevice to @a newDevice.
         *
         * @param newDevice  InputDevice to attribute. Ownership is unaffected.
         *
         * @see hasDevice(), device()
         */
        void setDevice(InputDevice *newDevice);

        /**
         * Returns the bcontext_t attributed to the control; otherwise @c nullptr.
         *
         * @see hasBindContext(), setBindContext()
         */
        bcontext_t *bindContext() const;

        /**
         * Returns @c true of a bcontext_t is attributed to the control.
         *
         * @see bindContext(), setBindContext()
         */
        inline bool hasBindContext() const { return bindContext() != nullptr; }

        /**
         * Change the attributed bcontext_t to @a newContext.
         *
         * @param newContext  bcontext_t to attribute. Ownership is unaffected.
         *
         * @see hasBindContext(), bindContext()
         */
        void setBindContext(bcontext_t *newContext);

        /**
         * Returns the BindContextAssociation flags for the control.
         */
        BindContextAssociation bindContextAssociation() const;

        /**
         * Change the BindContextAssociation flags for the control.
         *
         * @param flagsToChange  Association flags to change.
         * @param op             Logical operation to perform.
         */
        void setBindContextAssociation(BindContextAssociation const &flagsToChange,
                                       de::FlagOp op = de::SetFlags);

        void clearBindContextAssociation();
        void expireBindContextAssociationIfChanged();

        /**
         * Register the console commands and variables of the control.
         */
        virtual void consoleRegister() {}

    private:
        DENG2_PRIVATE(d)
    };

public:
    /**
     * @note InputDevices are not @em active by default. Call @ref activate() once
     * device configuration has been completed.
     *
     * @param name  Symbolic name of the device.
     */
    InputDevice(de::String const &name);
    virtual ~InputDevice();

    /**
     * Returns @c true if the device is presently active.
     * @todo Document "active" status.
     */
    bool isActive() const;

    /**
     * Change the active status of this device.
     *
     * @see isActive()
     */
    void activate(bool yes = true);
    inline void deactivate() { activate(false); }

    /**
     * Returns the symbolic name of the device.
     */
    de::String name() const;

    /**
     * Returns the title of the device, intended for human-readable descriptions.
     *
     * @see setTitle()
     */
    de::String title() const;

    /**
     * Change the title of the device, intended for human-readable descriptions,
     * to @a newTitle.
     */
    void setTitle(de::String const &newTitle);

    /**
     * Returns information about the device as styled text.
     */
    de::String description() const;

    /**
     * Reset the state of all controls to their "initial" positions (i.e., buttons
     * in the up positions, axes at center, etc...).
     */
    void reset();

    /**
     * Iterate through all the controls of the device.
     */
    de::LoopResult forAllControls(std::function<de::LoopResult (Control &)> func);

    /**
     * Translate a symbolic axis @a name to the associated unique axis id.
     *
     * @return  Index of the named axis control if found, otherwise @c -1.
     */
    de::dint toAxisId(de::String const &name) const;

    /**
     * Returns @c true if @a id is a known axis control.
     */
    bool hasAxis(de::dint id) const;

    /**
     * Lookup an axis control by unique @a id.
     *
     * @param id  Unique id of the axis control.
     *
     * @return  Axis control associated with the given @id.
     */
    InputDeviceAxisControl &axis(de::dint id) const;

    /**
     * Add an @a axis control to the input device.
     *
     * @param axis  Axis control to add. Ownership is given to the device.
     */
    void addAxis(InputDeviceAxisControl *axis);

    /**
     * Returns the number of axis controls of the device.
     */
    de::dint axisCount() const;

    /**
     * Translate a symbolic key @a name to the associated unique key id.
     *
     * @return  Index of the named key control if found, otherwise @c -1.
     */
    de::dint toButtonId(de::String const &name) const;

    /**
     * Returns @c true if @a id is a known button control.
     */
    bool hasButton(de::dint id) const;

    /**
     * Lookup a button control by unique @a id.
     *
     * @param id  Unique id of the button control.
     *
     * @return  Button control associated with the given @id.
     */
    InputDeviceButtonControl &button(de::dint id) const;

    void initButtons(de::dint count);

    /**
     * Returns the number of button controls of the device.
     */
    de::dint buttonCount() const;

    /**
     * Returns @c true if @a id is a known hat control.
     */
    bool hasHat(de::dint id) const;

    /**
     * Lookup a hat control by unique @a id.
     *
     * @param id  Unique id of the hat control.
     *
     * @return  Hat control associated with the given @id.
     */
    InputDeviceHatControl &hat(de::dint id) const;

    void initHats(de::dint count);

    /**
     * Returns the number of hat controls of the device.
     */
    de::dint hatCount() const;

    /**
     * Register the console commands and variables for this device and all controls.
     */
    void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(InputDevice::Control::BindContextAssociation)

typedef InputDevice::Control InputDeviceControl;

// Input device axis flags for use with cvars:
#define IDA_DISABLED    0x1  ///< Axis is always zero.
#define IDA_INVERT      0x2  ///< Real input data should be inverted.
#define IDA_RAW         0x4  ///< Do not smooth the input values; always use latest received value.

/**
 * Models an axis control on a "physical" input device (e.g., mouse along one axis).
 *
 * @ingroup ui
 */
class InputDeviceAxisControl : public InputDeviceControl
{
public:
    enum Type {
        Stick,   ///< Joysticks, gamepads
        Pointer  ///< Mouse
    };

public:
    /**
     * @param name  Symbolic name of the axis.
     * @param type  Logical axis type.
     */
    InputDeviceAxisControl(de::String const &name, Type type);
    virtual ~InputDeviceAxisControl();

    Type type() const;
    void setRawInput(bool yes = true);

    bool isActive() const;
    bool isInverted() const;

    /**
     * Returns the current position of the axis.
     */
    de::ddouble position() const;
    void setPosition(de::ddouble newPosition);

    /**
     * Update the position of the axis control from a "real" position.
     *
     * @param newPosition  New position to be applied (maybe filtered, normalized, etc...).
     */
    void applyRealPosition(de::dfloat newPosition);

    de::dfloat translateRealPosition(de::dfloat rawPosition) const;

    /**
     * Returns the current dead zone (0..1) limit for the axis.
     */
    de::dfloat deadZone() const;
    void setDeadZone(de::dfloat newDeadZone);

    /**
     * Returns the current position scaling factor (applied to "real" positions).
     */
    de::dfloat scale() const;
    void setScale(de::dfloat newScale);

    /**
     * When the state of the control last changed, in milliseconds since app init.
     */
    de::duint time() const;

    void update(timespan_t ticLength);

    de::String description() const;
    bool inDefaultState() const;
    void reset();
    void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

/**
 * Models a button control on a "physical" input device (e.g., key on a keyboard).
 *
 * @ingroup ui
 */
class InputDeviceButtonControl : public InputDeviceControl
{
public:
    explicit InputDeviceButtonControl(de::String const &name = "");
    virtual ~InputDeviceButtonControl();

    /**
     * Returns @c true if the button is currently in the down (i.e., pressed) state.
     */
    bool isDown() const;

    /**
     * Change the "down" state of the button.
     */
    void setDown(bool yes);

    /**
     * When the state of the control last changed, in milliseconds since app init.
     */
    de::duint time() const;

    de::String description() const;
    bool inDefaultState() const;
    void reset();

private:
    bool _isDown = false; ///< @c true= currently depressed.
    de::duint _time = 0;
};

/**
 * Models a hat control on a "physical" input device (such as that found on joysticks).
 *
 * @ingroup ui
 */
class InputDeviceHatControl : public InputDeviceControl
{
public:
    explicit InputDeviceHatControl(de::String const &name = "");
    virtual ~InputDeviceHatControl();

    /**
     * Returns the current position of the hat.
     */
    de::dint position() const;

    /**
     * @param newPosition  @c -1= centered.
     */
    void setPosition(de::dint newPosition);

    /**
     * When the state of the control last changed, in milliseconds since app init.
     */
    de::duint time() const;

    de::String description() const;
    bool inDefaultState() const;

private:
    de::dint _pos   = -1;  ///< Current position. @c -1= centered.
    de::duint _time = 0;   ///< Timestamp of the latest change.
};

// -----------------------------------------------------------------------------------

// Input device identifiers:
enum
{
    IDEV_KEYBOARD,
    IDEV_MOUSE,
    IDEV_JOY1,
    IDEV_JOY2,
    IDEV_JOY3,
    IDEV_JOY4,
    IDEV_HEAD_TRACKER,
    NUM_INPUT_DEVICES  ///< Theoretical maximum.
};

enum ddeventtype_t
{
    E_TOGGLE,    ///< Two-state device
    E_AXIS,      ///< Axis position
    E_ANGLE,     ///< Hat angle
    E_SYMBOLIC,  ///< Symbolic event
    E_FOCUS      ///< Window focus
};

enum ddevent_togglestate_t
{
    ETOG_DOWN,
    ETOG_UP,
    ETOG_REPEAT
};

enum ddevent_axistype_t
{
    EAXIS_ABSOLUTE,  ///< Absolute position on the axis
    EAXIS_RELATIVE   ///< Offset relative to the previous position
};

/**
 * Internal input event.
 *
 * These are used internally, a cutdown version containing
 * only need-to-know metadata is sent down the games' responder chain.
 *
 * @todo Replace with a de::Event-derived class.
 */
struct ddevent_t
{
    uint device;         ///< e.g. IDEV_KEYBOARD
    ddeventtype_t type;  ///< E_TOGGLE, E_AXIS, E_ANGLE, or E_SYMBOLIC
    union {
        struct {
            int id;                       ///< Button/key index number
            ddevent_togglestate_t state;  ///< State of the toggle
            char text[8];                 ///< For characters, latin1-encoded text to insert (or empty).
        } toggle;
        struct {
            int id;                       ///< Axis index number
            float pos;                    ///< Position of the axis
            ddevent_axistype_t type;      ///< Type of the axis (absolute or relative)
        } axis;
        struct {
            int id;                       ///< Angle index number
            float pos;                    ///< Angle, or negative if centered
        } angle;
        struct {
            int id;                       ///< Console that originated the event.
            char const *name;             ///< Symbolic name of the event.
        } symbolic;
        struct {
            dd_bool gained;                ///< Gained or lost focus.
            int inWindow;                 ///< Window where the focus change occurred (index).
        } focus;
    };
};

// Convenience macros.
#define IS_TOGGLE_DOWN(evp)            ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_DOWN)
#define IS_TOGGLE_DOWN_ID(evp, togid)  ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_DOWN && (evp)->toggle.id == togid)
#define IS_TOGGLE_UP(evp)              ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_UP)
#define IS_TOGGLE_REPEAT(evp)          ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_REPEAT)
#define IS_KEY_TOGGLE(evp)             ((evp)->device == IDEV_KEYBOARD && (evp)->type == E_TOGGLE)
#define IS_KEY_DOWN(evp)               ((evp)->device == IDEV_KEYBOARD && (evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_DOWN)
#define IS_KEY_PRESS(evp)              ((evp)->device == IDEV_KEYBOARD && (evp)->type == E_TOGGLE && (evp)->toggle.state != ETOG_UP)
#define IS_MOUSE_DOWN(evp)             ((evp)->device == IDEV_MOUSE && IS_TOGGLE_DOWN(evp))
#define IS_MOUSE_UP(evp)               ((evp)->device == IDEV_MOUSE && IS_TOGGLE_UP(evp))
#define IS_MOUSE_MOTION(evp)           ((evp)->device == IDEV_MOUSE && (evp)->type == E_AXIS)

void I_ConsoleRegister();

/**
 * Initialize the virtual input devices.
 *
 * @note There need not be actual physical devices available in order to use
 * these state tables.
 */
void I_InitAllDevices();

/**
 * Free the memory allocated for the input devices.
 */
void I_ShutdownAllDevices();

void I_ResetAllDevices();

void I_ClearAllDeviceContextAssociations();

/**
 * Lookup an InputDevice by it's unique @a id.
 */
InputDevice &I_Device(int id);

/**
 * Lookup an InputDevice by it's unique @a id.
 *
 * @return  Pointer to the associated InputDevice; otherwise @c nullptr.
 */
InputDevice *I_DevicePtr(int id);

/**
 * Iterate through all the InputDevices.
 */
de::LoopResult I_ForAllDevices(std::function<de::LoopResult (InputDevice &)> func);

/**
 * Initializes the key mappings to the default values.
 */
void I_InitKeyMappings();

/**
 * Checks the current keyboard state, generates input events based on pressed/held
 * keys and posts them.
 */
void I_ReadKeyboard();

/**
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void I_ReadMouse();

/**
 * Checks the current joystick state (axis, sliders, hat and buttons).
 * Generates events and posts them. Axis clamps and dead zone is done
 * here.
 */
void I_ReadJoystick();

void I_ReadHeadTracker();

/**
 * Clear the input event queue.
 */
void I_ClearEvents();

bool I_IgnoreEvents(bool yes = true);

/**
 * Process all incoming input for the given timestamp.
 * This is called only in the main thread, and also from the busy loop.
 *
 * This gets called at least 35 times per second. Usually more frequently
 * than that.
 */
void I_ProcessEvents(timespan_t ticLength);

void I_ProcessSharpEvents(timespan_t ticLength);

/**
 * @param ev  A copy is made.
 */
void I_PostEvent(ddevent_t *ev);

bool I_ConvertEvent(ddevent_t const *ddEvent, event_t *ev);

/**
 * Converts a libcore Event into an old-fashioned ddevent_t.
 *
 * @param event    Event instance.
 * @param ddEvent  ddevent_t instance.
 */
void I_ConvertEvent(de::Event const &event, ddevent_t *ddEvent);

/**
 * Update the input device state table.
 */
void I_TrackInput(ddevent_t *ev);

bool I_ShiftDown();

#ifdef DENG2_DEBUG
/**
 * Render a visual representation of the current state of all input devices.
 */
void Rend_DrawInputDeviceVisuals();
#else
#  define Rend_DrawInputDeviceVisuals()
#endif

#endif // CLIENT_CORE_INPUT_H
