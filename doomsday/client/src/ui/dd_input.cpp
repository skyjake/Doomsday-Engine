/** @file dd_input.cpp  Platform-independent input subsystem.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_platform.h" // strdup macro
#include "ui/dd_input.h"

#include <QList>
#include <QtAlgorithms>
#include <de/concurrency.h>
#include <de/ddstring.h>
#include <de/memory.h>
#include <de/smoother.h>
#include <de/timer.h> // SECONDSPERTIC
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <de/Block>
#include <de/KeyEvent>

#include "dd_def.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "sys_system.h" // novideo

#include "render/vr.h"

#include "ui/b_main.h"
#include "ui/clientwindow.h"
#include "ui/clientwindowsystem.h"
#include "ui/joystick.h"
#include "ui/infine/finale.h"
#include "ui/sys_input.h"
#include "ui/ui_main.h"

// For the debug visuals:
#ifdef DENG2_DEBUG
#  include <de/point.h>
#  include "de_graphics.h"
#  include "api_fontrender.h"
#endif

using namespace de;

DENG2_PIMPL_NOREF(InputDeviceAxisControl)
{
    Type type = Pointer;
    dint flags = 0;

    ddouble position     = 0;      ///< Current translated position (-1..1) including any filtering.
    ddouble realPosition = 0;      ///< The actual latest position (-1..1).

    dfloat scale    = 1;           ///< Scaling factor for real input values.
    dfloat deadZone = 0;           ///< Dead zone in (0..1) range.

    ddouble sharpPosition = 0;     ///< Current sharp (accumulated) position, entered into the Smoother.
    Smoother *smoother = nullptr;  ///< Smoother for the input values (owned).
    ddouble prevSmoothPos = 0;     ///< Previous evaluated smooth position (needed for producing deltas).

    duint time = 0;                ///< Timestamp of the last position update.

    Instance()
    {
        Smoother_SetMaximumPastNowDelta(smoother = Smoother_New(), 2 * SECONDSPERTIC);
    }

    ~Instance()
    {
        Smoother_Delete(smoother);
    }

#if 0
    static float filter(int grade, float *accumulation, float ticLength)
    {
        DENG2_ASSERT(accumulation);
        int dir     = de::sign(*accumulation);
        float avail = fabs(*accumulation);
        // Determine the target velocity.
        float target = avail * (MAX_AXIS_FILTER - de::clamp(1, grade, 39));

        /*
        // test: clamp
        if(target < -.7) target = -.7;
        else if(target > .7) target = .7;
        else target = 0;
        */

        // Determine the amount of mickeys to send. It depends on the
        // current mouse velocity, and how much time has passed.
        float used = target * ticLength;

        // Don't go past the available motion.
        if(used > avail)
        {
            *accumulation = nullptr;
            used = avail;
        }
        else
        {
            if(*accumulation > nullptr)
                *accumulation -= used;
            else
                *accumulation += used;
        }

        // This is the new (filtered) axis position.
        return dir * used;
    }
#endif
};

InputDeviceAxisControl::InputDeviceAxisControl(String const &name, Type type) : d(new Instance)
{
    setName(name);
    d->type = type;
}

InputDeviceAxisControl::~InputDeviceAxisControl()
{}

InputDeviceAxisControl::Type InputDeviceAxisControl::type() const
{
    return d->type;
}

void InputDeviceAxisControl::setRawInput(bool yes)
{
    if(yes) d->flags |= IDA_RAW;
    else    d->flags &= ~IDA_RAW;
}

bool InputDeviceAxisControl::isActive() const
{
    return (d->flags & IDA_DISABLED) == 0;
}

bool InputDeviceAxisControl::isInverted() const
{
    return (d->flags & IDA_INVERT) != 0;
}

void InputDeviceAxisControl::update(timespan_t ticLength)
{
    Smoother_Advance(d->smoother, ticLength);

    if(d->type == Stick)
    {
        if(d->flags & IDA_RAW)
        {
            // The axis is supposed to be unfiltered.
            d->position = d->realPosition;
        }
        else
        {
            // Absolute positions are straightforward to evaluate.
            Smoother_EvaluateComponent(d->smoother, 0, &d->position);
        }
    }
    else if(d->type == Pointer)
    {
        if(d->flags & IDA_RAW)
        {
            // The axis is supposed to be unfiltered.
            d->position    += d->realPosition;
            d->realPosition = 0;
        }
        else
        {
            // Apply smoothing by converting back into a delta.
            coord_t smoothPos = d->prevSmoothPos;
            Smoother_EvaluateComponent(d->smoother, 0, &smoothPos);
            d->position     += smoothPos - d->prevSmoothPos;
            d->prevSmoothPos = smoothPos;
        }
    }

    // We can clear the expiration now that an updated value is available.
    setBindContextAssociation(Expired, UnsetFlags);
}

ddouble InputDeviceAxisControl::position() const
{
    return d->position;
}

void InputDeviceAxisControl::setPosition(ddouble newPosition)
{
    d->position = newPosition;
}

void InputDeviceAxisControl::applyRealPosition(dfloat pos)
{
    dfloat const oldRealPos  = d->realPosition;
    dfloat const transformed = translateRealPosition(pos);

    // The unfiltered position.
    d->realPosition = transformed;

    if(oldRealPos != d->realPosition)
    {
        // Mark down the time of the change.
        d->time = DD_LatestRunTicsStartTime();
    }

    if(d->type == Stick)
    {
        d->sharpPosition = d->realPosition;
    }
    else // Cumulative.
    {
        // Convert the delta to an absolute position for smoothing.
        d->sharpPosition += d->realPosition;
    }

    Smoother_AddPosXY(d->smoother, DD_LatestRunTicsStartTime(), d->sharpPosition, 0);
}

dfloat InputDeviceAxisControl::translateRealPosition(dfloat realPos) const
{
    // An inactive axis is always zero.
    if(!isActive()) return 0;

    // Apply scaling, deadzone and clamping.
    float outPos = realPos * d->scale;
    if(d->type == Stick) // Only stick axes are dead-zoned and clamped.
    {
        if(fabs(outPos) <= d->deadZone)
        {
            outPos = 0;
        }
        else
        {
            outPos -= d->deadZone * de::sign(outPos);  // Remove the dead zone.
            outPos *= 1.0f/(1.0f - d->deadZone);       // Normalize.
            outPos = de::clamp(-1.0f, outPos, 1.0f);
        }
    }

    if(isInverted())
    {
        // Invert the axis position.
        outPos = -outPos;
    }

    return outPos;
}

dfloat InputDeviceAxisControl::deadZone() const
{
    return d->deadZone;
}

void InputDeviceAxisControl::setDeadZone(dfloat newDeadZone)
{
    d->deadZone = newDeadZone;
}

dfloat InputDeviceAxisControl::scale() const
{
    return d->scale;
}

void InputDeviceAxisControl::setScale(dfloat newScale)
{
    d->scale = newScale;
}

