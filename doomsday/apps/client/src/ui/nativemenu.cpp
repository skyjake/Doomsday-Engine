/** @file nativemenu.cpp  Native menu with application-level functions.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/nativemenu.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "updater.h"
#include "clientapp.h"

#include <doomsday/Games>
#include <doomsday/console/exec.h>
#include <QMenuBar>
#include <QMenu>

using namespace de;

DE_PIMPL(NativeMenu)
, DE_OBSERVES(Games, Readiness)
{
    QMenuBar *menuBar = nullptr;
    QMenu *helpMenu = nullptr;
    QMenu *gameMenu = nullptr;
    LoopCallback mainCall;

    Impl(Public *i) : Base(i)
    {
#ifdef MACOSX
        menuBar = new QMenuBar;

        gameMenu = menuBar->addMenu(QObject::tr("&Game"));

        helpMenu = menuBar->addMenu(QObject::tr("&Help"));
        helpMenu->addAction(tr("About Doomsday"),
                            &ClientWindow::main().taskBar(),
                            SLOT(showAbout()));
        QAction *checkForUpdates = helpMenu->addAction(tr("Check For &Updates..."),
                                                       &ClientApp::updater(),
                                                       SLOT(checkNowShowingProgress()));
        checkForUpdates->setMenuRole(QAction::ApplicationSpecificRole);

        updateGameMenuItems();
        DoomsdayApp::games().audienceForReadiness() += this;
#endif
    }

    void gameReadinessUpdated() override
    {
#ifdef MACOSX
        mainCall.enqueue([this] () { updateGameMenuItems(); });
#endif
    }

    void updateGameMenuItems()
    {
#ifdef MACOSX
        if (!gameMenu) return;

        gameMenu->clear();

        QList<Game *> allGames = DoomsdayApp::games().all();
        qSort(allGames.begin(), allGames.end(), [] (Game const *a, Game const *b) {
            return a->id().compare(b->id()) < 0;
        });

        for (Game *game : allGames)
        {
            QAction *load = new QAction(tr("Load %1").arg(game->title()), thisPublic);
            load->setData(game->id());
            load->setEnabled(game->isPlayable());
            connect(load, &QAction::triggered, [load] () {
                auto &win = ClientWindow::main();
                win.glActivate();
                Con_Executef(CMDS_DDAY, false, "load %s",
                             load->data().toString());
                win.glDone();
            });
            gameMenu->addAction(load);
        }
#endif
    }
};

NativeMenu::NativeMenu()
    : d(new Impl(this))
{}
