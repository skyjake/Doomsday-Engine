/** @file texturevariant.cpp Context specialized texture variant.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "misc/r_util.h"

#include "gl/gl_defer.h"
#include "gl/gl_main.h"
#include "gl/gl_tex.h"
#include "gl/texturecontent.h"

#include "resource/image.h" // GL_LoadSourceImage

#include "render/rend_main.h" // misc global vars awaiting new home

#include <doomsday/res/colorpalettes.h>
#include <doomsday/res/texture.h>
#include <de/logbuffer.h>
#include <de/legacy/mathutil.h> // M_CeilPow

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
    , minFilter(1) // linear
    , magFilter(1) // linear
    , anisoFilter(0)
    , tClass(0)
    , tMap(0)
{}

variantspecification_t::variantspecification_t(const variantspecification_t &other)
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

bool variantspecification_t::operator == (const variantspecification_t &other) const
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

GLenum variantspecification_t::glMinFilter() const
{
    if (minFilter >= 0) // Constant logical value.
    {
        return (mipmapped? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST) + minFilter;
    }
    // "No class" preference.
    return mipmapped? glmode[mipmapping] : GL_LINEAR;
}

GLenum variantspecification_t::glMagFilter() const
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

TextureVariantSpec::TextureVariantSpec(const TextureVariantSpec &other)
    : type(other.type)
    , variant(other.variant)
    , detailVariant(other.detailVariant)
{}

bool detailvariantspecification_t::operator == (const detailvariantspecification_t &other) const
{
    if(this == &other) return true;
    return contrast == other.contrast; // Equal.
}

bool TextureVariantSpec::operator == (const TextureVariantSpec &other) const
{
    if(this == &other) return true; // trivial
    if(type != other.type) return false;
    switch(type)
    {
    case TST_GENERAL: return variant       == other.variant;
    case TST_DETAIL:  return detailVariant == other.detailVariant;
    }
    DE_ASSERT_FAIL("Invalid texture variant specification type");
    return false;
}

static String nameForGLTextureWrapMode(GLenum mode)
{
    if(mode == GL_REPEAT) return "repeat";
//#if defined (DE_OPENGL)
//    if(mode == GL_CLAMP) return "clamp";
//#endif
    if(mode == GL_CLAMP_TO_EDGE) return "clamp_edge";
    return "(unknown)";
}

String TextureVariantSpec::asText() const
{
    static const char *textureUsageContextNames[1 + TEXTUREVARIANTUSAGECONTEXT_COUNT] = {
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
    static const char *textureSpecificationTypeNames[2] = {
        /* TST_GENERAL */   "general",
        /* TST_DETAIL */    "detail"
    };
    static const char *filterModeNames[] = { "ui", "sprite", "noclass", "const" };
    static const char *glFilterNames[] = {
        "nearest", "linear", "nearest_mipmap_nearest", "linear_mipmap_nearest",
        "nearest_mipmap_linear", "linear_mipmap_linear"
    };

    String text = Stringf("Type:%s", textureSpecificationTypeNames[type]);

    switch(type)
    {
    case TST_DETAIL: {
        const detailvariantspecification_t &spec = detailVariant;
        text += " Contrast:" + String::asText(int(.5f + spec.contrast / 255.f * 100)) + "%";
        break; }

    case TST_GENERAL: {
        const variantspecification_t &spec = variant;
        texturevariantusagecontext_t tc = spec.context;
        DE_ASSERT(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

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

        text += DE_STR(" Context:") + textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1]
              + " Flags:" + String::asText(spec.flags & ~TSF_INTERNAL_MASK)
              + " Border:" + String::asText(spec.border)
              + " MinFilter:" + filterModeNames[3 + de::clamp(-1, spec.minFilter, 0)]
                              + "|" + glFilterNames[glMinFilterNameIdx]
              + " MagFilter:" + filterModeNames[3 + de::clamp(-3, spec.magFilter, 0)]
                              + "|" + glFilterNames[glMagFilterNameIdx]
              + " AnisoFilter:" + String::asText(spec.anisoFilter)
              + " WrapS:" + nameForGLTextureWrapMode(spec.wrapS)
              + " WrapT:" + nameForGLTextureWrapMode(spec.wrapT)
              + " CorrectGamma:" + (spec.gammaCorrection? "yes" : "no")
              + " NoStretch:" + (spec.noStretch? "yes" : "no")
              + " ToAlpha:" + (spec.toAlpha? "yes" : "no");

        if(spec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            text += " Translated:(tclass:" + String::asText(spec.tClass)
                                           + " tmap:" + String::asText(spec.tMap) + ")";
        }
        break; }
    }

    return text;
}

DE_PIMPL(ClientTexture::Variant)
{
    ClientTexture &texture; /// The base for which "this" is a context derivative.
    TextureVariantSpec spec; /// Usage context specification.
    Flags flags;

    res::Source texSource; ///< Logical source of the image.

    /// Name of the associated GL texture object.
    /// @todo Use GLTexture
    uint glTexName;

    /// Prepared coordinates for the bottom right of the texture minus border.
    float s, t;

    Impl(Public *i, ClientTexture &generalCase, const TextureVariantSpec &spec)
        : Base(i)
        , texture(generalCase)
        , spec(spec)
        , flags(0)
        , texSource(res::None)
        , glTexName(0)
        , s(0)
        , t(0)
    {}

    ~Impl()
    {
        // Release any GL texture we may have prepared.
        self().release();
    }
};

