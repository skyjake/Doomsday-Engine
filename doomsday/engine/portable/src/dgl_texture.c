/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * dgl_texture.c: Texture Handling
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

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

gl_state_texture_t GL_state_texture;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Choose an internal texture format based on the number of color components.
 *
 * @param comps         Number of color components.
 * @return              The internal texture format.
 */
GLenum ChooseFormat(int comps)
{
    boolean     compress = (GL_state_texture.useCompr && GL_state.allowCompression);

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

void loadPalette(int sharedPalette)
{
    int         i;
    byte        paldata[256 * 3];

    if(GL_state_texture.usePalTex == false)
        return;

    // Prepare the color table (RGBA -> RGB).
    for(i = 0; i < 256; ++i)
        memcpy(paldata + i * 3, GL_state_texture.palette[i].color, 3);

    glColorTableEXT(sharedPalette ? GL_SHARED_TEXTURE_PALETTE_EXT :
                    GL_TEXTURE_2D, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE,
                    paldata);
}

boolean GL_EnablePalTexExt(boolean enable)
{
    if(!GL_state.palExtAvailable && !GL_state.sharedPalExtAvailable)
    {
        Con_Message
            ("drOpenGL.GL_EnablePalTexExt: No paletted texture support.\n");
        return false;
    }

    if((enable && GL_state_texture.usePalTex) ||
       (!enable && !GL_state_texture.usePalTex))
        return true;

    if(!enable && GL_state_texture.usePalTex)
    {
        GL_state_texture.usePalTex = false;
        if(GL_state.sharedPalExtAvailable)
            glDisable(GL_SHARED_TEXTURE_PALETTE_EXT);
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
    if(GL_state.sharedPalExtAvailable)
    {
        Con_Message("drOpenGL.GL_EnablePalTexExt: Using shared tex palette.\n");
        glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
        loadPalette(true);
    }
    else
    {   // Palette will be loaded separately for each texture.
        Con_Message("drOpenGL.GL_EnablePalTexExt: Using tex palette.\n");
    }

    return true;
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

boolean grayMipmap(gltexformat_t format, int width, int height, void *data)
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

    GL_TexFilter(DGL_ANISO_FILTER, GL_GetTexAnisoMul(-1 /*best*/));
    return true;
}

/**
 * @param format        DGL texture format symbolic, one of:
 *                          DGL_RGB
 *                          DGL_RGBA
 *                          DGL_COLOR_INDEX_8
 *                          DGL_COLOR_INDEX_8_PLUS_A8
 *                          DGL_LUMINANCE
 * @param width         Width of the texture, must be power of two.
 * @param height        Height of the texture, must be power of two.
 * @param genMips       If negative, sets a specific mipmap level,
 *                      e.g. @c -1, means mipmap level 1.
 * @param data          Ptr to the texture data.
 *
 * @return              @c true iff successful.
 */
boolean GL_TexImage(gltexformat_t format, int width, int height,
                     int genMips, void *data)
{
    int         mipLevel = 0;
    byte       *bdata = data;

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

    // Special fade-to-gray luminance texture? (used for details)
    if(genMips == DGL_GRAY_MIPMAP)
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
                              height, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, data);
        }
        else
        {   // The texture has no mipmapping.
            glTexImage2D(GL_TEXTURE_2D, mipLevel, GL_COLOR_INDEX8_EXT, width,
                         height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, data);
        }

        // Load palette, too (if not shared).
        if(!GL_state.sharedPalExtAvailable)
            loadPalette(false);
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
                loadFormat = GL_RGB;
                for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 3)
                {
                    pixel[CR] = GL_state_texture.palette[bdata[i]].color[CR];
                    pixel[CG] = GL_state_texture.palette[bdata[i]].color[CG];
                    pixel[CB] = GL_state_texture.palette[bdata[i]].color[CB];
                }
                break;

            case DGL_COLOR_INDEX_8_PLUS_A8:
                for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 4)
                {
                    pixel[CR] = GL_state_texture.palette[bdata[i]].color[CR];
                    pixel[CG] = GL_state_texture.palette[bdata[i]].color[CG];
                    pixel[CB] = GL_state_texture.palette[bdata[i]].color[CB];
                    pixel[CA] = bdata[numPixels + i];
                }
                break;

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
            gluBuild2DMipmaps(GL_TEXTURE_2D, ChooseFormat(colorComps), width,
                              height, loadFormat, GL_UNSIGNED_BYTE, buffer);
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

void DGL_DeleteTextures(int num, const DGLuint *names)
{
    if(!num || !names)
        return;

    glDeleteTextures(num, (const GLuint*) names);
}

void GL_TexFilter(int pname, int param)
{
    switch(pname)
    {
    case DGL_ANISO_FILTER:
        if(GL_state.useAnisotropic)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            param);
        break;

    case DGL_MIN_FILTER:
    case DGL_MAG_FILTER:
        {
        GLenum  mlevs[] = {
            GL_NEAREST,
            GL_LINEAR,
            GL_NEAREST_MIPMAP_NEAREST,
            GL_LINEAR_MIPMAP_NEAREST,
            GL_NEAREST_MIPMAP_LINEAR,
            GL_LINEAR_MIPMAP_LINEAR
        };

        if(param >= DGL_NEAREST && param <= DGL_LINEAR_MIPMAP_LINEAR)
            glTexParameteri(GL_TEXTURE_2D,
                            pname == DGL_MIN_FILTER ? GL_TEXTURE_MIN_FILTER :
                            GL_TEXTURE_MAG_FILTER, mlevs[param - DGL_NEAREST]);
        break;
        }

    default:
        Con_Error("GL_TexFilter: Invalid value, pname = %i", pname);
        break;
    }
}

void GL_Palette(gltexformat_t format, void *data)
{
    unsigned char *ptr = data;
    int         i;
    size_t      size = sizeof(unsigned char) *(format == DGL_RGBA ? 4 : 3);

    for(i = 0; i < 256; i++, ptr += size)
    {
        GL_state_texture.palette[i].color[CR] = ptr[CR];
        GL_state_texture.palette[i].color[CG] = ptr[CG];
        GL_state_texture.palette[i].color[CB] = ptr[CB];
        GL_state_texture.palette[i].color[CA] = format == DGL_RGBA ? ptr[CA] : 0xff;
    }
    loadPalette(GL_state.sharedPalExtAvailable);

#ifdef _DEBUG
    Sys_CheckGLError();
#endif
}

int DGL_Bind(DGLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
#ifdef _DEBUG
    Sys_CheckGLError();
#endif
    return 0;
}
