/** @file finaleanimwidget.cpp  InFine animation system, FinaleAnimWidget.
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

#include <de/vector1.h>
#include "ui/infine/finaleanimwidget.h"

#include "dd_main.h"   // App_ResourceSystem()
#include "api_sound.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "gl/gl_main.h"
#  include "gl/gl_texmanager.h" // GL_PrepareRawTexture()
#  include "render/r_draw.h"    // Rend_PatchTextureSpec()
#  include "render/rend_main.h" // filterUI
#endif

using namespace de;

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

    static Frame *makeFrame(Frame::Type type, int tics, void *texRef, short sound, bool flagFlipH)
    {
        Frame *f = new Frame;
        f->flags.flip = flagFlipH;
        f->type  = type;
        f->tics  = tics;
        f->sound = sound;

        switch(f->type)
        {
        case Frame::PFT_MATERIAL:  f->texRef.material =  ((Material *)texRef);  break;
        case Frame::PFT_PATCH:     f->texRef.patch    = *((patchid_t *)texRef); break;
        case Frame::PFT_RAW:       f->texRef.lumpNum  = *((lumpnum_t *)texRef); break;
        case Frame::PFT_XIMAGE:    f->texRef.tex      = *((DGLuint *)texRef);   break;

        default: throw Error("FinaleAnimWidget::makeFrame", "Unknown frame type #" + String::number(type));
        }

        return f;
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

void FinaleAnimWidget::draw(Vector3f const &offset)
{
    // Fully transparent pics will not be drawn.
    if(!(d->color[3].value > 0)) return;

    vec3f_t _scale, _origin;
    V3f_Set(_origin, origin()[VX].value, origin()[VY].value, origin()[VZ].value);
    V3f_Set(_scale, scale()[VX].value, scale()[VY].value, scale()[VZ].value);

    vec4f_t rgba, rgba2;
    V4f_Set(rgba, d->color[0].value, d->color[1].value, d->color[2].value, d->color[3].value);
    if(!frameCount())
    {
        V4f_Set(rgba2, d->otherColor[0].value, d->otherColor[1].value, d->otherColor[2].value, d->otherColor[3].value);
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
    d->frames.append(d->makeFrame(type, tics, texRef, sound, flagFlipH));
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
