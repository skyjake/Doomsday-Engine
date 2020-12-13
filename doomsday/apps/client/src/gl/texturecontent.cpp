/** @file texturecontent.cpp  GL-texture content.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include <cstring>
#include <doomsday/resource/colorpalettes.h>
#include <de/concurrency.h>
#include <de/memory.h>
#include <de/GLInfo>
#include <de/texgamma.h>
#include "dd_def.h"  // texGamma
#include "dd_main.h"  // App_Resources()
#include "sys_system.h"

#include "gl/gl_main.h"
#include "gl/gl_tex.h"

#include "render/rend_main.h"  // misc global vars awaiting new home

using namespace de;

static int BytesPerPixelFmt(dgltexformat_t format)
{
    switch (format)
    {
    case DGL_LUMINANCE:
    case DGL_COLOR_INDEX_8:         return 1;

    case DGL_LUMINANCE_PLUS_A8:
    case DGL_COLOR_INDEX_8_PLUS_A8: return 2;

    case DGL_RGB:                   return 3;

    case DGL_RGBA:                  return 4;
    default:
        App_Error("BytesPerPixelFmt: Unknown format %i, don't know pixel size.\n", format);
        return 0; // Unreachable.
    }
}

#if 0
/**
 * Given a pixel format return the number of bytes to store one pixel.
 * @pre Input data is of GL_UNSIGNED_BYTE type.
 */
static int BytesPerPixel(GLint format)
{
    switch (format)
    {
    case GL_COLOR_INDEX:
    case GL_STENCIL_INDEX:
    case GL_DEPTH_COMPONENT:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:              return 1;

    case GL_LUMINANCE_ALPHA:        return 2;

    case GL_RGB:
    case GL_RGB8:
    case GL_BGR:                    return 3;

    case GL_RGBA:
    case GL_RGBA8:
    case GL_BGRA:                   return 4;

    default:
        App_Error("BytesPerPixel: Unknown format %i.", (int) format);
        return 0; // Unreachable.
    }
}
#endif

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
    if (content->pixels) M_Free((uint8_t *)content->pixels);
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

    if (spec.toAlpha)
    {
        if (0 != image.paletteId)
        {
            // Paletted.
            uint8_t *newPixels = GL_ConvertBuffer(image.pixels, image.size.x, image.size.y,
                                                  ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                                                  image.paletteId, 3);
            M_Free(image.pixels);
            image.pixels = newPixels;
            image.pixelSize = 3;
            image.paletteId = 0;
            image.flags &= ~IMGF_IS_MASKED;
        }

        Image_ConvertToLuminance(image, false /*discard alpha*/);
        long total = image.size.x * image.size.y;
        for (long i = 0; i < total; ++i)
        {
            image.pixels[total + i] = image.pixels[i];
            image.pixels[i] = 255;
        }
        image.pixelSize = 2;
    }
    else if (image.paletteId)
    {
        if (fillOutlines && (image.flags & IMGF_IS_MASKED))
        {
            ColorOutlinesIdx(image.pixels, image.size.x, image.size.y);
        }

        if (monochrome && !scaleSharp)
        {
            GL_DeSaturatePalettedImage(image.pixels,
                                       App_Resources().colorPalettes().colorPalette(image.paletteId),
                                       image.size.x, image.size.y);
        }

        if (scaleSharp)
        {
            int scaleMethod = GL_ChooseSmartFilter(image.size.x, image.size.y, 0);
            bool origMasked = (image.flags & IMGF_IS_MASKED) != 0;
            colorpaletteid_t origPaletteId = image.paletteId;

            uint8_t *newPixels = GL_ConvertBuffer(image.pixels, image.size.x, image.size.y,
                                                  ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                                                  image.paletteId, 4);
            if (newPixels != image.pixels)
            {
                M_Free(image.pixels);
                image.pixels = newPixels;
                image.pixelSize = 4;
                image.paletteId = 0;
                image.flags &= ~IMGF_IS_MASKED;
            }

            if (monochrome)
            {
                Desaturate(image.pixels, image.size.x, image.size.y, image.pixelSize);
            }

            int newWidth = 0, newHeight = 0;
            newPixels = GL_SmartFilter(scaleMethod, image.pixels, image.size.x, image.size.y,
                                       0, &newWidth, &newHeight);
            image.size = Vector2ui(newWidth, newHeight);
            if (newPixels != image.pixels)
            {
                M_Free(image.pixels);
                image.pixels = newPixels;
            }

            EnhanceContrast(image.pixels, image.size.x, image.size.y, image.pixelSize);
            //SharpenPixels(image.pixels, image.size.x, image.size.y, image.pixelSize);
            //BlackOutlines(image.pixels, image.size.x, image.size.y, image.pixelSize);

            // Back to paletted+alpha?
            if (monochrome)
            {
                // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                Image_ConvertToLuminance(image);
                AmplifyLuma(image.pixels, image.size.x, image.size.y, image.pixelSize == 2);
            }
            else
            {   // Yes. Quantize down from RGA(+A) to Paletted(+A), replacing the old image.
                newPixels = GL_ConvertBuffer(image.pixels, image.size.x, image.size.y,
                                             (origMasked? 2 : 1),
                                             origPaletteId, 4);

                if (newPixels != image.pixels)
                {
                    M_Free(image.pixels);
                    image.pixels = newPixels;
                    image.pixelSize = (origMasked? 2 : 1);
                    image.paletteId = origPaletteId;
                    if (origMasked)
                    {
                        image.flags |= IMGF_IS_MASKED;
                    }
                }
            }
        }
    }
    else if (image.pixelSize > 2)
    {        
        if (fillOutlines && image.pixelSize == 4)
        {
            ColorOutlinesRGBA(image.pixels, image.size.x, image.size.y);
        }

        if (monochrome)
        {
            Image_ConvertToLuminance(image);
            AmplifyLuma(image.pixels, image.size.x, image.size.y, image.pixelSize == 2);
        }
    }

    /*
     * Choose the final GL texture format.
     */
    if (monochrome)
    {
        return image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE;
    }
    if (image.paletteId)
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
    if (image.pixelSize > 2)
    {
        Image_ConvertToLuminance(image, false /*discard alpha*/);
    }

    // Try to normalize the luminance data so it works expectedly as a detail texture.
    EqualizeLuma(image.pixels, image.size.x, image.size.y, baMul, hiMul, loMul);

    return DGL_LUMINANCE;
}

