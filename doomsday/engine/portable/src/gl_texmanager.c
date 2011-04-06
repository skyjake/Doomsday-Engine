/**\file gl_texmanager.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2002 Graham Jackson <no contact email published>
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
 * Texture management routines.
 *
 * Much of this stuff actually belongs in Refresh.
 * This file needs to be split into smaller portions.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>

#ifdef UNIX
#   include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_play.h"

#include "def_main.h"
#include "ui_main.h"
#include "p_particle.h"

#include "colorpalette.h"
#include "image.h"
#include "texturecontent.h"
#include "texture.h"
#include "texturevariant.h"

typedef struct {
    const char* name; // Format/handler name.
    const char* ext; // Expected file extension.
    boolean (*loadFunc)(image_t* img, DFILE* file);
    const char* (*getLastErrorFunc) (void);
} imagehandler_t;

typedef struct texturevariantspecificationlist_node_s {
    struct texturevariantspecificationlist_node_s* next;
    texturevariantspecification_t* spec;
} texturevariantspecificationlist_node_t;

typedef texturevariantspecificationlist_node_t variantspecificationlist_t;

typedef struct texturenamespace_hashnode_s {
    struct texturenamespace_hashnode_s* next;
    uint textureIndex; // 1-based index
} texturenamespace_hashnode_t;

#define TEXTURENAMESPACE_HASH_SIZE (512)

typedef struct texturenamespace_s {
    texturenamespace_hashnode_t* hashTable[TEXTURENAMESPACE_HASH_SIZE];
} texturenamespace_t;

D_CMD(LowRes);
D_CMD(ResetTextures);
D_CMD(MipMap);

void GL_DoResetDetailTextures(const cvar_t* /*cvar*/);

static void calcGammaTable(void);

static boolean tryLoadPCX(image_t* img, DFILE* file);
static boolean tryLoadPNG(image_t* img, DFILE* file);
static boolean tryLoadTGA(image_t* img, DFILE* file);

int ratioLimit = 0; // Zero if none.
boolean fillOutlines = true;
int monochrome = 0; // desaturate a patch (average colours)
int upscaleAndSharpenPatches = 0;
int useSmartFilter = 0; // Smart filter mode (cvar: 1=hq2x)
int mipmapping = 5, filterUI = 1, texQuality = TEXQ_BEST;
int filterSprites = true;
int texMagMode = 1; // Linear.
int texAniso = -1; // Use best.

boolean noHighResTex = false;
boolean noHighResPatches = false;
boolean highResWithPWAD = false;
byte loadExtAlways = false; // Always check for extres (cvar)

float texGamma = 0;
byte gammaTable[256];

int glmode[6] = // Indexed by 'mipmapping'.
{
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR
};

// Names of the dynamic light textures.
ddtexture_t lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
ddtexture_t sysFlareTextures[NUM_SYSFLARE_TEXTURES];

static boolean texInited = false; // Init done.

// Image file handlers.
static const imagehandler_t handlers[] = {
    { "PNG", "png", tryLoadPNG, PNG_LastError },
    { "TGA", "tga", tryLoadTGA, TGA_LastError },
    { "PCX", "pcx", tryLoadPCX, PCX_LastError },
    { 0 } // Terminate.
};

static variantspecificationlist_t* variantSpecs[TEXTUREVARIANTSPECIFICATIONTYPE_COUNT];

static int texturesCount;
static texture_t** textures;
static texturenamespace_t textureNamespaces[TEXTURENAMESPACE_COUNT];

void GL_TexRegister(void)
{
    C_VAR_INT("rend-tex", &renderTextures, CVF_NO_ARCHIVE, 0, 2);
    C_VAR_INT("rend-tex-detail", &r_detail, 0, 0, 1);
    C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1);
    C_VAR_FLOAT("rend-tex-detail-scale", &detailScale, CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-tex-detail-strength", &detailFactor, 0, 0, 10, GL_DoResetDetailTextures);
    C_VAR_BYTE2("rend-tex-external-always", &loadExtAlways, 0, 0, 1, GL_DoTexReset);
    C_VAR_INT("rend-tex-filter-anisotropic", &texAniso, 0, -1, 4);
    C_VAR_INT("rend-tex-filter-mag", &texMagMode, 0, 0, 1);
    C_VAR_INT2("rend-tex-filter-smart", &useSmartFilter, 0, 0, 1, GL_DoTexReset);
    C_VAR_INT("rend-tex-filter-sprite", &filterSprites, 0, 0, 1);
    C_VAR_INT("rend-tex-filter-ui", &filterUI, 0, 0, 1);
    C_VAR_FLOAT2("rend-tex-gamma", &texGamma, 0, 0, 1, GL_DoUpdateTexGamma);
    C_VAR_INT2("rend-tex-mipmap", &mipmapping, CVF_PROTECTED, 0, 5, GL_DoTexReset);
    C_VAR_INT2("rend-tex-quality", &texQuality, 0, 0, 8, GL_DoTexReset);

    C_CMD_FLAGS("lowres", "", LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("mipmap", "i", MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("texreset", "", ResetTextures, CMDF_NO_DEDICATED);
}

static texturevariantspecification_t* copyVariantSpecification(
    const texturevariantspecification_t* tpl)
{
    texturevariantspecification_t* spec;
    if(NULL == (spec = (texturevariantspecification_t*) malloc(sizeof(*spec))))
        Con_Error("Textures::copyVariantSpecification: Failed on allocation of "
                  "%lu bytes for new TextureVariantSpecification.",
                  (unsigned long) sizeof(*spec));
    memcpy(spec, tpl, sizeof(texturevariantspecification_t));
    if(TS_GENERAL(tpl)->flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        colorpalettetranslationspecification_t* cpt;
        if(NULL == (cpt = (colorpalettetranslationspecification_t*) malloc(sizeof(*cpt))))
            Con_Error("Textures::copyVariantSpecification: Failed on allocation of "
                  "%lu bytes for new ColorPaletteTranslationSpecification.",
                  (unsigned long) sizeof(*cpt));
        memcpy(cpt, TS_GENERAL(tpl)->translated, sizeof(colorpalettetranslationspecification_t));
        TS_GENERAL(spec)->translated = cpt;
    }
    return spec;
}

static texturevariantspecification_t* copyDetailVariantSpecification(
    const texturevariantspecification_t* tpl)
{
    texturevariantspecification_t* spec;
    if(NULL == (spec = (texturevariantspecification_t*) malloc(sizeof(*spec))))
        Con_Error("Textures::copyDetailVariantSpecification: Failed on allocation of "
                  "%lu bytes for new TextureVariantSpecification.",
                  (unsigned long) sizeof(*spec));
    memcpy(spec, tpl, sizeof(texturevariantspecification_t));
    return spec;
}

static int compareVariantSpecifications(const variantspecification_t* a,
    const variantspecification_t* b)
{
    /// \todo We can be a bit cleverer here...
    if(a->context != b->context)
        return 1;
    if(a->flags   != b->flags)
        return 1;
    if(a->wrapS   != b->wrapS || a->wrapT != b->wrapT)
        return 1;
    if(a->mipmapped != b->mipmapped)
        return 1;
    if(a->noStretch != b->noStretch)
        return 1;
    if(a->anisoFilter != b->anisoFilter)
        return 1;
    if(a->gammaCorrection != b->gammaCorrection)
        return 1;
    if(a->toAlpha != b->toAlpha)
        return 1;
    if(a->border  != b->border)
        return 1;
    if(a->flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        const colorpalettetranslationspecification_t* cptA = a->translated;
        const colorpalettetranslationspecification_t* cptB = b->translated;
        assert(cptA && cptB);
        if(cptA->tClass != cptB->tClass)
            return 1;
        if(cptA->tMap   != cptB->tMap)
            return 1;
    }
    return 0; // Equal.
}

static int compareDetailVariantSpecifications(const detailvariantspecification_t* a,
    const detailvariantspecification_t* b)
{
    if(a->contrast != b->contrast)
        return 1;
    return 0; // Equal.
}

static colorpalettetranslationspecification_t* applyColorPaletteTranslationSpecification(
    colorpalettetranslationspecification_t* spec, int tClass, int tMap)
{
    assert(texInited && spec);

    spec->tClass = MAX_OF(0, tClass);
    spec->tMap   = MAX_OF(0, tMap);

#if _DEBUG
    if(0 == tClass && 0 == tMap)
        Con_Message("Warning:applyColorPaletteTranslationSpecification: Applied "
            "unnecessary zero-translation (tClass:0 tMap:0).\n");
#endif

    return spec;
}

static variantspecification_t* applyVariantSpecification(
    variantspecification_t* spec, texturevariantusagecontext_t tc, int flags,
    byte border, colorpalettetranslationspecification_t* colorPaletteTranslationSpec,
    int wrapS, int wrapT, int anisoFilter, boolean mipmapped, boolean gammaCorrection,
    boolean noStretch, boolean toAlpha)
{
    assert(texInited && spec && (tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc)));

    flags &= ~TSF_INTERNAL_MASK;

    spec->context = tc;
    spec->flags = flags;
    spec->border = (flags & TSF_UPSCALE_AND_SHARPEN)? 1 : border;
    spec->mipmapped = mipmapped;
    spec->wrapS = wrapS;
    spec->wrapT = wrapT;
    spec->anisoFilter = anisoFilter < 0? -1 : MINMAX_OF(0, anisoFilter, 4);
    spec->gammaCorrection = gammaCorrection;
    spec->noStretch = noStretch;
    spec->toAlpha = toAlpha;
    if(NULL != colorPaletteTranslationSpec)
    {
        spec->flags |= TSF_HAS_COLORPALETTE_XLAT;
        spec->translated = colorPaletteTranslationSpec;
    }
    else
    {
        spec->translated = NULL;
    }

    return spec;
}

static detailvariantspecification_t* applyDetailVariantSpecification(
    detailvariantspecification_t* spec, float contrast)
{
    assert(texInited && spec);
    // Round off contrast to nearest 1/10
    spec->contrast = 255 * (int)MINMAX_OF(0, contrast * 10 + .5f, 10) * (1/10.f);
    return spec;
}

static texturevariantspecification_t* linkVariantSpecification(
    texturevariantspecificationtype_t type, texturevariantspecification_t* spec)
{
    assert(texInited && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type) && spec);
    {
    texturevariantspecificationlist_node_t* node;
    if(NULL == (node = (texturevariantspecificationlist_node_t*) malloc(sizeof(*node))))
        Con_Error("Textures::linkVariantSpecification: Failed on allocation of "
                  "%lu bytes for new TextureVariantSpecificationListNode.",
                  (unsigned long) sizeof(*node));
    node->spec = spec;
    node->next = variantSpecs[type];
    variantSpecs[type] = (variantspecificationlist_t*)node;
    return spec;
    }
}

static texturevariantspecification_t* findVariantSpecification(
    texturevariantspecificationtype_t type, const texturevariantspecification_t* tpl,
    boolean canCreate)
{
    assert(texInited && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type) && tpl);
    {
    texturevariantspecificationlist_node_t* node;
    for(node = variantSpecs[type]; node; node = node->next)
    {
        if(!GL_CompareTextureVariantSpecifications(node->spec, tpl))
            return node->spec;
    }
    if(!canCreate)
        return NULL;
    switch(type)
    {
    case TST_GENERAL:
        return linkVariantSpecification(type, copyVariantSpecification(tpl));
    case TST_DETAIL:
        return linkVariantSpecification(type, copyDetailVariantSpecification(tpl));
    }
    // Unreachable (hopefully).
    assert(1);
    return NULL;
    }
}

static texturevariantspecification_t* getVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int anisoFilter, boolean mipmapped,
    boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    assert(texInited);
    {
    static texturevariantspecification_t tpl;
    static colorpalettetranslationspecification_t cptTpl;
    boolean haveCpt = false;

    tpl.type = TST_GENERAL;
    if(0 != tClass || 0 != tMap)
    {   // A color palette translation spec is required.
        applyColorPaletteTranslationSpecification(&cptTpl, tClass, tMap);
        haveCpt = true;
    }

    applyVariantSpecification(TS_GENERAL(&tpl), tc, flags, border, haveCpt? &cptTpl : NULL,
        wrapS, wrapT, anisoFilter, mipmapped, gammaCorrection, noStretch, toAlpha);

    // Retrieve a concrete version of the rationalized specification.
    return findVariantSpecification(tpl.type, &tpl, true);
    }
}

static texturevariantspecification_t* getDetailVariantSpecificationForContext(
    float contrast)
{
    static texturevariantspecification_t tpl;
    assert(texInited);
    tpl.type = TST_DETAIL;
    applyDetailVariantSpecification(TS_DETAIL(&tpl), contrast);
    return findVariantSpecification(tpl.type, &tpl, true);
}

static void destroyVariantSpecifications(void)
{
    assert(texInited);
    { int i;
    for(i = 0; i < TEXTUREVARIANTSPECIFICATIONTYPE_COUNT; ++i)
    {
        texturevariantspecificationlist_node_t* node = variantSpecs[i];
        while(node)
        {
            texturevariantspecificationlist_node_t* next = node->next;
            if(node->spec->type == TST_GENERAL &&
               (TS_GENERAL(node->spec)->flags & TSF_HAS_COLORPALETTE_XLAT))
                free(TS_GENERAL(node->spec)->translated);
            free(node->spec);
            free(node);
            node = next;
        }
        variantSpecs[i] = NULL;
    }}
}

