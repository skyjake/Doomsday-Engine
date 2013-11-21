/** @file texturecontent.cpp  GL-texture content.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "gl/texturecontent.h"

#include "de_console.h"
#include "dd_def.h" // texGamma
#include "dd_main.h" // App_ResourceSystem()
#include "gl/gl_main.h"
#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h" // misc global vars awaiting new home
#include <de/memory.h>
#include <cstring>

using namespace de;

static int BytesPerPixelFmt(dgltexformat_t format)
{
    switch(format)
    {
    case DGL_LUMINANCE:
    case DGL_COLOR_INDEX_8:         return 1;

    case DGL_LUMINANCE_PLUS_A8:
    case DGL_COLOR_INDEX_8_PLUS_A8: return 2;

    case DGL_RGB:                   return 3;

    case DGL_RGBA:                  return 4;
    default:
        Con_Error("BytesPerPixelFmt: Unknown format %i, don't know pixel size.\n", format);
        return 0; // Unreachable.
    }
}

void GL_InitTextureContent(texturecontent_t *content)
{
    DENG_ASSERT(content);
    content->format = dgltexformat_t(0);
    content->name = 0;
    content->pixels = 0;
    content->paletteId = 0;
    content->width = 0;
    content->height = 0;
    content->minFilter = GL_LINEAR;
    content->magFilter = GL_LINEAR;
    content->anisoFilter = -1; // Best.
    content->wrap[0] = GL_CLAMP_TO_EDGE;
    content->wrap[1] = GL_CLAMP_TO_EDGE;
    content->grayMipmap = 0;
    content->flags = 0;
}

texturecontent_t *GL_ConstructTextureContentCopy(texturecontent_t const *other)
{
    DENG_ASSERT(other);

    texturecontent_t *c = (texturecontent_t*) M_Malloc(sizeof(*c));

    std::memcpy(c, other, sizeof(*c));

    // Duplicate the image buffer.
    int bytesPerPixel = BytesPerPixelFmt(other->format);
    size_t bufferSize = bytesPerPixel * other->width * other->height;
    uint8_t *pixels = (uint8_t*) M_Malloc(bufferSize);
    std::memcpy(pixels, other->pixels, bufferSize);
    c->pixels = pixels;
    return c;
}

void GL_DestroyTextureContent(texturecontent_t *content)
{
    DENG_ASSERT(content);
    if(content->pixels) M_Free((uint8_t *)content->pixels);
    M_Free(content);
}

/**
 * Prepares the image for use as a GL texture in accordance with the given
 * specification.
 *
 * @param image     The image to prepare (in place).
 * @param spec      Specification describing any transformations which should
 *                  be applied to the image.
 *
 * @return  The DGL texture format determined for the image.
 */
