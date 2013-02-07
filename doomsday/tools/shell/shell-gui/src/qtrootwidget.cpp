/** @file qtrootwidget.cpp  Root widget that works with a Qt canvas->
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "qtrootwidget.h"
#include "qttextcanvas.h"
#include <de/shell/KeyEvent>
#include <de/AnimationVector>
#include <de/Clock>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

using namespace de;
using namespace de::shell;

const int BLINK_INTERVAL = 500; // ms

#ifdef MACOSX
#  define CONTROL_MOD   Qt::MetaModifier
#else
#  define CONTROL_MOD   Qt::ControlModifier
#endif

DENG2_PIMPL(QtRootWidget)
{
    int margin;
    Vector2i charSize;
    QtTextCanvas *canvas;
    TextRootWidget root;
    QFont font;
    QTimer *blinkTimer;
    QTimer *cursorTimer;
    bool blinkVisible;
    bool cursorVisible;
    QPoint origin;

    Instance(Public &inst) : Private(inst),
        margin(4),
        canvas(new QtTextCanvas(Vector2i(1, 1))),
        root(canvas),
        blinkTimer(0),
        cursorTimer(0),
        blinkVisible(true),
        cursorVisible(true)
    {
        canvas->setForegroundColor(QColor(40, 40, 50));
        canvas->setBackgroundColor(QColor(210, 210, 220));
    }

    void setFont(QFont const &fnt)
    {
        font = fnt;

        QFontMetrics metrics(font);
        charSize.x = metrics.width('W');
        charSize.y = metrics.lineSpacing();

        canvas->setFont(font);
        canvas->setCharSize(charSize);
    }

    void updateSize(int widthPx, int heightPx)
    {
        if(!charSize.x || !charSize.y) return;

        // Determine number of characters that fits in the new size.
        Vector2i size((widthPx - 2*margin) / charSize.x, (heightPx - 2*margin) / charSize.y);
        root.setViewSize(size);

        //origin = QPoint((widthPx  - canvas->image().width())  / 2,
        //                (heightPx - canvas->image().height()) / 2);
        origin = QPoint(margin, heightPx - canvas->image().height() - margin);
    }
};

QtRootWidget::QtRootWidget(QWidget *parent)
    : QWidget(parent), d(new Instance(*this))
{
    setFocusPolicy(Qt::StrongFocus);

    // Blinking timers.
    d->blinkTimer = new QTimer(this);
    connect(d->blinkTimer, SIGNAL(timeout()), this, SLOT(blink()));
    d->blinkTimer->start(BLINK_INTERVAL);

    d->cursorTimer = new QTimer(this);
    connect(d->cursorTimer, SIGNAL(timeout()), this, SLOT(cursorBlink()));
    d->cursorTimer->start(BLINK_INTERVAL);
}

QtRootWidget::~QtRootWidget()
{
    delete d;
}

TextRootWidget &QtRootWidget::rootWidget()
{
    return d->root;
}

void QtRootWidget::setFont(QFont const &font)
{
    d->setFont(font);
    d->updateSize(width(), height());

    setMinimumSize(d->charSize.x * 40 + 2 * d->margin,
                   d->charSize.y * 6 + 2 * d->margin);
}

void QtRootWidget::keyPressEvent(QKeyEvent *ev)
{
    bool eaten;

    /*
    qDebug() << "key:" << QString::number(ev->key(), 16) << "text:" << ev->text()
             << "mods:" << ev->modifiers();
    */

    if(!ev->text().isEmpty() && ev->text()[0].isPrint() &&
            !ev->modifiers().testFlag(CONTROL_MOD))
    {
        KeyEvent event(ev->text());
        eaten = d->root.processEvent(&event);
    }
    else
    {
        int key = ev->key();
        KeyEvent::Modifiers mods = ev->modifiers().testFlag(CONTROL_MOD)?
                    KeyEvent::Control : KeyEvent::None;

        // Special control key mappings.
        if(mods & KeyEvent::Control)
        {
            switch(key)
            {
            case Qt::Key_A:
                key = Qt::Key_Home;
                mods = KeyEvent::None;
                break;

            case Qt::Key_D:
                key = Qt::Key_Delete;
                mods = KeyEvent::None;
                break;

            case Qt::Key_E:
                key = Qt::Key_End;
                mods = KeyEvent::None;
                break;

            default:
                break;
            }
        }

        KeyEvent event(key, mods);
        eaten = d->root.processEvent(&event);
    }

    if(eaten)
    {
        ev->accept();

        // Restart cursor blink.
        d->cursorVisible = true;
        d->cursorTimer->stop();
        d->cursorTimer->start(BLINK_INTERVAL);

        update();
    }
    else
    {
        ev->ignore();
        QWidget::keyPressEvent(ev);
    }
}

void QtRootWidget::resizeEvent(QResizeEvent *ev)
{
    d->updateSize(width(), height());

    QWidget::resizeEvent(ev);
}

void QtRootWidget::paintEvent(QPaintEvent *)
{
    Clock::appClock().setTime(Time());

    d->canvas->setBlinkVisible(d->blinkVisible);

    // Update changed portions.
    d->root.update();
    d->root.draw();

    QSize size(width(), height());

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRect(QPoint(0, 0), size), d->canvas->backgroundColor());

    QImage const &buf = d->canvas->image();
    QPoint origin = d->origin;

    painter.drawImage(origin, buf);

    // Blinking cursor.
    if(d->cursorVisible)
    {
        QPoint pos = origin + QPoint(d->charSize.x * d->canvas->cursorPosition().x,
                                     d->charSize.y * d->canvas->cursorPosition().y);

        TextCanvas::Char ch = d->canvas->at(d->canvas->cursorPosition());

        painter.setPen(Qt::NoPen);
        painter.fillRect(QRect(pos, QSize(de::max(1, d->charSize.x / 5), d->charSize.y)),
                         ch.attribs.testFlag(TextCanvas::Char::Reverse)?
                             d->canvas->backgroundColor() : d->canvas->foregroundColor());
    }
}

void QtRootWidget::blink()
{
    d->blinkVisible = !d->blinkVisible;
    update();
}

void QtRootWidget::cursorBlink()
{
    d->cursorVisible = !d->cursorVisible;
    update();
}
