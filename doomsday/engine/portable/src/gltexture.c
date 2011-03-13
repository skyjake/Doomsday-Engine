/**\file gltexture.c
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"

#include "gl_tex.h"
#include "sys_opengl.h"
#include "m_misc.h"

#include "image.h"
#include "texturecontent.h"
#include "gltexturevariant.h"
#include "gltexture.h"

typedef struct gltexture_variantlist_node_s {
    struct gltexture_variantlist_node_s* next;
    gltexturevariant_t* variant;
} gltexture_variantlist_node_t;

void GLTexture_Destruct(gltexture_t* tex)
{
    assert(tex);
    {
    gltexture_variantlist_node_t* node = tex->variants;
    int i;
    while(node)
    {
        gltexture_variantlist_node_t* next = node->next;
        for(i = 0; i < GLTEXTURE_ANALYSISID_COUNT; ++i)
            if(node->variant->analyses[i])
                Z_Free(node->variant->analyses[i]);
        Z_Free(node->variant);
        Z_Free(node);
        node = next;
    }
    Z_Free(tex);
    }
}

void GLTexture_AddVariant(gltexture_t* tex, gltexturevariant_t* variant)
{
    assert(tex);
    {
    gltexture_variantlist_node_t* node;

    if(NULL == variant)
    {
#if _DEBUG
        Con_Error("GLTexture::AddInstance: Warning, argument variant==NULL, ignoring.");
#endif
        return;
    }

    if(NULL == (node = Z_Malloc(sizeof(*node), PU_APPSTATIC, 0)))
        Con_Error("GLTexture::AddInstance: failed on allocation of %lu bytes for new node.",
                  (unsigned long) sizeof(*node));

    node->variant = variant;
    node->next = tex->variants;
    tex->variants = node;
    }
}

const char* GLTexture_Name(const gltexture_t* tex)
{
    assert(tex);
    return tex->name;
}

boolean GLTexture_IsFromIWAD(const gltexture_t* tex)
{
    switch(tex->type)
    {
    case GLT_FLAT:
        return !R_GetFlatForIdx(tex->ofTypeID)->isCustom;

    case GLT_DOOMTEXTURE:
        return (R_GetDoomTextureDef(tex->ofTypeID)->flags & TXDF_IWAD) != 0;

    case GLT_SPRITE:
        return !R_SpriteTextureForIndex(tex->ofTypeID)->isCustom;

    case GLT_DOOMPATCH:
        return !R_PatchTextureForIndex(tex->ofTypeID)->isCustom;

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
        Con_Error("updateGLTexture: Internal Error, invalid type %i.",
                  (int) tex->type);
    }

    return false; // Unreachable.
}

float GLTexture_GetWidth(const gltexture_t* tex)
{
    if(!tex)
        return 0;

    switch(tex->type)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_DOOMTEXTURE:
        return R_GetDoomTextureDef(tex->ofTypeID)->width;

    case GLT_SPRITE:
        return R_SpriteTextureForIndex(tex->ofTypeID)->width;

    case GLT_DOOMPATCH:
        return R_PatchTextureForIndex(tex->ofTypeID)->width;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->ofTypeID]->width;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("GLTexture_GetWidth: Internal error, "
                  "invalid material type %i.", (int) tex->type);
    }

    return 0; // Unreachable.
}

float GLTexture_GetHeight(const gltexture_t* tex)
{
    if(!tex)
        return 0;

    switch(tex->type)
    {
    case GLT_FLAT:
        return 64; /// \fixme not all flats are 64x64

    case GLT_DOOMTEXTURE:
        return R_GetDoomTextureDef(tex->ofTypeID)->height;

    case GLT_SPRITE:
        return R_SpriteTextureForIndex(tex->ofTypeID)->height;

    case GLT_DOOMPATCH:
        return R_PatchTextureForIndex(tex->ofTypeID)->height;

    case GLT_DETAIL:
        return 128;

    case GLT_SHINY:
        return 128; // Could be used for something useful.

    case GLT_MASK:
        return maskTextures[tex->ofTypeID]->height;

    case GLT_SYSTEM: /// \fixme Do not assume!
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN:
    case GLT_LIGHTMAP:
    case GLT_FLARE:
        return 64;

    default:
        Con_Error("GLTexture_GetHeight: Internal error, "
                  "invalid material type %i.", (int) tex->type);
    }

    return 0; // Unreachable.
}

void GLTexture_ReleaseTextures(gltexture_t* tex)
{
    assert(tex);
    {
    gltexture_variantlist_node_t* node = tex->variants;
    while(node)
    {
        gltexturevariant_t* variant = node->variant;
        // Have we uploaded it?
        if(variant->glName)
        {
            // Delete and mark it not-loaded.
            glDeleteTextures(1, (const GLuint*) &variant->glName);
            variant->glName = 0;
        }
        node = node->next;
    }
    }
}

void GLTexture_SetMinMode(gltexture_t* tex, int minMode)
{
    assert(tex);
    {
    gltexture_variantlist_node_t* node = tex->variants;
    while(node)
    {
        gltexturevariant_t* variant = node->variant;
        // Have we uploaded it?
        if(variant->glName)
        {
            glBindTexture(GL_TEXTURE_2D, variant->glName);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMode);
        }
        node = node->next;
    }
    }
}

int GLTexture_IterateInstances(gltexture_t* tex,
    int (*callback)(gltexturevariant_t* instance, void* paramaters), void* paramaters)
{
    assert(tex);
    {
    int result = 0;
    if(callback)
    {
        gltexture_variantlist_node_t* node = tex->variants;
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

typedef struct {
    gltexturevariantspecification_t spec;
    gltexturevariant_t* chosen;
} choosegltexturevariantworker_paramaters_t;

static int chooseGLTextureVariantWorker(gltexturevariant_t* variant, void* context)
{
    choosegltexturevariantworker_paramaters_t* p = (choosegltexturevariantworker_paramaters_t*)context;
    if(variant->spec.loadFlags == p->spec.loadFlags &&
       variant->spec.flags     == p->spec.flags &&
       variant->spec.border    == p->spec.border)
    {   // This will do fine.
        p->chosen = variant;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
}

static gltexturevariant_t* chooseGLTextureVariant(gltexture_t* tex, void* context)
{
    material_load_params_t* mlParams = (material_load_params_t*) context;
    choosegltexturevariantworker_paramaters_t params;
    memset(&params.spec, 0, sizeof(params.spec));
    if(NULL != mlParams)
    {
        params.spec.loadFlags = mlParams->flags;
        params.spec.flags     = mlParams->tex.flags;
        params.spec.border    = mlParams->tex.border;
    }
    params.chosen = NULL;
    { gltexture_variantlist_node_t* node = tex->variants;
    while(node)
    {
        if(0 != chooseGLTextureVariantWorker(node->variant, &params))
            break;
        node = node->next;
    }}
    return params.chosen;
}

typedef struct {
    gltexturevariantspecification_t spec;
    gltexturevariant_t* chosen;
} choosedetailgltexturevariantworker_paramaters_t;

static int chooseDetailGLTextureVariantWorker(gltexturevariant_t* variant, void* context)
{
    assert(variant && context);
    {
    choosedetailgltexturevariantworker_paramaters_t* p = (choosedetailgltexturevariantworker_paramaters_t*) context;
    if(variant->spec.type.detail.contrast == p->spec.type.detail.contrast)
    {   // This will do fine.
        p->chosen = variant;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
    }
}

static gltexturevariant_t* chooseDetailGLTextureVariant(gltexture_t* tex, void* context)
{
    assert(tex && context);
    {
    choosedetailgltexturevariantworker_paramaters_t params;
    memset(&params.spec, 0, sizeof(params.spec));
    params.spec.type.detail.contrast = *((float*) context);
    //params.spec.flags = GLTF_MONOCHROME;
    params.chosen = NULL;
    { gltexture_variantlist_node_t* node = tex->variants;
    while(node)
    {
        if(0 != chooseDetailGLTextureVariantWorker(node->variant, &params))
            break;
        node = node->next;
    }}
    return params.chosen;
    }
}

typedef struct {
    gltexturevariantspecification_t spec;
    gltexturevariant_t* chosen;
} choosespritegltexturevariantworker_paramaters_t;

static int chooseSpriteGLTextureVariantWorker(gltexturevariant_t* variant, void* context)
{
    choosespritegltexturevariantworker_paramaters_t* p = (choosespritegltexturevariantworker_paramaters_t*) context;
    if(variant->spec.flags               == p->spec.flags &&
       variant->spec.loadFlags           == p->spec.loadFlags &&
       variant->spec.border              == p->spec.border &&
       variant->spec.type.sprite.pSprite == p->spec.type.sprite.pSprite &&
       variant->spec.type.sprite.tmap    == p->spec.type.sprite.tmap &&
       variant->spec.type.sprite.tclass  == p->spec.type.sprite.tclass)
    {   // This will do fine.
        p->chosen = variant;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
}

static gltexturevariant_t* chooseSpriteGLTextureVariant(gltexture_t* tex, void* context)
{
    material_load_params_t* mlParams = (material_load_params_t*) context;
    choosespritegltexturevariantworker_paramaters_t params;
    memset(&params.spec, 0, sizeof(params.spec));
    if(NULL != mlParams)
    {
        params.spec.type.sprite.tmap = mlParams->tmap;
        params.spec.type.sprite.tclass = mlParams->tclass;
        params.spec.type.sprite.pSprite = mlParams->pSprite;
        params.spec.flags = mlParams->tex.flags;
        params.spec.border = mlParams->tex.border;
        params.spec.loadFlags = mlParams->flags;
    }
    params.chosen = NULL;
    { gltexture_variantlist_node_t* node = tex->variants;
    while(node)
    {
        if(0 != chooseSpriteGLTextureVariantWorker(node->variant, &params))
            break;
        node = node->next;
    }}
    return params.chosen;
}

const dduri_t* GLTexture_SearchPath(const gltexture_t* tex)
{
    assert(tex);
    switch(tex->type)
    {
    case GLT_SYSTEM: {
        systex_t* sysTex = sysTextures[tex->ofTypeID];
        return sysTex->external;
      }
    /*case GLT_FLAT:
        tmpResult = GL_LoadFlat(&image, texInst->tex, context);
        break;
    case GLT_DOOMTEXTURE:
        tmpResult = GL_LoadDoomTexture(&image, texInst->tex, context);
        break;
    case GLT_DOOMPATCH:
        tmpResult = GL_LoadDoomPatch(&image, texInst->tex, context);
        break;
    case GLT_SPRITE:
        tmpResult = GL_LoadSprite(&image, texInst->tex, context);
        break;
    case GLT_DETAIL:
        tmpResult = GL_LoadDetailTexture(&image, texInst->tex, context);
        break;*/
    case GLT_SHINY: {
        shinytex_t* sTex = shinyTextures[tex->ofTypeID];
        return sTex->external;
      }
    case GLT_MASK: {
        masktex_t* mTex = maskTextures[tex->ofTypeID];
        return mTex->external;
      }
    case GLT_MODELSKIN:
    case GLT_MODELSHINYSKIN: {
        skinname_t* sn = &skinNames[tex->ofTypeID];
        return sn->path;
      }
    case GLT_LIGHTMAP: {
        lightmap_t* lmap = lightMaps[tex->ofTypeID];
        return lmap->external;
      }
    case GLT_FLARE: {
        flaretex_t* fTex = flareTextures[tex->ofTypeID];
        return fTex->external;
      }
    default:
        Con_Error("GLTexture::SearchPath: Unknown texture type %i.", (int) tex->type);
        return NULL; // Unreachable.
    }
}

