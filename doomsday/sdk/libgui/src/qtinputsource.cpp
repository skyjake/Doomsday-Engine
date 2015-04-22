/** @file qtinputsource.cpp  Input event source based on Qt events.
 *
 * @authors Copyright (c) 2012-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Canvas"
#include "de/GLState"
#include "de/GLTexture"
#include "de/graphics/opengl.h"

#include <de/App>
#include <de/Log>
#include <de/Drawable>
#include <de/GLInfo>
#include <de/GLFramebuffer>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QImage>
#include <QTimer>
#include <QTime>
#include <QDebug>

namespace de {

DENG2_PIMPL(QtInputSource)
{
    QObject *fallback = nullptr;
#ifdef WIN32
    bool altIsDown = false;
#endif
    bool mouseGrabbed = false;
    QPoint prevMousePos;
    QTime prevWheelAt;
    QPoint wheelAngleAccum;
    int wheelDir[2];

    Instance(Public *i, QObject &fallbackHandler)
        : Base(i)
        , fallback(&fallbackHandler)
    {
        zap(wheelDir);
    }

    void grabMouse()
    {
        //if(!self.isVisible()) return;

        if(!mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Grabbing mouse") << mouseGrabbed;

            mouseGrabbed = true;

            DENG2_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i)
            {
                i->mouseStateChanged(Trapped);
            }
        }
    }

    void ungrabMouse()
    {
        //if(!self.isVisible()) return;

        if(mouseGrabbed)
        {
            LOG_INPUT_VERBOSE("Ungrabbing mouse");

            // Tell the mouse driver that the mouse is untrapped.
            mouseGrabbed = false;

            DENG2_FOR_PUBLIC_AUDIENCE2(MouseStateChange, i)
            {
                i->mouseStateChanged(Untrapped);
            }
        }
    }

    static int nativeCode(QKeyEvent const *ev)
    {
#if defined(UNIX) && !defined(MACOSX)
        return ev->nativeScanCode();
#else
        return ev->nativeVirtualKey();
#endif
    }

    void handleKeyEvent(QKeyEvent *ev)
    {
        ev->accept();

        /*
        qDebug() << "Canvas: key press" << ev->key() << QString("0x%1").arg(ev->key(), 0, 16)
                 << "text:" << ev->text()
                 << "native:" << ev->nativeVirtualKey()
                 << "scancode:" << ev->nativeScanCode();
        */

#ifdef WIN32
        // We must track the state of the alt key ourselves as the OS grabs the up event...
        if(ev->key() == Qt::Key_Alt)
        {
            if(ev->type() == QEvent::KeyPress)
            {
                if(altIsDown) return; // Ignore repeat down events(!)?
                altIsDown = true;
            }
            else if(ev->type() == QEvent::KeyRelease)
            {
                if(!altIsDown)
                {
                    LOG_DEBUG("Ignoring repeat alt up.");
                    return; // Ignore repeat up events.
                }
                altIsDown = false;
                //LOG_DEBUG("Alt is up.");
            }
        }
