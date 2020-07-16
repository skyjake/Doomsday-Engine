/** @file windoweventhandler.cpp  Window event handling.
 *
 * @authors Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/WindowEventHandler"
#include "de/GLWindow"

#include <de/App>
#include <de/LogBuffer>
#include <de/Loop>
#include <de/Rule>
#include <de/WindowSystem>

#include <SDL_events.h>

namespace de {

static constexpr TimeSpan GESTURE_MOMENTUM_EVAL_TIME = 70_ms;

/**
 * Simulates momentum along a single axis for scrolling.
 */
struct Flywheel
{
    struct Sample {
        float pos;
        float delta;
        uint32_t at;

        Sample(float p = 0, float dt = 0, uint32_t t = 0)
            : pos(p)
            , delta(dt)
            , at(t)
        {}
    };

    float        pos        = 0;
    float        friction   = 2.0f;
    float        momentum   = 0;
    bool         isReleased = true;
    List<Sample> samples;

    void applyMove(float delta, uint32_t timestamp)
    {
        pos += delta;
        samples.emplace_back(pos, delta, timestamp);
        isReleased = false;
    }

    void updateMomentum(uint32_t currentTime)
    {
        int      includedSamples = 0;
        uint32_t startTime       = 0;

        momentum = 0;

        int sampleIndex = samples.sizei() - 1;
        for (; sampleIndex >= 0; --sampleIndex)
        {
            const auto &sample = samples[sampleIndex];
            if (TimeSpan::fromMilliSeconds(currentTime - sample.at) > GESTURE_MOMENTUM_EVAL_TIME)
            {
                break;
            }
            includedSamples++;
            momentum += sample.delta;
            startTime = sample.at;
        }

        if (includedSamples > 0)
        {
            const auto span = TimeSpan::fromMilliSeconds(currentTime - startTime);
            momentum /= float(span);
//            debug("span: %f seconds (%d smp), mom: %f u/s", span, includedSamples, momentum);
        }

        // Cull obsolete samples.
        if (sampleIndex > 0)
        {
            samples.remove(0, sampleIndex);
        }
    }

    void release(uint32_t timestamp)
    {
        if (!isReleased)
        {
            isReleased = true;
            updateMomentum(timestamp);
            /*
            debug("[%p] released with mom %f (%d samples)", this, momentum, samples.sizei());
            for (int i = 0; i < samples.sizei(); ++i)
            {
                debug("  %d: pos %f delta %f time %u",
                      i,
                      samples[i].pos,
                      samples[i].delta,
                      samples[i].at);
            }
            */
            samples.clear();
        }
    }

    float advanceTime(TimeSpan elapsed)
    {
        if (isReleased)
        {
            const auto F = float(friction * elapsed);

            if (std::abs(momentum) <= F) momentum = 0;
            else if (momentum > 0) momentum -= F;
            else if (momentum < 0) momentum += F;

            return float(momentum * elapsed);
        }
        return 0.f;
    }
};

