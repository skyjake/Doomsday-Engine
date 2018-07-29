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
#include <de/Log>
#include <de/Loop>

#include <SDL_events.h>

namespace de {

DE_PIMPL(WindowEventHandler)
{
    GLWindow *   window;
    bool         mouseGrabbed = false;
    KeyboardMode keyboardMode = TextInput;
    Vec2i        prevMousePos;
    Time         prevWheelAt;
    Vec2i        wheelAngleAccum;
    int          wheelDir[2];
    Vec2i        currentMousePos;
    //#if defined (WIN32)
    //    bool      altIsDown = false;
    //#endif

    Impl(Public *i, GLWindow *parentWindow)
        : Base(i)
        , window(parentWindow)
    {
        wheelDir[0] = wheelDir[1] = 0;
    }

    void grabMouse()
    {
//        if (!window->isVisible()) return;

        if (!mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Grabbing mouse") << mouseGrabbed;
            mouseGrabbed = true;
            DE_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i) { i->mouseStateChanged(Trapped); }
        }
    }

    void ungrabMouse()
    {
//        if (!window->isVisible()) return;

        if (mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Ungrabbing mouse");
            mouseGrabbed = false;
            DE_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i) { i->mouseStateChanged(Untrapped); }
        }
    }

#if 0
    static int nativeCode(QKeyEvent const *ev)
    {
#if defined(UNIX) && !defined(MACOSX)
        return ev->nativeScanCode();
#else
        return ev->nativeVirtualKey();
#endif
    }
