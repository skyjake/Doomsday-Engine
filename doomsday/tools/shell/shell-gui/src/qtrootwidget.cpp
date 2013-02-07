/** @file qtrootwidget.cpp  Root widget that works with a Qt canvas.
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
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

using namespace de;
using namespace de::shell;

struct QtRootWidget::Instance
{
    QtRootWidget &self;
    int margin;
    Vector2i charSize;
    QtTextCanvas canvas;
    QFont font;
    QTimer *blinkTimer;
    bool blinkVisible;

    Instance(QtRootWidget &inst)
        : self(inst), margin(4), canvas(Vector2i(1, 1)),
          blinkTimer(0),
          blinkVisible(true)
    {
        canvas.setForegroundColor(QColor(40, 40, 50));
        canvas.setBackgroundColor(QColor(200, 200, 210));
    }

    void setFont(QFont const &fnt)
    {
        font = fnt;

        QFontMetrics metrics(font);
        charSize.x = metrics.width('W');
        charSize.y = metrics.lineSpacing();

        canvas.setFont(font);
        canvas.setCharSize(charSize);
    }

    void updateSize(int widthPx, int heightPx)
    {
        if(!charSize.x || !charSize.y) return;

        widthPx  -= margin * 2;
        heightPx -= margin * 2;

        // Determine number of characters that fits in the new size.
        Vector2i size(widthPx / charSize.x, heightPx / charSize.y);
        canvas.resize(size);
        self.setViewSize(size);
    }
};

QtRootWidget::QtRootWidget(QWidget *parent)
    : QWidget(parent), d(new Instance(*this))
{
    setFocusPolicy(Qt::StrongFocus);

    d->blinkTimer = new QTimer(this);
    connect(d->blinkTimer, SIGNAL(timeout()), this, SLOT(blink()));
    d->blinkTimer->start(500);
}

QtRootWidget::~QtRootWidget()
{
    delete d;
}

void QtRootWidget::setFont(QFont const &font)
{
    d->setFont(font);
    d->updateSize(width(), height());

    setMinimumSize(d->charSize.x * 80 + 2 * d->margin,
                   d->charSize.y * 24 + 2 * d->margin);
}

void QtRootWidget::keyPressEvent(QKeyEvent *ev)
{
    ev->accept();

    if(!ev->text().isEmpty())
    {
        KeyEvent event(ev->text());
        processEvent(&event);
    }
    else
    {
        KeyEvent event(ev->key(),
                       ev->modifiers().testFlag(Qt::ControlModifier)?
                           KeyEvent::Control : KeyEvent::None);
        processEvent(&event);
    }

    QWidget::keyPressEvent(ev);
}

void QtRootWidget::resizeEvent(QResizeEvent *ev)
{
    d->updateSize(width(), height());

    QWidget::resizeEvent(ev);
}

void QtRootWidget::paintEvent(QPaintEvent *)
{
    QSize size(width(), height());

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRect(QPoint(0, 0), size), d->canvas.backgroundColor());

    // Update changed portions.
    d->canvas.show();

    QImage const &buf = d->canvas.image();
    painter.drawImage(d->margin, d->margin, buf);

    // Blinking cursor.
    if(d->blinkVisible)
    {
        QPoint pos(d->margin + d->charSize.x * d->canvas.cursorPosition().x,
                   d->margin + d->charSize.y * d->canvas.cursorPosition().y);

        TextCanvas::Char ch = d->canvas.at(d->canvas.cursorPosition());

        painter.setPen(Qt::NoPen);
        painter.fillRect(QRect(pos, QSize(de::max(1, d->charSize.x / 5), d->charSize.y)),
                         ch.attribs.testFlag(TextCanvas::Char::Reverse)?
                             d->canvas.backgroundColor() : d->canvas.foregroundColor());
    }
}

void QtRootWidget::blink()
{
    d->blinkVisible = !d->blinkVisible;
    QWidget::update();
}