static dgltexformat_t prepareImageAsTexture(image_t &image,
    variantspecification_t const &spec)
{
    DENG_ASSERT(image.pixels);

    bool const monochrome = (spec.flags & TSF_MONOCHROME) != 0;
    bool const scaleSharp = (spec.flags & TSF_UPSCALE_AND_SHARPEN) != 0;

    if(spec.toAlpha)
    {
        if(0 != image.paletteId)
        {
            // Paletted.
            uint8_t *newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
                                                  ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                                                  image.paletteId, 3);
            M_Free(image.pixels);
            image.pixels = newPixels;
            image.pixelSize = 3;
            image.paletteId = 0;
            image.flags &= ~IMGF_IS_MASKED;
        }

        Image_ConvertToLuminance(&image, false);
        long total = image.size.width * image.size.height;
        for(long i = 0; i < total; ++i)
        {
            image.pixels[total + i] = image.pixels[i];
            image.pixels[i] = 255;
        }
        image.pixelSize = 2;
    }
    else if(0 != image.paletteId)
    {
        if(fillOutlines && (image.flags & IMGF_IS_MASKED))
        {
            ColorOutlinesIdx(image.pixels, image.size.width, image.size.height);
        }

        if(monochrome && !scaleSharp)
        {
            GL_DeSaturatePalettedImage(image.pixels,
                                       App_ResourceSystem().colorPalette(image.paletteId),
                                       image.size.width, image.size.height);
        }

        if(scaleSharp)
        {
            int scaleMethod = GL_ChooseSmartFilter(image.size.width, image.size.height, 0);
            bool origMasked = (image.flags & IMGF_IS_MASKED) != 0;
            colorpaletteid_t origPaletteId = image.paletteId;

            uint8_t *newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
                                                  ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                                                  image.paletteId, 4);
            if(newPixels != image.pixels)
            {
                M_Free(image.pixels);
                image.pixels = newPixels;
                image.pixelSize = 4;
                image.paletteId = 0;
                image.flags &= ~IMGF_IS_MASKED;
            }

            if(monochrome)
                Desaturate(image.pixels, image.size.width, image.size.height, image.pixelSize);

            newPixels = GL_SmartFilter(scaleMethod, image.pixels, image.size.width, image.size.height,
                                       0, &image.size.width, &image.size.height);
            if(newPixels != image.pixels)
            {
                M_Free(image.pixels);
                image.pixels = newPixels;
            }

            EnhanceContrast(image.pixels, image.size.width, image.size.height, image.pixelSize);
            //SharpenPixels(image.pixels, image.size.width, image.size.height, image.pixelSize);
            //BlackOutlines(image.pixels, image.size.width, image.size.height, image.pixelSize);

            // Back to paletted+alpha?
            if(monochrome)
            {
                // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                Image_ConvertToLuminance(&image, true);
                AmplifyLuma(image.pixels, image.size.width, image.size.height, image.pixelSize == 2);
            }
            else
            {   // Yes. Quantize down from RGA(+A) to Paletted(+A), replacing the old image.
                newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
                                             (origMasked? 2 : 1),
                                             origPaletteId, 4);

                if(newPixels != image.pixels)
                {
                    M_Free(image.pixels);
                    image.pixels = newPixels;
                    image.pixelSize = (origMasked? 2 : 1);
                    image.paletteId = origPaletteId;
                    if(origMasked)
                    {
                        image.flags |= IMGF_IS_MASKED;
                    }
                }
            }
        }
    }
    else if(image.pixelSize > 2)
    {
        if(monochrome)
        {
            Image_ConvertToLuminance(&image, true);
            AmplifyLuma(image.pixels, image.size.width, image.size.height, image.pixelSize == 2);
        }
    }

    /*
     * Choose the final GL texture format.
     */
    if(monochrome)
    {
        return image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE;
    }
    if(image.paletteId)
    {
        return (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8;
    }
    return   image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8
           : image.pixelSize == 3 ? DGL_RGB
           : image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE;
}

/**
 * Prepares the image for use as a detail GL texture in accordance with the
 * given specification.
 *
 * @param image     The image to prepare (in place).
 * @param spec      Specification describing any transformations which should
 *                  be applied to the image.
 *
 * Return values:
 * @param baMul     Luminance equalization balance factor is written here.
 * @param hiMul     Luminance equalization white-shift factor is written here.
 * @param loMul     Luminance equalization black-shift factor is written here.
 *
 * @return  The DGL texture format determined for the image.
 */
static dgltexformat_t prepareImageAsDetailTexture(image_t &image,
    detailvariantspecification_t const &spec, float *baMul, float *hiMul, float *loMul)
{
    DENG_UNUSED(spec);

    // We want a luminance map.
    if(image.pixelSize > 2)
    {
        Image_ConvertToLuminance(&image, false);
    }

    // Try to normalize the luminance data so it works expectedly as a detail texture.
    EqualizeLuma(image.pixels, image.size.width, image.size.height, baMul, hiMul, loMul);

    return DGL_LUMINANCE;
}

