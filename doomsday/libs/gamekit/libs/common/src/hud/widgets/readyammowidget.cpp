/** @file readyammowidget.cpp  GUI widget for -.
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

#include "common.h"
#include "hud/widgets/readyammowidget.h"

#include "player.h"
#include "p_actor.h"
#if __JHERETIC__
#  include "hu_inventory.h"
#endif

using namespace de;

guidata_readyammo_t::guidata_readyammo_t(void (*updateGeometry) (HudWidget *wi),
                                         void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                         dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_readyammo_t::~guidata_readyammo_t()
{}

void guidata_readyammo_t::reset()
{
    _value = 1994;
}

void guidata_readyammo_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    _value = 1994; // Means n/a.

    player_t *plr  = &::players[player()];
    if(!VALID_WEAPONTYPE(plr->readyWeapon)) return;

#if __JHERETIC__
    const dint lvl = (plr->powers[PT_WEAPONLEVEL2]? 1 : 0);
#else
    const dint lvl = 0;
#endif

    for(dint i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        // Does the weapon use this type of ammo?
        if(WEAPON_INFO(plr->readyWeapon, plr->class_, lvl)->ammoType[i])
        {
            _value = plr->ammo[i].owned;
            break;
        }
    }
}

#if defined(__JDOOM__) || defined(__JDOOM64__)

void ReadyAmmo_Drawer(guidata_readyammo_t *ammo, const Point2Raw *offset)
{
    DE_ASSERT(ammo);
    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(ammo->_value == 1994) return;

    if(!::cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(ammo->font());
    FR_SetColorAndAlpha(::cfg.common.hudColor[0], ::cfg.common.hudColor[1], ::cfg.common.hudColor[2], textOpacity);
    FR_DrawTextXY(valueAsText, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarReadyAmmo_Drawer(guidata_readyammo_t *ammo, const Point2Raw *offset)
{
    static const int ORIGINX       = -ST_WIDTH / 2;
    static const int ORIGINY       = -ST_HEIGHT;
    static const int ST_READYAMMOX = 44;
    static const int ST_READYAMMOY = 3;
    static const int X             = ORIGINX + ST_READYAMMOX;
    static const int Y             = ORIGINY + ST_READYAMMOY;

    DE_ASSERT(ammo);
    if(ammo->_value == 1994) return;

    const dint activeHud     = ST_ActiveHud(ammo->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(ammo->player()));
    const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    //const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(ammo->font());

    // XXX TODO This is precisely the reason we can't have nice things
    //          I will come through and untangle this mess of live power cabling and venomous snakes later
#   if defined(__JDOOM__)
    if(::gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(::defFontRGB3[0], ::defFontRGB3[1], ::defFontRGB3[2], textOpacity);
    }
    else
#   endif
    {
        FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    }
    FR_DrawTextXY3(valueAsText, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void ReadyAmmo_UpdateGeometry(guidata_readyammo_t *ammo)
{
    DE_ASSERT(ammo);
    Rect_SetWidthHeight(&ammo->geometry(), 0, 0);

    if(ammo->_value == 1994) return;

    if(!::cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    FR_SetFont(ammo->font());
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    textSize.width  *= ::cfg.common.hudScale;
    textSize.height *= ::cfg.common.hudScale;
    Rect_SetWidthHeight(&ammo->geometry(), textSize.width, textSize.height);
}

void SBarReadyAmmo_UpdateGeometry(guidata_readyammo_t *ammo)
{
    DE_ASSERT(ammo);
    Rect_SetWidthHeight(&ammo->geometry(), 0, 0);

    if(ammo->_value == 1994) return;

    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    FR_SetFont(ammo->font());
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    textSize.width  *= ::cfg.common.statusbarScale;
    textSize.height *= ::cfg.common.statusbarScale;
    Rect_SetWidthHeight(&ammo->geometry(), textSize.width, textSize.height);
}

#endif  // __JDOOM__

#if __JHERETIC__

void ReadyAmmo_Drawer(guidata_readyammo_t *ammo, const Point2Raw *offset)
{
    static const int TRACKING = 1;

    DE_ASSERT(ammo);
    if(ammo->_value == 1994) return;

    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(ST_StatusBarIsActive(ammo->player())) return;

    if(!::cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(ammo->font());
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    FR_DrawTextXY(valueAsText, 0, -2);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarReadyAmmo_Drawer(guidata_readyammo_t *ammo, const Point2Raw *offset)
{
#define ORIGINX                 (-ST_WIDTH / 2 )
#define ORIGINY                 (-ST_HEIGHT )
#define ST_AMMOX                ( 135 )
#define ST_AMMOY                (   4 )
#define X                       ( ORIGINX + ST_AMMOX )
#define Y                       ( ORIGINY + ST_AMMOY )
#define MAXDIGITS               ( ST_AMMOWIDTH )
#define TRACKING                ( 1 )

    DE_ASSERT(ammo);
    if(ammo->_value == 1994) return;

    const dint activeHud     = ST_ActiveHud(ammo->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(ammo->player()));
    //const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(ammo->player())) return;
    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(ammo->font());
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], iconOpacity);
    FR_DrawTextXY3(valueAsText, X, Y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef MAXDIGITS
#undef Y
#undef X
#undef ST_AMMOY
#undef ST_AMMOX
#undef ORIGINY
#undef ORIGINX
}

void ReadyAmmo_UpdateGeometry(guidata_readyammo_t *ammo)
{
    static const int TRACKING = 1;

    DE_ASSERT(ammo);
    Rect_SetWidthHeight(&ammo->geometry(), 0, 0);

    if(ammo->_value == 1994) return;

    if(ST_StatusBarIsActive(ammo->player())) return;

    if(!::cfg.hudShown[HUD_AMMO]) return;
    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    FR_SetFont(ammo->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize;
    FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&ammo->geometry(), textSize.width  * ::cfg.common.hudScale,
                                           textSize.height * ::cfg.common.hudScale);

}

void SBarReadyAmmo_UpdateGeometry(guidata_readyammo_t *ammo)
{
    static const int TRACKING = 1;

    DE_ASSERT(ammo);
    Rect_SetWidthHeight(&ammo->geometry(), 0, 0);

    if(ammo->_value == 1994) return;

    if(Hu_InventoryIsOpen(ammo->player())) return;
    if(ST_AutomapIsOpen(ammo->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[ammo->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(ammo->_value);

    FR_SetFont(ammo->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&ammo->geometry(), textSize.width  * ::cfg.common.statusbarScale,
                                           textSize.height * ::cfg.common.statusbarScale);

}

#endif  // __JHERETIC__