typedef struct {
    texturevariantspecificationtype_t type;
    const texturevariantspecification_t* spec;
    texturevariant_t* chosen;
} choosetexturevariantworker_paramaters_t;

static int chooseTextureVariantWorker(texturevariant_t* variant, void* context)
{
    assert(variant && context);
    {
    choosetexturevariantworker_paramaters_t* p =
        (choosetexturevariantworker_paramaters_t*) context;
    const texturevariantspecification_t* cand = TextureVariant_Spec(variant);
    if(!GL_CompareTextureVariantSpecifications(cand, p->spec))
    {   // This will do fine.
        p->chosen = variant;
        return 1; // Stop iteration.
    }
    return 0; // Continue iteration.
    }
}

static texturevariant_t* chooseTextureVariant(texture_t* tex,
    const texturevariantspecification_t* spec)
{
    assert(texInited && tex && spec);
    {
    choosetexturevariantworker_paramaters_t params;
    params.type = spec->type;
    params.spec = spec;
    params.chosen = NULL;
    Texture_IterateVariants(tex, chooseTextureVariantWorker, &params);
    return params.chosen;
    }
}

int releaseVariantGLTexture(texturevariant_t* variant, void* paramaters)
{
    // Have we uploaded yet?
    if(TextureVariant_IsUploaded(variant))
    {
        // Delete and mark it not-loaded.
        DGLuint glName = TextureVariant_GLName(variant);
        glDeleteTextures(1, (const GLuint*) &glName);
        TextureVariant_SetGLName(variant, 0);
        TextureVariant_FlagUploaded(variant, false);
    }
    return 0; // Continue iteration.
}

static void destroyTextures(void)
{
    assert(texInited);
    if(texturesCount)
    {
        int i;
        for(i = 0; i < texturesCount; ++i)
        {
            texture_t* tex = textures[i];
            GL_ReleaseGLTexturesForTexture(tex);
            Texture_Destruct(tex);
        }
        free(textures);
    }
    textures = 0;
    texturesCount = 0;

    { uint i;
    for(i = 0; i < TEXTURENAMESPACE_COUNT; ++i)
    {
        texturenamespace_t* texNamespace = &textureNamespaces[i];
        uint j;
        for(j = 0; j < TEXTURENAMESPACE_HASH_SIZE; ++j)
        {
            texturenamespace_hashnode_t* node = texNamespace->hashTable[j];
            while(node)
            {
                texturenamespace_hashnode_t* next = node->next;
                free(node);
                node = next;
            }
        }
        memset(texNamespace->hashTable, 0, sizeof(textureNamespaces[i].hashTable));
    }}
}

static const dduri_t* searchPath(texturenamespaceid_t texNamespace, int typeIndex)
{
    switch(texNamespace)
    {
    case TN_SYSTEM: {
        systex_t* sysTex = sysTextures[typeIndex];
        return sysTex->external;
      }
    /*case TN_FLATS:
        tmpResult = GL_LoadFlat(&image, texInst->tex, context);
        break;
    case TN_TEXTURES:
        tmpResult = GL_LoadDoomTexture(&image, texInst->tex, context);
        break;
    case TN_PATCHES:
        tmpResult = GL_LoadDoomPatch(&image, texInst->tex, context);
        break;
    case TN_SPRITES:
        tmpResult = GL_LoadSprite(&image, texInst->tex, context);
        break;
    case TN_DETAILS:
        tmpResult = GL_LoadDetailTexture(&image, texInst->tex, context);
        break;*/
    case TN_REFLECTIONS: {
        shinytex_t* sTex = shinyTextures[typeIndex];
        return sTex->external;
      }
    case TN_MASKS: {
        masktex_t* mTex = maskTextures[typeIndex];
        return mTex->external;
      }
    case TN_MODELSKINS:
    case TN_MODELREFLECTIONSKINS: {
        skinname_t* sn = &skinNames[typeIndex];
        return sn->path;
      }
    case TN_LIGHTMAPS: {
        lightmap_t* lmap = lightmapTextures[typeIndex];
        return lmap->external;
      }
    case TN_FLAREMAPS: {
        flaretex_t* fTex = flareTextures[typeIndex];
        return fTex->external;
      }
    default:
        Con_Error("Texture::SearchPath: Unknown namespace %i.", (int) texNamespace);
        return NULL; // Unreachable.
    }
}

static byte loadSourceImage(image_t* img, const texture_t* tex,
    const texturevariantspecification_t* baseSpec)
{
    assert(img && tex && baseSpec);
    {
    const variantspecification_t* spec = TS_GENERAL(baseSpec);
    byte loadResult = 0;
    switch(Texture_Namespace(tex))
    {
    case TN_FLATS: {
        const flat_t* flat = R_FlatTextureByIndex(Texture_TypeIndex(tex));
        assert(NULL != flat);

        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || Texture_IsFromIWAD(tex)))
        {
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;

            // First try the flats namespace then the old-fashioned "flat-name"
            // in the textures namespace.
            Str_Init(&searchPath);
            Str_Appendf(&searchPath,
                FLATS_RESOURCE_NAMESPACE_NAME":%s;"
                TEXTURES_RESOURCE_NAMESPACE_NAME":flat-%s;", flat->name, flat->name);

            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
        {
            lumpnum_t lumpNum = W_CheckNumForName2(flat->name, true);
            loadResult = GL_LoadFlatLump(img, lumpNum);
        }
        break;
      }
    case TN_PATCHES: {
        const patchtex_t* pTex = R_PatchTextureByIndex(Texture_TypeIndex(tex));
        int tclass = 0, tmap = 0;

        assert(NULL != pTex);

        if(spec->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            assert(spec->translated);
            tclass = spec->translated->tClass;
            tmap   = spec->translated->tMap;
        }

        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || Texture_IsFromIWAD(tex)))
        {
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;
            Str_Init(&searchPath);
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", W_LumpName(pTex->lump));

            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
        {
            loadResult = GL_LoadPatchLump(img, pTex->lump, tclass, tmap, spec->border);
        }
        break;
      }
    case TN_SPRITES: {
        const spritetex_t* sprTex = R_SpriteTextureByIndex(Texture_TypeIndex(tex));
        int tclass = 0, tmap = 0;

        assert(NULL != sprTex);

        if(spec->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            assert(spec->translated);
            tclass = spec->translated->tClass;
            tmap   = spec->translated->tMap;
        }

        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            ddstring_t searchPath, suffix = { "-ck" };

            // Prefer psprite or translated versions if available.
            Str_Init(&searchPath);
            if(TC_PSPRITE_DIFFUSE == spec->context)
            {
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-hud;", sprTex->name);
            }
            else if(tclass || tmap)
            {
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-table%i%i;",
                    sprTex->name, tclass, tmap);
            }
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s", sprTex->name);

            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
        {
            lumpnum_t lumpNum = W_GetNumForName(sprTex->name);
            loadResult = GL_LoadPatchLump(img, lumpNum, tclass, tmap, spec->border);
        }
        break;
      }
    case TN_SYSTEM:
    case TN_REFLECTIONS:
    case TN_MASKS:
    case TN_LIGHTMAPS:
    case TN_FLAREMAPS:
    case TN_MODELSKINS:
    case TN_MODELREFLECTIONSKINS: {
        ddstring_t* path = Uri_ComposePath(searchPath(Texture_Namespace(tex), Texture_TypeIndex(tex)));
        loadResult = GL_LoadExtTextureEX(img, Str_Text(path), NULL, false);
        Str_Delete(path);
        break;
      }
    default:
        Con_Error("Textures::loadSourceImage: Unknown texture namespace %i.",
                  (int) Texture_Namespace(tex));
        return 0; // Unreachable.
    }
    return loadResult;
    }
}

