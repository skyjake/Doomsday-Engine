/** @file armoriconwidget.cpp  GUI widget for -.
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

#include "hud/widgets/armoriconwidget.h"

#include "player.h"
#include "p_actor.h"

using namespace de;

static void ArmorIcon_Draw(guidata_armoricon_t *icon, const Point2Raw *offset)
{
    DE_ASSERT(icon);
    icon->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void ArmorIcon_UpdateGeometry(guidata_armoricon_t *icon)
{
    DE_ASSERT(icon);
    icon->updateGeometry();
}

guidata_armoricon_t::guidata_armoricon_t(dint player, int sprite1, int sprite2)
    : HudWidget(function_cast<UpdateGeometryFunc>(ArmorIcon_UpdateGeometry),
                function_cast<DrawFunc>(ArmorIcon_Draw),
                player)
{
    this->armorSprite1 = sprite1;
    this->armorSprite2 = sprite2;
}

guidata_armoricon_t::~guidata_armoricon_t()
{}

void guidata_armoricon_t::reset()
{
    currentSprite = -1;
}

void guidata_armoricon_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t *plr = &players[player()];
    currentSprite = (plr->armorType == 2 ? this->armorSprite2 : this->armorSprite1);
}

void guidata_armoricon_t::draw(const Vec2i &offset) const
{
    const dfloat iconOpacity = uiRendState->pageAlpha * cfg.common.hudIconAlpha;

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsOpen(player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(currentSprite < 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    GUI_DrawSprite(currentSprite, 0, 0, HOT_TLEFT, 1, iconOpacity, false, nullptr, nullptr);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_armoricon_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!cfg.hudShown[HUD_ARMOR]) return;
    if(ST_AutomapIsOpen(player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(currentSprite < 0) return;

    Size2Raw iconSize;
    GUI_SpriteSize(currentSprite, 1, &iconSize.width, &iconSize.height);
    iconSize.width  *= cfg.common.hudScale;
    iconSize.height *= cfg.common.hudScale;
    Rect_SetWidthHeight(&geometry(), iconSize.width, iconSize.height);
}