static gltexturevariant_t* findSuitableVariant(gltexture_t* tex, void* context)
{
    switch(tex->type)
    {
    case GLT_DETAIL:
        return chooseDetailGLTextureVariant(tex, context);
    case GLT_SPRITE:
        return chooseSpriteGLTextureVariant(tex, context);
    default:
        return chooseGLTextureVariant(tex, context);
    }
}

static byte loadSourceImage(image_t* img, const gltexturevariant_t* tex, void* context)
{
    assert(img && tex);
    {
    const gltexture_t* generalCase = tex->generalCase;
    byte loadResult = 0;
    switch(generalCase->type)
    {
    case GLT_FLAT:
        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || GLTexture_IsFromIWAD(generalCase)))
        {
            flat_t* flat = R_GetFlatForIdx(generalCase->ofTypeID);
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;
            assert(NULL != flat);
            // First try the flats namespace then the old-fashioned "flat-name" in the textures namespace.
            Str_Init(&searchPath); Str_Appendf(&searchPath, FLATS_RESOURCE_NAMESPACE_NAME":%s;" TEXTURES_RESOURCE_NAMESPACE_NAME":flat-%s;", flat->name, flat->name);
            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadFlatLump(img, generalCase, context);
        break;
    case GLT_DOOMPATCH:
        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || GLTexture_IsFromIWAD(generalCase)))
        {
            material_load_params_t* params = (material_load_params_t*) context;
            const patchtex_t* p = R_PatchTextureForIndex(generalCase->ofTypeID);
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;
            assert(NULL != p);
            Str_Init(&searchPath);
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", W_LumpName(p->lump));
            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadDoomPatchLump(img, generalCase, context);
        break;
    case GLT_SPRITE:
        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            material_load_params_t* params = (material_load_params_t*) context;
            const spritetex_t* sprTex = R_SpriteTextureForIndex(generalCase->ofTypeID);
            ddstring_t searchPath, suffix = { "-ck" };
            int tclass = 0, tmap = 0;
            boolean pSprite = false;

            assert(NULL != sprTex);

            if(params)
            {
                tmap = params->tmap;
                tclass = params->tclass;
                pSprite = params->pSprite;
            }

            // Prefer psprite or translated versions if available.
            Str_Init(&searchPath);
            if(pSprite)
            {
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-hud;", sprTex->name);
            }
            else if(tmap || tclass)
            {   // Translated.
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-table%i%i;", sprTex->name, tclass, tmap);
            }
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s", sprTex->name);

            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadSpriteLump(img, generalCase, context);
        break;
    case GLT_DETAIL: {
        const detailtex_t* dTex;
        assert(generalCase->ofTypeID >= 0 && generalCase->ofTypeID < numDetailTextures);
        dTex = detailTextures[generalCase->ofTypeID];
        if(dTex->isExternal)
        {
            ddstring_t* searchPath = Uri_ComposePath(dTex->filePath);
            loadResult = GL_LoadExtTextureEX(img, Str_Text(searchPath), NULL, false);
            Str_Delete(searchPath);
        }
        else
        {
            loadResult = GL_LoadDetailTextureLump(img, generalCase, context);
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
        ddstring_t* searchPath = Uri_ComposePath(GLTexture_SearchPath(generalCase));
        loadResult = GL_LoadExtTextureEX(img, Str_Text(searchPath), NULL, false);
        Str_Delete(searchPath);
        break;
      }
    default:
        Con_Error("GLTexture::Prepare: Unknown texture type %i.", (int) generalCase->type);
        return 0; // Unreachable.
    }
    return loadResult;
    }
}