static void prepareVariant(texturevariant_t* tex, image_t* image)
{
    assert(tex && image);
    {
    const variantspecification_t* spec = TS_GENERAL(TextureVariant_Spec(tex));
    boolean monochrome    = (spec->flags & TSF_MONOCHROME) != 0;
    boolean noCompression = (spec->flags & TSF_NO_COMPRESSION) != 0;
    boolean scaleSharp    = (spec->flags & TSF_UPSCALE_AND_SHARPEN) != 0;
    int wrapS = spec->wrapS, wrapT = spec->wrapT;
    int magFilter, minFilter, anisoFilter, grayMipmap = 0, flags = 0;
    boolean noSmartFilter = false, didDefer = false;
    dgltexformat_t dglFormat;
    float s, t;

    if(spec->toAlpha)
    {
        if(0 != image->palette)
        {   // Paletted.
            uint8_t* newPixels = GL_ConvertBuffer(image->pixels, image->width, image->height,
                ((image->flags & IMGF_IS_MASKED)? 2 : 1), image->palette, 3);
            free(image->pixels);
            image->pixels = newPixels;
            image->pixelSize = 3;
            image->palette = 0;
            image->flags &= ~IMGF_IS_MASKED;
        }

        GL_ConvertToLuminance(image, false);
        { long i, total = image->width * image->height;
        for(i = 0; i < total; ++i)
        {
            image->pixels[total + i] = image->pixels[i];
            image->pixels[i] = 255;
        }}
        image->pixelSize = 2;
    }
    else if(0 != image->palette)
    {
        if(monochrome && !scaleSharp)
            GL_DeSaturatePalettedImage(image->pixels, image->palette, image->width, image->height);

        if(scaleSharp)
        {
            int scaleMethod = GL_ChooseSmartFilter(image->width, image->height, 0);
            int numpels = image->width * image->height;
            boolean origMasked = (image->flags & IMGF_IS_MASKED) != 0;
            int origPalette = image->palette;
            uint8_t* newPixels;

            newPixels = GL_ConvertBuffer(image->pixels, image->width, image->height,
                ((image->flags & IMGF_IS_MASKED)? 2 : 1), image->palette, 4);
            if(newPixels != image->pixels)
            {
                free(image->pixels);
                image->pixels = newPixels;
                image->pixelSize = 4;
                image->palette = 0;
                image->flags &= ~IMGF_IS_MASKED;
            }

            if(monochrome)
                Desaturate(image->pixels, image->width, image->height, image->pixelSize);

            newPixels = GL_SmartFilter(scaleMethod, image->pixels, image->width, image->height,
                0, &image->width, &image->height);
            if(newPixels != image->pixels)
            {
                free(image->pixels);
                image->pixels = newPixels;
            }

            EnhanceContrast(image->pixels, image->width, image->height, image->pixelSize);
            //SharpenPixels(image->pixels, image->width, image->height, image->pixelSize);
            //BlackOutlines(image->pixels, image->width, image->height, image->pixelSize);

            // Back to paletted+alpha?
            if(monochrome)
            {   // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                GL_ConvertToLuminance(image, true);
                AmplifyLuma(image->pixels, image->width, image->height, image->pixelSize == 2);
            }
            else
            {   // Yes. Quantize down from RGA(+A) to Paletted(+A), replacing the old image->
                newPixels = GL_ConvertBuffer(image->pixels, image->width, image->height,
                    (origMasked? 2 : 1), origPalette, 4);
                if(newPixels != image->pixels)
                {
                    free(image->pixels);
                    image->pixels = newPixels;
                    image->pixelSize = (origMasked? 2 : 1);
                    image->palette = origPalette;
                    if(origMasked)
                        image->flags |= IMGF_IS_MASKED;
                }
            }

            // Lets not do this again.
            noSmartFilter = true;
        }

        if(fillOutlines && 0 != image->palette && (image->flags & IMGF_IS_MASKED))
            ColorOutlinesIdx(image->pixels, image->width, image->height);
    }
    else if(image->pixelSize > 2)
    {
        if(monochrome)
        {
            GL_ConvertToLuminance(image, true);
            AmplifyLuma(image->pixels, image->width, image->height, image->pixelSize == 2);
        }
    }

    if(noCompression || (image->width < 128 || image->height < 128))
        flags |= TXCF_NO_COMPRESSION;

    if(spec->gammaCorrection)
        flags |= TXCF_APPLY_GAMMACORRECTION;

    if(spec->noStretch)
        flags |= TXCF_UPLOAD_ARG_NOSTRETCH;

    if(spec->mipmapped)
        flags |= TXCF_MIPMAP;

    if(noSmartFilter)
        flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

    if(monochrome)
    {
        dglFormat = ( image->pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
    }
    else
    {
        if(0 != image->palette)
        {   // Paletted.
            dglFormat = (image->flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8;
        }
        else
        {
            dglFormat = ( image->pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                          image->pixelSize == 3 ? DGL_RGB :
                          image->pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE );
        }
    }

    switch(spec->context)
    {
    case TC_MAPSURFACE_DIFFUSE:
    case TC_MAPSURFACE_REFLECTIONMASK:
    case TC_MAPSURFACE_LIGHTMAP:
        magFilter = glmode[texMagMode];
        break;
    case TC_SPRITE_DIFFUSE:
    case TC_MODELSKIN_DIFFUSE:
        magFilter = filterSprites ? GL_LINEAR : GL_NEAREST;
        break;
    default:
        magFilter = GL_LINEAR;
        break;
    }

    switch(spec->context)
    {
    case TC_MAPSURFACE_DIFFUSE:
    //case TC_MAPSURFACE_REFLECTION:
    case TC_MAPSURFACE_REFLECTIONMASK:
    //case TC_MAPSURFACE_LIGHTMAP:
        minFilter = glmode[mipmapping];
        break;
    case TC_UI:
        minFilter = GL_NEAREST;
        break;
    default:
        minFilter = GL_LINEAR;
        break;
    }

    anisoFilter = spec->anisoFilter < 0? texAniso : spec->anisoFilter;

    // Upload texture content.
    {
    texturecontent_t c;
    GL_InitTextureContent(&c);
    c.name = TextureVariant_GLName(tex);
    c.format = dglFormat;
    c.width = image->width;
    c.height = image->height;
    c.pixels = image->pixels;
    c.palette = image->palette;
    c.flags = flags;
    c.magFilter = magFilter;
    c.minFilter = minFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;
    c.grayMipmap = grayMipmap;
    didDefer = GL_NewTexture(&c);
    }

    TextureVariant_FlagUploaded(tex, true);
    TextureVariant_FlagMasked(tex, (image->flags & IMGF_IS_MASKED) != 0);

#ifdef _DEBUG
    VERBOSE( Con_Printf("Prepared TextureVariant \"%s\" (glName:%u)%s\n",
        Texture_Name(TextureVariant_GeneralCase(tex)), (unsigned int) TextureVariant_GLName(tex),
        !didDefer? " while not busy!" : "") );
    VERBOSE2( GL_PrintTextureVariantSpecification(TextureVariant_Spec(tex)) );
#endif

    /**
     * Calculate texture coordinates based on the image dimensions. The
     * coordinates are calculated as width/CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     *
     * \fixme Image dimensions may not be the same as the uploaded texture!
     */
    if((flags & TXCF_UPLOAD_ARG_NOSTRETCH) &&
       (!GL_state.features.texNonPowTwo || spec->mipmapped))
    {
        int pw = M_CeilPow2(image->width), ph = M_CeilPow2(image->height);
        s =  image->width / (float) pw;
        t = image->height / (float) ph;
    }
    else
    {
        s = t = 1;
    }

    TextureVariant_SetCoords(tex, s, t);

    if(TC_SKYSPHERE_DIFFUSE == spec->context)
    {
        averagecolor_analysis_t* avgTopColor;
        if(NULL == (avgTopColor = (averagecolor_analysis_t*)TextureVariant_Analysis(tex, TA_SKY_SPHEREFADECOLOR)))
        {
            if(NULL == (avgTopColor = (averagecolor_analysis_t*) malloc(sizeof(*avgTopColor))))
                Con_Error("Textures::prepareTextureVariant: Failed on allocation of %lu bytes for "
                          "new AverageColorAnalysis.", (unsigned long) sizeof(*avgTopColor));
            TextureVariant_AddAnalysis(tex, TA_SKY_SPHEREFADECOLOR, avgTopColor);
        }

        // Average color for glow planes and top line color.
        if(0 == image->palette)
        {
            FindAverageLineColor(image->pixels, image->width, image->height, image->pixelSize, 0, avgTopColor->color);
        }
        else
        {
            FindAverageLineColorIdx(image->pixels, image->width, image->height, 0, image->palette, false, avgTopColor->color);
        }
   }
   
    if(TC_SPRITE_DIFFUSE == spec->context)
    {
        pointlight_analysis_t* pl;
        if(NULL == (pl = (pointlight_analysis_t*)TextureVariant_Analysis(tex, TA_SPRITE_AUTOLIGHT)))
        {
            if(NULL == (pl = (pointlight_analysis_t*) malloc(sizeof(*pl))))
                Con_Error("Textures::prepareTextureVariant: Failed on allocation of %lu bytes for "
                          "new PointLightAnalysis.", (unsigned long) sizeof(*pl));
            TextureVariant_AddAnalysis(tex, TA_SPRITE_AUTOLIGHT, pl);
        }
        // Calculate light source properties.
        GL_CalcLuminance(image->pixels, image->width, image->height, image->pixelSize, image->palette,
                         &pl->originX, &pl->originY, pl->color, &pl->brightMul);
    }

    if(TC_MAPSURFACE_DIFFUSE == spec->context ||
       TC_SKYSPHERE_DIFFUSE  == spec->context)
    {
        ambientlight_analysis_t* al;
        if(NULL == (al = (ambientlight_analysis_t*)TextureVariant_Analysis(tex, TA_MAP_AMBIENTLIGHT)))
        {
            if(NULL == (al = (ambientlight_analysis_t*) malloc(sizeof(*al))))
                Con_Error("Textures::prepareTextureVariant: Failed on allocation of %lu bytes for "
                          "new AmbientLightAnalysis.", (unsigned long) sizeof(*al));
            TextureVariant_AddAnalysis(tex, TA_MAP_AMBIENTLIGHT, al);
        }

        if(0 == image->palette)
        {
            FindAverageColor(image->pixels, image->width, image->height, image->pixelSize, al->color);
        }
        else
        {
            FindAverageColorIdx(image->pixels, image->width, image->height, image->palette, false, al->color);
        }
        memcpy(al->colorAmplified, al->color, sizeof(al->colorAmplified));
        amplify(al->colorAmplified);
    }

    GL_DestroyImagePixels(image);
    }
}

static void prepareDetailVariant(texturevariant_t* tex, image_t* image)
{
    assert(tex && image);
    {
    const detailvariantspecification_t* spec = TS_DETAIL(TextureVariant_Spec(tex));
    texturecontent_t c;
    boolean didDefer;
    int flags = 0;

    if(image->pixelSize > 2)
    {
        GL_ConvertToLuminance(image, false);
    }

    { float baMul, hiMul, loMul;
    EqualizeLuma(image->pixels, image->width, image->height, &baMul, &hiMul, &loMul);
    if(verbose && (baMul != 1 || hiMul != 1 || loMul != 1))
    {
        Con_Message("Equalized TextureVariant \"%s\" (balance: %g, high amp: %g, low amp: %g).\n",
            Texture_Name(TextureVariant_GeneralCase(tex)), baMul, hiMul, loMul);
    }}

    // Disable compression?
    if(image->width < 128 || image->height < 128)
        flags |= TXCF_NO_COMPRESSION;

    // Calculate prepared texture coordinates.
    { int pw = M_CeilPow2(image->width), ph = M_CeilPow2(image->height);
    float s =  image->width / (float) pw;
    float t = image->height / (float) ph;
    TextureVariant_SetCoords(tex, s, t);
    }

    // Upload texture content.
    GL_InitTextureContent(&c);
    c.name = TextureVariant_GLName(tex);
    c.format = DGL_LUMINANCE;
    c.flags = flags | TXCF_GRAY_MIPMAP | TXCF_UPLOAD_ARG_NOSMARTFILTER;
    c.grayMipmap = spec->contrast;
    c.width = image->width;
    c.height = image->height;
    c.pixels = image->pixels;
    c.anisoFilter = texAniso;
    c.magFilter = glmode[texMagMode];
    c.minFilter = GL_LINEAR_MIPMAP_LINEAR;
    c.wrap[0] = GL_REPEAT;
    c.wrap[1] = GL_REPEAT;

    didDefer = GL_NewTexture(&c);
    TextureVariant_FlagUploaded(tex, true);

    // We're done with the image data.
    GL_DestroyImagePixels(image);

#ifdef _DEBUG
    VERBOSE( Con_Printf("Prepared TextureVariant \"%s\" (glName:%u)%s\n",
        Texture_Name(TextureVariant_GeneralCase(tex)), (unsigned int) TextureVariant_GLName(tex),
        !didDefer? " while not busy!" : "") );
    VERBOSE2( GL_PrintTextureVariantSpecification(TextureVariant_Spec(tex)) );
#endif
    }
}

static __inline texture_t* getTexture(textureid_t id)
{
    if(id > 0 && id <= texturesCount)
        return textures[id - 1];
    return NULL;
}

/**
 * This is a hash function. Given a texture name it generates a
 * somewhat-random number between 0 and TEXTURENAMESPACE_HASH_SIZE.
 *
 * @return              The generated hash index.
 */
static uint hashForTextureName(const char* name)
{
    ushort              key = 0;
    int                 i;

    // Stop when the name ends.
    for(i = 0; *name; ++i, name++)
    {
        if(i == 0)
            key ^= (int) (*name);
        else if(i == 1)
            key *= (int) (*name);
        else if(i == 2)
        {
            key -= (int) (*name);
            i = -1;
        }
    }

    return key % TEXTURENAMESPACE_HASH_SIZE;
}

/**
 * Given a name and texture type, search the gltextures db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name  Name of the texture to search for. Must have been
 *      transformed to all lower case.
 * @param type  Specific texture data.
 * @return  Ptr to the found texture_t else, @c NULL.
 */
static texture_t* getTextureByName(const char* name, uint hash,
    texturenamespaceid_t texNamespace)
{
    assert(VALID_TEXTURENAMESPACE(texNamespace));
    if(name && name[0])
    {
        const texturenamespace_hashnode_t* node;
        for(node = textureNamespaces[texNamespace-TEXTURENAMESPACE_FIRST].hashTable[hash]; node; node = node->next)
        {
            texture_t* tex = textures[node->textureIndex - 1];
            if(!strncmp(Texture_Name(tex), name, 8))
                return tex;
        }
    }
    return 0; // Not found.
}

static const texture_t* findTextureByName(const char* rawName,
    texturenamespaceid_t texNamespace)
{
    char name[9];
    uint hash;
    int n;

    if(!rawName || !rawName[0])
        return NULL;

    // Prepare 'name'.
    for(n = 0; *rawName && n < 8; ++n, rawName++)
        name[n] = tolower(*rawName);
    name[n] = '\0';
    hash = hashForTextureName(name);
    return getTextureByName(name, hash, texNamespace);
}

void GL_EarlyInitTextureManager(void)
{
    GL_InitSmartFilterHQ2x();
    calcGammaTable();

    { int i;
    for(i = 0; i < TEXTUREVARIANTSPECIFICATIONTYPE_COUNT; ++i)
        variantSpecs[i] = NULL;
    }
    textures = NULL;
    texturesCount = 0;

    { int i;
    for(i = 0; i < TEXTURENAMESPACE_COUNT; ++i)
        memset(textureNamespaces[i].hashTable, 0, sizeof(textureNamespaces[i].hashTable));
    }
}

void GL_InitTextureManager(void)
{
    if(novideo)
        return;

    if(texInited)
        return; // Don't init again.

    // Disable the use of 'high resolution' textures and/or patches?
    noHighResTex = ArgExists("-nohightex");
    noHighResPatches = ArgExists("-nohighpat");

    // Should we allow using external resources with PWAD textures?
    highResWithPWAD = ArgExists("-pwadtex");

    // System textures loaded in GL_LoadSystemTextures.
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));
    memset(lightingTextures, 0, sizeof(lightingTextures));

    // Initialization done.
    texInited = true;
}

void GL_ResetTextureManager(void)
{
    if(!texInited)
        return;
    GL_ClearTextureMemory();
}

int GL_CompareTextureVariantSpecifications(const texturevariantspecification_t* a,
    const texturevariantspecification_t* b)
{
    assert(a && b);
    if(a->type != b->type)
        return 1;
    switch(a->type)
    {
    case TST_GENERAL: return compareVariantSpecifications(TS_GENERAL(a), TS_GENERAL(b));
    case TST_DETAIL:  return compareDetailVariantSpecifications(TS_DETAIL(a), TS_DETAIL(b));
    }
    Con_Error("GL_CompareTextureVariantSpecifications: Invalid type %i.", (int) a->type);
    return 1; // Unreachable.
}

void GL_PrintTextureVariantSpecification(const texturevariantspecification_t* spec)
{
    static const char* textureUsageContextNames[1+TEXTUREVARIANTUSAGECONTEXT_COUNT] = {
        /* TC_UNKNOWN */                    { "unknown" },
        /* TC_UI */                         { "ui" },
        /* TC_MAPSURFACE_DIFFUSE */         { "mapsurface_diffuse" },
        /* TC_MAPSURFACE_REFLECTION */      { "mapsurface_reflection" },
        /* TC_MAPSURFACE_REFLECTIONMASK */  { "mapsurface_reflectionmask" },
        /* TC_MAPSURFACE_LIGHTMAP */        { "mapsurface_lightmap" },
        /* TC_SPRITE_DIFFUSE */             { "sprite_diffuse" },
        /* TC_MODELSKIN_DIFFUSE */          { "modelskin_diffuse" },
        /* TC_MODELSKIN_REFLECTION */       { "modelskin_reflection" },
        /* TC_HALO_LUMINANCE */             { "halo_luminance" },
        /* TC_PSPRITE_DIFFUSE */            { "psprite_diffuse" },
        /* TC_SKYSPHERE_DIFFUSE */          { "skysphere_diffuse" }
    };
    static const char* textureSpecificationTypeNames[TEXTUREVARIANTSPECIFICATIONTYPE_COUNT] = {
        /* TST_GENERAL */   { "general" },
        /* TST_DETAIL */    { "detail"  }
    };

    if(NULL == spec) return;

    Con_Printf("type:%s", textureSpecificationTypeNames[spec->type]);

    switch(spec->type)
    {
    case TST_DETAIL:
        Con_Printf(" contrast:%i%%\n", (int)(.5f + TS_DETAIL(spec)->contrast / 255.f * 100));
        break;
    case TST_GENERAL: {
        texturevariantusagecontext_t tc = TS_GENERAL(spec)->context;
        assert(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

        Con_Printf(" context:%s flags:%i border:%i",
            textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1],
            (TS_GENERAL(spec)->flags & ~TSF_INTERNAL_MASK), TS_GENERAL(spec)->border);
        if(TS_GENERAL(spec)->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            const colorpalettetranslationspecification_t* cpt = TS_GENERAL(spec)->translated;
            assert(cpt);
            Con_Printf(" translated(tclass:%i tmap:%i)", cpt->tClass, cpt->tMap);
        }
        Con_Printf("\n");
        break;
      }
    }
}

