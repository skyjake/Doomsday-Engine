/** @file colorpreviewwidget.cpp  UI widget for previewing a color.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "menu/widgets/colorpreviewwidget.h"

#include "hu_menu.h" // menu sounds
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

DENG2_PIMPL_NOREF(ColorPreviewWidget)
{
    bool rgbaMode       = false;
    Vector4f color      = Vector4f(0, 0, 0, 1.f);
    Vector2i dimensions = Vector2i(MNDATA_COLORBOX_WIDTH, MNDATA_COLORBOX_HEIGHT);  ///< Inner dimensions in fixed 320x200 space.
};

ColorPreviewWidget::ColorPreviewWidget(Vector4f const &color, bool rgbaMode)
    : Widget()
    , d(new Instance)
{
    Widget::_pageFontIdx  = MENU_FONT1;
    Widget::_pageColorIdx = MENU_COLOR1;

    d->rgbaMode = rgbaMode;
    d->color    = color;
    if(!d->rgbaMode) d->color.w = 1.f;
}

ColorPreviewWidget::~ColorPreviewWidget()
{}

void ColorPreviewWidget::draw(Point2Raw const *offset)
{
    DENG2_ASSERT(offset != 0);

    patchinfo_t t, b, l, r, tl, tr, br, bl;
    int const up = 1;

    R_GetPatchInfo(borderPatches[0], &t);
    R_GetPatchInfo(borderPatches[2], &b);
    R_GetPatchInfo(borderPatches[3], &l);
    R_GetPatchInfo(borderPatches[1], &r);
    R_GetPatchInfo(borderPatches[4], &tl);
    R_GetPatchInfo(borderPatches[5], &tr);
    R_GetPatchInfo(borderPatches[6], &br);
    R_GetPatchInfo(borderPatches[7], &bl);

    int x = offset->x;
    int y = offset->y;
    int w = d->dimensions.x;
    int h = d->dimensions.y;

    if(t.id || tl.id || tr.id)
    {
        int height = 0;
        if(t.id)  height = t.geometry.size.height;
        if(tl.id) height = de::max(height, tl.geometry.size.height);
        if(tr.id) height = de::max(height, tr.geometry.size.height);

        y += height;
    }

    if(l.id || tl.id || bl.id)
    {
        int width = 0;
        if(l.id)  width = l.geometry.size.width;
        if(tl.id) width = de::max(width, tl.geometry.size.width);
        if(bl.id) width = de::max(width, bl.geometry.size.width);

        x += width;
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_SetMaterialUI((Material *)P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString(borderGraphics[0])), DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectf2Tiled(x, y, w, h, 64, 64);

    // Top
    if(t.id)
    {
        DGL_SetPatch(t.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x, y - t.geometry.size.height, w, t.geometry.size.height, up * t.geometry.size.width, up * t.geometry.size.height);
    }

    // Bottom
    if(b.id)
    {
        DGL_SetPatch(b.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x, y + h, w, b.geometry.size.height, up * b.geometry.size.width, up * b.geometry.size.height);
    }

    // Left
    if(l.id)
    {
        DGL_SetPatch(l.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x - l.geometry.size.width, y, l.geometry.size.width, h, up * l.geometry.size.width, up * l.geometry.size.height);
    }

    // Right
    if(r.id)
    {
        DGL_SetPatch(r.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x + w, y, r.geometry.size.width, h, up * r.geometry.size.width, up * r.geometry.size.height);
    }

    // Top Left
    if(tl.id)
    {
        DGL_SetPatch(tl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x - tl.geometry.size.width, y - tl.geometry.size.height, tl.geometry.size.width, tl.geometry.size.height);
    }

    // Top Right
    if(tr.id)
    {
        DGL_SetPatch(tr.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + w, y - tr.geometry.size.height, tr.geometry.size.width, tr.geometry.size.height);
    }

    // Bottom Right
    if(br.id)
    {
        DGL_SetPatch(br.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + w, y + h, br.geometry.size.width, br.geometry.size.height);
    }

    // Bottom Left
    if(bl.id)
    {
        DGL_SetPatch(bl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x - bl.geometry.size.width, y + h, bl.geometry.size.width, bl.geometry.size.height);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(x, y, w, h, d->color.x, d->color.y, d->color.z, d->color.w * mnRendState->pageAlpha);
}

int ColorPreviewWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_SELECT:
        if(!(Widget::_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            Widget::_flags |= MNF_ACTIVE;
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            Widget::_flags &= ~MNF_ACTIVE;
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
        }
        return true;

    default: return false; // Not eaten.
    }
}

void ColorPreviewWidget::updateGeometry(Page * /*page*/)
{
    patchinfo_t info;

    Rect_SetWidthHeight(_geometry, d->dimensions.x, d->dimensions.y);

    // Add bottom border?
    if(R_GetPatchInfo(borderPatches[2], &info))
    {
        info.geometry.size.width = d->dimensions.x;
        info.geometry.origin.y   = d->dimensions.y;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add right border?
    if(R_GetPatchInfo(borderPatches[1], &info))
    {
        info.geometry.size.height = d->dimensions.y;
        info.geometry.origin.x    = d->dimensions.x;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top border?
    if(R_GetPatchInfo(borderPatches[0], &info))
    {
        info.geometry.size.width = d->dimensions.x;
        info.geometry.origin.y   = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add left border?
    if(R_GetPatchInfo(borderPatches[3], &info))
    {
        info.geometry.size.height = d->dimensions.y;
        info.geometry.origin.x    = -info.geometry.size.width;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top-left corner?
    if(R_GetPatchInfo(borderPatches[4], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add top-right corner?
    if(R_GetPatchInfo(borderPatches[5], &info))
    {
        info.geometry.origin.x = d->dimensions.x;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add bottom-right corner?
    if(R_GetPatchInfo(borderPatches[6], &info))
    {
        info.geometry.origin.x = d->dimensions.x;
        info.geometry.origin.y = d->dimensions.y;
        Rect_UniteRaw(_geometry, &info.geometry);
    }

    // Add bottom-left corner?
    if(R_GetPatchInfo(borderPatches[7], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = d->dimensions.y;
        Rect_UniteRaw(_geometry, &info.geometry);
    }
}

void ColorPreviewWidget::setPreviewDimensions(Vector2i const &newDimensions)
{
    d->dimensions = newDimensions;
}

Vector2i ColorPreviewWidget::previewDimensions() const
{
    return d->dimensions;
}

bool ColorPreviewWidget::rgbaMode() const
{
    return d->rgbaMode;
}

Vector4f ColorPreviewWidget::color() const
{
    if(!d->rgbaMode)
    {
        return Vector4f(d->color, 1.0f);
    }
    return d->color;
}

bool ColorPreviewWidget::setRed(float red, int flags)
{
    float const oldRed = d->color.x;

    d->color.x = red;
    if(d->color.x != oldRed)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }
        return true;
    }
    return false;
}

bool ColorPreviewWidget::setGreen(float green, int flags)
{
    float const oldGreen = d->color.y;

    d->color.y = green;
    if(d->color.y != oldGreen)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }
        return true;
    }
    return false;
}

bool ColorPreviewWidget::setBlue(float blue, int flags)
{
    float const oldBlue = d->color.z;

    d->color.z = blue;
    if(d->color.z != oldBlue)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
        {
            execAction(MNA_MODIFIED);
        }
        return true;
    }
    return false;
}

bool ColorPreviewWidget::setAlpha(float alpha, int flags)
{
    if(d->rgbaMode)
    {
        float const oldAlpha = d->color.w;
        d->color.w = alpha;
        if(d->color.w != oldAlpha)
        {
            if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
            return true;
        }
    }
    return false;
}

bool ColorPreviewWidget::setColor(Vector4f const &newColor, int flags)
{
    int setComps = 0;
    int const setCompFlags = (flags | MNCOLORBOX_SCF_NO_ACTION);

    if(setRed  (newColor.x, setCompFlags)) setComps |= 0x1;
    if(setGreen(newColor.y, setCompFlags)) setComps |= 0x2;
    if(setBlue (newColor.z, setCompFlags)) setComps |= 0x4;
    if(setAlpha(newColor.w, setCompFlags)) setComps |= 0x8;

    if(!setComps) return false;

    if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED);
    }
    return true;
}

} // namespace menu
} // namespace common
