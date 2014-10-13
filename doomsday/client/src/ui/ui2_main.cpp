/** @file ui2_main.cpp  InFine animation system widgets.
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

#include <cstring> // memcpy, memmove
#include <QtAlgorithms>
#include <de/concurrency.h>
#include <de/memoryzone.h>
#include <de/timer.h>
#include <de/vector1.h>
#include <de/Log>

#include "de_base.h"
#include "de_audio.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_resource.h"
#include "de_ui.h"

#include "ui/dd_ui.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "gl/gl_texmanager.h"
#  include "gl/sys_opengl.h" // TODO: get rid of this
#  include "ui/clientwindow.h"
#endif

using namespace de;

static bool inited;

/// Global widget store.
static QList<FinalePageWidget *> pages;
static QList<FinaleWidget *> widgets;

static FinaleWidget *findWidget(Id const &id)
{
    if(!id.isNone())
    {
        for(FinaleWidget *wi : widgets)
        {
            if(wi->id() == id) return wi;
        }
    }
    return nullptr;
}

void UI_Init()
{
    // Already been here?
    if(inited) return;

    inited = true;
}

void UI_Shutdown()
{
    if(!inited) return;

    // Garbage collection.
    qDeleteAll(widgets); widgets.clear();
    qDeleteAll(pages); pages.clear();

    inited = false;
}

int UI_PageCount()
{
    if(!inited) return 0;
    return pages.count();
}

void UI2_Ticker(timespan_t ticLength)
{
#ifdef __CLIENT__
    // Always tic.
    FR_Ticker(ticLength);
#endif

    if(!inited) return;

    // All pages tick unless paused.
    for(FinalePageWidget *page : pages) page->runTicks(ticLength);
}

FinaleWidget *FI_Widget(Id const &id)
{
    if(!inited)
    {
        LOG_AS("FI_Widget")
        LOGDEV_SCR_WARNING("Widget %i not initialized yet!") << id;
        return 0;
    }
    return findWidget(id);
}

FinaleWidget *FI_Link(FinaleWidget *widgetToLink)
{
    if(widgetToLink)
    {
        widgets.append(widgetToLink);
    }
    return widgetToLink;
}

FinaleWidget *FI_Unlink(FinaleWidget *widgetToUnlink)
{
    if(widgetToUnlink)
    {
        widgets.removeOne(widgetToUnlink);
    }
    return widgetToUnlink;
}

FinalePageWidget *FI_CreatePageWidget()
{
    pages.append(new FinalePageWidget);
    return pages.last();
}

void FI_DestroyPageWidget(FinalePageWidget *widget)
{
    if(!widget) return;
    pages.removeOne(widget);
    delete widget;
}

// -------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(FinaleWidget)
{
    Id id;
    String name;
    animatorvector3_t pos;
    animator_t angle;
    animatorvector3_t scale;
    FinalePageWidget *page = nullptr;

    Instance()
    {
        AnimatorVector3_Init(pos, 0, 0, 0);
        Animator_Init(&angle, 0);
        AnimatorVector3_Init(scale, 1, 1, 1);
    }
};

FinaleWidget::FinaleWidget(de::String const &name) : d(new Instance)
{
    setName(name);
}

FinaleWidget::~FinaleWidget()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->finaleWidgetBeingDeleted(*this);
}

Id FinaleWidget::id() const
{
    return d->id;
}

String FinaleWidget::name() const
{
    return d->name;
}

FinaleWidget &FinaleWidget::setName(String const &newName)
{
    d->name = newName;
    return *this;
}

animatorvector3_t const &FinaleWidget::origin() const
{
    return d->pos;
}

FinaleWidget &FinaleWidget::setOrigin(Vector3f const &newPos, int steps)
{
    AnimatorVector3_Set(d->pos, newPos.x, newPos.y, newPos.z, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setOriginX(float newX, int steps)
{
    Animator_Set(&d->pos[0], newX, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setOriginY(float newY, int steps)
{
    Animator_Set(&d->pos[1], newY, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setOriginZ(float newZ, int steps)
{
    Animator_Set(&d->pos[2], newZ, steps);
    return *this;
}

animator_t const &FinaleWidget::angle() const
{
    return d->angle;
}

FinaleWidget &FinaleWidget::setAngle(float newAngle, int steps)
{
    Animator_Set(&d->angle, newAngle, steps);
    return *this;
}

animatorvector3_t const &FinaleWidget::scale() const
{
    return d->scale;
}

FinaleWidget &FinaleWidget::setScale(Vector3f const &newScale, int steps)
{
    AnimatorVector3_Set(d->scale, newScale.x, newScale.y, newScale.z, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setScaleX(float newXScale, int steps)
{
    Animator_Set(&d->scale[0], newXScale, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setScaleY(float newYScale, int steps)
{
    Animator_Set(&d->scale[1], newYScale, steps);
    return *this;
}

FinaleWidget &FinaleWidget::setScaleZ(float newZScale, int steps)
{
    Animator_Set(&d->scale[2], newZScale, steps);
    return *this;
}

FinalePageWidget *FinaleWidget::page() const
{
    return d->page;
}

FinaleWidget &FinaleWidget::setPage(FinalePageWidget *newPage)
{
    d->page = newPage;
    return *this;
}

void FinaleWidget::runTicks(/*timespan_t timeDelta*/)
{
    AnimatorVector3_Think(d->pos);
    AnimatorVector3_Think(d->scale);
    Animator_Think(&d->angle);
}

