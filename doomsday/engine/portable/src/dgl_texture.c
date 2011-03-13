/**\file dgl_texture.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "sys_opengl.h"

#define RGB18(r, g, b)      ((r)+((g)<<6)+((b)<<12))

// Color Palette Flags (CPF):
#define CPF_UPDATE_18TO8    0x1 // The 18To8 table needs updating.

typedef struct {
    ushort num;
    uint8_t flags; /// CPF_* flags.
    DGLubyte* data; /// RGB888 color triplets, [num * 3].
    ushort* pal18To8; /// 262144 unique mappings.
} gl_colorpalette_t;

static gl_colorpalette_t* colorPalettes = NULL;
static DGLuint numColorPalettes = 0;

/**
 * Prepares an 18 to 8 bit quantization table from the specified palette.
 * Finds the color index that most closely resembles this RGB combination.
 *
 * \note A time-consuming operation (64 * 64 * 64 * 256!).
 */
static void PrepareColorPalette18To8(gl_colorpalette_t* pal)
{
    if((pal->flags & CPF_UPDATE_18TO8) || !pal->pal18To8)
    {
#define SIZEOF18TO8     (sizeof(ushort) * 262144)

        ushort closestIndex = 0;
        int r, g, b;

        if(!pal->pal18To8)
        {
            pal->pal18To8 = Z_Malloc(SIZEOF18TO8, PU_APPSTATIC, 0);
        }

        for(r = 0; r < 64; ++r)
        {
            for(g = 0; g < 64; ++g)
            {
                for(b = 0; b < 64; ++b)
                {
                    ushort i;
                    int smallestDiff = DDMAXINT;

                    for(i = 0; i < pal->num; ++i)
                    {
                        const DGLubyte* rgb = &pal->data[i * 3];
                        int diff;

                        diff =
                            (rgb[CR] - (r << 2)) * (rgb[CR] - (r << 2)) +
                            (rgb[CG] - (g << 2)) * (rgb[CG] - (g << 2)) +
                            (rgb[CB] - (b << 2)) * (rgb[CB] - (b << 2));

                        if(diff < smallestDiff)
                        {
                            smallestDiff = diff;
                            closestIndex = i;
                        }
                    }

                    pal->pal18To8[RGB18(r, g, b)] = closestIndex;
                }
            }
        }

        pal->flags &= ~CPF_UPDATE_18TO8;

        /*if(ArgCheck("-dump_pal18to8"))
        {
            filename_t name;
            FILE* file;

            dd_snprintf(name, FILENAME_T_MAXLEN, "%s_18To8.lmp", pal->name);
            file = fopen(name, "wb");

            fwrite(pal->pal18To8, SIZEOF18TO8, 1, file);
            fclose(file);
        }*/

#undef SIZEOF18TO8
    }
}

