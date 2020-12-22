/** @file healthiconwidget.cpp  GUI widget for -.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "hud/widgets/healthiconwidget.h"

#include "common.h"
#include "p_actor.h"

using namespace de;

static void HealthIconWidget_Draw(guidata_healthicon_t *icon, const Point2Raw *offset)
{
    DE_ASSERT(icon);
    icon->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void HealthIconWidget_UpdateGeometry(guidata_healthicon_t *icon)
{
    DE_ASSERT(icon);
    icon->updateGeometry();
}

guidata_healthicon_t::guidata_healthicon_t(dint player, int sprite)
    : HudWidget(function_cast<UpdateGeometryFunc>(HealthIconWidget_UpdateGeometry),
                function_cast<DrawFunc>(HealthIconWidget_Draw),
                player)
{
    this->iconSpriteId = sprite; 
}

guidata_healthicon_t::~guidata_healthicon_t()
{}

void guidata_healthicon_t::tick(timespan_t /*elapsed*/)
{
    // Nothing to do.
}

void guidata_healthicon_t::draw(const Vec2i &offset) const
{
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(!::cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    GUI_DrawSprite(this->iconSpriteId, 0, 0, HOT_TLEFT, 1, iconOpacity, false, nullptr, nullptr);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_healthicon_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!::cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    Size2Raw iconSize;
    GUI_SpriteSize(this->iconSpriteId, 1, &iconSize.width, &iconSize.height);
    iconSize.width  *= ::cfg.common.hudScale;
    iconSize.height *= ::cfg.common.hudScale;
    Rect_SetWidthHeight(&geometry(), iconSize.width, iconSize.height);
}