texturevariantspecification_t* GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int anisoFilter, boolean mipmapped, boolean gammaCorrection,
    boolean noStretch, boolean toAlpha)
{
    if(!texInited)
        Con_Error("GL_TextureVariantSpecificationForContext: Textures collection "
            "not yet initialized.");
    return getVariantSpecificationForContext(tc, flags, border, tClass, tMap, wrapS,
        wrapT, anisoFilter, mipmapped, gammaCorrection, noStretch, toAlpha);
}

texturevariantspecification_t* GL_DetailTextureVariantSpecificationForContext(
    float contrast)
{
    if(!texInited)
        Con_Error("GL_DetailTextureVariantSpecificationForContext: Textures collection "
            "not yet initialized.");
    return getDetailVariantSpecificationForContext(contrast);
}

void GL_DestroyTextures(void)
{
    if(!texInited) return;
    destroyTextures();
}

void GL_ShutdownTextureManager(void)
{
    if(!texInited)
        return; // Already been here?

    GL_ClearSystemTextures();
    destroyVariantSpecifications();
    destroyTextures();
    texInited = false;
}

static void calcGammaTable(void)
{
    double invGamma = 1.0f - MINMAX_OF(0, texGamma, 1); // Clamp to a sane range.
    int i;
    for(i = 0; i < 256; ++i)
        gammaTable[i] = (byte)(255.0f * pow(i / 255.0f, invGamma));
}

void GL_LoadSystemTextures(void)
{
    if(!texInited)
        return;

    UI_LoadTextures();

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);

    GL_PrepareSysFlareTexture(FXT_ROUND);
    GL_PrepareSysFlareTexture(FXT_FLARE);
    if(!haloRealistic)
    {
        GL_PrepareSysFlareTexture(FXT_BRFLARE);
        GL_PrepareSysFlareTexture(FXT_BIGFLARE);
    }

    Rend_ParticleLoadSystemTextures();
    R_InitSystemTextures();
}

void GL_ClearSystemTextures(void)
{
    if(!texInited)
        return;

    { int i;
    for(i = 0; i < NUM_LIGHTING_TEXTURES; ++i)
        glDeleteTextures(1, (const GLuint*) &lightingTextures[i].tex);
    }
    memset(lightingTextures, 0, sizeof(lightingTextures));

    { int i;
    for(i = 0; i < NUM_SYSFLARE_TEXTURES; ++i)
        glDeleteTextures(1, (const GLuint*) &sysFlareTextures[i].tex);
    }
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));

    Materials_DeleteGLTextures(MN_SYSTEM_NAME);
    UI_ClearTextures();

    Rend_ParticleClearSystemTextures();
    R_DestroySystemTextures();
}

void GL_ClearRuntimeTextures(void)
{
    if(!texInited)
        return;

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    // texture-wrapped GL textures; textures, flats, sprites...
    GL_ReleaseGLTexturesByNamespace(TN_FLATS);
    GL_ReleaseGLTexturesByNamespace(TN_TEXTURES);
    GL_ReleaseGLTexturesByNamespace(TN_PATCHES);
    GL_ReleaseGLTexturesByNamespace(TN_SPRITES);
    GL_ReleaseGLTexturesByNamespace(TN_DETAILS);
    GL_ReleaseGLTexturesByNamespace(TN_REFLECTIONS);
    GL_ReleaseGLTexturesByNamespace(TN_MASKS);
    GL_ReleaseGLTexturesByNamespace(TN_MODELSKINS);
    GL_ReleaseGLTexturesByNamespace(TN_MODELREFLECTIONSKINS);
    GL_ReleaseGLTexturesByNamespace(TN_LIGHTMAPS);
    GL_ReleaseGLTexturesByNamespace(TN_FLAREMAPS);
    GL_DeleteRawImages();

    Rend_ParticleClearExtraTextures();
}

void GL_ClearTextureMemory(void)
{
    if(!texInited)
        return;
    // Delete runtime textures (textures, flats, ...)
    GL_ClearRuntimeTextures();
}

void GL_InitImage(image_t* img)
{
    assert(img);
    img->width = 0;
    img->height = 0;
    img->pixelSize = 0;
    img->flags = 0;
    img->palette = 0;
    img->pixels = 0;
}

static boolean tryLoadPCX(image_t* img, DFILE* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = PCX_Load(file, &img->width, &img->height, &img->pixelSize);
    return (0 != img->pixels);
}

static boolean tryLoadPNG(image_t* img, DFILE* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = PNG_Load(file, &img->width, &img->height, &img->pixelSize);
    return (0 != img->pixels);
}

static boolean tryLoadTGA(image_t* img, DFILE* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = TGA_Load(file, &img->width, &img->height, &img->pixelSize);
    return (0 != img->pixels);
}

const imagehandler_t* findHandlerFromFileName(const char* filePath)
{
    const imagehandler_t* hdlr = 0; // No handler for this format.
    const char* p;
    if(0 != (p = M_FindFileExtension(filePath)))
    {
        int i = 0;
        do
        {
            if(!stricmp(p, handlers[i].ext))
                hdlr = &handlers[i];
        } while(0 == hdlr && 0 != handlers[++i].ext);
    }
    return hdlr;
}

/**
 * Returns true if the given path name refers to an image, which should
 * be color keyed.
 */
static boolean isColorKeyed(const char* path)
{
    filename_t buf;
    strncpy(buf, path, FILENAME_T_MAXLEN);
    strlwr(buf);
    return strstr(buf, "-ck.") != NULL;
}

/// \todo Remove the filePath argument by obtaining the path via the
/// File Stream Abstraction Layer. This method can then be made public.
static uint8_t* GL_LoadImageDFile(image_t* img, DFILE* file, const char* filePath)
{
    assert(img && file && filePath);
    {
    const imagehandler_t* hdlr;

    GL_InitImage(img);

    // Firstly try the expected format given the file name.
    if(0 != (hdlr = findHandlerFromFileName(filePath)))
    {
        hdlr->loadFunc(img, file);
    }

    // If not loaded. Try each recognisable format.
    /// \todo Order here should be determined by the resource locator.
    { int n = 0;
    while(0 == img->pixels && 0 != handlers[n].name)
    {
        if(&handlers[n] == hdlr) continue; // We already know its not in this format.
        handlers[n++].loadFunc(img, file);
    }}

    if(0 == img->pixels)
        return 0; // Not a recogniseable format.

    VERBOSE( Con_Message("GL_LoadImage: \"%s\" (%ix%i)\n", M_PrettyPath(filePath), img->width, img->height) );

    // How about some color-keying?
    if(isColorKeyed(filePath))
    {
        uint8_t* out = ApplyColorKeying(img->pixels, img->width, img->height, img->pixelSize);
        if(out != img->pixels)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            free(img->pixels);
            img->pixels = out;
        }
        // Color keying is done; now we have 4 bytes per pixel.
        img->pixelSize = 4;
    }

    // Any alpha pixels?
    if(GL_ImageHasAlpha(img))
        img->flags |= IMGF_IS_MASKED;

    return img->pixels;
    }
}

uint8_t* GL_LoadImage(image_t* img, const char* filePath)
{
    assert(img);
    {
    byte* result = 0;
    DFILE* file;
    if(NULL != (file = F_Open(filePath, "rb")))
    {
        result = GL_LoadImageDFile(img, file, filePath);
        F_Close(file);
    }
    return result;
    }
}

uint8_t* GL_LoadImageStr(image_t* img, const ddstring_t* filePath)
{
    if(filePath)
        return GL_LoadImage(img, Str_Text(filePath));
    return 0;
}

void GL_DestroyImagePixels(image_t* img)
{
    assert(img);
    if(NULL == img->pixels) return;
    free(img->pixels); img->pixels = NULL;
}

boolean GL_PalettizeImage(uint8_t* out, int outformat, int paletteIdx,
    boolean applyTexGamma, const uint8_t* in, int informat, int width, int height)
{
    assert(in && out);

    if(0 >= width || 0 >= height)
        return false;

    if(informat <= 2 && outformat >= 3)
    {
        long numPels = width * height;
        int inSize = (informat == 2 ? 1 : informat);
        int outSize = (outformat == 2 ? 1 : outformat);
        colorpalette_t* pal = R_ToColorPalette(paletteIdx);

        if(NULL == pal)
            Con_Error("GL_PalettizeImage: Failed to locate ColorPalette for index %i.", paletteIdx);

        { long i;
        for(i = 0; i < numPels; ++i)
        {
            ColorPalette_Color(pal, *in, out);
            if(applyTexGamma)
            {
                out[CR] = gammaTable[out[CR]];
                out[CG] = gammaTable[out[CG]];
                out[CB] = gammaTable[out[CB]];
            }

            if(outformat == 4)
            {
                if(informat == 2)
                    out[CA] = in[numPels * inSize];
                else
                    out[CA] = 0;
            }

            in  += inSize;
            out += outSize;
        }}
        return true;
    }
    return false;
}