void GL_PrepareTextureContent(texturecontent_t &c, DGLuint glTexName,
    image_t &image, texturevariantspecification_t const &spec,
    TextureManifest const &textureManifest)
{
    DENG_ASSERT(glTexName != 0);
    DENG_ASSERT(image.pixels != 0);

    // Initialize and assign a GL name to the content.
    GL_InitTextureContent(&c);
    c.name = glTexName;

    switch(spec.type)
    {
    case TST_GENERAL: {
        variantspecification_t const &vspec = TS_GENERAL(spec);
        bool const noCompression = (vspec.flags & TSF_NO_COMPRESSION) != 0;
        // If the Upscale And Sharpen filter is enabled, scaling is applied
        // implicitly by prepareImageAsTexture(), so don't do it again.
        bool const noSmartFilter = (vspec.flags & TSF_UPSCALE_AND_SHARPEN) != 0;

        // Prepare the image for upload.
        dgltexformat_t dglFormat = prepareImageAsTexture(image, vspec);

        // Configure the texture content.
        c.format      = dglFormat;
        c.width       = image.size.width;
        c.height      = image.size.height;
        c.pixels      = image.pixels;
        c.paletteId   = image.paletteId;

        if(noCompression || (image.size.width < 128 || image.size.height < 128))
            c.flags |= TXCF_NO_COMPRESSION;
        if(vspec.gammaCorrection) c.flags |= TXCF_APPLY_GAMMACORRECTION;
        if(vspec.noStretch)       c.flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
        if(vspec.mipmapped)       c.flags |= TXCF_MIPMAP;
        if(noSmartFilter)         c.flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

        c.magFilter   = vspec.glMagFilter();
        c.minFilter   = vspec.glMinFilter();
        c.anisoFilter = vspec.logicalAnisoLevel();
        c.wrap[0]     = vspec.wrapS;
        c.wrap[1]     = vspec.wrapT;
        break; }

    case TST_DETAIL: {
        detailvariantspecification_t const &dspec = TS_DETAIL(spec);

        // Prepare the image for upload.
        float baMul, hiMul, loMul;
        dgltexformat_t dglFormat = prepareImageAsDetailTexture(image, dspec, &baMul, &hiMul, &loMul);

        // Determine the gray mipmap factor.
        int grayMipmapFactor = dspec.contrast;
        if(baMul != 1 || hiMul != 1 || loMul != 1)
        {
            // Integrate the normalization factor with contrast.
            float const hiContrast = 1 - 1. / hiMul;
            float const loContrast = 1 - loMul;
            float const shift = ((hiContrast + loContrast) / 2);

            grayMipmapFactor = int(255 * de::clamp(0.f, dspec.contrast / 255.f - shift, 1.f));

            // Announce the normalization.
            de::Uri uri = textureManifest.composeUri();
            LOG_VERBOSE("Normalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).")
                << uri << baMul << hiMul << loMul;
        }

        // Configure the texture content.
        c.format      = dglFormat;
        c.flags       = TXCF_GRAY_MIPMAP | TXCF_UPLOAD_ARG_NOSMARTFILTER;

        // Disable compression?
        if(image.size.width < 128 || image.size.height < 128)
            c.flags |= TXCF_NO_COMPRESSION;

        c.grayMipmap  = grayMipmapFactor;
        c.width       = image.size.width;
        c.height      = image.size.height;
        c.pixels      = image.pixels;
        c.anisoFilter = texAniso;
        c.magFilter   = glmode[texMagMode];
        c.minFilter   = GL_LINEAR_MIPMAP_LINEAR;
        c.wrap[0]     = GL_REPEAT;
        c.wrap[1]     = GL_REPEAT;
        break; }

    default:
        // Invalid spec type.
        DENG_ASSERT(false);
    }
}

/**
 * Choose an internal texture format.
 *
 * @param format  DGL texture format identifier.
 * @param allowCompression  @c true == use compression if available.
 * @return  The chosen texture format.
 */