#endif

    void handleTextInput(const SDL_TextInputEvent &ev)
    {
        KeyEvent keyEvent(KeyEvent::Pressed, 0, 0, 0, ev.text);
        DE_FOR_PUBLIC_AUDIENCE2(KeyEvent, i)
        {
            i->keyEvent(keyEvent);
        }
    }

    void handleKeyEvent(const SDL_KeyboardEvent &ev)
    {
        debug("text input active: %i", SDL_IsTextInputActive());

        const int ddKey = KeyEvent::ddKeyFromSDL(ev.keysym.sym, ev.keysym.scancode);
        KeyEvent keyEvent(ev.state == SDL_PRESSED
                              ? (ev.repeat ? KeyEvent::Repeat : KeyEvent::Pressed)
                              : KeyEvent::Released,
                          ddKey,
                          ev.keysym.sym,
                          ev.keysym.scancode,
                          String(),
                          KeyEvent::modifiersFromSDL(ev.keysym.mod));

        DE_FOR_PUBLIC_AUDIENCE2(KeyEvent, i)
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
        //debug("focus: %i", focusGained);
        DE_FOR_PUBLIC_AUDIENCE2(FocusChange, i)
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
        const auto pos = Vec2i(ev.x, ev.y) * DE_GUI_APP->dpiFactor();

        if (ev.type == SDL_MOUSEBUTTONDOWN)
        {
            DE_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(translateButton(ev.button), MouseEvent::Pressed, pos));
            }
            if (ev.clicks == 2)
            {
                DE_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
                {
                    i->mouseEvent(
                        MouseEvent(translateButton(ev.button), MouseEvent::DoubleClick, pos));
                }
            }
        }
        else
        {
            DE_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(translateButton(ev.button), MouseEvent::Released, pos));
            }
        }
    }

    void handleMouseMoveEvent(const SDL_MouseMotionEvent &ev)
    {
        currentMousePos = Vec2i(ev.x, ev.y) * DE_GUI_APP->dpiFactor();

        // Absolute events are only emitted when the mouse is untrapped.
        if (!mouseGrabbed)
        {
            DE_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Absolute, currentMousePos));
            }
        }
        else
        {
            DE_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Relative, Vec2i(ev.xrel, ev.yrel)));
            }
        }
    }

    void handleMouseWheelEvent(const SDL_MouseWheelEvent &ev)
    {
        DE_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
        {
            if (ev.x)
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vec2i(ev.x, 0),
                                         currentMousePos));
            }
            if (ev.y)
            {
                i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vec2i(0, ev.y),
                                         currentMousePos));
            }
        }

        /*
        float const devicePixels = d->window->devicePixelRatio();

        QPoint numPixels = ev->pixelDelta();
        QPoint numDegrees = ev->angleDelta() / 8;
        d->wheelAngleAccum += numDegrees;

        if (!numPixels.isNull())
        {
            DE_FOR_AUDIENCE2(MouseEvent, i)
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

        QPoint const steps = d->wheelAngleAccum / 15;
        if (!steps.isNull())
        {
            DE_FOR_AUDIENCE2(MouseEvent, i)
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

    DE_PIMPL_AUDIENCE(FocusChange)
};

DE_AUDIENCE_METHOD(WindowEventHandler, FocusChange)

WindowEventHandler::WindowEventHandler(GLWindow *window)
    : d(new Impl(this, window))
{
    //setMouseTracking(true);
    //setFocusPolicy(Qt::StrongFocus);
}

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
            LOG_INPUT_MSG("Keyboard mode changed to text input");
            SDL_StartTextInput();
        }
        else
        {
            LOG_INPUT_MSG("Keyboard mode changed to raw key events");
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
void WindowEventHandler::focusInEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INPUT_VERBOSE("Gained focus");

    DE_FOR_AUDIENCE2(FocusChange, i) i->windowFocusChanged(*d->window, true);
}

void WindowEventHandler::focusOutEvent(QFocusEvent*)
{
    LOG_AS("Canvas");
    LOG_INPUT_VERBOSE("Lost focus");

    // Automatically ungrab the mouse if focus is lost.
    d->ungrabMouse();

    DE_FOR_AUDIENCE2(FocusChange, i) i->windowFocusChanged(*d->window, false);
}

void WindowEventHandler::keyPressEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

void WindowEventHandler::keyReleaseEvent(QKeyEvent *ev)
{
    d->handleKeyEvent(ev);
}

static MouseEvent::Button translateButton(Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton)   return MouseEvent::Left;
    if (btn == Qt::MiddleButton) return MouseEvent::Middle;
    if (btn == Qt::RightButton)  return MouseEvent::Right;
    if (btn == Qt::XButton1)     return MouseEvent::XButton1;
    if (btn == Qt::XButton2)     return MouseEvent::XButton2;

    return MouseEvent::Unknown;
}

void WindowEventHandler::mousePressEvent(QMouseEvent *ev)
{
    ev->accept();

    DE_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Pressed,
                                 d->translatePosition(ev)));
    }
}

void WindowEventHandler::mouseReleaseEvent(QMouseEvent* ev)
{
    ev->accept();

    DE_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Released,
                                 d->translatePosition(ev)));
    }
}

void WindowEventHandler::mouseDoubleClickEvent(QMouseEvent *ev)
{
    ev->accept();

    DE_FOR_AUDIENCE2(MouseEvent, i)
    {
        i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::DoubleClick,
                                 d->translatePosition(ev)));
    }
}

void WindowEventHandler::mouseMoveEvent(QMouseEvent *ev)
{
    ev->accept();

    // Absolute events are only emitted when the mouse is untrapped.
    if (!d->mouseGrabbed)
    {
        DE_FOR_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::Absolute,
                                     d->translatePosition(ev)));
        }
    }
}

void WindowEventHandler::wheelEvent(QWheelEvent *ev)
{
    ev->accept();

    float const devicePixels = d->window->devicePixelRatio();

    QPoint numPixels = ev->pixelDelta();
    QPoint numDegrees = ev->angleDelta() / 8;
    d->wheelAngleAccum += numDegrees;

    if (!numPixels.isNull())
    {
        DE_FOR_AUDIENCE2(MouseEvent, i)
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

    QPoint const steps = d->wheelAngleAccum / 15;
    if (!steps.isNull())
    {
        DE_FOR_AUDIENCE2(MouseEvent, i)
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