DE_PIMPL(WindowEventHandler)
, DE_OBSERVES(Clock, TimeChange)
{
    GLWindow *   window;
    bool         mouseGrabbed = false;
    KeyboardMode keyboardMode = TextInput;
    Vec2i        prevMousePos;
    Vec2i        wheelAngleAccum;
    int          wheelDir[2];
    Vec2i        currentMousePos;
    Time         prevUpdateAt;

    struct InertiaScrollState {
        bool     active      = false;
        float    sensitivity = 750;
        Flywheel pos[2];
        Vec2f    prevGesturePos;
        double   swipeRoundoff[2]{};
    };
    InertiaScrollState inertiaScroll;

    Impl(Public *i, GLWindow *parentWindow)
        : Base(i)
        , window(parentWindow)
    {
        wheelDir[0] = wheelDir[1] = 0;

        Clock::get().audienceForTimeChange() += this;
    }

    void grabMouse()
    {
        if (!mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Grabbing mouse") << mouseGrabbed;
            mouseGrabbed = true;
            DE_NOTIFY_PUBLIC(MouseStateChange, i) { i->mouseStateChanged(Trapped); }
        }
    }

    void ungrabMouse()
    {
        if (mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Ungrabbing mouse");
            mouseGrabbed = false;
            DE_NOTIFY_PUBLIC(MouseStateChange, i) { i->mouseStateChanged(Untrapped); }
        }
    }

    void handleTextInput(const SDL_TextInputEvent &ev)
    {
        KeyEvent keyEvent(ev.text);
        DE_NOTIFY_PUBLIC(KeyEvent, i)
        {
            i->keyEvent(keyEvent);
        }
    }

    void handleKeyEvent(const SDL_KeyboardEvent &ev)
    {
        LOGDEV_INPUT_XVERBOSE("text input active: %i", SDL_IsTextInputActive());

        KeyEvent keyEvent(ev.state == SDL_PRESSED
                              ? (ev.repeat ? KeyEvent::Repeat : KeyEvent::Pressed)
                              : KeyEvent::Released,
                          ev.keysym.sym,
                          ev.keysym.scancode,
                          KeyEvent::modifiersFromSDL(ev.keysym.mod));

        DE_NOTIFY_PUBLIC(KeyEvent, i)
        {
            i->keyEvent(keyEvent);

            /*            i->keyEvent(KeyEvent(ev->isAutoRepeat()?             KeyEvent::Repeat :
                                 ev->type() == QEvent::KeyPress? KeyEvent::Pressed :
                                                                 KeyEvent::Released,
                                 ev->key(),
                                 KeyEvent::ddKeyFromQt(ev->key(), ev->nativeVirtualKey(), ev->nativeScanCode()),
                                 nativeCode(ev),
                                 ev->text(),
                                 (ev->modifiers().testFlag(Qt::ShiftModifier)?   KeyEvent::Shift   : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::ControlModifier)? KeyEvent::Control : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::AltModifier)?     KeyEvent::Alt     : KeyEvent::NoModifiers) |
                                 (ev->modifiers().testFlag(Qt::MetaModifier)?    KeyEvent::Meta    : KeyEvent::NoModifiers)));
   */
        }
    }

    void handleFocus(bool focusGained)
    {
        if (focusGained)
        {
            WindowSystem::get().setFocusedWindow(window->id());
        }

        //debug("focus: %i", focusGained);
        DE_NOTIFY_PUBLIC(FocusChange, i)
        {
            i->windowFocusChanged(*window, focusGained);
        }
    }

    static MouseEvent::Button translateButton(Uint8 sdlButton)
    {
        switch (sdlButton)
        {
        case SDL_BUTTON_LEFT:   return MouseEvent::Left;
        case SDL_BUTTON_MIDDLE: return MouseEvent::Middle;
        case SDL_BUTTON_RIGHT:  return MouseEvent::Right;
        case SDL_BUTTON_X1:     return MouseEvent::XButton1;
        case SDL_BUTTON_X2:     return MouseEvent::XButton2;
        default:                return MouseEvent::Unknown;
        }
    }

    void handleMouseButtonEvent(const SDL_MouseButtonEvent &ev)
    {
        const auto pos = Vec2i(ev.x, ev.y) * DE_GUI_APP->devicePixelRatio();
        
        if (ev.type == SDL_MOUSEBUTTONDOWN)
        {
            DE_NOTIFY_PUBLIC(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(translateButton(ev.button), MouseEvent::Pressed, pos));
            }
            if (ev.clicks == 2)
            {
                DE_NOTIFY_PUBLIC(MouseEvent, i)
                {
                    i->mouseEvent(
                        MouseEvent(translateButton(ev.button), MouseEvent::DoubleClick, pos));
                }
            }
        }
        else
        {
            DE_NOTIFY_PUBLIC(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(translateButton(ev.button), MouseEvent::Released, pos));
            }
        }
    }

    void handleMouseMoveEvent(const SDL_MouseMotionEvent &ev)
    {
        currentMousePos = Vec2i(ev.x, ev.y) * DE_GUI_APP->devicePixelRatio();

        // Absolute events are only emitted when the mouse is untrapped.
        if (!mouseGrabbed)
        {
            DE_NOTIFY_PUBLIC(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Absolute, currentMousePos));
            }
        }
        else
        {
            DE_NOTIFY_PUBLIC(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Relative, Vec2i(ev.xrel, ev.yrel)));
            }
        }
    }

    void handleMouseWheelEvent(const SDL_MouseWheelEvent &ev)
    {
        DE_NOTIFY_PUBLIC(MouseEvent, i)
        {
            if (ev.x)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Steps, Vec2i(ev.x, 0),
                                         currentMousePos));
            }
            if (ev.y)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Steps, Vec2i(0, ev.y),
                                         currentMousePos));
            }
        }

        /*
        const float devicePixels = d->window->devicePixelRatio();

        QPoint numPixels = ev->pixelDelta();
        QPoint numDegrees = ev->angleDelta() / 8;
        d->wheelAngleAccum += numDegrees;

        if (!numPixels.isNull())
        {
            DE_NOTIFY(MouseEvent, i)
            {
                if (numPixels.x())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vec2i(devicePixels * numPixels.x(), 0),
                                             d->translatePosition(ev)));
                }
                if (numPixels.y())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vec2i(0, devicePixels * numPixels.y()),
                                             d->translatePosition(ev)));
                }
            }
        }

        const QPoint steps = d->wheelAngleAccum / 15;
        if (!steps.isNull())
        {
            DE_NOTIFY(MouseEvent, i)
            {
                if (steps.x())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::Step, Vec2i(steps.x(), 0),
                                             !d->mouseGrabbed? d->translatePosition(ev) : Vec2i()));
                }
                if (steps.y())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::Step, Vec2i(0, steps.y()),
                                             !d->mouseGrabbed? d->translatePosition(ev) : Vec2i()));
                }
            }
            d->wheelAngleAccum -= steps * 15;
        }

        d->prevWheelAt.start();
        */
    }

    void handleGestureEvent(const SDL_MultiGestureEvent &ev)
    {
        if (ev.numFingers == 2)
        {
            auto &scr = inertiaScroll;

            const Vec2f currentPos{ev.x, ev.y};

            if (scr.active)
            {
                const Vec2f delta = currentPos - scr.prevGesturePos;

                scr.pos[0].applyMove(delta.x, ev.timestamp);
                scr.pos[1].applyMove(delta.y, ev.timestamp);

                for (int axis = 0; axis < 2; ++axis)
                {
                    const double units_f = delta[axis] * scr.sensitivity + scr.swipeRoundoff[axis];
                    const int    units   = int(units_f);

                    scr.swipeRoundoff[axis] = units_f - units;

                    if (units)
                    {
                        DE_NOTIFY_PUBLIC(MouseEvent, i)
                        {
                            i->mouseEvent(MouseEvent(MouseEvent::Pixels,
                                                     axis == 0 ? Vec2i(units, 0) : Vec2i(0, units),
                                                     currentMousePos));
                        }
                    }
                }
            }
            scr.active         = true;
            scr.prevGesturePos = currentPos;
        }
    }

    void handleFingerEvent(const SDL_TouchFingerEvent &ev)
    {
        if (ev.type == SDL_FINGERUP)
        {
            inertiaScroll.pos[0].release(ev.timestamp);
            inertiaScroll.pos[1].release(ev.timestamp);
        }
        else if (ev.type == SDL_FINGERDOWN)
        {
            // Stop scrolling.
            inertiaScroll.active = false;
        }
    }

    /**
     * Perform actions that occur over time, e.g., inertia scrolling.
     */
    void timeChanged(const Clock &) override
    {
        if (!prevUpdateAt.isValid()) return;

        const TimeSpan elapsed = prevUpdateAt.since();
        prevUpdateAt = Time::currentHighPerformanceTime();

        auto &scr = inertiaScroll;

        // Inertia scroll events.
        if (scr.active)
        {
            // Handle the axes separately.
            for (int axis = 0; axis < 2; ++axis)
            {
                const int units = round<int>(scr.pos[axis].advanceTime(elapsed) * scr.sensitivity);

                // Generate a scroll event.
                if (units)
                {
                    DE_NOTIFY_PUBLIC(MouseEvent, i)
                    {
                        i->mouseEvent(MouseEvent(MouseEvent::Pixels,
                                                 axis == 0 ? Vec2i(units, 0) : Vec2i(0, units),
                                                 currentMousePos));
                    }
                }
            }
        }
    }

    DE_PIMPL_AUDIENCE(FocusChange)
};