ClientTexture::Variant::Variant(ClientTexture &generalCase, const TextureVariantSpec &spec)
    : d(new Impl(this, generalCase, spec))
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
static void performImageAnalyses(const image_t &image,
    texturevariantusagecontext_t context, ClientTexture &tex, bool forceUpdate)
{
    // Do we need color palette info?
    if(image.paletteId != 0)
    {
        colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(tex.analysisDataPointer(ClientTexture::ColorPaletteAnalysis));
        bool firstInit = (!cp);

        if(firstInit)
        {
            cp = (colorpalette_analysis_t *) M_Malloc(sizeof(*cp));
            tex.setAnalysisDataPointer(ClientTexture::ColorPaletteAnalysis, cp);
        }

        if(firstInit || forceUpdate)
            cp->paletteId = image.paletteId;
    }

    // Calculate a point light source for Dynlight and/or Halo?
    if(context == TC_SPRITE_DIFFUSE)
    {
        pointlight_analysis_t *pl = reinterpret_cast<pointlight_analysis_t*>(tex.analysisDataPointer(ClientTexture::BrightPointAnalysis));
        bool firstInit = (!pl);

        if(firstInit)
        {
            pl = (pointlight_analysis_t *) M_Malloc(sizeof *pl);
            tex.setAnalysisDataPointer(ClientTexture::BrightPointAnalysis, pl);
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
        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t*>(tex.analysisDataPointer(ClientTexture::AverageAlphaAnalysis));
        bool firstInit = (!aa);

        if(firstInit)
        {
            aa = (averagealpha_analysis_t *) M_Malloc(sizeof(*aa));
            tex.setAnalysisDataPointer(ClientTexture::AverageAlphaAnalysis, aa);
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
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(ClientTexture::AverageColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(ClientTexture::AverageColorAnalysis, ac);
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
                                    App_Resources().colorPalettes().colorPalette(image.paletteId),
                                    false, &ac->color);
            }
        }
    }

    // Amplified average color for plane glow?
    if(context == TC_MAPSURFACE_DIFFUSE)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(ClientTexture::AverageColorAmplifiedAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(ClientTexture::AverageColorAmplifiedAnalysis, ac);
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
                                    App_Resources().colorPalettes().colorPalette(image.paletteId),
                                    false, &ac->color);
            }
            Vec3f color(ac->color.rgb);
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
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(ClientTexture::AverageTopColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(ClientTexture::AverageTopColorAnalysis, ac);
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
                                        App_Resources().colorPalettes().colorPalette(image.paletteId),
                                        false, &ac->color);
            }
        }
    }

    // Average bottom line color for sky sphere fadeout?
    if(context == TC_SKYSPHERE_DIFFUSE)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(ClientTexture::AverageBottomColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            tex.setAnalysisDataPointer(ClientTexture::AverageBottomColorAnalysis, ac);
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
                                        App_Resources().colorPalettes().colorPalette(image.paletteId),
                                        false, &ac->color);
            }
        }
    }
}

uint ClientTexture::Variant::prepare()
{
    // Have we already prepared this?
    if(isPrepared())
        return d->glTexName;

    LOG_AS("TextureVariant::prepare");

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
    if ((c.flags & TXCF_UPLOAD_ARG_NOSTRETCH) &&
        (c.flags & TXCF_MIPMAP))
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
    gfx::UploadMethod uploadMethod = GL_ChooseUploadMethod(&c);
    GL_UploadTextureContent(c, uploadMethod);

    LOGDEV_RES_XVERBOSE("Prepared \"%s\" variant (glName:%u)%s",
                        d->texture.manifest().composeUri() << uint(d->glTexName) <<
                        (uploadMethod == gfx::Immediate? " while not busy!" : ""));
    LOGDEV_RES_XVERBOSE("  Content: %s", Image_Description(image));
    LOGDEV_RES_XVERBOSE("  Specification %p: %s", &d->spec << d->spec.asText());

    // Are we setting the logical dimensions to the pixel dimensions
    // of the source image?
    if(d->texture.width() == 0 && d->texture.height() == 0)
    {
        LOG_RES_XVERBOSE("World dimensions for \"%s\" taken from image pixels %s",
                         d->texture.manifest().composeUri() << image.size.asText());

        d->texture.setDimensions(image.size);
    }

    // We're done with the image data.
    Image_ClearPixelData(image);

    return d->glTexName;
}

void ClientTexture::Variant::release()
{
    if (isPrepared())
    {
        Deferred_glDeleteTextures(1, (const GLuint *) &d->glTexName);
        d->glTexName = 0;
    }
}

ClientTexture &ClientTexture::Variant::base() const
{
    return d->texture;
}

const TextureVariantSpec &ClientTexture::Variant::spec() const
{
    return d->spec;
}

res::Source ClientTexture::Variant::source() const
{
    return d->texSource;
}

String ClientTexture::Variant::sourceDescription() const
{
    if(d->texSource == res::Original) return "original";
    if(d->texSource == res::External) return "external";
    return "none";
}

Flags ClientTexture::Variant::flags() const
{
    return d->flags;
}

void ClientTexture::Variant::glCoords(float *outS, float *outT) const
{
    if(outS) *outS = d->s;
    if(outT) *outT = d->t;
}

uint ClientTexture::Variant::glName() const
{
    return d->glTexName;
}
