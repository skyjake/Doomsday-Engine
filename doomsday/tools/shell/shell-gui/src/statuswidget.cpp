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
    String gameMode;
    String map;
    QPicture mapOutline;
    QRect mapBounds;
    shell::Link *link;

    Instance(Public &i) : Base(i), link(0)
    {
        //gameMode = "Ultimate DOOM";
        //map = "E1M3";
    }

    void clear()
    {
        gameMode.clear();
        map.clear();
        mapBounds = QRect();
        mapOutline = QPicture();
    }
};

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent), d(new Instance(*this))
{
    d->smallFont = d->largeFont = font();
    d->smallFont.setPointSize(font().pointSize() * 3 / 4);
    d->largeFont.setPointSize(font().pointSize() * 3 / 2);
    d->largeFont.setBold(true);


}

StatusWidget::~StatusWidget()
{
    delete d;
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
            // Doesn't fit this way, fit to vertically instead.
            viewSize.setHeight(outlineRect.height());
            viewSize.setWidth(outlineRect.height() * mapRatio);
        }
        painter.setViewport(QRect(outlineRect.center().x() - viewSize.width()/2,
                                  outlineRect.center().y() - viewSize.height()/2,
                                  viewSize.width(), viewSize.height()));

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawPicture(0, 0, d->mapOutline);
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