#endif

        DENG2_FOR_PUBLIC_AUDIENCE2(KeyEvent, i)
        {
            i->keyEvent(KeyEvent(ev->isAutoRepeat()?             KeyEvent::Repeat :
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
        }
    }

    template <typename QtEventType>
    Vector2i translatePosition(QtEventType const *ev) const
    {
#ifdef DENG2_QT_5_1_OR_NEWER
        return Vector2i(ev->pos().x(), ev->pos().y()) * qApp->devicePixelRatio();
#else
        return Vector2i(ev->pos().x(), ev->pos().y());
#endif
    }

    void focusInEvent(QFocusEvent *)
    {
        LOG_AS("QtInputSource");
        LOG_INPUT_VERBOSE("Gained focus");

        DENG2_FOR_PUBLIC_AUDIENCE2(FocusChange, i) i->inputFocusChanged(true);
    }

    void focusOutEvent(QFocusEvent *)
    {
        LOG_AS("QtInputSource");
        LOG_INPUT_VERBOSE("Lost focus");

        // Automatically ungrab the mouse if focus is lost.
        ungrabMouse();

        DENG2_FOR_PUBLIC_AUDIENCE2(FocusChange, i) i->inputFocusChanged(false);
    }

    void keyPressEvent(QKeyEvent *ev)
    {
        handleKeyEvent(ev);
    }

    void keyReleaseEvent(QKeyEvent *ev)
    {
        handleKeyEvent(ev);
    }

    static MouseEvent::Button translateButton(Qt::MouseButton btn)
    {
        if(btn == Qt::LeftButton)   return MouseEvent::Left;
        if(btn == Qt::MiddleButton) return MouseEvent::Middle;
        if(btn == Qt::RightButton)  return MouseEvent::Right;
        if(btn == Qt::XButton1)     return MouseEvent::XButton1;
        if(btn == Qt::XButton2)     return MouseEvent::XButton2;

        return MouseEvent::Unknown;
    }

    void mousePressEvent(QMouseEvent *ev)
    {
        ev->accept();

        DENG2_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Pressed,
                                     translatePosition(ev)));
        }
    }

    void mouseReleaseEvent(QMouseEvent* ev)
    {
        ev->accept();

        DENG2_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(translateButton(ev->button()), MouseEvent::Released,
                                     translatePosition(ev)));
        }
    }

    void mouseMoveEvent(QMouseEvent *ev)
    {
        ev->accept();

        // Absolute events are only emitted when the mouse is untrapped.
        if(!mouseGrabbed)
        {
            DENG2_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Absolute, translatePosition(ev)));
            }
        }
    }

    void wheelEvent(QWheelEvent *ev)
    {
        ev->accept();

#ifdef DENG2_QT_5_0_OR_NEWER
        float const devicePixels = qApp->devicePixelRatio();

        QPoint numPixels  = ev->pixelDelta();
        QPoint numDegrees = ev->angleDelta() / 8;
        wheelAngleAccum += numDegrees;

        if(!numPixels.isNull())
        {
            DENG2_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                if(numPixels.x())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vector2i(devicePixels * numPixels.x(), 0),
                                             translatePosition(ev)));
                }
                if(numPixels.y())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::FineAngle, Vector2i(0, devicePixels * numPixels.y()),
                                             translatePosition(ev)));
                }
            }
        }

        QPoint const steps = wheelAngleAccum / 15;
        if(!steps.isNull())
        {
            DENG2_FOR_PUBLIC_AUDIENCE2(MouseEvent, i)
            {
                if(steps.x())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::Step, Vector2i(steps.x(), 0),
                                             !mouseGrabbed? translatePosition(ev) : Vector2i()));
                }
                if(steps.y())
                {
                    i->mouseEvent(MouseEvent(MouseEvent::Step, Vector2i(0, steps.y()),
                                             !mouseGrabbed? translatePosition(ev) : Vector2i()));
                }
            }
            wheelAngleAccum -= steps * 15;
        }

#else
        static const int MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS = 100;

        bool continuousMovement = (prevWheelAt.elapsed() < MOUSE_WHEEL_CONTINUOUS_THRESHOLD_MS);
        int axis = (ev->orientation() == Qt::Horizontal? 0 : 1);
        int dir  = (ev->delta() < 0? -1 : 1);

        DENG2_FOR_AUDIENCE2(MouseEvent, i)
        {
            i->mouseEvent(MouseEvent(MouseEvent::FineAngle,
                                     axis == 0? Vector2i(ev->delta(), 0) :
                                                Vector2i(0, ev->delta()),
                                     translatePosition(ev)));
        }

        if(!continuousMovement || wheelDir[axis] != dir)
        {
            wheelDir[axis] = dir;

            DENG2_FOR_AUDIENCE2(MouseEvent, i)
            {
                i->mouseEvent(MouseEvent(MouseEvent::Step,
                                         axis == 0? Vector2i(dir, 0) :
                                         axis == 1? Vector2i(0, dir) : Vector2i(),
                                         !mouseGrabbed? translatePosition(ev) : Vector2i()));
            }
        }
#endif

        prevWheelAt.start();
    }

    DENG2_PIMPL_AUDIENCE(FocusChange)
};

DENG2_AUDIENCE_METHOD(QtInputSource, FocusChange)

QtInputSource::QtInputSource(QObject &fallbackHandler)
    : d(new Instance(this, fallbackHandler))
{}

void QtInputSource::trapMouse(bool trap)
{
    if(trap)
    {
        d->grabMouse();
    }
    else
    {
        d->ungrabMouse();
    }
}

bool QtInputSource::isMouseTrapped() const
{
    return d->mouseGrabbed;
}

bool QtInputSource::eventFilter(QObject *watched, QEvent *event)
{
    switch(event->type())
    {
    case QEvent::FocusIn:
        d->focusInEvent(static_cast<QFocusEvent *>(event));
        return true;

    case QEvent::FocusOut:
        d->focusOutEvent(static_cast<QFocusEvent *>(event));
        return true;

    case QEvent::KeyPress:
        d->keyPressEvent(static_cast<QKeyEvent *>(event));
        return true;

    case QEvent::KeyRelease:
        d->keyReleaseEvent(static_cast<QKeyEvent *>(event));
        return true;

    case QEvent::MouseButtonPress:
        d->mousePressEvent(static_cast<QMouseEvent *>(event));
        return true;

    case QEvent::MouseButtonRelease:
        d->mouseReleaseEvent(static_cast<QMouseEvent *>(event));
        return true;

    case QEvent::MouseMove:
        d->mouseMoveEvent(static_cast<QMouseEvent *>(event));
        return true;

    case QEvent::Wheel:
        d->wheelEvent(static_cast<QWheelEvent *>(event));
        return true;

    default:
        break;
    }
    return d->fallback->eventFilter(watched, event);
}

} // namespace de
