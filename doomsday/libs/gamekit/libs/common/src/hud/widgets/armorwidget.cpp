/** @file armorwidget.cpp  GUI widget for -.
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

#include "hud/widgets/armorwidget.h"

#include "common.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "p_actor.h"
#include "player.h"

using namespace de;

guidata_armor_t::guidata_armor_t(void (*updateGeometry) (HudWidget *wi),
                                 void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                 dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_armor_t::~guidata_armor_t()
{}

void guidata_armor_t::reset()
{
    _value = 1994;
}

void guidata_armor_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

#if __JHEXEN__
    const player_t *plr = &::players[player()];
    const dint pClass   = ::cfg.playerClass[player()];  // Original player class (i.e. not pig).
    _value = FixedDiv(PCLASS_INFO(pClass)->autoArmorSave
                      + plr->armorPoints[ARMOR_ARMOR ]
                      + plr->armorPoints[ARMOR_SHIELD]
                      + plr->armorPoints[ARMOR_HELMET]
                      + plr->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
#else
    const player_t *plr = &::players[player()];
    _value = plr->armorPoints;
#endif
}

void ArmorWidget_Draw(guidata_armor_t *armor, const Point2Raw *offset)
{
#if __JDOOM__
#  define SUFFIX                ( "%" )
#  define TRACKING              ( 0 )
#elif __JHERETIC__
#  define SUFFIX                ( "" )
#  define TRACKING              ( 1 )
#else
#  define SUFFIX                ( "" )
#  define TRACKING              ( 0 )
#endif

    DE_ASSERT(armor);
    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(armor->_value == 1994) return;

#if !__JHEXEN__
    if(!::cfg.hudShown[HUD_ARMOR]) return;
#endif

    if(ST_AutomapIsOpen(armor->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[armor->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(armor->_value) + SUFFIX;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(armor->font());
    FR_SetTracking(TRACKING);
#if __JHERETIC__
    FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    FR_DrawTextXY(valueAsText, 0, -2); /// @todo Why is an offset needed?
#else
    FR_SetColorAndAlpha(::cfg.common.hudColor[0], ::cfg.common.hudColor[1], ::cfg.common.hudColor[2], textOpacity);
    FR_DrawTextXY(valueAsText, 0, 0);
#endif
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef SUFFIX
}


void SBarArmorWidget_Draw(guidata_armor_t *armor, const Point2Raw *offset)
{
#if __JDOOM__
#  define X_OFFSET              ( 221 )
#  define Y_OFFSET              (   3 )
#  define TRACKING              ( 0 )
#elif __JHERETIC__
#  define X_OFFSET              ( 254 )
#  define Y_OFFSET              (  12 )
#  define TRACKING              ( 1 )
#elif __JHEXEN__
#  define X_OFFSET              ( 274 )
#  define Y_OFFSET              (  14 )
#  define TRACKING              ( 0 )
#else
#  define X_OFFSET              ( 0 )
#  define Y_OFFSET              ( 0 )
#  define TRACKING              ( 0 )
#endif

    static Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    const dint activeHud     = ST_ActiveHud(armor->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(armor->player()));
    const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(armor->_value == 1994) return;

#if __JHERETIC__ || __JHEXEN__
    if(Hu_InventoryIsOpen(armor->player())) return;
#endif
#if __JHEXEN__
    if(ST_AutomapIsOpen(armor->player())) return;
#else
    if(ST_AutomapIsOpen(armor->player()) && ::cfg.common.automapHudDisplay == 0) return;
#endif
    if(P_MobjIsCamera(::players[armor->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(armor->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(armor->font());
    FR_SetTracking(TRACKING);
#if __JDOOM__
    if(::gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(::defFontRGB3[0], ::defFontRGB3[1], ::defFontRGB3[2], textOpacity);
    }
    else
#endif
    {
        FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    }
    FR_DrawTextXY3(valueAsText, origin.x + X_OFFSET, origin.y + Y_OFFSET, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
#if __JDOOM__
    FR_DrawCharXY('%', origin.x + X_OFFSET, origin.y + Y_OFFSET);
#endif
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef Y_OFFSET
#undef X_OFFSET
}

void Armor_UpdateGeometry(guidata_armor_t *armor)
{
#if __JHERETIC__
#  define TRACKING              ( 1 )
#else
#  define TRACKING              ( 0 )
#endif

    DE_ASSERT(armor);
    Rect_SetWidthHeight(&armor->geometry(), 0, 0);

    if(armor->_value == 1994) return;

#if !__JHEXEN__
    if(!::cfg.hudShown[HUD_ARMOR]) return;
#endif

    if(ST_AutomapIsOpen(armor->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[armor->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(armor->_value) + "%";

    FR_SetFont(armor->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&armor->geometry(), textSize.width  * ::cfg.common.hudScale,
                                            textSize.height * ::cfg.common.hudScale);

#undef TRACKING
}

void SBarArmor_UpdateGeometry(guidata_armor_t *armor)
{
#if __JHERETIC__
#  define TRACKING              ( 1 )
#else
#  define TRACKING              ( 0 )
#endif

    DE_ASSERT(armor);
    Rect_SetWidthHeight(&armor->geometry(), 0, 0);

    if(armor->_value == 1994) return;

#if __JHERETIC__ || __JHEXEN__
    if(Hu_InventoryIsOpen(armor->player())) return;
#endif
#if __JHEXEN__
    if(ST_AutomapIsOpen(armor->player())) return;
#else
    if(ST_AutomapIsOpen(armor->player()) && ::cfg.common.automapHudDisplay == 0) return;
#endif
    if(P_MobjIsCamera(::players[armor->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(armor->_value);

    FR_SetFont(armor->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
#if __JDOOM__
    Rect_SetWidthHeight(&armor->geometry(), (textSize.width + FR_CharWidth('%')) * ::cfg.common.statusbarScale,
                                             de::max(textSize.height, FR_CharHeight('%')) * ::cfg.common.statusbarScale);
#else
    Rect_SetWidthHeight(&armor->geometry(), textSize.width  * ::cfg.common.statusbarScale,
                                            textSize.height * ::cfg.common.statusbarScale);
#endif

#undef TRACKING
}
