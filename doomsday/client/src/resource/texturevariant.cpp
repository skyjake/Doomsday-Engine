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

#include "de_base.h"
#include "resource/texture.h"

#include "r_util.h"

#include "gl/gl_defer.h"
#include "gl/gl_main.h"
#include "gl/gl_tex.h"
#include "gl/texturecontent.h"

#include "resource/colorpalettes.h"
#include "resource/image.h" // GL_LoadSourceImage

#include "render/rend_main.h" // misc global vars awaiting new home

#include <de/Log>
#include <de/mathutil.h> // M_CeilPow

using namespace de;

variantspecification_t::variantspecification_t()
    : context(TC_UNKNOWN)
    , flags(0)
    , border(0)
    , wrapS(GL_REPEAT)
    , wrapT(GL_REPEAT)
    , mipmapped(false)
    , gammaCorrection(true)
    , noStretch(false)
    , toAlpha(false)
    , minFilter(GL_LINEAR)
    , magFilter(GL_LINEAR)
    , anisoFilter(0)
    , tClass(0)
    , tMap(0)
{}

variantspecification_t::variantspecification_t(variantspecification_t const &other)
    : context(other.context)
    , flags(other.flags)
    , border(other.border)
    , wrapS(other.wrapS)
    , wrapT(other.wrapT)
    , mipmapped(other.mipmapped)
    , gammaCorrection(other.gammaCorrection)
    , noStretch(other.noStretch)
    , toAlpha(other.toAlpha)
    , minFilter(other.minFilter)
    , magFilter(other.magFilter)
    , anisoFilter(other.anisoFilter)
    , tClass(other.tClass)
    , tMap(other.tMap)
{}

bool variantspecification_t::operator == (variantspecification_t const &other) const
{
    if(this == &other) return 1;
    /// @todo We can be a bit cleverer here...
    if(context != other.context) return 0;
    if(flags != other.flags) return 0;
    if(wrapS != other.wrapS || wrapT != other.wrapT) return 0;
    //if(magFilter != b.magFilter) return 0;
    //if(anisoFilter != b.anisoFilter) return 0;
    if(mipmapped != other.mipmapped) return 0;
    if(noStretch != other.noStretch) return 0;
    if(gammaCorrection != other.gammaCorrection) return 0;
    if(toAlpha != other.toAlpha) return 0;
    if(border != other.border) return 0;
    if(flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        if(tClass != other.tClass) return 0;
        if(tMap   != other.tMap) return 0;
    }
    return 1; // Equal.
}

GLint variantspecification_t::glMinFilter() const
{
    if(minFilter >= 0) // Constant logical value.
    {
        return (mipmapped? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST) + minFilter;
    }
    // "No class" preference.
    return mipmapped? glmode[mipmapping] : GL_LINEAR;
}

GLint variantspecification_t::glMagFilter() const
{
    if(magFilter >= 0) // Constant logical value.
    {
        return GL_NEAREST + magFilter;
    }

    // Preference for texture class id.
    switch(abs(magFilter)-1)
    {
    case 1: // Sprite class.
        return filterSprites? GL_LINEAR : GL_NEAREST;
    case 2: // UI class.
        return filterUI? GL_LINEAR : GL_NEAREST;
    default: // "No class" preference.
        return glmode[texMagMode];
    }
}

int variantspecification_t::logicalAnisoLevel() const
{
    return anisoFilter < 0? texAniso : anisoFilter;
}

TextureVariantSpec::TextureVariantSpec(texturevariantspecificationtype_t type)
    : type(type)
{}

TextureVariantSpec::TextureVariantSpec(TextureVariantSpec const &other)
    : type(other.type)
    , variant(other.variant)
    , detailVariant(other.detailVariant)
{}

bool detailvariantspecification_t::operator == (detailvariantspecification_t const &other) const
{
    if(this == &other) return true;
    return contrast == other.contrast; // Equal.
}

bool TextureVariantSpec::operator == (TextureVariantSpec const &other) const
{
    if(this == &other) return true;
    if(type != other.type) return false;
    switch(type)
    {
    case TST_GENERAL: return variant == other.variant;
    case TST_DETAIL:  return detailVariant == other.detailVariant;
    }
    DENG2_ASSERT(false);
    return false;
}

static String nameForGLTextureWrapMode(int mode)
{
    if(mode == GL_REPEAT) return "repeat";
    if(mode == GL_CLAMP) return "clamp";
    if(mode == GL_CLAMP_TO_EDGE) return "clamp_edge";
    return "(unknown)";
}

