/** @file lineeditwidget.cpp  UI widget for an editable line of text.
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
#include "menu/widgets/lineeditwidget.h"

#include "hu_menu.h" // menu sounds
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

static patchid_t pEditLeft;
static patchid_t pEditRight;
static patchid_t pEditMiddle;

DENG2_PIMPL_NOREF(LineEditWidget)
{
    String text;
    String oldText;    ///< For restoring a canceled edit.
    String emptyText;  ///< Used when value is empty.
    int maxLength       = 0;
    int maxVisibleChars = 0;
};

LineEditWidget::LineEditWidget()
    : Widget()
    , d(new Instance)
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

static void drawEditBackground(Widget const *wi, int x, int y, int width, float alpha)
{
    DENG2_UNUSED(wi);

    patchinfo_t leftInfo, rightInfo, middleInfo;
    int leftOffset = 0, rightOffset = 0;

    DGL_Color4f(1, 1, 1, alpha);

    if(R_GetPatchInfo(pEditLeft, &leftInfo))
    {
        DGL_SetPatch(pEditLeft, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x, y, leftInfo.geometry.size.width, leftInfo.geometry.size.height);
        leftOffset = leftInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditRight, &rightInfo))
    {
        DGL_SetPatch(pEditRight, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + width - rightInfo.geometry.size.width, y, rightInfo.geometry.size.width, rightInfo.geometry.size.height);
        rightOffset = rightInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        DGL_SetPatch(pEditMiddle, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x + leftOffset, y, width - leftOffset - rightOffset, middleInfo.geometry.size.height, middleInfo.geometry.size.width, middleInfo.geometry.size.height);
    }
}

void LineEditWidget::draw(Point2Raw const *_origin)
{
    DENG2_ASSERT(_origin != 0);

    fontid_t fontId = mnRendState->textFonts[font()];

    Point2Raw origin(_origin->x + MNDATA_EDIT_OFFSET_X, _origin->y + MNDATA_EDIT_OFFSET_Y);

    String useText;
    float light = 1, textAlpha = mnRendState->pageAlpha;
    if(!d->text.isEmpty())
    {
        useText = d->text;
    }
    else if(!((flags() & Active) && (flags() & Focused)))
    {
        useText = d->emptyText;
        light *= .5f;
        textAlpha = mnRendState->pageAlpha * .75f;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(fontId);

    //int const numVisCharacters = de::clamp(0, useText.isNull()? 0 : useText.length(), d->maxVisibleChars);

    drawEditBackground(this, origin.x + MNDATA_EDIT_BACKGROUND_OFFSET_X,
                             origin.y + MNDATA_EDIT_BACKGROUND_OFFSET_Y,
                       Rect_Width(geometry()), mnRendState->pageAlpha);

    //if(string)
    {
        float textColor[4], t = 0;

        // Flash if focused?
        if(!(flags() & Active) && (flags() & Focused) && cfg.menuTextFlashSpeed > 0)
        {
            float const speed = cfg.menuTextFlashSpeed / 2.f;
            t = (1 + sin(page().timer() / (float)TICSPERSEC * speed * DD_PI)) / 2;
        }

        lerpColor(textColor, cfg.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX], cfg.menuTextFlashColor, t, false/*rgb mode*/);
        textColor[CA] = textAlpha;

        // Light the text.
        textColor[CR] *= light; textColor[CG] *= light; textColor[CB] *= light;

        // Draw the text:
        FR_SetColorAndAlphav(textColor);
        FR_DrawText3(useText.toUtf8().constData(), &origin, ALIGN_TOPLEFT, Hu_MenuMergeEffectWithDrawTextFlags(0));

        // Are we drawing a cursor?
        if((flags() & Active) && (flags() & Focused) && (menuTime & 8) &&
           (!d->maxLength || d->text.length() < d->maxLength))
        {
            origin.x += FR_TextWidth(useText.toUtf8().constData());
            FR_DrawChar3('_', &origin, ALIGN_TOPLEFT,  Hu_MenuMergeEffectWithDrawTextFlags(0));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int LineEditWidget::maxLength() const
{
    return d->maxLength;
}

LineEditWidget &LineEditWidget::setMaxLength(int newMaxLength)
{
    newMaxLength = de::max(newMaxLength, 0);
    if(d->maxLength != newMaxLength)
    {
        if(newMaxLength < d->maxLength)
        {
            d->text.truncate(newMaxLength);
            d->oldText.truncate(newMaxLength);
        }
        d->maxLength = newMaxLength;
    }
    return *this;
}

String LineEditWidget::text() const
{
    return d->text;
}

LineEditWidget &LineEditWidget::setText(String const &newText, int flags)
{
    d->text = newText;
    if(d->maxLength) d->text.truncate(d->maxLength);

    if(flags & MNEDIT_STF_REPLACEOLD)
    {
        d->oldText = d->text;
    }

    if(!(flags & MNEDIT_STF_NO_ACTION) && hasAction(MNA_MODIFIED))
    {
        execAction(MNA_MODIFIED);
    }
    return *this;
}

LineEditWidget &LineEditWidget::setEmptyText(String const &newEmptyText)
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
int LineEditWidget::handleEvent(event_t *ev)
{
    DENG2_ASSERT(ev != 0);

    if(!isActive() || ev->type != EV_KEY)
        return false;

    if(DDKEY_RSHIFT == ev->data1)
    {
        shiftdown = (EVS_DOWN == ev->state || EVS_REPEAT == ev->state);
        return true;
    }

    if(!(EVS_DOWN == ev->state || EVS_REPEAT == ev->state))
        return false;

    if(DDKEY_BACKSPACE == ev->data1)
    {
        if(!d->text.isEmpty())
        {
            d->text.truncate(d->text.length() - 1);
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
        }
        return true;
    }

    if(ev->data1 >= ' ' && ev->data1 <= 'z')
    {
        char ch = char(ev->data1);
        if(shiftdown)
        {
            ch = shiftXForm[ch];
        }

        // Filter out nasty charactemnRendState->
        if(ch == '%')
            return true;

        if(!d->maxLength || d->text.length() < d->maxLength)
        {
            d->text += ch;
            if(hasAction(MNA_MODIFIED))
            {
                execAction(MNA_MODIFIED);
            }
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
            if(hasAction(MNA_ACTIVE))
            {
                execAction(MNA_ACTIVE);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            d->oldText = d->text;
            setFlags(Active, UnsetFlags);
            if(hasAction(MNA_ACTIVEOUT))
            {
                execAction(MNA_ACTIVEOUT);
            }
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
            if(hasAction(MNA_CLOSE))
            {
                execAction(MNA_CLOSE);
            }
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

void LineEditWidget::updateGeometry(Page * /*page*/)
{
    // @todo calculate visible dimensions properly.
    Rect_SetWidthHeight(geometry(), 170, 14);
}

} // namespace menu
} // namespace common
