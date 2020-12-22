/** @file fragswidget.cpp  GUI widget for -.
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

#include "hud/widgets/fragswidget.h"

#include "g_common.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "player.h"
#include "p_actor.h"

using namespace de;

guidata_frags_t::guidata_frags_t(void (*updateGeometry) (HudWidget *wi),
                                 void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                 dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_frags_t::~guidata_frags_t()
{}

void guidata_frags_t::reset()
{
    _value = 1994;
}

void guidata_frags_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t *plr = &::players[player()];

    _value = 0;
    for(dint i = 0; i < MAXPLAYERS; ++i)
    {
        if(::players[i].plr->inGame)
        {
            _value += plr->frags[i] * (i != player() ? 1 : -1);
        }
    }
}

void FragsWidget_Draw(guidata_frags_t *frags, const Point2Raw *offset)
{
#if __JDOOM__ || __JDOOM64__
#  define TRACKING              ( 0 )
#else
#  define TRACKING              ( 1 )
#endif

    DE_ASSERT(frags);
    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

#if __JDOOM__ || __JDOOM64__
    if(!::cfg.hudShown[HUD_FRAGS]) return;
#endif

    if(!gfw_Rule(deathmatch)) return;
    if(ST_AutomapIsOpen(frags->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[frags->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(frags->_value == 1994) return;

    const auto valueAsText = Stringf("Frags: %i", frags->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(frags->font());
    FR_SetTracking(TRACKING);
    FR_SetColorAndAlpha(::cfg.common.hudColor[0], ::cfg.common.hudColor[1], ::cfg.common.hudColor[2], textOpacity);
    FR_DrawTextXY(valueAsText, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void FragsWidget_UpdateGeometry(guidata_frags_t *frags)
{
#if __JDOOM__ || __JDOOM64__
#  define TRACKING              ( 0 )
#else
#  define TRACKING              ( 1 )
#endif

    DE_ASSERT(frags);
    Rect_SetWidthHeight(&frags->geometry(), 0, 0);

#if __JDOOM__ || __JDOOM64__
    if(!::cfg.hudShown[HUD_FRAGS]) return;
#endif

    if(!gfw_Rule(deathmatch)) return;
    if(ST_AutomapIsOpen(frags->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[frags->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(frags->_value == 1994) return;

    const auto valueAsText = Stringf("Frags: %i", frags->_value);

    FR_SetFont(frags->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    textSize.width  *= ::cfg.common.hudScale;
    textSize.height *= ::cfg.common.hudScale;
    Rect_SetWidthHeight(&frags->geometry(), textSize.width, textSize.height);

#undef TRACKING
}

void SBarFragsWidget_Draw(guidata_frags_t *frags, const Point2Raw *offset)
{
#define X_ORIGIN                (-ST_WIDTH / 2 )
#define Y_ORIGIN                (-ST_HEIGHT )
#if __JDOOM__
#  define X_OFFSET              ( 138 )
#  define Y_OFFSET              (   3 )
#elif __JHERETIC__
#  define X_OFFSET              (  85 )
#  define Y_OFFSET              (  13 )
#elif __JHEXEN__
#  define X_OFFSET              (  64 )
#  define Y_OFFSET              (  14 )
#else
#  define X_OFFSET              (   0 )
#  define Y_OFFSET              (   0 )
#endif
#if __JHERETIC__
#  define TRACKING              ( 1 )
#else
#  define TRACKING              ( 0 )
#endif

    DE_ASSERT(frags);

    const dint activeHud     = ST_ActiveHud(frags->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(frags->player()));
    const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    //const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(!gfw_Rule(deathmatch)) return;
#if __JHERETIC__ || __JHEXEN__
    if(Hu_InventoryIsOpen(frags->player())) return;
#endif

#if __JHEXEN__
    if(ST_AutomapIsOpen(frags->player())) return;
#else
    if(ST_AutomapIsOpen(frags->player()) && ::cfg.common.automapHudDisplay == 0) return;
#endif
    if(P_MobjIsCamera(::players[frags->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(frags->_value == 1994) return;

    const auto valueAsText = String::asText(frags->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(frags->font());
    FR_SetTracking(TRACKING);
#if __JDOOM__
    if(gameMode == doom_chex)
    {
        FR_SetColorAndAlpha(::defFontRGB3[0], ::defFontRGB3[1], ::defFontRGB3[2], textOpacity);
    }
    else
#endif
    {
        FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
    }
    FR_DrawTextXY3(valueAsText, X_ORIGIN + X_OFFSET, Y_ORIGIN + Y_OFFSET,
                   ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef Y_OFFSET
#undef X_OFFSET
#undef Y_ORIGIN
#undef X_ORIGIN
}

void SBarFragsWidget_UpdateGeometry(guidata_frags_t *frags)
{
#if __JHERETIC__
#  define TRACKING                ( 1 )
#else
#  define TRACKING                ( 0 )
#endif

    DE_ASSERT(frags);

    Rect_SetWidthHeight(&frags->geometry(), 0, 0);

    if(!gfw_Rule(deathmatch)) return;
#if __JHERETIC__ || __JHEXEN__
    if(Hu_InventoryIsOpen(frags->player())) return;
#endif

#if __JHEXEN__
    if(ST_AutomapIsOpen(frags->player())) return;
#else
    if(ST_AutomapIsOpen(frags->player()) && ::cfg.common.automapHudDisplay == 0) return;
#endif
    if(P_MobjIsCamera(::players[frags->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(frags->_value == 1994) return;

    const auto valueAsText = String::asText(frags->_value);

    FR_SetFont(frags->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&frags->geometry(), textSize.width  * ::cfg.common.statusbarScale,
                                            textSize.height * ::cfg.common.statusbarScale);

#undef TRACKING
}
