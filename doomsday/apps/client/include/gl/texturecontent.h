/** @file texturecontent.h  GL-texture content.
 *
 * @author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_GL_TEXTURECONTENT_H
#define DE_CLIENT_GL_TEXTURECONTENT_H

#include "api_gl.h"
#include "gl/gl_defer.h"
#include <doomsday/res/texturemanifest.h>

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
    dgltexformat_t   format;
    GLuint           name;
    const uint8_t *  pixels;
    colorpaletteid_t paletteId;
    int              width;
    int              height;
    GLenum           minFilter;
    GLenum           magFilter;
    int              anisoFilter;
    GLenum           wrap[2];
    int              grayMipmap;
    int              flags; /// @ref textureContentFlags
} texturecontent_t;

/**
 * Initializes a texture content struct with default params.
 */
void GL_InitTextureContent(texturecontent_t *content);

texturecontent_t *GL_ConstructTextureContentCopy(const texturecontent_t *other);

void GL_DestroyTextureContent(texturecontent_t *content);

/**
 * Prepare the texture content @a c, using the given image in accordance with
 * the supplied specification. The image data will be transformed in-place.
 *
 * @param c             Texture content to be completed.
 * @param glTexName     GL name for the texture we intend to upload.
 * @param image         Source image containing the pixel data to be prepared.
 * @param spec          Specification describing any transformations which
 *                      should be applied to the image.
 *
 * @param textureManifest  Manifest for the logical texture being prepared.
 *                      (for informational purposes, i.e., logging)
 */
void GL_PrepareTextureContent(texturecontent_t &c,
                              GLuint glTexName,
                              image_t &image,
                              const TextureVariantSpec &spec,
                              const res::TextureManifest &textureManifest);

/**
 * @param method  GL upload method. By default the upload is deferred.
 *
 * @note Can be rather time-consuming due to forced scaling operations and
 * the generation of mipmaps.
 */
void GL_UploadTextureContent(const texturecontent_t &content,
                             de::gfx::UploadMethod method = de::gfx::Deferred);

#endif // DE_CLIENT_GL_TEXTURECONTENT_H
