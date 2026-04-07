/** @file flightwidget.cpp  GUI widget for -.
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

#include "hud/widgets/flightwidget.h"

#include "common.h"
#include "gl_drawpatch.h"
#include "p_tick.h"

using namespace de;

static void FlightWidget_UpdateGeometry(guidata_flight_t *flht)
{
    DE_ASSERT(flht);
    flht->updateGeometry();
}

static void FlightWidget_Draw(guidata_flight_t *flht, const Point2Raw *offset)
{
    DE_ASSERT(flht);
    flht->draw(offset? Vec2i(offset->xy) : Vec2i());
}

guidata_flight_t::guidata_flight_t(int player)
    : HudWidget(function_cast<UpdateGeometryFunc>(FlightWidget_UpdateGeometry),
                function_cast<DrawFunc>(FlightWidget_Draw),
                player)
{}

#if __JHERETIC__ || __JHEXEN__
static patchid_t pIcon[16];
#endif

guidata_flight_t::~guidata_flight_t()
{}

void guidata_flight_t::reset()
{
    _patchId        = 0;
    _hitCenterFrame = false;
}

void guidata_flight_t::tick(timespan_t /*elapsed*/)
{
#if __JHERETIC__ || __JHEXEN__
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    _patchId = 0;

    const player_t *plr = &::players[player()];
    if(plr->powers[PT_FLIGHT] <= 0) return;

    if(plr->powers[PT_FLIGHT] > BLINKTHRESHOLD || !(plr->powers[PT_FLIGHT] & 16))
    {
        dint frame = (::mapTime / 3) & 15;
        if(plr->plr->mo->flags2 & MF2_FLY)
        {
            if(_hitCenterFrame && (frame != 15 && frame != 0))
                frame = 15;
            else
                _hitCenterFrame = false;
        }
        else
        {
            if(!_hitCenterFrame && (frame != 15 && frame != 0))
            {
                _hitCenterFrame = false;
            }
            else
            {
                frame = 15;
                _hitCenterFrame = true;
            }
        }
        _patchId = ::pIcon[frame];
    }
#endif
}

void guidata_flight_t::draw(const Vec2i &offset) const
{
#if __JHERETIC__ || __JHEXEN__
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(_patchId)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Translatef(offset.x, offset.y, 0);
        DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(_patchId, Vec2i(16, 14));

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
#else
    DE_UNUSED(offset);
#endif
}

void guidata_flight_t::updateGeometry()
{
#if __JHERETIC__ || __JHEXEN__
#  if __JHERETIC__
#  define HEIGHT                ( 32 )
#  else
#  define HEIGHT                ( 28 )
#  endif

    const player_t *plr = &::players[player()];

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(plr->powers[PT_FLIGHT] <= 0) return;

    /// @todo Calculate dimensions properly!
    Rect_SetWidthHeight(&geometry(), 32 * ::cfg.common.hudScale, HEIGHT * ::cfg.common.hudScale);

#endif
}

void guidata_flight_t::prepareAssets()  // static
{
#if __JHERETIC__ || __JHEXEN__
    for(dint i = 0; i < 16; ++i)
    {
        ::pIcon[i] = R_DeclarePatch(Stringf("SPFLY%i", i));
    }
#endif
}
