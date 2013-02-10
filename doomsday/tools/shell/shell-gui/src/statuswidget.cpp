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
#include <QTimer>

using namespace de;

DENG2_PIMPL(StatusWidget)
{
    QFont smallFont;
    QFont largeFont;
    String gameMode;
    String map;
    shell::Link *link;

    Instance(Public &i) : Private(i), link(0)
    {
        //gameMode = "Ultimate DOOM";
        //map = "E1M3";
    }

    void clear()
    {
        gameMode.clear();
        map.clear();
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
    if(!rules.isEmpty()) d->gameMode += " - " + rules;

    d->map = mapTitle;
    if(!mapId.isEmpty() && !mapTitle.contains(mapId))
    {
        d->map += " (" + mapId + ")";
    }

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
