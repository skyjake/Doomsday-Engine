/** @file readyammoiconwidget.h  GUI widget for visualizing high-level player status.
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

#include "hud/widgets/readyammoiconwidget.h"

#include "common.h"
#include "gl_drawpatch.h"
#if __JHERETIC__
#  include "hu_inventory.h"
#endif
#include "p_actor.h"

using namespace de;

#if !__JDOOM__
static patchid_t pAmmoIcons[11];
#endif

guidata_readyammoicon_t::guidata_readyammoicon_t(void (*updateGeometry) (HudWidget *wi),
                                                 void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                                 dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_readyammoicon_t::~guidata_readyammoicon_t()
{}

void guidata_readyammoicon_t::reset()
{
#if __JDOOM__
    _sprite = -1;
#else
    _patchId = -1;
#endif
}

void guidata_readyammoicon_t::tick(timespan_t /*elapsed*/)
{
#if __JDOOM__
    static dint const ammoSprite[NUM_AMMO_TYPES] = {
        SPR_AMMO, SPR_SBOX, SPR_CELL, SPR_ROCK
    };
#endif

    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t *plr = &::players[player()];
    if(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)) return;

#if __JDOOM__
    _sprite = -1;
#else
    _patchId = -1;
#endif

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
#if __JDOOM__
            _sprite = ammoSprite[i];
#else
            _patchId = ::pAmmoIcons[i];
#endif
            break;
        }
    }
}

void ReadyAmmoIconWidget_Drawer(guidata_readyammoicon_t *icon, const Point2Raw *offset)
{
    DE_ASSERT(icon);

    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

#if !__JDOOM64__
    if(ST_StatusBarIsActive(icon->player())) return;
#endif

#if !__JHEXEN__
    if(!::cfg.hudShown[HUD_AMMO]) return;
#endif

    if(ST_AutomapIsOpen(icon->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

#if __JDOOM__
    if(icon->_sprite < 0) return;
#else
    if(icon->_patchId <= 0) return;
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

#if __JDOOM__
    dfloat scale = (icon->_sprite == SPR_ROCK? .72f : 1);
    GUI_DrawSprite(icon->_sprite, 0, 0, HOT_TLEFT, scale, iconOpacity, false, nullptr, nullptr);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(icon->_patchId, Vec2i(0, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
    DGL_Disable(DGL_TEXTURE_2D);
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

#if __JHERETIC__
void SBarReadyAmmoIconWidget_Drawer(guidata_readyammoicon_t *icon, const Point2Raw *offset)
{
#define ORIGINX             (-ST_WIDTH / 2 )
#define ORIGINY             (-ST_HEIGHT )
#define X_OFFSET            ( 111 )
#define Y_OFFSET            (  14 )
#define X                   ( ORIGINX + X_OFFSET )
#define Y                   ( ORIGINY + Y_OFFSET )

    DE_ASSERT(icon);

    const dint activeHud     = ST_ActiveHud(icon->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(icon->player()));
    //const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(icon->player())) return;
    if(ST_AutomapIsOpen(icon->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(icon->_patchId <= 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);

    GL_DrawPatch(icon->_patchId, Vec2i(X, Y));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef Y
#undef X
#undef Y_OFFSET
#undef X_OFFSET
#undef ORIGINY
#undef ORIGINX
}
#endif  // __JHERETIC__

void ReadyAmmoIconWidget_UpdateGeometry(guidata_readyammoicon_t *icon)
{
    DE_ASSERT(icon);

    Rect_SetWidthHeight(&icon->geometry(), 0, 0);

#if !__JDOOM64__
    if(ST_StatusBarIsActive(icon->player())) return;
#endif

#if !__JHEXEN__
    if(!::cfg.hudShown[HUD_AMMO]) return;
#endif

    if(ST_AutomapIsOpen(icon->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

#if __JDOOM__
    if(icon->_sprite < 0) return;

    dfloat scale = (icon->_sprite == SPR_ROCK? .72f : 1);
    Size2Raw iconSize;
    GUI_SpriteSize(icon->_sprite, scale, &iconSize.width, &iconSize.height);
    Rect_SetWidthHeight(&icon->geometry(), iconSize.width  * ::cfg.common.hudScale,
                                           iconSize.height * ::cfg.common.hudScale);
#else
    if(icon->_patchId <= 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(icon->_patchId, &info)) return;

    Rect_SetWidthHeight(&icon->geometry(), info.geometry.size.width  * ::cfg.common.hudScale,
                                           info.geometry.size.height * ::cfg.common.hudScale);
#endif
}

#if __JHERETIC__
void SBarReadyAmmoIconWidget_UpdateGeometry(guidata_readyammoicon_t *icon)
{
    DE_ASSERT(icon);

    Rect_SetWidthHeight(&icon->geometry(), 0, 0);

    if(Hu_InventoryIsOpen(icon->player())) return;
    if(ST_AutomapIsOpen(icon->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[icon->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(icon->_patchId <= 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(icon->_patchId, &info)) return;

    Rect_SetWidthHeight(&icon->geometry(), info.geometry.size.width  * ::cfg.common.statusbarScale,
                                           info.geometry.size.height * ::cfg.common.statusbarScale);
}
#endif  // __JHERETIC__

void guidata_readyammoicon_t::prepareAssets()
{
#if __JHERETIC__
    de::zap(::pAmmoIcons);
    for(dint i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        const AmmoDef *def = P_AmmoDef(ammotype_t( i ));
        // Available in the current game mode?
        if(def->gameModeBits & ::gameModeBits)
        {
            ::pAmmoIcons[i] = R_DeclarePatch(def->hudIcon);
        }
    }
#endif
}
