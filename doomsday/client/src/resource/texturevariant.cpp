/** @file texturevariant.cpp Context specialized texture variant.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/Log>
#include <de/mathutil.h> // M_CeilPow
#include "de_base.h"
#include "r_util.h"
#include "gl/gl_defer.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/gl_tex.h"
#include "resource/colorpalettes.h"

#include "resource/texture.h"

using namespace de;

/**
 * @todo Magnification, Anisotropic filter level and GL texture wrap modes
 * will be handled through dynamic changes to GL's texture environment state.
 * Consequently they should be ignored here.
 */
static int compareVariantSpecifications(variantspecification_t const &a,
    variantspecification_t const &b)
{
    /// @todo We can be a bit cleverer here...
    if(a.context != b.context) return 0;
    if(a.flags != b.flags) return 0;
    if(a.wrapS != b.wrapS || a.wrapT != b.wrapT) return 0;
    //if(a.magFilter != b.magFilter) return 0;
    //if(a.anisoFilter != b.anisoFilter) return 0;
    if(a.mipmapped != b.mipmapped) return 0;
    if(a.noStretch != b.noStretch) return 0;
    if(a.gammaCorrection != b.gammaCorrection) return 0;
    if(a.toAlpha != b.toAlpha) return 0;
    if(a.border != b.border) return 0;
    if(a.flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        colorpalettetranslationspecification_t const *cptA = a.translated;
        colorpalettetranslationspecification_t const *cptB = b.translated;
        DENG_ASSERT(cptA && cptB);
        if(cptA->tClass != cptB->tClass) return 0;
        if(cptA->tMap   != cptB->tMap) return 0;
    }
    return 1; // Equal.
}

static int compareDetailVariantSpecifications(detailvariantspecification_t const &a,
    detailvariantspecification_t const &b)
{
    if(a.contrast != b.contrast) return 0;
    return 1; // Equal.
}

int TextureVariantSpec_Compare(texturevariantspecification_t const *a,
    texturevariantspecification_t const *b)
{
    DENG_ASSERT(a && b);
    if(a == b) return 1;
    if(a->type != b->type) return 0;
    switch(a->type)
    {
    case TST_GENERAL: return compareVariantSpecifications(TS_GENERAL(*a), TS_GENERAL(*b));
    case TST_DETAIL:  return compareDetailVariantSpecifications(TS_DETAIL(*a), TS_DETAIL(*b));
    }
    throw Error("TextureVariantSpec_Compare", QString("Invalid type %1").arg(a->type));
}

DENG2_PIMPL(Texture::Variant)
{
    /// Superior Texture of which this is a derivative.
    Texture &texture;

    /// Specification used to derive this variant.
    texturevariantspecification_t const &spec;

    /// Variant flags.
    Texture::Variant::Flags flags;

    /// Source of this texture.
    TexSource texSource;

    /// Name of the associated GL texture object.
    uint glTexName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    Instance(Public *i, Texture &generalCase,
             texturevariantspecification_t const &spec) : Base(i),
      texture(generalCase),
      spec(spec),
      flags(0),
      texSource(TEXS_NONE),
      glTexName(0),
      s(0),
      t(0)
    {}
};

Texture::Variant::Variant(Texture &generalCase, texturevariantspecification_t const &spec)
    : d(new Instance(this, generalCase, spec))
{}

/**
 * Perform analyses of the @a image pixel data and record this information
 * for reference later.
 *
 * @param image         Image data to be analyzed.
 * @param context       Context in which the uploaded image will be used.
 * @param tex           Logical texture which will hold the analysis data.
 * @param forceUpdate   Force an update of the recorded analysis data.
 */
