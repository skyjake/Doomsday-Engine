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
 * Choose an internal texture format based on the number of color
 * components.
 *
 * @param comps  Number of color components.
 * @return  The internal texture format.
 */
static GLint ChooseTextureFormat(int comps)
{
    boolean compress = (GL_state.useTexCompression && GL_state.allowTexCompression);

    if(!(comps == 1 || comps == 3 || comps == 4))
        Con_Error("ChooseTextureFormat: Unsupported comps: %i.", comps);

    if(comps == 1)
    {   // Luminance.
        return !compress ? GL_LUMINANCE : GL_COMPRESSED_LUMINANCE;
    }

#if USE_TEXTURE_COMPRESSION_S3
    if(compress && GL_state.extensions.texCompressionS3)
    {
        if(comps == 3)
        {   // RGB.
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        }
        // RGBA.
        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }
#endif

    if(comps == 3)
    {   // RGB.
        return !compress ? GL_RGB8 : GL_COMPRESSED_RGB;
    }

    // RGBA.
    return !compress ? GL_RGBA8 : GL_COMPRESSED_RGBA;
}

/**
 * Given a pixel format return the number of bytes to store one pixel.
 * \assume Input data is of GL_UNSIGNED_BYTE type.
 */
static int BytesPerPixel(GLint format)
{
    int n;
    switch(format)
    {
    case GL_COLOR_INDEX:
    case GL_STENCIL_INDEX:
    case GL_DEPTH_COMPONENT:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
        n = 1;
        break;
    case GL_LUMINANCE_ALPHA:
        n = 2;
        break;
    case GL_RGB:
    case GL_RGB8:
    case GL_BGR:
        n = 3;
        break;
    case GL_RGBA:
    case GL_RGBA8:
    case GL_BGRA:
        n = 4;
        break;
    default:
        Con_Error("BytesPerPixel: Unknown format %i.", (int) format);
        return 0; // Unreachable.
    }
    return n;
}

static boolean GrayMipmap(dgltexformat_t format, uint8_t* data, int width, int height)
{
    assert(data);
    {
    int numLevels = GL_NumMipmapLevels(width, height);
    uint8_t* image, *in, *out, *faded;
    int i, w, h, size = width * height, res;
    uint comps = (format == DGL_LUMINANCE? 1 : 3);
    float invFactor = 1 - GL_state.currentGrayMipmapFactor;
    GLint glTexFormat = ChooseTextureFormat(1);

    // Buffer used for the faded texture.
    faded = malloc(size / 4);
    image = malloc(size);

    // Initial fading.
    if(format == DGL_LUMINANCE || format == DGL_RGB)
    {
        for(i = 0, in = data, out = image; i < size; ++i, in += comps)
        {
            res = (int) (*in * GL_state.currentGrayMipmapFactor + 127 * invFactor);
            *out++ = MINMAX_OF(0, res, 255);
        }
    }

    // Upload the first level right away.
    glTexImage2D(GL_TEXTURE_2D, 0, glTexFormat, width, height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, image);

    // Generate all mipmaps levels.
    for(i = 0, w = width, h = height; i < numLevels; ++i)
    {
        GL_DownMipmap8(image, faded, w, h, (i * 1.75f) / numLevels);

        // Go down one level.
        if(w > 1)
            w /= 2;
        if(h > 1)
            h /= 2;

        glTexImage2D(GL_TEXTURE_2D, i + 1, glTexFormat, w, h, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, faded);
    }

    // Do we need to free the temp buffer?
    free(faded);
    free(image);

    if(GL_state.useTexFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        GL_GetTexAnisoMul(-1 /*best*/));

    return true;
    }
}

