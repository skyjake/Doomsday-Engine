/** @file gamewidget.h  Widget for legacy UI components.
 * @ingroup gui
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/guiwidget.h>

/**
 * Widget for drawing the game world.
 *
 * @ingroup gui
 */
class GameWidget : public de::GuiWidget
{
public:
    GameWidget(const de::String &name = "game");

    /**
     * Pauses the game, if one is currently running and pausing is allowed.
     */
    void pause();

    void drawComposited();

    void renderCubeMap(uint size, const de::String &outputImagePath);

    // Events.
    void viewResized() override;
    void update() override;
    void drawContent() override;
    bool handleEvent(const de::Event &event) override;

protected:
    void glDeinit() override;

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_GAMEWIDGET_H