boolean GL_QuantizeImageToPalette(uint8_t* out, int outformat, int paletteIdx,
    const uint8_t* in, int informat, int width, int height)
{
    assert(out && in);
    if(informat >= 3 && outformat <= 2 && width > 0 && height > 0)
    {
        int inSize = (informat == 2 ? 1 : informat);
        int outSize = (outformat == 2 ? 1 : outformat);
        int i, numPixels = width * height;
        colorpalette_t* pal = R_ToColorPalette(paletteIdx);

        if(NULL == pal)
            Con_Error("GL_QuantizeImageToPalette: Failed to locate ColorPalette for index %i.", paletteIdx);

        for(i = 0; i < numPixels; ++i, in += inSize, out += outSize)
        {
            // Convert the color value.
            *out = ColorPalette_NearestIndexv(pal, in);

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

void GL_DeSaturatePalettedImage(uint8_t* buffer, int paletteIdx, int width, int height)
{
    assert(buffer);
    {
    const long numPels = width * height;
    colorpalette_t* pal;
    uint8_t rgb[3];
    int max, temp;

    if(width == 0 || height == 0)
        return; // Nothing to do.

    pal = R_ToColorPalette(paletteIdx);
    if(NULL == pal)
        Con_Error("GL_DeSaturatePalettedImage: Failed to locate ColorPalette for index %i.", paletteIdx);

    // What is the maximum color value?
    max = 0;
    { long i;
    for(i = 0; i < numPels; ++i)
    {
        ColorPalette_Color(pal, buffer[i], rgb);
        if(rgb[CR] == rgb[CG] && rgb[CR] == rgb[CB])
        {
            if(rgb[CR] > max)
                max = rgb[CR];
            continue;
        }

        temp = (2 * (int)rgb[CR] + 4 * (int)rgb[CG] + 3 * (int)rgb[CB]) / 9;
        if(temp > max)
            max = temp;
    }}

    { long i;
    for(i = 0; i < numPels; ++i)
    {
        ColorPalette_Color(pal, buffer[i], rgb);
        if(rgb[CR] == rgb[CG] && rgb[CR] == rgb[CB])
            continue;

        // Calculate a weighted average.
        temp = (2 * (int)rgb[CR] + 4 * (int)rgb[CG] + 3 * (int)rgb[CB]) / 9;
        if(max)
            temp *= 255.f / max;
        buffer[i] = ColorPalette_NearestIndex(pal, temp, temp, temp);
    }}
    }
}

static int BytesPerPixelFmt(dgltexformat_t format)
{
    switch(format)
    {
    case DGL_LUMINANCE:
    case DGL_COLOR_INDEX_8:         return 1;

    case DGL_LUMINANCE_PLUS_A8:
    case DGL_COLOR_INDEX_8_PLUS_A8: return 2;

    case DGL_RGB:                   return 3;

    case DGL_RGBA:                  return 4;
    default:
        Con_Error("BytesPerPixelFmt: Unknown format %i, don't know pixel size.\n", format);
        return 0; // Unreachable.
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
            return GL_RGB8;
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

boolean GL_TexImageGrayMipmap(int glFormat, int loadFormat, const uint8_t* pixels,
    int width, int height, float grayFactor)
{
    assert(pixels);
    {
    int w, h, numpels = width * height, numLevels, pixelSize;
    uint8_t* image, *faded, *out;
    const uint8_t* in;
    float invFactor;

    if(!(GL_RGB == loadFormat || GL_LUMINANCE == loadFormat))
        Con_Error("GL_TexImageGrayMipmap: Unsupported load format %i.", (int) loadFormat);

    pixelSize = (loadFormat == GL_LUMINANCE? 1 : 3);

    // Can't operate on null texture.
    if(width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if(!GL_state.features.texNonPowTwo &&
       (width != M_CeilPow2(width) || height != M_CeilPow2(height)))
        return false;

    if(width > GL_state.maxTexSize || height > GL_state.maxTexSize)
        return false;

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
    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, (GLint)loadFormat,
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

        glTexImage2D(GL_TEXTURE_2D, i + 1, glFormat, w, h, 0, (GLint)loadFormat,
            GL_UNSIGNED_BYTE, faded);
    }}

    // Do we need to free the temp buffer?
    free(faded);
    free(image);

    assert(!Sys_GLCheckError());
    return true;
    }
}

boolean GL_TexImage(int glFormat, int loadFormat, const uint8_t* pixels,
    int width,  int height, int genMipmaps)
{
    assert(pixels);
    {
    const int packRowLength = 0, packAlignment = 1, packSkipRows = 0, packSkipPixels = 0;
    const int unpackRowLength = 0, unpackAlignment = 1, unpackSkipRows = 0, unpackSkipPixels = 0;
    int mipLevel = 0;

    if(!(GL_LUMINANCE_ALPHA == loadFormat || GL_LUMINANCE == loadFormat ||
         GL_RGB == loadFormat || GL_RGBA == loadFormat))
         Con_Error("GL_TexImage: Unsupported load format %i.", (int) loadFormat);

    // Can't operate on null texture.
    if(width < 1 || height < 1)
        return false;

    // Check that the texture dimensions are valid.
    if(width > GL_state.maxTexSize || height > GL_state.maxTexSize)
        return false;

    if(!GL_state.features.texNonPowTwo &&
       (width != M_CeilPow2(width) || height != M_CeilPow2(height)))
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

    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)packRowLength);
    glPixelStorei(GL_PACK_ALIGNMENT, (GLint)packAlignment);
    glPixelStorei(GL_PACK_SKIP_ROWS, (GLint)packSkipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, (GLint)packSkipPixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)unpackRowLength);
    glPixelStorei(GL_UNPACK_ALIGNMENT, (GLint)unpackAlignment);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, (GLint)unpackSkipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, (GLint)unpackSkipPixels);

    if(genMipmaps && !GL_state.extensions.genMipmapSGIS)
    {   // Build all mipmap levels.
        int neww, newh, bpp, w, h;
        void* image, *newimage;

        bpp = BytesPerPixel(loadFormat);
        if(bpp == 0)
            Con_Error("GL_TexImage: Unknown GL format %i.\n", (int) loadFormat);

        GL_OptimalTextureSize(width, height, false, true, &w, &h);

        if(w != width || h != height)
        {
            // Must rescale image to get "top" mipmap texture image.
            image = GL_ScaleBufferEx(pixels, width, height, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                w, h, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment, packSkipRows,
                packSkipPixels);
            if(NULL == image)
                Con_Error("GL_TexImage: Unknown error resizing mipmap level #0.");
        }
        else
        {
            image = (void*) pixels;
        }

        for(;;)
        {
            glTexImage2D(GL_TEXTURE_2D, mipLevel, (GLint)glFormat, w, h, 0, (GLint)loadFormat,
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
                Con_Error("GL_TexImage: Unknown error resizing mipmap level #%i.", mipLevel);

            if(image != pixels)
                free(image);
            image = newimage;

            w = neww;
            h = newh;
        }

        if(image != pixels)
            free(image);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, mipLevel, (GLint)glFormat, (GLsizei)width,
            (GLsizei)height, 0, (GLint)loadFormat, GL_UNSIGNED_BYTE, pixels);
    }

    glPopClientAttrib();
    assert(!Sys_GLCheckError());

    return true;
    }
}

DGLuint GL_UploadTextureWithParams(const uint8_t* pixels, int width, int height,
    dgltexformat_t format, boolean flagGenerateMipmaps, boolean flagNoStretch,
    boolean flagNoSmartFilter, int minFilter, int magFilter, int anisoFilter,
    int wrapS, int wrapT, int otherFlags)
{
    texturecontent_t content;

    GL_InitTextureContent(&content);
    content.pixels = pixels;
    content.format = format;
    content.width = width;
    content.height = height;
    content.flags = otherFlags;
    if(flagGenerateMipmaps) content.flags |= TXCF_MIPMAP;
    if(flagNoStretch) content.flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
    if(flagNoSmartFilter) content.flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;
    content.minFilter = minFilter;
    content.magFilter = magFilter;
    content.anisoFilter = anisoFilter;
    content.wrap[0] = wrapS;
    content.wrap[1] = wrapT;
    content.name = GL_GetReservedTextureName();
    GL_NewTexture(&content);
    return content.name;
}

DGLuint GL_UploadTextureContent(const texturecontent_t* content)
{
    assert(content);
    {
    boolean generateMipmaps  = ((content->flags & (TXCF_MIPMAP|TXCF_GRAY_MIPMAP)) != 0);
    boolean allowCompression = ((content->flags & TXCF_NO_COMPRESSION) == 0);
    boolean applyTexGamma    = ((content->flags & TXCF_APPLY_GAMMACORRECTION) != 0);
    boolean noSmartFilter    = ((content->flags & TXCF_UPLOAD_ARG_NOSMARTFILTER) != 0);
    boolean noStretch        = ((content->flags & TXCF_UPLOAD_ARG_NOSTRETCH) != 0);
    int loadWidth = content->width, loadHeight = content->height;
    const uint8_t* loadPixels = content->pixels;
    dgltexformat_t dglFormat = content->format;

    if(DGL_COLOR_INDEX_8 == dglFormat || DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat)
    {
        // Convert a paletted source image to truecolor.
        uint8_t* newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight,
            DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 2 : 1, R_FindColorPaletteIndexForId(content->palette),
            DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 4 : 3);
        if(loadPixels != content->pixels)
            free((uint8_t*)loadPixels);
        loadPixels = newPixels;
        dglFormat = DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? DGL_RGBA : DGL_RGB;
    }

    if(DGL_RGBA == dglFormat || DGL_RGB == dglFormat)
    {
        int comps = (DGL_RGBA == dglFormat ? 4 : 3);

        if(applyTexGamma && texGamma > .0001f)
        {
            uint8_t* dst, *localBuffer = NULL;
            long numPels = loadWidth * loadHeight;
            const uint8_t* src;

            src = loadPixels;
            if(loadPixels == content->pixels)
            {
                if(0 == (localBuffer = (uint8_t*) malloc(comps * numPels)))
                    Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for"
                              "tex-gamma translation buffer.", (unsigned long) (comps * numPels));
                dst = localBuffer;
            }
            else
            {
                dst = (uint8_t*)loadPixels;
            }

            { long i;
            for(i = 0; i < numPels; ++i)
            {
                dst[CR] = gammaTable[src[CR]];
                dst[CG] = gammaTable[src[CG]];
                dst[CB] = gammaTable[src[CB]];
                src += comps;
                dst += comps;
            }}

            if(NULL != localBuffer)
            {
                if(loadPixels != content->pixels)
                    free((uint8_t*)loadPixels);
                loadPixels = localBuffer;
            }
        }

        if(useSmartFilter && !noSmartFilter)
        {
            int smartFlags = ICF_UPSCALE_SAMPLE_WRAP;
            uint8_t* filtered;

            if(comps == 3)
            {   // Need to add an alpha channel.
                uint8_t* newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight, 3, 0, 4);
                if(loadPixels != content->pixels)
                    free((uint8_t*)loadPixels);
                loadPixels = newPixels;
                dglFormat = DGL_RGBA;
            }

            filtered = GL_SmartFilter(GL_ChooseSmartFilter(loadWidth, loadHeight, 0), loadPixels,
                loadWidth, loadHeight, smartFlags, &loadWidth, &loadHeight);
            if(filtered != loadPixels)
            {
                if(loadPixels != content->pixels)
                    free((uint8_t*)loadPixels);
                loadPixels = filtered;
            }
        }
    }

    if(DGL_LUMINANCE_PLUS_A8 == dglFormat)
    {   // Needs converting. This adds some overhead.
        long numPixels = content->width * content->height;
        uint8_t* pixel, *localBuffer;

        if(NULL == (localBuffer = (uint8_t*) malloc(2 * numPixels)))
            Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for "
                "luminance conversion buffer.", (unsigned long) (2 * numPixels));
        
        pixel = localBuffer;
        { long i;
        for(i = 0; i < numPixels; ++i)
        {
            pixel[0] = loadPixels[i];
            pixel[1] = loadPixels[numPixels + i];
            pixel += 2;
        }}

        if(loadPixels != content->pixels)
            free((uint8_t*)loadPixels);
        loadPixels = localBuffer;
    }

    if(DGL_LUMINANCE == dglFormat && (content->flags & TXCF_CONVERT_8BIT_TO_ALPHA))
    {   // Needs converting. This adds some overhead.
        long numPixels = content->width * content->height;
        uint8_t* pixel, *localBuffer;

        if(NULL == (localBuffer = (uint8_t*) malloc(2 * numPixels)))
            Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for "
                "luminance conversion buffer.", (unsigned long) (2 * numPixels));

        // Move the average color to the alpha channel, make the actual color white.
        pixel = localBuffer;
        { long i;
        for(i = 0; i < numPixels; ++i)
        {
            pixel[0] = 255;
            pixel[1] = loadPixels[i];
            pixel += 2;
        }}

        if(loadPixels != content->pixels)
            free((uint8_t*)loadPixels);
        loadPixels = localBuffer;
        dglFormat = DGL_LUMINANCE_PLUS_A8;
    }

    // Calculate the final dimensions for the texture, as required by
    // the graphics hardware and/or engine configuration.
    {
    int width = loadWidth, height = loadHeight;
    noStretch = GL_OptimalTextureSize(width, height, noStretch, generateMipmaps,
        &loadWidth, &loadHeight);

    // Do we need to resize?
    if(width != loadWidth || height != loadHeight)
    {
        int comps = BytesPerPixelFmt(dglFormat);

        if(noStretch)
        {   // Copy the texture into a power-of-two canvas.
            uint8_t* localBuffer;
            if(NULL == (localBuffer = (uint8_t*) calloc(1, comps * loadWidth * loadHeight)))
                Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for"
                          "upscale buffer.", (unsigned long) (comps * loadWidth * loadHeight));

            // Copy line by line.
            { int i;
            for(i = 0; i < height; ++i)
            {
                memcpy(localBuffer + loadWidth * comps * i,
                       loadPixels  + width     * comps * i, comps * width);
            }}
            if(loadPixels != content->pixels)
                free((uint8_t*)loadPixels);
            loadPixels = localBuffer;
        }
        else
        {   // Stretch into a new power-of-two texture.
            uint8_t* newPixels = GL_ScaleBuffer(loadPixels, width, height,
                comps, loadWidth, loadHeight);
            if(loadPixels != content->pixels)
                free((uint8_t*)loadPixels);
            loadPixels = newPixels;
        }
    }
    }

    glBindTexture(GL_TEXTURE_2D, content->name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content->minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content->magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, content->wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, content->wrap[1]);
    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        GL_GetTexAnisoMul(content->anisoFilter));

    if(!(content->flags & TXCF_GRAY_MIPMAP))
    {
        GLint glFormat, loadFormat;

        switch(dglFormat)
        {
        case DGL_LUMINANCE_PLUS_A8: loadFormat = GL_LUMINANCE_ALPHA; break;
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB; break;
        case DGL_RGBA:              loadFormat = GL_RGBA; break;
        default:
            Con_Error("GL_UploadTextureContent: Unknown format %i.", (int) dglFormat);
        }

        glFormat = ChooseTextureFormat(dglFormat, allowCompression);

        if(!GL_TexImage(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                generateMipmaps ? true : false))
        {
            Con_Error("GL_UploadTextureContent: TexImage failed (%u:%ix%i fmt%i).",
                content->name, loadWidth, loadHeight, (int) dglFormat);
        }
    }
    else
    {   // Special fade-to-gray luminance texture (used for details).
        GLint glFormat, loadFormat;

        switch(dglFormat)
        {
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB; break;
        default:
            Con_Error("GL_UploadTextureContent: Unknown format %i.", (int) dglFormat);
        }

        glFormat = ChooseTextureFormat(DGL_LUMINANCE, allowCompression);

        if(!GL_TexImageGrayMipmap(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                content->grayMipmap * reciprocal255))
        {
            Con_Error("GL_UploadTextureContent: TexImageGrayMipmap failed (%u:%ix%i fmt%i).",
                content->name, loadWidth, loadHeight, (int) dglFormat);
        }
    }

    if(loadPixels != content->pixels)
        free((uint8_t*)loadPixels);

    return content->name;
    }
}

