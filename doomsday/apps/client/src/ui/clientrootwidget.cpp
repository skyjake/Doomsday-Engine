/** @file clientrootwidget.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/clientrootwidget.h"
#include "ui/clientwindow.h"
#include "ui/inputsystem.h"
#include "clientapp.h"

using namespace de;

DE_PIMPL_NOREF(ClientRootWidget)
{
    PackageIconBank packageIconBank;
};

ClientRootWidget::ClientRootWidget(GLWindow *window)
    : GuiRootWidget(window)
    , d(new Impl)
{
    d->packageIconBank.setDisplaySize(GuiWidget::pointsToPixels(PackageIconBank::Size(32, 32)));
}

ClientWindow &ClientRootWidget::window()
{
    return GuiRootWidget::window().as<ClientWindow>();
}

PackageIconBank &ClientRootWidget::packageIconBank()
{
    return d->packageIconBank;
}

void ClientRootWidget::update()
{
    if (!DoomsdayApp::app().isShuttingDown())
    {
        GuiRootWidget::update();

        if (window().isGLReady() && !d->packageIconBank.atlas())
        {
            d->packageIconBank.setAtlas(&atlas());
        }
    }
}

void ClientRootWidget::addOnTop(GuiWidget *widget)
{
    // The window knows what is the correct top to add to.
    window().addOnTop(widget);
}

//void ClientRootWidget::dispatchLatestMousePosition()
//{
//    ClientApp::windowSystem().dispatchLatestMousePosition();
//}

void ClientRootWidget::handleEventAsFallback(const Event &event)
{
    // The bindings might have use for this event.
    ClientApp::input().tryEvent(event, "global");
}
