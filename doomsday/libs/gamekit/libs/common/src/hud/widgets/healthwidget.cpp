/** @file healthwidget.cpp  GUI widget for -.
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

#include "hud/widgets/healthwidget.h"

#include "g_common.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "player.h"
#include "p_actor.h"

using namespace de;

guidata_health_t::guidata_health_t(void (*updateGeometry) (HudWidget *wi),
                                   void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                   dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_health_t::~guidata_health_t()
{}

void guidata_health_t::reset()
{
    _value = 1994;
}

void guidata_health_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t *plr = &::players[player()];
    _value = plr->health;
}

void HealthWidget_Draw(guidata_health_t *hlth, const Point2Raw *offset)
{
#if __JDOOM__
#  define X_OFFSET              ( 0 )
#  define Y_OFFSET              ( 0 )
#  define TRACKING              ( 0 )
#  define SUFFIX                ( "%" )
#else
#  define X_OFFSET              (-1 )
#  define Y_OFFSET              (-1 )
#  define TRACKING              ( 1 )
#  define SUFFIX                ( "" )
#endif

    DE_ASSERT(hlth);
    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(hlth->_value == 1994) return;

    if(!::cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsOpen(hlth->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[hlth->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(hlth->_value) + SUFFIX;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(hlth->font());
    FR_SetTracking(TRACKING);

#if __JHERETIC__
    // Draw a shadow.
    FR_SetColorAndAlpha(0, 0, 0, textOpacity * .4f);
    FR_DrawTextXY(valueAsText, X_OFFSET + 2, Y_OFFSET + 2);
#endif

    FR_SetColorAndAlpha(::cfg.common.hudColor[0], ::cfg.common.hudColor[1], ::cfg.common.hudColor[2], textOpacity);
    FR_DrawTextXY(valueAsText, X_OFFSET, Y_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef SUFFIX
#undef TRACKING
#undef Y_OFFSET
#undef X_OFFSET
}

void SBarHealthWidget_Draw(guidata_health_t *hlth, const Point2Raw *offset)
{
#if __JDOOM__
#  define X_OFFSET              ( 90 )
#  define Y_OFFSET              (  3 )
#elif __JHERETIC__
#  define X_OFFSET              ( 85 )
#  define Y_OFFSET              ( 12 )
#elif __JHEXEN__
#  define X_OFFSET              ( 64 )
#  define Y_OFFSET              ( 14 )
#else
#  define X_OFFSET              (  0 )
#  define Y_OFFSET              (  0 )
#endif
#if __JHERETIC__
#  define TRACKING              ( 1 )
#else
#  define TRACKING              ( 0 )
#endif
    DE_ASSERT(hlth);

    Vec2i const origin((-ST_WIDTH / 2 ) + X_OFFSET, (-ST_HEIGHT ) + Y_OFFSET );

    if(hlth->_value == 1994) return;

    const dint activeHud     = ST_ActiveHud(hlth->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(hlth->player()));
    const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    //const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

#if __JHERETIC__ || __JHEXEN__
    if(gfw_Rule(deathmatch)) return;
    if(Hu_InventoryIsOpen(hlth->player())) return;
#endif

#if __JHEXEN__
    if(ST_AutomapIsOpen(hlth->player())) return;
#else
    if(ST_AutomapIsOpen(hlth->player()) && ::cfg.common.automapHudDisplay == 0) return;
#endif
    if(P_MobjIsCamera(::players[hlth->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(hlth->_value);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(hlth->font());
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
    FR_DrawTextXY3(valueAsText, origin.x, origin.y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
#if __JDOOM__
    FR_DrawCharXY('%', origin.x, origin.y);
#endif

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
#undef Y_OFFSET
#undef X_OFFSET
}

void HealthWidget_UpdateGeometry(guidata_health_t *hlth)
{
#if __JDOOM__
#  define TRACKING              ( 0 )
#  define SUFFIX                ( "%" )
#else
#  define TRACKING              ( 1 )
#  define SUFFIX                ( "" )
#endif

    DE_ASSERT(hlth);
    Rect_SetWidthHeight(&hlth->geometry(), 0, 0);

    if(hlth->_value == 1994) return;

    if(!::cfg.hudShown[HUD_HEALTH]) return;
    if(ST_AutomapIsOpen(hlth->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[hlth->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(hlth->_value) + SUFFIX;

    FR_SetFont(hlth->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    textSize.width  *= ::cfg.common.hudScale;
    textSize.height *= ::cfg.common.hudScale;
    Rect_SetWidthHeight(&hlth->geometry(), textSize.width, textSize.height);

#undef SUFFIX
#undef TRACKING
}

void SBarHealthWidget_UpdateGeometry(guidata_health_t *hlth)
{
#if __JHERETIC__
#  define TRACKING              ( 1 )
#else
#  define TRACKING              ( 0 )
#endif

    DE_ASSERT(hlth);
    Rect_SetWidthHeight(&hlth->geometry(), 0, 0);

    if(hlth->_value == 1994) return;

#if __JHERETIC__ || __JHEXEN__
    if(gfw_Rule(deathmatch)) return;
    if(Hu_InventoryIsOpen(hlth->player())) return;
#endif

#if __JHEXEN__
    if(ST_AutomapIsOpen(hlth->player())) return;
#else
    if(ST_AutomapIsOpen(hlth->player()) && ::cfg.common.automapHudDisplay == 0) return;
#endif
    if(P_MobjIsCamera(::players[hlth->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    const auto valueAsText = String::asText(hlth->_value);

    FR_SetFont(hlth->font());
    FR_SetTracking(TRACKING);
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
#if __JDOOM__
    textSize.width += FR_CharWidth('%');
    textSize.height = de::max(textSize.height, FR_CharHeight('%'));
#endif
    Rect_SetWidthHeight(&hlth->geometry(), textSize.width  * ::cfg.common.statusbarScale,
                                           textSize.height * ::cfg.common.statusbarScale);

#undef TRACKING
}
