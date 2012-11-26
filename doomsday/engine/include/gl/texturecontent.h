/**
 * @file texturecontent.h Texture Content
 *
 * @author Copyright &copy; 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_TEXTURECONTENT_H
#define LIBDENG_TEXTURECONTENT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup textureContentFlags  Texture Content Flags
 * @ingroup flags
 */
/*@{*/
#define TXCF_NO_COMPRESSION             0x1
#define TXCF_MIPMAP                     0x2
#define TXCF_GRAY_MIPMAP                0x4
#define TXCF_CONVERT_8BIT_TO_ALPHA      0x8
#define TXCF_APPLY_GAMMACORRECTION      0x10
#define TXCF_UPLOAD_ARG_NOSTRETCH       0x20
#define TXCF_UPLOAD_ARG_NOSMARTFILTER   0x40
#define TXCF_NEVER_DEFER                0x80
/*@}*/

/**
 * Defines the content of a GL texture. Used when creating textures either
 * immediately or in deferred mode (when busy).
 */
typedef struct texturecontent_s {
    dgltexformat_t format;
    DGLuint name;
    uint8_t const *pixels;
    colorpaletteid_t paletteId;
    int width;
    int height;
    int minFilter;
    int magFilter;
    int anisoFilter;
    int wrap[2];
    int grayMipmap;
    int flags; /// @see textureContentFlags
} texturecontent_t;

/**
 * Initializes a texture content struct with default params.
 */
void GL_InitTextureContent(texturecontent_t *content);

texturecontent_t *GL_ConstructTextureContentCopy(texturecontent_t const *other);

void GL_DestroyTextureContent(texturecontent_t *content);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_TEXTURECONTENT_H */
