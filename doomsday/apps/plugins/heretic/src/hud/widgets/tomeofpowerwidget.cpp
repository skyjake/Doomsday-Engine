/** @file tomeofpowerwidget.cpp  GUI widget for -.
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

#include "hud/widgets/tomeofpowerwidget.h"

#include "doomdef.h"
#include "p_tick.h"
#include "player.h"

using namespace de;

#define FRAME_COUNT                 ( 16 )

static void TomeWidget_Draw(guidata_tomeofpower_t *tome, Point2Raw const *offset)
{
    DENG2_ASSERT(tome);
    tome->draw(offset? Vector2i(offset->xy) : Vector2i());
}

static void TomeWidget_UpdateGeometry(guidata_tomeofpower_t *tome)
{
    DENG2_ASSERT(tome);
    tome->updateGeometry();
}

static patchid_t pIcons[FRAME_COUNT];

DENG2_PIMPL_NOREF(guidata_tomeofpower_t)
{
    patchid_t patchId = 0;
    dint countdownSeconds = 0;  ///< Number of seconds remaining or zero if disabled.
    dint play = 0;              ///< Used with the countdown sound.
};

guidata_tomeofpower_t::guidata_tomeofpower_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(TomeWidget_UpdateGeometry),
                function_cast<DrawFunc>(TomeWidget_Draw),
                player)
    , d(new Instance)
{}

guidata_tomeofpower_t::~guidata_tomeofpower_t()
{}

void guidata_tomeofpower_t::reset()
{
    d->patchId = 0;
    d->play    = 0;
}

void guidata_tomeofpower_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    d->patchId = 0;
    d->countdownSeconds = 0;

    player_t const *plr  = &::players[player()];
    dint const ticsRemain = plr->powers[PT_WEAPONLEVEL2];
    if(ticsRemain <= 0 || 0 != plr->morphTics) return;

    // Time to play the countdown sound?
    if(ticsRemain && ticsRemain < cfg.tomeSound * TICSPERSEC)
    {
        dint timeleft = ticsRemain / TICSPERSEC;
        if(d->play != timeleft)
        {
            d->play = timeleft;
            S_LocalSound(SFX_KEYUP, NULL);
        }
    }

    if(::cfg.tomeCounter > 0 || ticsRemain > BLINKTHRESHOLD || !(ticsRemain & FRAME_COUNT))
    {
        dint frame = (mapTime / 3) & (FRAME_COUNT - 1);
        d->patchId = ::pIcons[frame];
    }

    if(::cfg.tomeCounter > 0 && ticsRemain < cfg.tomeCounter * TICSPERSEC)
    {
        d->countdownSeconds = 1 + ticsRemain / TICSPERSEC;
    }
}

void guidata_tomeofpower_t::draw(Vector2i const &offset) const
{
#define TRACKING            ( 2 )

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(d->patchId == 0 && d->countdownSeconds == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    if(d->patchId != 0)
    {
        dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;
        if(d->countdownSeconds != 0)
        {
            iconOpacity *= d->countdownSeconds / dfloat( cfg.tomeCounter );
        }

        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(d->patchId, Vector2i(13, 13));

        DGL_Disable(DGL_TEXTURE_2D);
    }

    if(d->countdownSeconds != 0)
    {
        auto const counterAsText = String::number(d->countdownSeconds);
        dfloat const textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

        DGL_Enable(DGL_TEXTURE_2D);

        FR_SetFont(font());
        FR_SetTracking(TRACKING);
        FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
        FR_DrawTextXY2(counterAsText.toUtf8().constData(), 26, 26 - 2, ALIGN_BOTTOMRIGHT);

        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void guidata_tomeofpower_t::updateGeometry()
{
#define TRACKING            ( 2 )

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    player_t const *plr   = &::players[player()];
    dint const ticsRemain = plr->powers[PT_WEAPONLEVEL2];
    if(ticsRemain <= 0 || 0 != plr->morphTics) return;

    if(d->patchId != 0)
    {
        // @todo Determine the actual center point of the animation at widget creation time.
        Rect_SetWidthHeight(&geometry(), 26 * ::cfg.common.hudScale, 26 * ::cfg.common.hudScale);
    }
    else
    {
        auto const counterAsText = String::number(d->countdownSeconds);

        FR_SetFont(font());
        FR_SetTracking(TRACKING);
        Size2Raw textSize; FR_TextSize(&textSize, counterAsText.toUtf8().constData());
        Rect_SetWidthHeight(&geometry(), textSize.width  * ::cfg.common.hudScale,
                                         textSize.height * ::cfg.common.hudScale);
    }

#undef TRACKING
}

void guidata_tomeofpower_t::prepareAssets()  // static
{
    for(dint i = 0; i < FRAME_COUNT; ++i)
    {
        ::pIcons[i] = R_DeclarePatch(String("SPINBK%1").arg(i).toUtf8().constData());
    }
}