void GL_PrepareTextureContent(texturecontent_t &c,
                              GLuint glTexName,
                              image_t &image,
                              TextureVariantSpec const &spec,
                              res::TextureManifest const &textureManifest)
{
    DENG_ASSERT(glTexName != 0);
    DENG_ASSERT(image.pixels != 0);

    // Initialize and assign a GL name to the content.
    GL_InitTextureContent(&c);
    c.name = glTexName;

    switch (spec.type)
    {
    case TST_GENERAL: {
        variantspecification_t const &vspec = spec.variant;
        bool const noCompression = (vspec.flags & TSF_NO_COMPRESSION) != 0;
        // If the Upscale And Sharpen filter is enabled, scaling is applied
        // implicitly by prepareImageAsTexture(), so don't do it again.
        bool const noSmartFilter = (vspec.flags & TSF_UPSCALE_AND_SHARPEN) != 0;

        // Prepare the image for upload.
        dgltexformat_t dglFormat = prepareImageAsTexture(image, vspec);

        // Configure the texture content.
        c.format      = dglFormat;
        c.width       = image.size.x;
        c.height      = image.size.y;
        c.pixels      = image.pixels;
        c.paletteId   = image.paletteId;

        if (noCompression || (image.size.x < 128 || image.size.y < 128))
            c.flags |= TXCF_NO_COMPRESSION;
        if (vspec.gammaCorrection) c.flags |= TXCF_APPLY_GAMMACORRECTION;
        if (vspec.noStretch)       c.flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
        if (vspec.mipmapped)       c.flags |= TXCF_MIPMAP;
        if (noSmartFilter)         c.flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

        c.magFilter   = vspec.glMagFilter();
        c.minFilter   = vspec.glMinFilter();
        c.anisoFilter = vspec.logicalAnisoLevel();
        c.wrap[0]     = vspec.wrapS;
        c.wrap[1]     = vspec.wrapT;
        break; }

    case TST_DETAIL: {
        detailvariantspecification_t const &dspec = spec.detailVariant;

        // Prepare the image for upload.
        float baMul, hiMul, loMul;
        dgltexformat_t dglFormat = prepareImageAsDetailTexture(image, dspec, &baMul, &hiMul, &loMul);

        // Determine the gray mipmap factor.
        int grayMipmapFactor = dspec.contrast;
        if (baMul != 1 || hiMul != 1 || loMul != 1)
        {
            // Integrate the normalization factor with contrast.
            float const hiContrast = 1 - 1. / hiMul;
            float const loContrast = 1 - loMul;
            float const shift = ((hiContrast + loContrast) / 2);

            grayMipmapFactor = int(255 * de::clamp(0.f, dspec.contrast / 255.f - shift, 1.f));

            // Announce the normalization.
            de::Uri uri = textureManifest.composeUri();
            LOG_GL_VERBOSE("Normalized detail texture \"%s\" (balance: %f, high amp: %f, low amp: %f)")
                << uri << baMul << hiMul << loMul;
        }

        // Configure the texture content.
        c.format      = dglFormat;
        c.flags       = TXCF_GRAY_MIPMAP | TXCF_UPLOAD_ARG_NOSMARTFILTER;

        // Disable compression?
        if (image.size.x < 128 || image.size.y < 128)
            c.flags |= TXCF_NO_COMPRESSION;

        c.grayMipmap  = grayMipmapFactor;
        c.width       = image.size.x;
        c.height      = image.size.y;
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
static GLint ChooseTextureFormat(dgltexformat_t format, dd_bool allowCompression)
{
#if defined (DENG_OPENGL_ES)
    
    switch (format)
    {
    case DGL_RGB:
    case DGL_COLOR_INDEX_8:
        return GL_RGB8;
        
    case DGL_RGBA:
    case DGL_COLOR_INDEX_8_PLUS_A8:
        return GL_RGBA8;

    default:
        DENG2_ASSERT(!"ChooseTextureFormat: Invalid texture source format");
        return 0;
    }

#else
    
    dd_bool compress = (allowCompression && GL_state.features.texCompression);
    
    switch (format)
    {
    case DGL_RGB:
    case DGL_COLOR_INDEX_8:
        if (!compress)
            return GL_RGB8;
#if USE_TEXTURE_COMPRESSION_S3
        if (GLInfo::extensions().EXT_texture_compression_s3tc)
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
#endif
        return GL_COMPRESSED_RGB;

    case DGL_RGBA:
    case DGL_COLOR_INDEX_8_PLUS_A8:
        if (!compress)
            return GL_RGBA8;
#if USE_TEXTURE_COMPRESSION_S3
        if (GLInfo::extensions().EXT_texture_compression_s3tc)
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#endif
        return GL_COMPRESSED_RGBA;

    /*case DGL_LUMINANCE:
        return !compress ? GL_LUMINANCE : GL_COMPRESSED_LUMINANCE;

    case DGL_LUMINANCE_PLUS_A8:
        return !compress ? GL_LUMINANCE_ALPHA : GL_COMPRESSED_LUMINANCE_ALPHA;*/

    default:
        DENG2_ASSERT(!"ChooseTextureFormat: Invalid texture source format");
        return 0; // Unreachable.
    }
    
#endif
}

/**
 * @param glFormat  Identifier of the desired GL texture format.
 * @param loadFormat  Identifier of the GL texture format used during upload.
 * @param pixels  Texture pixel data to be uploaded.
 * @param width  Width of the texture in pixels.
 * @param height  Height of the texture in pixels.
 * @param genMipmaps  If negative sets a specific mipmap level, e.g.:
 *      @c -1, means mipmap level 1.
 *
 * @return  @c true iff successful.
 */
static dd_bool uploadTexture(int glFormat, int loadFormat, const uint8_t* pixels,
    int width,  int height, int genMipmaps)
{
    GLenum const properties[8] =
    {
       GL_PACK_ROW_LENGTH,
       GL_PACK_ALIGNMENT,
       GL_PACK_SKIP_ROWS,
       GL_PACK_SKIP_PIXELS,
       GL_UNPACK_ROW_LENGTH,
       GL_UNPACK_ALIGNMENT,
       GL_UNPACK_SKIP_ROWS,
       GL_UNPACK_SKIP_PIXELS,
    };

    const int packRowLength = 0, packAlignment = 1, packSkipRows = 0, packSkipPixels = 0;
    const int unpackRowLength = 0, unpackAlignment = 1, unpackSkipRows = 0, unpackSkipPixels = 0;
    int mipLevel = 0;
    DENG_ASSERT(pixels);

    if (!(GL_LUMINANCE_ALPHA == loadFormat || GL_LUMINANCE == loadFormat ||
         GL_RGB == loadFormat || GL_RGBA == loadFormat))
    {
        throw Error("texturecontent_t::uploadTexture", "Unsupported load format " + String::number(loadFormat));
    }

    // Can't operate on null texture.
    if (width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if (width > GLInfo::limits().maxTexSize || height > GLInfo::limits().maxTexSize)
        return false;

    // Negative indices signify a specific mipmap level is being uploaded.
    if (genMipmaps < 0)
    {
        mipLevel = -genMipmaps;
        genMipmaps = 0;
    }

    //DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    auto &GL = LIBGUI_GL;

    // Automatic mipmap generation?
    /*if (genMipmaps)
    {
        GL.glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        LIBGUI_ASSERT_GL_OK();
    }*/

    //LIBGUI_GL.glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    GLint oldPixelStore[8];
    for (int i = 0; i < 8; ++i)
    {
        GL.glGetIntegerv(properties[i], &oldPixelStore[i]);
        LIBGUI_ASSERT_GL_OK();
    }

    GL.glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)packRowLength);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_PACK_ALIGNMENT, (GLint)packAlignment);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_PACK_SKIP_ROWS, (GLint)packSkipRows);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_PACK_SKIP_PIXELS, (GLint)packSkipPixels);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)unpackRowLength);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_UNPACK_ALIGNMENT, (GLint)unpackAlignment);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_UNPACK_SKIP_ROWS, (GLint)unpackSkipRows);
    LIBGUI_ASSERT_GL_OK();
    GL.glPixelStorei(GL_UNPACK_SKIP_PIXELS, (GLint)unpackSkipPixels);
    LIBGUI_ASSERT_GL_OK();