DE_AUDIENCE_METHOD(WindowEventHandler, FocusChange)

WindowEventHandler::WindowEventHandler(GLWindow *window)
    : d(new Impl(this, window))
{}

void WindowEventHandler::trapMouse(bool trap)
{
    if (trap)
    {
        d->grabMouse();
    }
    else
    {
        d->ungrabMouse();
    }
}

void WindowEventHandler::setKeyboardMode(KeyboardMode kbMode)
{
    if (d->keyboardMode != kbMode)
    {
        d->keyboardMode = kbMode;
        if (kbMode == TextInput)
        {
            LOGDEV_INPUT_VERBOSE("Keyboard mode changed to text input");
            SDL_StartTextInput();
        }
        else
        {
            LOGDEV_INPUT_VERBOSE("Keyboard mode changed to raw key events");
            SDL_StopTextInput();
        }
    }
}

WindowEventHandler::KeyboardMode WindowEventHandler::keyboardMode() const
{
    return d->keyboardMode;
}

bool WindowEventHandler::isMouseTrapped() const
{
    return d->mouseGrabbed;
}

void WindowEventHandler::handleSDLEvent(const void *ptr)
{
    const SDL_Event *event = reinterpret_cast<const SDL_Event *>(ptr);
    switch (event->type)
    {
    case SDL_MOUSEMOTION:
        d->handleMouseMoveEvent(event->motion);
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        d->handleMouseButtonEvent(event->button);
        break;

    case SDL_MOUSEWHEEL:
        d->handleMouseWheelEvent(event->wheel);
        break;

    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
        d->handleFingerEvent(event->tfinger);
        break;

    case SDL_MULTIGESTURE:
        d->handleGestureEvent(event->mgesture);
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        d->handleKeyEvent(event->key);
        break;

    case SDL_TEXTINPUT:
        d->handleTextInput(event->text);
        break;

    case SDL_WINDOWEVENT:
        switch (event->window.event)
        {
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST:
            d->handleFocus(event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED);
            break;
        }
        break;
    }
}