// -------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(FinalePageWidget)
, DENG2_OBSERVES(FinaleWidget, Deletion)
{
    struct Flags {
        char hidden:1;         ///< Not drawn.
        char paused:1;         ///< Does not tick.
        char showBackground:1;
    } flags;
    Widgets widgets;           ///< Child widgets (@em not owned).
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

    void finaleWidgetBeingDeleted(FinaleWidget const &widget)
    {
        widgets.removeOne(&const_cast<FinaleWidget &>(widget));
    }
};

FinalePageWidget::FinalePageWidget() : d(new Instance)
{}

FinalePageWidget::~FinalePageWidget()
{}

#ifdef __CLIENT__
static void useColor(animator_t const *color, int components)
{
    if(components == 3)
    {
        glColor3f(color[0].value, color[1].value, color[2].value);
    }
    else if(components == 4)
    {
        glColor4f(color[0].value, color[1].value, color[2].value, color[3].value);
    }
}

void FinalePageWidget::draw()
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
            if(d->bg.material)
            {
                MaterialVariantSpec const &spec =
                    App_ResourceSystem().materialSpec(UiContext, 0, 0, 0, 0,
                                                      GL_REPEAT, GL_REPEAT, 0, 1, 0,
                                                      false, false, false, false);
                MaterialSnapshot const &ms = d->bg.material->prepare(spec);

                GL_BindTexture(&ms.texture(MTU_PRIMARY));
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

    // The 3D projection matrix.
    // We're assuming pixels are squares.
    /*float aspect = DENG_WINDOW->width / (float) DENG_WINDOW->height;
    yfov = 2 * RAD2DEG(atan(tan(DEG2RAD(90) / 2) / aspect));
    GL_InfinitePerspective(yfov, aspect, .05f);*/

    // We need a left-handed yflipped coordinate system.
    //glScalef(1, -1, -1);

    // Clear Z buffer (prevent the objects being clipped by nearby polygons).
    glClear(GL_DEPTH_BUFFER_BIT);

    if(renderWireframe > 1)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);

    Vector3f worldOrigin(/*-SCREENWIDTH/2*/ - d->offset[VX].value,
                         /*-SCREENHEIGHT/2*/ - d->offset[VY].value,
                         0/*.05f - d->offset[VZ].value*/);

    for(FinaleWidget *wi : d->widgets) wi->draw(worldOrigin);

    /*{rendmodelparm_t parm; de::zap(parm);

    glEnable(GL_DEPTH_TEST);

    worldOrigin[VY] += 50.f / SCREENWIDTH * (40);
    worldOrigin[VZ] += 20; // Suitable default?
    setupModelparmForFIObject(&parm, "testmodel", worldOrigin);
    Rend_RenderModel(&parm);

    worldOrigin[VX] -= 160.f / SCREENWIDTH * (40);
    setupModelparmForFIObject(&parm, "testmodel", worldOrigin);
    Rend_RenderModel(&parm);

    worldOrigin[VX] += 320.f / SCREENWIDTH * (40);
    setupModelparmForFIObject(&parm, "testmodel", worldOrigin);
    Rend_RenderModel(&parm);

    glDisable(GL_DEPTH_TEST);}*/

    // Restore original matrices and state: back to normal 2D.
    glDisable(GL_ALPHA_TEST);
    //glDisable(GL_CULL_FACE);
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
    if(!DD_IsSharpTick()) return;

    // A new 'sharp' tick has begun.
    d->timer++;

    for(FinaleWidget *wi : d->widgets) wi->runTicks(/*timeDelta*/);

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
    return d->widgets.contains(widget);
}

FinaleWidget *FinalePageWidget::addWidget(FinaleWidget *widgetToAdd)
{
    if(widgetToAdd && !d->widgets.contains(widgetToAdd))
    {
        d->widgets.append(widgetToAdd);
        widgetToAdd->setPage(this);
        widgetToAdd->audienceForDeletion += d;
    }
    return widgetToAdd;
}