#if 0
    if (genMipmaps && !GLInfo::extensions().SGIS_generate_mipmap)
    {   // Build all mipmap levels.
        int neww, newh, bpp, w, h;
        void* image, *newimage;

        bpp = BytesPerPixel(loadFormat);
        if (bpp == 0)
            throw Error("texturecontent_t::uploadTexture", "Unknown GL format " + String::number(loadFormat));

        GL_OptimalTextureSize(width, height, false, true, &w, &h);

        if (w != width || h != height)
        {
            // Must rescale image to get "top" mipmap texture image.
            image = GL_ScaleBufferEx(pixels, width, height, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                w, h, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment, packSkipRows,
                packSkipPixels);
            if (!image)
                throw Error("texturecontent_t::uploadTexture", "Unknown error resizing mipmap level #0");
        }
        else
        {
            image = (void*) pixels;
        }

        for (;;)
        {
            LIBGUI_GL.glTexImage2D(GL_TEXTURE_2D, mipLevel, (GLint)glFormat, w, h, 0, (GLint)loadFormat,
                GL_UNSIGNED_BYTE, image);

            if (w == 1 && h == 1)
                break;

            ++mipLevel;
            neww = (w < 2) ? 1 : w / 2;
            newh = (h < 2) ? 1 : h / 2;
            newimage = GL_ScaleBufferEx(image, w, h, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                neww, newh, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment,
                packSkipRows, packSkipPixels);
            if (!newimage)
                throw Error("texturecontent_t::uploadTexture", "Unknown error resizing mipmap level #" + String::number(mipLevel));

            if (image != pixels)
                M_Free(image);
            image = newimage;

            w = neww;
            h = newh;
        }

        if (image != pixels)
            M_Free(image);
    }
    else
