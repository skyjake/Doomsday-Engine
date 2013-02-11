/** @file qttextcanvas.cpp  Text-based drawing surface for Qt.
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

#include "qttextcanvas.h"
#include <QPainter>
#include <QImage>
#include <QMap>

using namespace de;
using namespace de::shell;

struct QtTextCanvas::Instance
{
    Size dims;
    QImage backBuffer;
    Vector2i charSizePx;
    QFont font;
    QFont boldFont;
    QColor foreground;
    QColor background;
    Vector2i cursorPos;
    bool blinkVisible;

    typedef QMap<Char, QImage> Cache;
    Cache cache;

    Instance() : blinkVisible(true)
    {}

    QSize pixelSize() const
    {
        return QSize(dims.x * charSizePx.x, dims.y * charSizePx.y);
    }

    QRect bounds() const
    {
        return backBuffer.rect();
    }

    void clearCache()
    {
        cache.clear();
    }

    QImage glyph(Char const &originalChar)
    {
        // Ignore some attributes.
        Char ch = originalChar;
        ch.attribs &= ~(Char::Blink | Char::Underline);

        Cache::const_iterator found = cache.constFind(ch);
        if(found != cache.constEnd())
        {
            return found.value();
        }

        // Render a new one.
        QImage img(QSize(charSizePx.x, charSizePx.y), QImage::Format_ARGB32);
        QRect const rect = img.rect();
        QPainter painter(&img);
        painter.setPen(Qt::NoPen);
        QColor fg;
        QColor bg;
        if(!ch.attribs.testFlag(Char::Reverse))
        {
            fg = foreground;
            bg = background;
        }
        else
        {
            fg = background;
            bg = foreground;
        }

        // Draw background.
        painter.fillRect(rect, bg);

        painter.setFont(ch.attribs.testFlag(Char::Bold)? boldFont : font);
        QFontMetrics metrics(painter.font());

        // Draw the character.
        painter.setPen(fg);
        painter.setBrush(Qt::NoBrush);
        painter.drawText(0, metrics.ascent(), ch.ch);

        cache.insert(ch, img);
        return img;
    }
};

QtTextCanvas::QtTextCanvas(TextCanvas::Size const &size)
    : d(new Instance)
{
    d->dims = size;

    // Drawing will be first done here, then copied to target buffer.
    d->backBuffer = QImage(d->pixelSize(), QImage::Format_ARGB32);
}

QtTextCanvas::~QtTextCanvas()
{
    delete d;
}

QImage const &QtTextCanvas::image() const
{
    return d->backBuffer;
}

void QtTextCanvas::resize(TextCanvas::Size const &newSize)
{
    if(size() == newSize) return;

    TextCanvas::resize(newSize);

    // Existing contents were lost.
    markDirty();

    d->dims = newSize;
    d->backBuffer = QImage(d->pixelSize(), QImage::Format_ARGB32);
}

void QtTextCanvas::setCharSize(Vector2i const &pixelSize)
{
    d->clearCache();
    d->charSizePx = pixelSize;
}

void QtTextCanvas::setFont(QFont const &font)
{
    d->clearCache();

    d->font = font;

    d->boldFont = font;
    d->boldFont.setWeight(QFont::Bold);
}

void QtTextCanvas::setForegroundColor(QColor const &fg)
{
    d->clearCache();
    markDirty();
    d->foreground = fg;
}

QColor QtTextCanvas::foregroundColor() const
{
    return d->foreground;
}

void QtTextCanvas::setBackgroundColor(QColor const &bg)
{
    d->clearCache();
    markDirty();
    d->background = bg;
}

QColor QtTextCanvas::backgroundColor() const
{
    return d->background;
}

void QtTextCanvas::setCursorPosition(Coord const &pos)
{
    d->cursorPos = pos;
}

TextCanvas::Coord QtTextCanvas::cursorPosition() const
{
    return d->cursorPos;
}

void QtTextCanvas::setBlinkVisible(bool visible)
{
    d->blinkVisible = visible;
}

void QtTextCanvas::show()
{
    QPainter painter(&d->backBuffer);
    QFontMetrics metrics(d->font);

    for(int y = 0; y < height(); ++y)
    {
        for(int x = 0; x < width(); ++x)
        {
            Coord const pos(x, y);
            Char const &ch = at(pos);

            if(!ch.isDirty() && !ch.attribs.testFlag(Char::Blink))
                continue;

            QRect const rect(QPoint(x * d->charSizePx.x,
                                    y * d->charSizePx.y),
                             QSize(d->charSizePx.x, d->charSizePx.y));

            if(ch.attribs.testFlag(Char::Blink) && !d->blinkVisible)
            {
                // Blinked out.
                painter.drawImage(rect.topLeft(), d->glyph(Char(' ', ch.attribs)));
            }
            else
            {
                painter.drawImage(rect.topLeft(), d->glyph(ch));
            }

            // Select attributes.
            if(ch.attribs.testFlag(Char::Underline))
            {
                int baseline = rect.top() + metrics.ascent();
                QColor col = ch.attribs.testFlag(Char::Reverse)? d->background : d->foreground;
                col.setAlpha(160);
                painter.setPen(col);
                painter.drawLine(QPoint(rect.left(),  baseline + metrics.underlinePos()),
                                 QPoint(rect.right(), baseline + metrics.underlinePos()));
                painter.setPen(Qt::NoPen);
            }
        }
    }

    // Mark everything clean.
    TextCanvas::show();
}
