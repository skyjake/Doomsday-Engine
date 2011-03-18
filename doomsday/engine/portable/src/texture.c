/**\file texture.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"

#include "gl_tex.h"
#include "gl_texmanager.h"
#include "p_material.h"
#include "m_misc.h"
#include "image.h"
#include "texturecontent.h"
#include "texturevariant.h"

#include "texture.h"

typedef struct texture_variantlist_node_s {
    struct texture_variantlist_node_s* next;
    texturevariant_t* variant;
} texture_variantlist_node_t;

texture_t* Texture_Construct(textureid_t id, const char rawName[9],
    gltexture_type_t glType, int index)
{
    assert(rawName && rawName[0] && VALID_GLTEXTURETYPE(glType));
    {
    texture_t* tex;

    if(NULL == (tex = (texture_t*) malloc(sizeof(*tex))))
        Con_Error("Texture::Construct: Failed on allocation of %lu bytes for "
                  "new Texture.", (unsigned long) sizeof(*tex));

    tex->_id = id;
    tex->_variants = NULL;
    tex->_index = index;
    tex->_glType = glType;

    // Prepare name for hashing.
    memset(tex->_name, 0, sizeof(tex->_name));
    { int i;
    for(i = 0; *rawName && i < 8; ++i, rawName++)
        tex->_name[i] = tolower(*rawName);
    }
    return tex;
    }
}

void Texture_Destruct(texture_t* tex)
{
    assert(tex);
    {
    texture_variantlist_node_t* node = tex->_variants;
    while(node)
    {
        texture_variantlist_node_t* next = node->next;
        DGLuint glName;
        // Have we uploaded yet?
        if(0 != (glName = TextureVariant_GLName(node->variant)))
            glDeleteTextures(1, (const GLuint*) &glName);
        TextureVariant_Destruct(node->variant);
        free(node);
        node = next;
    }
    free(tex);
    }
}

void Texture_AddVariant(texture_t* tex, texturevariant_t* variant)
{
    assert(tex);
    {
    texture_variantlist_node_t* node;

    if(NULL == variant)
    {
#if _DEBUG
        Con_Error("Texture::AddInstance: Warning, argument variant==NULL, ignoring.");
#endif
        return;
    }

    if(NULL == (node = (texture_variantlist_node_t*) malloc(sizeof(*node))))
        Con_Error("Texture::AddInstance: failed on allocation of %lu bytes for new node.",
                  (unsigned long) sizeof(*node));

    node->variant = variant;
    node->next = tex->_variants;
    tex->_variants = node;
    }
}

textureid_t Texture_Id(const texture_t* tex)
{
    assert(tex);
    return tex->_id;
}

const char* Texture_Name(const texture_t* tex)
{
    assert(tex);
    return tex->_name;
}

boolean Texture_IsFromIWAD(const texture_t* tex)
{
    assert(tex);
    switch(tex->_glType)
    {
    case GLT_FLAT:
        return !R_FlatTextureByIndex(tex->_index)->isCustom;

    case GLT_PATCHCOMPOSITE:
        return (R_PatchCompositeTextureByIndex(tex->_index)->flags & TXDF_IWAD) != 0;

    case GLT_SPRITE:
        return !R_SpriteTextureByIndex(tex->_index)->isCustom;

    case GLT_PATCH:
        return !R_PatchTextureByIndex(tex->_index)->isCustom;

    case GLT_DETAIL:
    case GLT_SHINY:
    case GLT_MASK:
    case GLT_SYSTEM:
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return false; // Its definitely not.

    default:
        Con_Error("Texture::IsFromIWAD: Internal Error, invalid type %i.",
                  (int) tex->_glType);
    }

    return false; // Unreachable.
}

int Texture_Width(const texture_t* tex)
{
    assert(tex);
    switch(tex->_glType)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_PATCHCOMPOSITE:
        return R_PatchCompositeTextureByIndex(tex->_index)->width;

    case GLT_SPRITE:
        return R_SpriteTextureByIndex(tex->_index)->width;

    case GLT_PATCH:
        return R_PatchTextureByIndex(tex->_index)->width;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->_index]->width;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("Texture::Width: Internal error, invalid type %i.", (int) tex->_glType);
        return 0; // Unreachable.
    }
}

int Texture_Height(const texture_t* tex)
{
    assert(tex);
    switch(tex->_glType)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_PATCHCOMPOSITE:
        return R_PatchCompositeTextureByIndex(tex->_index)->height;

    case GLT_SPRITE:
        return R_SpriteTextureByIndex(tex->_index)->height;

    case GLT_PATCH:
        return R_PatchTextureByIndex(tex->_index)->height;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->_index]->height;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("Texture::Height: Internal error, invalid type %i.", (int) tex->_glType);
        return 0; // Unreachable.
    }
}

void Texture_Dimensions(const texture_t* tex, int* width, int* height)
{
    if(width)   *width = Texture_Width(tex);
    if(height) *height = Texture_Height(tex);
}

int Texture_TypeIndex(const texture_t* tex)
{
    assert(tex);
    return tex->_index;
}

gltexture_type_t Texture_GLType(const texture_t* tex)
{
    assert(tex);
    return tex->_glType;
}

int Texture_IterateVariants(texture_t* tex,
    int (*callback)(texturevariant_t* instance, void* paramaters), void* paramaters)
{
    assert(tex);
    {
    int result = 0;
    if(callback)
    {
        texture_variantlist_node_t* node = tex->_variants;
        while(node)
        {
            if(0 != (result = callback(node->variant, paramaters)))
                break;
            node = node->next;
        }
    }
    return result;
    }
}

static const dduri_t* searchPath(gltexture_type_t glType, int typeIndex)
{
    switch(glType)
    {
    case GLT_SYSTEM: {
        systex_t* sysTex = sysTextures[typeIndex];
        return sysTex->external;
      }
    /*case GLT_FLAT:
        tmpResult = GL_LoadFlat(&image, texInst->tex, context);
        break;
    case GLT_PATCHCOMPOSITE:
        tmpResult = GL_LoadDoomTexture(&image, texInst->tex, context);
        break;
    case GLT_PATCH:
        tmpResult = GL_LoadDoomPatch(&image, texInst->tex, context);
        break;
    case GLT_SPRITE:
        tmpResult = GL_LoadSprite(&image, texInst->tex, context);
        break;
    case GLT_DETAIL:
        tmpResult = GL_LoadDetailTexture(&image, texInst->tex, context);
        break;*/
    case GLT_SHINY: {
        shinytex_t* sTex = shinyTextures[typeIndex];
        return sTex->external;
      }
    case GLT_MASK: {
        masktex_t* mTex = maskTextures[typeIndex];
        return mTex->external;
      }
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN: {
        skinname_t* sn = &skinNames[typeIndex];
        return sn->path;
      }
    case GLT_LIGHTMAP: {
        lightmap_t* lmap = lightmapTextures[typeIndex];
        return lmap->external;
      }
    case GLT_FLARE: {
        flaretex_t* fTex = flareTextures[typeIndex];
        return fTex->external;
      }
    default:
        Con_Error("Texture::SearchPath: Unknown type %i.", (int) glType);
        return NULL; // Unreachable.
    }
}

