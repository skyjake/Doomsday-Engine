/** @file armoriconswidget.cpp  GUI widget for -.
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

#include "hud/widgets/armoriconswidget.h"

#include "jhexen.h"
#include "gl_drawpatch.h"
#include "hu_inventory.h"

using namespace de;

static patchid_t pArmorIcon[NUMARMOR];

static void ArmorIconsWidget_Draw(guidata_armoricons_t *icons, const Point2Raw *offset)
{
    DE_ASSERT(icons);
    icons->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void ArmorIconsWidget_UpdateGeometry(guidata_armoricons_t *icons)
{
    DE_ASSERT(icons);
    icons->updateGeometry();
}

guidata_armoricons_t::guidata_armoricons_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(ArmorIconsWidget_UpdateGeometry),
                function_cast<DrawFunc>(ArmorIconsWidget_Draw),
                player)
{}

guidata_armoricons_t::~guidata_armoricons_t()
{}

void guidata_armoricons_t::reset()
{
    de::zap(_armorPoints);
}

void guidata_armoricons_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t &plr = ::players[player()];
    for(dint i = 0; i < NUMARMOR; ++i)
    {
        _armorPoints[i] = plr.armorPoints[i];
    }
}

void guidata_armoricons_t::draw(const Vec2i &offset) const
{
#define X_OFFSET                ( 150 )
#define Y_OFFSET                (   2 )

    static Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    const dint plrClass      = ::cfg.playerClass[player()];  // Original player class (i.e. not pig).
    const dint activeHud     = ST_ActiveHud(player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(player()));
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(player())) return;
    if(!ST_AutomapIsOpen(player())) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    const classinfo_t &pcdata = *PCLASS_INFO(plrClass);
    for(dint i = 0; i < NUMARMOR; ++i)
    {
        if(!_armorPoints[i]) continue;

        dfloat opacity = 1;
        if(_armorPoints[i] <= (pcdata.armorIncrement[i] >> 2))
        {
            opacity = .3f;
        }
        else if(_armorPoints[i] <= (pcdata.armorIncrement[i] >> 1))
        {
            opacity = .6f;
        }

        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconOpacity * opacity);
        GL_DrawPatch(::pArmorIcon[i], origin + Vec2i(X_OFFSET + 31 * i, Y_OFFSET));
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y_OFFSET
#undef X_OFFSET
}

void guidata_armoricons_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(Hu_InventoryIsOpen(player())) return;
    if(!ST_AutomapIsOpen(player())) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    for(dint i = 0, x = 0; i < NUMARMOR; ++i, x += 31)
    {
        if(!_armorPoints[i]) continue;
        if(!R_GetPatchInfo(::pArmorIcon[i], &info)) continue;

        info.geometry.origin.x = x;
        info.geometry.origin.y = 0;
        Rect_UniteRaw(&geometry(), &info.geometry);
    }

    Rect_SetWidthHeight(&geometry(), Rect_Width (&geometry()) * ::cfg.common.statusbarScale,
                                     Rect_Height(&geometry()) * ::cfg.common.statusbarScale);
}

void guidata_armoricons_t::prepareAssets()  // static
{
    for(dint i = 0; i < NUMARMOR; ++i)
    {
        ::pArmorIcon[i] = R_DeclarePatch(Stringf("ARMSLOT%d", i + 1));
    }
}