static void performImageAnalyses(image_t const &image,
    texturevariantusagecontext_t context, Texture &tex, bool forceUpdate)
{
    // Do we need color palette info?
    if(image.paletteId != 0)
    {
        colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(tex.analysisDataPointer(Texture::ColorPaletteAnalysis));
        bool firstInit = (!cp);

        if(firstInit)
        {
            cp = (colorpalette_analysis_t *) M_Malloc(sizeof(*cp));
            tex.setAnalysisDataPointer(Texture::ColorPaletteAnalysis, cp);
        }

        if(firstInit || forceUpdate)
            cp->paletteId = image.paletteId;
    }

    // Calculate a point light source for Dynlight and/or Halo?
    if(context == TC_SPRITE_DIFFUSE)
    {
        pointlight_analysis_t *pl = reinterpret_cast<pointlight_analysis_t*>(tex.analysisDataPointer(Texture::BrightPointAnalysis));
        bool firstInit = (!pl);

        if(firstInit)
        {
            pl = (pointlight_analysis_t *) M_Malloc(sizeof *pl);
            tex.setAnalysisDataPointer(Texture::BrightPointAnalysis, pl);
        }

        if(firstInit || forceUpdate)
            GL_CalcLuminance(image.pixels, image.size.width, image.size.height, image.pixelSize,
                             R_ToColorPalette(image.paletteId), &pl->originX, &pl->originY,
                             &pl->color, &pl->brightMul);
    }

    // Average alpha?
    if(context == TC_SPRITE_DIFFUSE || context == TC_UI)
    {
        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t*>(tex.analysisDataPointer(Texture::AverageAlphaAnalysis));
        bool firstInit = (!aa);

        if(firstInit)
        {
            aa = (averagealpha_analysis_t *) M_Malloc(sizeof(*aa));
            tex.setAnalysisDataPointer(Texture::AverageAlphaAnalysis, aa);
        }

        if(firstInit || forceUpdate)
        {
            if(!image.paletteId)
            {
                FindAverageAlpha(image.pixels, image.size.width, image.size.height,
                                 image.pixelSize, &aa->alpha, &aa->coverage);
            }
            else
            {
                if(image.flags & IMGF_IS_MASKED)
                {
                    FindAverageAlphaIdx(image.pixels, image.size.width, image.size.height,
                                        R_ToColorPalette(image.paletteId), &aa->alpha, &aa->coverage);
                }
                else
                {
                    // It has no mask, so it must be opaque.
                    aa->alpha = 1;
                    aa->coverage = 0;
                }
            }
        }
    }

    // Average color for sky ambient color?
    if(context == TC_SKYSPHERE_DIFFUSE)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageColorAnalysis, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image.paletteId)
            {
                FindAverageColor(image.pixels, image.size.width, image.size.height,
                                 image.pixelSize, &ac->color);
            }
            else
            {
                FindAverageColorIdx(image.pixels, image.size.width, image.size.height,
                                    R_ToColorPalette(image.paletteId), false, &ac->color);
            }
        }
    }

    // Amplified average color for plane glow?
    if(context == TC_MAPSURFACE_DIFFUSE)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageColorAmplifiedAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageColorAmplifiedAnalysis, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image.paletteId)
            {
                FindAverageColor(image.pixels, image.size.width, image.size.height,
                                 image.pixelSize, &ac->color);
            }
            else
            {
                FindAverageColorIdx(image.pixels, image.size.width, image.size.height,
                                    R_ToColorPalette(image.paletteId), false, &ac->color);
            }
            R_AmplifyColor(ac->color.rgb);
        }
    }

    // Average top line color for sky sphere fadeout?
    if(context == TC_SKYSPHERE_DIFFUSE)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageTopColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageTopColorAnalysis, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image.paletteId)
            {
                FindAverageLineColor(image.pixels, image.size.width, image.size.height,
                                     image.pixelSize, 0, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image.pixels, image.size.width, image.size.height, 0,
                                        R_ToColorPalette(image.paletteId), false, &ac->color);
            }
        }
    }

    // Average bottom line color for sky sphere fadeout?
    if(context == TC_SKYSPHERE_DIFFUSE)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageBottomColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageBottomColorAnalysis, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image.paletteId)
            {
                FindAverageLineColor(image.pixels, image.size.width, image.size.height,
                                     image.pixelSize, image.size.height - 1, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image.pixels, image.size.width, image.size.height,
                                        image.size.height - 1, R_ToColorPalette(image.paletteId),
                                        false, &ac->color);
            }
        }
    }
}

