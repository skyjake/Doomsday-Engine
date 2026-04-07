/** @file keyswidget.cpp  GUI widget for -.
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

#include "hud/widgets/keyswidget.h"

#include "gl_drawpatch.h"
#if __JHERETIC__ || __JHEXEN__
#  include "hu_inventory.h"
#endif
#include "player.h"

#if __JHERETIC__ || __JHEXEN__
static patchid_t pKeys[NUM_KEY_TYPES];
#endif

using namespace de;

static void KeysWidget_UpdateGeometry(guidata_keys_t *keys)
{
    DE_ASSERT(keys);
    keys->updateGeometry();
}

static void KeysWidget_Draw(guidata_keys_t *keys, const Point2Raw *offset)
{
    DE_ASSERT(keys);
    keys->draw(offset? Vec2i(offset->xy) : Vec2i());
}

guidata_keys_t::guidata_keys_t(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(KeysWidget_UpdateGeometry),
                function_cast<DrawFunc>(KeysWidget_Draw),
                player)
{}

guidata_keys_t::~guidata_keys_t()
{}

void guidata_keys_t::reset()
{
    de::zap(_keyBoxes);
}

void guidata_keys_t::tick(timespan_t /*elapsed*/)
{
    if(Pause_IsPaused() || !DD_IsSharpTick()) return;

    const player_t *plr = &::players[player()];
    for(dint i = 0; i < NUM_KEY_TYPES; ++i)
    {
#if __JHEXEN__
        _keyBoxes[i] = (plr->keys & (1 << i));
#else
        _keyBoxes[i] = CPP_BOOL(plr->keys[i]);
#endif
    }
}