static byte loadSourceImage(image_t* img, const texturevariant_t* tex)
{
    assert(img && tex);
    {
    const texture_t* generalCase = TextureVariant_GeneralCase(tex);
    const texturevariantspecification_t* spec = TextureVariant_Spec(tex);
    byte loadResult = 0;
    switch(spec->glType)
    {
    case GLT_FLAT:
        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || Texture_IsFromIWAD(generalCase)))
        {
            flat_t* flat = R_FlatTextureByIndex(Texture_TypeIndex(generalCase));
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;
            assert(NULL != flat);
            // First try the flats namespace then the old-fashioned "flat-name" in the textures namespace.
            Str_Init(&searchPath); Str_Appendf(&searchPath, FLATS_RESOURCE_NAMESPACE_NAME":%s;" TEXTURES_RESOURCE_NAMESPACE_NAME":flat-%s;", flat->name, flat->name);
            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadFlatLump(img, generalCase);
        break;
    case GLT_PATCH:
        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || Texture_IsFromIWAD(generalCase)))
        {
            const patchtex_t* p = R_PatchTextureByIndex(Texture_TypeIndex(generalCase));
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;
            assert(NULL != p);
            Str_Init(&searchPath);
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", W_LumpName(p->lump));
            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadDoomPatchLump(img, generalCase, (spec->flags & TF_UPSCALE_AND_SHARPEN) != 0);
        break;
    case GLT_SPRITE:
        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            const spritetex_t* sprTex = R_SpriteTextureByIndex(Texture_TypeIndex(generalCase));
            ddstring_t searchPath, suffix = { "-ck" };

            assert(NULL != sprTex);

            // Prefer psprite or translated versions if available.
            Str_Init(&searchPath);
            if(spec->type.sprite.pSprite)
            {
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-hud;", sprTex->name);
            }
            else if(spec->type.sprite.tclass || spec->type.sprite.tmap)
            {   // Translated.
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-table%i%i;",
                    sprTex->name, spec->type.sprite.tclass, spec->type.sprite.tmap);
            }
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s", sprTex->name);

            /// \fixme What about the border?
            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadSpriteLump(img, generalCase, spec->type.sprite.pSprite,
                spec->type.sprite.tclass, spec->type.sprite.tmap, spec->border);
        break;
    case GLT_DETAIL: {
        int idx = Texture_TypeIndex(generalCase);
        const detailtex_t* dTex;
        assert(idx >= 0 && idx < detailTexturesCount);
        dTex = detailTextures[idx];
        if(dTex->isExternal)
        {
            ddstring_t* searchPath = Uri_ComposePath(dTex->filePath);
            loadResult = GL_LoadExtTextureEX(img, Str_Text(searchPath), NULL, false);
            Str_Delete(searchPath);
        }
        else
        {
            loadResult = GL_LoadDetailTextureLump(img, generalCase);
        }
        break;
      }
    case GLT_SYSTEM:
    case GLT_SHINY:
    case GLT_MASK:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN: {
        ddstring_t* path = Uri_ComposePath(searchPath(spec->glType, Texture_TypeIndex(generalCase)));
        loadResult = GL_LoadExtTextureEX(img, Str_Text(path), NULL, false);
        Str_Delete(path);
        break;
      }
    default:
        Con_Error("Textures::loadSourceImage: Unknown texture type %i.", (int) spec->glType);
        return 0; // Unreachable.
    }
    return loadResult;
    }
}