duint InputDeviceAxisControl::time() const
{
    return d->time;
}

String InputDeviceAxisControl::description() const
{
    QStringList flags;
    if(!isActive()) flags << "disabled";
    if(isInverted()) flags << "inverted";
    String flagsAsText = flags.join("|");

    return String(_E(b) "%1 " _E(.) "(Axis-%2)\n"
                  //_E(l) "Filter: "    _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Dead Zone: " _E(.)_E(i) "%3\n" _E(.)
                  _E(l) "Scale: "     _E(.)_E(i) "%4\n" _E(.)
                  _E(l) "Flags: "     _E(.)_E(i) "%5")
               .arg(fullName())
               .arg(d->type == Stick? "Stick" : "Pointer")
               //.arg(d->filter)
               .arg(d->deadZone)
               .arg(d->scale)
               .arg(flagsAsText);
}

bool InputDeviceAxisControl::inDefaultState() const
{
    return d->position == 0; // Centered?
}

void InputDeviceAxisControl::reset()
{
    if(d->type == Pointer)
    {
        // Clear the accumulation.
        d->position      = 0;
        d->sharpPosition = 0;
        d->prevSmoothPos = 0;
    }
    Smoother_Clear(d->smoother);
}

void InputDeviceAxisControl::consoleRegister()
{
    DENG2_ASSERT(hasDevice() && !name().isEmpty());
    String controlName = String("input-%1-%2").arg(device().name()).arg(name());

    Block scale = (controlName + "-scale").toUtf8();
    C_VAR_FLOAT(scale.constData(), &d->scale, CVF_NO_MAX, 0, 0);

    Block flags = (controlName + "-flags").toUtf8();
    C_VAR_INT(flags.constData(), &d->flags, 0, 0, 7);

    if(d->type == Stick)
    {
        Block deadzone = (controlName + "-deadzone").toUtf8();
        C_VAR_FLOAT(deadzone.constData(), &d->deadZone, 0, 0, 1);
    }
}

InputDeviceButtonControl::InputDeviceButtonControl(String const &name)
{
    setName(name);
}

InputDeviceButtonControl::~InputDeviceButtonControl()
{}

bool InputDeviceButtonControl::isDown() const
{
    return _isDown;
}

void InputDeviceButtonControl::setDown(bool yes)
{
    bool const oldDown = _isDown;

    _isDown = yes;

    if(_isDown != oldDown)
    {
        // Remember when the change occurred.
        _time = Timer_RealMilliseconds();
    }

    if(_isDown)
    {
        // This will get cleared after the state is checked by someone.
        setBindContextAssociation(Triggered);
    }
    else
    {
        // We can clear the expiration when the key is released.
        setBindContextAssociation(Triggered, UnsetFlags);
    }

}

String InputDeviceButtonControl::description() const
{
    return String(_E(b) "%1 " _E(.) "(Button)").arg(fullName());
}

bool InputDeviceButtonControl::inDefaultState() const
{
    return !_isDown; // Not depressed?
}

void InputDeviceButtonControl::reset()
{
    if(_isDown)
    {
        setBindContextAssociation(Expired);
    }
    else
    {
        _isDown = false;
        _time   = 0;
        setBindContextAssociation(Triggered | Expired, UnsetFlags);
    }
}

duint InputDeviceButtonControl::time() const
{
    return _time;
}

InputDeviceHatControl::InputDeviceHatControl(String const &name)
{
    setName(name);
}

InputDeviceHatControl::~InputDeviceHatControl()
{}

dint InputDeviceHatControl::position() const
{
    return _pos;
}

void InputDeviceHatControl::setPosition(dint newPosition)
{
    _pos  = newPosition;
    _time = Timer_RealMilliseconds(); // Remember when the change occured.

    // We can clear the expiration when centered.
    if(_pos < 0)
    {
        setBindContextAssociation(Expired, UnsetFlags);
    }
}

duint InputDeviceHatControl::time() const
{
    return _time;
}

String InputDeviceHatControl::description() const
{
    return String(_E(b) "%1 " _E(.) "(Hat)").arg(fullName());
}

bool InputDeviceHatControl::inDefaultState() const
{
    return _pos < 0; // Centered?
}

// --------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(InputDevice::Control)
{
    String name;  ///< Symbolic
    InputDevice *device = nullptr;
    BindContextAssociation flags { DefaultFlags };
    bcontext_t *bindContext     = nullptr;
    bcontext_t *prevBindContext = nullptr;
};

InputDevice::Control::Control(InputDevice *device) : d(new Instance)
{
    setDevice(device);
}

InputDevice::Control::~Control()
{}

String InputDevice::Control::name() const
{
    return d->name;
}

void InputDevice::Control::setName(String const &newName)
{
    d->name = newName;
}

String InputDevice::Control::fullName() const
{
    String desc;
    if(hasDevice()) desc += device().name() + "-";
    desc += (d->name.isEmpty()? "<unnamed>" : d->name);
    return desc;
}

InputDevice &InputDevice::Control::device() const
{
    if(d->device) return *d->device;
    /// @throw MissingDeviceError  Missing InputDevice attribution.
    throw MissingDeviceError("InputDevice::Control::device", "No InputDevice is attributed");
}

bool InputDevice::Control::hasDevice() const
{
    return d->device != nullptr;
}

void InputDevice::Control::setDevice(InputDevice *newDevice)
{
    d->device = newDevice;
}

bcontext_t *InputDevice::Control::bindContext() const
{
    return d->bindContext;
}

void InputDevice::Control::setBindContext(bcontext_t *newContext)
{
    d->bindContext = newContext;
}

InputDevice::Control::BindContextAssociation InputDevice::Control::bindContextAssociation() const
{
    return d->flags;
}

void InputDevice::Control::setBindContextAssociation(BindContextAssociation const &flagsToChange, FlagOp op)
{
    applyFlagOperation(d->flags, flagsToChange, op);
}

void InputDevice::Control::clearBindContextAssociation()
{
    d->prevBindContext = d->bindContext;
    d->bindContext     = nullptr;
    setBindContextAssociation(Triggered, UnsetFlags);
}

void InputDevice::Control::expireBindContextAssociationIfChanged()
{
    // No change?
    if(d->bindContext == d->prevBindContext) return;

    // No longer valid.
    setBindContextAssociation(Expired);
    setBindContextAssociation(Triggered, UnsetFlags); // Not any more.
}

DENG2_PIMPL(InputDevice)
{
    bool active = false;  ///< Initially inactive.
    String title;         ///< Human-friendly title.
    String name;          ///< Symbolic name.

    typedef QList<InputDeviceAxisControl *> Axes;
    Axes axes;

    typedef QList<InputDeviceButtonControl *> Buttons;
    Buttons buttons;

    typedef QList<InputDeviceHatControl *> Hats;
    Hats hats;

    Instance(Public *i) : Base(i) {}

    ~Instance()
    {
        qDeleteAll(hats);
        qDeleteAll(buttons);
        qDeleteAll(axes);
    }
};

