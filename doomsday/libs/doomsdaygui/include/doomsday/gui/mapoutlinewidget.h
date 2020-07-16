/** @file mapoutlinewidget.h  Shows a map outline.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include <doomsday/network/protocol.h>
#include <de/guiwidget.h>

/**
 * Map outline visualizing information from a shell::MapOutlinePacket.
 */
class MapOutlineWidget : public de::GuiWidget
{
public:
    MapOutlineWidget(const de::String &name = {});

    void setColors(const de::DotPath &oneSidedLine, const de::DotPath &twoSidedLine);
    void setOutline(const network::MapOutlinePacket &mapOutline);
    void setPlayerInfo(const network::PlayerInfoPacket &plrInfo);

    // Events.
    void drawContent() override;
    void update() override;
    void viewResized() override;

protected:
    void glInit() override;
    void glDeinit() override;

private:
    DE_PRIVATE(d)
};
