/** @file weaponpieceswidget.cpp  GUI widget for -.
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

#include "hud/widgets/weaponpieceswidget.h"

#include "jhexen.h"
#include "gl_drawpatch.h"
#include "hu_inventory.h"

using namespace de;

static patchid_t pPiece[NUM_PLAYER_CLASSES][WEAPON_FOURTH_PIECE_COUNT];
static patchid_t pComplete[NUM_PLAYER_CLASSES];

static void WeaponPiecesWidget_Draw(guidata_weaponpieces_t *wp, const Point2Raw *offset)
{
    DE_ASSERT(wp);
    wp->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void WeaponPiecesWidget_UpdateGeometry(guidata_weaponpieces_t *wp)
{
    DE_ASSERT(wp);
    wp->updateGeometry();
}

guidata_weaponpieces_t::guidata_weaponpieces_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(WeaponPiecesWidget_UpdateGeometry),
                function_cast<DrawFunc>(WeaponPiecesWidget_Draw),
                player)
{}

guidata_weaponpieces_t::~guidata_weaponpieces_t()
{}

void guidata_weaponpieces_t::reset()
{
    _ownedPieces = 0;
}

void guidata_weaponpieces_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    _ownedPieces = players[player()].pieces;
}

void guidata_weaponpieces_t::draw(const Vec2i &offset) const
{
    static Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    const dint plrClass    = ::cfg.playerClass[player()];  // Original player class (i.e. not pig).
    const dint activeHud     = ST_ActiveHud(player());
    const dfloat yOffset     = ST_HEIGHT * (1 - ST_StatusBarShown(player()));
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(player())) return;
    if(ST_AutomapIsOpen(player())) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    if(_ownedPieces == WEAPON_FOURTH_COMPLETE)
    {
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(::pComplete[plrClass], origin + Vec2i(190, 0));
    }
    else
    {
        const classinfo_t &pcdata = *PCLASS_INFO(plrClass);
        for(dint piece = 0; piece < WEAPON_FOURTH_PIECE_COUNT; ++piece)
        {
            if(_ownedPieces & (1 << piece))
            {
                DGL_Color4f(1, 1, 1, iconOpacity);
                GL_DrawPatch(::pPiece[plrClass][piece], origin + Vec2i(pcdata.fourthWeaponPiece[piece].offset.xy));
            }
        }
    }
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_weaponpieces_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(Hu_InventoryIsOpen(player())) return;
    if(ST_AutomapIsOpen(player())) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    Rect_SetWidthHeight(&geometry(), 57 * ::cfg.common.statusbarScale,
                                     30 * ::cfg.common.statusbarScale);
}

void guidata_weaponpieces_t::prepareAssets()  // static
{
    de::zap(::pComplete);
    de::zap(::pPiece);

    for(dint plrClass = 0; plrClass < NUM_PLAYER_CLASSES; ++plrClass)
    {
        const classinfo_t &pcdata = *PCLASS_INFO(plrClass);

        // Only user-selectable player classes can collect fourth-weapon pieces.
        if(!pcdata.userSelectable) continue;

        ::pComplete[plrClass] = R_DeclarePatch(pcdata.fourthWeaponCompletePatchName);
        for(dint piece = 0; piece < WEAPON_FOURTH_PIECE_COUNT; ++piece)
        {
            ::pPiece[plrClass][piece] = R_DeclarePatch(pcdata.fourthWeaponPiece[piece].patchName);
        }
    }
}
