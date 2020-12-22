/** @file greenmanavialwidget.cpp  GUI widget for -.
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

#include "hud/widgets/greenmanavialwidget.h"

#include "jhexen.h"
#include "gl_drawpatch.h"
#include "hu_inventory.h"

using namespace de;

static patchid_t pGreenBackground[2];  ///< [ dim, bright ]

static void GreenManaVialWidget_Draw(guidata_greenmanavial_t *vial, const Point2Raw *offset)
{
    DE_ASSERT(vial);
    vial->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void GreenManaVial_UpdateGeometry(guidata_greenmanavial_t *vial)
{
    DE_ASSERT(vial);
    vial->updateGeometry();
}

guidata_greenmanavial_t::guidata_greenmanavial_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(GreenManaVial_UpdateGeometry),
                function_cast<DrawFunc>(GreenManaVialWidget_Draw),
                player)
{}

guidata_greenmanavial_t::~guidata_greenmanavial_t()
{}

void guidata_greenmanavial_t::reset()
{
    _backgroundIdx = -1;
    _filled = 0;
}

void guidata_greenmanavial_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    _backgroundIdx = 0;  // Dim icon.

    const player_t &plr = ::players[player()];
    if(VALID_WEAPONTYPE(plr.readyWeapon))
    {
        // If the player owns some of this ammo and the ready weapon consumes it - use the bright icon.
        if(plr.ammo[AT_GREENMANA].owned > 0)
        {
            if(WEAPON_INFO(plr.readyWeapon, plr.class_, 0)->ammoType[AT_GREENMANA])
            {
                _backgroundIdx = 1;  // Bright icon.
            }
        }
    }

    _filled = de::clamp(0.f, dfloat( plr.ammo[AT_GREENMANA].owned ) / MAX_MANA, 1.f);
}

void guidata_greenmanavial_t::draw(const Vec2i &offset) const
{
#define X_OFFSET            ( 102 )
#define Y_OFFSET            (   2 )
#define VIAL_WIDTH          (   3 )
#define VIAL_HEIGHT         (  22 )

    static Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

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

    if(_backgroundIdx >= 0)
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(::pGreenBackground[_backgroundIdx], origin + Vec2i(X_OFFSET, Y_OFFSET));
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(origin.x + X_OFFSET + 1, origin.y + Y_OFFSET + 1, VIAL_WIDTH, dint( VIAL_HEIGHT * (1 - _filled) + .5f ), 0, 0, 0, iconOpacity);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef VIAL_HEIGHT
#undef VIAL_WIDTH
#undef Y_OFFSET
#undef X_OFFSET
}

void guidata_greenmanavial_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(_backgroundIdx < 0) return;

    if(Hu_InventoryIsOpen(player())) return;
    if(ST_AutomapIsOpen(player())) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    if(R_GetPatchInfo(::pGreenBackground[_backgroundIdx], &info))
    {
        Rect_SetWidthHeight(&geometry(), info.geometry.size.width  * ::cfg.common.statusbarScale,
                                         info.geometry.size.height * ::cfg.common.statusbarScale);
    }
}

void guidata_greenmanavial_t::prepareAssets()  // static
{
    ::pGreenBackground[0] = R_DeclarePatch("MANAVL2D");
    ::pGreenBackground[1] = R_DeclarePatch("MANAVL2");
}
