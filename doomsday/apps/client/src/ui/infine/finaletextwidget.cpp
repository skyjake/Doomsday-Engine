/** @file finaletextwidget.cpp  InFine animation system, FinaleTextWidget.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_base.h"
#include "ui/infine/finaletextwidget.h"

#include <cstring> // memcpy, memmove
#include <de/legacy/concurrency.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/timer.h>
#include "api_fontrender.h"
#include "ui/infine/finalepagewidget.h"

#ifdef __CLIENT__
#  include "gl/gl_main.h"
#  include <de/glinfo.h>
#endif

using namespace de;

DE_PIMPL_NOREF(FinaleTextWidget)
{
    animatorvector4_t color;
    uint pageColor    = 0;                ///< 1-based page color index. @c 0= Use our own color.
    uint pageFont     = 0;                ///< 1-based page font index. @c 0= Use our own font.
    int alignFlags    = ALIGN_TOPLEFT;    ///< @ref alignmentFlags
    short textFlags   = DTF_ONLY_SHADOW;  ///< @ref drawTextFlags
    int scrollWait    = 0;
    int scrollTimer   = 0;                ///< Automatic scrolling upwards.
    int cursorPos     = 0;
    int wait          = 3;
    int timer         = 0;
    float lineHeight  = 11.f / 7 - 1;
    fontid_t fontNum  = 0;
    char *text        = nullptr;
    bool animComplete = true;

    Impl()  { AnimatorVector4_Init(color, 1, 1, 1, 1); }
    ~Impl() { Z_Free(text); }

#ifdef __CLIENT__
    static int textLineWidth(const char *text)
    {
        int width = 0;

        for (; *text; text++)
        {
            if (*text == '\n') break;
            if (*text == '\\')
            {
                if (!*++text)     break;
                if (*text == 'n') break;

                if (*text >= '0' && *text <= '9')
                    continue;
                if (*text == 'w' || *text == 'W' || *text == 'p' || *text == 'P')
                    continue;
            }
            width += FR_CharWidth(*text);
        }

        return width;
    }
#endif
};

FinaleTextWidget::FinaleTextWidget(const String &name)
    : FinaleWidget(name)
    , d(new Impl)
{}

FinaleTextWidget::~FinaleTextWidget()
{}

void FinaleTextWidget::accelerate()
{
    // Fill in the rest very quickly.
    d->wait = -10;
}

FinaleTextWidget &FinaleTextWidget::setCursorPos(int newPos)
{
    d->cursorPos = de::max(0, newPos);
    return *this;
}

void FinaleTextWidget::runTicks(/*timespan_t timeDelta*/)
{
    FinaleWidget::runTicks(/*timeDelta*/);

    AnimatorVector4_Think(d->color);

    if (d->wait)
    {
        if (--d->timer <= 0)
        {
            if (d->wait > 0)
            {
                // Positive wait: move cursor one position, wait again.
                d->cursorPos++;
                d->timer = d->wait;
            }
            else
            {
                // Negative wait: move cursor several positions, don't wait.
                d->cursorPos += ABS(d->wait);
                d->timer = 1;
            }
        }
    }

    if (d->scrollWait)
    {
        if (--d->scrollTimer <= 0)
        {
            d->scrollTimer = d->scrollWait;
            setOriginY(origin()[1].target - 1, d->scrollWait);
            //pos[1].target -= 1;
            //pos[1].steps = d->scrollWait;
        }
    }

    // Is the text object fully visible?
    d->animComplete = (!d->wait || d->cursorPos >= visLength());
}

#ifdef __CLIENT__
void FinaleTextWidget::draw(const Vec3f &offset)
{
    if (!d->text) return;

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    //glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);
    DGL_Translatef(origin()[0].value + offset.x, origin()[1].value + offset.y, origin()[2].value + offset.z);

    if (angle().value != 0)
    {
        // Counter the VGA aspect ratio.
        DGL_Scalef(1, 200.0f / 240.0f, 1);
        DGL_Rotatef(angle().value, 0, 0, 1);
        DGL_Scalef(1, 240.0f / 200.0f, 1);
    }

    DGL_Scalef(scale()[0].value, scale()[1].value, scale()[2].value);
    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(d->pageFont? page()->predefinedFont(d->pageFont - 1) : d->fontNum);

    // Set the normal color.
    const animatorvector3_t *color;
    if (d->pageColor == 0)
        color = (const animatorvector3_t *)&d->color;
    else
        color = page()->predefinedColor(d->pageColor - 1);
    FR_SetColor((*color)[CR].value, (*color)[CG].value, (*color)[CB].value);
    FR_SetAlpha(d->color[CA].value);

    int x = 0, y = 0, ch, linew = -1;
    char *ptr = d->text;
    for (int cnt = 0; *ptr && (!d->wait || cnt < d->cursorPos); ptr++)
    {
        if (linew < 0)
            linew = d->textLineWidth(ptr);

        ch = *ptr;
        if (*ptr == '\n') // Newline? (see below for unescaped \n)
        {
            x = 0;
            y += FR_CharHeight('A') * (1 + d->lineHeight);
            linew = -1;
            cnt++; // Include newlines in the wait count.
            continue;
        }

        if (*ptr == '\\') // Escape?
        {
            if (!*++ptr)
                break;

            // Change of color?
            if (*ptr >= '0' && *ptr <= '9')
            {
                uint colorIdx = *ptr - '0';
                if (colorIdx == 0)
                    color = (const animatorvector3_t *)&d->color;
                else
                    color = page()->predefinedColor(colorIdx - 1);
                FR_SetColor((*color)[CR].value, (*color)[CG].value, (*color)[CB].value);
                FR_SetAlpha(d->color[CA].value);
                continue;
            }

            // 'w' = half a second wait, 'W' = second'f wait
            if (*ptr == 'w' || *ptr == 'W') // Wait?
            {
                if (d->wait)
                    cnt += int(float(TICRATE) / d->wait / (*ptr == 'w' ? 2 : 1));
                continue;
            }

            // 'p' = 5 second wait, 'P' = 10 second wait
            if (*ptr == 'p' || *ptr == 'P') // Longer pause?
            {
                if (d->wait)
                    cnt += int(float(TICRATE) / d->wait * (*ptr == 'p' ? 5 : 10));
                continue;
            }

            if (*ptr == 'n' || *ptr == 'N') // Newline?
            {
                x = 0;
                y += FR_CharHeight('A') * (1 + d->lineHeight);
                linew = -1;
                cnt++; // Include newlines in the wait count.
                continue;
            }

            if (*ptr == '_')
                ch = ' ';
        }

        // Let'f do Y-clipping (in case of tall text blocks).
        if (scale()[1].value * y + origin()[1].value >= -scale()[1].value * d->lineHeight &&
           scale()[1].value * y + origin()[1].value < SCREENHEIGHT)
        {
            FR_DrawCharXY(ch, (d->alignFlags & ALIGN_LEFT) ? x : x - linew / 2, y);
            x += FR_CharWidth(ch);
        }

        ++cnt;
    }

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}
#endif

