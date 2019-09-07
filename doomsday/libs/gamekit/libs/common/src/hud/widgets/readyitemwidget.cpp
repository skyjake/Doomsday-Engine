/** @file readyitemwidget.cpp  GUI widget for -.
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

#include "hud/widgets/readyitemwidget.h"

#include "common.h"
#include "gl_drawpatch.h"
#include "hu_inventory.h"
#include "p_inventory.h"

using namespace de;

#define FLASH_FRAME_COUNT           ( 5 )

static patchid_t pBackground;
static patchid_t pIcons[FLASH_FRAME_COUNT];

guidata_readyitem_t::guidata_readyitem_t(void (*updateGeometry) (HudWidget *wi),
                                         void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                                         dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
{}

guidata_readyitem_t::~guidata_readyitem_t()
{}

void guidata_readyitem_t::reset()
{
    _patchId = 0;
}

void guidata_readyitem_t::tick(timespan_t /*elapsed*/)
{
    _patchId = 0;

#if __JHERETIC__ || __JHEXEN__
    const dint flashCounter = ST_ReadyItemFlashCounter(player());
    if(flashCounter > 0)
    {
        _patchId = ::pIcons[flashCounter % FLASH_FRAME_COUNT];
    }
    else
    {
        inventoryitemtype_t readyItem = P_InventoryReadyItem(player());
        if(readyItem != IIT_NONE)
        {
            _patchId = P_GetInvItem(dint( readyItem ) - 1)->patchId;
        }
    }
#endif
}

#if __JHERETIC__

