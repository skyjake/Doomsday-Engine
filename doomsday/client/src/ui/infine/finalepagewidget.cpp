/** @file finalepagewidget.cpp  InFine animation system, FinalePageWidget.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/infine/finalepagewidget.h"

#include <de/vector1.h>
#include "dd_main.h" // App_ResourceSystem()
#include "Material"

#ifdef __CLIENT__
#  include "gl/gl_draw.h"
#  include "gl/gl_main.h"
#  include "render/rend_main.h" // renderWireframe
#  include "MaterialAnimator"
#endif

using namespace de;

DENG2_PIMPL_NOREF(FinalePageWidget)
, DENG2_OBSERVES(FinaleWidget, Deletion)
{
    struct Flags {
        char hidden:1;         ///< Not drawn.
        char paused:1;         ///< Does not tick.
        char showBackground:1;
    } flags;
    Children children;         ///< Child widgets (owned).
    uint timer = 0;

    animatorvector3_t offset;  ///< Offset the world origin.
    animatorvector4_t filter;
    animatorvector3_t preColor[FIPAGE_NUM_PREDEFINED_COLORS];
    fontid_t preFont[FIPAGE_NUM_PREDEFINED_FONTS];

    struct Background {
        Material *material;
        animatorvector4_t topColor;
        animatorvector4_t bottomColor;

        Background() : material(nullptr) {
            AnimatorVector4_Init(topColor,    1, 1, 1, 0);
            AnimatorVector4_Init(bottomColor, 1, 1, 1, 0);
        }
    } bg;

    Instance()
    {
        de::zap(flags);
        flags.showBackground = true; /// Draw background by default.

        AnimatorVector3_Init(offset, 0, 0, 0);
        AnimatorVector4_Init(filter, 0, 0, 0, 0);
        for(int i = 0; i < FIPAGE_NUM_PREDEFINED_COLORS; ++i)
        {
            AnimatorVector3_Init(preColor[i], 1, 1, 1);
        }
        de::zap(preFont);
    }

    ~Instance()
    {
        qDeleteAll(children);
        DENG2_ASSERT(children.isEmpty());
    }

    void finaleWidgetBeingDeleted(FinaleWidget const &widget)
    {
        DENG2_ASSERT(children.contains(&const_cast<FinaleWidget &>(widget)));
        children.removeOne(&const_cast<FinaleWidget &>(widget));
    }
};

FinalePageWidget::FinalePageWidget() : d(new Instance)
{}

FinalePageWidget::~FinalePageWidget()
{}

#ifdef __CLIENT__
static inline MaterialVariantSpec const &uiMaterialSpec()
{
    return App_ResourceSystem().materialSpec(UiContext, 0, 0, 0, 0,
                                             GL_REPEAT, GL_REPEAT, 0, 1, 0,
                                             false, false, false, false);
}

void FinalePageWidget::draw() const
{
    if(d->flags.hidden) return;

    // Draw a background?
    if(d->flags.showBackground)
    {
        vec3f_t topColor; V3f_Set(topColor, d->bg.topColor[0].value, d->bg.topColor[1].value, d->bg.topColor[2].value);
        float topAlpha = d->bg.topColor[3].value;

        vec3f_t bottomColor; V3f_Set(bottomColor, d->bg.bottomColor[0].value, d->bg.bottomColor[1].value, d->bg.bottomColor[2].value);
        float bottomAlpha = d->bg.bottomColor[3].value;

        if(topAlpha > 0 && bottomAlpha > 0)
        {
            if(Material *material = d->bg.material)
            {
                MaterialAnimator &matAnimator = material->getAnimator(uiMaterialSpec());

                // Ensure we've up to date info about the material.
                matAnimator.prepare();

                GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
                glEnable(GL_TEXTURE_2D);
            }

            if(d->bg.material || topAlpha < 1.0 || bottomAlpha < 1.0)
            {
                GL_BlendMode(BM_NORMAL);
            }
            else
            {
                glDisable(GL_BLEND);
            }

            GL_DrawRectf2TextureColor(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64,
                                      topColor, topAlpha, bottomColor, bottomAlpha);

            GL_SetNoTexture();
            glEnable(GL_BLEND);
        }
    }

    // Now lets go into 3D mode for drawing the p objects.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    //glLoadIdentity();

    GL_SetMultisample(true);

    // Clear Z buffer (prevent the objects being clipped by nearby polygons).
    glClear(GL_DEPTH_BUFFER_BIT);

    if(renderWireframe > 1)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_ALPHA_TEST);

    Vector3f worldOrigin(/*-SCREENWIDTH/2*/ - d->offset[VX].value,
                         /*-SCREENHEIGHT/2*/ - d->offset[VY].value,
                         0/*.05f - d->offset[VZ].value*/);

    for(FinaleWidget *widget : d->children)
    {
        widget->draw(worldOrigin);
    }

    // Restore original matrices and state: back to normal 2D.
    glDisable(GL_ALPHA_TEST);
    // Back from wireframe mode?
    if(renderWireframe > 1)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Filter on top of everything. Only draw if necessary.
    if(d->filter[3].value > 0)
    {
        GL_DrawRectf2Color(0, 0, SCREENWIDTH, SCREENHEIGHT, d->filter[0].value, d->filter[1].value, d->filter[2].value, d->filter[3].value);
    }

    GL_SetMultisample(false);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