InputDevice::InputDevice(String const &name) : d(new Instance(this))
{
    DENG2_ASSERT(!name.isEmpty());
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
    d->active = yes;
}

String InputDevice::name() const
{
    return d->name;
}

String InputDevice::title() const
{
    return (d->title.isEmpty()? d->name : d->title);
}

void InputDevice::setTitle(String const &newTitle)
{
    d->title = newTitle;
}

String InputDevice::description() const
{
    String desc = String(_E(b) "%1").arg(name());
    if(!d->title.isEmpty())
    {
        desc += String(_E(.) " - " _E(b) "%1" _E(.)).arg(d->title);
    }
    desc += String(" (%1)").arg(isActive()? "active" : " inactive");

    if(axisCount())
    {
        desc += String("\n %1 axes:").arg(axisCount());
        int idx = 0;
        for(InputDeviceAxisControl *axis : d->axes)
        {
            desc += String("\n  %1: ").arg(idx++) + axis->description();
        }
    }

    if(buttonCount())
    {
        desc += String("\n %1 buttons:").arg(buttonCount());
        int idx = 0;
        for(InputDeviceButtonControl *button : d->buttons)
        {
            desc += String("\n  %1: ").arg(idx++) + button->description();
        }
    }

    if(hatCount())
    {
        desc += String("\n %1 hats:").arg(hatCount());
        int idx = 0;
        for(InputDeviceHatControl *hat : d->hats)
        {
            desc += String("\n  %1: ").arg(idx++) + hat->description();
        }
    }

    return desc;
}

void InputDevice::reset()
{
    LOG_AS("InputDevice");
    LOG_INPUT_VERBOSE("Reseting %s") << title();

    for(InputDeviceAxisControl *axis : d->axes)
    {
        axis->reset();
    }
    for(InputDeviceButtonControl *button : d->buttons)
    {
        button->reset();
    }
    for(InputDeviceHatControl *hat : d->hats)
    {
        hat->reset();
    }

    if(!d->name.compareWithCase("key"))
    {
        extern bool shiftDown, altDown;

        altDown = shiftDown = false;
    }
}

LoopResult InputDevice::forAllControls(std::function<de::LoopResult (Control &)> func)
{
    for(Control *axis : d->axes)
    {
        if(auto result = func(*axis)) return result;
    }
    for(Control *button : d->buttons)
    {
        if(auto result = func(*button)) return result;
    }
    for(Control *hat : d->hats)
    {
        if(auto result = func(*hat)) return result;
    }
    return LoopContinue;
}

void InputDevice::consoleRegister()
{
    for(InputDeviceAxisControl *axis : d->axes)
    {
        axis->consoleRegister();
    }
}

dint InputDevice::toAxisId(String const &name) const
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < d->axes.count(); ++i)
        {
            if(!d->axes.at(i)->name().compareWithoutCase(name))
                return i;
        }
    }
    return -1;
}

dint InputDevice::toButtonId(String const &name) const
{
    if(!name.isEmpty())
    {
        for(int i = 0; i < d->buttons.count(); ++i)
        {
            if(!d->buttons.at(i)->name().compareWithoutCase(name))
                return i;
        }
    }
    return -1;
}

bool InputDevice::hasAxis(de::dint id) const
{
    return (id >= 0 && id < d->axes.count());
}

InputDeviceAxisControl &InputDevice::axis(dint id) const
{
    if(hasAxis(id)) return *d->axes.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::axis", "Invalid id:" + String::number(id));
}

void InputDevice::addAxis(InputDeviceAxisControl *axis)
{
    if(!axis) return;
    d->axes.append(axis);
    axis->setDevice(this);
}

int InputDevice::axisCount() const
{
    return d->axes.count();
}

bool InputDevice::hasButton(de::dint id) const
{
    return (id >= 0 && id < d->buttons.count());
}

InputDeviceButtonControl &InputDevice::button(dint id) const
{
    if(hasButton(id)) return *d->buttons.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::button", "Invalid id:" + String::number(id));
}

void InputDevice::addButton(InputDeviceButtonControl *button)
{
    if(!button) return;
    d->buttons.append(button);
    button->setDevice(this);
}

int InputDevice::buttonCount() const
{
    return d->buttons.count();
}

bool InputDevice::hasHat(de::dint id) const
{
    return (id >= 0 && id < d->hats.count());
}

InputDeviceHatControl &InputDevice::hat(dint id) const
{
    if(hasHat(id)) return *d->hats.at(id);
    /// @throw MissingControlError  The given id is invalid.
    throw MissingControlError("InputDevice::hat", "Invalid id:" + String::number(id));
}

void InputDevice::addHat(InputDeviceHatControl *hat)
{
    if(!hat) return;
    d->hats.append(hat);
    hat->setDevice(this);
}

int InputDevice::hatCount() const
{
    return d->hats.count();
}

// -------------------------------------------------------------------------

#define DEFAULT_JOYSTICK_DEADZONE  .05f  ///< 5%

#define MAX_AXIS_FILTER  40

// The initial and secondary repeater delays (tics).
//int repWait1 = 15;
//int repWait2 = 3;
bool shiftDown;
bool altDown;

static bool ignoreInput;
#ifdef OLD_FILTER
static uint mouseFreq;
#endif

typedef QList<InputDevice *> Devices;
static Devices devices;

#if 0
struct repeater_t
{
    int key;           ///< The DDKEY code (@c 0 if not in use).
    int native;        ///< Used to determine which key is repeating.
    char text[8];      ///< Text to insert.
    timespan_t timer;  ///< How's the time?
    int count;         ///< How many times has been repeated?
};
#endif

struct eventqueue_t
{
    ddevent_t events[MAXEVENTS];
    int head;
    int tail;
};
static eventqueue_t queue;
static eventqueue_t sharpQueue;

#define MAX_KEYMAPPINGS  256
static byte altKeyMappings[MAX_KEYMAPPINGS];
static byte shiftKeyMappings[MAX_KEYMAPPINGS];

static char defaultShiftTable[96] = // Contains characters 32 to 127.
{
/* 32 */    ' ', 0, 0, 0, 0, 0, 0, '"',
/* 40 */    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
/* 50 */    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
/* 60 */    0, '+', 0, 0, 0, 'a', 'b', 'c', 'd', 'e',
/* 70 */    'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 80 */    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
/* 90 */    'z', '{', '|', '}', 0, 0, '~', 'A', 'B', 'C',
/* 100 */   'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
/* 110 */   'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
/* 120 */   'X', 'Y', 'Z', 0, 0, 0, 0, 0
};

static float oldPOV = IJOY_POV_CENTER;

static char *eventStrings[MAXEVENTS];

static byte useSharpInputEvents = true; ///< cvar

#ifdef DENG2_DEBUG
static byte devRendKeyState;   ///< cvar
static byte devRendMouseState; ///< cvar
static byte devRendJoyState;   ///< cvar
#endif

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