FinaleWidget *FinalePageWidget::removeWidget(FinaleWidget *widgetToRemove)
{
    if(widgetToRemove && d->widgets.contains(widgetToRemove))
    {
        widgetToRemove->audienceForDeletion -= d;
        widgetToRemove->setPage(nullptr);
        d->widgets.removeOne(widgetToRemove);
    }
    return widgetToRemove;
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

#if 0
static void setupModelParamsForFIObject(rendmodelparams_t *params,
    char const *modelId, float const worldOffset[3])
{
    float pos[] = { SCREENWIDTH/2, SCREENHEIGHT/2, 0 };
    modeldef_t* mf = Models_Definition(modelId);
    if(!mf) return;

    params->mf = mf;
    params->origin[VX] = worldOffset[VX] + pos[VX];
    params->origin[VY] = worldOffset[VZ] + pos[VZ];
    params->origin[VZ] = worldOffset[VY] + pos[VY];
    params->distance = -10; /// @todo inherit depth.
    params->yawAngleOffset   = (SCREENWIDTH/2  - pos[VX]) * weaponOffsetScale + 90;
    params->pitchAngleOffset = (SCREENHEIGHT/2 - pos[VY]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
    params->yaw = params->yawAngleOffset + 180;
    params->pitch = params->yawAngleOffset + 90;
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shinepspriteCoordSpace = true;
    params->ambientColor[CR] = params->ambientColor[CG] = params->ambientColor[CB] = 1;
    params->ambientColor[CA] = 1;
    /**
     * @todo This additional scale multiplier is necessary for the model
     * to be drawn at a scale consistent with the other object types
     * (e.g., Model compared to Pic).
     *
     * Both terms are also present in the other object scale calcs and can
     * therefore be refactored away.
     */
    params->extraScale = .1f - (.05f * .05f);

    // Lets get it spinning so we can better see whats going on.
    params->yaw += rFrameCount;
}
#endif

#ifdef __CLIENT__

static void setupProjectionForFinale(dgl_borderedprojectionstate_t *bp)
{
    GL_ConfigureBorderedProjection(bp, BPF_OVERDRAW_CLIP,
                                   SCREENWIDTH, SCREENHEIGHT,
                                   DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT,
                                   scalemode_t(Con_GetByte("rend-finale-stretch")));
}

bool FI_IsStretchedToView()
{
    dgl_borderedprojectionstate_t bp;
    setupProjectionForFinale(&bp);
    return (bp.scaleMode == SCALEMODE_STRETCH);
}

void UI2_Drawer()
{
    LOG_AS("UI2_Drawer");
    if(!inited)
    {
        LOGDEV_ERROR("Not initialized yet!");
        return;
    }

    if(pages.isEmpty()) return;

    dgl_borderedprojectionstate_t bp;
    //dd_bool bordered;

    setupProjectionForFinale(&bp);
    GL_BeginBorderedProjection(&bp);

    /*bordered = (FI_ScriptActive() && FI_ScriptCmdExecuted());
    if(bordered)
    {
        // Draw using the special bordered projection.
        GL_ConfigureBorderedProjection(&borderedProjection);
        GL_BeginBorderedProjection(&borderedProjection);
    }*/

    for(FinalePageWidget *page : pages) page->draw();

    GL_EndBorderedProjection(&bp);

    //if(bordered)
    //    GL_EndBorderedProjection(&borderedProjection);
}

static int buildGeometry(float const /*dimensions*/[3], dd_bool flipTextureS,
    Vector4f const &bottomColor, Vector4f const &topColor, Vector3f **posCoords,
    Vector4f **colorCoords, Vector2f **texCoords)
{
    static Vector3f posCoordBuf[4];
    static Vector4f colorCoordBuf[4];
    static Vector2f texCoordBuf[4];

    // 0 - 1
    // | / |  Vertex layout
    // 2 - 3

    posCoordBuf[0] = Vector3f(0, 0, 0);
    posCoordBuf[1] = Vector3f(1, 0, 0);
    posCoordBuf[2] = Vector3f(0, 1, 0);
    posCoordBuf[3] = Vector3f(1, 1, 0);

    texCoordBuf[0] = Vector2f((flipTextureS? 1:0), 0);
    texCoordBuf[1] = Vector2f((flipTextureS? 0:1), 0);
    texCoordBuf[2] = Vector2f((flipTextureS? 1:0), 1);
    texCoordBuf[3] = Vector2f((flipTextureS? 0:1), 1);

    colorCoordBuf[0] = bottomColor;
    colorCoordBuf[1] = bottomColor;
    colorCoordBuf[2] = topColor;
    colorCoordBuf[3] = topColor;

    *posCoords   = posCoordBuf;
    *texCoords   = texCoordBuf;
    *colorCoords = colorCoordBuf;

    return 4;
}

static void drawGeometry(int numVerts, Vector3f const *posCoords,
    Vector4f const *colorCoords, Vector2f const *texCoords)
{
    glBegin(GL_TRIANGLE_STRIP);
    Vector3f const *posIt   = posCoords;
    Vector4f const *colorIt = colorCoords;
    Vector2f const *texIt   = texCoords;
    for(int i = 0; i < numVerts; ++i, posIt++, colorIt++, texIt++)
    {
        if(texCoords)
            glTexCoord2f(texIt->x, texIt->y);

        if(colorCoords)
            glColor4f(colorIt->x, colorIt->y, colorIt->z, colorIt->w);

        glVertex3f(posIt->x, posIt->y, posIt->z);
    }
    glEnd();
}

static void drawPicFrame(FinaleAnimWidget *p, uint frame, float const _origin[3],
    float /*const*/ scale[3], float const rgba[4], float const rgba2[4], float angle,
    Vector3f const &worldOffset)
{
    vec3f_t offset = { 0, 0, 0 }, dimensions, origin, originOffset, center;
    vec2f_t texScale = { 1, 1 };
    vec2f_t rotateCenter = { .5f, .5f };
    dd_bool showEdges = true, flipTextureS = false;
    dd_bool mustPopTextureMatrix = false;
    dd_bool textureEnabled = false;
    int numVerts;
    Vector3f *posCoords;
    Vector4f *colorCoords;
    Vector2f *texCoords;

    if(p->frameCount())
    {
        /// @todo Optimize: Texture/Material searches should be NOT be done here -ds
        FinaleAnimWidget::Frame *f = p->allFrames().at(frame);

        flipTextureS = (f->flags.flip != 0);
        showEdges = false;

        switch(f->type)
        {
        case FinaleAnimWidget::Frame::PFT_RAW: {
            rawtex_t *rawTex = App_ResourceSystem().declareRawTexture(f->texRef.lumpNum);
            if(rawTex)
            {
                DGLuint glName = GL_PrepareRawTexture(*rawTex);
                V3f_Set(offset, 0, 0, 0);
                // Raw images are always considered to have a logical size of 320x200
                // even though the actual texture resolution may be different.
                V3f_Set(dimensions, 320 /*rawTex->width*/, 200 /*rawTex->height*/, 0);
                // Rotation occurs around the center of the screen.
                V2f_Set(rotateCenter, 160, 100);
                GL_BindTextureUnmanaged(glName, gl::ClampToEdge, gl::ClampToEdge,
                                        (filterUI ? gl::Linear : gl::Nearest));
                if(glName)
                {
                    glEnable(GL_TEXTURE_2D);
                    textureEnabled = true;
                }
            }
            break; }

        case FinaleAnimWidget::Frame::PFT_XIMAGE:
            V3f_Set(offset, 0, 0, 0);
            V3f_Set(dimensions, 1, 1, 0);
            V2f_Set(rotateCenter, .5f, .5f);
            GL_BindTextureUnmanaged(f->texRef.tex, gl::ClampToEdge, gl::ClampToEdge,
                                    (filterUI ? gl::Linear : gl::Nearest));
            if(f->texRef.tex)
            {
                glEnable(GL_TEXTURE_2D);
                textureEnabled = true;
            }
            break;

        case FinaleAnimWidget::Frame::PFT_MATERIAL: {
            Material *mat = f->texRef.material;
            if(mat)
            {
                MaterialVariantSpec const &spec =
                    App_ResourceSystem().materialSpec(UiContext, 0, 0, 0, 0,
                                                      GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                                      0, -3, 0, false, false, false, false);
                MaterialSnapshot const &ms = mat->prepare(spec);

                GL_BindTexture(&ms.texture(MTU_PRIMARY));
                glEnable(GL_TEXTURE_2D);
                textureEnabled = true;

                TextureVariantSpec const &texSpec = ms.texture(MTU_PRIMARY).spec();

                /// @todo Utilize *all* properties of the Material.
                V3f_Set(dimensions,
                        ms.width() + texSpec.variant.border * 2,
                        ms.height() + texSpec.variant.border * 2, 0);
                V2f_Set(rotateCenter, dimensions[VX] / 2, dimensions[VY] / 2);
                ms.texture(MTU_PRIMARY).glCoords(&texScale[VX], &texScale[VY]);

                Texture const &texture = ms.texture(MTU_PRIMARY).generalCase();
                de::Uri uri = texture.manifest().composeUri();
                if(!uri.scheme().compareWithoutCase("Sprites"))
                {
                    V3f_Set(offset, texture.origin().x, texture.origin().y, 0);
                }
                else
                {
                    V3f_Set(offset, 0, 0, 0);
                }
            }
            break; }

        case FinaleAnimWidget::Frame::PFT_PATCH: {
            TextureManifest &manifest = App_ResourceSystem().textureScheme("Patches")
                                            .findByUniqueId(f->texRef.patch);
            if(manifest.hasTexture())
            {
                Texture &tex = manifest.texture();
                TextureVariantSpec const &texSpec =
                    Rend_PatchTextureSpec(0 | (tex.isFlagged(Texture::Monochrome)        ? TSF_MONOCHROME : 0)
                                            | (tex.isFlagged(Texture::UpscaleAndSharpen) ? TSF_UPSCALE_AND_SHARPEN : 0));
                GL_BindTexture(tex.prepareVariant(texSpec));
                glEnable(GL_TEXTURE_2D);
                textureEnabled = true;

                V3f_Set(offset, tex.origin().x, tex.origin().y, 0);
                V3f_Set(dimensions, tex.width(), tex.height(), 0);
                V2f_Set(rotateCenter, dimensions[VX]/2, dimensions[VY]/2);
            }
            break; }

        default:
            App_Error("drawPicFrame: Invalid FI_PIC frame type %i.", int(f->type));
        }
    }

    // If we've not chosen a texture by now set some defaults.
    /// @todo This is some seriously funky logic... refactor or remove.
    if(!textureEnabled)
    {
        V3f_Copy(dimensions, scale);
        V3f_Set(scale, 1, 1, 1);
        V2f_Set(rotateCenter, dimensions[VX] / 2, dimensions[VY] / 2);
    }

    V3f_Set(center, dimensions[VX] / 2, dimensions[VY] / 2, dimensions[VZ] / 2);

    V3f_Sum(origin, _origin, center);
    V3f_Subtract(origin, origin, offset);
    for(int i = 0; i < 3; ++i) { origin[i] += worldOffset[i]; }

    V3f_Subtract(originOffset, offset, center);
    offset[VX] *= scale[VX]; offset[VY] *= scale[VY]; offset[VZ] *= scale[VZ];
    V3f_Sum(originOffset, originOffset, offset);

    numVerts = buildGeometry(dimensions, flipTextureS, rgba, rgba2, &posCoords, &colorCoords, &texCoords);

    // Setup the transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    //glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);

    // Move to the object origin.
    glTranslatef(origin[VX], origin[VY], origin[VZ]);

    // Translate to the object center.
    /// @todo Remove this; just go to origin directly. Rotation origin is
    /// now separately in 'rotateCenter'. -jk
    glTranslatef(originOffset[VX], originOffset[VY], originOffset[VZ]);

    glScalef(scale[VX], scale[VY], scale[VZ]);

    if(angle != 0)
    {
        glTranslatef(rotateCenter[VX], rotateCenter[VY], 0);

        // With rotation we must counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(angle, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);

        glTranslatef(-rotateCenter[VX], -rotateCenter[VY], 0);
    }

    glMatrixMode(GL_MODELVIEW);
    // Scale up our unit-geometry to the desired dimensions.
    glScalef(dimensions[VX], dimensions[VY], dimensions[VZ]);

    if(texScale[0] != 1 || texScale[1] != 1)
    {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glScalef(texScale[0], texScale[1], 1);
        mustPopTextureMatrix = true;
    }

    drawGeometry(numVerts, posCoords, colorCoords, texCoords);

    GL_SetNoTexture();

    if(mustPopTextureMatrix)
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }

    if(showEdges)
    {
        glBegin(GL_LINES);
            useColor(p->edgeColor(), 4);
            glVertex2f(0, 0);
            glVertex2f(1, 0);
            glVertex2f(1, 0);

            useColor(p->otherEdgeColor(), 4);
            glVertex2f(1, 1);
            glVertex2f(1, 1);
            glVertex2f(0, 1);
            glVertex2f(0, 1);

            useColor(p->edgeColor(), 4);
            glVertex2f(0, 0);
        glEnd();
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
#endif // __CLIENT__

// -----------------------------------------------------------------------------------

FinaleAnimWidget::Frame::Frame()
    : tics (0)
    , type (PFT_MATERIAL)
    , sound(0)
{
    de::zap(flags);
    de::zap(texRef);
}

FinaleAnimWidget::Frame::~Frame()
{
#ifdef __CLIENT__
    if(type == PFT_XIMAGE)
    {
        DGL_DeleteTextures(1, (DGLuint *)&texRef.tex);
    }
#endif
}

static FinaleAnimWidget::Frame *newAnimWidgetFrame(FinaleAnimWidget::Frame::Type type, int tics,
    void *texRef, short sound, bool flagFlipH)
{
    FinaleAnimWidget::Frame *f = new FinaleAnimWidget::Frame;
    f->flags.flip = flagFlipH;
    f->type  = type;
    f->tics  = tics;
    f->sound = sound;

    switch(f->type)
    {
    case FinaleAnimWidget::Frame::PFT_MATERIAL:  f->texRef.material =  ((Material *)texRef);  break;
    case FinaleAnimWidget::Frame::PFT_PATCH:     f->texRef.patch    = *((patchid_t *)texRef); break;
    case FinaleAnimWidget::Frame::PFT_RAW:       f->texRef.lumpNum  = *((lumpnum_t *)texRef); break;
    case FinaleAnimWidget::Frame::PFT_XIMAGE:    f->texRef.tex      = *((DGLuint *)texRef);   break;

    default: throw Error("createPicFrame", "Unknown frame type #" + String::number(type));
    }

    return f;
}

DENG2_PIMPL_NOREF(FinaleAnimWidget)
{
    bool animComplete = true;
    bool animLooping  = false;  ///< @c true= loop back to the start when the end is reached.
    int tics          = 0;
    int curFrame      = 0;
    Frames frames;

    animatorvector4_t color;

    /// For rectangle-objects.
    animatorvector4_t otherColor;
    animatorvector4_t edgeColor;
    animatorvector4_t otherEdgeColor;

    Instance()
    {
        AnimatorVector4_Init(color,          1, 1, 1, 1);
        AnimatorVector4_Init(otherColor,     0, 0, 0, 0);
        AnimatorVector4_Init(edgeColor,      0, 0, 0, 0);
        AnimatorVector4_Init(otherEdgeColor, 0, 0, 0, 0);
    }
};

FinaleAnimWidget::FinaleAnimWidget(String const &name)
    : FinaleWidget(name)
    , d(new Instance)
{}

FinaleAnimWidget::~FinaleAnimWidget()
{
    clearAllFrames();
}

bool FinaleAnimWidget::animationComplete() const
{
    return d->animComplete;
}

FinaleAnimWidget &FinaleAnimWidget::setLooping(bool yes)
{
    d->animLooping = yes;
    return *this;
}

#ifdef __CLIENT__
void FinaleAnimWidget::draw(Vector3f const &offset)
{
    // Fully transparent pics will not be drawn.
    if(!(d->color[CA].value > 0)) return;

    vec3f_t _scale, _origin;
    V3f_Set(_origin, origin()[VX].value, origin()[VY].value, origin()[VZ].value);
    V3f_Set(_scale, scale()[VX].value, scale()[VY].value, scale()[VZ].value);

    vec4f_t rgba, rgba2;
    V4f_Set(rgba, d->color[CR].value, d->color[CG].value, d->color[CB].value, d->color[CA].value);
    if(!frameCount())
    {
        V4f_Set(rgba2, d->otherColor[CR].value, d->otherColor[CG].value, d->otherColor[CB].value, d->otherColor[CA].value);
    }

    drawPicFrame(this, d->curFrame, _origin, _scale, rgba, (!frameCount()? rgba2 : rgba), angle().value, offset);
}
#endif

void FinaleAnimWidget::runTicks(/*timespan_t timeDelta*/)
{
    FinaleWidget::runTicks();

    AnimatorVector4_Think(d->color);
    AnimatorVector4_Think(d->otherColor);
    AnimatorVector4_Think(d->edgeColor);
    AnimatorVector4_Think(d->otherEdgeColor);

    if(!(d->frames.count() > 1)) return;

    // If animating, decrease the sequence timer.
    if(d->frames.at(d->curFrame)->tics > 0)
    {
        if(--d->tics <= 0)
        {
            Frame *f;
            // Advance the sequence position. k = next pos.
            uint next = d->curFrame + 1;

            if(next == d->frames.count())
            {
                // This is the end.
                d->animComplete = true;

                // Stop the sequence?
                if(d->animLooping)
                {
                    next = 0; // Rewind back to beginning.
                }
                else // Yes.
                {
                    d->frames.at(next = d->curFrame)->tics = 0;
                }
            }

            // Advance to the next pos.
            f = d->frames.at(d->curFrame = next);
            d->tics = f->tics;

            // Play a sound?
            if(f->sound > 0)
                S_LocalSound(f->sound, 0);
        }
    }
}

int FinaleAnimWidget::newFrame(Frame::Type type, int tics, void *texRef, short sound, bool flagFlipH)
{
    d->frames.append(newAnimWidgetFrame(type, tics, texRef, sound, flagFlipH));
    // The addition of a new frame means the animation has not yet completed.
    d->animComplete = false;
    return d->frames.count();
}

FinaleAnimWidget::Frames const &FinaleAnimWidget::allFrames() const
{
    return d->frames;
}

FinaleAnimWidget &FinaleAnimWidget::clearAllFrames()
{
    qDeleteAll(d->frames); d->frames.clear();
    d->curFrame     = 0;
    d->animComplete = true;  // Nothing to animate.
    d->animLooping  = false; // Yeah?
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::resetAllColors()
{
    // Default colors.
    AnimatorVector4_Init(d->color,      1, 1, 1, 1);
    AnimatorVector4_Init(d->otherColor, 1, 1, 1, 1);

    // Edge alpha is zero by default.
    AnimatorVector4_Init(d->edgeColor,      1, 1, 1, 0);
    AnimatorVector4_Init(d->otherEdgeColor, 1, 1, 1, 0);
    return *this;
}

animator_t const *FinaleAnimWidget::color() const
{
    return d->color;
}

FinaleAnimWidget &FinaleAnimWidget::setColor(Vector3f const &newColor, int steps)
{
    AnimatorVector3_Set(d->color, newColor.x, newColor.y, newColor.z, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setAlpha(float newAlpha, int steps)
{
    Animator_Set(&d->color[3], newAlpha, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->color, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

animator_t const *FinaleAnimWidget::edgeColor() const
{
    return d->edgeColor;
}

FinaleAnimWidget &FinaleAnimWidget::setEdgeColor(Vector3f const &newColor, int steps)
{
    AnimatorVector3_Set(d->edgeColor, newColor.x, newColor.y, newColor.z, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setEdgeAlpha(float newAlpha, int steps)
{
    Animator_Set(&d->edgeColor[3], newAlpha, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setEdgeColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->edgeColor, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

animator_t const *FinaleAnimWidget::otherColor() const
{
    return d->otherColor;
}

FinaleAnimWidget &FinaleAnimWidget::setOtherColor(de::Vector3f const &newColor, int steps)
{
    AnimatorVector3_Set(d->otherColor, newColor.x, newColor.y, newColor.z, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setOtherAlpha(float newAlpha, int steps)
{
    Animator_Set(&d->otherColor[3], newAlpha, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setOtherColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->otherColor, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

animator_t const *FinaleAnimWidget::otherEdgeColor() const
{
    return d->otherEdgeColor;
}

FinaleAnimWidget &FinaleAnimWidget::setOtherEdgeColor(de::Vector3f const &newColor, int steps)
{
    AnimatorVector3_Set(d->otherEdgeColor, newColor.x, newColor.y, newColor.z, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setOtherEdgeAlpha(float newAlpha, int steps)
{
    Animator_Set(&d->otherEdgeColor[3], newAlpha, steps);
    return *this;
}

FinaleAnimWidget &FinaleAnimWidget::setOtherEdgeColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
{
    AnimatorVector4_Set(d->otherEdgeColor, newColorAndAlpha.x, newColorAndAlpha.y, newColorAndAlpha.z, newColorAndAlpha.w, steps);
    return *this;
}

// ------------------------------------------------------------------------------------

DENG2_PIMPL_NOREF(FinaleTextWidget)
{
    animatorvector4_t color;
    uint pageColor    = 0;                ///< 1-based page color index. @c 0= Use our own color.
    uint pageFont     = 0;                ///< 1-based page font index. @c 0= Use our own font.
    int alignFlags    = ALIGN_TOPLEFT;    ///< @ref alignmentFlags
    short textFlags   = DTF_ONLY_SHADOW;  ///< @ref drawTextFlags
    int scrollWait    = 0;
    int scrollTimer   = 0;   ///< Automatic scrolling upwards.
    int cursorPos     = 0;
    int wait          = 3;
    int timer         = 0;
    float lineHeight  = 11.f / 7 - 1;
    fontid_t fontNum  = 0;
    char *text        = nullptr;
    bool animComplete = true;

    Instance()  { AnimatorVector4_Init(color, 1, 1, 1, 1); }
    ~Instance() { Z_Free(text); }

#ifdef __CLIENT__
    static int textLineWidth(char const *text)
    {
        int width = 0;

        for(; *text; text++)
        {
            if(*text == '\\')
            {
                if(!*++text)     break;
                if(*text == 'n') break;

                if(*text >= '0' && *text <= '9')
                    continue;
                if(*text == 'w' || *text == 'W' || *text == 'p' || *text == 'P')
                    continue;
            }
            width += FR_CharWidth(*text);
        }

        return width;
    }
#endif
};

FinaleTextWidget::FinaleTextWidget(String const &name)
    : FinaleWidget(name)
    , d(new Instance)
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

void FinaleTextWidget::runTicks()
{
    FinaleWidget::runTicks();

    AnimatorVector4_Think(d->color);

    if(d->wait)
    {
        if(--d->timer <= 0)
        {
            if(d->wait > 0)
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

    if(d->scrollWait)
    {
        if(--d->scrollTimer <= 0)
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
void FinaleTextWidget::draw(Vector3f const &offset)
{
    if(!d->text) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    //glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);
    glTranslatef(origin()[0].value + offset.x, origin()[1].value + offset.y, origin()[2].value + offset.z);

    if(angle().value != 0)
    {
        // Counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(angle().value, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    glScalef(scale()[0].value, scale()[1].value, scale()[2].value);
    glEnable(GL_TEXTURE_2D);

    FR_SetFont(d->pageFont? page()->predefinedFont(d->pageFont - 1) : d->fontNum);

    // Set the normal color.
    animatorvector3_t const *color;
    if(d->pageColor == 0)
        color = (animatorvector3_t const *)&d->color;
    else
        color = page()->predefinedColor(d->pageColor - 1);
    FR_SetColor((*color)[CR].value, (*color)[CG].value, (*color)[CB].value);
    FR_SetAlpha(d->color[CA].value);

    int x = 0, y = 0, ch, linew = -1;
    char *ptr = d->text;
    for(int cnt = 0; *ptr && (!d->wait || cnt < d->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = d->textLineWidth(ptr);

        ch = *ptr;
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;

            // Change of color?
            if(*ptr >= '0' && *ptr <= '9')
            {
                uint colorIdx = *ptr - '0';
                if(colorIdx == 0)
                    color = (animatorvector3_t const *)&d->color;
                else
                    color = page()->predefinedColor(colorIdx - 1);
                FR_SetColor((*color)[CR].value, (*color)[CG].value, (*color)[CB].value);
                FR_SetAlpha(d->color[CA].value);
                continue;
            }

            // 'w' = half a second wait, 'W' = second'f wait
            if(*ptr == 'w' || *ptr == 'W') // Wait?
            {
                if(d->wait)
                    cnt += int(float(TICRATE) / d->wait / (*ptr == 'w' ? 2 : 1));
                continue;
            }

            // 'p' = 5 second wait, 'P' = 10 second wait
            if(*ptr == 'p' || *ptr == 'P') // Longer pause?
            {
                if(d->wait)
                    cnt += int(float(TICRATE) / d->wait * (*ptr == 'p' ? 5 : 10));
                continue;
            }

            if(*ptr == 'n' || *ptr == 'N') // Newline?
            {
                x = 0;
                y += FR_CharHeight('A') * (1 + d->lineHeight);
                linew = -1;
                cnt++; // Include newlines in the wait count.
                continue;
            }

            if(*ptr == '_')
                ch = ' ';
        }

        // Let'f do Y-clipping (in case of tall text blocks).
        if(scale()[1].value * y + origin()[1].value >= -scale()[1].value * d->lineHeight &&
           scale()[1].value * y + origin()[1].value < SCREENHEIGHT)
        {
            FR_DrawCharXY(ch, (d->alignFlags & ALIGN_LEFT) ? x : x - linew / 2, y);
            x += FR_CharWidth(ch);
        }

        ++cnt;
    }

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
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
    if(d->text)
    {
        float const secondLen = (d->wait? TICRATE / d->wait : 0);

        for(char const *ptr = d->text; *ptr; ptr++)
        {
            if(*ptr == '\\') // Escape?
            {
                if(!*++ptr) break;

                switch(*ptr)
                {
                case 'w':   count += secondLen / 2;   break;
                case 'W':   count += secondLen;       break;
                case 'p':   count += 5 * secondLen;   break;
                case 'P':   count += 10 * secondLen;  break;

                default:
                    if((*ptr >= '0' && *ptr <= '9') || *ptr == 'n' || *ptr == 'N')
                        continue;
                }
            }
            count++;
        }
    }
    return count;
}

char const *FinaleTextWidget::text() const
{
    return d->text;
}

FinaleTextWidget &FinaleTextWidget::setText(char const *newText)
{
    Z_Free(d->text); d->text = nullptr;
    if(newText && newText[0])
    {
        int len = (int)qstrlen(newText) + 1;
        d->text = (char *) Z_Malloc(len, PU_APPSTATIC, 0);
        std::memcpy(d->text, newText, len);
    }
    int const visLen = visLength();
    if(d->cursorPos > visLen)
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

FinaleTextWidget &FinaleTextWidget::setColor(Vector3f const &newColor, int steps)
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

FinaleTextWidget &FinaleTextWidget::setColorAndAlpha(Vector4f const &newColorAndAlpha, int steps)
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