String TextureVariantSpec::asText() const
{
    static String const textureUsageContextNames[1 + TEXTUREVARIANTUSAGECONTEXT_COUNT] = {
        /* TC_UNKNOWN */                    "unknown",
        /* TC_UI */                         "ui",
        /* TC_MAPSURFACE_DIFFUSE */         "mapsurface_diffuse",
        /* TC_MAPSURFACE_REFLECTION */      "mapsurface_reflection",
        /* TC_MAPSURFACE_REFLECTIONMASK */  "mapsurface_reflectionmask",
        /* TC_MAPSURFACE_LIGHTMAP */        "mapsurface_lightmap",
        /* TC_SPRITE_DIFFUSE */             "sprite_diffuse",
        /* TC_MODELSKIN_DIFFUSE */          "modelskin_diffuse",
        /* TC_MODELSKIN_REFLECTION */       "modelskin_reflection",
        /* TC_HALO_LUMINANCE */             "halo_luminance",
        /* TC_PSPRITE_DIFFUSE */            "psprite_diffuse",
        /* TC_SKYSPHERE_DIFFUSE */          "skysphere_diffuse"
    };
    static String const textureSpecificationTypeNames[2] = {
        /* TST_GENERAL */   "general",
        /* TST_DETAIL */    "detail"
    };
    static String const filterModeNames[] = { "ui", "sprite", "noclass", "const" };
    static String const glFilterNames[] = {
        "nearest", "linear", "nearest_mipmap_nearest", "linear_mipmap_nearest",
        "nearest_mipmap_linear", "linear_mipmap_linear"
    };

    String text = String("Type:%1").arg(textureSpecificationTypeNames[type]);

    switch(type)
    {
    case TST_DETAIL: {
        detailvariantspecification_t const &spec = detailVariant;
        text += " Contrast:" + String::number(int(.5f + spec.contrast / 255.f * 100)) + "%";
        break; }

    case TST_GENERAL: {
        variantspecification_t const &spec = variant;
        texturevariantusagecontext_t tc = spec.context;
        DENG2_ASSERT(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

        int glMinFilterNameIdx;
        if(spec.minFilter >= 0) // Constant logical value.
        {
            glMinFilterNameIdx = (spec.mipmapped? 2 : 0) + spec.minFilter;
        }
        else // "No class" preference.
        {
            glMinFilterNameIdx = spec.mipmapped? mipmapping : 1;
        }

        int glMagFilterNameIdx;
        if(spec.magFilter >= 0) // Constant logical value.
        {
            glMagFilterNameIdx = spec.magFilter;
        }
        else
        {
            // Preference for texture class id.
            switch(abs(spec.magFilter)-1)
            {
            // "No class" preference.
            default: glMagFilterNameIdx = texMagMode; break;

            // "Sprite" class.
            case 1:  glMagFilterNameIdx = filterSprites; break;

            // "UI" class.
            case 2:  glMagFilterNameIdx = filterUI; break;
            }
        }

        text += " Context:" + textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1]
              + " Flags:" + String::number(spec.flags & ~TSF_INTERNAL_MASK)
              + " Border:" + String::number(spec.border)
              + " MinFilter:" + filterModeNames[3 + de::clamp(-1, spec.minFilter, 0)]
                              + "|" + glFilterNames[glMinFilterNameIdx]
              + " MagFilter:" + filterModeNames[3 + de::clamp(-3, spec.magFilter, 0)]
                              + "|" + glFilterNames[glMagFilterNameIdx]
              + " AnisoFilter:" + String::number(spec.anisoFilter)
              + " WrapS:" + nameForGLTextureWrapMode(spec.wrapS)
              + " WrapT:" + nameForGLTextureWrapMode(spec.wrapT)
              + " CorrectGamma:" + (spec.gammaCorrection? "yes" : "no")
              + " NoStretch:" + (spec.noStretch? "yes" : "no")
              + " ToAlpha:" + (spec.toAlpha? "yes" : "no");

        if(spec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            text += " Translated:(tclass:" + String::number(spec.tClass)
                                           + " tmap:" + String::number(spec.tMap) + ")";
        }
        break; }
    }

    return text;
}

DENG2_PIMPL(Texture::Variant)
{
    Texture &texture; /// The base for which "this" is a context derivative.
    TextureVariantSpec const &spec; /// Usage context specification.
    Flags flags;

    res::Source texSource; ///< Logical source of the image.

    /// Name of the associated GL texture object.
    /// @todo Use GLTexture
    uint glTexName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    Instance(Public *i, Texture &generalCase, TextureVariantSpec const &spec)
        : Base(i)
        , texture(generalCase)
        , spec(spec)
        , flags(0)
        , texSource(res::None)
        , glTexName(0)
        , s(0)
        , t(0)
    {}

    ~Instance()
    {
        // Release any GL texture we may have prepared.
        self.release();
    }
};