/**
 * @param device  InputDevice to add.
 * @return  Same as @a device for caller convenience.
 */
static InputDevice *addDevice(InputDevice *device)
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
                    throw Error("InputSystem::addInputDevice", "Multiple devices with name:" + device->name() + " cannot coexist");
                }
            }

            // Add this device to the collection.
            devices.append(device);
        }
    }
    return device;
}

void I_InitAllDevices()
{
    // Allow re-init.
    I_ShutdownAllDevices();

    addDevice(makeKeyboard("key", "Keyboard"))->activate(); // A keyboard is assumed to always be present.

    addDevice(makeMouse("mouse", "Mouse"))->activate(Mouse_IsPresent()); // A mouse may not be present.

    addDevice(makeJoystick("joy", "Joystick"))->activate(Joystick_IsPresent()); // A joystick may not be present.

    /// @todo: Add support for multiple joysticks (just some generics, for now).
    addDevice(new InputDevice("joy2"));
    addDevice(new InputDevice("joy3"));
    addDevice(new InputDevice("joy4"));

    addDevice(makeHeadTracker("head", "Head Tracker")); // Head trackers are activated later.

    // Register console variables for the controls of all devices.
    for(InputDevice *device : devices)
    {
        device->consoleRegister();
    }
}

void I_ShutdownAllDevices()
{
    qDeleteAll(devices);
    devices.clear();
}

InputDevice &I_Device(int id)
{
    if(id >= 0 && id < devices.count())
    {
        return *devices.at(id);
    }
    throw Error("I_InputDevice", "Unknown id:" + String::number(id));
}

InputDevice *I_DevicePtr(int id)
{
    if(id >= 0 && id < devices.count())
    {
        return devices.at(id);
    }
    return nullptr;
}

LoopResult I_ForAllDevices(std::function<LoopResult (InputDevice &)> func)
{
    for(InputDevice *device : devices)
    {
        if(auto result = func(*device)) return result;
    }
    return LoopContinue;
}

bool I_ShiftDown()
{
    return shiftDown;
}

void I_TrackInput(ddevent_t *ev)
{
    DENG2_ASSERT(ev);

    if(ev->type == E_FOCUS || ev->type == E_SYMBOLIC)
        return; // Not a tracked device state.

    InputDevice *dev = I_DevicePtr(ev->device);
    if(!dev || !dev->isActive()) return;

    // Track the state of Shift and Alt.
    if(IS_KEY_TOGGLE(ev))
    {
        if(ev->toggle.id == DDKEY_RSHIFT)
        {
            if(ev->toggle.state == ETOG_DOWN)
                shiftDown = true;
            else if(ev->toggle.state == ETOG_UP)
                shiftDown = false;
        }
        else if(ev->toggle.id == DDKEY_RALT)
        {
            if(ev->toggle.state == ETOG_DOWN)
            {
                altDown = true;
                //qDebug() << "Alt down";
            }
            else if(ev->toggle.state == ETOG_UP)
            {
                altDown = false;
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

#if 0
static int DD_KeyOrCode(char *token)
{
    char *end = M_FindWhite(token);

    if(end - token > 1)
    {
        // Longer than one character, it must be a number.
        return strtol(token, 0, !strnicmp(token, "0x", 2) ? 16 : 10);
    }
    // Direct mapping.
    return (unsigned char) *token;
}
#endif

void I_InitKeyMappings()
{
    for(int i = 0; i < 256; ++i)
    {
        if(i >= 32 && i <= 127)
            shiftKeyMappings[i] = defaultShiftTable[i - 32] ? defaultShiftTable[i - 32] : i;
        else
            shiftKeyMappings[i] = i;
        altKeyMappings[i] = i;
    }
}

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

void DD_ClearEventStrings()
{
    for(int i = 0; i < MAXEVENTS; ++i)
    {
        M_Free(eventStrings[i]); eventStrings[i] = nullptr;
    }
}

static void clearQueue(eventqueue_t *q)
{
    DENG2_ASSERT(q);
    q->head = q->tail;
}

/**
 * Poll all event sources (i.e., input devices) and post events.
 */
static void postEventsFromInputDevices()
{
#ifdef __CLIENT__
    // On the client may have have input devices.
    I_ReadKeyboard();
    I_ReadMouse();
    I_ReadJoystick();
    I_ReadHeadTracker();
#endif
}

bool I_IgnoreEvents(bool yes)
{
    bool const oldIgnoreInput = ignoreInput;

    ignoreInput = yes;
    LOG_INPUT_VERBOSE("Ignoring input: %b") << yes;
    if(!yes)
    {
        // Clear all the event buffers.
        postEventsFromInputDevices();
        I_ClearEvents();
    }
    return oldIgnoreInput;
}

void I_ClearEvents()
{
    clearQueue(&queue);
    clearQueue(&sharpQueue);

    DD_ClearEventStrings();
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

/// @note Called by the I/O functions when input is detected.
void I_PostEvent(ddevent_t *ev)
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

void I_ConvertEvent(de::Event const &event, ddevent_t *ddEvent)
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

bool I_ConvertEvent(ddevent_t const *ddEvent, event_t *ev)
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
            App_Error("DD_ProcessEvents: Unknown deviceID in ddevent_t");
#endif
            return false;
        }
    }
    return true;
}

static void updateAllDeviceAxes(timespan_t ticLength)
{
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

/**
 * Send all the events of the given timestamp down the responder chain.
 */
static void dispatchEvents(eventqueue_t *q, timespan_t ticLength, dd_bool updateAxes)
{
    dd_bool const callGameResponders = App_GameLoaded();

    ddevent_t *ddev;
    while((ddev = nextFromQueue(q)))
    {
        // Update the state of the input device tracking table.
        I_TrackInput(ddev);

        if(ignoreInput && ddev->type != E_FOCUS)
            continue;

        event_t ev;
        bool validGameEvent = I_ConvertEvent(ddev, &ev);

        if(validGameEvent && callGameResponders)
        {
            // Does the game's special responder use this event? This is
            // intended for grabbing events when creating bindings in the
            // Controls menu.
            if(gx.PrivilegedResponder && gx.PrivilegedResponder(&ev))
                continue;
        }

        // The bindings responder.
        if(B_Responder(ddev)) continue;

        // The "fallback" responder. Gets the event if no one else is interested.
        if(validGameEvent && callGameResponders && gx.FallbackResponder)
            gx.FallbackResponder(&ev);
    }

    if(updateAxes)
    {
        // Input events have modified input device state: update the axis positions.
        updateAllDeviceAxes(ticLength);
    }
}

void I_ProcessEvents(timespan_t ticLength)
{
    // Poll all event sources (i.e., input devices) and post events.
    postEventsFromInputDevices();

    // Dispatch all accumulated events down the responder chain.
    dispatchEvents(&queue, ticLength, !useSharpInputEvents);
}

void I_ProcessSharpEvents(timespan_t ticLength)
{
    // Sharp ticks may have some events queued on the side.
    if(DD_IsSharpTick() || !DD_IsFrameTimeAdvancing())
    {
        dispatchEvents(&sharpQueue, DD_IsFrameTimeAdvancing()? SECONDSPERTIC : ticLength, true);
    }
}

/**
 * Apply all active modifiers to the key.
 */
static byte DD_ModKey(byte key)
{
    if(shiftDown)
        key = shiftKeyMappings[key];
    if(altDown)
        key = altKeyMappings[key];

    if(key >= DDKEY_NUMPAD7 && key <= DDKEY_NUMPAD0)
    {
        byte numPadKeys[10] = { '7', '8', '9', '4', '5', '6', '1', '2', '3', '0' };
        return numPadKeys[key - DDKEY_NUMPAD7];
    }
    else if(key == DDKEY_DIVIDE)
    {
        return '/';
    }
    else if(key == DDKEY_SUBTRACT)
    {
        return '-';
    }
    else if(key == DDKEY_ADD)
    {
        return '+';
    }
    else if(key == DDKEY_DECIMAL)
    {
        return '.';
    }
    else if(key == DDKEY_MULTIPLY)
    {
        return '*';
    }

    return key;
}

void I_ReadKeyboard()
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
                << ev.toggle.id << ev.toggle.id << ev.toggle.text << strlen(ev.toggle.text);

        I_PostEvent(&ev);
    }

#undef QUEUESIZE
}

void I_ReadMouse()
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
        I_PostEvent(&ev);
    }
    if(ypos)
    {
        ev.axis.id  = 1;
        ev.axis.pos = ypos;
        I_PostEvent(&ev);
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
            LOGDEV_INPUT_XVERBOSE("[%02i] %i/%i") << i << mouse.buttonDowns[i] << mouse.buttonUps[i];
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
                I_PostEvent(&ev);
            }
            if(mouse.buttonUps[i]-- > 0)
            {
                ev.toggle.state = ETOG_UP;
                LOG_INPUT_XVERBOSE("Mouse button %i up") << i;
                I_PostEvent(&ev);
            }
        }
    }
}