DGLuint GL_CreateColorPalette(const int compOrder[3], const uint8_t compSize[3],
    const uint8_t* data, ushort num)
{
    assert(compOrder && compSize && data);
    {
    gl_colorpalette_t* pal;
    uint8_t order[3], bits[3];

    // Ensure input is in range.
    order[0] = MINMAX_OF(0, compOrder[0], 2);
    order[1] = MINMAX_OF(0, compOrder[1], 2);
    order[2] = MINMAX_OF(0, compOrder[2], 2);

    bits[CR] = MIN_OF(compSize[CR], 32);
    bits[CG] = MIN_OF(compSize[CG], 32);
    bits[CB] = MIN_OF(compSize[CB], 32);

    // Allocate a new color palette and data buffer.
    colorPalettes = Z_Realloc(colorPalettes,
        ++numColorPalettes * sizeof(*pal), PU_APPSTATIC);

    pal = &colorPalettes[numColorPalettes-1];

    pal->num = num;
    pal->data = Z_Malloc(pal->num * sizeof(DGLubyte) * 3, PU_APPSTATIC, 0);

    /**
     * Copy the source data and convert to R8G8B8 in the process.
     */

    // First, see if the source data is already in the format we want.
    if(bits[CR] == 8 && bits[CG] == 8 && bits[CB] == 8)
    {   // Great! Just copy it as-is.
        memcpy(pal->data, data, pal->num * 3);

        // Do we need to adjust the order?
        if(order[CR] != 0 || order[CG] != 1 || order[CB] != 2)
        {
            uint i;
            for(i = 0; i < pal->num; ++i)
            {
                uint8_t* dst = &pal->data[i * 3];
                uint8_t tmp[3];

                tmp[0] = dst[0];
                tmp[1] = dst[1];
                tmp[2] = dst[2];

                dst[CR] = tmp[order[CR]];
                dst[CG] = tmp[order[CG]];
                dst[CB] = tmp[order[CB]];
            }
        }
    }
    else
    {   // Another format entirely.
        const uint8_t* src = data;
        uint8_t cb = 0;
        uint i;

        for(i = 0; i < pal->num; ++i)
        {
            uint8_t* dst = &pal->data[i * 3];
            int tmp[3];

            tmp[CR] = tmp[CG] = tmp[CB] = 0;

            M_ReadBits(bits[order[CR]], &src, &cb, (uint8_t*) &(tmp[order[CR]]));
            M_ReadBits(bits[order[CG]], &src, &cb, (uint8_t*) &(tmp[order[CG]]));
            M_ReadBits(bits[order[CB]], &src, &cb, (uint8_t*) &(tmp[order[CB]]));

            // Need to do any scaling?
            if(bits[CR] != 8)
            {
                if(bits[CR] < 8)
                    tmp[CR] <<= 8 - bits[CR];
                else
                    tmp[CR] >>= bits[CR] - 8;
            }

            if(bits[CG] != 8)
            {
                if(bits[CG] < 8)
                    tmp[CG] <<= 8 - bits[CG];
                else
                    tmp[CG] >>= bits[CG] - 8;
            }

            if(bits[CB] != 8)
            {
                if(bits[CB] < 8)
                    tmp[CB] <<= 8 - bits[CB];
                else
                    tmp[CB] >>= bits[CB] - 8;
            }

            // Store the final color.
            dst[CR] = (uint8_t) MINMAX_OF(0, tmp[CR], 255);
            dst[CG] = (uint8_t) MINMAX_OF(0, tmp[CG], 255);
            dst[CB] = (uint8_t) MINMAX_OF(0, tmp[CB], 255);
        }
    }

    // We defer creation of the 18 to 8 translation table as it may not
    // be needed depending on what this palette is used for.
    pal->flags = CPF_UPDATE_18TO8;
    pal->pal18To8 = NULL;

    return numColorPalettes; // 1-based index.
    }
}

void GL_DeleteColorPalettes(DGLsizei n, const DGLuint* palettes)
{
    DGLsizei i;

    if(!(n > 0) || !palettes)
        return;

    for(i = 0; i < n; ++i)
    {
        if(palettes[i] != 0 && palettes[i] - 1 < numColorPalettes)
        {
            uint idx = palettes[i]-1;
            gl_colorpalette_t* pal = &colorPalettes[idx];

            Z_Free(pal->data);
            if(pal->pal18To8)
                Z_Free(pal->pal18To8);

            memmove(&colorPalettes[idx], &colorPalettes[idx+1], sizeof(*pal));
            numColorPalettes--;
        }
    }

    if(numColorPalettes)
    {
        colorPalettes = Z_Realloc(colorPalettes,
            numColorPalettes * sizeof(gl_colorpalette_t), PU_APPSTATIC);
    }
    else
    {
        Z_Free(colorPalettes);
        colorPalettes = NULL;
    }
}

void GL_GetColorPaletteRGB(DGLuint id, DGLubyte rgb[3], ushort idx)
{
    if(id != 0 && id - 1 < numColorPalettes)
    {
        const gl_colorpalette_t* pal = &colorPalettes[id-1];

        if(idx >= pal->num)
            VERBOSE(
            Con_Message("GL_GetColorPaletteRGB: Warning, color idx %u "
                        "out of range in palette %u.\n", idx, id))

        idx = MINMAX_OF(0, idx, pal->num-1) * 3;
        rgb[CR] = pal->data[idx];
        rgb[CG] = pal->data[idx + 1];
        rgb[CB] = pal->data[idx + 2];
    }
}

