/** @file weaponslotwidget.cpp  GUI widget for visualizing player weapon ownership.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "hud/widgets/weaponslotwidget.h"

#include <de/vector.h>
#include "hu_stuff.h"
#include "player.h"

using namespace de;

#define ST_ARMSX                ( 111 )
#define ST_ARMSY                (   4 )
#define ST_ARMSXSPACE           (  12 )
#define ST_ARMSYSPACE           (  10 )

// Weapon ownership patches.
static patchid_t pArms[6][2];

static void WeaponSlotWidget_Draw(guidata_weaponslot_t *ws, const Point2Raw *offset)
{
    DE_ASSERT(ws);
    ws->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void WeaponSlotWidget_UpdateGeometry(guidata_weaponslot_t *ws)
{
    DE_ASSERT(ws);
    ws->updateGeometry();
}

struct countownedweaponsinslot_params_t
{
    int player;
    int numOwned;
};

static int countOwnedWeaponsInSlot(weapontype_t type, void *context)
{
    auto &p = *static_cast<countownedweaponsinslot_params_t *>(context);
    const player_t *plr = &players[p.player];
    if(plr->weapons[type].owned)
    {
        p.numOwned += 1;
    }
    return 1;  // Continue iteration.
}

guidata_weaponslot_t::guidata_weaponslot_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(WeaponSlotWidget_UpdateGeometry),
                function_cast<DrawFunc>(WeaponSlotWidget_Draw),
                player)
{}

guidata_weaponslot_t::~guidata_weaponslot_t()
{}

void guidata_weaponslot_t::reset()
{
    _patchId = 0;
}

guidata_weaponslot_t &guidata_weaponslot_t::setSlot(dint newSlotNum)
{
    _slot = newSlotNum + 1;  // 1-based
    return *this;
}

void guidata_weaponslot_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t *plr = &players[player()];

    bool used = false;
    if(cfg.fixStatusbarOwnedWeapons)
    {
        // Does the player own any weapon bound to this slot?
        countownedweaponsinslot_params_t parm;
        parm.player   = player();
        parm.numOwned = 0;
        P_IterateWeaponsBySlot(_slot, false, countOwnedWeaponsInSlot, &parm);
        used = (parm.numOwned > 0);
    }
    else
    {
        // Does the player own the originally hardwired weapon to this slot?
        used = CPP_BOOL(plr->weapons[_slot].owned);
    }
    _patchId = pArms[_slot - 1][dint( used )];
}

void guidata_weaponslot_t::draw(const Vec2i &offset) const
{
#define ORIGINX (-ST_WIDTH/2)
#define ORIGINY (-ST_HEIGHT)

    static Vec2i const elements[] = {
        Vec2i( ORIGINX+ST_ARMSX,                     ORIGINY+ST_ARMSY ),
        Vec2i( ORIGINX+ST_ARMSX + ST_ARMSXSPACE,     ORIGINY+ST_ARMSY ),
        Vec2i( ORIGINX+ST_ARMSX + ST_ARMSXSPACE*2,   ORIGINY+ST_ARMSY ),
        Vec2i( ORIGINX+ST_ARMSX,                     ORIGINY+ST_ARMSY + ST_ARMSYSPACE ),
        Vec2i( ORIGINX+ST_ARMSX + ST_ARMSXSPACE,     ORIGINY+ST_ARMSY + ST_ARMSYSPACE ),
        Vec2i( ORIGINX+ST_ARMSX + ST_ARMSXSPACE*2,   ORIGINY+ST_ARMSY + ST_ARMSYSPACE ),
    };

    const Vec2i &element = elements[_slot - 1];

    const int activeHud     = ST_ActiveHud(player());
    const int yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(player()));
    const float textOpacity = (activeHud == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);
    //const float iconOpacity = (fullscreen == 0? 1 : uiRendState->pageAlpha * cfg.common.statusbarCounterAlpha);

    if(gfw_Rule(deathmatch)) return;
    if(ST_AutomapIsOpen(player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(cfg.common.statusbarScale, cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, textOpacity);

    FR_SetFont(font());
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(::defFontRGB3[0], ::defFontRGB3[1], ::defFontRGB3[2], textOpacity);
    }
    else
    {
        FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    }
    WI_DrawPatch(_patchId, Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.hudPatchReplaceMode), _patchId),
                 element, ALIGN_TOPLEFT, 0, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX
}

void guidata_weaponslot_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(gfw_Rule(deathmatch)) return;
    if(ST_AutomapIsOpen(player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    const String text = Hu_ChoosePatchReplacement(patchreplacemode_t(cfg.common.hudPatchReplaceMode), _patchId);
    if(text.isEmpty() && !R_GetPatchInfo(_patchId, &info)) return;

    if(!text.isEmpty())
    {
        FR_SetFont(font());
        Size2Raw textSize; FR_TextSize(&textSize, text);
        Rect_SetWidthHeight(&geometry(), textSize.width  * cfg.common.statusbarScale,
                                         textSize.height * cfg.common.statusbarScale);
        return;
    }

    R_GetPatchInfo(_patchId, &info);
    Rect_SetWidthHeight(&geometry(), info.geometry.size.width  * cfg.common.statusbarScale,
                                     info.geometry.size.height * cfg.common.statusbarScale);
}

void guidata_weaponslot_t::prepareAssets()  // static
{
    char nameBuf[9];
    for(int i = 0; i < 6; ++i)
    {
        // gray
        sprintf(nameBuf, "STGNUM%d", i + 2);
        pArms[i][0] = R_DeclarePatch(nameBuf);

        // yellow
        sprintf(nameBuf, "STYSNUM%d", i + 2);
        pArms[i][1] = R_DeclarePatch(nameBuf);
    }
}
