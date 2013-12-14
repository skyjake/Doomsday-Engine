/** @file gamewidget.h  Widget for legacy UI components.
 * @ingroup gui
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_GAMEWIDGET_H
#define CLIENT_GAMEWIDGET_H

#include "GuiWidget"

/**
 * Widget for drawing the game world.
 *
 * @ingroup gui
 */
class GameWidget : public GuiWidget
{
public:
    GameWidget(de::String const &name = "game");

    /**
     * Convenience method for changing and immediately applying a new GL
     * viewport. The viewport is automatically normalized in relation to the
     * root view size.
     *
     * This is only intended to support old graphics code that doesn't use libgui.
     */
    void glApplyViewport(de::Rectanglei const &rect);

    void viewResized();
    void update();
    void drawContent();
    bool handleEvent(de::Event const &event);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_GAMEWIDGET_H
