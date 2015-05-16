/** @file keyslotwidget.cpp  GUI widget for -.
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

#include "hud/widgets/keyslotwidget.h"

#include <de/Vector>
#include "common.h"
#include "gl_drawpatch.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "p_actor.h"

using namespace de;

static patchid_t pKeys[NUM_KEY_TYPES];

static void KeySlotWidget_UpdateGeometry(guidata_keyslot_t *kslt)
{
    DENG2_ASSERT(kslt);
    kslt->updateGeometry();
}

static void KeySlotWidget_Draw(guidata_keyslot_t *kslt, Point2Raw const *offset)
{
    DENG2_ASSERT(kslt);
    kslt->draw(offset? Vector2i(offset->xy) : Vector2i());
}

DENG2_PIMPL_NOREF(guidata_keyslot_t)
{
    dint slotNum = 0;
    keytype_t keyTypeA = keytype_t(0);
#if __JDOOM__
    keytype_t keyTypeB = keytype_t(0);
#endif

    patchid_t patchId  = -1;
#if __JDOOM__
    patchid_t patchId2 = -1;
#endif
};

guidata_keyslot_t::guidata_keyslot_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(KeySlotWidget_UpdateGeometry),
                function_cast<DrawFunc>(KeySlotWidget_Draw),
                player)
    , d(new Instance)
{}

guidata_keyslot_t::~guidata_keyslot_t()
{}

void guidata_keyslot_t::reset()
{
    d->patchId  = -1;
#if __JDOOM__
    d->patchId2 = -1;
#endif
}

guidata_keyslot_t &guidata_keyslot_t::setSlot(dint newSlotNum)
{
    d->slotNum  = newSlotNum;
    /// @todo Do not assume a slot-number => key-type relationship.
    d->keyTypeA = keytype_t( newSlotNum );
#if __JDOOM__
    d->keyTypeB = keytype_t( newSlotNum + 3 );
#endif
    return *this;
}

void guidata_keyslot_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    player_t const *plr = &players[player()];

#if __JDOOM__
    d->patchId = -1;
    if(plr->keys[d->keyTypeA] || plr->keys[d->keyTypeB])
    {
        d->patchId = ::pKeys[plr->keys[d->keyTypeB]? d->keyTypeB : d->keyTypeA];
    }

    d->patchId2 = -1;
    if(!cfg.hudKeysCombine && plr->keys[d->keyTypeA] && plr->keys[d->keyTypeB])
    {
        d->patchId2 = ::pKeys[d->keyTypeA];
    }
#elif __JHEXEN__
    d->patchId = (plr->keys & (1 << d->keyTypeA)) ? pKeys[d->keyTypeA] : -1;
#else
    d->patchId = plr->keys[d->keyTypeA] ? pKeys[d->keyTypeA] : -1;
#endif
}

void guidata_keyslot_t::draw(Vector2i const &offset) const
{
    Vector2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    dint const activeHud     = ST_ActiveHud(player());
    dint const yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(player()));
    dfloat const iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

#if __JHERETIC__ || __JHEXEN__
    if(Hu_InventoryIsOpen(player())) return;
#endif

#if __JDOOM__
    if(d->patchId <= 0 && d->patchId2 <= 0) return;
#else
    if(d->patchId <= 0) return;
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);

#if __JDOOM__
    Vector2i const comboOffset((d->patchId2 > 0? -1 : 0), (d->patchId2 > 0? -1 : 0));
    GL_DrawPatch(d->patchId, origin + Vector2i(239, 3 + 10 * d->slotNum) + comboOffset);
    if(d->patchId2 > 0)
    {
        GL_DrawPatch(d->patchId2, origin + Vector2i(239, 3 + 10 * d->slotNum) - comboOffset);
    }
#else
    GL_DrawPatch(d->patchId, origin + Vector2i(153, 6 + 8 * d->keyTypeA));
#endif

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void guidata_keyslot_t::updateGeometry()
{
    Rect_SetWidthHeight(&geometry(), 0, 0);

#if __JHERETIC__
    if(Hu_InventoryIsOpen(player())) return;
#endif
    if(ST_AutomapIsOpen(player()) && cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

#if __JDOOM__
    if(d->patchId <= 0 && d->patchId2 <= 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(d->patchId, &info)) return;

    Rect_SetWidthHeight(&geometry(), info.geometry.size.width,
                                     info.geometry.size.height);

    if(d->patchId2 > 0 && R_GetPatchInfo(d->patchId, &info))
    {
        info.geometry.origin.x = info.geometry.origin.y = 2; // Combine offset.
        Rect_UniteRaw(&geometry(), &info.geometry);
    }

    Rect_SetWidthHeight(&geometry(), Rect_Width (&geometry()) * cfg.common.statusbarScale,
                                     Rect_Height(&geometry()) * cfg.common.statusbarScale);
#else
    patchinfo_t info;
    if(d->patchId <= 0 || !R_GetPatchInfo(d->patchId, &info)) return;

    Rect_SetWidthHeight(&geometry(), info.geometry.size.width  * cfg.common.statusbarScale,
                                     info.geometry.size.height * cfg.common.statusbarScale);
#endif
}

void guidata_keyslot_t::prepareAssets()
{
#if __JHERETIC__
    dint n = 0;
    ::pKeys[/*KT_YELLOW*/ n++] = R_DeclarePatch("YKEYICON");
    ::pKeys[/*KT_GREEN*/  n++] = R_DeclarePatch("GKEYICON");
    ::pKeys[/*KT_BLUE*/   n++] = R_DeclarePatch("BKEYICON");
    DENG2_ASSERT(n == NUM_KEY_TYPES);
#else
    for(dint i = 0; i < NUM_KEY_TYPES; ++i)
    {
        ::pKeys[i] = R_DeclarePatch(String("STKEYS%1").arg(i).toUtf8().constData());
    }
#endif
}
