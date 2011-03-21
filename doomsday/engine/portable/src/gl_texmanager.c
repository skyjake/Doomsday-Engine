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

boolean allowMaskedTexEnlarge = false;
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
                  (unsigned int) sizeof(*spec));
    memcpy(spec, tpl, sizeof(texturevariantspecification_t));
    return spec;
}

static texturevariantspecification_t* copyDetailVariantSpecification(
    const texturevariantspecification_t* tpl)
{
    texturevariantspecification_t* spec;
    if(NULL == (spec = (texturevariantspecification_t*) malloc(sizeof(*spec))))
        Con_Error("Textures::copyDetailVariantSpecification: Failed on allocation of "
                  "%lu bytes for new TextureVariantSpecification.",
                  (unsigned int) sizeof(*spec));
    memcpy(spec, tpl, sizeof(texturevariantspecification_t));
    return spec;
}

static int compareVariantSpecifications(const variantspecification_t* a,
    const variantspecification_t* b)
{
    /// \todo We can be a bit cleverer here...
    if(a->context != b->context)
        return 1;
    if(a->flags  != b->flags)
        return 1;
    if(a->border != b->border)
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
    if(!INRANGE_OF(a->contrast, b->contrast, .05f))
        return 1;
    return 0; // Equal.
}

static variantspecification_t* applyVariantSpecification(
    variantspecification_t* spec, texturevariantusagecontext_t tc, int flags,
    byte border, int tClass, int tMap)
{
    assert(spec && (tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc)));

    spec->context = tc;
    spec->flags = flags & ~TSF_INTERNAL_MASK;
    spec->border = border;

    if(0 != tMap || 0 != tClass)
    {
        colorpalettetranslationspecification_t* cpt;
        spec->flags |= TSF_HAS_COLORPALETTE_XLAT;
        if(NULL == (cpt = (colorpalettetranslationspecification_t*) malloc(sizeof(*cpt))))
            Con_Error("applyVariantSpecification: Failed on allocation of %lu bytes for "
                      "new ColorPaletteTranslationSpecification.", (unsigned int) sizeof(*cpt));
        cpt->tMap   = tMap;
        cpt->tClass = tClass;
        spec->translated = cpt;
    }
    return spec;
}

static detailvariantspecification_t* applyDetailVariantSpecification(
    detailvariantspecification_t* spec, float contrast)
{
    assert(spec);
    // Round off contrast to nearest 1/10
    spec->contrast = MINMAX_OF(0, (int) ((contrast + .05f) * 10) / 10.f, 1);
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
                  (unsigned int) sizeof(*node));
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
        switch(type)
        {
        case TS_NORMAL:
            if(!compareVariantSpecifications(TS_NORMAL(node->spec), TS_NORMAL(tpl)))
                return node->spec;
            break;
        case TS_DETAIL:
            if(!compareDetailVariantSpecifications(TS_DETAIL(node->spec), TS_DETAIL(tpl)))
                return node->spec;
            break;
        }
    }
    if(!canCreate)
        return NULL;
    switch(type)
    {
    case TS_NORMAL:
        return linkVariantSpecification(type, copyVariantSpecification(tpl));
    case TS_DETAIL:
        return linkVariantSpecification(type, copyDetailVariantSpecification(tpl));
    }
    // Unreachable (hopefully).
    assert(1);
    return NULL;
    }
}

static texturevariantspecification_t* getVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap)
{
    static texturevariantspecification_t tpl;
    assert(texInited);
    tpl.type = TS_NORMAL;
    applyVariantSpecification(TS_NORMAL(&tpl), tc, flags, border, tClass, tMap);
    return findVariantSpecification(tpl.type, &tpl, true);
}

