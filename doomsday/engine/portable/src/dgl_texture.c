/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * dgl_texture.c: Textures and color palette handling.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define RGB18(r, g, b)      ((r)+((g)<<6)+((b)<<12))

// TYPES -------------------------------------------------------------------

// Color Palette Flags (CPF):
#define CPF_UPDATE_18TO8    0x1 // The 18To8 table needs updating.

typedef struct {
    ushort          num;
    byte            flags; // CPF_* flags.
    DGLubyte*       data; // R8G8B8 color triplets, [num * 3].
    ushort*         pal18To8; // 262144 unique mappings.
} gl_colorpalette_t;

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

gl_state_texture_t GL_state_texture;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static gl_colorpalette_t* colorPalettes = NULL;
static DGLuint numColorPalettes = 0;

// CODE --------------------------------------------------------------------

static void loadPalette(const gl_colorpalette_t* pal)
{
    if(GL_state_texture.usePalTex)
    {
        int                 i;
        byte                paldata[256 * 3];
        byte*               buf;

        if(pal->num > 256)
            buf = malloc(pal->num * 3);
        else
            buf = paldata;

        // Prepare the color table.
        for(i = 0; i < pal->num; ++i)
        {
            // Adjust the values for the appropriate gamma level.
            buf[i * 3]     = gammaTable[pal->data[i * 3]];
            buf[i * 3 + 1] = gammaTable[pal->data[i * 3 + 1]];
            buf[i * 3 + 2] = gammaTable[pal->data[i * 3 + 2]];
        }

        glColorTableEXT(GL_TEXTURE_2D, GL_RGB, pal->num, GL_RGB,
                        GL_UNSIGNED_BYTE, paldata);

        if(pal->num > 256)
            free(buf);
    }
}

boolean GL_EnablePalTexExt(boolean enable)
{
    if(!GL_state.palExtAvailable)
    {
        Con_Message("GL_EnablePalTexExt: No paletted texture support.\n");
        return false;
    }

    if((enable && GL_state_texture.usePalTex) ||
       (!enable && !GL_state_texture.usePalTex))
        return true;

    if(!enable && GL_state_texture.usePalTex)
    {
        GL_state_texture.usePalTex = false;
#ifdef WIN32
        glColorTableEXT = NULL;
#endif
        return true;
    }

    GL_state_texture.usePalTex = false;

#ifdef WIN32
    if((glColorTableEXT =
        (PFNGLCOLORTABLEEXTPROC) wglGetProcAddress("glColorTableEXT")) == NULL)
    {
        Con_Message("drOpenGL.GL_EnablePalTexExt: getProcAddress failed.\n");
        return false;
    }
#endif

    GL_state_texture.usePalTex = true;

    // Palette will be loaded separately for each texture.
    Con_Message("drOpenGL.GL_EnablePalTexExt: Using tex palette.\n");

    return true;
}

/**
 * Prepares an 18 to 8 bit quantization table from the specified palette.
 * Finds the color index that most closely resembles this RGB combination.
 *
 * \note A time-consuming operation (64 * 64 * 64 * 256!).
 */