void I_ReadJoystick()
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
                I_PostEvent(&ev);
                LOG_INPUT_XVERBOSE("Joy button %i down") << i;
            }
            if(state.buttonUps[i]-- > 0)
            {
                ev.toggle.state = ETOG_UP;
                I_PostEvent(&ev);
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
            I_PostEvent(&ev);

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
        I_PostEvent(&ev);
    }
}

void I_ReadHeadTracker()
{
    // These values are for the input subsystem and gameplay. The renderer will check the head
    // orientation independently, with as little latency as possible.

    // If a head tracking device is connected, the device is marked active.
    if(!DD_GetInteger(DD_USING_HEAD_TRACKING))
    {
        I_Device(IDEV_HEAD_TRACKER).deactivate();
        return;
    }

    I_Device(IDEV_HEAD_TRACKER).activate();

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
    I_PostEvent(&ev);

    ev.axis.id  = 1; // Pitch (1.0 means 85 degrees).
    ev.axis.pos = de::radianToDegree(pry[0]) * 1.0 / 85.0;
    I_PostEvent(&ev);

    // So I'll assume that if roll ever gets used, 1.0 will mean 180 degrees there too.
    ev.axis.id  = 2; // Roll.
    ev.axis.pos = de::radianToDegree(pry[1]) * 1.0 / 180.0;
    I_PostEvent(&ev);
}

#ifdef DENG2_DEBUG

static void initDrawStateForVisual(Point2Raw const *origin)
{
    FR_PushAttrib();

    // Ignore zero offsets.
    if(origin && !(origin->x == 0 && origin->y == 0))
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(origin->x, origin->y, 0);
    }
}

static void endDrawStateForVisual(Point2Raw const *origin)
{
    // Ignore zero offsets.
    if(origin && !(origin->x == 0 && origin->y == 0))
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    FR_PopAttrib();
}

