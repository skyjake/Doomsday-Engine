/** @file inputdevice.h  Logical input device.
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

#ifndef CLIENT_INPUTSYSTEM_INPUTDEVICE_H
#define CLIENT_INPUTSYSTEM_INPUTDEVICE_H

#include <functional>
#include <QFlags>
#include <de/Error>
#include <de/Observers>
#include <de/String>

class BindContext;

/// @todo remove:
class InputDeviceAxisControl;
class InputDeviceButtonControl;
class InputDeviceHatControl;
/// end todo

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

    /// Notified when the active state of the device changes.
    DENG2_DEFINE_AUDIENCE2(ActiveChange, void inputDeviceActiveChanged(InputDevice &device))

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

    /**
     * Add a @a button control to the input device.
     *
     * @param button  Button control to add. Ownership is given to the device.
     */
    void addButton(InputDeviceButtonControl *button);

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

    /**
     * Add a @a hat control to the input device.
     *
     * @param hat  Hat control to add. Ownership is given to the device.
     */
    void addHat(InputDeviceHatControl *hat);

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

#endif // CLIENT_INPUTSYSTEM_INPUTDEVICE_H
