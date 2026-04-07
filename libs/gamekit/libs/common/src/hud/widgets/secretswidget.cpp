/** @file secretswidget.cpp  GUI widget for -.
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

#include "hud/widgets/secretswidget.h"

#include "common.h"
#include "p_actor.h"

using namespace de;

static void SecretsWidget_Draw(guidata_secrets_t *scrts, const Point2Raw *offset)
{
    DE_ASSERT(scrts);
    scrts->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void SecretsWidget_UpdateGeometry(guidata_secrets_t *scrts)
{
    DE_ASSERT(scrts);
    scrts->updateGeometry();
}

guidata_secrets_t::guidata_secrets_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(SecretsWidget_UpdateGeometry),
                function_cast<DrawFunc>(SecretsWidget_Draw),
                player)
{}

guidata_secrets_t::~guidata_secrets_t()
{}

void guidata_secrets_t::reset()
{
    _value = 1994;
}

void guidata_secrets_t::draw(const Vec2i &offset) const
{
#if !__JHEXEN__

    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(_value == 1994) return;

    if(!(::cfg.common.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT))) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(::cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsOpen(player())) return;

    String valueAsText("Secret");
    if(::cfg.common.hudShownCheatCounters & CCH_SECRETS)
    {
        valueAsText += Stringf(" %i/%i", _value, totalSecret);
    }
    if(::cfg.common.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        valueAsText += Stringf(" %s%i%%%s",
                                      (cfg.common.hudShownCheatCounters & CCH_SECRETS) ? "(" : "",
                                      totalSecret ? _value * 100 / totalSecret : 100,
                                      (cfg.common.hudShownCheatCounters & CCH_SECRETS) ? ")" : "");
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudCheatCounterScale, ::cfg.common.hudCheatCounterScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(font());
    FR_SetColorAndAlpha(::cfg.common.hudColor[0], ::cfg.common.hudColor[1], ::cfg.common.hudColor[2], textOpacity);
    FR_DrawTextXY(valueAsText, 0, 0);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#else  // !__JHEXEN__
    DE_UNUSED(offset);
#endif
}

void guidata_secrets_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t &plr = ::players[player()];
    _value = plr.secretCount;
}

void guidata_secrets_t::updateGeometry()
{
#if !__JHEXEN__

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(_value == 1994) return;

    if(!(::cfg.common.hudShownCheatCounters & (CCH_SECRETS | CCH_SECRETS_PRCNT))) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(::cfg.common.hudCheatCounterShowWithAutomap && !ST_AutomapIsOpen(player())) return;

    String valueAsText("Secret");
    if(::cfg.common.hudShownCheatCounters & CCH_SECRETS)
    {
        valueAsText += Stringf(" %i/%i", _value, totalSecret);
    }
    if(::cfg.common.hudShownCheatCounters & CCH_SECRETS_PRCNT)
    {
        valueAsText += Stringf(" %s%i%%%s",
                                      (cfg.common.hudShownCheatCounters & CCH_SECRETS) ? "(" : "",
                                      totalSecret ? _value * 100 / totalSecret : 100,
                                      (cfg.common.hudShownCheatCounters & CCH_SECRETS) ? ")" : "");
    }

    FR_SetFont(font());
    Size2Raw textSize; FR_TextSize(&textSize, valueAsText);
    Rect_SetWidthHeight(&geometry(), .5f + textSize.width  * ::cfg.common.hudCheatCounterScale,
                                     .5f + textSize.height * ::cfg.common.hudCheatCounterScale);

#endif  // !__JHEXEN__
}