boolean GL_PalettizeImage(uint8_t* out, int outformat, DGLuint palid,
    boolean gammaCorrect, const uint8_t* in, int informat, int width, int height)
{
    assert(in && out);
    if(width <= 0 || height <= 0)
        return false;

    if(informat <= 2 && outformat >= 3 && palid && palid - 1 < numColorPalettes)
    {
        int i, numPixels = width * height;
        int inSize = (informat == 2 ? 1 : informat);
        int outSize = (outformat == 2 ? 1 : outformat);
        const gl_colorpalette_t* pal = &colorPalettes[palid-1];

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            ushort idx = MINMAX_OF(0, (*in), pal->num) * 3;

            if(gammaCorrect)
            {
                out[CR] = gammaTable[pal->data[idx]];
                out[CG] = gammaTable[pal->data[idx + 1]];
                out[CB] = gammaTable[pal->data[idx + 2]];
            }
            else
            {
                out[CR] = pal->data[idx];
                out[CG] = pal->data[idx + 1];
                out[CB] = pal->data[idx + 2];
            }

            // Will the alpha channel be necessary?
            if(outformat == 4)
            {
                if(informat == 2)
                    out[CA] = in[numPixels * inSize];
                else
                    out[CA] = 0;
            }
        }
        return true;
    }
    return false;
}

boolean GL_QuantizeImageToPalette(uint8_t* out, int outformat, DGLuint palid,
    const uint8_t* in, int informat, int width, int height)
{
    if(in && out && informat >= 3 && outformat <= 2 && width > 0 &&
       height > 0 && palid && palid - 1 < numColorPalettes)
    {
        gl_colorpalette_t*  pal = &colorPalettes[palid-1];
        int i, numPixels = width * height;
        int inSize = (informat == 2 ? 1 : informat);
        int outSize = (outformat == 2 ? 1 : outformat);

        // Ensure we've prepared the 18 to 8 table.
        PrepareColorPalette18To8(pal);

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            // Convert the color value.
            *out = pal->pal18To8[RGB18(in[0] >> 2, in[1] >> 2, in[2] >> 2)];

            // Alpha channel?
            if(outformat == 2)
            {
                if(informat == 4)
                    out[numPixels * outSize] = in[3];
                else
                    out[numPixels * outSize] = 0;
            }
        }
        return true;
    }
    return false;
}

void GL_DeSaturatePalettedImage(uint8_t* buffer, DGLuint palid, int width, int height)
{
    const int numpels = width * height;
    const gl_colorpalette_t* pal;
    int i, max;

    if(!buffer || !palid || palid - 1 >= numColorPalettes)
        return;

    if(width == 0 || height == 0)
        return; // Nothing to do.

    // Ensure we've prepared the 18 to 8 table.
    PrepareColorPalette18To8(&colorPalettes[palid-1]);

    pal = &colorPalettes[palid-1];

    // What is the maximum color value?
    max = 0;
    for(i = 0; i < numpels; ++i)
    {
        const DGLubyte* rgb = &pal->data[MINMAX_OF(0, buffer[i], pal->num) * 3];
        int temp;

        if(rgb[CR] == rgb[CG] && rgb[CR] == rgb[CB])
        {
            if(rgb[CR] > max)
                max = rgb[CR];
            continue;
        }

        temp = (2 * (int)rgb[CR] + 4 * (int)rgb[CG] + 3 * (int)rgb[CB]) / 9;
        if(temp > max)
            max = temp;
    }

    for(i = 0; i < numpels; ++i)
    {
        const DGLubyte* rgb = &pal->data[MINMAX_OF(0, buffer[i], pal->num) * 3];
        int temp;

        if(rgb[CR] == rgb[CG] && rgb[CR] == rgb[CB])
            continue;

        // Calculate a weighted average.
        temp = (2 * (int)rgb[CR] + 4 * (int)rgb[CG] + 3 * (int)rgb[CB]) / 9;
        if(max)
            temp *= 255.f / max;
        buffer[i] = pal->pal18To8[RGB18(temp >> 2, temp >> 2, temp >> 2)];
    }
}

/**
 * Given a pixel format return the number of bytes to store one pixel.
 * \assume Input data is of GL_UNSIGNED_BYTE type.
 */