static GLint ChooseTextureFormat(dgltexformat_t format, boolean allowCompression)
{
    boolean compress = (allowCompression && GL_state.features.texCompression);

    switch(format)
    {
    case DGL_RGB:
    case DGL_COLOR_INDEX_8:
        if(!compress)
            return GL_RGB8;
#if USE_TEXTURE_COMPRESSION_S3
        if(GL_state.extensions.texCompressionS3)
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
#endif
        return GL_COMPRESSED_RGB;

    case DGL_RGBA:
    case DGL_COLOR_INDEX_8_PLUS_A8:
        if(!compress)
            return GL_RGBA8;
#if USE_TEXTURE_COMPRESSION_S3
        if(GL_state.extensions.texCompressionS3)
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#endif
        return GL_COMPRESSED_RGBA;

    case DGL_LUMINANCE:
        return !compress ? GL_LUMINANCE : GL_COMPRESSED_LUMINANCE;

    case DGL_LUMINANCE_PLUS_A8:
        return !compress ? GL_LUMINANCE_ALPHA : GL_COMPRESSED_LUMINANCE_ALPHA;

    default:
        Con_Error("ChooseTextureFormat: Invalid source format %i.", (int) format);
        return 0; // Unreachable.
    }
}

/// @note Texture parameters will NOT be set here!
void GL_UploadTextureContent(texturecontent_t const &content, gl::UploadMethod method)
{
    if(method == gl::Deferred)
    {
        GL_DeferTextureUpload(&content);
        return;
    }

    if(novideo) return;

    // Do this right away. No need to take a copy.
    bool generateMipmaps = (content.flags & (TXCF_MIPMAP|TXCF_GRAY_MIPMAP)) != 0;
    bool applyTexGamma   = (content.flags & TXCF_APPLY_GAMMACORRECTION)     != 0;
    bool noCompression   = (content.flags & TXCF_NO_COMPRESSION)            != 0;
    bool noSmartFilter   = (content.flags & TXCF_UPLOAD_ARG_NOSMARTFILTER)  != 0;
    bool noStretch       = (content.flags & TXCF_UPLOAD_ARG_NOSTRETCH)      != 0;

    int loadWidth = content.width, loadHeight = content.height;
    uint8_t const *loadPixels = content.pixels;
    dgltexformat_t dglFormat = content.format;

    if(DGL_COLOR_INDEX_8 == dglFormat || DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat)
    {
        // Convert a paletted source image to truecolor.
        uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight,
                                              DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 2 : 1,
                                              content.paletteId,
                                              DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 4 : 3);
        if(loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = newPixels;
        dglFormat = DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? DGL_RGBA : DGL_RGB;
    }

    if(DGL_RGBA == dglFormat || DGL_RGB == dglFormat)
    {
        int comps = (DGL_RGBA == dglFormat ? 4 : 3);

        if(applyTexGamma && texGamma > .0001f)
        {
            uint8_t* dst, *localBuffer = 0;
            long const numPels = loadWidth * loadHeight;

            uint8_t const *src = loadPixels;
            if(loadPixels == content.pixels)
            {
                localBuffer = (uint8_t *) M_Malloc(comps * numPels);
                dst = localBuffer;
            }
            else
            {
                dst = const_cast<uint8_t *>(loadPixels);
            }

            for(long i = 0; i < numPels; ++i)
            {
                dst[CR] = texGammaLut[src[CR]];
                dst[CG] = texGammaLut[src[CG]];
                dst[CB] = texGammaLut[src[CB]];
                if(comps == 4)
                    dst[CA] = src[CA];

                dst += comps;
                src += comps;
            }

            if(localBuffer)
            {
                if(loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = localBuffer;
            }
        }

        if(useSmartFilter && !noSmartFilter)
        {
            if(comps == 3)
            {
                // Need to add an alpha channel.
                uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight, 3, 0, 4);
                if(loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = newPixels;
                dglFormat = DGL_RGBA;
            }

            uint8_t *filtered = GL_SmartFilter(GL_ChooseSmartFilter(loadWidth, loadHeight, 0),
                                               loadPixels, loadWidth, loadHeight,
                                               ICF_UPSCALE_SAMPLE_WRAP,
                                               &loadWidth, &loadHeight);
            if(filtered != loadPixels)
            {
                if(loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = filtered;
            }
        }
    }

    if(DGL_LUMINANCE_PLUS_A8 == dglFormat)
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(2 * numPixels);

        uint8_t *pixel = localBuffer;
        for(long i = 0; i < numPixels; ++i)
        {
            pixel[0] = loadPixels[i];
            pixel[1] = loadPixels[numPixels + i];
            pixel += 2;
        }

        if(loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
    }

    if(DGL_LUMINANCE == dglFormat && (content.flags & TXCF_CONVERT_8BIT_TO_ALPHA))
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(2 * numPixels);

        // Move the average color to the alpha channel, make the actual color white.
        uint8_t *pixel = localBuffer;
        for(long i = 0; i < numPixels; ++i)
        {
            pixel[0] = 255;
            pixel[1] = loadPixels[i];
            pixel += 2;
        }

        if(loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
        dglFormat = DGL_LUMINANCE_PLUS_A8;
    }

    // Calculate the final dimensions for the texture, as required by
    // the graphics hardware and/or engine configuration.
    int width = loadWidth, height = loadHeight;

    noStretch = GL_OptimalTextureSize(width, height, noStretch, generateMipmaps,
                                      &loadWidth, &loadHeight);

    // Do we need to resize?
    if(width != loadWidth || height != loadHeight)
    {
        int comps = BytesPerPixelFmt(dglFormat);

        if(noStretch)
        {
            // Copy the texture into a power-of-two canvas.
            uint8_t *localBuffer = (uint8_t *) M_Calloc(comps * loadWidth * loadHeight);

            // Copy line by line.
            for(int i = 0; i < height; ++i)
            {
                std::memcpy(localBuffer + loadWidth * comps * i,
                            loadPixels  + width     * comps * i, comps * width);
            }

            if(loadPixels != content.pixels)
            {
                M_Free(const_cast<uint8_t *>(loadPixels));
            }
            loadPixels = localBuffer;
        }
        else
        {
            // Stretch into a new power-of-two texture.
            uint8_t *newPixels = GL_ScaleBuffer(loadPixels, width, height, comps,
                                                loadWidth, loadHeight);
            if(loadPixels != content.pixels)
            {
                M_Free(const_cast<uint8_t *>(loadPixels));
            }
            loadPixels = newPixels;
        }
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, content.name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content.magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     content.wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     content.wrap[1]);
    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(content.anisoFilter));

    if(!(content.flags & TXCF_GRAY_MIPMAP))
    {
        GLint loadFormat;
        switch(dglFormat)
        {
        case DGL_LUMINANCE_PLUS_A8: loadFormat = GL_LUMINANCE_ALPHA; break;
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE;       break;
        case DGL_RGB:               loadFormat = GL_RGB;             break;
        case DGL_RGBA:              loadFormat = GL_RGBA;            break;
        default:
            throw Error("GL_UploadTextureContent", QString("Unknown format %1").arg(int(dglFormat)));
        }

        GLint glFormat = ChooseTextureFormat(dglFormat, !noCompression);

        if(!GL_UploadTexture(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                             generateMipmaps ? true : false))
        {
            throw Error("GL_UploadTextureContent", QString("TexImage failed (%1:%2 fmt%3)")
                                                       .arg(content.name)
                                                       .arg(Vector2i(loadWidth, loadHeight).asText())
                                                       .arg(int(dglFormat)));
        }
    }
    else
    {
        // Special fade-to-gray luminance texture (used for details).
        GLint glFormat, loadFormat;

        switch(dglFormat)
        {
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB;       break;
        default:
            throw Error("GL_UploadTextureContent", QString("Unknown format %1").arg(int(dglFormat)));
        }

        glFormat = ChooseTextureFormat(DGL_LUMINANCE, !noCompression);

        if(!GL_UploadTextureGrayMipmap(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                                       content.grayMipmap * reciprocal255))
        {
            throw Error("GL_UploadTextureContent", QString("TexImageGrayMipmap failed (%1:%2 fmt%3)")
                                                       .arg(content.name)
                                                       .arg(Vector2i(loadWidth, loadHeight).asText())
                                                       .arg(int(dglFormat)));
        }
    }

    if(loadPixels != content.pixels)
    {
        M_Free(const_cast<uint8_t *>(loadPixels));
    }
}
