/** @file mpselectionwidget.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_MPSELECTIONWIDGET_H
#define DENG_CLIENT_MPSELECTIONWIDGET_H

#include <de/MenuWidget>
#include "network/net_main.h"

/**
 * Menu that populates itself with available multiplayer games.
 *
 * @ingroup ui
 */
class MPSelectionWidget : public de::MenuWidget
{
    Q_OBJECT

public:
    DENG2_DEFINE_AUDIENCE(Selection, void gameSelected(serverinfo_t const &info))

    enum DiscoveryMode {
        NoDiscovery,
        DiscoverUsingMaster,
        DirectDiscoveryOnly
    };

    /**
     * Action for joining a game on a multiplayer server.
     */
    class JoinAction : public de::Action
    {
    public:
        JoinAction(serverinfo_t const &sv);
        void trigger();

    private:
        DENG2_PRIVATE(d)
    };

public:
    MPSelectionWidget(DiscoveryMode discovery = NoDiscovery);

    /**
     * Enables or disables joining games by pressing the menu items in the widget.
     * By default, this is enabled. If disabled, one will only get a notification
     * about the selection.
     *
     * @param enableJoin  @c true to allow automatic joining, @c false to disallow.
     */
    void setJoinGameWhenSelected(bool enableJoin);

    void setColumns(int numberOfColumns);

    serverinfo_t const &serverInfo(de::ui::DataPos pos) const;

signals:
    void availabilityChanged();
    void gameSelected();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MPSELECTIONWIDGET_H