#endif

void FinalePageWidget::makeVisible(bool yes)
{
    d->flags.hidden = !yes;
}

void FinalePageWidget::pause(bool yes)
{
    d->flags.paused = yes;
}

void FinalePageWidget::runTicks(timespan_t /*timeDelta*/)
{
    /// @todo Interpolate in fractional time.
    if(!DD_IsSharpTick()) return;

    // A new 'sharp' tick has begun.
    d->timer++;

    for(FinaleWidget *widget : d->children)
    {
        widget->runTicks(/*timeDelta*/);
    }

    AnimatorVector3_Think(d->offset);
    AnimatorVector4_Think(d->bg.topColor);
    AnimatorVector4_Think(d->bg.bottomColor);
    AnimatorVector4_Think(d->filter);
    for(int i = 0; i < FIPAGE_NUM_PREDEFINED_COLORS; ++i)
    {
        AnimatorVector3_Think(d->preColor[i]);
    }
}

bool FinalePageWidget::hasWidget(FinaleWidget *widget)
{
    if(!widget) return false;
    return d->children.contains(widget);
}

FinaleWidget *FinalePageWidget::addChild(FinaleWidget *widgetToAdd)
{
    if(!hasWidget(widgetToAdd))
    {
        d->children.append(widgetToAdd);
        widgetToAdd->setPage(this);
        widgetToAdd->audienceForDeletion += d;
    }
    return widgetToAdd;
}

FinaleWidget *FinalePageWidget::removeChild(FinaleWidget *widgetToRemove)
{
    if(hasWidget(widgetToRemove))
    {
        widgetToRemove->audienceForDeletion -= d;
        widgetToRemove->setPage(nullptr);
        d->children.removeOne(widgetToRemove);
    }
    return widgetToRemove;
}

FinalePageWidget::Children const &FinalePageWidget::children() const
{
    return d->children;
}

Material *FinalePageWidget::backgroundMaterial() const
{
    return d->bg.material;
}

FinalePageWidget &FinalePageWidget::setBackgroundMaterial(Material *newMaterial)
{
    d->bg.material = newMaterial;
    return *this;
}

FinalePageWidget &FinalePageWidget::setBackgroundTopColor(Vector3f const &newColor, int steps)
{
    AnimatorVector3_Set(d->bg.topColor, newColor.x, newColor.y, newColor.z, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setBackgroundTopColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->bg.topColor, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setBackgroundBottomColor(Vector3f const &newColor, int steps)
{
    AnimatorVector3_Set(d->bg.bottomColor, newColor.x, newColor.y, newColor.z, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setBackgroundBottomColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->bg.bottomColor, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setOffset(Vector3f const &newOffset, int steps)
{
    AnimatorVector3_Set(d->offset, newOffset.x, newOffset.y, newOffset.z, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setOffsetX(float x, int steps)
{
    Animator_Set(&d->offset[VX], x, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setOffsetY(float y, int steps)
{
    Animator_Set(&d->offset[VY], y, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setOffsetZ(float y, int steps)
{
    Animator_Set(&d->offset[VZ], y, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setFilterColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->filter, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

FinalePageWidget &FinalePageWidget::setPredefinedColor(uint idx, Vector3f const &newColor, int steps)
{
    if(VALID_FIPAGE_PREDEFINED_COLOR(idx))
    {
        AnimatorVector3_Set(d->preColor[idx], newColor.x, newColor.y, newColor.z, steps);
    }
    else
    {
        throw InvalidColorError("FinalePageWidget::setPredefinedColor", "Invalid color #" + String::number(idx));
    }
    return *this;
}

animatorvector3_t const *FinalePageWidget::predefinedColor(uint idx)
{
    if(VALID_FIPAGE_PREDEFINED_COLOR(idx))
    {
        return &d->preColor[idx];
    }
    throw InvalidColorError("FinalePageWidget::predefinedColor", "Invalid color #" + String::number(idx));
}

FinalePageWidget &FinalePageWidget::setPredefinedFont(uint idx, fontid_t fontNum)
{
    if(VALID_FIPAGE_PREDEFINED_FONT(idx))
    {
        d->preFont[idx] = fontNum;
    }
    else
    {
        throw InvalidFontError("FinalePageWidget::setPredefinedFont", "Invalid font #" + String::number(idx));
    }
    return *this;
}

fontid_t FinalePageWidget::predefinedFont(uint idx)
{
    if(VALID_FIPAGE_PREDEFINED_FONT(idx))
    {
        return d->preFont[idx];
    }
    throw InvalidFontError("FinalePageWidget::predefinedFont", "Invalid font #" + String::number(idx));
}