boolean GL_TexImage(dgltexformat_t format, DGLuint palid, int width,
    int height, int genMipmaps, void* data)
{
    uint8_t* bdata = data;
    int mipLevel = 0;

    // Can't operate on null texture.
    if(!data || width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if(!GL_state.extensions.texNonPow2 &&
       (width != M_CeilPow2(width) || height != M_CeilPow2(height)))
        return false;

    if(width > GL_state.maxTexSize || height > GL_state.maxTexSize)
        return false;

    // If this is a paletted texture, we must know which palette to use.
    if((format == DGL_COLOR_INDEX_8 ||
        format == DGL_COLOR_INDEX_8_PLUS_A8) &&
       (!palid || palid - 1 >= numColorPalettes))
        return false;

    // Negative genMipmaps values mean that the specific mipmap level is being uploaded.
    if(genMipmaps < 0)
    {
        mipLevel = -genMipmaps;
        genMipmaps = 0;
    }

    // Special fade-to-gray luminance texture? (used for details)
    if(genMipmaps == DDMAXINT)
    {
        return GrayMipmap(format, data, width, height);
    }

    // Automatic mipmap generation?
    if(GL_state.extensions.genMipmapSGIS && genMipmaps)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);

    {// Use true color textures.
    int alphachannel = ((format == DGL_RGBA) ||
                        (format == DGL_COLOR_INDEX_8_PLUS_A8) ||
                        (format == DGL_LUMINANCE_PLUS_A8));
    GLint loadFormat = GL_RGBA;
    GLint glFormat = ChooseTextureFormat(alphachannel ? 4 : 3);
    int numPixels = width * height;
    uint8_t* buffer, *pixel, *in;
    boolean needFree = false;

    // Convert to either RGB or RGBA, if necessary.
    if(format == DGL_RGBA)
    {
        buffer = data;
    }
    // A bug in Nvidia's drivers? Very small RGBA textures don't load
    // properly.
    else if(format == DGL_RGB && width > 2 && height > 2)
    {
        buffer = data;
        loadFormat = GL_RGB;
    }
    else
    {   // Needs converting.
        // \fixme This adds some overhead.
        int i;

        needFree = true;
        buffer = malloc(numPixels * 4);
        if(!buffer)
            return false;

        switch(format)
        {
        case DGL_RGB:
            for(i = 0, pixel = buffer, in = bdata; i < numPixels; i++, pixel += 4)
            {
                pixel[CR] = *in++;
                pixel[CG] = *in++;
                pixel[CB] = *in++;
                pixel[CA] = 255;
            }
            break;

        case DGL_COLOR_INDEX_8:
            {
            const gl_colorpalette_t* pal = &colorPalettes[palid];

            loadFormat = GL_RGB;
            for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 3)
            {
                const uint8_t* src = &pal->data[MIN_OF(bdata[i], pal->num) * 3];

                pixel[CR] = gammaTable[src[CR]];
                pixel[CG] = gammaTable[src[CG]];
                pixel[CB] = gammaTable[src[CB]];
            }
            break;
            }
        case DGL_COLOR_INDEX_8_PLUS_A8:
            {
            const gl_colorpalette_t* pal = &colorPalettes[palid];

            for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 4)
            {
                const uint8_t* src = &pal->data[MIN_OF(bdata[i], pal->num) * 3];

                pixel[CR] = gammaTable[src[CR]];
                pixel[CG] = gammaTable[src[CG]];
                pixel[CB] = gammaTable[src[CB]];
                pixel[CA] = bdata[numPixels + i];
            }
            break;
            }

        case DGL_LUMINANCE:
            loadFormat = GL_RGB;
            for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 3)
                pixel[CR] = pixel[CG] = pixel[CB] = bdata[i];
            break;

        case DGL_LUMINANCE_PLUS_A8:
            for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 4)
            {
                pixel[CR] = pixel[CG] = pixel[CB] = bdata[i];
                pixel[CA] = bdata[numPixels + i];
            }
            break;

        default:
            free(buffer);
            Con_Error("LoadTexture: Unknown format %i.\n", (int) format);
            break;
        }
    }

    if(genMipmaps && !GL_state.extensions.genMipmapSGIS)
    {   // Build all mipmap levels.
        const int packRowLength = 0, packAlignment = 1, packSkipRows = 0, packSkipPixels = 0;
        const int unpackRowLength = 0, unpackAlignment = 1, unpackSkipRows = 0, unpackSkipPixels = 0;
        int neww, newh, bpp, w, h;
        void* image, *newimage;

        bpp = BytesPerPixel(loadFormat);
        if(bpp == 0)
            Con_Error("LoadTexture: Unknown GL format %i.\n", (int) loadFormat);

        GL_OptimalSize(width, height, false, true, &w, &h);

        if(w != width || h != height)
        {
            // Must rescale image to get "top" mipmap texture image.
            image = GL_ScaleBufferEx(buffer, width, height, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                w, h, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment, packSkipRows,
                packSkipPixels);
            if(NULL == image)
                Con_Error("LoadTexture: Unknown error resizing mipmap level #0.");
        }
        else
        {
            image = (void*) buffer;
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

            if(image != buffer)
                free(image);
            image = newimage;

            w = neww;
            h = newh;
        }

        glPopClientAttrib();

        if(image != buffer)
            free(image);
    }
    else
    {   // The texture will not use mipmapping, just the one level.
        glTexImage2D(GL_TEXTURE_2D, mipLevel, glFormat, width, height, 0,
            loadFormat, GL_UNSIGNED_BYTE, buffer);
    }

    if(needFree)
        free(buffer);
    }

    assert(!Sys_GLCheckError());
    return true;
}
