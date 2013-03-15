/** @file statuswidget.cpp  Widget for showing server's status.
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

#include "statuswidget.h"
#include <de/libdeng2.h>
#include <de/String>
#include <de/shell/DoomsdayInfo>
#include <QPainter>
#include <QPicture>
#include <QTimer>

using namespace de;

DENG2_PIMPL(StatusWidget)
{
    QFont smallFont;
    QFont largeFont;
    QFont playerFont;
    String gameMode;
    String map;
    QPicture mapOutline;
    QRect mapBounds;
    shell::Link *link;

    typedef shell::PlayerInfoPacket::Player Player;
    shell::PlayerInfoPacket::Players players;
    QMap<int, QPoint> oldPlayerPositions;

    Instance(Public &i) : Base(i), link(0)
    {}

    void clear()
    {
        gameMode.clear();
        map.clear();
        mapBounds = QRect();
        mapOutline = QPicture();
        oldPlayerPositions.clear();
        players.clear();
    }
};

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent), d(new Instance(*this))
{
    d->playerFont = d->smallFont = d->largeFont = font();
    d->smallFont.setPointSize(font().pointSize() * 3 / 4);
    d->largeFont.setPointSize(font().pointSize() * 3 / 2);
    d->largeFont.setBold(true);
    d->playerFont.setPointSizeF(font().pointSizeF() * .8f);
}

void StatusWidget::setGameState(QString mode, QString rules, QString mapId, QString mapTitle)
{
    d->gameMode = shell::DoomsdayInfo::titleForGameMode(mode);
    if(!rules.isEmpty()) d->gameMode = rules + " - " + d->gameMode;

    d->map = mapTitle;
    if(!mapId.isEmpty() && !mapTitle.contains(mapId))
    {
        d->map += " (" + mapId + ")";
    }

    update();
}

void StatusWidget::setMapOutline(shell::MapOutlinePacket const &outline)
{
    d->mapBounds = QRect();
    d->mapOutline = QPicture();

    QPainter painter(&d->mapOutline);
    for(int i = 0; i < outline.lineCount(); ++i)
    {
        shell::MapOutlinePacket::Line const &ln = outline.line(i);
        painter.setPen(ln.type == shell::MapOutlinePacket::OneSidedLine? Qt::black : Qt::gray);

        QPoint a(ln.start.x, -ln.start.y);
        QPoint b(ln.end.x, -ln.end.y);

        painter.drawLine(a, b);

        if(!i)
            d->mapBounds = QRect(a, QSize(1, 1));
        else
            d->mapBounds = d->mapBounds.united(QRect(a, QSize(1, 1)));

        d->mapBounds = d->mapBounds.united(QRect(b, QSize(1, 1)));
    }

    update();
}

void StatusWidget::setPlayerInfo(shell::PlayerInfoPacket const &plrInfo)
{
    foreach(Instance::Player const &plr, d->players)
    {
        d->oldPlayerPositions[plr.number] = QPoint(plr.position.x, -plr.position.y);
    }

    d->players = plrInfo.players();
    update();
}

void StatusWidget::paintEvent(QPaintEvent *)
{
    if(!d->link)
    {
        return;
    }

    QFontMetrics metrics(font());
    QFontMetrics largeMetrics(d->largeFont);
    QColor const dim(0, 0, 0, 160);
    int lineHeight = metrics.lineSpacing();
    QPainter painter(this);

    painter.setFont(font());
    painter.setPen(dim);
    painter.drawText(QRect(0, 10, width(), lineHeight), d->gameMode, QTextOption(Qt::AlignCenter));

    painter.setFont(d->largeFont);
    painter.setPen(Qt::black);
    painter.drawText(QRect(0, 15 + lineHeight, width(), largeMetrics.lineSpacing()),
                     d->map, QTextOption(Qt::AlignCenter));

    QRect outlineRect(QPoint(20, 15 + lineHeight + largeMetrics.lineSpacing() + 15),
                      QPoint(width() - 20, height() - 20));

    if(!d->mapBounds.isNull())
    {
        painter.setWindow(d->mapBounds);

        // First try fitting horizontally.
        float mapRatio = float(d->mapBounds.width()) / float(d->mapBounds.height());
        QSize viewSize;
        viewSize.setWidth(outlineRect.width());
        viewSize.setHeight(outlineRect.width() / mapRatio);
        if(viewSize.height() > outlineRect.height())
        {
            // Doesn't fit this way, fit vertically instead.
            viewSize.setHeight(outlineRect.height());
            viewSize.setWidth(outlineRect.height() * mapRatio);
        }
        painter.setViewport(QRect(outlineRect.center().x() - viewSize.width()/2,
                                  outlineRect.center().y() - viewSize.height()/2,
                                  viewSize.width(), viewSize.height()));

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawPicture(0, 0, d->mapOutline);

        // Draw player markers.
        float const factor = float(d->mapBounds.width()) / float(viewSize.width());
        QFontMetrics const metrics(d->playerFont);
        foreach(Instance::Player const &plr, d->players.values())
        {
            painter.save();

            QColor const color(plr.color.x, plr.color.y, plr.color.z);

            QColor markColor = color;
            markColor.setAlpha(180);

            QPoint plrPos(plr.position.x, -plr.position.y);

            if(d->oldPlayerPositions.contains(plr.number))
            {
                QPointF const start = d->oldPlayerPositions[plr.number];
                QPointF const end   = plrPos;
                QPointF const delta = end - start;

                /// @todo Qt has no gradient support for drawing lines?

                int const STOPS = 64;
                for(int i = 0; i < STOPS; ++i)
                {
                    QColor grad = color;
                    grad.setAlpha(i * 100 / STOPS);
                    float a = float(i) / float(STOPS);
                    float b = float(i + 1) / float(STOPS);
                    QPen gradPen(grad);
                    gradPen.setWidthF(2 * factor);
                    painter.setPen(gradPen);
                    painter.drawLine(start + a * delta, start + b * delta);
                }
            }

            painter.setTransform(QTransform::fromScale(factor, factor) *
                                 QTransform::fromTranslate(plrPos.x(), plrPos.y()));

            painter.setPen(Qt::black);
            painter.setBrush(markColor);
            painter.drawEllipse(QPoint(0, 0), 4, 4);
            painter.drawLine(QPoint(0, 4), QPoint(0, 10));
            markColor.setAlpha(160);
            painter.setBrush(markColor);

            QString label = QString("%1: %2").arg(plr.number).arg(plr.name);
            if(label.size() > 20) label = label.left(20);

            QRect textBounds = metrics.boundingRect(label);
            int const gap = 3;
            textBounds.moveTopLeft(QPoint(-textBounds.width()/2, 10 + gap));
            QRect boxBounds = textBounds.adjusted(-gap, -gap, gap, metrics.descent() + gap);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(boxBounds, 2, 2);

            painter.setFont(d->playerFont);

            // Label text with a shadow.
            bool const isDark = ((color.red() + color.green()*2 + color.blue())/3 < 140);
            painter.setPen(isDark? Qt::black : Qt::white);
            painter.drawText(textBounds.topLeft() + QPoint(0, metrics.ascent()), label);
            painter.setPen(isDark? Qt::white : Qt::black);
            painter.drawText(textBounds.topLeft() + QPoint(0, metrics.ascent() - 1), label);

            painter.restore();
        }
    }
}

/*
void StatusWidget::updateWhenConnected()
{
    if(d->link)
    {
        update();
        QTimer::singleShot(1000, this, SLOT(updateWhenConnected()));
    }
}
*/

void StatusWidget::linkConnected(shell::Link *link)
{
    d->link = link;
    update();
}

void StatusWidget::linkDisconnected()
{
    d->link = 0;
    d->clear();
    update();
}