uint Texture::Variant::prepare()
{
    LOG_AS("TextureVariant::prepare");

    // Have we already prepared this?
    if(isPrepared())
        return d->glTexName;

    // Load the source image data.
    image_t image;
    TexSource source = GL_LoadSourceImage(image, d->texture, d->spec);
    if(source == TEXS_NONE)
        return 0;

    // Do we need to perform any image pixel data analyses?
    if(d->spec.type == TST_GENERAL)
    {
        performImageAnalyses(image, TS_GENERAL(d->spec).context,
                             d->texture, true /*force update*/);
    }

    // Are we preparing a new GL texture?
    if(d->glTexName == 0)
    {
        // Acquire a new GL texture name.
        d->glTexName = GL_GetReservedTextureName();

        // Record the source of the image.
        d->texSource = source;
    }

    // Prepare texture content for uploading.
    texturecontent_t c;
    GL_PrepareTextureContent(c, d->glTexName, image, d->spec, d->texture.manifest());

    /**
     * Calculate GL texture coordinates based on the image dimensions. The
     * coordinates are calculated as width / CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     *
     * @todo fixme: Image dimensions may not be the same as the uploaded
     * texture - defer this logic until all processing has been completed.
     */
    if((c.flags & TXCF_UPLOAD_ARG_NOSTRETCH) &&
       (!GL_state.features.texNonPowTwo || (c.flags & TXCF_MIPMAP)))
    {
        d->s =  image.size.width / float( M_CeilPow2(image.size.width) );
        d->t = image.size.height / float( M_CeilPow2(image.size.height) );
    }
    else
    {
        d->s = 1;
        d->t = 1;
    }

    if(image.flags & IMGF_IS_MASKED)
        d->flags |= TextureVariant::Masked;

    // Submit the content for uploading (possibly deferred).
    GLUploadMethod uploadMethod = GL_ChooseUploadMethod(c);
    GL_UploadTextureContent(c, uploadMethod);

#ifdef DENG_DEBUG
    LOG_VERBOSE("Prepared \"%s\" variant (glName:%u)%s")
        << d->texture.manifest().composeUri() << uint(d->glTexName)
        << (uploadMethod == Immediate? " while not busy!" : "");

    if(verbose >= 2)
    {
        String textualVariantSpec = d->spec.asText();
        LOG_INFO("  Content: %s") << Image_Description(&image);
        LOG_INFO("  Specification [%p]: %s")
            << de::dintptr(&d->spec) << textualVariantSpec;
    }
#endif

    // Are we setting the logical dimensions to the pixel dimensions
    // of the source image?
    if(d->texture.width() == 0 && d->texture.height() == 0)
    {
        Vector2i dimensions = Image_Dimensions(&image);
#ifdef DENG_DEBUG
        LOG_VERBOSE("World dimensions for \"%s\" taken from image pixels %s.")
            << d->texture.manifest().composeUri() << dimensions.asText();
#endif
        d->texture.setDimensions(dimensions);
    }

    // We're done with the image data.
    Image_Destroy(&image);

    return d->glTexName;
}

void Texture::Variant::release()
{
    if(!isPrepared()) return;

    glDeleteTextures(1, (GLuint const *) &d->glTexName);
    d->glTexName = 0;
}

Texture &Texture::Variant::generalCase() const
{
    return d->texture;
}

texturevariantspecification_t const &Texture::Variant::spec() const
{
    return d->spec;
}

TexSource Texture::Variant::source() const
{
    return d->texSource;
}

String Texture::Variant::sourceDescription() const
{
    if(d->texSource == TEXS_ORIGINAL) return "original";
    if(d->texSource == TEXS_EXTERNAL) return "external";
    return "none";
}

Texture::Variant::Flags Texture::Variant::flags() const
{
    return d->flags;
}

void Texture::Variant::glCoords(float *outS, float *outT) const
{
    if(outS) *outS = d->s;
    if(outT) *outT = d->t;
}

uint Texture::Variant::glName() const
{
    return d->glTexName;
}