byte GL_LoadExtTextureEX(image_t* image, const char* searchPath, const char* optionalSuffix,
    boolean silent)
{
    assert(image && searchPath);
    {
    ddstring_t foundPath;
    Str_Init(&foundPath);
    if(0 == F_FindResource3(RC_GRAPHIC, searchPath, &foundPath, optionalSuffix))
    {
        Str_Free(&foundPath);
        if(!silent)
            Con_Message("GL_LoadExtTextureEX: Warning, failed to locate \"%s\"\n", searchPath);
        return 0;
    }
    if(GL_LoadImage(image, Str_Text(&foundPath)))
    {
        Str_Free(&foundPath);
        return 2;
    }
    Str_Free(&foundPath);
    if(!silent)
        Con_Message("GL_LoadExtTextureEX: Warning, failed to load \"%s\"\n", M_PrettyPath(searchPath));
    return 0;
    }
}

DGLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    struct lstex_s {
        const char*     name;
        int             wrapS, wrapT;
    } lstexes[NUM_LIGHTING_TEXTURES] = {
        { "dLight",     GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "wallglow",   GL_REPEAT,          GL_CLAMP_TO_EDGE },
        { "radioCO",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "radioCC",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "radioOO",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        { "radioOE",    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE }
    };

    if(which < 0 || which >= NUM_LIGHTING_TEXTURES)
        return 0;

    if(!lightingTextures[which].tex)
    {
        lightingTextures[which].tex = GL_PrepareExtTexture(lstexes[which].name, LGM_WHITE_ALPHA,
            false, GL_LINEAR, GL_LINEAR, -1 /*best anisotropy*/, lstexes[which].wrapS,
            lstexes[which].wrapT, TXCF_NO_COMPRESSION);
    }
    return lightingTextures[which].tex;
}

DGLuint GL_PrepareSysFlareTexture(flaretexid_t flare)
{
    if(flare < 0 || flare >= NUM_SYSFLARE_TEXTURES)
        return 0;

    if(!sysFlareTextures[flare].tex)
    {
        // We don't want to compress the flares (banding would be noticeable).
        sysFlareTextures[flare].tex = GL_PrepareExtTexture(flare == 0 ? "dlight" : flare == 1 ? "flare" : flare == 2 ? "brflare" : "bigflare", LGM_WHITE_ALPHA,
            false, GL_NEAREST, GL_LINEAR, 0 /*no anisotropy*/, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            TXCF_NO_COMPRESSION);
    }
    if(sysFlareTextures[flare].tex == 0)
        Con_Error("GL_PrepareSysFlareTexture: Error, flare texture %i not found!\n", flare);
    return sysFlareTextures[flare].tex;
}

byte GL_LoadExtTexture(image_t* image, const char* name, gfxmode_t mode)
{
    ddstring_t foundPath;
    byte result = 0;

    Str_Init(&foundPath);
    if(F_FindResource2(RC_GRAPHIC, name, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        // Force it to grayscale?
        if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
        {
            GL_ConvertToAlpha(image, mode == LGM_WHITE_ALPHA);
        }
        else if(mode == LGM_GRAYSCALE)
        {
            GL_ConvertToLuminance(image, true);
        }
        result = 2; // External.
    }
    Str_Free(&foundPath);
    return result;
}

// Posts are runs of non masked source pixels.
typedef struct {
    byte topdelta; // @c -1 is the last post in a column.
    byte length;
    // Length data bytes follows.
} post_t;

// column_t is a list of 0 or more post_t, (uint8_t)-1 terminated
typedef post_t column_t;

/**
 * \important: The buffer must have room for the new alpha data!
 *
 * @param dstBuf  The destination buffer the patch will be drawn to.
 * @param texwidth  Width of the dst buffer in pixels.
 * @param texheight  Height of the dst buffer in pixels.
 * @param patch  Ptr to the patch structure to draw to the dst buffer.
 * @param origx  X coordinate in the dst buffer to draw the patch too.
 * @param origy  Y coordinate in the dst buffer to draw the patch too.
 * @param tclass  Translation class to use.
 * @param tmap  Translation map to use.
 * @param maskZero  Used with sky textures.
 */
static void loadDoomPatch(uint8_t* buffer, int texwidth, int texheight,
    const doompatch_header_t* patch, int origx, int origy, int tclass,
    int tmap, boolean maskZero)
{
    assert(buffer && texwidth > 0 && texheight > 0 && patch);
    {
    int count, col = 0, trans = -1;
    int x = origx, y, top; // Keep track of pos (clipping).
    int w = SHORT(patch->width);
    size_t bufsize = texwidth * texheight;
    const column_t* column;
    const uint8_t* source;
    uint8_t* dest1, *dest2, *destTop, *destAlphaTop;
    // Column offsets begin immediately following the header.
    const int32_t* columnOfs = (const int32_t*)((const uint8_t*) patch + sizeof(doompatch_header_t));
    // \todo Validate column offset is within the Patch!

    if(tmap || tclass)
    {   // We need to translate the patch.
        trans = MAX_OF(0, -256 + tclass * ((8-1) * 256) + tmap * 256);
    }

    destTop = buffer + origx;
    destAlphaTop = buffer + origx + bufsize;

    for(; col < w; ++col, destTop++, destAlphaTop++, ++x)
    {
        column = (const column_t*) ((const uint8_t*) patch + LONG(columnOfs[col]));
        top = -1;

        // Step through the posts in a column
        while(column->topdelta != 0xff)
        {
            source = (uint8_t*) column + 3;

            if(x < 0 || x >= texwidth)
                break; // Out of bounds.

            if(column->topdelta <= top)
                top += column->topdelta;
            else
                top = column->topdelta;

            if((count = column->length) > 0)
            {
                y = origy + top;
                dest1 = destTop + y * texwidth;
                dest2 = destAlphaTop + y * texwidth;

                while(count--)
                {
                    uint8_t palidx = *source++;

                    if(trans >= 0)
                    {
                        /// \fixme Check bounds!
                        palidx = *(translationTables + trans + palidx);
                    }

                    // Is the destination within bounds?
                    if(y >= 0 && y < texheight)
                    {
                        if(!maskZero || palidx)
                            *dest1 = palidx;

                        if(maskZero)
                            *dest2 = (palidx ? 0xff : 0);
                        else
                            *dest2 = 0xff;
                    }

                    // One row down.
                    dest1 += texwidth;
                    dest2 += texwidth;
                    ++y;
                }
            }

            column = (const column_t*) ((const uint8_t*) column + column->length + 4);
        }
    }
    }
}

static boolean palettedIsMasked(const uint8_t* pixels, int width, int height)
{
    assert(pixels);
    // Jump to the start of the alpha data.
    pixels += width * height;
    { int i;
    for(i = 0; i < width * height; ++i)
        if(255 != pixels[i])
            return true;
    }
    return false;
}

byte GL_LoadDetailTextureLump(image_t* image, lumpnum_t lumpNum)
{
    assert(image);
    {
    byte result = 0;
    DFILE* file;
    if(NULL != (file = F_OpenLump(lumpNum, false)))
    {
        if(0 != GL_LoadImageDFile(image, file, W_LumpName(lumpNum)))
        {
            result = 1;
        }
        else
        {   // It must be an old-fashioned "raw" image.
            size_t bufSize, fileLength = F_Length(file);

            GL_InitImage(image);

            /**
             * \fixme Do not fatal error here if the not a known format!
             * Perform this check much earlier, when the definitions are
             * read and mark which are valid.
             */

            // How big is it?
            switch(fileLength)
            {
            case 256 * 256: image->width = image->height = 256; break;
            case 128 * 128: image->width = image->height = 128; break;
            case  64 *  64: image->width = image->height =  64; break;
            default:
                Con_Error("GL_LoadDetailTextureLump: Must be 256x256, 128x128 or 64x64.\n");
                return 0; // Unreachable.
            }

            image->pixelSize = 1;
            bufSize = (size_t)image->width * image->height;
            image->pixels = (uint8_t*) malloc(bufSize);
            if(NULL == image->pixels)
                Con_Error("GL_LoadDetailTextureLump: Failed on allocation of %lu bytes for "
                    "image pixel buffer.", (unsigned long) bufSize);
            if(fileLength < bufSize)
                memset(image->pixels, 0, bufSize);

            // Load the raw image data.
            F_Read(image->pixels, fileLength, file);
            result = 1;
        }
        F_Close(file);
    }
    return result;
    }
}

byte GL_LoadFlatLump(image_t* image, lumpnum_t lumpNum)
{
    assert(image);
    {
    byte result = 0;
    DFILE* file;
    if(NULL != (file = F_OpenLump(lumpNum, false)))
    {
        if(0 != GL_LoadImageDFile(image, file, W_LumpName(lumpNum)))
        {
            result = 1;
        }
        else
        {   // A DOOM flat.
#define FLAT_WIDTH          64
#define FLAT_HEIGHT         64

            size_t bufSize, fileLength = F_Length(file);

            GL_InitImage(image);

            /// \fixme not all flats are 64x64!
            image->width  = FLAT_WIDTH;
            image->height = FLAT_HEIGHT;
            image->pixelSize = 1;
            image->palette = R_FindColorPaletteIndexForId(0);

            bufSize = MAX_OF(fileLength, (size_t)image->width * image->height);
            image->pixels = (uint8_t*) malloc(bufSize);
            if(NULL == image->pixels)
                Con_Error("GL_LoadFlatLump: Failed on allocation of %lu bytes for "
                    "image pixel buffer.", (unsigned long) bufSize);
            if(fileLength < bufSize)
                memset(image->pixels, 0, bufSize);

            // Load the raw image data.
            F_Read(image->pixels, fileLength, file);
            result = 1;

#undef FLAT_HEIGHT
#undef FLAT_WIDTH
        }
        F_Close(file);
    }
    return result;
    }
}

byte GL_LoadPatchLump(image_t* image, lumpnum_t lumpNum, int tclass, int tmap, int border)
{
    assert(image);
    {
    byte result = 0;
    DFILE* file;
    if(NULL != (file = F_OpenLump(lumpNum, false)))
    {
        if(0 != GL_LoadImageDFile(image, file, W_LumpName(lumpNum)))
        {
            result = 1;
        }
        else
        {   // A DOOM patch.
            const doompatch_header_t* patch;
            size_t fileLength = F_Length(file);
            uint8_t* buf = (uint8_t*) malloc(fileLength);

            if(NULL == buf)
                Con_Error("GL_LoadPatchLump: Failed on allocation of %lu bytes for "
                    "temporary lump buffer.", (unsigned long) (fileLength));
            F_Read(buf, fileLength, file);
            patch = (const doompatch_header_t*)buf;

            GL_InitImage(image);

            image->width  = SHORT(patch->width)  + border*2;
            image->height = SHORT(patch->height) + border*2;
            image->pixelSize = 1;
            image->palette = R_FindColorPaletteIndexForId(0);
            image->pixels = (uint8_t*) calloc(1, 2 * image->width * image->height);
            if(NULL == image->pixels)
                Con_Error("GL_LoadPatchLump: Failed on allocation of %lu bytes for "
                    "image pixel buffer.", (unsigned long) (2 * image->width * image->height));

            loadDoomPatch(image->pixels, image->width, image->height, patch,
                border, border, tclass, tmap, false);
            if(palettedIsMasked(image->pixels, image->width, image->height))
                image->flags |= IMGF_IS_MASKED;

            free(buf);
            result = 1;
        }
        F_Close(file);
    }
    return result;
    }
}

DGLuint GL_PrepareExtTexture(const char* name, gfxmode_t mode, int useMipmap,
    int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT, int otherFlags)
{
    image_t image;
    DGLuint texture = 0;

    if(GL_LoadExtTexture(&image, name, mode))
    {   // Loaded successfully and converted accordingly.
        // Upload the image to GL.
        texture = GL_NewTextureWithParams2(
            ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
              image.pixelSize == 3 ? DGL_RGB :
              image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
            image.width, image.height, image.pixels,
            ( otherFlags | (useMipmap? TXCF_MIPMAP : 0) |
              (useMipmap == DDMAXINT? TXCF_GRAY_MIPMAP : 0) |
              (image.width < 128 && image.height < 128? TXCF_NO_COMPRESSION : 0) ),
            0, (useMipmap ? glmode[mipmapping] : GL_LINEAR),
            magFilter, texAniso, wrapS, wrapT);

        GL_DestroyImagePixels(&image);
    }

    return texture;
}

byte GL_LoadPatchComposite(image_t* image, const texture_t* tex)
{
    assert(image && tex);
    {
    patchcompositetex_t* texDef = R_PatchCompositeTextureByIndex(Texture_TypeIndex(tex));
    assert(NULL != texDef);

    GL_InitImage(image);
    image->pixelSize = 1;
    image->width = texDef->width;
    image->height = texDef->height;
    image->palette = R_FindColorPaletteIndexForId(0);
    if(NULL == (image->pixels = (uint8_t*) calloc(1, 2 * image->width * image->height)))
        Con_Error("GL_LoadPatchComposite: Failed on allocation of %lu bytes for ",
            "new image pixel data.", (unsigned long) (2 * image->width * image->height));

    { int i;
    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t* patchDef = &texDef->patches[i];
        const doompatch_header_t* patch = (doompatch_header_t*)
            W_CacheLumpNum(patchDef->lump, PU_CACHE);

        // Draw the patch in the buffer.
        loadDoomPatch(image->pixels, image->width, image->height,
            patch, patchDef->offX, patchDef->offY, 0, 0, false);
    }}

    if(palettedIsMasked(image->pixels, image->width, image->height))
        image->flags |= IMGF_IS_MASKED;

    return 1;
    }
}