static int BytesPerPixel(GLint format)
{
    switch(format)
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
        Con_Error("BytesPerPixel: Unknown format %i.", (int) format);
        return 0; // Unreachable.
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
            return GL_RGBA;
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

boolean GL_TexImageGrayMipmap(const uint8_t* pixels, int width, int height,
    int pixelSize, float grayFactor)
{
    assert(pixels);
    {
    int w, h, numpels = width * height, numLevels;
    uint8_t* image, *faded, *out;
    GLint glTexFormat;
    const uint8_t* in;
    float invFactor;

    // Can't operate on null texture.
    if(width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if(!GL_state.features.texNonPowTwo &&
       (width != M_CeilPow2(width) || height != M_CeilPow2(height)))
        return false;

    if(width > GL_state.maxTexSize || height > GL_state.maxTexSize)
        return false;

    glTexFormat = ChooseTextureFormat(1, GL_state.currentUseTexCompression);
    numLevels = GL_NumMipmapLevels(width, height);
    grayFactor = MINMAX_OF(0, grayFactor, 1);
    invFactor = 1 - grayFactor;

    // Buffer used for the faded texture.
    faded = malloc(numpels / 4);
    image = malloc(numpels);

    // Initial fading.
    in = pixels;
    out = image;
    { int i;
    for(i = 0; i < numpels; ++i)
    {
        *out++ = (uint8_t) MINMAX_OF(0, (*in * grayFactor + 127 * invFactor), 255);
        in += pixelSize;
    }}

    // Upload the first level right away.
    glTexImage2D(GL_TEXTURE_2D, 0, glTexFormat, width, height, 0, GL_LUMINANCE,
        GL_UNSIGNED_BYTE, image);

    // Generate all mipmaps levels.
    w = width;
    h = height;
    { int i;
    for(i = 0; i < numLevels; ++i)
    {
        GL_DownMipmap8(image, faded, w, h, (i * 1.75f) / numLevels);

        // Go down one level.
        if(w > 1)
            w /= 2;
        if(h > 1)
            h /= 2;

        glTexImage2D(GL_TEXTURE_2D, i + 1, glTexFormat, w, h, 0, GL_LUMINANCE,
            GL_UNSIGNED_BYTE, faded);
    }}

    // Do we need to free the temp buffer?
    free(faded);
    free(image);

    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
            GL_GetTexAnisoMul(-1 /*best*/));
    return true;
    }
}

boolean GL_TexImage(dgltexformat_t format, const uint8_t* pixels, int width, 
    int height, DGLuint palid, int genMipmaps)
{
    assert(pixels);
    {
    int mipLevel = 0, numPixels = width * height;
    GLint glFormat, loadFormat;
    boolean needFree = false;

    // Can't operate on null texture.
    if(width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if(!GL_state.features.texNonPowTwo &&
       (width != M_CeilPow2(width) || height != M_CeilPow2(height)))
        return false;

    if(width > GL_state.maxTexSize || height > GL_state.maxTexSize)
        return false;

    // Negative indices signify a specific mipmap level is being uploaded.
    if(genMipmaps < 0)
    {
        mipLevel = -genMipmaps;
        genMipmaps = 0;
    }

    // Automatic mipmap generation?
    if(GL_state.extensions.genMipmapSGIS && genMipmaps)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);

    switch(format)
    {
    case DGL_COLOR_INDEX_8:
    case DGL_COLOR_INDEX_8_PLUS_A8: {
        /// \fixme Needs converting. This adds some overhead.
        const gl_colorpalette_t* pal;
        uint8_t* pixel, *buffer;
        int pixelSize;

        // We must know which palette to use.
        if(!palid || palid - 1 >= numColorPalettes)
            return false;
        pal = &colorPalettes[palid];

        if(format == DGL_COLOR_INDEX_8)
        {
            loadFormat = GL_RGB;
            pixelSize = 3;
        }
        else
        {
            loadFormat = GL_RGBA;
            pixelSize = 4;
        }

        if(NULL == (buffer = malloc(numPixels * pixelSize)))
            Con_Error("GL_TexImage: Failed on allocation of %lu bytes for conversion buffer.",
                      (unsigned long) (numPixels * pixelSize));
        
        pixel = buffer;
        { int i;
        for(i = 0; i < numPixels; ++i)
        {
            const uint8_t* src = &pal->data[MIN_OF(pixels[i], pal->num) * 3];

            pixel[CR] = gammaTable[src[CR]];
            pixel[CG] = gammaTable[src[CG]];
            pixel[CB] = gammaTable[src[CB]];
            if(pixelSize == 4)
                pixel[CA] = pixels[numPixels + i];
            pixel += pixelSize;
        }}

        pixels = buffer;
        needFree = true;
        break;
      }
    case DGL_LUMINANCE_PLUS_A8: {
        /// \fixme Needs converting. This adds some overhead.
        uint8_t* pixel, *buffer;

        if(NULL == (buffer = malloc(2 * numPixels)))
            Con_Error("GL_TexImage: Failed on allocation of %lu bytes for conversion buffer.",
                      (unsigned long) (2 * numPixels));
        
        pixel = buffer;
        { int i;
        for(i = 0; i < numPixels; ++i)
        {
            pixel[0] = pixels[i];
            pixel[1] = pixels[numPixels + i];
            pixel += 2;
        }}

        loadFormat = GL_LUMINANCE_ALPHA;
        pixels = buffer;
        needFree = true;
        break;
      }
    case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
    case DGL_RGB:               loadFormat = GL_RGB; break;
    case DGL_RGBA:              loadFormat = GL_RGBA; break;
    default:
        Con_Error("GL_TexImage: Unknown format %i.", (int) format);
    }

    glFormat = ChooseTextureFormat(format, GL_state.currentUseTexCompression);

    if(genMipmaps && !GL_state.extensions.genMipmapSGIS)
    {   // Build all mipmap levels.
        const int packRowLength = 0, packAlignment = 1, packSkipRows = 0, packSkipPixels = 0;
        const int unpackRowLength = 0, unpackAlignment = 1, unpackSkipRows = 0, unpackSkipPixels = 0;
        int neww, newh, bpp, w, h;
        void* image, *newimage;

        bpp = BytesPerPixel(loadFormat);
        if(bpp == 0)
            Con_Error("LoadTexture: Unknown GL format %i.\n", (int) loadFormat);

        GL_OptimalTextureSize(width, height, false, true, &w, &h);

        if(w != width || h != height)
        {
            // Must rescale image to get "top" mipmap texture image.
            image = GL_ScaleBufferEx(pixels, width, height, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                w, h, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment, packSkipRows,
                packSkipPixels);
            if(NULL == image)
                Con_Error("LoadTexture: Unknown error resizing mipmap level #0.");
        }
        else
        {
            image = (void*) pixels;
        }

        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
        glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)packRowLength);
        glPixelStorei(GL_PACK_ALIGNMENT, (GLint)packAlignment);
        glPixelStorei(GL_PACK_SKIP_ROWS, (GLint)packSkipRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, (GLint)packSkipPixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)unpackRowLength);
        glPixelStorei(GL_UNPACK_ALIGNMENT, (GLint)unpackAlignment);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, (GLint)unpackSkipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, (GLint)unpackSkipPixels);

        for(;;)
        {
            glTexImage2D(GL_TEXTURE_2D, mipLevel, glFormat, w, h, 0, loadFormat,
                GL_UNSIGNED_BYTE, image);

            if(w == 1 && h == 1)
                break;

            ++mipLevel;
            neww = (w < 2) ? 1 : w / 2;
            newh = (h < 2) ? 1 : h / 2;
            newimage = GL_ScaleBufferEx(image, w, h, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                neww, newh, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment,
                packSkipRows, packSkipPixels);
            if(NULL == newimage)
                Con_Error("LoadTexture: Unknown error resizing mipmap level #%i.", mipLevel);

            if(image != pixels)
                free(image);
            image = newimage;

            w = neww;
            h = newh;
        }

        glPopClientAttrib();

        if(image != pixels)
            free(image);
    }
    else
    {   // Not using mipmapping or we are uploading just the one level.
        glTexImage2D(GL_TEXTURE_2D, mipLevel, glFormat, width, height, 0,
            loadFormat, GL_UNSIGNED_BYTE, pixels);
    }

    if(needFree)
        free((uint8_t*)pixels);

    assert(!Sys_GLCheckError());
    return true;
    }
}