Texture::Variant::Variant(Texture &generalCase, TextureVariantSpec const &spec)
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
        {
            GL_CalcLuminance(image.pixels, image.size.x, image.size.y,
                             image.pixelSize, image.paletteId,
                             &pl->originX, &pl->originY, &pl->color, &pl->brightMul);
        }
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
                FindAverageAlpha(image.pixels, image.size.x, image.size.y,
                                 image.pixelSize, &aa->alpha, &aa->coverage);
            }
            else
            {
                if(image.flags & IMGF_IS_MASKED)
                {
                    FindAverageAlphaIdx(image.pixels, image.size.x, image.size.y,
                                        &aa->alpha, &aa->coverage);
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
                FindAverageColor(image.pixels, image.size.x, image.size.y,
                                 image.pixelSize, &ac->color);
            }
            else
            {
                FindAverageColorIdx(image.pixels, image.size.x, image.size.y,
                                    App_ResourceSystem().colorPalette(image.paletteId),
                                    false, &ac->color);
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
                FindAverageColor(image.pixels, image.size.x, image.size.y,
                                 image.pixelSize, &ac->color);
            }
            else
            {
                FindAverageColorIdx(image.pixels, image.size.x, image.size.y,
                                    App_ResourceSystem().colorPalette(image.paletteId),
                                    false, &ac->color);
            }
            Vector3f color(ac->color.rgb);
            R_AmplifyColor(color);
            for(int i = 0; i < 3; ++i)
            {
                ac->color.rgb[i] = color[i];
            }
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
                FindAverageLineColor(image.pixels, image.size.x, image.size.y,
                                     image.pixelSize, 0, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image.pixels, image.size.x, image.size.y, 0,
                                        App_ResourceSystem().colorPalette(image.paletteId),
                                        false, &ac->color);
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
                FindAverageLineColor(image.pixels, image.size.x, image.size.y,
                                     image.pixelSize, image.size.y - 1, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image.pixels, image.size.x, image.size.y,
                                        image.size.y - 1,
                                        App_ResourceSystem().colorPalette(image.paletteId),
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
    res::Source source = GL_LoadSourceImage(image, d->texture, d->spec);
    if(source == res::None)
        return 0;

    // Do we need to perform any image pixel data analyses?
    if(d->spec.type == TST_GENERAL)
    {
        performImageAnalyses(image, d->spec.variant.context, d->texture,
                             true /*force update*/);
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
        d->s = image.size.x / float( de::ceilPow2(image.size.x) );
        d->t = image.size.y / float( de::ceilPow2(image.size.y) );
    }
    else
    {
        d->s = 1;
        d->t = 1;
    }

    if(image.flags & IMGF_IS_MASKED)
    {
        d->flags |= TextureVariant::Masked;
    }

    // Submit the content for uploading (possibly deferred).
    gl::UploadMethod uploadMethod = GL_ChooseUploadMethod(&c);
    GL_UploadTextureContent(c, uploadMethod);

#ifdef DENG_DEBUG
    LOG_DEBUG("Prepared \"%s\" variant (glName:%u)%s")
        << d->texture.manifest().composeUri() << uint(d->glTexName)
        << (uploadMethod == gl::Immediate? " while not busy!" : "");
    LOG_TRACE("  Content: %s") << Image_Description(image);
    LOG_TRACE("  Specification [%p]: %s") << de::dintptr(&d->spec) << d->spec.asText();
#endif

    // Are we setting the logical dimensions to the pixel dimensions
    // of the source image?
    if(d->texture.width() == 0 && d->texture.height() == 0)
    {
        LOG_DEBUG("World dimensions for \"%s\" taken from image pixels %s.")
            << d->texture.manifest().composeUri() << image.size.asText();

        d->texture.setDimensions(image.size.toVector2i());
    }

    // We're done with the image data.
    Image_ClearPixelData(image);

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

TextureVariantSpec const &Texture::Variant::spec() const
{
    return d->spec;
}

res::Source Texture::Variant::source() const
{
    return d->texSource;
}

String Texture::Variant::sourceDescription() const
{
    if(d->texSource == res::Original) return "original";
    if(d->texSource == res::External) return "external";
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
