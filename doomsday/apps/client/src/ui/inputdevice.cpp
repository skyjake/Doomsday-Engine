/** @file inputdevice.cpp  Logical input device.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "ui/inputdevice.h"
#include "ui/joystick.h"
#include "ui/axisinputcontrol.h"
#include "ui/buttoninputcontrol.h"
#include "ui/hatinputcontrol.h"
#include <de/list.h>
#include <de/log.h>

using namespace de;

DE_PIMPL(InputDevice)
{
    bool active = false;  ///< Initially inactive.
    String title;         ///< Human-friendly title.
    String name;          ///< Symbolic name.

    List<  AxisInputControl *> axes;
    List<ButtonInputControl *> buttons;
    List<   HatInputControl *> hats;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        deleteAll(hats);
        deleteAll(buttons);
        deleteAll(axes);
    }

    DE_PIMPL_AUDIENCE(ActiveChange)
};

DE_AUDIENCE_METHOD(InputDevice, ActiveChange)

InputDevice::InputDevice(const String &name) : d(new Impl(this))
{
    DE_ASSERT(!name.isEmpty());
    d->name = name;
}

InputDevice::~InputDevice()
{}

bool InputDevice::isActive() const
{
    return d->active;
}

void InputDevice::activate(bool yes)
{
    if (d->active != yes)
    {
        d->active = yes;

        // Notify interested parties.
        DE_NOTIFY(ActiveChange, i) i->inputDeviceActiveChanged(*this);
    }
}

String InputDevice::name() const
{
    return d->name;
}

String InputDevice::title() const
{
    return (d->title.isEmpty()? d->name : d->title);
}

void InputDevice::setTitle(const String &newTitle)
{
    d->title = newTitle;
}

String InputDevice::description() const
{
    String desc;
    if (!d->title.isEmpty())
    {
        desc += Stringf(_E(D)_E(b) "%s" _E(.)_E(.) " - ", d->title.c_str());
    }
    desc += Stringf(
        _E(b) "%s" _E(.) _E(l) " (%s)" _E(.), name().c_str(), isActive() ? "active" : "inactive");

    if (const int count = axisCount())
    {
        desc += Stringf("\n  " _E(b) "%i axes:" _E(.), count);
        int idx = 0;
        for (const Control *axis : d->axes)
        {
            desc += Stringf("\n    [%3i] " _E(>) "%s" _E(<), idx++,
                                   axis->description().c_str());
        }
    }

    if (const int count = buttonCount())
    {
        desc += Stringf("\n  " _E(b) "%i buttons:" _E(.), count);
        int idx = 0;
        for (const Control *button : d->buttons)
        {
            desc += Stringf("\n    [%03i] " _E(>) "%s" _E(<), idx++,
                                   button->description().c_str());
        }
    }

    if (const int count = hatCount())
    {
        desc += Stringf("\n  " _E(b) "%i hats:" _E(.), count);
        int idx = 0;
        for (const Control *hat : d->hats)
        {
            desc += Stringf("\n    [%3i] " _E(>) "%s" _E(<), idx++,
                                   hat->description().c_str());
        }
    }

    return desc;
}

void InputDevice::reset()
{
    LOG_AS("InputDevice");
    for (Control *axis : d->axes)
    {
        axis->reset();
    }
    for (Control *button : d->buttons)
    {
        button->reset();
    }
    for (Control *hat : d->hats)
    {
        hat->reset();
    }

    if (!d->name.compareWithCase("key"))
    {
        extern bool shiftDown, altDown;

        altDown = shiftDown = false;
    }
    LOG_INPUT_VERBOSE(_E(b) "'%s'" _E(.) " controls reset") << title();
}

LoopResult InputDevice::forAllControls(const std::function<de::LoopResult (Control &)>& func)
{
    for (Control *axis : d->axes)
    {
        if (auto result = func(*axis)) return result;
    }
    for (Control *button : d->buttons)
    {
        if (auto result = func(*button)) return result;
    }
    for (Control *hat : d->hats)
    {
        if (auto result = func(*hat)) return result;
    }
    return LoopContinue;
}

dint InputDevice::toAxisId(const String &name) const
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < d->axes.count(); ++i)
        {
            if (!d->axes.at(i)->name().compareWithoutCase(name))
                return i;
        }
    }
    return -1;
}

dint InputDevice::toButtonId(const String &name) const
{
    if (!name.isEmpty())
    {
        for (int i = 0; i < d->buttons.count(); ++i)
        {
            if (!d->buttons.at(i)->name().compareWithoutCase(name))
                return i;
        }
    }
    return -1;
}

bool InputDevice::hasAxis(int id) const
{
    return (id >= 0 && id < d->axes.count());
}

AxisInputControl &InputDevice::axis(dint id) const
{
    if (hasAxis(id)) return *d->axes.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::axis", "Invalid id:" + String::asText(id));
}

void InputDevice::addAxis(AxisInputControl *axis)
{
    if (!axis) return;
    d->axes.append(axis);
    axis->setDevice(this);
}

int InputDevice::axisCount() const
{
    return d->axes.count();
}

bool InputDevice::hasButton(int id) const
{
    return (id >= 0 && id < d->buttons.count());
}

ButtonInputControl &InputDevice::button(dint id) const
{
    if (hasButton(id)) return *d->buttons.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::button", "Invalid id:" + String::asText(id));
}

void InputDevice::addButton(ButtonInputControl *button)
{
    if (!button) return;
    d->buttons.append(button);
    button->setDevice(this);
}

int InputDevice::buttonCount() const
{
    return d->buttons.count();
}

bool InputDevice::hasHat(int id) const
{
    return (id >= 0 && id < d->hats.count());
}

HatInputControl &InputDevice::hat(dint id) const
{
    if (hasHat(id)) return *d->hats.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::hat", "Invalid id:" + String::asText(id));
}

void InputDevice::addHat(HatInputControl *hat)
{
    if (!hat) return;
    d->hats.append(hat);
    hat->setDevice(this);
}

int InputDevice::hatCount() const
{
    return d->hats.count();
}

void InputDevice::consoleRegister()
{
    for (Control *axis : d->axes)
    {
        axis->consoleRegister();
    }
    for (Control *button : d->buttons)
    {
        button->consoleRegister();
    }
    for (Control *hat : d->hats)
    {
        hat->consoleRegister();
    }
}

//---------------------------------------------------------------------------------------

DE_PIMPL_NOREF(InputDevice::Control)
{
    String name;  ///< Symbolic
    InputDevice *device = nullptr;
    BindContextAssociation flags = DefaultFlags;
    BindContext *bindContext     = nullptr;
    BindContext *prevBindContext = nullptr;
};

InputDevice::Control::Control(InputDevice *device) : d(new Impl)
{
    setDevice(device);
}

InputDevice::Control::~Control()
{}

String InputDevice::Control::name() const
{
    DE_GUARD(this);
    return d->name;
}

void InputDevice::Control::setName(const String &newName)
{
    DE_GUARD(this);
    d->name = newName;
}

String InputDevice::Control::fullName() const
{
    DE_GUARD(this);
    String desc;
    if (hasDevice()) desc += device().name() + "-";
    desc += (d->name.isEmpty()? "<unnamed>" : d->name);
    return desc;
}

InputDevice &InputDevice::Control::device() const
{
    DE_GUARD(this);
    if (d->device) return *d->device;
    /// @throw MissingDeviceError  Missing InputDevice attribution.
    throw MissingDeviceError("InputDevice::Control::device", "No InputDevice is attributed");
}

bool InputDevice::Control::hasDevice() const
{
    DE_GUARD(this);
    return d->device != nullptr;
}

void InputDevice::Control::setDevice(InputDevice *newDevice)
{
    DE_GUARD(this);
    d->device = newDevice;
}

BindContext *InputDevice::Control::bindContext() const
{
    DE_GUARD(this);
    return d->bindContext;
}

void InputDevice::Control::setBindContext(BindContext *newContext)
{
    DE_GUARD(this);
    d->bindContext = newContext;
}

InputDevice::Control::BindContextAssociation InputDevice::Control::bindContextAssociation() const
{
    DE_GUARD(this);
    return d->flags;
}

void InputDevice::Control::setBindContextAssociation(const BindContextAssociation &flagsToChange, FlagOp op)
{
    DE_GUARD(this);
    applyFlagOperation(d->flags, flagsToChange, op);
}

void InputDevice::Control::clearBindContextAssociation()
{
    DE_GUARD(this);
    d->prevBindContext = d->bindContext;
    d->bindContext     = nullptr;
    applyFlagOperation(d->flags, Triggered, UnsetFlags);
}

void InputDevice::Control::expireBindContextAssociationIfChanged()
{
    DE_GUARD(this);

    // No change?
    if (d->bindContext == d->prevBindContext) return;

    // No longer valid.
    applyFlagOperation(d->flags, Expired, SetFlags);
    applyFlagOperation(d->flags, Triggered, UnsetFlags); // Not any more.
}