static void prepareColorPalette18To8(gl_colorpalette_t* pal)
{
    if((pal->flags & CPF_UPDATE_18TO8) || !pal->pal18To8)
    {
#define SIZEOF18TO8     (sizeof(ushort) * 262144) // \fixme Too big?

        int                 r, g, b;
        ushort              closestIndex = 0;

        if(!pal->pal18To8)
        {
            pal->pal18To8 = Z_Malloc(SIZEOF18TO8, PU_STATIC, 0);
        }

        for(r = 0; r < 64; ++r)
        {
            for(g = 0; g < 64; ++g)
            {
                for(b = 0; b < 64; ++b)
                {
                    ushort              i;
                    int                 smallestDiff = DDMAXINT;

                    for(i = 0; i < pal->num; ++i)
                    {
                        int                 diff;
                        const DGLubyte*     rgb = &pal->data[i * 3];

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
            filename_t          name;
            FILE*               file;

            dd_snprintf(name, FILENAME_T_MAXLEN, "%s_18To8.lmp",
                     pal->name);
            file = fopen(name, "wb");

            fwrite(pal->pal18To8, SIZEOF18TO8, 1, file);
            fclose(file);
        }*/

#undef SIZEOF18TO8
    }
}

static void readBits(byte* out, const byte** src, byte* cb, uint numBits)
{
    int                 offset = 0, unread = numBits;

    // Read full bytes.
    if(unread >= 8)
    {
        do
        {
            out[offset++] = **src, (*src)++;
        } while((unread -= 8) >= 8);
    }

    if(unread != 0)
    {   // Read remaining bits.
        byte                fb = 8 - unread;

        if((*cb) == 0)
            (*cb) = 8;

        do
        {
            (*cb)--;
            out[offset] <<= 1;
            out[offset] |= ((**src >> (*cb)) & 0x01);
        } while(--unread > 0);

        out[offset] <<= fb;

        if((*cb) == 0)
            (*src)++;
    }
}

/**
 * @param compOrder     Component order. Examples:
 *                         [0,1,2] == RGB
 *                         [2,1,0] == BGR
 * @param compSize      Number of bits per component [R,G,B].
 */
DGLuint GL_CreateColorPalette(const int compOrder[3],
                              const byte compSize[3], const byte* data,
                              ushort num)
{
    gl_colorpalette_t*  pal;
    byte                order[3], bits[3];

    // Ensure input is in range.
    order[0] = MINMAX_OF(0, compOrder[0], 2);
    order[1] = MINMAX_OF(0, compOrder[1], 2);
    order[2] = MINMAX_OF(0, compOrder[2], 2);

    bits[CR] = MIN_OF(compSize[CR], 32);
    bits[CG] = MIN_OF(compSize[CG], 32);
    bits[CB] = MIN_OF(compSize[CB], 32);

    // Allocate a new color palette and data buffer.
    colorPalettes = Z_Realloc(colorPalettes,
        ++numColorPalettes * sizeof(*pal), PU_STATIC);

    pal = &colorPalettes[numColorPalettes-1];

    pal->num = num;
    pal->data = Z_Malloc(pal->num * sizeof(DGLubyte) * 3, PU_STATIC, 0);

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
            uint                i;

            for(i = 0; i < pal->num; ++i)
            {
                byte*               dst = &pal->data[i * 3];
                byte                tmp[3];

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
        uint                i;
        byte                cb = 0;
        const byte*         src = data;

        for(i = 0; i < pal->num; ++i)
        {
            byte*               dst = &pal->data[i * 3];
            int                 tmp[3];

            tmp[CR] = tmp[CG] = tmp[CB] = 0;

            readBits((byte*) &(tmp[order[CR]]), &src, &cb, bits[order[CR]]);
            readBits((byte*) &(tmp[order[CG]]), &src, &cb, bits[order[CG]]);
            readBits((byte*) &(tmp[order[CB]]), &src, &cb, bits[order[CB]]);

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
            dst[CR] = MINMAX_OF(0, tmp[CR], 255);
            dst[CG] = MINMAX_OF(0, tmp[CG], 255);
            dst[CB] = MINMAX_OF(0, tmp[CB], 255);
        }
    }

    // We defer creation of the 18 to 8 translation table as it may not
    // be needed depending on what this palette is used for.
    pal->flags = CPF_UPDATE_18TO8;
    pal->pal18To8 = NULL;

    return numColorPalettes; // 1-based index.
}

void GL_DeleteColorPalettes(DGLsizei n, const DGLuint* palettes)
{
    DGLsizei            i;

    if(!(n > 0) || !palettes)
        return;

    for(i = 0; i < n; ++i)
    {
        if(palettes[i] != 0 && palettes[i] - 1 < numColorPalettes)
        {
            uint                idx = palettes[i]-1;
            gl_colorpalette_t*  pal = &colorPalettes[idx];

            Z_Free(pal->data);
            if(pal->pal18To8)
                Z_Free(pal->pal18To8);

            memmove(&colorPalettes[idx], &colorPalettes[idx+1],
                    sizeof(*pal));
            numColorPalettes--;
        }
    }

    if(numColorPalettes)
    {
        colorPalettes = Z_Realloc(colorPalettes,
            numColorPalettes * sizeof(gl_colorpalette_t), PU_STATIC);
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

boolean GL_PalettizeImage(byte* out, int outformat, DGLuint palid,
                          boolean gammaCorrect, const byte* in,
                          int informat, int width, int height)
{
    if(out && in && width > 0 && height > 0 && informat <= 2 &&
       outformat >= 3 && palid && palid - 1 < numColorPalettes)
    {
        int                 i, numPixels = width * height,
                            inSize = (informat == 2 ? 1 : informat),
                            outSize = (outformat == 2 ? 1 : outformat);
        const gl_colorpalette_t* pal = &colorPalettes[palid-1];

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            ushort          idx = MINMAX_OF(0, (*in), pal->num) * 3;

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
                    out[3] = in[numPixels * inSize];
                else
                    out[3] = 0;
            }
        }

        return true;
    }

    return false;
}

boolean GL_QuantizeImageToPalette(byte* out, int outformat,
                                  DGLuint palid, const byte* in,
                                  int informat, int width, int height)
{
    if(in && out && informat >= 3 && outformat <= 2 && width > 0 &&
       height > 0 && palid && palid - 1 < numColorPalettes)
    {
        gl_colorpalette_t*  pal = &colorPalettes[palid-1];
        int                 i, numPixels = width * height,
                            inSize = (informat == 2 ? 1 : informat),
                            outSize = (outformat == 2 ? 1 : outformat);

        // Ensure we've prepared the 18 to 8 table.
        prepareColorPalette18To8(pal);

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

/**
 * Desaturates the texture in the dest buffer by averaging the colour then
 * looking up the nearest match in the palette. Increases the brightness
 * to maximum.
 */
void GL_DeSaturatePalettedImage(byte* buffer, DGLuint palid, int width,
                                int height)
{
    int                 i, max;
    const int           numpels = width * height;
    const gl_colorpalette_t* pal;

    if(!buffer || !palid || palid - 1 >= numColorPalettes)
        return;

    if(width == 0 || height == 0)
        return; // Nothing to do.

    // Ensure we've prepared the 18 to 8 table.
    prepareColorPalette18To8(&colorPalettes[palid-1]);

    pal = &colorPalettes[palid-1];

    // What is the maximum color value?
    max = 0;
    for(i = 0; i < numpels; ++i)
    {
        const DGLubyte*     rgb = &pal->data[
            MINMAX_OF(0, buffer[i], pal->num) * 3];
        int                 temp;

        temp = (2 * (int)rgb[CR] + 4 * (int)rgb[CG] + 3 * (int)rgb[CB]) / 9;
        if(temp > max)
            max = temp;
    }

    for(i = 0; i < numpels; ++i)
    {
        const DGLubyte*     rgb = &pal->data[
            MINMAX_OF(0, buffer[i], pal->num) * 3];
        int                 temp;

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
 * @param comps         Number of color components.
 * @return              The internal texture format.
 */
GLenum ChooseFormat(int comps)
{
    boolean             compress =
        (GL_state_texture.useCompr && GL_state.allowCompression);

    switch(comps)
    {
    case 1: // Luminance
        return compress ? GL_COMPRESSED_LUMINANCE : GL_LUMINANCE;

    case 3: // RGB
        return !compress ? 3 : GL_state_ext.s3TC ? GL_COMPRESSED_RGB_S3TC_DXT1_EXT :
            GL_COMPRESSED_RGB;

    case 4: // RGBA
        return !compress ? 4 : GL_state_ext.s3TC ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT // >1-bit alpha
            : GL_COMPRESSED_RGBA;

    default:
        Con_Error("drOpenGL.ChooseFormat: Unsupported comps: %i.", comps);
    }

    // The fallback.
    return comps;
}

int GL_GetTexAnisoMul(int level)
{
    int         mul = 1;

    // Should anisotropic filtering be used?
    if(GL_state.useAnisotropic)
    {
        if(level < 0)
        {   // Go with the maximum!
            mul = GL_state.maxAniso;
        }
        else
        {   // Convert from a DGL aniso level to a multiplier.
            // i.e 0 > 1, 1 > 2, 2 > 4, 3 > 8, 4 > 16
            switch(level)
            {
            case 0: mul = 1; break; // x1 (normal)
            case 1: mul = 2; break; // x2
            case 2: mul = 4; break; // x4
            case 3: mul = 8; break; // x8
            case 4: mul = 16; break; // x16

            default: // Wha?
                mul = 1;
                break;
            }

            // Clamp.
            if(mul > GL_state.maxAniso)
                mul = GL_state.maxAniso;
        }
    }

    return mul;
}

/**
 * Works within the given data, reducing the size of the picture to half
 * its original.
 *
 * @param width         Width of the final texture, must be power of two.
 * @param height        Height of the final texture, must be power of two.
 */
void downMip8(byte *in, byte *fadedOut, int width, int height, float fade)
{
    byte       *out = in;
    int         x, y, outW = width / 2, outH = height / 2;
    float       invFade;

    if(fade > 1)
        fade = 1;
    invFade = 1 - fade;

    if(width == 1 && height == 1)
    {   // Nothing can be done.
        return;
    }

    if(!outW || !outH)
    {   // Limited, 1x2|2x1 -> 1x1 reduction?
        int     outDim = (width > 1 ? outW : outH);

        for(x = 0; x < outDim; x++, in += 2)
        {
            *out = (in[0] + in[1]) / 2;
            *fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
            out++;
        }
    }
    else
    {   // Unconstrained, 2x2 -> 1x1 reduction?
        for(y = 0; y < outH; y++, in += width)
            for(x = 0; x < outW; x++, in += 2)
            {
                *out = (in[0] + in[1] + in[width] + in[width + 1]) / 4;
                *fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
                out++;
            }
    }
}

boolean grayMipmap(dgltexformat_t format, int width, int height, void *data)
{
    byte       *image, *in, *out, *faded;
    int         i, numLevels, w, h, size = width * height, res;
    uint        comps = (format == DGL_LUMINANCE? 1 : 3);
    float       invFactor = 1 - GL_state_texture.grayMipmapFactor;

    // Buffer used for the faded texture.
    faded = M_Malloc(size / 4);
    image = M_Malloc(size);

    // Initial fading.
    if(format == DGL_LUMINANCE || format == DGL_RGB)
    {
        for(i = 0, in = data, out = image; i < size; ++i, in += comps)
        {
            res = (int) (*in * GL_state_texture.grayMipmapFactor + 0x80 * invFactor);

            // Clamp to [0, 255].
            if(res < 0)
                res = 0;
            if(res > 255)
                res = 255;

            *out++ = res;
        }
    }

    // How many levels will there be?
    for(numLevels = 0, w = width, h = height; w > 1 || h > 1;
        w /= 2, h /= 2, numLevels++);

    // We do not want automatical mipmaps.
    if(GL_state_ext.genMip)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);

    // Upload the first level right away.
    glTexImage2D(GL_TEXTURE_2D, 0, ChooseFormat(1), width, height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, image);

    // Generate all mipmaps levels.
    for(i = 0, w = width, h = height; i < numLevels; ++i)
    {
        downMip8(image, faded, w, h, (i * 1.75f) / numLevels);

        // Go down one level.
        if(w > 1)
            w /= 2;
        if(h > 1)
            h /= 2;

        glTexImage2D(GL_TEXTURE_2D, i + 1, ChooseFormat(1), w, h, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, faded);
    }

    // Do we need to free the temp buffer?
    M_Free(faded);
    M_Free(image);

    if(GL_state.useAnisotropic)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        GL_GetTexAnisoMul(-1 /*best*/));
    return true;
}

/**
 * @param format        DGL texture format symbolic, one of:
 *                          DGL_RGB
 *                          DGL_RGBA
 *                          DGL_COLOR_INDEX_8
 *                          DGL_COLOR_INDEX_8_PLUS_A8
 *                          DGL_LUMINANCE
 * @param palid         Id of the color palette to use with this texture.
 *                      Only has meaning if the input format is one of:
 *                          DGL_COLOR_INDEX_8
 *                          DGL_COLOR_INDEX_8_PLUS_A8
 * @param width         Width of the texture, must be power of two.
 * @param height        Height of the texture, must be power of two.
 * @param genMips       If negative, sets a specific mipmap level,
 *                      e.g. @c -1, means mipmap level 1.
 * @param data          Ptr to the texture data.
 *
 * @return              @c true iff successful.
 */
boolean GL_TexImage(dgltexformat_t format, DGLuint palid,
                    int width, int height, int genMips, void *data)
{
    int                 mipLevel = 0;
    byte*               bdata = data;

    // Negative genMips values mean that the specific mipmap level is
    // being uploaded.
    if(genMips < 0)
    {
        mipLevel = -genMips;
        genMips = 0;
    }

    // Can't operate on the null texture.
    if(!data)
        return false;

    // Check that the texture dimensions are valid.
    if(!GL_state.textureNonPow2 &&
       (width != M_CeilPow2(width) || height != M_CeilPow2(height)))
        return false;

    if(width > GL_state.maxTexSize || height > GL_state.maxTexSize)
        return false;

    // If this is a paletted texture, we must know which palette to use.
    if((format == DGL_COLOR_INDEX_8 ||
        format == DGL_COLOR_INDEX_8_PLUS_A8) &&
       (!palid || palid - 1 >= numColorPalettes))
        return false;

    // Special fade-to-gray luminance texture? (used for details)
    if(genMips == DDMAXINT)
    {
        return grayMipmap(format, width, height, data);
    }

    // Automatic mipmap generation?
    if(GL_state_ext.genMip && genMips)
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);

    // Paletted texture?
    if(GL_state_texture.usePalTex && format == DGL_COLOR_INDEX_8)
    {
        if(genMips && !GL_state_ext.genMip)
        {   // Build mipmap textures.
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_COLOR_INDEX8_EXT, width,
                              height, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
                              data);
        }
        else
        {   // The texture has no mipmapping.
            glTexImage2D(GL_TEXTURE_2D, mipLevel, GL_COLOR_INDEX8_EXT,
                         width, height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE,
                         data);
        }

        // Load palette, too.
        loadPalette(&colorPalettes[palid-1]);
    }
    else
    {   // Use true color textures.
        int         alphachannel = ((format == DGL_RGBA) ||
                        (format == DGL_COLOR_INDEX_8_PLUS_A8) ||
                        (format == DGL_LUMINANCE_PLUS_A8));
        int         i, colorComps = alphachannel ? 4 : 3;
        int         numPixels = width * height;
        byte       *buffer;
        byte       *pixel, *in;
        boolean     needFree = false;
        int         loadFormat = GL_RGBA;

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
            needFree = true;
            buffer = M_Malloc(numPixels * 4);
            if(!buffer)
                return false;

            switch(format)
            {
            case DGL_RGB:
                for(i = 0, pixel = buffer, in = bdata; i < numPixels;
                    i++, pixel += 4)
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
                    const byte*         src =
                        &pal->data[MIN_OF(bdata[i], pal->num) * 3];

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
                    const byte*         src =
                        &pal->data[MIN_OF(bdata[i], pal->num) * 3];

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
                M_Free(buffer);
                Con_Error("LoadTexture: Unknown format %x.\n", format);
                break;
            }
        }

        if(genMips && !GL_state_ext.genMip)
        {   // Build all mipmap levels.
            gluBuild2DMipmaps(GL_TEXTURE_2D, ChooseFormat(colorComps),
                              width, height, loadFormat, GL_UNSIGNED_BYTE,
                              buffer);
        }
        else
        {   // The texture has no mipmapping, just one level.
            glTexImage2D(GL_TEXTURE_2D, mipLevel, ChooseFormat(colorComps),
                         width, height, 0, loadFormat, GL_UNSIGNED_BYTE,
                         buffer);
        }

        if(needFree)
            M_Free(buffer);
    }

#ifdef _DEBUG
    Sys_CheckGLError();
#endif

    return true;
}