byte GL_LoadPatchCompositeAsSky(image_t* image, const texture_t* tex,
    boolean zeroMask)
{
    assert(image && tex);
    {
    patchcompositetex_t* texDef = R_PatchCompositeTextureByIndex(Texture_TypeIndex(tex));
    int width, height, offX, offY;

    assert(NULL != texDef);

    /**
     * Heretic sky textures are reported to be 128 tall, despite the patch
     * data is 200. We'll adjust the real height of the texture up to
     * 200 pixels (remember Caldera?).
     */
    width = texDef->width;
    height = texDef->height;
    if(texDef->patchCount == 1)
    {
        const doompatch_header_t* patch = (const doompatch_header_t*)
            W_CacheLumpNum(texDef->patches[0].lump, PU_CACHE);
        int bufHeight = SHORT(patch->height) > height ? SHORT(patch->height) : height;
        if(bufHeight > height)
        {
            height = bufHeight;
            if(height > 200)
                height = 200;
        }
    }

    GL_InitImage(image);
    image->pixelSize = 1;
    image->width = width;
    image->height = height;
    image->palette = R_FindColorPaletteIndexForId(0);
    if(NULL == (image->pixels = (uint8_t*) calloc(1, 2 * image->width * image->height)))
        Con_Error("GL_LoadPatchCompositeAsSky: Failed on allocation of %lu bytes for ",
            "new image pixel data.", (unsigned long) (2 * image->width * image->height));

    { int i;
    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t* patchDef = &texDef->patches[i];
        const doompatch_header_t* patch = (const doompatch_header_t*)
            W_CacheLumpNum(patchDef->lump, PU_CACHE);

        if(texDef->patchCount != 1)
        {
            offX = patchDef->offX;
            offY = patchDef->offY;
        }
        else
        {
            offX = offY = 0;
        }

        loadDoomPatch(image->pixels, image->width, image->height, patch,
            offX, offY, 0, 0, zeroMask);
    }}

    if(zeroMask)
        image->flags |= IMGF_IS_MASKED;

    return 1;
    }
}

byte GL_LoadRawTex(image_t* image, const rawtex_t* r)
{
    assert(image);
    {
    const char* lumpName = W_LumpName(r->lump);
    ddstring_t searchPath, foundPath;
    byte result = 0;

    // First try to find an external resource.
    Str_Init(&searchPath); Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", lumpName);
    Str_Init(&foundPath);

    if(F_FindResourceStr2(RC_GRAPHIC, &searchPath, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {   // "External" image loaded.
        result = 2;
    }
    else
    {
        DFILE* file;
        lumpnum_t lumpIndex = W_CheckNumForName(lumpName);
        if(NULL != (file = F_OpenLump(lumpIndex, false)))
        {
            if(0 != GL_LoadImageDFile(image, file, lumpName))
            {
                result = 1;
            }
            else
            {   // It must be an old-fashioned "raw" image.
#define RAW_WIDTH           320
#define RAW_HEIGHT          200

                size_t fileLength = F_Length(file);
                size_t bufSize = 3 * RAW_WIDTH * RAW_HEIGHT;

                GL_InitImage(image);
                image->pixels = malloc(bufSize);
                if(fileLength < bufSize)
                    memset(image->pixels, 0, bufSize);

                // Load the raw image data.
                F_Read(image->pixels, fileLength, file);
                image->width = RAW_WIDTH;
                image->height = (int) (fileLength / image->width);
                image->pixelSize = 1;
                result = 1;

#undef RAW_HEIGHT
#undef RAW_WIDTH
            }
            F_Close(file);
        }
    }

    Str_Free(&searchPath);
    Str_Free(&foundPath);

    return result;
    }
}

DGLuint GL_PrepareRawTex2(rawtex_t* raw)
{
    if(!raw)
        return 0; // Wha?

    if(raw->lump < 0 || raw->lump >= W_NumLumps())
    {
        GL_BindTexture(0, 0);
        return 0;
    }

    if(!raw->tex)
    {
        image_t image;
        byte result;

        // Clear any old values.
        memset(&image, 0, sizeof(image));

        if(2 == (result = GL_LoadRawTex(&image, raw)))
        {   // Loaded an external raw texture.
            raw->tex = GL_UploadTextureWithParams(image.pixels, image.width, image.height,
                image.pixelSize == 4? DGL_RGBA : DGL_RGB, false, false, false,
                GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST),
                0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0);
        }
        else
        {
            raw->tex = GL_UploadTextureWithParams(image.pixels, image.width, image.height,
                (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 :
                          image.pixelSize == 4? DGL_RGBA :
                          image.pixelSize == 3? DGL_RGB : DGL_COLOR_INDEX_8,
                false, false, false, GL_NEAREST,
                (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0);

        }

        raw->width  = image.width;
        raw->height = image.height;
        GL_DestroyImagePixels(&image);
    }

    return raw->tex;
}

DGLuint GL_PrepareRawTex(rawtex_t* rawTex)
{
    if(rawTex)
    {
        if(!rawTex->tex)
        {   // The rawtex isn't yet bound with OpenGL.
            rawTex->tex = GL_PrepareRawTex2(rawTex);
        }
        return rawTex->tex;
    }
    return 0;
}

DGLuint GL_GetLightMapTexture(const dduri_t* uri)
{
    if(uri)
    {
        lightmap_t* lmap;
        if(!Str_CompareIgnoreCase(Uri_Path(uri), "-"))
            return 0;

        if(0 != (lmap = R_GetLightMap(uri)))
        {
            texturevariantspecification_t* texSpec =
                GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_LIGHTMAP,
                    0, 0, 0, 0, GL_CLAMP, GL_CLAMP, -1, false, false, false, true);
            const texturevariant_t* tex;
            if(NULL != (tex = GL_PrepareTexture(GL_ToTexture(lmap->id), texSpec)))
                return TextureVariant_GLName(tex);
        }
    }
    // Return the default texture name.
    return GL_PrepareLSTexture(LST_DYNAMIC);
}

DGLuint GL_GetFlareTexture(const dduri_t* uri, int oldIdx)
{
    if(uri)
    {
        const ddstring_t* path = Uri_Path(uri);
        flaretex_t* fTex;

        if(Str_At(path, 0) == '-' || (Str_At(path, 0) == '0' && !Str_At(path, 1)))
            return 0; // Use the automatic selection logic.

        if(Str_At(path, 0) >= '1' && Str_At(path, 0) <= '4' && !Str_At(path, 1))
            return GL_PrepareSysFlareTexture(Str_At(path, 0) - '1');

        if((fTex = R_GetFlareTexture(uri)))
        {
            texturevariantspecification_t* texSpec =
                GL_TextureVariantSpecificationForContext(TC_HALO_LUMINANCE,
                    TSF_NO_COMPRESSION, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0, false, false, false, true);
            const texturevariant_t* tex;
            if(NULL != (tex = GL_PrepareTexture(GL_ToTexture(fTex->id), texSpec)))
                return TextureVariant_GLName(tex);
        }
    }
    else if(oldIdx > 0 && oldIdx < NUM_SYSFLARE_TEXTURES)
    {
        return GL_PrepareSysFlareTexture(oldIdx-1);
    }
    return 0; // Use the automatic selection logic.
}

DGLuint GL_PreparePatch(patchtex_t* patchTex)
{
    if(patchTex)
    {
        texturevariantspecification_t* texSpec =
            GL_TextureVariantSpecificationForContext(TC_UI,
                0 | ((patchTex->flags & PF_MONOCHROME)         ? TSF_MONOCHROME : 0)
                  | ((patchTex->flags & PF_UPSCALE_AND_SHARPEN)? TSF_UPSCALE_AND_SHARPEN : 0),
                0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, 0, false, false, false, false);
        const texturevariant_t* tex;
        if(NULL != (tex = GL_PrepareTexture(GL_ToTexture(patchTex->texId), texSpec)))
            return TextureVariant_GLName(tex);
    }
    return 0;
}

boolean GL_OptimalTextureSize(int width, int height, boolean noStretch, boolean isMipMapped,
    int* optWidth, int* optHeight)
{
    assert(optWidth && optHeight);
    if(GL_state.features.texNonPowTwo && !isMipMapped)
    {
        *optWidth  = width;
        *optHeight = height;
    }
    else if(noStretch)
    {
        *optWidth  = M_CeilPow2(width);
        *optHeight = M_CeilPow2(height);
    }
    else
    {
        // Determine the most favorable size for the texture.
        if(texQuality == TEXQ_BEST) // The best quality.
        {
            // At the best texture quality *opt, all textures are
            // sized *upwards*, so no details are lost. This takes
            // more memory, but naturally looks better.
            *optWidth  = M_CeilPow2(width);
            *optHeight = M_CeilPow2(height);
        }
        else if(texQuality == 0)
        {
            // At the lowest quality, all textures are sized down to the
            // nearest power of 2.
            *optWidth  = M_FloorPow2(width);
            *optHeight = M_FloorPow2(height);
        }
        else
        {
            // At the other quality *opts, a weighted rounding is used.
            *optWidth  = M_WeightPow2(width,  1 - texQuality / (float) TEXQ_BEST);
            *optHeight = M_WeightPow2(height, 1 - texQuality / (float) TEXQ_BEST);
        }
    }

    // Hardware limitations may force us to modify the preferred size.
    if(*optWidth > GL_state.maxTexSize)
    {
        *optWidth = GL_state.maxTexSize;
        noStretch = false;
    }
    if(*optHeight > GL_state.maxTexSize)
    {
        *optHeight = GL_state.maxTexSize;
        noStretch = false;
    }

    // Some GL drivers seem to have problems with VERY small textures.
    if(*optWidth < MINTEXWIDTH)
        *optWidth = MINTEXWIDTH;
    if(*optHeight < MINTEXHEIGHT)
        *optHeight = MINTEXHEIGHT;

    if(ratioLimit)
    {
        if(*optWidth > *optHeight) // Wide texture.
        {
            if(*optHeight < *optWidth / ratioLimit)
                *optHeight = *optWidth / ratioLimit;
        }
        else // Tall texture.
        {
            if(*optWidth < *optHeight / ratioLimit)
                *optWidth = *optHeight / ratioLimit;
        }
    }

    return noStretch;
}

static void setTextureMinMode(DGLuint tex, int minMode)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMode);
}

void GL_SetRawTextureParams(int minMode)
{
    rawtex_t** rawTexs, **ptr;

    rawTexs = R_CollectRawTexs(NULL);
    for(ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t* r = (*ptr);
        if(r->tex) // Is the texture loaded?
            setTextureMinMode(r->tex, minMode);
    }
    Z_Free(rawTexs);
}

void GL_SetTextureParams(int minMode, int gameTex, int uiTex)
{
    if(gameTex)
    {
        GL_SetAllTexturesMinFilter(minMode);
    }

    if(uiTex)
    {
        GL_SetRawTextureParams(minMode);
    }
}

void GL_UpdateTexParams(int mipmode)
{
    mipmapping = mipmode;
    GL_SetTextureParams(glmode[mipmode], true, false);
}

void GL_DoUpdateTexParams(const cvar_t* unused)
{
    GL_SetTextureParams(glmode[mipmapping], true, true);
}

static int doTexReset(void* parm)
{
    boolean usingBusyMode = *((boolean*) parm);

    /// \todo re-upload ALL textures currently in use.
    GL_LoadSystemTextures();
    Rend_ParticleLoadExtraTextures();
    R_SkyUpdate();

    if(usingBusyMode)
    {
        Con_SetProgress(200);
        Con_BusyWorkerEnd();
    }
    return 0;
}

