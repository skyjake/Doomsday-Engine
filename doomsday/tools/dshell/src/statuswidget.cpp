/** @file statuswidget.cpp  Widget for status information.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/timer.h>
#include <de/term/textrootwidget.h>

using namespace de;

DE_PIMPL(StatusWidget)
{
    network::Link *link{nullptr};
    Timer          updateTimer;
    String         gameMode;
    String         rules;
    String         mapId;

    Impl(Public * i) : Base(i) {}

    void refresh() { self().redraw(); }

    void linkConnected()
    {
        updateTimer.start(1.0);
        refresh();
    }

    void linkDisconnected()
    {
        updateTimer.stop();
        refresh();
    }
};

StatusWidget::StatusWidget(const String &name)
    : Widget(name), d(new Impl(this))
{
    d->updateTimer += [this]() { d->refresh(); };
}

void StatusWidget::setShellLink(network::Link *link)
{
    d->link = link;

    if (link)
    {
        // Observe changes in link status.
        link->audienceForAddressResolved() += [this]() { d->refresh(); };
        link->audienceForConnected()       += [this]() { d->linkConnected(); };
        link->audienceForDisconnected()    += [this]() { d->linkDisconnected(); };
    }

    root().requestDraw();
}

void StatusWidget::setGameState(const String &mode, const String &rules, const String &mapId)
{
    d->gameMode = mode;
    d->rules    = rules;
    d->mapId    = mapId;

    redraw();
}

void StatusWidget::draw()
{
    Rectanglei pos = rule().recti();
    term::TextCanvas buf(pos.size());

    if (!d->link || d->link->status() == network::Link::Disconnected)
    {
        String msg = "Not connected to a server";
        buf.drawText(Vec2i(buf.size().x/2 - msg.lengthi()/2), msg /*, TextCanvas::Char::Bold*/);
    }
    else if (d->link->status() == network::Link::Connecting)
    {
        String msg;
        if (!d->link->address().isNull())
        {
            msg = "Connecting to " + d->link->address().asText();
        }
        else
        {
            msg = "Looking up host...";
        }
        buf.drawText(Vec2i(buf.size().x/2 - msg.lengthi()/2), msg);
    }
    else if (d->link->status() == network::Link::Connected)
    {
        String msg = d->gameMode;
        if (!d->mapId.isEmpty()) msg += " " + d->mapId;
        if (!d->rules.isEmpty()) msg += " (" + d->rules + ")";
        buf.drawText(Vec2i(1, 0), msg);

        TimeSpan elapsed = d->link->connectedAt().since();
        String time = Stringf("| %d:%02d:%02d",
                int(elapsed.asHours()),
                int(elapsed.asMinutes()) % 60,
                int(elapsed) % 60);
        String host = "| " + d->link->address().asText();

        int x = buf.size().x - time.lengthi() - 1;
        buf.drawText(x, time);

        x -= host.size() + 1;
        buf.drawText(x, host);
    }

    targetCanvas().draw(buf, pos.topLeft);
}


