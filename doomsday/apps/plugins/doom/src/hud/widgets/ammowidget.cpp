/** @file ammowidget.cpp  GUI widget for visualizing player ammo ownership.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "hud/widgets/ammowidget.h"

#include "player.h"

using namespace de;

static void AmmoWidget_UpdateGeometry(guidata_ammo_t *ammo)
{
    DENG2_ASSERT(ammo);
    ammo->updateGeometry();
}

static void AmmoWidget_Draw(guidata_ammo_t *wi, Point2Raw const *offset)
{
    DENG2_ASSERT(wi);
    wi->draw(offset? Vector2i(offset->xy) : Vector2i());
}

guidata_ammo_t::guidata_ammo_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(AmmoWidget_UpdateGeometry),
                function_cast<DrawFunc>(AmmoWidget_Draw),
                player)
{}

guidata_ammo_t::~guidata_ammo_t()
{}

void guidata_ammo_t::reset()
{
    _value = 1994;
}

guidata_ammo_t &guidata_ammo_t::setAmmoType(ammotype_t newAmmoType)
{
    _ammotype = newAmmoType;
    return *this;
}

void guidata_ammo_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &::players[player()];
    _value = plr->ammo[_ammotype].owned;
}

void guidata_ammo_t::draw(Vector2i const &offset) const
{
    static Vector2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);
    static Vector2i const offsets[NUM_AMMO_TYPES] = {
        { 288, 5 + 6 * 0 }, { 288, 5 + 6 * 1 }, { 288, 5 + 6 * 3 }, { 288, 5 + 6 * 2 },
    };

    dint const activeHud     = ST_ActiveHud(player());
    dint const yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(player()));
    dfloat const textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(_value == 1994) return;

    Vector2i const pos     = origin + offsets[dint( _ammotype )];
    auto const valueAsText = String::number(_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(font());
    FR_SetColorAndAlpha(::defFontRGB3[0], ::defFontRGB3[1], ::defFontRGB3[2], textOpacity);
    FR_DrawTextXY3(valueAsText.toUtf8().constData(), pos.x, pos.y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_ammo_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    FR_SetFont(font());
    Rect_SetWidthHeight(&geometry(), (FR_CharWidth('0') * 3) * ::cfg.common.statusbarScale,
                                      FR_CharHeight('0') * ::cfg.common.statusbarScale);
}