void GL_TexReset(void)
{
    boolean useBusyMode = !Con_IsBusy();

    GL_ClearTextureMemory();
    FR_Update();
    Con_Printf("All DGL textures deleted.\n");

    if(useBusyMode)
    {
        Con_InitProgress(200);
        Con_Busy(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Reseting textures...", doTexReset, &useBusyMode);
    }
    else
    {
        doTexReset(&useBusyMode);
    }
}

void GL_DoUpdateTexGamma(const cvar_t* unused)
{
    if(texInited)
    {
        calcGammaTable();
        GL_TexReset();
    }

    Con_Printf("Gamma correction set to %f.\n", texGamma);
}

void GL_DoTexReset(const cvar_t* unused)
{
    GL_TexReset();
}

void GL_DoResetDetailTextures(const cvar_t* unused)
{
    GL_ReleaseGLTexturesByNamespace(TN_DETAILS);
}

void GL_DeleteRawImages(void)
{
    rawtex_t** rawTexs, **ptr;

    rawTexs = R_CollectRawTexs(NULL);
    for(ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t* r = (*ptr);
        if(r->tex)
        {
            glDeleteTextures(1, (const GLuint*) &r->tex);
            r->tex = 0;
        }
    }
    Z_Free(rawTexs);
}

int setGLMinFilter(texturevariant_t* tex, void* paramaters)
{
    DGLuint glName;
    if(0 != (glName = TextureVariant_GLName(tex)))
    {
        int minFilter = *((int*)paramaters);
        glBindTexture(GL_TEXTURE_2D, glName);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    }
    return 0; // Continue iteration.
}

void GL_SetAllTexturesMinFilter(int minFilter)
{
    int localMinFilter = minFilter;
    int i;
    for(i = 0; i < texturesCount; ++i)
        Texture_IterateVariants(textures[i], setGLMinFilter, (void*)&localMinFilter);
}

const texture_t* GL_CreateTexture(const char* rawName, uint index,
    texturenamespaceid_t texNamespace)
{
    assert(VALID_TEXTURENAMESPACE(texNamespace));
    {
    texturenamespace_hashnode_t* node;
    texture_t* tex;
    uint hash;

    // Check if we've already created a texture for this.
    { const texture_t* existingTex = GL_TextureByIndex(index, texNamespace);
    if(NULL != existingTex)
        return existingTex;
    }

    if(!rawName || !rawName[0])
        Con_Error("GL_CreateTexture: Cannot create texture with NULL name.");

    /**
     * A new texture.
     */

    tex = Texture_Construct(texturesCount+1/*1-based index*/, rawName, texNamespace, index);

    // We also hash the name for faster searching.
    hash = hashForTextureName(Texture_Name(tex));
    if(NULL == (node = (texturenamespace_hashnode_t*) malloc(sizeof(*node))))
        Con_Error("GL_CreateTexture: Failed on allocation of %lu bytes for "
                  "namespace hashnode.", (unsigned long) sizeof(*node));
    node->textureIndex = texturesCount + 1; // 1-based index.
    node->next = textureNamespaces[texNamespace-TEXTURENAMESPACE_FIRST].hashTable[hash];
    textureNamespaces[texNamespace-TEXTURENAMESPACE_FIRST].hashTable[hash] = node;

    // Link the new texture into the global list of gltextures.
    textures = (texture_t**) realloc(textures, sizeof(texture_t*) * ++texturesCount);
    textures[texturesCount - 1] = tex;

    return tex;
    }
}

uint GL_TextureIndexForUri2(const dduri_t* uri, boolean silent)
{
    const texture_t* glTex;
    if((glTex = GL_TextureByUri2(uri, silent)))
        return Texture_TypeIndex(glTex) + 1; // 1-based index.
    if(!silent)
    {
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, unknown texture: %s\n", Str_Text(path));
        Str_Delete(path);
    }
    return 0;
}

uint GL_TextureIndexForUri(const dduri_t* uri)
{
    return GL_TextureIndexForUri2(uri, false);
}

static boolean variantIsPrepared(texturevariant_t* variant)
{
    assert(texInited);
    return TextureVariant_IsUploaded(variant) && 0 != TextureVariant_GLName(variant);
}

static texturevariant_t* findPreparedVariant(texture_t* tex,
    const texturevariantspecification_t* spec)
{
    assert(texInited);
    {
    texturevariant_t* variant = GL_ChooseTextureVariant(tex, spec);
    return NULL != variant && variantIsPrepared(variant)? variant : NULL;
    }
}

static texturevariant_t* tryLoadImageAndPrepareVariant(texture_t* tex,
    texturevariant_t* variant, texturevariantspecification_t* spec,
    byte* result)
{
    assert(texInited && spec);
    {
    byte loadResult = 0;
    image_t image;

    // Load the source image data.
    switch(spec->type)
    {
    case TST_GENERAL:
        if(TN_TEXTURES == Texture_Namespace(tex))
        {
            // Try to load a replacement version of this texture?
            if(!noHighResTex && (loadExtAlways || highResWithPWAD || Texture_IsFromIWAD(tex)))
            {
                patchcompositetex_t* texDef = R_PatchCompositeTextureByIndex(Texture_TypeIndex(tex));
                const ddstring_t suffix = { "-ck" };
                ddstring_t searchPath;
                assert(NULL != texDef);
                Str_Init(&searchPath);
                Str_Appendf(&searchPath, TEXTURES_RESOURCE_NAMESPACE_NAME":%s;", texDef->name);
                loadResult = GL_LoadExtTextureEX(&image, Str_Text(&searchPath), Str_Text(&suffix), true); 
                Str_Free(&searchPath);
            }

            if(0 == loadResult)
            {
                if(TC_SKYSPHERE_DIFFUSE != TS_GENERAL(spec)->context)
                {
                    loadResult = GL_LoadPatchComposite(&image, tex);
                }
                else
                {
                    loadResult = GL_LoadPatchCompositeAsSky(&image, tex,
                        (TS_GENERAL(spec)->flags & TSF_ZEROMASK) != 0);
                }
            }
        }
        else
        {
            loadResult = loadSourceImage(&image, tex, spec);
        }
        break;

    case TST_DETAIL: {
        const detailtex_t* dTex;
        int idx = Texture_TypeIndex(tex);
        assert(idx >= 0 && idx < detailTexturesCount);
        dTex = detailTextures[idx];
        if(dTex->isExternal)
        {
            ddstring_t* searchPath = Uri_ComposePath(dTex->filePath);
            loadResult = GL_LoadExtTextureEX(&image, Str_Text(searchPath), NULL, false);
            Str_Delete(searchPath);
        }
        else
        {
            lumpnum_t lumpNum = W_CheckNumForName2(Str_Text(Uri_Path(dTex->filePath)), true);
            loadResult = GL_LoadDetailTextureLump(&image, lumpNum);
        }
        break;
      }
    }

    if(result) *result = loadResult;

    if(0 == loadResult)
    {   // No image found/failed to load.
        //Con_Message("Warning:Textures::tryLoadImageAndPrepareVariant: No image found for "
        //      "\"%s\"\n", Texture_Name(tex));
        return NULL;
    }

    // Do we need to allocate a variant?
    if(NULL == variant)
    {
        DGLuint newGLName = GL_GetReservedTextureName();
        variant = TextureVariant_Construct(tex, newGLName, spec);
        Texture_AddVariant(tex, variant);
    }
    // Are we re-preparing a released texture?
    else if(0 == TextureVariant_GLName(variant))
    {
        DGLuint newGLName = GL_GetReservedTextureName();
        TextureVariant_SetGLName(variant, newGLName);
    }

    // (Re)Prepare the variant according to the usage context.
    switch(spec->type)
    {
    case TST_GENERAL: prepareVariant(variant, &image); break;
    case TST_DETAIL:  prepareDetailVariant(variant, &image); break;
    }

    return variant;
    }
}

const texturevariant_t* GL_PrepareTexture2(texture_t* tex, texturevariantspecification_t* spec,
    preparetextureresult_t* returnOutcome)
{
    assert(texInited);
    {
    // Have we already prepared something suitable?
    texturevariant_t* variant = findPreparedVariant(tex, spec);

    if(NULL != variant)
    {
        if(returnOutcome) *returnOutcome = PTR_FOUND;
    }
    else
    {   // Suffer the cache miss and attempt to (re)prepare a variant.
        byte loadResult;

        variant = tryLoadImageAndPrepareVariant(tex, variant, spec, &loadResult);

        if(returnOutcome)
        switch(loadResult)
        {
        case 1:     *returnOutcome = PTR_UPLOADED_ORIGINAL; break;
        case 2:     *returnOutcome = PTR_UPLOADED_EXTERNAL; break;
        default:    *returnOutcome = PTR_NOTFOUND; break;
        }
    }

    return variant;
    }
}

const texturevariant_t* GL_PrepareTexture(texture_t* tex, texturevariantspecification_t* spec)
{
    return GL_PrepareTexture2(tex, spec, NULL);
}

texturevariant_t* GL_ChooseTextureVariant(texture_t* tex,
    const texturevariantspecification_t* spec)
{
    if(!texInited)
        Con_Error("GL_ChooseTextureVariant: Textures collection not yet initialized.");
    return chooseTextureVariant(tex, spec);
}

void GL_ReleaseGLTexturesForTexture(texture_t* tex)
{
    assert(tex);
    Texture_IterateVariants(tex, releaseVariantGLTexture, 0);
}

void GL_ReleaseGLTexturesByNamespace(texturenamespaceid_t texNamespace)
{
    if(texNamespace != TN_ANY && !VALID_TEXTURENAMESPACE(texNamespace))
        Con_Error("GL_ReleaseGLTexturesByNamespace: Internal error, "
                  "invalid namespace %i.", (int) texNamespace);

    { int i;
    for(i = 0; i < texturesCount; ++i)
    {
        texture_t* tex = textures[i];
        if(texNamespace != TN_ANY && Texture_Namespace(tex) != texNamespace)
            continue;
        GL_ReleaseGLTexturesForTexture(tex);
    }}
}

texture_t* GL_ToTexture(textureid_t id)
{
    texture_t* tex = getTexture(id);
#if _DEBUG
    if(NULL == tex)
        Con_Message("Warning:GL_ToTexture: Failed to locate texture for id #%i.\n", id);
#endif
    return tex;
}

const texture_t* GL_TextureByUri2(const dduri_t* uri, boolean silent)
{
    ddstring_t* path;
    if(uri && NULL != (path = Uri_Resolved(uri)))
    {
        texturenamespaceid_t texNamespace = DD_ParseTextureNamespace(Str_Text(Uri_Scheme(uri)));
        const texture_t* tex;

        if(texNamespace == TEXTURENAMESPACE_COUNT)
        {
            if(!silent)
            {
                ddstring_t* path = Uri_ToString(uri);
                Con_Message("Warning, unknown texture namespace '%s' encountered parsing "
                            "uri: %s", Str_Text(Uri_Scheme(uri)), Str_Text(path));
                Str_Delete(path);
            }
            return NULL;
        }

        tex = findTextureByName(Str_Text(Uri_Path(uri)), texNamespace);
        Str_Delete(path);
        return tex;
    }
    return NULL;
}

const texture_t* GL_TextureByUri(const dduri_t* uri)
{
    return GL_TextureByUri2(uri, false);
}

const texture_t* GL_TextureByIndex(int index, texturenamespaceid_t texNamespace)
{
    int i;
    for(i = 0; i < texturesCount; ++i)
    {
        texture_t* tex = textures[i];
        if(Texture_Namespace(tex) == texNamespace && Texture_TypeIndex(tex) == index)
            return tex;
    }
    return 0; // Not found.
}

void GL_InitTextureContent(texturecontent_t* content)
{
    assert(content);
    content->format = 0;
    content->name = 0;
    content->pixels = 0;
    content->palette = 0; // Use the default.
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

texturecontent_t* GL_ConstructTextureContentCopy(const texturecontent_t* other)
{
    assert(other);
    {
    texturecontent_t* c = malloc(sizeof(*c));
    uint8_t* pixels;
    int bytesPerPixel;
    size_t bufferSize;

    memcpy(c, other, sizeof(*c));

    // Duplicate the image buffer.
    bytesPerPixel = BytesPerPixelFmt(other->format);
    bufferSize = bytesPerPixel * other->width * other->height;
    pixels = malloc(bufferSize);
    memcpy(pixels, other->pixels, bufferSize);
    c->pixels = pixels;
    return c;
    }
}

void GL_DestroyTextureContent(texturecontent_t* content)
{
    assert(content);
    if(NULL != content->pixels)
        free((uint8_t*)content->pixels);
    free(content);
}

boolean GL_NewTexture(const texturecontent_t* content)
{
    if((content->flags & TXCF_NEVER_DEFER) || !Con_IsBusy())
    {
#ifdef _DEBUG
        Con_Message("GL_NewTexture: Uploading (%i:%ix%i) while not busy! Should be "
            "precached in busy mode?\n", content->name, content->width, content->height);
#endif

        // Let's do this right away. No need to take a copy.
        GL_UploadTextureContent(content);
        return false;
    }
    // Defer this operation. Need to make a copy.
    GL_EnqueueDeferredTask(DTT_UPLOAD_TEXTURECONTENT, GL_ConstructTextureContentCopy(content));
    return true;
}

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags)
{
    texturecontent_t c;

    GL_InitTextureContent(&c);
    c.format = format;
    c.width = width;
    c.height = height;
    c.pixels = pixels;
    c.flags = flags;
    c.name = GL_GetReservedTextureName();
    GL_NewTexture(&c);
    return c.name;
}

DGLuint GL_NewTextureWithParams3(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int grayMipmap, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT, boolean* didDefer)
{
    texturecontent_t c;
    boolean deferred;

    GL_InitTextureContent(&c);
    c.format = format;
    c.width = width;
    c.height = height;
    c.pixels = pixels;
    c.flags = flags;
    c.magFilter = magFilter;
    c.minFilter = minFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;
    c.grayMipmap = grayMipmap;
    c.name = GL_GetReservedTextureName();
    deferred = GL_NewTexture(&c);
    if(didDefer)
        *didDefer = deferred;
    return c.name;
}

DGLuint GL_NewTextureWithParams2(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int grayMipmap, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT)
{
    return GL_NewTextureWithParams3(format, width, height, pixels, flags,
        grayMipmap, minFilter, magFilter, anisoFilter, wrapS, wrapT, NULL);
}

D_CMD(LowRes)
{
    GL_LowRes();
    return true;
}

D_CMD(ResetTextures)
{
    if(argc == 2 && !stricmp(argv[1], "raw"))
    {
        // Reset just raw images.
        GL_DeleteRawImages();
    }
    else
    {
        // Reset everything.
        GL_TexReset();
    }
    return true;
}

D_CMD(MipMap)
{
    GL_UpdateTexParams(strtol(argv[1], NULL, 0));
    return true;
}
