/** @file lineeditwidget.cpp  UI widget for an editable line of text.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "menu/widgets/lineeditwidget.h"

#include "hu_menu.h"   // menu sounds
#include "hu_stuff.h"  // shiftXForm
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

static patchid_t pEditLeft;
static patchid_t pEditRight;
static patchid_t pEditMiddle;

DE_PIMPL_NOREF(LineEditWidget)
{
    String text;
    String oldText;    ///< For restoring a canceled edit.
    String emptyText;  ///< Used when value is empty.
    dsize  maxLength       = 0;
    int    maxVisibleChars = 0;
};

LineEditWidget::LineEditWidget()
    : Widget()
    , d(new Impl)
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);
}

LineEditWidget::~LineEditWidget()
{}

void LineEditWidget::loadResources() // static
{
#if defined(MNDATA_EDIT_BACKGROUND_PATCH_LEFT)
    pEditLeft   = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_LEFT);
#else
    pEditLeft   = 0;
#endif
#if defined(MNDATA_EDIT_BACKGROUND_PATCH_RIGHT)
    pEditRight  = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_RIGHT);
#else
    pEditRight  = 0;
#endif
    pEditMiddle = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE);
}

static void drawEditBackground(const Vec2i &origin, int width, float alpha)
{
    DGL_Color4f(1, 1, 1, alpha);

    int leftOffset = 0;
    patchinfo_t leftInfo;
    if(R_GetPatchInfo(pEditLeft, &leftInfo))
    {
        DGL_SetPatch(pEditLeft, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(origin.x, origin.y, leftInfo.geometry.size.width, leftInfo.geometry.size.height);
        leftOffset = leftInfo.geometry.size.width;
    }

    int rightOffset = 0;
    patchinfo_t rightInfo;
    if(R_GetPatchInfo(pEditRight, &rightInfo))
    {
        DGL_SetPatch(pEditRight, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(origin.x + width - rightInfo.geometry.size.width, origin.y, rightInfo.geometry.size.width, rightInfo.geometry.size.height);
        rightOffset = rightInfo.geometry.size.width;
    }

    patchinfo_t middleInfo;
    if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        if (!pEditLeft && !pEditRight)
        {
            // Stretch the middle patch to the desired width.
            DGL_SetPatch(pEditMiddle, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
            DGL_DrawRectf2(origin.x, origin.y,
                           width, middleInfo.geometry.size.height);
        }
        else
        {
            DGL_SetPatch(pEditMiddle, DGL_REPEAT, DGL_REPEAT);
            DGL_DrawRectf2Tiled(origin.x + leftOffset, origin.y, width - leftOffset - rightOffset, middleInfo.geometry.size.height, middleInfo.geometry.size.width, middleInfo.geometry.size.height);
        }
    }
}

void LineEditWidget::draw() const
{
    fontid_t fontId = mnRendState->textFonts[font()];

    Vec2i origin = geometry().topLeft + Vec2i(MNDATA_EDIT_OFFSET_X, MNDATA_EDIT_OFFSET_Y);

    String useText;
    float light = 1, textOpacity = mnRendState->pageAlpha;
    if(!d->text.isEmpty())
    {
        useText = d->text;
    }
    else if(!(isActive() && isFocused()))
    {
        useText = d->emptyText;
        light *= .5f;
        textOpacity = mnRendState->pageAlpha * .75f;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(fontId);

    const float fadeout = scrollingFadeout();

    //const int numVisCharacters = de::clamp(0, useText.isNull()? 0 : useText.length(), d->maxVisibleChars);

    drawEditBackground(origin + Vec2i(MNDATA_EDIT_BACKGROUND_OFFSET_X, MNDATA_EDIT_BACKGROUND_OFFSET_Y),
                       geometry().width(), mnRendState->pageAlpha * fadeout);

    //if(string)
    {
//        float t = 0;
        Vec4f color = Vec4f(Vec3f(cfg.common.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX]), 1.f);

        // Flash if focused?
        if (!isActive()) /* && isFocused() && cfg.common.menuTextFlashSpeed > 0)
        {
            const float speed = cfg.common.menuTextFlashSpeed / 2.f;
            t = (1 + sin(page().timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
        }*/
        {
            color = selectionFlashColor(color);
        }