#endif
    {
        LIBGUI_GL.glTexImage2D(GL_TEXTURE_2D, mipLevel, GLint(glFormat),
                               GLsizei(width), GLsizei(height), 0,
                               GLenum(loadFormat), GL_UNSIGNED_BYTE, pixels);
        LIBGUI_ASSERT_GL_OK();

        if (genMipmaps)
        {
            LIBGUI_GL.glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    //LIBGUI_GL.glPopClientAttrib();

    for (int i = 0; i < 8; ++i)
    {
        GL.glPixelStorei(properties[i], oldPixelStore[i]);
        LIBGUI_ASSERT_GL_OK();
    }

    DENG_ASSERT(!Sys_GLCheckError());

    return true;
}

/**
 * @param glFormat  Identifier of the desired GL texture format.
 * @param loadFormat  Identifier of the GL texture format used during upload.
 * @param pixels  Texture pixel data to be uploaded.
 * @param width  Width of the texture in pixels.
 * @param height  Height of the texture in pixels.
 * @param grayFactor  Strength of the blend where @c 0:none @c 1:full.
 *
 * @return  @c true iff successful.
 */
static dd_bool uploadTextureGrayMipmap(int glFormat, int loadFormat, const uint8_t* pixels,
    int width, int height, float grayFactor)
{
    int i, w, h, numpels = width * height, numLevels, pixelSize;
    uint8_t* image, *faded, *out;
    const uint8_t* in;
    float invFactor;
    DENG_ASSERT(pixels);

    if (!(GL_RGB == loadFormat || GL_LUMINANCE == loadFormat))
    {
        throw Error("texturecontent_t::uploadTextureGrayMipmap", "Unsupported load format " + String::number(loadFormat));
    }

    pixelSize = (loadFormat == GL_LUMINANCE? 1 : 3);

    // Can't operate on null texture.
    if (width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if (width > GLInfo::limits().maxTexSize || height > GLInfo::limits().maxTexSize)
        return false;

    numLevels = GL_NumMipmapLevels(width, height);
    grayFactor = MINMAX_OF(0, grayFactor, 1);
    invFactor = 1 - grayFactor;

    // Buffer used for the faded texture.
    faded = (uint8_t*) M_Malloc(numpels / 4);
    image = (uint8_t*) M_Malloc(numpels);

    // Initial fading.
    in = pixels;
    out = image;
    for (i = 0; i < numpels; ++i)
    {
        *out++ = (uint8_t) MINMAX_OF(0, (*in * grayFactor + 127 * invFactor), 255);
        in += pixelSize;
    }

    // Upload the first level right away.
    LIBGUI_GL.glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, (GLint)loadFormat,
        GL_UNSIGNED_BYTE, image);

    // Generate all mipmaps levels.
    w = width;
    h = height;
    for (i = 0; i < numLevels; ++i)
    {
        GL_DownMipmap8(image, faded, w, h, (i * 1.75f) / numLevels);

        // Go down one level.
        if (w > 1)
            w /= 2;
        if (h > 1)
            h /= 2;

        LIBGUI_GL.glTexImage2D(GL_TEXTURE_2D, i + 1, glFormat, w, h, 0, (GLint)loadFormat,
            GL_UNSIGNED_BYTE, faded);
    }

    // Do we need to free the temp buffer?
    M_Free(faded);
    M_Free(image);

    DENG_ASSERT(!Sys_GLCheckError());
    return true;
}

/// @note Texture parameters will NOT be set here!
void GL_UploadTextureContent(texturecontent_t const &content, gl::UploadMethod method)
{
    if (method == gl::Deferred)
    {
        GL_DeferTextureUpload(&content);
        return;
    }

    if (novideo) return;

    // Do this right away. No need to take a copy.
    bool generateMipmaps = (content.flags & (TXCF_MIPMAP|TXCF_GRAY_MIPMAP)) != 0;
    bool applyTexGamma   = (content.flags & TXCF_APPLY_GAMMACORRECTION)     != 0;
    bool noCompression   = (content.flags & TXCF_NO_COMPRESSION)            != 0;
    bool noSmartFilter   = (content.flags & TXCF_UPLOAD_ARG_NOSMARTFILTER)  != 0;
    bool noStretch       = (content.flags & TXCF_UPLOAD_ARG_NOSTRETCH)      != 0;

    int loadWidth             = content.width;
    int loadHeight            = content.height;
    uint8_t const *loadPixels = content.pixels;
    dgltexformat_t dglFormat  = content.format;

    // Convert a paletted source image to truecolor.
    if (dglFormat == DGL_COLOR_INDEX_8 || dglFormat == DGL_COLOR_INDEX_8_PLUS_A8)
    {
        uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight,
                                              dglFormat == DGL_COLOR_INDEX_8_PLUS_A8 ? 2 : 1,
                                              content.paletteId,
                                              dglFormat == DGL_COLOR_INDEX_8_PLUS_A8 ? 4 : 3);
        if (loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = newPixels;
        dglFormat = (dglFormat == DGL_COLOR_INDEX_8_PLUS_A8 ? DGL_RGBA : DGL_RGB);
    }

    // Gamma adjustment and smart filtering.
    if (dglFormat == DGL_RGBA || dglFormat == DGL_RGB)
    {
        int comps = (dglFormat == DGL_RGBA ? 4 : 3);

        if (applyTexGamma && texGamma > .0001f)
        {
            uint8_t* dst, *localBuffer = 0;
            long const numPels = loadWidth * loadHeight;

            uint8_t const *src = loadPixels;
            if (loadPixels == content.pixels)
            {
                localBuffer = (uint8_t *) M_Malloc(comps * numPels);
                dst = localBuffer;
            }
            else
            {
                dst = const_cast<uint8_t *>(loadPixels);
            }

            for (long i = 0; i < numPels; ++i)
            {
                dst[CR] = R_TexGammaLut(src[CR]);
                dst[CG] = R_TexGammaLut(src[CG]);
                dst[CB] = R_TexGammaLut(src[CB]);
                if (comps == 4)
                    dst[CA] = src[CA];

                dst += comps;
                src += comps;
            }

            if (localBuffer)
            {
                if (loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = localBuffer;
            }
        }

        if (useSmartFilter && !noSmartFilter)
        {
            if (comps == 3)
            {
                // Need to add an alpha channel.
                uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight, 3, 0, 4);
                if (loadPixels != content.pixels)
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
            if (filtered != loadPixels)
            {
                if (loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = filtered;
            }
        }
    }

    if (dglFormat == DGL_LUMINANCE && (content.flags & TXCF_CONVERT_8BIT_TO_ALPHA))
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(4 * numPixels);

        // Move the average color to the alpha channel, make the actual color white.
        uint8_t *pixel = localBuffer;
        for (long i = 0; i < numPixels; ++i)
        {
            *pixel++ = 255;
            *pixel++ = 255;
            *pixel++ = 255;
            *pixel++ = loadPixels[i];
        }

        if (loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
        dglFormat = DGL_RGBA;
    }
    else if (dglFormat == DGL_LUMINANCE)
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(3 * numPixels);

        // Move the average color to the alpha channel, make the actual color white.
        uint8_t *pixel = localBuffer;
        for (long i = 0; i < numPixels; ++i)
        {
            *pixel++ = loadPixels[i];
            *pixel++ = loadPixels[i];
            *pixel++ = loadPixels[i];
        }

        if (loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
        dglFormat = DGL_RGB;
    }

    if (dglFormat == DGL_LUMINANCE_PLUS_A8)
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(4 * numPixels);

        uint8_t *pixel = localBuffer;
        for (long i = 0; i < numPixels; ++i)
        {
            *pixel++ = loadPixels[i];
            *pixel++ = loadPixels[i];
            *pixel++ = loadPixels[i];
            *pixel++ = loadPixels[numPixels + i];
        }

        if (loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
        dglFormat = DGL_RGBA;
    }

    // Calculate the final dimensions for the texture, as required by
    // the graphics hardware and/or engine configuration.
    int width = loadWidth, height = loadHeight;

    noStretch = GL_OptimalTextureSize(width, height, noStretch, generateMipmaps,
                                      &loadWidth, &loadHeight);

    // Do we need to resize?
    if (width != loadWidth || height != loadHeight)
    {
        int comps = BytesPerPixelFmt(dglFormat);

        if (noStretch)
        {
            // Copy the texture into a power-of-two canvas.
            uint8_t *localBuffer = (uint8_t *) M_Calloc(comps * loadWidth * loadHeight);

            // Copy line by line.
            for (int i = 0; i < height; ++i)
            {
                std::memcpy(localBuffer + loadWidth * comps * i,
                            loadPixels  + width     * comps * i, comps * width);
            }

            if (loadPixels != content.pixels)
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
            if (loadPixels != content.pixels)
            {
                M_Free(const_cast<uint8_t *>(loadPixels));
            }
            loadPixels = newPixels;
        }
    }

    //DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glBindTexture(GL_TEXTURE_2D, content.name);
    LIBGUI_GL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content.minFilter);
    LIBGUI_GL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content.magFilter);
    LIBGUI_GL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     content.wrap[0]);
    LIBGUI_GL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     content.wrap[1]);
    if (GL_state.features.texFilterAniso)
        LIBGUI_GL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(content.anisoFilter));

    DENG2_ASSERT(dglFormat == DGL_RGB || dglFormat == DGL_RGBA);

    if (!(content.flags & TXCF_GRAY_MIPMAP))
    {
        GLint loadFormat;
        switch (dglFormat)
        {
        //case DGL_LUMINANCE_PLUS_A8: loadFormat = GL_LUMINANCE_ALPHA; break;
        //case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE;       break;
        case DGL_RGB:               loadFormat = GL_RGB;             break;
        case DGL_RGBA:              loadFormat = GL_RGBA;            break;
        default:
            throw Error("GL_UploadTextureContent", QString("Unknown format %1").arg(int(dglFormat)));
        }

        GLint glFormat = ChooseTextureFormat(dglFormat, !noCompression);

        if (!uploadTexture(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
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

        switch (dglFormat)
        {
        //case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB;       break;
        default:
            throw Error("GL_UploadTextureContent", QString("Unknown format %1").arg(int(dglFormat)));
        }

        glFormat = ChooseTextureFormat(dglFormat, !noCompression);

        if (!uploadTextureGrayMipmap(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                                     content.grayMipmap * reciprocal255))
        {
            throw Error("GL_UploadTextureContent", QString("TexImageGrayMipmap failed (%1:%2 fmt%3)")
                                                       .arg(content.name)
                                                       .arg(Vector2i(loadWidth, loadHeight).asText())
                                                       .arg(int(dglFormat)));
        }
    }

    if (loadPixels != content.pixels)
    {
        M_Free(const_cast<uint8_t *>(loadPixels));
    }
}
