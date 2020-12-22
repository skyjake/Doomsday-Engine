/** @file sliderwidget.cpp  UI widget for a graphical slider.
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
#include "menu/widgets/sliderwidget.h"

#include "hu_menu.h" // menu sounds
#include "hu_stuff.h" // M_DrawGlowBar, etc..
#include "menu/page.h" // mnRendState

using namespace de;

namespace common {
namespace menu {

static patchid_t pSliderLeft;
static patchid_t pSliderRight;
static patchid_t pSliderMiddle;
static patchid_t pSliderHandle;

DE_PIMPL(SliderWidget)
{
    float min      = 0.0f;
    float max      = 1.0f;
    float value    = 0.0f;
    float step     = 0.1f;  ///< Button step.
    bool floatMode = true;  ///< @c false= only integers are allowed.

    Impl(Public *i) : Base(i) {}

    int thumbPos() const
    {
#define WIDTH           (middleInfo.geometry.size.width)

        patchinfo_t middleInfo;
        if(!R_GetPatchInfo(pSliderMiddle, &middleInfo)) return 0;

        float range = max - min;
        if(!range) range = 1; // Should never happen...

        float useVal = self().value() - min;
        return useVal / range * MNDATA_SLIDER_SLOTS * WIDTH;

#undef WIDTH
    }
};

SliderWidget::SliderWidget(float min, float max, float step, bool floatMode)
    : Widget()
    , d(new Impl(this))
{
    setFont(MENU_FONT1);
    setColor(MENU_COLOR1);

    d->min       = min;
    d->max       = max;
    d->step      = step;
    d->floatMode = floatMode;
}

SliderWidget::~SliderWidget()
{}

void SliderWidget::loadResources() // static
{
    pSliderLeft   = R_DeclarePatch(MNDATA_SLIDER_PATCH_LEFT);
    pSliderRight  = R_DeclarePatch(MNDATA_SLIDER_PATCH_RIGHT);
    pSliderMiddle = R_DeclarePatch(MNDATA_SLIDER_PATCH_MIDDLE);
    pSliderHandle = R_DeclarePatch(MNDATA_SLIDER_PATCH_HANDLE);
}

SliderWidget &SliderWidget::setValue(float newValue, int /*flags*/)
{
    if(d->floatMode) d->value = newValue;
    else             d->value = int( newValue + (newValue > 0? + .5f : -.5f) );
    return *this;
}

float SliderWidget::value() const
{
    if(d->floatMode)
    {
        return d->value;
    }
    return int( d->value + (d->value > 0? .5f : -.5f) );
}

SliderWidget &SliderWidget::setRange(float newMin, float newMax, float newStep)
{
    d->min  = newMin;
    d->max  = newMax;
    d->step = newStep;
    return *this;
}

float SliderWidget::min() const
{
    return d->min;
}

float SliderWidget::max() const
{
    return d->max;
}

float SliderWidget::step() const
{
    return d->step;
}

SliderWidget &SliderWidget::setFloatMode(bool yes)
{
    d->floatMode = yes;
    return *this;
}

bool SliderWidget::floatMode() const
{
    return d->floatMode;
}

void SliderWidget::draw() const
{
#define WIDTH                   (middleInfo.geometry.size.width)
#define HEIGHT                  (middleInfo.geometry.size.height)

    patchinfo_t middleInfo, leftInfo;

    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo)) return;
    if(!R_GetPatchInfo(pSliderLeft, &leftInfo)) return;
    if(WIDTH <= 0 || HEIGHT <= 0) return;

    const float fadeout = scrollingFadeout();
    if (fadeout < .001f) return;

    Vec2f origin = geometry().topLeft;
    origin += Vec2f(MNDATA_SLIDER_OFFSET_X + leftInfo.geometry.size.width, MNDATA_SLIDER_OFFSET_Y) * MNDATA_SLIDER_SCALE;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(origin.x, origin.y, 0);
    DGL_Scalef(MNDATA_SLIDER_SCALE, MNDATA_SLIDER_SCALE, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    if(cfg.common.menuShadow > 0)
    {
        float const from[] = { 2, float(1 + HEIGHT / 2) };
        float const to[]   = { float(MNDATA_SLIDER_SLOTS * WIDTH - 2), float(1 + HEIGHT / 2) };
        M_DrawGlowBar(from, to, HEIGHT * 1.1f, true, true, true, 0, 0, 0, mnRendState->pageAlpha * mnRendState->textShadow);
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha * fadeout);

    GL_DrawPatch(pSliderLeft,  Vec2i(0, 0), ALIGN_TOPRIGHT, DPF_NO_OFFSETX);
    GL_DrawPatch(pSliderRight, Vec2i(MNDATA_SLIDER_SLOTS * WIDTH, 0));

    DGL_SetPatch(pSliderMiddle, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectf2Tiled(0, middleInfo.geometry.origin.y, MNDATA_SLIDER_SLOTS * WIDTH, HEIGHT, middleInfo.geometry.size.width, middleInfo.geometry.size.height);

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha * fadeout);
    GL_DrawPatch(pSliderHandle, {d->thumbPos(), 1}, ALIGN_TOP, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef HEIGHT
#undef WIDTH
}

int SliderWidget::handleCommand(menucommand_e cmd)
{
    switch(cmd)
    {
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        const float oldvalue = d->value;

        if(MCMD_NAV_LEFT == cmd)
        {
            d->value -= d->step;
            if(d->value < d->min)
                d->value = d->min;
        }
        else
        {
            d->value += d->step;
            if(d->value > d->max)
                d->value = d->max;
        }

        // Did the value change?
        if(oldvalue != d->value)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            execAction(Modified);
        }
        return true; }

    default: break;
    }

    return false; // Not eaten.
}

void SliderWidget::updateGeometry()
{
    patchinfo_t info;
    if(!R_GetPatchInfo(pSliderMiddle, &info)) return;

    int middleWidth = info.geometry.size.width * MNDATA_SLIDER_SLOTS;
    geometry().setSize(Vec2ui(middleWidth, info.geometry.size.height));

    if(R_GetPatchInfo(pSliderLeft, &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        geometry() |= Rectanglei::fromSize(Vec2i(info.geometry.origin.xy), Vec2ui(info.geometry.size.width, info.geometry.size.height));
    }

    if(R_GetPatchInfo(pSliderRight, &info))
    {
        info.geometry.origin.x += middleWidth;
        geometry() |= Rectanglei::fromSize(Vec2i(info.geometry.origin.xy), Vec2ui(info.geometry.size.width, info.geometry.size.height));
    }

    geometry().setSize(Vec2ui(.5f + geometry().width()  * MNDATA_SLIDER_SCALE,
                                 .5f + geometry().height() * MNDATA_SLIDER_SCALE));
}

} // namespace menu
} // namespace common