//        Vector4f color = de::lerp(Vector3f(cfg.common.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX]), Vector3f(cfg.common.menuTextFlashColor), t);
        color *= light;
        color.w = textOpacity;

        // Draw the text:
        FR_SetColorAndAlpha(color.x, color.y, color.z, color.w * fadeout);
        FR_DrawTextXY3(useText, origin.x, origin.y, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

        // Are we drawing a cursor?
        if (isActive() && isFocused() && (menuTime & 8) &&
            (!d->maxLength || d->text.length() < d->maxLength))
        {
            origin.x += FR_TextWidth(useText);
            FR_DrawCharXY3('_', origin.x, origin.y, ALIGN_TOPLEFT,  Hu_MenuMergeEffectWithDrawTextFlags(0));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int LineEditWidget::maxLength() const
{
    return int(d->maxLength);
}

LineEditWidget &LineEditWidget::setMaxLength(int newMaxLength)
{
    newMaxLength = de::max(newMaxLength, 0);
    if (d->maxLength != dsize(newMaxLength))
    {
        if (dsize(newMaxLength) < d->maxLength)
        {
            d->text.truncate(CharPos(newMaxLength));
            d->oldText.truncate(CharPos(newMaxLength));
        }
        d->maxLength = newMaxLength;
    }
    return *this;
}

String LineEditWidget::text() const
{
    return d->text;
}

LineEditWidget &LineEditWidget::setText(const String &newText, int flags)
{
    d->text = newText;
    if(d->maxLength) d->text.truncate(CharPos(d->maxLength));

    if(flags & MNEDIT_STF_REPLACEOLD)
    {
        d->oldText = d->text;
    }

    if(!(flags & MNEDIT_STF_NO_ACTION))
    {
        execAction(Modified);
    }
    return *this;
}

LineEditWidget &LineEditWidget::setEmptyText(const String &newEmptyText)
{
    d->emptyText = newEmptyText;
    return *this;
}

String LineEditWidget::emptyText() const
{
    return d->emptyText;
}

/**
 * Responds to alphanumeric input for edit fields.
 */
int LineEditWidget::handleEvent(const event_t &ev)
{
    if(!isActive() || ev.type != EV_KEY)
        return false;

    if(ev.data1 == DDKEY_RSHIFT)
    {
        shiftdown = (ev.state == EVS_DOWN || ev.state == EVS_REPEAT);
        return true;
    }

    if(!(ev.state == EVS_DOWN || ev.state == EVS_REPEAT))
        return false;

    if(ev.data1 == DDKEY_BACKSPACE)
    {
        if(!d->text.isEmpty())
        {
            d->text.truncate(CharPos(d->text.length() - 1));
            execAction(Modified);
        }
        return true;
    }

    if(ev.data1 >= ' ' && ev.data1 <= 'z')
    {
        char ch = char(ev.data1);
        if(shiftdown)
        {
            ch = shiftXForm[int(ch)];
        }

        // Filter out nasty charactemnRendState->
        if(ch == '%')
            return true;

        if(!d->maxLength || d->text.length() < dsize(d->maxLength))
        {
            d->text += ch;
            execAction(Modified);
        }
        return true;
    }

    return false;
}

int LineEditWidget::handleCommand(menucommand_e cmd)
{
    if(cmd == MCMD_SELECT)
    {
        if(!isActive())
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            setFlags(Active);
            // Store a copy of the present text value so we can restore it.
            d->oldText = d->text;
            execAction(Activated);
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            d->oldText = d->text;
            setFlags(Active, UnsetFlags);
            execAction(Deactivated);
        }
        return true;
    }

    if(isActive())
    {
        switch(cmd)
        {
        case MCMD_NAV_OUT:
            d->text = d->oldText;
            setFlags(Active, UnsetFlags);
            execAction(Closed);
            return true;

        // Eat all other navigation commands, when active.
        case MCMD_NAV_LEFT:
        case MCMD_NAV_RIGHT:
        case MCMD_NAV_DOWN:
        case MCMD_NAV_UP:
        case MCMD_NAV_PAGEDOWN:
        case MCMD_NAV_PAGEUP:
            return true;

        default: break;
        }
    }

    return false; // Not eaten.
}

void LineEditWidget::updateGeometry()
{
    FR_SetFont(mnRendState->textFonts[font()]);
    geometry().setSize(Vec2ui(FR_CharWidth('w') * d->maxLength - MNDATA_EDIT_BACKGROUND_OFFSET_X*2, 14));
}

} // namespace menu
} // namespace common