static texturevariantspecification_t* getDetailVariantSpecificationForContext(
    float contrast)
{
    static texturevariantspecification_t tpl;
    assert(texInited);
    tpl.type = TS_DETAIL;
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
            if(node->spec->type == TS_NORMAL &&
               (TS_NORMAL(node->spec)->flags & TSF_HAS_COLORPALETTE_XLAT))
                free(TS_NORMAL(node->spec)->translated);
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
    if(cand->type == p->type)
    switch(p->type)
    {
    case TS_NORMAL:
        if(!compareVariantSpecifications(TS_NORMAL(cand), TS_NORMAL(p->spec)))
        {   // This will do fine.
            p->chosen = variant;
            return 1; // Stop iteration.
        }
        break;
    case TS_DETAIL:
        if(!compareDetailVariantSpecifications(TS_DETAIL(cand), TS_DETAIL(p->spec)))
        {   // This will do fine.
            p->chosen = variant;
            return 1; // Stop iteration.
        }
        break;
    default:
        Con_Error("chooseTextureVariantWorker: Invalid specification type %i.", (int) p->type);
        return 1; // Unreachable.
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
    DGLuint glName = TextureVariant_GLName(variant);
    // Have we uploaded yet?
    if(0 != glName)
    {
        // Delete and mark it not-loaded.
        glDeleteTextures(1, (const GLuint*) &glName);
        TextureVariant_SetGLName(variant, 0);
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

static byte loadSourceImage(image_t* img, const texturevariant_t* tex)
{
    assert(img && tex);
    {
    const texture_t* generalCase = TextureVariant_GeneralCase(tex);
    const variantspecification_t* spec = TS_NORMAL(TextureVariant_Spec(tex));
    byte loadResult = 0;
    switch(Texture_Namespace(generalCase))
    {
    case TN_FLATS:
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
    case TN_PATCHES:
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
            loadResult = GL_LoadDoomPatchLump(img, generalCase, (spec->flags & TSF_UPSCALE_AND_SHARPEN) != 0);
        break;
    case TN_SPRITES: {
        int tclass = 0, tmap = 0;
        if(spec->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            assert(spec->translated);
            tclass = spec->translated->tClass;
            tmap   = spec->translated->tMap;
        }

        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            const spritetex_t* sprTex = R_SpriteTextureByIndex(Texture_TypeIndex(generalCase));
            ddstring_t searchPath, suffix = { "-ck" };

            assert(NULL != sprTex);

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

            /// \fixme What about the border?
            loadResult = GL_LoadExtTextureEX(img, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }
        if(0 == loadResult)
            loadResult = GL_LoadSpriteLump(img, generalCase, tclass, tmap, spec->border);
        break;
      }
    case TN_SYSTEM:
    case TN_REFLECTIONS:
    case TN_MASKS:
    case TN_LIGHTMAPS:
    case TN_FLAREMAPS:
    case TN_MODELSKINS:
    case TN_MODELREFLECTIONSKINS: {
        ddstring_t* path = Uri_ComposePath(searchPath(Texture_Namespace(generalCase), Texture_TypeIndex(generalCase)));
        loadResult = GL_LoadExtTextureEX(img, Str_Text(path), NULL, false);
        Str_Delete(path);
        break;
      }
    default:
        Con_Error("Textures::loadSourceImage: Unknown texture namespace %i.",
                  (int) Texture_Namespace(generalCase));
        return 0; // Unreachable.
    }
    return loadResult;
    }
}

static byte prepareVariant(texturevariant_t* tex)
{
    assert(tex);
    {
    const variantspecification_t* spec = TS_NORMAL(TextureVariant_Spec(tex));
    boolean monochrome    = (spec->flags & TSF_MONOCHROME) != 0;
    boolean noCompression = (spec->flags & TSF_NO_COMPRESSION) != 0;
    boolean scaleSharp    = (spec->flags & TSF_UPSCALE_AND_SHARPEN) != 0;
    int magFilter, minFilter, anisoFilter, wrapS, wrapT, grayMipmap = 0, flags = 0;
    boolean noSmartFilter = false, didDefer = false, loadedPaletted = false;
    dgltexformat_t dglFormat;
    boolean alphaChannel;
    byte loadResult = 0;
    DGLuint glName;
    image_t image;
    float s, t;

    // Load in the raw source image.
    if(TN_TEXTURES == Texture_Namespace(TextureVariant_GeneralCase(tex)))
    {
        loadResult = GL_LoadDoomTexture(&image, TextureVariant_GeneralCase(tex),
            spec->context == TC_SKYSPHERE_DIFFUSE,
            (spec->flags & TSF_ZEROMASK) != 0);
    }
    else
    {
        loadResult = loadSourceImage(&image, tex);
    }

    if(0 == loadResult)
    {   // Not found/failed load.
        //Con_Message("Warning:Textures::prepareTextureVariant: No image found for \"%s\"\n",
        //            Texture_Name(TextureVariant_GeneralCase(tex)));
        return loadResult;
    }

    { texturenamespaceid_t texNamespace = Texture_Namespace(TextureVariant_GeneralCase(tex));
    if(TN_FLATS   == texNamespace || TN_TEXTURES == texNamespace ||
       TN_PATCHES == texNamespace || TN_SPRITES  == texNamespace)
        loadedPaletted = true;
    }

    if(image.pixelSize == 1)
    {
        if(monochrome && !scaleSharp)
            GL_DeSaturatePalettedImage(image.pixels, R_GetColorPalette(0), image.width, image.height);

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

            if(monochrome)
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
            if(monochrome)
            {   // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                GL_ConvertToLuminance(&image, true);
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
    else if(image.pixelSize > 2)
    {
        if(monochrome)
        {
            GL_ConvertToLuminance(&image, true);
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
            Con_Message("Textures::prepareTextureVariant: Warning, non RGB(A) texture larger than max size (%ix%i bpp%i).\n",
                        image.width, image.height, image.pixelSize);
        }
    }

    // Lightmaps and flare textures should always be monochrome images.
    if(((TC_MAPSURFACE_LIGHTMAP == spec->context) ||
        (TC_HALO_LUMINANCE      == spec->context && image.pixelSize != 4)) &&
       !(image.flags & IMGF_IS_MASKED))
    {
        // An alpha channel is required. If one is not in the image data, we'll generate it.
        GL_ConvertToAlpha(&image, true);
    }

    // Disable compression?
    if(noCompression || (image.width < 128 || image.height < 128) ||
       TC_HALO_LUMINANCE        == spec->context ||
       TC_MAPSURFACE_REFLECTION == spec->context)
        flags |= TXCF_NO_COMPRESSION;

    if(spec->context == TC_MODELSKIN_DIFFUSE)
        flags |= TXCF_APPLY_GAMMACORRECTION;

    if(image.pixelSize > 2)
    {
        if(TC_MAPSURFACE_DIFFUSE == spec->context ||
           TC_SPRITE_DIFFUSE     == spec->context ||
           TC_PSPRITE_DIFFUSE    == spec->context ||
           TC_SKYSPHERE_DIFFUSE  == spec->context)
            flags |= TXCF_APPLY_GAMMACORRECTION;
    }

    if(TC_SPRITE_DIFFUSE == spec->context)
        flags |= TXCF_UPLOAD_ARG_NOSTRETCH;

    switch(spec->context)
    {
    case TC_MAPSURFACE_DIFFUSE:
    //case TC_MAPSURFACE_REFLECTION:
    case TC_MAPSURFACE_REFLECTIONMASK:
    //case TC_MAPSURFACE_LIGHTMAP:
    case TC_SPRITE_DIFFUSE:
    case TC_MODELSKIN_DIFFUSE:
    //case TC_MODELSKIN_REFLECTION:
        flags |= TXCF_MIPMAP;
        break;
    default: break;
    }

    alphaChannel = false;
    if(!monochrome)
    {
        texturenamespaceid_t texNamespace = Texture_Namespace(TextureVariant_GeneralCase(tex));
        if(!(TN_SYSTEM      == texNamespace ||
             TN_REFLECTIONS == texNamespace ||
             TN_MASKS       == texNamespace))
            flags |= TXCF_EASY_UPLOAD;
    }

    if(loadedPaletted)
    {
        if(!monochrome && image.pixelSize > 1)
            flags |= TXCF_UPLOAD_ARG_RGBDATA;

        if((image.pixelSize == 1 && (image.flags & IMGF_IS_MASKED)) ||
           image.pixelSize == 4)
            alphaChannel = true;
    }
    else
    {
        if(!monochrome && image.pixelSize > 2 &&
            !(TC_MAPSURFACE_REFLECTIONMASK == spec->context ||
              TC_MAPSURFACE_LIGHTMAP       == spec->context))
            flags |= TXCF_UPLOAD_ARG_RGBDATA;

        if(!(TC_MAPSURFACE_REFLECTIONMASK == spec->context ||
             TC_MAPSURFACE_REFLECTION     == spec->context))
        {
            if(image.pixelSize == 2 || image.pixelSize == 4)
                alphaChannel = true;
        }
    }

    if(alphaChannel)
        flags |= TXCF_UPLOAD_ARG_ALPHACHANNEL;

    if(noSmartFilter)
        flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

    if(monochrome)
    {
        dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
    }
    else
    {
        if(loadedPaletted)
        {
            if(image.pixelSize == 1)
                dglFormat = alphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8;
            else
                dglFormat = alphaChannel? DGL_RGBA : DGL_RGB;
        }
        else
        {
            dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                          image.pixelSize == 3 ? DGL_RGB :
                          image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE );
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

    switch(spec->context)
    {
    case TC_UI:
    case TC_PSPRITE_DIFFUSE:
    case TC_HALO_LUMINANCE:
        anisoFilter = 0 /*no anisotropic filtering*/;
        break;
    default:
        anisoFilter = texAniso; /// \fixme is "best" truely a suitable default?
        break;
    }

    switch(spec->context)
    {
    case TC_UI:
    case TC_SPRITE_DIFFUSE:
    case TC_PSPRITE_DIFFUSE:
    case TC_HALO_LUMINANCE:
    case TC_MAPSURFACE_LIGHTMAP:
        wrapS = wrapT = GL_CLAMP_TO_EDGE;
        break;
    default:
        wrapS = wrapT = GL_REPEAT;
        break;
    }

    glName = GL_NewTextureWithParams3(dglFormat, image.width, image.height,
        image.pixels, flags, grayMipmap, minFilter, magFilter, anisoFilter, wrapS,
        wrapT, &didDefer);

    /// \todo Register name during variant construction/specification.
    TextureVariant_SetGLName(tex, glName);

#ifdef _DEBUG
    VERBOSE( Con_Printf("Prepared TextureVariant \"%s\" (glName:%i)%s\n",
                    Texture_Name(TextureVariant_GeneralCase(tex)), glName,
                    !didDefer? " while not busy!" : "") );
    VERBOSE2( GL_PrintTextureVariantSpecification(TextureVariant_Spec(tex)) );
#endif

    /**
     * Calculate texture coordinates based on the image dimensions. The
     * coordinates are calculated as width/CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     */
    if((TC_SPRITE_DIFFUSE  == spec->context ||
        TC_PSPRITE_DIFFUSE == spec->context) &&
       GL_state.features.texNonPowTwo && !(flags & TXCF_UPLOAD_ARG_NOSTRETCH) &&
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

    if(TC_SKYSPHERE_DIFFUSE == spec->context)
    {
        averagecolor_analysis_t* avgTopColor;
        if(NULL == (avgTopColor = (averagecolor_analysis_t*)TextureVariant_Analysis(tex, TA_SKY_SPHEREFADECOLOR)))
        {
            if(NULL == (avgTopColor = (averagecolor_analysis_t*) malloc(sizeof(*avgTopColor))))
                Con_Error("Textures::prepareTextureVariant: Failed on allocation of %lu bytes for "
                          "new AverageColorAnalysis.", (unsigned int) sizeof(*avgTopColor));
            TextureVariant_AddAnalysis(tex, TA_SKY_SPHEREFADECOLOR, avgTopColor);
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
   
    if(TC_SPRITE_DIFFUSE == spec->context)
    {
        pointlight_analysis_t* pl;
        if(NULL == (pl = (pointlight_analysis_t*)TextureVariant_Analysis(tex, TA_SPRITE_AUTOLIGHT)))
        {
            if(NULL == (pl = (pointlight_analysis_t*) malloc(sizeof(*pl))))
                Con_Error("Textures::prepareTextureVariant: Failed on allocation of %lu bytes for "
                          "new PointLightAnalysis.", (unsigned int) sizeof(*pl));
            TextureVariant_AddAnalysis(tex, TA_SPRITE_AUTOLIGHT, pl);
        }
        // Calculate light source properties.
        GL_CalcLuminance(image.pixels, image.width, image.height, image.pixelSize, 0,
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
                          "new AmbientLightAnalysis.", (unsigned int) sizeof(*al));
            TextureVariant_AddAnalysis(tex, TA_MAP_AMBIENTLIGHT, al);
        }

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

static byte prepareDetailVariant(texturevariant_t* tex)
{
    assert(tex);
    {
    const detailvariantspecification_t* spec = TS_DETAIL(TextureVariant_Spec(tex));
    int magFilter, minFilter, anisoFilter, wrapS, wrapT, grayMipmap = 0, flags = 0;
    boolean didDefer = false;
    dgltexformat_t dglFormat;
    byte loadResult = 0;
    DGLuint glName;
    image_t image;
    float s, t;

    // Load in the raw source image.
    {
    texture_t* generalCase = TextureVariant_GeneralCase(tex);
    int idx = Texture_TypeIndex(generalCase);
    const detailtex_t* dTex;
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
        loadResult = GL_LoadDetailTextureLump(&image, generalCase);
    }
    }

    if(0 == loadResult)
    {   // Not found/failed load.
        //Con_Message("Warning:Textures::prepareDetailVariant: No image found for \"%s\"\n",
        //            Texture_Name(TextureVariant_GeneralCase(tex)));
        return loadResult;
    }

    if(image.pixelSize > 2)
    {
        GL_ConvertToLuminance(&image, false);
    }

    {
    float baMul, hiMul, loMul;
    EqualizeLuma(image.pixels, image.width, image.height, &baMul, &hiMul, &loMul);
    if(verbose && (baMul != 1 || hiMul != 1 || loMul != 1))
    {
        VERBOSE2( Con_Message("Equalized TextureVariant \"%s\" (balance: %g, high amp: %g, low amp: %g).\n",
                    Texture_Name(TextureVariant_GeneralCase(tex)), baMul, hiMul, loMul) );
    }
    }

    // Disable compression?
    if(image.width < 128 || image.height < 128)
        flags |= TXCF_NO_COMPRESSION;

    /**
     * Detail textures are faded to gray depending on the contrast
     * factor. The texture is also progressively faded towards gray
     * when each mipmap level is loaded.
     */
    grayMipmap = MINMAX_OF(0, spec->contrast * 255, 255);
    flags |= TXCF_GRAY_MIPMAP;
    dglFormat = DGL_LUMINANCE;
    magFilter = glmode[texMagMode];
    minFilter = GL_LINEAR_MIPMAP_LINEAR;
    anisoFilter = texAniso;
    wrapS = wrapT = GL_REPEAT;

    // Too big for us?
    if(image.width  > GL_state.maxTexSize || image.height > GL_state.maxTexSize)
        Con_Message("Warning:Textures::prepareDetailVariant: Texture larger than max size (%ix%i bpp%i).\n",
                    image.width, image.height, image.pixelSize);

    glName = GL_NewTextureWithParams3(dglFormat, image.width, image.height,
        image.pixels, flags, grayMipmap, minFilter, magFilter, anisoFilter, wrapS,
        wrapT, &didDefer);

    /// \todo Register name during variant construction/specification.
    TextureVariant_SetGLName(tex, glName);

#ifdef _DEBUG
    VERBOSE( Con_Printf("Prepared TextureVariant \"%s\" (glName:%i)%s\n",
                    Texture_Name(TextureVariant_GeneralCase(tex)), glName,
                    !didDefer? " while not busy!" : "") );
    VERBOSE2( GL_PrintTextureVariantSpecification(TextureVariant_Spec(tex)) );
#endif

    {
    int pw = M_CeilPow2(image.width), ph = M_CeilPow2(image.height);
    s =  image.width / (float) pw;
    t = image.height / (float) ph;
    }

    TextureVariant_SetCoords(tex, s, t);
    TextureVariant_SetMasked(tex, false);

    GL_DestroyImagePixels(&image);
    return loadResult;
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

    // The -bigmtex option allows the engine to enlarge masked textures
    // that have taller patches than they are themselves.
    allowMaskedTexEnlarge = ArgExists("-bigmtex");

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

    if(NULL == spec) return;

    Con_Printf("type:%s", TS_DETAIL == spec->type? "detail" : "normal");

    switch(spec->type)
    {
    case TS_DETAIL:
        Con_Printf(" contrast:%g\n", TS_DETAIL(spec)->contrast);
        break;
    case TS_NORMAL: {
        texturevariantusagecontext_t tc = TS_NORMAL(spec)->context;
        assert(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

        Con_Printf(" context:%s flags:%i border:%i",
            textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1],
            TS_NORMAL(spec)->flags, TS_NORMAL(spec)->border);
        if(TS_NORMAL(spec)->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            const colorpalettetranslationspecification_t* cpt = TS_NORMAL(spec)->translated;
            assert(cpt);
            Con_Printf(" translated(tclass:%i tmap:%i)", cpt->tClass, cpt->tMap);
        }
        Con_Printf("\n");
        break;
      }
    }
}

texturevariantspecification_t* GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap)
{
    if(!texInited)
        Con_Error("GL_TextureVariantSpecificationForContext: Textures collection "
            "not yet initialized.");
    return getVariantSpecificationForContext(tc, flags, border, tClass, tMap);
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
    img->originalBits = 0;
    img->flags = 0;
    img->pixels = 0;
}

static boolean tryLoadPCX(image_t* img, DFILE* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = PCX_Load(file, &img->width, &img->height, &img->pixelSize);
    img->originalBits = 8 * img->pixelSize;
    return (0 != img->pixels);
}

static boolean tryLoadPNG(image_t* img, DFILE* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = PNG_Load(file, &img->width, &img->height, &img->pixelSize);
    img->originalBits = 8 * img->pixelSize;
    return (0 != img->pixels);
}

static boolean tryLoadTGA(image_t* img, DFILE* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = TGA_Load(file, &img->width, &img->height, &img->pixelSize);
    img->originalBits = 8 * img->pixelSize;
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
        img->originalBits = 32; // Effectively...
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
    if(img->pixels)
        free(img->pixels);
}

DGLuint GL_UploadTexture(const uint8_t* pixels, int width, int height,
    boolean flagAlphaChannel, boolean flagGenerateMipmaps, boolean flagRgbData,
    boolean flagNoStretch, boolean flagNoSmartFilter, int minFilter,
    int magFilter, int anisoFilter, int wrapS, int wrapT, int otherFlags)
{
    texturecontent_t content;

    GL_InitTextureContent(&content);
    content.pixels = pixels;
    content.format = (flagRgbData? (flagAlphaChannel? DGL_RGBA : DGL_RGB) :
                      (flagAlphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8));
    content.width = width;
    content.height = height;
    content.flags = TXCF_EASY_UPLOAD | otherFlags;
    if(flagAlphaChannel) content.flags |= TXCF_UPLOAD_ARG_ALPHACHANNEL;
    if(flagGenerateMipmaps) content.flags |= TXCF_MIPMAP;
    if(flagRgbData) content.flags |= TXCF_UPLOAD_ARG_RGBDATA;
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

DGLuint GL_UploadTexture2(const texturecontent_t* content)
{
    const uint8_t* pixels = (const uint8_t*) content->pixels;
    int width = content->width;
    int height = content->height;
    boolean alphaChannel = ((content->flags & TXCF_UPLOAD_ARG_ALPHACHANNEL) != 0);
    boolean generateMipmaps = ((content->flags & TXCF_MIPMAP) != 0);
    boolean RGBData = ((content->flags & TXCF_UPLOAD_ARG_RGBDATA) != 0);
    boolean noStretch = ((content->flags & TXCF_UPLOAD_ARG_NOSTRETCH) != 0);
    boolean noSmartFilter = ((content->flags & TXCF_UPLOAD_ARG_NOSMARTFILTER) != 0);
    boolean applyTexGamma = ((content->flags & TXCF_APPLY_GAMMACORRECTION) != 0);
    int i, comps, levelWidth, levelHeight; // width and height at the current level
    uint8_t* buffer = NULL, *rgbaOriginal = NULL, *idxBuffer = NULL;
    boolean freeOriginal;
    boolean freeBuffer;
    boolean oldCompression = GL_state.currentUseTexCompression;

    GL_SetTextureCompression((content->flags & TXCF_NO_COMPRESSION) == 0);

    // Number of color components in the destination image.
    comps = (alphaChannel ? 4 : 3);

    // Calculate the real dimensions for the texture, as required by
    // the graphics hardware.
    noStretch = GL_OptimalTextureSize(width, height, noStretch, generateMipmaps, &levelWidth, &levelHeight);

    // Get the RGB(A) version of the original texture.
    if(RGBData)
    {
        // The source image can be used as-is.
        freeOriginal = false;
        rgbaOriginal = (uint8_t*)pixels;
    }
    else
    {
        // Convert a paletted source image to truecolor so it can be scaled.
        // If there isn't an alpha channel one will be added.
        comps = 4;
        rgbaOriginal = GL_ConvertBuffer(pixels, width, height, alphaChannel ? 2 : 1, 0, true, comps);
        if(rgbaOriginal != pixels)
        {
            freeOriginal = true;
        }

        if(!alphaChannel)
        {
            int n;
            for(n = 0; n < width * height; ++n)
                rgbaOriginal[n * 4 + 3] = 255;
            alphaChannel = true;
        }
    }

    if(applyTexGamma)
    {
        for(i = 0; i < width * height * comps; i += comps)
        {
            rgbaOriginal[i]   = gammaTable[pixels[i]];
            rgbaOriginal[i+1] = gammaTable[pixels[i+1]];
            rgbaOriginal[i+2] = gammaTable[pixels[i+2]];
        }
    }

    if(useSmartFilter && !noSmartFilter)
    {
        uint8_t* filtered;
        int smartFlags = ICF_UPSCALE_SAMPLE_WRAP;

        if(comps == 3)
        {
            // Must convert to RGBA.
            uint8_t* temp = GL_ConvertBuffer(rgbaOriginal, width, height, 3, 0, true, 4);
            if(temp != rgbaOriginal)
            {
                if(freeOriginal)
                    free(rgbaOriginal);
            }
            rgbaOriginal = temp;
            freeOriginal = true;
            comps = 4;
            alphaChannel = true;
        }

        filtered = GL_SmartFilter(GL_ChooseSmartFilter(width, height, 0), rgbaOriginal, width, height, smartFlags, &width, &height);
        noStretch = GL_OptimalTextureSize(width, height, noStretch, generateMipmaps, &levelWidth, &levelHeight);

        if(filtered != rgbaOriginal)
        {
            freeOriginal = true;
            // The filtered copy will be the 'original' image data.
            if(freeOriginal)
                free(rgbaOriginal);
            rgbaOriginal = filtered;
        }
    }

    // Prepare the RGB(A) buffer for the texture: we want a buffer with
    // power-of-two dimensions. It will be the mipmap level zero.
    // The buffer will be modified in the mipmap generation (if done here).
    if(width == levelWidth && height == levelHeight)
    {
        // No resizing necessary.
        buffer = rgbaOriginal;
        freeBuffer = freeOriginal;
        freeOriginal = false;
    }
    else
    {
        if(noStretch)
        {
            // Copy the image into a buffer with power-of-two dimensions.
            if(0 == (buffer = malloc(levelWidth * levelHeight * comps)))
                Con_Error("GL_UploadTexture2: Failed on allocation of %lu bytes for"
                          "upscale buffer.", (unsigned long) (levelWidth * levelHeight * comps));

            memset(buffer, 0, levelWidth * levelHeight * comps);
            for(i = 0; i < height; ++i) // Copy line by line.
                memcpy(buffer + levelWidth * comps * i, rgbaOriginal + width * comps * i, comps * width);
            freeBuffer = true;
        }
        else
        {
            // Stretch to fit into power-of-two.
            if(width != levelWidth || height != levelHeight)
            {
                buffer = GL_ScaleBuffer(rgbaOriginal, width, height, comps, levelWidth, levelHeight);
                if(buffer != rgbaOriginal)
                    freeBuffer = true;
            }
        }
    }

    // The RGB(A) copy of the source image is no longer needed.
    if(freeOriginal)
    {
        free(rgbaOriginal);
        rgbaOriginal = NULL;
    }

    // Bind the texture so we can upload content.
    glBindTexture(GL_TEXTURE_2D, content->name);

    if(!GL_TexImage(alphaChannel ? DGL_RGBA : DGL_RGB, buffer, levelWidth, levelHeight,
                    0, generateMipmaps ? true : false))
    {
        Con_Error("GL_UploadTexture: TexImage failed (%i x %i), alpha:%i\n", levelWidth,
            levelHeight, alphaChannel);
    }

    if(freeBuffer)
        free(buffer);

    GL_SetTextureCompression(oldCompression);

    return content->name;
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
        // Too big for us? \todo Should not be done here.
        if(image->width  > GL_state.maxTexSize || image->height > GL_state.maxTexSize)
        {
            int newWidth = MIN_OF(image->width, GL_state.maxTexSize);
            int newHeight = MIN_OF(image->height, GL_state.maxTexSize);
            uint8_t* scaledPixels = GL_ScaleBuffer(image->pixels, image->width, image->height, image->pixelSize, newWidth, newHeight);
            if(scaledPixels != image->pixels)
            {
                free(image->pixels);
                image->pixels = scaledPixels;
                image->width = newWidth;
                image->height = newHeight;
            }
        }

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
 * @param checkForAlpha If @c true, the composited image will be checked
 *     for alpha pixels and will return accordingly if present.
 *
 * @return  If @a checkForAlpha @c = false will return @c false. Else,
 *          @c true iff the buffer really has alpha information.
 */
static int loadDoomPatch(uint8_t* buffer, int texwidth, int texheight,
    const doompatch_header_t* patch, int origx, int origy, int tclass,
    int tmap, boolean maskZero, boolean checkForAlpha)
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

    if(checkForAlpha)
    {
        boolean allowSingleAlpha = (texwidth < 128 || texheight < 128);
        int i;

        // Scan through the RGBA buffer and check for sub-0xff alpha.
        source = buffer + texwidth * texheight;
        for(i = 0, count = 0; i < texwidth * texheight; ++i)
        {
            if(source[i] < 0xff)
            {
                /// \kludge 'Small' textures tolerate no alpha.
                if(allowSingleAlpha)
                    return true;

                // Big ones can have a single alpha pixel (ZZZFACE3!).
                if(count++ > 1)
                    return true; // Has alpha data.
            }
        }
    }

    return false; // Doesn't have alpha data.
    }
}

byte GL_LoadDetailTextureLump(image_t* image, const texture_t* tex)
{
    assert(image && tex);
    {
    detailtex_t* dTex = detailTextures[Texture_TypeIndex(tex)];
    const char* lumpName;
    lumpnum_t lumpIndex;
    byte result = 0;
    DFILE* file;

    assert(dTex);

    if(dTex->isExternal)
        return 0;

    lumpName = Str_Text(Uri_Path(dTex->filePath));
    lumpIndex = W_CheckNumForName2(lumpName, true);
    if(NULL != (file = F_OpenLump(lumpIndex, false)))
    {
        if(0 != GL_LoadImageDFile(image, file, lumpName))
        {
            result = 1;
        }
        else
        {   // It must be an old-fashioned "raw" image.
            size_t fileLength = F_Length(file);

            GL_InitImage(image);

            /**
             * @fixme we should not error out here if the lump is not
             * of the required format! Perform this check much earlier,
             * when the definitions are read and mark which are valid.
             */

            // How big is it?
            if(fileLength != 256 * 256)
            {
                if(fileLength != 128 * 128)
                {
                    if(fileLength != 64 * 64)
                        Con_Error("GL_LoadDetailTexture: Must be 256x256, 128x128 or 64x64.\n");
                    image->width = image->height = 64;
                }
                else
                {
                    image->width = image->height = 128;
                }
            }
            else
                image->width = image->height = 256;

            image->pixelSize = 1;
            image->pixels = malloc(image->width * image->height);
            if(fileLength < (unsigned) (image->width * image->height))
                memset(image->pixels, 0, image->width * image->height);
            // Load the raw image data.
            F_Read(image->pixels, fileLength, file);
            result = 1;
        }
        F_Close(file);
    }
    return result;
    }
}

/**
 * @return  @c 1 = found and prepared a lump resource.
 */
byte GL_LoadFlatLump(image_t* image, const texture_t* tex)
{
    assert(image && tex);
    {
    flat_t* flat = R_FlatTextureByIndex(Texture_TypeIndex(tex));
    lumpnum_t lumpIndex = W_CheckNumForName2(flat->name, true);
    byte result = 0;
    DFILE* file;
    assert(NULL != flat);
    if(NULL != (file = F_OpenLump(lumpIndex, false)))
    {
        if(0 != GL_LoadImageDFile(image, file, flat->name))
        {
            result = 1;
        }
        else
        {   // It must be an old-fashioned DOOM flat.
#define FLAT_WIDTH          64
#define FLAT_HEIGHT         64

            size_t fileLength = F_Length(file);
            size_t bufSize = MAX_OF(fileLength, 4096);

            GL_InitImage(image);
            image->pixels = malloc(bufSize);
            if(fileLength < bufSize)
                memset(image->pixels, 0, bufSize);

            // Load the raw image data.
            F_Read(image->pixels, fileLength, file);
            /// \fixme not all flats are 64x64!
            image->width = FLAT_WIDTH;
            image->height = FLAT_HEIGHT;
            image->pixelSize = 1;
            result = 1;

#undef FLAT_HEIGHT
#undef FLAT_WIDTH
        }
        F_Close(file);
    }
    return result;
    }
}

byte GL_LoadSpriteLump(image_t* image, const texture_t* tex, int tclass,
    int tmap, int border)
{
    assert(image && tex);
    {
    const doompatch_header_t* patch;
    const spritetex_t* sprTex;
    lumpnum_t lumpNum;

    sprTex = R_SpriteTextureByIndex(Texture_TypeIndex(tex));
    assert(NULL != sprTex);

    lumpNum = W_GetNumForName(sprTex->name);
    patch = W_CacheLumpNum(lumpNum, PU_APPSTATIC);
    GL_InitImage(image);
    image->pixelSize = 1;
    image->width  = sprTex->width  + border*2;
    image->height = sprTex->height + border*2;
    image->pixels = calloc(1, 2 * image->width * image->height);

    if(loadDoomPatch(image->pixels, image->width, image->height, patch,
        border, border, tclass, tmap, false, true))
        image->flags |= IMGF_IS_MASKED;

    W_ChangeCacheTag(lumpNum, PU_CACHE);
    return 1;
    }
}

byte GL_LoadDoomPatchLump(image_t* image, const texture_t* tex, boolean scaleSharp)
{
    assert(image && tex);
    {
    int tclass = 0, tmap = 0, border = (scaleSharp? 1 : 0);
    const doompatch_header_t* patch;
    lumpnum_t lumpNum;
    patchtex_t* p;

    p = R_PatchTextureByIndex(Texture_TypeIndex(tex));
    assert(NULL != p);
    lumpNum = p->lump;

    patch = W_CacheLumpNum(lumpNum, PU_APPSTATIC);
    GL_InitImage(image);
    image->pixelSize = 1;
    image->width  = SHORT(p->width)  + border*2;
    image->height = SHORT(p->height) + border*2;
    image->pixels = calloc(1, 2 * image->width * image->height);

    if(loadDoomPatch(image->pixels, image->width, image->height, patch,
        border, border, tclass, tmap, false, true))
        image->flags |= IMGF_IS_MASKED;

    W_ChangeCacheTag(lumpNum, PU_CACHE);
    return 1;
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

/**
 * Renders the given texture into the buffer.
 */
static boolean bufferTexture(const patchcompositetex_t* texDef, uint8_t* buffer,
 int width, int height, int* hasBigPatch)
{
    boolean hasAlpha = false;

    // Clear the buffer.
    memset(buffer, 0, 2 * width * height); // Pal8 plus 8-bit alpha.

    // By default zero is put in the big patch height.
    if(hasBigPatch)
        *hasBigPatch = 0;

    // Draw all the patches. Check for alpha pixels after last patch has
    // been drawn.
    { int i;
    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t* patchDef = &texDef->patches[i];
        const doompatch_header_t* patch =
            (doompatch_header_t*) W_CacheLumpNum(patchDef->lump, PU_CACHE);

        // Check for big patches?
        if(hasBigPatch && SHORT(patch->height) > texDef->height &&
           *hasBigPatch < SHORT(patch->height))
        {
            *hasBigPatch = SHORT(patch->height);
        }

        // Draw the patch in the buffer.
        hasAlpha = loadDoomPatch(buffer, /*palette,*/ width, height, patch,
            patchDef->offX, patchDef->offY, 0, 0, false, i == texDef->patchCount - 1);
    }}

    return hasAlpha;
}

/**
 * Draws the given sky texture in a buffer. The returned buffer must be
 * freed by the caller. Idx must be a valid texture number.
 */
static void bufferSkyTexture(const patchcompositetex_t* texDef, byte** outbuffer,
                             int width, int height, boolean zeroMask)
{
    int                 i, numpels;
    byte*               imgdata;

    if(texDef->patchCount > 1)
    {
        numpels = width * height;
        imgdata = M_Calloc(2 * numpels);

        for(i = 0; i < texDef->patchCount; ++i)
        {
            const texpatch_t*   patchDef = &texDef->patches[i];

            loadDoomPatch(imgdata, /*palette,*/ width, height,
                          W_CacheLumpNum(patchDef->lump, PU_CACHE),
                          patchDef->offX, patchDef->offY, 0, 0,
                          zeroMask, false);
        }
    }
    else
    {
        doompatch_header_t *patch =
            (doompatch_header_t*) W_CacheLumpNum(texDef->patches[0].lump, PU_CACHE);
        int     bufHeight =
            SHORT(patch->height) > height ? SHORT(patch->height) : height;

        if(bufHeight > height)
        {
            // Heretic sky textures are reported to be 128 tall, even if the
            // data is 200. We'll adjust the real height of the texture up to
            // 200 pixels (remember Caldera?).
            height = bufHeight;
            if(height > 200)
                height = 200;
        }

        // Allocate a large enough buffer.
        numpels = width * bufHeight;
        imgdata = M_Calloc(2 * numpels);
        loadDoomPatch(imgdata, /*palette,*/ width, bufHeight, patch, 0, 0, 0, 0,
                      zeroMask, false);
    }

    *outbuffer = imgdata;
}

/**
 * @return              The outcome:
 *                      0 = not loaded.
 *                      1 = loaded data from a lump resource.
 *                      2 = loaded data from an external resource.
 */
byte GL_LoadDoomTexture(image_t* image, const texture_t* tex,
    boolean prepareForSkySphere, boolean zeroMask)
{
    assert(image && tex);
    {
    patchcompositetex_t* texDef = R_PatchCompositeTextureByIndex(Texture_TypeIndex(tex));
    assert(NULL != texDef);
    // Try to load a replacement version of this texture?
    if(!noHighResTex && (loadExtAlways || highResWithPWAD || Texture_IsFromIWAD(tex)))
    {
        ddstring_t searchPath, foundPath, suffix = { "-ck" };
        byte result = 0;

        Str_Init(&searchPath); Str_Appendf(&searchPath, TEXTURES_RESOURCE_NAMESPACE_NAME":%s;", texDef->name);
        Str_Init(&foundPath);

        if(F_FindResourceStr3(RC_GRAPHIC, &searchPath, &foundPath, &suffix) != 0 &&
           GL_LoadImage(image, Str_Text(&foundPath)))
        {
            result = 2; // High resolution texture loaded.
        }

        Str_Free(&searchPath);
        Str_Free(&foundPath);

        if(result != 0)
            return result;
    }

    // None found. Load the original version.
    GL_InitImage(image);
    image->width = texDef->width;
    image->height = texDef->height;
    image->pixelSize = 1;

    if(prepareForSkySphere)
    {
        bufferSkyTexture(texDef, &image->pixels, image->width, image->height, zeroMask);
        if(zeroMask)
            image->flags |= IMGF_IS_MASKED;
    }
    else
    {
        int hasBigPatch;
        boolean isMasked;

        /**
         * \todo if we are resizing masked textures re match patches then
         * we are needlessly duplicating work.
         */
        image->pixels = M_Malloc(2 * image->width * image->height);
        isMasked = bufferTexture(texDef, image->pixels, image->width, image->height, &hasBigPatch);

        // The -bigmtex option allows the engine to resize masked
        // textures whose patches are too big to fit the texture.
        if(allowMaskedTexEnlarge && isMasked && hasBigPatch)
        {
            // Adjust the defined height to fit the largest patch.
            texDef->height = image->height = hasBigPatch;
            // Get a new buffer.
            M_Free(image->pixels);
            image->pixels = M_Malloc(2 * image->width * image->height);
            isMasked = bufferTexture(texDef, image->pixels, image->width, image->height, 0);
        }

        if(isMasked)
            image->flags |= IMGF_IS_MASKED;
    }

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

/**
 * Raw images are always 320x200.
 *
 * 2003-05-30 (skyjake): External resources can be larger than 320x200,
 * but they're never split into two parts.
 */
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

        if((result = GL_LoadRawTex(&image, raw)) == 2)
        {   // Loaded an external raw texture.
            // We have the image in the buffer. We'll upload it as one big texture.
            raw->tex =
                GL_UploadTexture(image.pixels, image.width, image.height,
                                 image.pixelSize == 4, false, true, false, false,
                                 GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST),
                                 0 /*no anisotropy*/,
                                 GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 (image.pixelSize == 4? TXCF_APPLY_GAMMACORRECTION : 0));

            raw->width = 320;
            raw->height = 200;

            GL_DestroyImagePixels(&image);
        }
        else
        {
            boolean rgbdata = (image.pixelSize > 1? true:false);
            int assumedWidth = GL_state.features.texNonPowTwo? image.width : 256;

            // Generate a texture.
            raw->tex = GL_UploadTexture(image.pixels, GL_state.features.texNonPowTwo? image.width : 256, image.height,
                false, false, rgbdata, false, false, GL_NEAREST,
                (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                (rgbdata? TXCF_APPLY_GAMMACORRECTION : 0));

            raw->width = GL_state.features.texNonPowTwo? image.width : 256;
            raw->height = image.height;

            GL_DestroyImagePixels(&image);
        }
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
                GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_LIGHTMAP, 0, 0, 0, 0);
            const texturevariant_t* tex;
            if(NULL != (tex = GL_PrepareTexture(lmap->id, texSpec, 0)))
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
                GL_TextureVariantSpecificationForContext(TC_HALO_LUMINANCE, 0, 0, 0, 0);
            const texturevariant_t* tex;
            if(NULL != (tex = GL_PrepareTexture(fTex->id, texSpec, 0)))
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
                1, 0, 0);
        const texturevariant_t* tex;
        if(NULL != (tex = GL_PrepareTexture(patchTex->texId, texSpec, NULL)))
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
        *optWidth = width;
        *optHeight = height;
    }
    else if(noStretch)
    {
        *optWidth = M_CeilPow2(width);
        *optHeight = M_CeilPow2(height);

        // MaxTexSize may prevent using noStretch.
        if(*optWidth  > GL_state.maxTexSize ||
           *optHeight > GL_state.maxTexSize)
        {
            noStretch = false;
        }
    }
    else
    {
        // Determine the most favorable size for the texture.
        if(texQuality == TEXQ_BEST) // The best quality.
        {
            // At the best texture quality *opt, all textures are
            // sized *upwards*, so no details are lost. This takes
            // more memory, but naturally looks better.
            *optWidth = M_CeilPow2(width);
            *optHeight = M_CeilPow2(height);
        }
        else if(texQuality == 0)
        {
            // At the lowest quality, all textures are sized down to the
            // nearest power of 2.
            *optWidth = M_FloorPow2(width);
            *optHeight = M_FloorPow2(height);
        }
        else
        {
            // At the other quality *opts, a weighted rounding is used.
            *optWidth  = M_WeightPow2(width,  1 - texQuality / (float) TEXQ_BEST);
            *optHeight = M_WeightPow2(height, 1 - texQuality / (float) TEXQ_BEST);
        }
    }

    // Hardware limitations may force us to modify the preferred
    // texture size.
    if(*optWidth > GL_state.maxTexSize)
        *optWidth = GL_state.maxTexSize;
    if(*optHeight > GL_state.maxTexSize)
        *optHeight = GL_state.maxTexSize;

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

const texturevariant_t* GL_PrepareTexture(textureid_t id,
    texturevariantspecification_t* spec, byte* result)
{
    texture_t* tex = getTexture(id);
    texturevariant_t* variant = NULL;
    boolean variantIsNew = false;
    byte loadResult;

    // Have we already registered and prepared a suitable variant?
    variant = GL_ChooseTextureVariant(tex, spec);
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
    switch(spec->type)
    {
    case TS_NORMAL:     loadResult = prepareVariant(variant); break;
    case TS_DETAIL:     loadResult = prepareDetailVariant(variant); break;
    }

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
    return getTexture(id);
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

int BytesPerPixelFmt(dgltexformat_t format)
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
    if(other->format == DGL_LUMINANCE && (other->flags & TXCF_CONVERT_8BIT_TO_ALPHA))
        bytesPerPixel = 2; // We'll need a larger buffer.
    else
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

void GL_UploadTextureContent(const texturecontent_t* content)
{
    boolean result = false;

    if(content->flags & TXCF_EASY_UPLOAD)
    {
        GL_UploadTexture2(content);
    }
    else
    {
        dgltexformat_t loadFormat = content->format;
        const uint8_t* loadPixels = content->pixels;

        // The texture name must already be created.
        glBindTexture(GL_TEXTURE_2D, content->name);

        if(content->flags & TXCF_NO_COMPRESSION)
        {
            GL_SetTextureCompression(false);
        }

        if((content->flags & TXCF_CONVERT_8BIT_TO_ALPHA) &&
           (loadFormat == DGL_LUMINANCE || loadFormat == DGL_COLOR_INDEX_8))
        {
            long numPixels = content->width * content->height;
            uint8_t* localBuffer;

            if(NULL == (localBuffer = malloc(2 * numPixels)))
                Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for "
                          "luminance conversion buffer.", (unsigned long) (2 * numPixels));

            // Move the average color to the alpha channel, make the
            // actual color white.
            memcpy(localBuffer + numPixels, loadPixels, numPixels);
            memset(localBuffer, 255, numPixels);

            loadFormat = DGL_LUMINANCE_PLUS_A8;
            loadPixels = localBuffer;
        }

        if(!(content->flags & TXCF_GRAY_MIPMAP))
        {
            DGLuint palid;

            // Do we need to locate a color palette?
            if(loadFormat == DGL_COLOR_INDEX_8 || loadFormat == DGL_COLOR_INDEX_8_PLUS_A8)
                palid = R_GetColorPalette(content->palette);
            else
                palid = 0;

            if(!GL_TexImage(loadFormat, loadPixels, content->width, content->height,
                    palid, (content->flags & TXCF_MIPMAP) != 0))
                Con_Error("GL_UploadTextureContent: TexImage failed "
                          "(%u:%ix%i fmt%i).", content->name, content->width,
                          content->height, (int) loadFormat);
        }
        else
        {   // Special fade-to-gray luminance texture (used for details).
            if(!(loadFormat == DGL_LUMINANCE || loadFormat == DGL_RGB))
                Con_Error("GL_UploadTextureContent: Cannot upload format %i with gray mipmap.",
                    (int) loadFormat);

            if(!GL_TexImageGrayMipmap(loadPixels, content->width, content->height,
                    (loadFormat == DGL_LUMINANCE? 1 : 3), content->grayMipmap * reciprocal255))
                Con_Error("GL_UploadTextureContent: TexImageGrayMipmap failed "
                          "(%u:%ix%i fmt%i).", content->name, content->width,
                          content->height, (int) loadFormat);
        }

        if(content->flags & TXCF_NO_COMPRESSION)
        {
            GL_SetTextureCompression(true);
        }

        if(loadPixels != content->pixels)
            free((uint8_t*)loadPixels);
    }

    glBindTexture(GL_TEXTURE_2D, content->name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content->minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content->magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, content->wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, content->wrap[1]);
    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        GL_GetTexAnisoMul(content->anisoFilter));

    assert(!Sys_GLCheckError());
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