void guidata_keys_t::draw(const Vec2i &offset) const
{
#if __JDOOM__

#define EXTRA_SCALE         ( .75f )

    static dint const keyPairs[3][2] = {
        { KT_REDCARD,    KT_REDSKULL },
        { KT_YELLOWCARD, KT_YELLOWSKULL },
        { KT_BLUECARD,   KT_BLUESKULL }
    };
    static dint const keySprites[NUM_KEY_TYPES] = {
        SPR_BKEY, SPR_YKEY, SPR_RKEY, SPR_BSKU, SPR_YSKU, SPR_RSKU
    };

    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;
    //const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];

    if(!::cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(EXTRA_SCALE * ::cfg.common.hudScale, EXTRA_SCALE * ::cfg.common.hudScale, 1);

    dint x = 0;
    dint numDrawnKeys = 0;
    for(dint i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(!_keyBoxes[i]) continue;

        bool shown = true;
        if(::cfg.hudKeysCombine)
        {
            for(dint k = 0; k < 3; ++k)
            {
                if(keyPairs[k][0] == i && _keyBoxes[keyPairs[k][0]] && _keyBoxes[keyPairs[k][1]])
                {
                    shown = false;
                    break;
                }
            }
        }

        if(shown)
        {
            dint w, h;
            GUI_DrawSprite(keySprites[i], x, 0, HOT_TLEFT, 1, iconOpacity, false, &w, &h);
            x += w + 2;
            numDrawnKeys += 1;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef EXTRA_SCALE

#elif __JHERETIC__  // __JDOOM__

    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(!::cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t pInfo;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);

    dint x = 0;
    if(_keyBoxes[KT_YELLOW])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(::pKeys[0], Vec2i(x, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        if(R_GetPatchInfo(::pKeys[0], &pInfo))
            x += pInfo.geometry.size.width + 1;
    }

    if(_keyBoxes[KT_GREEN])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(::pKeys[1], Vec2i(x, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);

        if(R_GetPatchInfo(::pKeys[1], &pInfo))
            x += pInfo.geometry.size.width + 1;
    }

    if(_keyBoxes[KT_BLUE])
    {
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(::pKeys[2], Vec2i(x, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Disable(DGL_TEXTURE_2D);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#elif __JHEXEN__  // __JHERETIC__

#define ORIGINX             (-ST_WIDTH / 2 )
#define ORIGINY             (-ST_HEIGHT * ST_StatusBarShown(player()) )

    const dint activeHud     = ST_ActiveHud(player());
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(player()) || !ST_AutomapIsOpen(player())) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);

    dint numDrawn = 0;
    for(dint i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(!_keyBoxes[i]) continue;

        patchid_t patch = ::pKeys[i];
        DGL_Enable(DGL_TEXTURE_2D);
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(patch, Vec2i(ORIGINX + 46 + numDrawn * 20, ORIGINY + 1));

        DGL_Disable(DGL_TEXTURE_2D);

        numDrawn += 1;
        if(numDrawn == 5) break;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ORIGINY
#undef ORIGINX

#else  // __JHEXEN__
    DE_UNUSED(offset);
#endif
}

void guidata_keys_t::updateGeometry()
{
#if __JDOOM__

#define EXTRA_SCALE         ( .75f )

    static dint const keyPairs[3][2] = {
        { KT_REDCARD,    KT_REDSKULL },
        { KT_YELLOWCARD, KT_YELLOWSKULL },
        { KT_BLUECARD,   KT_BLUESKULL }
    };
    static dint const keySprites[NUM_KEY_TYPES] = {
        SPR_BKEY, SPR_YKEY, SPR_RKEY, SPR_BSKU, SPR_YSKU, SPR_RSKU
    };

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!::cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    RectRaw iconGeometry{};
    for(int i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(!_keyBoxes[i]) continue;

        bool shown = true;
        if(cfg.hudKeysCombine)
        {
            for(dint k = 0; k < 3; ++k)
            {
                if(keyPairs[k][0] == i && _keyBoxes[keyPairs[k][0]] && _keyBoxes[keyPairs[k][1]])
                {
                    shown = false;
                    break;
                }
            }
        }

        if(shown)
        {
            GUI_SpriteSize(keySprites[i], 1, &iconGeometry.size.width, &iconGeometry.size.height);
            Rect_UniteRaw(&geometry(), &iconGeometry);
            iconGeometry.origin.x += iconGeometry.size.width + 2;
        }
    }

    Rect_SetWidthHeight(&geometry(), Rect_Width (&geometry()) * EXTRA_SCALE * ::cfg.common.hudScale,
                                     Rect_Height(&geometry()) * EXTRA_SCALE * ::cfg.common.hudScale);

#undef EXTRA_SCALE

#elif __JHERETIC__  // __JDOOM__

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(!::cfg.hudShown[HUD_KEYS]) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t pInfo;
    dint x = 0;

    if(_keyBoxes[KT_YELLOW] && R_GetPatchInfo(::pKeys[0], &pInfo))
    {
        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(&geometry(), &pInfo.geometry);

        x += pInfo.geometry.size.width + 1;
    }

    if(_keyBoxes[KT_GREEN] && R_GetPatchInfo(::pKeys[1], &pInfo))
    {
        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(&geometry(), &pInfo.geometry);

        x += pInfo.geometry.size.width + 1;
    }

    if(_keyBoxes[KT_BLUE] && R_GetPatchInfo(::pKeys[2], &pInfo))
    {
        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(&geometry(), &pInfo.geometry);
    }

    Rect_SetWidthHeight(&geometry(), Rect_Width (&geometry()) * ::cfg.common.hudScale,
                                     Rect_Height(&geometry()) * ::cfg.common.hudScale);

#elif __JHEXEN__  // __JHERETIC__

    Rect_SetWidthHeight(&geometry(), 0, 0);

    if(Hu_InventoryIsOpen(player()) || !ST_AutomapIsOpen(player())) return;
    if(ST_AutomapIsOpen(player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t pInfo;

    dint x = 0;
    dint numVisible = 0;
    for(dint i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(!_keyBoxes[i]) continue;

        patchid_t patch = ::pKeys[i];
        if(!R_GetPatchInfo(patch, &pInfo)) continue;

        pInfo.geometry.origin.x = x;
        pInfo.geometry.origin.y = 0;
        Rect_UniteRaw(&geometry(), &pInfo.geometry);

        numVisible += 1;
        if(numVisible == 5) break;

        x += 20;
    }

    Rect_SetWidthHeight(&geometry(), Rect_Width (&geometry()) * ::cfg.common.statusbarScale,
                                     Rect_Height(&geometry()) * ::cfg.common.statusbarScale);

#endif  // __JHEXEN__
}

void guidata_keys_t::prepareAssets()  // static
{
#if __JHERETIC__
    pKeys[0] = R_DeclarePatch("YKEYICON");
    pKeys[1] = R_DeclarePatch("GKEYICON");
    pKeys[2] = R_DeclarePatch("BKEYICON");
#elif __JHEXEN__
    for(dint i = 0; i < NUM_KEY_TYPES; ++i)
    {
        pKeys[i] = R_DeclarePatch(Stringf("KEYSLOT%X", i + 1));
    }
#endif
}