static byte prepareVariant(gltexturevariant_t* tex, void* context)
{
    assert(tex);
    {
    const gltexture_t* generalCase = tex->generalCase;
    boolean monochrome    = ((generalCase->type != GLT_DETAIL && context)? (((material_load_params_t*) context)->tex.flags & GLTF_MONOCHROME) != 0 : false);
    boolean noCompression = ((generalCase->type != GLT_DETAIL && context)? (((material_load_params_t*) context)->tex.flags & GLTF_NO_COMPRESSION) != 0 : false);
    boolean scaleSharp    = ((generalCase->type != GLT_DETAIL && context)? (((material_load_params_t*) context)->tex.flags & GLTF_UPSCALE_AND_SHARPEN) != 0 : false);
    boolean noSmartFilter = false, didDefer = false;
    byte loadResult = 0;
    image_t image;

    // Load in the raw source image.
    if(generalCase->type == GLT_DOOMTEXTURE)
    {
        loadResult = GL_LoadDoomTexture(&image, generalCase, context);
    }
    else
    {
        loadResult = loadSourceImage(&image, tex, context);
    }

    if(0 == loadResult)
    {   // Not found/failed load.
        return loadResult;
    }

    if(image.pixelSize == 1)
    {
        if(monochrome && !scaleSharp && (generalCase->type == GLT_DOOMPATCH || generalCase->type == GLT_SPRITE))
            GL_DeSaturatePalettedImage(image.pixels, R_GetColorPalette(0), image.width, image.height);

        if(generalCase->type == GLT_DETAIL)
        {
            float baMul, hiMul, loMul;
            EqualizeLuma(image.pixels, image.width, image.height, &baMul, &hiMul, &loMul);
            if(verbose && (baMul != 1 || hiMul != 1 || loMul != 1))
            {
                Con_Message("GLTexture_Prepare: Equalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).\n",
                            generalCase->name, baMul, hiMul, loMul);
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

            if(monochrome && (generalCase->type == GLT_DOOMPATCH || generalCase->type == GLT_SPRITE))
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
            if(monochrome && (generalCase->type == GLT_DOOMPATCH || generalCase->type == GLT_SPRITE))
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
        if(monochrome && generalCase->type == GLT_DOOMPATCH && image.pixelSize > 2)
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
            Con_Message("GLTexture_Prepare: Warning, non RGB(A) texture larger than max size (%ix%i bpp%i).\n",
                        image.width, image.height, image.pixelSize);
        }
    }

    // Lightmaps and flare textures should always be monochrome images.
    if((generalCase->type == GLT_LIGHTMAP || (generalCase->type == GLT_FLARE && image.pixelSize != 4)) &&
       !(image.flags & IMGF_IS_MASKED))
    {
        // An alpha channel is required. If one is not in the image data, we'll generate it.
        GL_ConvertToAlpha(&image, true);
    }

    {
    int magFilter, minFilter, anisoFilter, wrapS, wrapT, grayMipmap = 0, flags = 0;
    dgltexformat_t dglFormat;
    boolean alphaChannel;

    // Disable compression?
    if(noCompression || (image.width < 128 && image.height < 128) || generalCase->type == GLT_FLARE || generalCase->type == GLT_SHINY)
        flags |= TXCF_NO_COMPRESSION;

    if(!(generalCase->type == GLT_MASK || generalCase->type == GLT_SHINY || generalCase->type == GLT_LIGHTMAP) &&
       (image.pixelSize > 2 || generalCase->type == GLT_MODELSKIN))
        flags |= TXCF_APPLY_GAMMACORRECTION;

    if(generalCase->type == GLT_SPRITE /*|| generalCase->type == GLT_DOOMPATCH*/)
        flags |= TXCF_UPLOAD_ARG_NOSTRETCH;

    if(!monochrome && !(generalCase->type == GLT_DETAIL || generalCase->type == GLT_SYSTEM || generalCase->type == GLT_SHINY || generalCase->type == GLT_MASK))
        flags |= TXCF_EASY_UPLOAD;

    if(!monochrome)
    {
        if(generalCase->type == GLT_SPRITE || generalCase->type == GLT_MODELSKIN || generalCase->type == GLT_MODELSHINYSKIN)
        {
            if(image.pixelSize > 1)
                flags |= TXCF_UPLOAD_ARG_RGBDATA;
        }
        else if(image.pixelSize > 2 && !(generalCase->type == GLT_SHINY || generalCase->type == GLT_MASK || generalCase->type == GLT_LIGHTMAP))
            flags |= TXCF_UPLOAD_ARG_RGBDATA;
    }

    if(generalCase->type == GLT_DETAIL)
    {
        /**
         * Detail textures are faded to gray depending on the contrast
         * factor. The texture is also progressively faded towards gray
         * when each mipmap level is loaded.
         */
        grayMipmap = MINMAX_OF(0, *((float*)context) * 255, 255);
        flags |= TXCF_GRAY_MIPMAP;
    }
    else if(!(generalCase->type == GLT_SHINY || generalCase->type == GLT_DOOMPATCH ||
              generalCase->type == GLT_LIGHTMAP || generalCase->type == GLT_FLARE ||
             (generalCase->type == GLT_SPRITE && context && ((material_load_params_t*)context)->pSprite)))
        flags |= TXCF_MIPMAP;

    if(generalCase->type == GLT_DOOMTEXTURE || generalCase->type == GLT_DOOMPATCH ||
       generalCase->type == GLT_SPRITE || generalCase->type == GLT_FLAT)
        alphaChannel = ((/*loadResult == 2 &&*/ image.pixelSize == 4) ||
                        (/*loadResult == 1 &&*/ image.pixelSize == 1 && (image.flags & IMGF_IS_MASKED)));
    else
        alphaChannel = image.pixelSize != 3 && !(generalCase->type == GLT_MASK || generalCase->type == GLT_SHINY);

    if(alphaChannel)
        flags |= TXCF_UPLOAD_ARG_ALPHACHANNEL;

    if(noSmartFilter)
        flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

    if(monochrome)
    {
        dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
    }
    else if(generalCase->type == GLT_FLAT || generalCase->type == GLT_DOOMTEXTURE || generalCase->type == GLT_DOOMPATCH || generalCase->type == GLT_SPRITE)
    {
        dglFormat = ( image.pixelSize > 1?
                        ( alphaChannel? DGL_RGBA : DGL_RGB) :
                        ( alphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8 ) );
    }
    else if(generalCase->type == GLT_MODELSKIN || generalCase->type == GLT_MODELSHINYSKIN)
    {
        dglFormat = ( alphaChannel? DGL_RGBA : DGL_RGB );
    }
    else
    {
        dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                      image.pixelSize == 3 ? DGL_RGB :
                      image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE );
    }

    if(generalCase->type == GLT_FLAT || generalCase->type == GLT_DOOMTEXTURE || generalCase->type == GLT_MASK)
        magFilter = glmode[texMagMode];
    else if(generalCase->type == GLT_SPRITE)
        magFilter = filterSprites ? GL_LINEAR : GL_NEAREST;
    else
        magFilter = GL_LINEAR;

    if(generalCase->type == GLT_DETAIL)
        minFilter = GL_LINEAR_MIPMAP_LINEAR;
    else if(generalCase->type == GLT_DOOMPATCH || (generalCase->type == GLT_SPRITE && context && ((material_load_params_t*)context)->pSprite))
        minFilter = GL_NEAREST;
    else if(generalCase->type == GLT_LIGHTMAP || generalCase->type == GLT_FLARE || generalCase->type == GLT_SHINY)
        minFilter = GL_LINEAR;
    else
        minFilter = glmode[mipmapping];

    if(generalCase->type == GLT_DOOMPATCH || generalCase->type == GLT_FLARE || (generalCase->type == GLT_SPRITE && context && ((material_load_params_t*)context)->pSprite))
        anisoFilter = 0 /*no anisotropic filtering*/;
    else
        anisoFilter = texAniso; /// \fixme is "best" truely a suitable default for ALL texture types?

    if(generalCase->type == GLT_DOOMPATCH || generalCase->type == GLT_SPRITE || generalCase->type == GLT_LIGHTMAP || generalCase->type == GLT_FLARE)
        wrapS = wrapT = GL_CLAMP_TO_EDGE;
    else
        wrapS = wrapT = GL_REPEAT;

    tex->glName = GL_NewTextureWithParams3(dglFormat, image.width, image.height,
        image.pixels, flags, grayMipmap, minFilter, magFilter, anisoFilter, wrapS,
        wrapT, &didDefer);

#ifdef _DEBUG
    if(!didDefer)
        Con_Message("GLTexture_Prepare: Uploaded \"%s\" (%i) while not busy! "
                    "Should be precached in busy mode?\n", generalCase->name, tex->glName);
#endif

    /**
     * Calculate texture coordinates based on the image dimensions. The
     * coordinates are calculated as width/CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     */
    { float* tc = tex->coords;
    if((generalCase->type == GLT_SPRITE) && GL_state.features.texNonPowTwo &&
       ((context && ((material_load_params_t*)context)->pSprite) ||
        !(flags & TXCF_UPLOAD_ARG_NOSTRETCH)) &&
       !(image.width < MINTEXWIDTH || image.height < MINTEXHEIGHT))
    {
        tc[0] = tc[1] = 1;
    }
    else
    {
        int pw = M_CeilPow2(image.width), ph = M_CeilPow2(image.height);
        tc[0] =  image.width / (float) pw;
        tc[1] = image.height / (float) ph;
    }}

    tex->isMasked = (image.flags & IMGF_IS_MASKED) != 0;

    if(!(generalCase->type == GLT_DETAIL || generalCase->type == GLT_SPRITE) &&
       context && (((material_load_params_t*)context)->flags & MLF_LOAD_AS_SKY))
    {
        averagecolor_analysis_t* avgTopColor = Z_Malloc(sizeof(*avgTopColor), PU_APPSTATIC, 0);
        tex->analyses[GLTA_SKY_TOPCOLOR] = avgTopColor;
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

    if(generalCase->type == GLT_SPRITE)
    {
        pointlight_analysis_t* pl = Z_Malloc(sizeof(*pl), PU_APPSTATIC, 0);
        tex->analyses[GLTA_SPRITE_AUTOLIGHT] = pl;
        if(!(context && ((material_load_params_t*)context)->pSprite))
        {   // Calculate light source properties.
            GL_CalcLuminance(image.pixels, image.width, image.height, image.pixelSize, 0,
                             &pl->originX, &pl->originY, pl->color, &pl->brightMul);
        }
        else
        {
            memset(pl, 0, sizeof(pointlight_analysis_t));
        }
    }

    if(generalCase->type == GLT_FLAT || generalCase->type == GLT_DOOMTEXTURE)
    {
        ambientlight_analysis_t* al = Z_Malloc(sizeof(*al), PU_APPSTATIC, 0);
        tex->analyses[GLTA_WORLD_AMBIENTLIGHT] = al;
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
    }

    GL_DestroyImagePixels(&image);
    return loadResult;
    }
}

static void applyVariantSpecification(gltexturevariantspecification_t* spec,
    gltexture_type_t type, const void* context)
{
    assert(spec && VALID_GLTEXTURE_TYPE(type));
    if(type == GLT_DETAIL)
    {
        assert(context);
        spec->type.detail.contrast = *((const float*)context);
        return;
    }

    if(!context)
        return;

    {
    const material_load_params_t* params = (const material_load_params_t*) context;
    spec->flags = params->tex.flags;
    spec->loadFlags = params->flags;
    spec->border = params->tex.border;
    if(type == GLT_SPRITE)
    {
        spec->type.sprite.tmap = params->tmap;
        spec->type.sprite.tclass = params->tclass;
        spec->type.sprite.pSprite = params->pSprite;
    }
    }
}

static initializeVariant(gltexturevariant_t* variant, gltexture_t* generalCase,
    const void* context)
{
    assert(variant && generalCase);
    memset(variant, 0, sizeof(gltexturevariant_t));
    variant->generalCase = generalCase;
    applyVariantSpecification(&variant->spec, generalCase->type, context);
}

gltexturevariant_t* GLTexture_Prepare(gltexture_t* tex, void* context, byte* result)
{
    assert(tex && VALID_GLTEXTURE_TYPE(tex->type));
    {
    gltexturevariant_t* variant = NULL;
    boolean variantIsNew = false;
    float contrast = 1;
    byte loadResult;

    // Rationalize usage context paramaters.
    if(tex->type == GLT_DETAIL)
    {   // Round off the contrast to nearest 1/10
        if(context)
            contrast = (int) ((*((float*) context) + .05f) * 10) / 10.f;
        context = (void*) &contrast;
    }

    // Have we already registered and prepared a suitable variant?
    variant = findSuitableVariant(tex, context);
    if(NULL != variant && 0 != variant->glName)
    {   // Already prepared.
        if(result) *result = 0;
        return variant;
    }

    // Do we need to allocate a variant?
    if(NULL == variant)
    {
        variant = Z_Malloc(sizeof(*variant), PU_APPSTATIC, 0);
        initializeVariant(variant, tex, context);
        variantIsNew = true;
    }
    // (Re)Prepare the variant according to the usage context.
    loadResult = prepareVariant(variant, context);
    if(variantIsNew)
    {
        if(0 != loadResult)
            GLTexture_AddVariant(tex, variant);
        else
            Z_Free(variant);
    }

    if(result)
        *result = loadResult;
    return variant;
    }
}