static byte prepareTextureVariant(texturevariant_t* tex)
{
    assert(tex);
    {
    const texturevariantspecification_t* spec = TextureVariant_Spec(tex);
    gltexture_type_t glType = spec->glType;
    boolean monochrome    = (glType != GLT_DETAIL? (spec->flags & TF_MONOCHROME) != 0 : false);
    boolean noCompression = (glType != GLT_DETAIL? (spec->flags & TF_NO_COMPRESSION) != 0 : false);
    boolean scaleSharp    = (glType != GLT_DETAIL? (spec->flags & TF_UPSCALE_AND_SHARPEN) != 0 : false);
    int magFilter, minFilter, anisoFilter, wrapS, wrapT, grayMipmap = 0, flags = 0;
    boolean noSmartFilter = false, didDefer = false;
    dgltexformat_t dglFormat;
    boolean alphaChannel;
    byte loadResult = 0;
    DGLuint glName;
    image_t image;
    float s, t;

    // Load in the raw source image.
    if(glType == GLT_PATCHCOMPOSITE)
    {
        loadResult = GL_LoadDoomTexture(&image, TextureVariant_GeneralCase(tex),
            spec->prepareForSkySphere, (spec->flags & TF_ZEROMASK) != 0);
    }
    else
    {
        loadResult = loadSourceImage(&image, tex);
    }

    if(0 == loadResult)
    {   // Not found/failed load.
        //Con_Message("Warning:Texture::Prepare: No image found for \"%s\"\n",
        //            Texture_Name(TextureVariant_GeneralCase(tex)));
        return loadResult;
    }

    if(image.pixelSize == 1)
    {
        if(monochrome && !scaleSharp && (glType == GLT_PATCH || glType == GLT_SPRITE))
            GL_DeSaturatePalettedImage(image.pixels, R_GetColorPalette(0), image.width, image.height);

        if(glType == GLT_DETAIL)
        {
            float baMul, hiMul, loMul;
            EqualizeLuma(image.pixels, image.width, image.height, &baMul, &hiMul, &loMul);
            if(verbose && (baMul != 1 || hiMul != 1 || loMul != 1))
            {
                VERBOSE2( Con_Message("Texture::Prepare: Equalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).\n",
                            Texture_Name(TextureVariant_GeneralCase(tex)), baMul, hiMul, loMul) );
            }
        }

        if(scaleSharp)
        {
            int scaleMethod = GL_ChooseSmartFilter(image.width, image.height, 0);
            int numpels = image.width * image.height;
            uint8_t* newPixels;

            newPixels = GL_ConvertBuffer(image.pixels, image.width, image.height, ((image.flags & IMGF_IS_MASKED)? 2 : 1), 0, false, 4);
            if(newPixels != image.pixels)
            {
                free(image.pixels);
                image.pixels = newPixels;
                image.pixelSize = 4;
                image.originalBits = 32;
            }

            if(monochrome && (glType == GLT_PATCH || glType == GLT_SPRITE))
                Desaturate(image.pixels, image.width, image.height, image.pixelSize);

            newPixels = GL_SmartFilter(scaleMethod, image.pixels, image.width, image.height, 0, &image.width, &image.height);
            if(newPixels != image.pixels)
            {
                free(image.pixels);
                image.pixels = newPixels;
            }

            EnhanceContrast(image.pixels, image.width, image.height, image.pixelSize);
            //SharpenPixels(image.pixels, image.width, image.height, image.pixelSize);
            //BlackOutlines(image.pixels, image.width, image.height, image.pixelSize);

            // Back to indexed+alpha?
            if(monochrome && (glType == GLT_PATCH || glType == GLT_SPRITE))
            {   // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                GL_ConvertToLuminance(&image);
                AmplifyLuma(image.pixels, image.width, image.height, image.pixelSize == 2);
            }
            else
            {   // Yes. Quantize down from RGA(+A) to Indexed(+A), replacing the old image.
                newPixels = GL_ConvertBuffer(image.pixels, image.width, image.height, ((image.flags & IMGF_IS_MASKED)? 2 : 1), 0, false, 4);
                if(newPixels != image.pixels)
                {
                    free(image.pixels);
                    image.pixels = newPixels;
                    image.pixelSize = ((image.flags & IMGF_IS_MASKED)? 2 : 1);
                    image.originalBits = image.pixelSize * 8;
                }
            }

            // Lets not do this again.
            noSmartFilter = true;
        }

        if(fillOutlines /*&& !scaleSharp*/ && (image.flags & IMGF_IS_MASKED) && image.pixelSize == 1)
            ColorOutlinesIdx(image.pixels, image.width, image.height);
    }
    else
    {
        if(monochrome && glType == GLT_PATCH && image.pixelSize > 2)
        {
            GL_ConvertToLuminance(&image);
            AmplifyLuma(image.pixels, image.width, image.height, image.pixelSize == 2);
        }
    }

    // Too big for us?
    if(image.width  > GL_state.maxTexSize || image.height > GL_state.maxTexSize)
    {
        if(image.pixelSize == 3 || image.pixelSize == 4)
        {
            int newWidth  = MIN_OF(image.width, GL_state.maxTexSize);
            int newHeight = MIN_OF(image.height, GL_state.maxTexSize);
            uint8_t* scaledPixels = GL_ScaleBuffer(image.pixels, image.width, image.height, image.pixelSize,
                newWidth, newHeight);
            if(scaledPixels != image.pixels)
            {
                free(image.pixels);
                image.pixels = scaledPixels;
                image.width = newWidth;
                image.height = newHeight;
            }
        }
        else
        {
            Con_Message("Texture::Prepare: Warning, non RGB(A) texture larger than max size (%ix%i bpp%i).\n",
                        image.width, image.height, image.pixelSize);
        }
    }

    // Lightmaps and flare textures should always be monochrome images.
    if((glType == GLT_LIGHTMAP || (glType == GLT_FLARE && image.pixelSize != 4)) &&
       !(image.flags & IMGF_IS_MASKED))
    {
        // An alpha channel is required. If one is not in the image data, we'll generate it.
        GL_ConvertToAlpha(&image, true);
    }

    // Disable compression?
    if(noCompression || (image.width < 128 && image.height < 128) || glType == GLT_FLARE || glType == GLT_SHINY)
        flags |= TXCF_NO_COMPRESSION;

    if(!(glType == GLT_MASK || glType == GLT_SHINY || glType == GLT_LIGHTMAP) &&
       (image.pixelSize > 2 || glType == GLT_MODELSKIN))
        flags |= TXCF_APPLY_GAMMACORRECTION;

    if(glType == GLT_SPRITE /*|| glType == GLT_PATCH*/)
        flags |= TXCF_UPLOAD_ARG_NOSTRETCH;

    if(!monochrome && !(glType == GLT_DETAIL || glType == GLT_SYSTEM || glType == GLT_SHINY || glType == GLT_MASK))
        flags |= TXCF_EASY_UPLOAD;

    if(!monochrome)
    {
        if(glType == GLT_SPRITE || glType == GLT_MODELSKIN || glType == GLT_MODELSHINYSKIN)
        {
            if(image.pixelSize > 1)
                flags |= TXCF_UPLOAD_ARG_RGBDATA;
        }
        else if(image.pixelSize > 2 && !(glType == GLT_SHINY || glType == GLT_MASK || glType == GLT_LIGHTMAP))
            flags |= TXCF_UPLOAD_ARG_RGBDATA;
    }

    if(glType == GLT_DETAIL)
    {
        /**
         * Detail textures are faded to gray depending on the contrast
         * factor. The texture is also progressively faded towards gray
         * when each mipmap level is loaded.
         */
        grayMipmap = MINMAX_OF(0, spec->type.detail.contrast * 255, 255);
        flags |= TXCF_GRAY_MIPMAP;
    }
    else if(!(glType == GLT_SHINY || glType == GLT_PATCH ||
              glType == GLT_LIGHTMAP || glType == GLT_FLARE ||
             (glType == GLT_SPRITE && spec->type.sprite.pSprite)))
        flags |= TXCF_MIPMAP;

    if(glType == GLT_PATCHCOMPOSITE || glType == GLT_PATCH ||
       glType == GLT_SPRITE || glType == GLT_FLAT)
        alphaChannel = ((/*loadResult == 2 &&*/ image.pixelSize == 4) ||
                        (/*loadResult == 1 &&*/ image.pixelSize == 1 && (image.flags & IMGF_IS_MASKED)));
    else
        alphaChannel = image.pixelSize != 3 && !(glType == GLT_MASK || glType == GLT_SHINY);

    if(alphaChannel)
        flags |= TXCF_UPLOAD_ARG_ALPHACHANNEL;

    if(noSmartFilter)
        flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

    if(monochrome)
    {
        dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
    }
    else if(glType == GLT_FLAT || glType == GLT_PATCHCOMPOSITE || glType == GLT_PATCH || glType == GLT_SPRITE)
    {
        dglFormat = ( image.pixelSize > 1?
                        ( alphaChannel? DGL_RGBA : DGL_RGB) :
                        ( alphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8 ) );
    }
    else if(glType == GLT_MODELSKIN || glType == GLT_MODELSHINYSKIN)
    {
        dglFormat = ( alphaChannel? DGL_RGBA : DGL_RGB );
    }
    else
    {
        dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                      image.pixelSize == 3 ? DGL_RGB :
                      image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE );
    }

    if(glType == GLT_FLAT || glType == GLT_PATCHCOMPOSITE || glType == GLT_MASK)
        magFilter = glmode[texMagMode];
    else if(glType == GLT_SPRITE)
        magFilter = filterSprites ? GL_LINEAR : GL_NEAREST;
    else
        magFilter = GL_LINEAR;

    if(glType == GLT_DETAIL)
        minFilter = GL_LINEAR_MIPMAP_LINEAR;
    else if(glType == GLT_PATCH || (glType == GLT_SPRITE && spec->type.sprite.pSprite))
        minFilter = GL_NEAREST;
    else if(glType == GLT_LIGHTMAP || glType == GLT_FLARE || glType == GLT_SHINY)
        minFilter = GL_LINEAR;
    else
        minFilter = glmode[mipmapping];

    if(glType == GLT_PATCH || glType == GLT_FLARE || (glType == GLT_SPRITE && spec->type.sprite.pSprite))
        anisoFilter = 0 /*no anisotropic filtering*/;
    else
        anisoFilter = texAniso; /// \fixme is "best" truely a suitable default for ALL texture types?

    if(glType == GLT_PATCH || glType == GLT_SPRITE || glType == GLT_LIGHTMAP || glType == GLT_FLARE)
        wrapS = wrapT = GL_CLAMP_TO_EDGE;
    else
        wrapS = wrapT = GL_REPEAT;

    glName = GL_NewTextureWithParams3(dglFormat, image.width, image.height,
        image.pixels, flags, grayMipmap, minFilter, magFilter, anisoFilter, wrapS,
        wrapT, &didDefer);

    /// \todo Register name during variant construction/specification.
    TextureVariant_SetGLName(tex, glName);

#ifdef _DEBUG
    if(!didDefer)
        Con_Message("Texture::Prepare: Uploaded \"%s\" (%i) while not busy! "
                    "Should be precached in busy mode?\n", Texture_Name(TextureVariant_GeneralCase(tex)), glName);
#endif

    /**
     * Calculate texture coordinates based on the image dimensions. The
     * coordinates are calculated as width/CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     */
    if(glType == GLT_SPRITE && GL_state.features.texNonPowTwo &&
       (spec->type.sprite.pSprite || !(flags & TXCF_UPLOAD_ARG_NOSTRETCH)) &&
       !(image.width < MINTEXWIDTH || image.height < MINTEXHEIGHT))
    {
        s = t = 1;
    }
    else
    {
        int pw = M_CeilPow2(image.width), ph = M_CeilPow2(image.height);
        s =  image.width / (float) pw;
        t = image.height / (float) ph;
    }

    TextureVariant_SetCoords(tex, s, t);
    TextureVariant_SetMasked(tex, (image.flags & IMGF_IS_MASKED) != 0);

    if(!(glType == GLT_DETAIL || glType == GLT_SPRITE) &&
       spec->prepareForSkySphere)
    {
        averagecolor_analysis_t* avgTopColor;
        
        if(NULL == (avgTopColor = (averagecolor_analysis_t*)TextureVariant_Analysis(tex, TA_SKY_TOPCOLOR)))
        {
            if(NULL == (avgTopColor = (averagecolor_analysis_t*) malloc(sizeof(*avgTopColor))))
                Con_Error("Texture::Prepare: Failed on allocation of %lu bytes for "
                          "new AverageColorAnalysis.", (unsigned int) sizeof(*avgTopColor));
            TextureVariant_AddAnalysis(tex, TA_SKY_TOPCOLOR, avgTopColor);
        }

        // Average color for glow planes and top line color.
        if(image.pixelSize > 1)
        {
            FindAverageLineColor(image.pixels, image.width, image.height, image.pixelSize, 0, avgTopColor->color);
        }
        else
        {
            FindAverageLineColorIdx(image.pixels, image.width, image.height, 0, 0, false, avgTopColor->color);
        }
    }

    if(glType == GLT_SPRITE && !spec->type.sprite.pSprite)
    {
        pointlight_analysis_t* pl;
        
        if(NULL == (pl = (pointlight_analysis_t*)TextureVariant_Analysis(tex, TA_SPRITE_AUTOLIGHT)))
        {
            if(NULL == (pl = (pointlight_analysis_t*) malloc(sizeof(*pl))))
                Con_Error("Texture::Prepare: Failed on allocation of %lu bytes for "
                          "new PointLightAnalysis.", (unsigned int) sizeof(*pl));
            TextureVariant_AddAnalysis(tex, TA_SPRITE_AUTOLIGHT, pl);
        }
        // Calculate light source properties.
        GL_CalcLuminance(image.pixels, image.width, image.height, image.pixelSize, 0,
                         &pl->originX, &pl->originY, pl->color, &pl->brightMul);
    }

    if(glType == GLT_FLAT || glType == GLT_PATCHCOMPOSITE)
    {
        ambientlight_analysis_t* al;
        
        if(NULL == (al = (ambientlight_analysis_t*)TextureVariant_Analysis(tex, TA_WORLD_AMBIENTLIGHT)))
        {
            if(NULL == (al = (ambientlight_analysis_t*) malloc(sizeof(*al))))
                Con_Error("Texture::Prepare: Failed on allocation of %lu bytes for "
                          "new AmbientLightAnalysis.", (unsigned int) sizeof(*al));
            TextureVariant_AddAnalysis(tex, TA_WORLD_AMBIENTLIGHT, al);
        }

        // Average color for glow planes and top line color.
        if(image.pixelSize > 1)
        {
            FindAverageColor(image.pixels, image.width, image.height, image.pixelSize, al->color);
        }
        else
        {
            FindAverageColorIdx(image.pixels, image.width, image.height, 0, false, al->color);
        }
        memcpy(al->colorAmplified, al->color, sizeof(al->colorAmplified));
        amplify(al->colorAmplified);
    }

    GL_DestroyImagePixels(&image);
    return loadResult;
    }
}

texturevariant_t* Texture_Prepare(texture_t* tex, void* context, byte* result)
{
    assert(tex);
    {
    texturevariantspecification_t* spec = GL_TextureVariantSpecificationForContext(Texture_GLType(tex), context);
    texturevariant_t* variant = NULL;
    boolean variantIsNew = false;
    byte loadResult;

    assert(spec);

    // Have we already registered and prepared a suitable variant?
    variant = GL_FindSuitableTextureVariant(tex, spec);
    if(NULL != variant && 0 != TextureVariant_GLName(variant))
    {   // Already prepared.
        if(result) *result = 0;
        return variant;
    }

    // Do we need to allocate a variant?
    if(NULL == variant)
    {
        variant = TextureVariant_Construct(tex, spec);
        variantIsNew = true;
    }

    // (Re)Prepare the variant according to the usage context.
    loadResult = prepareTextureVariant(variant);
    if(variantIsNew)
    {
        if(0 != loadResult)
        {
            Texture_AddVariant(tex, variant);
        }
        else
        {
            free(variant);
            variant = NULL;
        }
    }

    if(result)
        *result = loadResult;
    return variant;
    }
}
