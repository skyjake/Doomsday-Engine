/**\file image.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_TEXTURECONTENT_H
#define LIBDENG_TEXTURECONTENT_H

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
    const uint8_t* pixels;
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
void GL_InitTextureContent(texturecontent_t* content);

texturecontent_t* GL_ConstructTextureContentCopy(const texturecontent_t* other);

void GL_DestroyTextureContent(texturecontent_t* content);

#endif /* LIBDENG_TEXTURECONTENT_H */