void Rend_RenderButtonStateVisual(InputDevice &device, int buttonID, Point2Raw const *_origin,
    RectRaw *geometry)
{
#define BORDER 4

    float const upColor[] = { .3f, .3f, .3f, .6f };
    float const downColor[] = { .3f, .3f, 1, .6f };
    float const expiredMarkColor[] = { 1, 0, 0, 1 };
    float const triggeredMarkColor[] = { 1, 0, 1, 1 };

    if(geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    InputDeviceButtonControl const &button = device.button(buttonID);

    Point2Raw origin;
    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    ddstring_t label; Str_InitStd(&label);

    // Compose the label.
    char const *buttonLabel = nullptr;
    char buttonLabelBuf[2];
    if(!button.name().isEmpty())
    {
        // Use the symbolic name.
        buttonLabel = button.name().toUtf8().constData();
    }
    else if(&device == I_DevicePtr(IDEV_KEYBOARD))
    {
        // Perhaps a printable ASCII character?
        // Apply all active modifiers to the key.
        byte asciiCode = DD_ModKey((byte)buttonID);
        if(asciiCode > 32 && asciiCode < 127)
        {
            buttonLabelBuf[0] = asciiCode;
            buttonLabelBuf[1] = '\0';
            buttonLabel = buttonLabelBuf;
        }

        // Is there symbolic name in the bindings system?
        if(!buttonLabel)
        {
            buttonLabel = B_ShortNameForKey(buttonID, false/*do not force lowercase*/);
        }
    }

    if(buttonLabel)
        Str_Append(&label, buttonLabel);
    else
        Str_Appendf(&label, "#%03u", buttonID);

    initDrawStateForVisual(&origin);

    // Calculate the size of the visual according to the dimensions of the text.
    Size2Raw textSize;
    FR_TextSize(&textSize, Str_Text(&label));

    // Enlarge by BORDER pixels.
    Rectanglei textGeom = Rectanglei::fromSize(Vector2i(0, 0),
                                               Vector2ui(textSize.width  + BORDER * 2,
                                                         textSize.height + BORDER * 2));

    // Draw a background.
    glColor4fv(button.isDown()? downColor : upColor);
    GL_DrawRect(textGeom);

    // Draw the text.
    glEnable(GL_TEXTURE_2D);
    Point2Raw const textOffset(BORDER, BORDER);
    FR_DrawText(Str_Text(&label), &textOffset);
    glDisable(GL_TEXTURE_2D);

    // Mark expired?
    if(button.bindContextAssociation() & InputDeviceControl::Expired)
    {
        int const markSize = .5f + de::min(textGeom.width(), textGeom.height()) / 3.f;

        glColor3fv(expiredMarkColor);
        glBegin(GL_TRIANGLES);
        glVertex2i(textGeom.width(), 0);
        glVertex2i(textGeom.width(), markSize);
        glVertex2i(textGeom.width() - markSize, 0);
        glEnd();
    }

    // Mark triggered?
    if(button.bindContextAssociation() & InputDeviceControl::Triggered)
    {
        int const markSize = .5f + de::min(textGeom.width(), textGeom.height()) / 3.f;

        glColor3fv(triggeredMarkColor);
        glBegin(GL_TRIANGLES);
        glVertex2i(0, 0);
        glVertex2i(markSize, 0);
        glVertex2i(0, markSize);
        glEnd();
    }

    endDrawStateForVisual(&origin);

    Str_Free(&label);

    if(geometry)
    {
        std::memcpy(&geometry->origin, &origin, sizeof(geometry->origin));
        geometry->size.width  = textGeom.width();
        geometry->size.height = textGeom.height();
    }

#undef BORDER
}

void Rend_RenderAxisStateVisual(InputDevice & /*device*/, int /*axisID*/, Point2Raw const *origin,
    RectRaw *geometry)
{
    if(geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    //inputdevaxis_t const &axis = device.axis(axisID);

    initDrawStateForVisual(origin);

    endDrawStateForVisual(origin);
}

void Rend_RenderHatStateVisual(InputDevice & /*device*/, int /*hatID*/, Point2Raw const *origin,
    RectRaw *geometry)
{
    if(geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    //inputdevhat_t const &hat = device.hat(hatID);

    initDrawStateForVisual(origin);

    endDrawStateForVisual(origin);
}

// Input device control types:
enum inputdev_controltype_t
{
    IDC_KEY,
    IDC_AXIS,
    IDC_HAT,
    NUM_INPUT_DEVICE_CONTROL_TYPES
};

struct inputdev_layout_control_t
{
    inputdev_controltype_t type;
    uint id;
};

struct inputdev_layout_controlgroup_t
{
    inputdev_layout_control_t *controls;
    uint numControls;
};

// Defines the order of controls in the visual.
struct inputdev_layout_t
{
    inputdev_layout_controlgroup_t *groups;
    uint numGroups;
};

static void drawControlGroup(InputDevice &device, inputdev_layout_controlgroup_t const *group,
    Point2Raw *_origin, RectRaw *retGeometry)
{
#define SPACING  2

    if(retGeometry)
    {
        retGeometry->origin.x = retGeometry->origin.y = 0;
        retGeometry->size.width = retGeometry->size.height = 0;
    }

    if(!group) return;

    Point2Raw origin;
    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    Rect *grpGeom = nullptr;
    RectRaw ctrlGeom;
    for(uint i = 0; i < group->numControls; ++i)
    {
        inputdev_layout_control_t const *ctrl = group->controls + i;

        switch(ctrl->type)
        {
        case IDC_AXIS: Rend_RenderAxisStateVisual(device, ctrl->id, &origin, &ctrlGeom); break;
        case IDC_KEY:  Rend_RenderButtonStateVisual(device, ctrl->id, &origin, &ctrlGeom); break;
        case IDC_HAT:  Rend_RenderHatStateVisual(device, ctrl->id, &origin, &ctrlGeom); break;

        default:
            App_Error("drawControlGroup: Unknown inputdev_controltype_t: %i.", (int)ctrl->type);
            exit(1); // Unreachable.
        }

        if(ctrlGeom.size.width > 0 && ctrlGeom.size.height > 0)
        {
            origin.x += ctrlGeom.size.width + SPACING;

            if(grpGeom)
                Rect_UniteRaw(grpGeom, &ctrlGeom);
            else
                grpGeom = Rect_NewFromRaw(&ctrlGeom);
        }
    }

    if(grpGeom)
    {
        if(retGeometry)
        {
            Rect_Raw(grpGeom, retGeometry);
        }

        Rect_Delete(grpGeom);
    }

#undef SPACING
}

/**
 * Render a visual representation of the current state of the specified device.
 */
void Rend_RenderInputDeviceStateVisual(InputDevice &device, inputdev_layout_t const *layout,
    Point2Raw const *origin, Size2Raw *retVisualDimensions)
{
#define SPACING  2

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(retVisualDimensions)
    {
        retVisualDimensions->width = retVisualDimensions->height = 0;
    }

    if(novideo || isDedicated) return; // Not for us.
    if(!layout) return;

    // Init render state.
    FR_SetFont(fontFixed);
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(1, 1, 1, 1);
    initDrawStateForVisual(origin);

    Point2Raw offset;
    Rect *visualGeom = nullptr;

    // Draw device name first.
    if(!device.title().isEmpty())
    {
        Size2Raw size;

        glEnable(GL_TEXTURE_2D);
        Block const fullName(device.title().toUtf8());
        FR_DrawText(fullName.constData(), nullptr/*no offset*/);
        glDisable(GL_TEXTURE_2D);

        FR_TextSize(&size, fullName.constData());
        visualGeom = Rect_NewWithOriginSize2(offset.x, offset.y, size.width, size.height);

        offset.y += size.height + SPACING;
    }

    // Draw control groups.
    for(uint i = 0; i < layout->numGroups; ++i)
    {
        inputdev_layout_controlgroup_t const *grp = &layout->groups[i];
        RectRaw grpGeometry;

        drawControlGroup(device, grp, &offset, &grpGeometry);

        if(grpGeometry.size.width > 0 && grpGeometry.size.height > 0)
        {
            if(visualGeom)
                Rect_UniteRaw(visualGeom, &grpGeometry);
            else
                visualGeom = Rect_NewFromRaw(&grpGeometry);

            offset.y = Rect_Y(visualGeom) + Rect_Height(visualGeom) + SPACING;
        }
    }

    // Back to previous render state.
    endDrawStateForVisual(origin);
    FR_PopAttrib();

    // Return the united geometry dimensions?
    if(visualGeom && retVisualDimensions)
    {
        retVisualDimensions->width  = Rect_Width(visualGeom);
        retVisualDimensions->height = Rect_Height(visualGeom);
    }

#undef SPACING
}

void Rend_DrawInputDeviceVisuals()
{
#define SPACING      2
#define NUMITEMS(x)  (sizeof(x) / sizeof((x)[0]))

    // Keyboard (Standard US English layout):
    static inputdev_layout_control_t keyGroup1[] = {
        { IDC_KEY,  27 }, // escape
        { IDC_KEY, 132 }, // f1
        { IDC_KEY, 133 }, // f2
        { IDC_KEY, 134 }, // f3
        { IDC_KEY, 135 }, // f4
        { IDC_KEY, 136 }, // f5
        { IDC_KEY, 137 }, // f6
        { IDC_KEY, 138 }, // f7
        { IDC_KEY, 139 }, // f8
        { IDC_KEY, 140 }, // f9
        { IDC_KEY, 141 }, // f10
        { IDC_KEY, 142 }, // f11
        { IDC_KEY, 143 }  // f12
    };
    static inputdev_layout_control_t keyGroup2[] = {
        { IDC_KEY,  96 }, // tilde
        { IDC_KEY,  49 }, // 1
        { IDC_KEY,  50 }, // 2
        { IDC_KEY,  51 }, // 3
        { IDC_KEY,  52 }, // 4
        { IDC_KEY,  53 }, // 5
        { IDC_KEY,  54 }, // 6
        { IDC_KEY,  55 }, // 7
        { IDC_KEY,  56 }, // 8
        { IDC_KEY,  57 }, // 9
        { IDC_KEY,  48 }, // 0
        { IDC_KEY,  45 }, // -
        { IDC_KEY,  61 }, // =
        { IDC_KEY, 127 }  // backspace
    };
    static inputdev_layout_control_t keyGroup3[] = {
        { IDC_KEY,   9 }, // tab
        { IDC_KEY, 113 }, // q
        { IDC_KEY, 119 }, // w
        { IDC_KEY, 101 }, // e
        { IDC_KEY, 114 }, // r
        { IDC_KEY, 116 }, // t
        { IDC_KEY, 121 }, // y
        { IDC_KEY, 117 }, // u
        { IDC_KEY, 105 }, // i
        { IDC_KEY, 111 }, // o
        { IDC_KEY, 112 }, // p
        { IDC_KEY,  91 }, // {
        { IDC_KEY,  93 }, // }
        { IDC_KEY,  92 }, // bslash
    };
    static inputdev_layout_control_t keyGroup4[] = {
        { IDC_KEY, 145 }, // capslock
        { IDC_KEY,  97 }, // a
        { IDC_KEY, 115 }, // s
        { IDC_KEY, 100 }, // d
        { IDC_KEY, 102 }, // f
        { IDC_KEY, 103 }, // g
        { IDC_KEY, 104 }, // h
        { IDC_KEY, 106 }, // j
        { IDC_KEY, 107 }, // k
        { IDC_KEY, 108 }, // l
        { IDC_KEY,  59 }, // semicolon
        { IDC_KEY,  39 }, // apostrophe
        { IDC_KEY,  13 }  // return
    };
    static inputdev_layout_control_t keyGroup5[] = {
        { IDC_KEY, 159 }, // shift
        { IDC_KEY, 122 }, // z
        { IDC_KEY, 120 }, // x
        { IDC_KEY,  99 }, // c
        { IDC_KEY, 118 }, // v
        { IDC_KEY,  98 }, // b
        { IDC_KEY, 110 }, // n
        { IDC_KEY, 109 }, // m
        { IDC_KEY,  44 }, // comma
        { IDC_KEY,  46 }, // period
        { IDC_KEY,  47 }, // fslash
        { IDC_KEY, 159 }, // shift
    };
    static inputdev_layout_control_t keyGroup6[] = {
        { IDC_KEY, 160 }, // ctrl
        { IDC_KEY,   0 }, // ???
        { IDC_KEY, 161 }, // alt
        { IDC_KEY,  32 }, // space
        { IDC_KEY, 161 }, // alt
        { IDC_KEY,   0 }, // ???
        { IDC_KEY,   0 }, // ???
        { IDC_KEY, 160 }  // ctrl
    };
    static inputdev_layout_control_t keyGroup7[] = {
        { IDC_KEY, 170 }, // printscrn
        { IDC_KEY, 146 }, // scrolllock
        { IDC_KEY, 158 }  // pause
    };
    static inputdev_layout_control_t keyGroup8[] = {
        { IDC_KEY, 162 }, // insert
        { IDC_KEY, 166 }, // home
        { IDC_KEY, 164 }  // pageup
    };
    static inputdev_layout_control_t keyGroup9[] = {
        { IDC_KEY, 163 }, // delete
        { IDC_KEY, 167 }, // end
        { IDC_KEY, 165 }, // pagedown
    };
    static inputdev_layout_control_t keyGroup10[] = {
        { IDC_KEY, 130 }, // up
        { IDC_KEY, 129 }, // left
        { IDC_KEY, 131 }, // down
        { IDC_KEY, 128 }, // right
    };
    static inputdev_layout_control_t keyGroup11[] = {
        { IDC_KEY, 144 }, // numlock
        { IDC_KEY, 172 }, // divide
        { IDC_KEY, DDKEY_MULTIPLY }, // multiply
        { IDC_KEY, 168 }  // subtract
    };
    static inputdev_layout_control_t keyGroup12[] = {
        { IDC_KEY, 147 }, // pad 7
        { IDC_KEY, 148 }, // pad 8
        { IDC_KEY, 149 }, // pad 9
        { IDC_KEY, 169 }, // add
    };
    static inputdev_layout_control_t keyGroup13[] = {
        { IDC_KEY, 150 }, // pad 4
        { IDC_KEY, 151 }, // pad 5
        { IDC_KEY, 152 }  // pad 6
    };
    static inputdev_layout_control_t keyGroup14[] = {
        { IDC_KEY, 153 }, // pad 1
        { IDC_KEY, 154 }, // pad 2
        { IDC_KEY, 155 }, // pad 3
        { IDC_KEY, 171 }  // enter
    };
    static inputdev_layout_control_t keyGroup15[] = {
        { IDC_KEY, 156 }, // pad 0
        { IDC_KEY, 157 }  // pad .
    };
    static inputdev_layout_controlgroup_t keyGroups[] = {
        { keyGroup1, NUMITEMS(keyGroup1) },
        { keyGroup2, NUMITEMS(keyGroup2) },
        { keyGroup3, NUMITEMS(keyGroup3) },
        { keyGroup4, NUMITEMS(keyGroup4) },
        { keyGroup5, NUMITEMS(keyGroup5) },
        { keyGroup6, NUMITEMS(keyGroup6) },
        { keyGroup7, NUMITEMS(keyGroup7) },
        { keyGroup8, NUMITEMS(keyGroup8) },
        { keyGroup9, NUMITEMS(keyGroup9) },
        { keyGroup10, NUMITEMS(keyGroup10) },
        { keyGroup11, NUMITEMS(keyGroup11) },
        { keyGroup12, NUMITEMS(keyGroup12) },
        { keyGroup13, NUMITEMS(keyGroup13) },
        { keyGroup14, NUMITEMS(keyGroup14) },
        { keyGroup15, NUMITEMS(keyGroup15) }
    };
    static inputdev_layout_t keyLayout = { keyGroups, NUMITEMS(keyGroups) };

    // Mouse:
    static inputdev_layout_control_t mouseGroup1[] = {
        { IDC_AXIS, 0 }, // x-axis
        { IDC_AXIS, 1 }, // y-axis
    };
    static inputdev_layout_control_t mouseGroup2[] = {
        { IDC_KEY, 0 }, // left
        { IDC_KEY, 1 }, // middle
        { IDC_KEY, 2 }  // right
    };
    static inputdev_layout_control_t mouseGroup3[] = {
        { IDC_KEY, 3 }, // wheelup
        { IDC_KEY, 4 }  // wheeldown
    };
    static inputdev_layout_control_t mouseGroup4[] = {
        { IDC_KEY,  5 },
        { IDC_KEY,  6 },
        { IDC_KEY,  7 },
        { IDC_KEY,  8 },
        { IDC_KEY,  9 },
        { IDC_KEY, 10 },
        { IDC_KEY, 11 },
        { IDC_KEY, 12 },
        { IDC_KEY, 13 },
        { IDC_KEY, 14 },
        { IDC_KEY, 15 }
    };
    static inputdev_layout_controlgroup_t mouseGroups[] = {
        { mouseGroup1, NUMITEMS(mouseGroup1) },
        { mouseGroup2, NUMITEMS(mouseGroup2) },
        { mouseGroup3, NUMITEMS(mouseGroup3) },
        { mouseGroup4, NUMITEMS(mouseGroup4) }
    };
    static inputdev_layout_t mouseLayout = { mouseGroups, NUMITEMS(mouseGroups) };

    // Joystick:
    static inputdev_layout_control_t joyGroup1[] = {
        { IDC_AXIS, 0 }, // x-axis
        { IDC_AXIS, 1 }, // y-axis
        { IDC_AXIS, 2 }, // z-axis
        { IDC_AXIS, 3 }  // w-axis
    };
    static inputdev_layout_control_t joyGroup2[] = {
        { IDC_AXIS,  4 },
        { IDC_AXIS,  5 },
        { IDC_AXIS,  6 },
        { IDC_AXIS,  7 },
        { IDC_AXIS,  8 },
        { IDC_AXIS,  9 },
        { IDC_AXIS, 10 },
        { IDC_AXIS, 11 },
        { IDC_AXIS, 12 },
        { IDC_AXIS, 13 },
        { IDC_AXIS, 14 },
        { IDC_AXIS, 15 },
        { IDC_AXIS, 16 },
        { IDC_AXIS, 17 }
    };
    static inputdev_layout_control_t joyGroup3[] = {
        { IDC_AXIS, 18 },
        { IDC_AXIS, 19 },
        { IDC_AXIS, 20 },
        { IDC_AXIS, 21 },
        { IDC_AXIS, 22 },
        { IDC_AXIS, 23 },
        { IDC_AXIS, 24 },
        { IDC_AXIS, 25 },
        { IDC_AXIS, 26 },
        { IDC_AXIS, 27 },
        { IDC_AXIS, 28 },
        { IDC_AXIS, 29 },
        { IDC_AXIS, 30 },
        { IDC_AXIS, 31 }
    };
    static inputdev_layout_control_t joyGroup4[] = {
        { IDC_HAT,  0 },
        { IDC_HAT,  1 },
        { IDC_HAT,  2 },
        { IDC_HAT,  3 }
    };
    static inputdev_layout_control_t joyGroup5[] = {
        { IDC_KEY,  0 },
        { IDC_KEY,  1 },
        { IDC_KEY,  2 },
        { IDC_KEY,  3 },
        { IDC_KEY,  4 },
        { IDC_KEY,  5 },
        { IDC_KEY,  6 },
        { IDC_KEY,  7 }
    };
    static inputdev_layout_control_t joyGroup6[] = {
        { IDC_KEY,  8 },
        { IDC_KEY,  9 },
        { IDC_KEY, 10 },
        { IDC_KEY, 11 },
        { IDC_KEY, 12 },
        { IDC_KEY, 13 },
        { IDC_KEY, 14 },
        { IDC_KEY, 15 },
    };
    static inputdev_layout_control_t joyGroup7[] = {
        { IDC_KEY, 16 },
        { IDC_KEY, 17 },
        { IDC_KEY, 18 },
        { IDC_KEY, 19 },
        { IDC_KEY, 20 },
        { IDC_KEY, 21 },
        { IDC_KEY, 22 },
        { IDC_KEY, 23 },
    };
    static inputdev_layout_control_t joyGroup8[] = {
        { IDC_KEY, 24 },
        { IDC_KEY, 25 },
        { IDC_KEY, 26 },
        { IDC_KEY, 27 },
        { IDC_KEY, 28 },
        { IDC_KEY, 29 },
        { IDC_KEY, 30 },
        { IDC_KEY, 31 },
    };
    static inputdev_layout_controlgroup_t joyGroups[] = {
        { joyGroup1, NUMITEMS(joyGroup1) },
        { joyGroup2, NUMITEMS(joyGroup2) },
        { joyGroup3, NUMITEMS(joyGroup3) },
        { joyGroup4, NUMITEMS(joyGroup4) },
        { joyGroup5, NUMITEMS(joyGroup5) },
        { joyGroup6, NUMITEMS(joyGroup6) },
        { joyGroup7, NUMITEMS(joyGroup7) },
        { joyGroup8, NUMITEMS(joyGroup8) }
    };
    static inputdev_layout_t joyLayout = { joyGroups, NUMITEMS(joyGroups) };

    Point2Raw origin(2, 2);
    Size2Raw dimensions;

    if(novideo || isDedicated) return; // Not for us.

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Disabled?
    if(!devRendKeyState && !devRendMouseState && !devRendJoyState) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT, 0, -1, 1);

    if(devRendKeyState)
    {
        Rend_RenderInputDeviceStateVisual(I_Device(IDEV_KEYBOARD), &keyLayout, &origin, &dimensions);
        origin.y += dimensions.height + SPACING;
    }

    if(devRendMouseState)
    {
        Rend_RenderInputDeviceStateVisual(I_Device(IDEV_MOUSE), &mouseLayout, &origin, &dimensions);
        origin.y += dimensions.height + SPACING;
    }

    if(devRendJoyState)
    {
        Rend_RenderInputDeviceStateVisual(I_Device(IDEV_JOY1), &joyLayout, &origin, &dimensions);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef NUMITEMS
#undef SPACING
}
#endif // DENG2_DEBUG

D_CMD(ListInputDevices)
{
    DENG2_UNUSED3(src, argc, argv);
    LOG_INPUT_MSG(_E(b) "Input Devices:");
    for(InputDevice *dev : devices)
    {
        LOG_INPUT_MSG("") << dev->description();
    }
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

void I_ConsoleRegister()
{
    // Cvars
    C_VAR_BYTE("input-sharp", &useSharpInputEvents, 0, 0, 1);

#ifdef DENG2_DEBUG
    C_VAR_BYTE("rend-dev-input-joy-state",   &devRendJoyState,   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-input-key-state",   &devRendKeyState,   CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-input-mouse-state", &devRendMouseState, CVF_NO_ARCHIVE, 0, 1);
#endif

    // Ccmds
    C_CMD("listinputdevices",   "", ListInputDevices);
    C_CMD("releasemouse",       "", ReleaseMouse);

    //C_CMD_FLAGS("setaxis", "s",      AxisPrintConfig, CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis", "ss",     AxisChangeOption, CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis", "sss",    AxisChangeValue, CMDF_NO_DEDICATED);
}
