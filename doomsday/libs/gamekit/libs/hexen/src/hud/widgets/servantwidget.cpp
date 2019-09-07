/** @file servantwidget.cpp  GUI widget for -.
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

#include "hud/widgets/servantwidget.h"

#include "jhexen.h"
#include "gl_drawpatch.h"

using namespace de;

#define FRAME_COUNT             ( 16 )  ///< min 1

static patchid_t pServantIcon[FRAME_COUNT];

static void ServantWidget_Draw(guidata_servant_t *svnt, const Point2Raw *offset)
{
    DE_ASSERT(svnt);
    svnt->draw(offset? Vec2i(offset->xy) : Vec2i());
}

static void ServantWidget_UpdateGeometry(guidata_servant_t *svnt)
{
    DE_ASSERT(svnt);
    svnt->updateGeometry();
}

guidata_servant_t::guidata_servant_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(ServantWidget_UpdateGeometry),
                function_cast<DrawFunc>(ServantWidget_Draw),
                player)
{}

guidata_servant_t::~guidata_servant_t()
{}

void guidata_servant_t::reset()
{
    _patchId = 0;
}

void guidata_servant_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    _patchId = 0;

    const player_t &plr = ::players[player()];
    if(plr.powers[PT_MINOTAUR] &&
       (plr.powers[PT_MINOTAUR] > BLINKTHRESHOLD || !(plr.powers[PT_MINOTAUR] & 16)))
    {
        _patchId = ::pServantIcon[(::mapTime / 3) & (FRAME_COUNT - 1)];
    }
}

void guidata_servant_t::draw(const Vec2i &offset) const
{
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(_patchId == 0) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(_patchId, Vec2i(13, 17));
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_servant_t::updateGeometry()
{
    const player_t &plr = ::players[player()];

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;
    if(!plr.powers[PT_MINOTAUR]) return;

    Rect_SetWidthHeight(&geometry(), 26 * ::cfg.common.hudScale,
                                     29 * ::cfg.common.hudScale);
}

void guidata_servant_t::prepareAssets()  // static
{
    for(dint i = 0; i < FRAME_COUNT; ++i)
    {
        ::pServantIcon[i] = R_DeclarePatch(Stringf("SPMINO%i", i));
    }
}