#if 0

void WindowEventHandler::mouseDoubleClickEvent(QMouseEvent *ev)
{
    ev->accept();

    DE_NOTIFY(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::DoubleClick,
                                 d->translatePosition(ev)));
    }
}

void WindowEventHandler::wheelEvent(QWheelEvent *ev)
{
    ev->accept();

    const float devicePixels = d->window->devicePixelRatio();

    QPoint numPixels = ev->pixelDelta();
    QPoint numDegrees = ev->angleDelta() / 8;
    d->wheelAngleAccum += numDegrees;

    if (!numPixels.isNull())
    {
        DE_NOTIFY(MouseEvent, i)
        {
            if (numPixels.x())
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vec2i(devicePixels * numPixels.x(), 0),
                                         d->translatePosition(ev)));
            }
            if (numPixels.y())
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vec2i(0, devicePixels * numPixels.y()),
                                         d->translatePosition(ev)));
            }
        }
    }

    const QPoint steps = d->wheelAngleAccum / 15;
    if (!steps.isNull())
    {
        DE_NOTIFY(MouseEvent, i)
        {
            if (steps.x())
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step, Vec2i(steps.x(), 0),
                                         !d->mouseGrabbed? d->translatePosition(ev) : Vec2i()));
            }
            if (steps.y())
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step, Vec2i(0, steps.y()),
                                         !d->mouseGrabbed? d->translatePosition(ev) : Vec2i()));
            }
        }
        d->wheelAngleAccum -= steps * 15;
    }

    d->prevWheelAt.start();
}

#endif

} // namespace de
