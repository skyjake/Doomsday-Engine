/** @file inputdevice.h  Logical input device.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_INPUTSYSTEM_INPUTDEVICE_H
#define CLIENT_INPUTSYSTEM_INPUTDEVICE_H

#include <functional>
#include <de/error.h>
#include <de/observers.h>
#include <de/string.h>

class BindContext;
class AxisInputControl;
class ButtonInputControl;
class HatInputControl;

/**
 * Base class for modelling a "physical" input device.
 *
 * @ingroup ui
 */
class InputDevice
{
public:
    /// Referenced control is missing. @ingroup errors
    DE_ERROR(MissingControlError);

    /// Notified when the active state of the device changes.
    DE_AUDIENCE(ActiveChange, void inputDeviceActiveChanged(InputDevice &device))

    /**
     * Base class for all controls.
     * @todo Attribute a GUID, to simplify bookkeeping. -ds
     */
    class Control : public de::Lockable
    {
    public:
        /// No InputDevice is associated with the control. @ingroup errors
        DE_ERROR(MissingDeviceError);

        /**
         * How the control state relates to binding contexts.
         */
        enum BindContextAssociationFlag {
            /// The state has expired. The control is considered to remain in
            /// default state until the flag gets cleared (which happens when
            /// the real control state returns to its default).
            Expired      = 0x1,

            /// The state has been triggered. This is cleared when someone checks
            /// the control state. (Only for buttons).
            Triggered    = 0x2,

            DefaultFlags = 0
        };
        using BindContextAssociation = de::Flags;

    public:
        explicit Control(InputDevice *device = nullptr);
        virtual ~Control();

        DE_CAST_METHODS()

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
        void setName(const de::String &newName);

        /**
         * Compose the full symbolic name of the control including the device name
         * (if one is attributed), for example:
         *
         * @code
         * <device-name>-<name> => "mouse-x"
         * @endcode
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
         * Returns the BindContext attributed to the control; otherwise @c nullptr.
         *
         * @see hasBindContext(), setBindContext()
         */
        BindContext *bindContext() const;

        /**
         * Returns @c true of a BindContext is attributed to the control.
         *
         * @see bindContext(), setBindContext()
         */
        inline bool hasBindContext() const { return bindContext() != nullptr; }

        /**
         * Change the attributed BindContext to @a newContext.
         *
         * @param newContext  BindContext to attribute. Ownership is unaffected.
         *
         * @see hasBindContext(), bindContext()
         */
        void setBindContext(BindContext *newContext);

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
        void setBindContextAssociation(const BindContextAssociation &flagsToChange,
                                       de::FlagOp op = de::SetFlags);

        void clearBindContextAssociation();
        void expireBindContextAssociationIfChanged();

        /**
         * Register the console commands and variables of the control.
         */
        virtual void consoleRegister() {}

    private:
        DE_PRIVATE(d)
    };

public:
    /**
     * @note InputDevices are not @em active by default. Call @ref activate() once
     * device configuration has been completed.
     *
     * @param name  Symbolic name of the device.
     */
    InputDevice(const de::String &name);
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
    void setTitle(const de::String &newTitle);

    /**
     * Returns information about the device as styled text.
     */
    virtual de::String description() const;

    /**
     * Reset the state of all controls to their "initial" positions (i.e., buttons
     * in the up positions, axes at center, etc...).
     */
    void reset();

    /**
     * Iterate through all the controls of the device.
     */
    de::LoopResult forAllControls(const std::function<de::LoopResult (Control &)>& func);

    /**
     * Translate a symbolic axis @a name to the associated unique axis id.
     *
     * @return  Index of the named axis control if found, otherwise @c -1.
     */
    int toAxisId(const de::String &name) const;

    /**
     * Returns @c true if @a id is a known axis control.
     */
    bool hasAxis(int id) const;

    /**
     * Lookup an axis control by unique @a id.
     *
     * @param id  Unique id of the axis control.
     *
     * @return  Axis control associated with the given @a id.
     */
    AxisInputControl &axis(int id) const;

    /**
     * Add an @a axis control to the input device.
     *
     * @param axis  Axis control to add. Ownership is given to the device.
     */
    void addAxis(AxisInputControl *axis);

    /**
     * Returns the number of axis controls of the device.
     */
    int axisCount() const;

    /**
     * Translate a symbolic key @a name to the associated unique key id.
     *
     * @return  Index of the named key control if found, otherwise @c -1.
     */
    int toButtonId(const de::String &name) const;

    /**
     * Returns @c true if @a id is a known button control.
     */
    bool hasButton(int id) const;

    /**
     * Lookup a button control by unique @a id.
     *
     * @param id  Unique id of the button control.
     *
     * @return  Button control associated with the given @a id.
     */
    ButtonInputControl &button(int id) const;

    /**
     * Add a @a button control to the input device.
     *
     * @param button  Button control to add. Ownership is given to the device.
     */
    void addButton(ButtonInputControl *button);

    /**
     * Returns the number of button controls of the device.
     */
    int buttonCount() const;

    /**
     * Returns @c true if @a id is a known hat control.
     */
    bool hasHat(int id) const;

    /**
     * Lookup a hat control by unique @a id.
     *
     * @param id  Unique id of the hat control.
     *
     * @return  Hat control associated with the given @a id.
     */
    HatInputControl &hat(int id) const;

    /**
     * Add a @a hat control to the input device.
     *
     * @param hat  Hat control to add. Ownership is given to the device.
     */
    void addHat(HatInputControl *hat);

    /**
     * Returns the number of hat controls of the device.
     */
    int hatCount() const;

    /**
     * Register the console commands and variables for this device and all controls.
     */
    void consoleRegister();

private:
    DE_PRIVATE(d)
};

typedef InputDevice::Control InputControl;

#endif // CLIENT_INPUTSYSTEM_INPUTDEVICE_H