bool FinaleTextWidget::animationComplete() const
{
    return d->animComplete;
}

/// @todo optimize: Cache this result.
int FinaleTextWidget::visLength()
{
    int count = 0;
    if (d->text)
    {
        const float secondLen = (d->wait? TICRATE / d->wait : 0);

        for (const char *ptr = d->text; *ptr; ptr++)
        {
            if (*ptr == '\\') // Escape?
            {
                if (!*++ptr) break;

                switch (*ptr)
                {
                case 'w':   count += secondLen / 2;   break;
                case 'W':   count += secondLen;       break;
                case 'p':   count += 5 * secondLen;   break;
                case 'P':   count += 10 * secondLen;  break;

                default:
                    if ((*ptr >= '0' && *ptr <= '9') || *ptr == 'n' || *ptr == 'N')
                        continue;
                }
            }
            count++;
        }
    }
    return count;
}

const char *FinaleTextWidget::text() const
{
    return d->text;
}

FinaleTextWidget &FinaleTextWidget::setText(const char *newText)
{
    Z_Free(d->text); d->text = nullptr;
    if (newText && newText[0])
    {
        int len = (int)strlen(newText) + 1;
        d->text = (char *) Z_Malloc(len, PU_APPSTATIC, 0);
        std::memcpy(d->text, newText, len);
    }
    const int visLen = visLength();
    if (d->cursorPos > visLen)
    {
        d->cursorPos = visLen;
    }
    return *this;
}

fontid_t FinaleTextWidget::font() const
{
    return d->fontNum;
}

FinaleTextWidget &FinaleTextWidget::setFont(fontid_t newFont)
{
    d->fontNum  = newFont;
    d->pageFont = 0;
    return *this;
}

int FinaleTextWidget::alignment() const
{
    return d->alignFlags;
}

FinaleTextWidget &FinaleTextWidget::setAlignment(int newAlignment)
{
    d->alignFlags = newAlignment;
    return *this;
}

float FinaleTextWidget::lineHeight() const
{
    return d->lineHeight;
}

FinaleTextWidget &FinaleTextWidget::setLineHeight(float newLineHeight)
{
    d->lineHeight = newLineHeight;
    return *this;
}

int FinaleTextWidget::scrollRate() const
{
    return d->scrollWait;
}

FinaleTextWidget &FinaleTextWidget::setScrollRate(int newRateInTics)
{
    d->scrollWait  = newRateInTics;
    d->scrollTimer = 0;
    return *this;
}

int FinaleTextWidget::typeInRate() const
{
    return d->wait;
}

FinaleTextWidget &FinaleTextWidget::setTypeInRate(int newRateInTics)
{
    d->wait = newRateInTics;
    return *this;
}

FinaleTextWidget &FinaleTextWidget::setColor(const Vec3f &newColor, int steps)
{
    AnimatorVector3_Set(*((animatorvector3_t *)d->color), newColor.x, newColor.y, newColor.z, steps);
    d->pageColor = 0;
    return *this;
}

FinaleTextWidget &FinaleTextWidget::setAlpha(float alpha, int steps)
{
    Animator_Set(&d->color[CA], alpha, steps);
    return *this;
}

FinaleTextWidget &FinaleTextWidget::setColorAndAlpha(const Vec4f &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->color, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    d->pageColor = 0;
    return *this;
}

FinaleTextWidget &FinaleTextWidget::setPageColor(uint id)
{
    d->pageColor = id;
    return *this;
}

FinaleTextWidget &FinaleTextWidget::setPageFont(uint id)
{
    d->pageFont = id;
    return *this;
}