void ReadyItem_Drawer(guidata_readyitem_t *item, const Point2Raw *offset)
{
#define TRACKING                ( 2 )

    DE_ASSERT(item);
    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(!::cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t boxInfo;
    if(!R_GetPatchInfo(::pBackground, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.hudScale, cfg.common.hudScale, 1);

    if(item->_patchId)
    {
        dint xOffset = 0, yOffset = 0;
        if(ST_ReadyItemFlashCounter(item->player()) > 0)
        {
            xOffset += 2;
            yOffset += 1;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        if(offset) DGL_Translatef(offset->x, offset->y, 0);
        DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
        DGL_Enable(DGL_TEXTURE_2D);

        DGL_Color4f(1, 1, 1, iconOpacity / 2);
        GL_DrawPatch(::pBackground, Vec2i(0, 0), ALIGN_TOPLEFT, DPF_NO_OFFSET);
        DGL_Color4f(1, 1, 1, iconOpacity);
        GL_DrawPatch(item->_patchId, Vec2i(xOffset, yOffset));

        inventoryitemtype_t readyItem = P_InventoryReadyItem(item->player());
        if(!(ST_ReadyItemFlashCounter(item->player()) > 0) && IIT_NONE != readyItem)
        {
            duint count = P_InventoryCount(item->player(), readyItem);
            if(count > 1)
            {
                const auto countAsText = String::asText(count);
                FR_SetFont(item->font());
                FR_SetTracking(TRACKING);
                FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
                FR_DrawTextXY2(countAsText,
                               boxInfo.geometry.size.width - 1, boxInfo.geometry.size.height - 3,
                               ALIGN_BOTTOMRIGHT);
            }
        }

        DGL_Disable(DGL_TEXTURE_2D);
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef TRACKING
}

void SBarReadyItem_Drawer(guidata_readyitem_t *item, const Point2Raw *offset)
{
#define TRACKING                ( 2 )
#define ORIGINX                 (-ST_WIDTH / 2 )
#define ORIGINY                 (-ST_HEIGHT * ST_StatusBarShown(item->player()) )
#define ICON_X_OFFSET           ( 179 )
#define ICON_Y_OFFSET           (   2 )
#define COUNT_X_OFFSET          ( 208 )
#define COUNT_Y_OFFSET          (  24 )

    DE_ASSERT(item);

    const dint activeHud     = ST_ActiveHud(item->player());
    const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(!item->_patchId) return;

    dint x, y;
    if(ST_ReadyItemFlashCounter(item->player()) > 0)
    {
        x = ORIGINX + ICON_X_OFFSET + 2;
        y = ORIGINY + ICON_Y_OFFSET + 1;
    }
    else
    {
        x = ORIGINX + ICON_X_OFFSET;
        y = ORIGINY + ICON_Y_OFFSET;
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(item->_patchId, Vec2i(x, y));

    inventoryitemtype_t readyItem = P_InventoryReadyItem(item->player());
    if(!(ST_ReadyItemFlashCounter(item->player()) > 0) && IIT_NONE != readyItem)
    {
        duint count = P_InventoryCount(item->player(), readyItem);
        if(count > 1)
        {
            const auto countAsText = String::asText(count);
            FR_SetFont(item->font());
            FR_SetTracking(TRACKING);
            FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
            FR_DrawTextXY3(countAsText,
                           ORIGINX + COUNT_X_OFFSET, ORIGINY + COUNT_Y_OFFSET,
                           ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef COUNT_Y_OFFSET
#undef COUNT_X_OFFSET
#undef ICON_Y_OFFSET
#undef ICON_X_OFFSET
#undef ORIGINY
#undef ORIGINX
#undef TRACKING
}

void ReadyItem_UpdateGeometry(guidata_readyitem_t *item)
{
    DE_ASSERT(item);

    Rect_SetWidthHeight(&item->geometry(), 0, 0);

    if(!::cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(::pBackground, &info)) return;

    Rect_SetWidthHeight(&item->geometry(), info.geometry.size.width  * ::cfg.common.hudScale,
                                           info.geometry.size.height * ::cfg.common.hudScale);
}

void SBarReadyItem_UpdateGeometry(guidata_readyitem_t *item)
{
    DE_ASSERT(item);

    Rect_SetWidthHeight(&item->geometry(), 0, 0);

    if(Hu_InventoryIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(!item->_patchId) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(item->_patchId, &info)) return;

    // @todo Calculate dimensions properly!
    Rect_SetWidthHeight(&item->geometry(), info.geometry.size.width  * ::cfg.common.statusbarScale,
                                           info.geometry.size.height * ::cfg.common.statusbarScale);
}

#endif  // __JHERETIC__

#if __JHEXEN__

void ReadyItem_Drawer(guidata_readyitem_t *item, const Point2Raw* offset)
{
    DE_ASSERT(item);
    const dfloat textOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudColor[3];
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(!::cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(!item->_patchId) return;

    patchinfo_t boxInfo;
    if(!R_GetPatchInfo(::pBackground, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.hudScale, ::cfg.common.hudScale, 1);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconOpacity / 2);
    GL_DrawPatch(::pBackground, Vec2i(0, 0));

    dint xOffset, yOffset;
    if(ST_ReadyItemFlashCounter(item->player()) > 0)
    {
        xOffset = 3;
        yOffset = 0;
    }
    else
    {
        xOffset = -2;
        yOffset = -1;
    }

    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(item->_patchId, Vec2i(xOffset, yOffset));

    inventoryitemtype_t readyItem = P_InventoryReadyItem(item->player());
    if(ST_ReadyItemFlashCounter(item->player()) == 0 && readyItem != IIT_NONE)
    {
        duint count = P_InventoryCount(item->player(), readyItem);
        if(count > 1)
        {
            const auto countAsText = String::asText(count);

            FR_SetFont(item->font());
            FR_SetTracking(0);
            FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
            FR_DrawTextXY2(countAsText,
                           boxInfo.geometry.size.width - 1, boxInfo.geometry.size.height - 3,
                           ALIGN_BOTTOMRIGHT);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void SBarReadyItem_Drawer(guidata_readyitem_t *item, const Point2Raw *offset)
{
#define ORIGINX             (-ST_WIDTH / 2 )
#define ORIGINY             (-ST_HEIGHT )
#define ST_INVITEMX         ( 143 )
#define ST_INVITEMY         (   1 )
#define ST_INVITEMCX        ( 174 )
#define ST_INVITEMCY        (  22 )

    DE_ASSERT(item);

    const dint activeHud     = ST_ActiveHud(item->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(item->player()));
    const dfloat textOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(Hu_InventoryIsOpen(item->player()) || ST_AutomapIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(!item->_patchId) return;

    patchinfo_t boxInfo;
    if(!R_GetPatchInfo(::pBackground, &boxInfo)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(::cfg.common.statusbarScale, ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    dint x, y;
    if(ST_ReadyItemFlashCounter(item->player()) > 0)
    {
        x = ST_INVITEMX + 4;
        y = ST_INVITEMY;
    }
    else
    {
        x = ST_INVITEMX;
        y = ST_INVITEMY;
    }

    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Color4f(1, 1, 1, iconOpacity);
    GL_DrawPatch(item->_patchId, Vec2i(ORIGINX + x, ORIGINY + y));

    inventoryitemtype_t readyItem = P_InventoryReadyItem(item->player());
    if(!(ST_ReadyItemFlashCounter(item->player()) > 0) && readyItem != IIT_NONE)
    {
        duint count = P_InventoryCount(item->player(), readyItem);
        if(count > 1)
        {
            const auto countAsText = String::asText(count);

            FR_SetFont(item->font());
            FR_SetTracking(0);
            FR_SetColorAndAlpha(::defFontRGB2[0], ::defFontRGB2[1], ::defFontRGB2[2], textOpacity);
            FR_DrawTextXY3(countAsText,
                           ORIGINX + ST_INVITEMCX, ORIGINY + ST_INVITEMCY,
                           ALIGN_TOPRIGHT, DTF_NO_EFFECTS);
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef ST_INVITEMCY
#undef ST_INVITEMCX
#undef ST_INVITEMY
#undef ST_INVITEMX
#undef ORIGINY
#undef ORIGINX
}

void ReadyItem_UpdateGeometry(guidata_readyitem_t *item)
{
    DE_ASSERT(item);

    Rect_SetWidthHeight(&item->geometry(), 0, 0);

    if(!::cfg.hudShown[HUD_READYITEM]) return;
    if(Hu_InventoryIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(::pBackground, &info)) return;

    Rect_SetWidthHeight(&item->geometry(), info.geometry.size.width  * ::cfg.common.hudScale,
                                           info.geometry.size.height * ::cfg.common.hudScale);
}

void SBarReadyItem_UpdateGeometry(guidata_readyitem_t *item)
{
    DE_ASSERT(item);

    Rect_SetWidthHeight(&item->geometry(), 0, 0);

    if(Hu_InventoryIsOpen(item->player()) || ST_AutomapIsOpen(item->player())) return;
    if(ST_AutomapIsOpen(item->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[item->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(item->_patchId != 0) return;

    patchinfo_t info;
    if(!R_GetPatchInfo(::pBackground, &info)) return;

    Rect_SetWidthHeight(&item->geometry(), info.geometry.size.width  * ::cfg.common.statusbarScale,
                                           info.geometry.size.height * ::cfg.common.statusbarScale);
}

#endif  // __JHEXEN__

void guidata_readyitem_t::prepareAssets()  // static
{
    ::pBackground = R_DeclarePatch("ARTIBOX");
    for(dint i = 0; i < FLASH_FRAME_COUNT; ++i)
    {
        ::pIcons[i] = R_DeclarePatch(Stringf("USEARTI%c", char('A' + i)));
    }
}
