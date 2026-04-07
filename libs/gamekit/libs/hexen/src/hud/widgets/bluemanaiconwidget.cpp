/** @file bluemanaiconwidget.h  GUI widget for -.
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

#include "hud/widgets/bluemanaiconwidget.h"

#include "jhexen.h"
#include "gl_drawpatch.h"
#include "hu_inventory.h"

using namespace de;

static patchid_t pBlueManaIcon[2];  ///< [ dim, bright ]

guidata_bluemanaicon_t::guidata_bluemanaicon_t(void (*updateGeometry) (HudWidget *wi),
                                               void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                               dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_bluemanaicon_t::~guidata_bluemanaicon_t()
{}

void guidata_bluemanaicon_t::reset()
{
    _iconIdx = -1;
}

void guidata_bluemanaicon_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    _iconIdx = 0;  // Dim icon.

    const player_t &plr = ::players[player()];
    if(!VALID_WEAPONTYPE(plr.readyWeapon)) return;

    // If the player owns some of this ammo and the ready weapon consumes it - use the bright icon.
    if(plr.ammo[AT_BLUEMANA].owned > 0)
    {
        if(WEAPON_INFO(plr.readyWeapon, plr.class_, 0)->ammoType[AT_BLUEMANA])
        {
            _iconIdx = 1;  // Bright icon.
        }
    }
}

void BlueManaIconWidget_Draw(guidata_bluemanaicon_t *icon, const Point2Raw *offset)
{
    DE_ASSERT(icon);
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(icon->_iconIdx < 0) return;

    if(!::cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsOpen(icon->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(::pBlueManaIcon[icon->_iconIdx]);
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarBlueManaIconWidget_Draw(guidata_bluemanaicon_t *icon, const Point2Raw *offset)
{
#define X_OFFSET                ( 77 )
#define Y_OFFSET                (  2 )

    DE_ASSERT(icon);
    static Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    const dint activeHud     = ST_ActiveHud(icon->player());
    const dfloat yOffset     = ST_HEIGHT * (1 - ST_StatusBarShown(icon->player()));
    const dfloat iconOpacity = (activeHud  == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(icon->_iconIdx < 0) return;

    if(Hu_InventoryIsOpen(icon->player())) return;
    if(ST_AutomapIsOpen(icon->player())) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(::pBlueManaIcon[icon->_iconIdx], origin + Vec2i(X_OFFSET, Y_OFFSET));
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y_OFFSET
#undef X_OFFSET
}

void BlueManaIconWidget_UpdateGeometry(guidata_bluemanaicon_t *icon)
{
    DE_ASSERT(icon);
    Rect_SetWidthHeight(&icon->geometry(), 0, 0);

    if(icon->_iconIdx < 0) return;

    if(!::cfg.hudShown[HUD_MANA]) return;
    if(ST_AutomapIsOpen(icon->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    if(R_GetPatchInfo(::pBlueManaIcon[icon->_iconIdx], &info))
    {
        Rect_SetWidthHeight(&icon->geometry(), info.geometry.size.width  * ::cfg.common.hudScale,
                                               info.geometry.size.height * ::cfg.common.hudScale);
    }
}

void SBarBlueManaIconWidget_UpdateGeometry(guidata_bluemanaicon_t *icon)
{
    DE_ASSERT(icon);
    Rect_SetWidthHeight(&icon->geometry(), 0, 0);

    if(icon->_iconIdx < 0) return;

    if(Hu_InventoryIsOpen(icon->player())) return;
    if(ST_AutomapIsOpen(icon->player())) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    if(R_GetPatchInfo(::pBlueManaIcon[icon->_iconIdx], &info))
    {
        Rect_SetWidthHeight(&icon->geometry(), info.geometry.size.width  * ::cfg.common.statusbarScale,
                                               info.geometry.size.height * ::cfg.common.statusbarScale);
    }
}

void guidata_bluemanaicon_t::prepareAssets()  // static
{
    ::pBlueManaIcon[0] = R_DeclarePatch("MANADIM1");
    ::pBlueManaIcon[1] = R_DeclarePatch("MANABRT1");
}
