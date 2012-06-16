/**\file gl_texmanager.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * GL-Texture management routines.
 *
 * This file needs to be split into smaller portions.
 */

#include <stdlib.h>
#include <ctype.h>

#ifdef UNIX
#   include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_play.h"
#include "de_ui.h"

#include "def_main.h"
#include "p_particle.h"

#include "colorpalette.h"
#include "image.h"
#include "texturecontent.h"
#include "texturevariant.h"

typedef enum {
    METHOD_IMMEDIATE = 0,
    METHOD_DEFERRED
} uploadcontentmethod_t;

typedef struct {
    const char* name; // Format/handler name.
    const char* ext; // Expected file extension.
    boolean (*loadFunc)(image_t* img, DFile* file);
    const char* (*getLastErrorFunc) (void); // Can be NULL.
} imagehandler_t;

typedef struct texturevariantspecificationlist_node_s {
    struct texturevariantspecificationlist_node_s* next;
    texturevariantspecification_t* spec;
} texturevariantspecificationlist_node_t;

typedef texturevariantspecificationlist_node_t variantspecificationlist_t;

D_CMD(LowRes);
D_CMD(MipMap);
D_CMD(TexReset);

void GL_DoResetDetailTextures(void);
void GL_DoTexReset(void);
void GL_DoUpdateTexGamma(void);
void GL_DoUpdateTexParams(void);

static int hashDetailVariantSpecification(const detailvariantspecification_t* spec);

static void calcGammaTable(void);

static boolean tryLoadPCX(image_t* img, DFile* file);
static boolean tryLoadPNG(image_t* img, DFile* file);
static boolean tryLoadJPG(image_t* img, DFile* file);
static boolean tryLoadTGA(image_t* img, DFile* file);

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

static boolean initedOk = false; // Init done.

// Image file handlers.
static const imagehandler_t handlers[] = {
    { "PNG",    "png",      tryLoadPNG, 0 },
    { "JPG",    "jpg",      tryLoadJPG, 0 }, // TODO: add alternate "jpeg" extension
    { "TGA",    "tga",      tryLoadTGA, TGA_LastError },
    { "PCX",    "pcx",      tryLoadPCX, PCX_LastError },
    { 0 } // Terminate.
};

static variantspecificationlist_t* variantSpecs;

/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)
static variantspecificationlist_t* detailVariantSpecs[DETAILVARIANT_CONTRAST_HASHSIZE];

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
    C_CMD_FLAGS("texreset", "", TexReset, CMDF_NO_DEDICATED);

    Textures_Register();
}

static __inline GLint glMinFilterForVariantSpec(const variantspecification_t* spec)
{
    assert(spec);
    if(spec->minFilter >= 0) // Constant logical value.
    {
        return (spec->mipmapped? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST) + spec->minFilter;
    }
    // "No class" preference.
    return spec->mipmapped? glmode[mipmapping] : GL_LINEAR;
}

static __inline GLint glMagFilterForVariantSpec(const variantspecification_t* spec)
{
    assert(spec);
    if(spec->magFilter >= 0) // Constant logical value.
    {
        return GL_NEAREST + spec->magFilter;
    }
    // Preference for texture class id.
    switch(abs(spec->magFilter)-1)
    {
    case 1: // Sprite class.
        return filterSprites? GL_LINEAR : GL_NEAREST;
    case 2: // UI class.
        return filterUI? GL_LINEAR : GL_NEAREST;
    default: // "No class" preference.
        return glmode[texMagMode];
    }
}

static __inline int logicalAnisoLevelForVariantSpec(const variantspecification_t* spec)
{
    assert(spec);
    return spec->anisoFilter < 0? texAniso : spec->anisoFilter;
}

static texturevariantspecification_t* unlinkVariantSpecification(texturevariantspecification_t* spec)
{
    variantspecificationlist_t** listHead;
    assert(initedOk && spec);

    // Select list head according to variant specification type.
    switch(spec->type)
    {
    case TST_GENERAL:   listHead = &variantSpecs; break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(spec));
        listHead = &detailVariantSpecs[hash];
        break;
      }
    default:
        Con_Error("unlinkVariantSpecification: Invalid spec type %i.", spec->type);
        exit(1); // Unreachable.
    }

    if(*listHead)
    {
        texturevariantspecificationlist_node_t* node = NULL;
        if((*listHead)->spec == spec)
        {
            node = (*listHead);
            *listHead = (*listHead)->next;
        }
        else
        {
            // Find the previous node.
            texturevariantspecificationlist_node_t* prevNode = (*listHead);
            while(prevNode->next && prevNode->next->spec != spec)
            {
                prevNode = prevNode->next;
            }
            if(prevNode)
            {
                node = prevNode->next;
                prevNode->next = prevNode->next->next;
            }
        }
        free(node);
    }

    return spec;
}

static void destroyVariantSpecification(texturevariantspecification_t* spec)
{
    assert(spec);
    unlinkVariantSpecification(spec);
    if(spec->type == TST_GENERAL && (TS_GENERAL(spec)->flags & TSF_HAS_COLORPALETTE_XLAT))
        free(TS_GENERAL(spec)->translated);
    free(spec);
}

static texturevariantspecification_t* copyVariantSpecification(
    const texturevariantspecification_t* tpl)
{
    texturevariantspecification_t* spec = (texturevariantspecification_t*) malloc(sizeof *spec);
    if(!spec)
        Con_Error("Textures::copyVariantSpecification: Failed on allocation of %lu bytes for new TextureVariantSpecification.", (unsigned long) sizeof *spec);

    memcpy(spec, tpl, sizeof(texturevariantspecification_t));
    if(TS_GENERAL(tpl)->flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        colorpalettetranslationspecification_t* cpt = (colorpalettetranslationspecification_t*) malloc(sizeof *cpt);
        if(!cpt)
            Con_Error("Textures::copyVariantSpecification: Failed on allocation of %lu bytes for new ColorPaletteTranslationSpecification.", (unsigned long) sizeof *cpt);

        memcpy(cpt, TS_GENERAL(tpl)->translated, sizeof(colorpalettetranslationspecification_t));
        TS_GENERAL(spec)->translated = cpt;
    }
    return spec;
}

static texturevariantspecification_t* copyDetailVariantSpecification(
    const texturevariantspecification_t* tpl)
{
    texturevariantspecification_t* spec = (texturevariantspecification_t*) malloc(sizeof *spec);
    if(!spec)
        Con_Error("Textures::copyDetailVariantSpecification: Failed on allocation of %lu bytes for new TextureVariantSpecification.", (unsigned long) sizeof *spec);
    memcpy(spec, tpl, sizeof(texturevariantspecification_t));
    return spec;
}

/**
 * @todo Magnification, Anisotropic filter level and GL texture wrap modes
 * will be handled through dynamic changes to GL's texture environment state.
 * Consequently they should be ignored here.
 */
static int compareVariantSpecifications(const variantspecification_t* a,
    const variantspecification_t* b)
{
    /// @todo We can be a bit cleverer here...
    if(a->context != b->context) return 0;
    if(a->flags != b->flags) return 0;
    if(a->wrapS != b->wrapS || a->wrapT != b->wrapT) return 0;
    //if(a->magFilter != b->magFilter) return 0;
    //if(a->anisoFilter != b->anisoFilter) return 0;
    if(a->mipmapped != b->mipmapped) return 0;
    if(a->noStretch != b->noStretch) return 0;
    if(a->gammaCorrection != b->gammaCorrection) return 0;
    if(a->toAlpha != b->toAlpha) return 0;
    if(a->border != b->border) return 0;
    if(a->flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        const colorpalettetranslationspecification_t* cptA = a->translated;
        const colorpalettetranslationspecification_t* cptB = b->translated;
        assert(cptA && cptB);
        if(cptA->tClass != cptB->tClass) return 0;
        if(cptA->tMap != cptB->tMap) return 0;
    }
    return 1; // Equal.
}

static int compareDetailVariantSpecifications(const detailvariantspecification_t* a,
    const detailvariantspecification_t* b)
{
    if(a->contrast != b->contrast) return 0;
    return 1; // Equal.
}

static colorpalettetranslationspecification_t* applyColorPaletteTranslationSpecification(
    colorpalettetranslationspecification_t* spec, int tClass, int tMap)
{
    assert(initedOk && spec);

    spec->tClass = MAX_OF(0, tClass);
    spec->tMap   = MAX_OF(0, tMap);

#if _DEBUG
    if(0 == tClass && 0 == tMap)
        Con_Message("Warning:applyColorPaletteTranslationSpecification: Applied unnecessary zero-translation (tClass:0 tMap:0).\n");
#endif

    return spec;
}

static variantspecification_t* applyVariantSpecification(
    variantspecification_t* spec, texturevariantusagecontext_t tc, int flags,
    byte border, colorpalettetranslationspecification_t* colorPaletteTranslationSpec,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter, boolean mipmapped,
    boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    assert(initedOk && spec && (tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc)));

    flags &= ~TSF_INTERNAL_MASK;

    spec->context = tc;
    spec->flags = flags;
    spec->border = (flags & TSF_UPSCALE_AND_SHARPEN)? 1 : border;
    spec->mipmapped = mipmapped;
    spec->wrapS = wrapS;
    spec->wrapT = wrapT;
    spec->minFilter = MINMAX_OF(-1, minFilter, spec->mipmapped? 3:1);
    spec->magFilter = MINMAX_OF(-3, magFilter, 1);
    spec->anisoFilter = MINMAX_OF(-1, anisoFilter, 4);
    spec->gammaCorrection = gammaCorrection;
    spec->noStretch = noStretch;
    spec->toAlpha = toAlpha;
    if(colorPaletteTranslationSpec)
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
    const int quantFactor = DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR;
    assert(spec);

    spec->contrast = 255 * (int)MINMAX_OF(0, contrast * quantFactor + .5f, quantFactor) * (1/(float)quantFactor);
    return spec;
}

static int hashDetailVariantSpecification(const detailvariantspecification_t* spec)
{
    assert(spec);
    return (int) (spec->contrast * (1/255.f) * DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR + .5f);
}

static texturevariantspecification_t* linkVariantSpecification(
    texturevariantspecificationtype_t type, texturevariantspecification_t* spec)
{
    texturevariantspecificationlist_node_t* node;
    assert(initedOk && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type) && spec);

    node = (texturevariantspecificationlist_node_t*) malloc(sizeof *node);
    if(!node)
        Con_Error("Textures::linkVariantSpecification: Failed on allocation of %lu bytes for new TextureVariantSpecificationListNode.", (unsigned long) sizeof *node);

    node->spec = spec;
    switch(type)
    {
    case TST_GENERAL:
        node->next = variantSpecs;
        variantSpecs = (variantspecificationlist_t*)node;
        break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(spec));
        node->next = detailVariantSpecs[hash];
        detailVariantSpecs[hash] = (variantspecificationlist_t*)node;
        break;
      }
    }
    return spec;
}

static texturevariantspecification_t* findVariantSpecification(
    texturevariantspecificationtype_t type, const texturevariantspecification_t* tpl,
    boolean canCreate)
{
    texturevariantspecificationlist_node_t* node;
    assert(initedOk && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type) && tpl);

    // Select list head according to variant specification type.
    switch(type)
    {
    case TST_GENERAL:   node = variantSpecs; break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(tpl));
        node = detailVariantSpecs[hash];
        break;
      }
    default:
        Con_Error("findVariantSpecification: Invalid spec type %i.", type);
        exit(1); // Unreachable.
    }

    // Do we already have a concrete version of the template specification?
    for(; node; node = node->next)
    {
        if(GL_CompareTextureVariantSpecifications(node->spec, tpl))
            return node->spec;
    }

    // Not found, can we create?
    if(canCreate)
    switch(type)
    {
    case TST_GENERAL:   return linkVariantSpecification(type, copyVariantSpecification(tpl));
    case TST_DETAIL:    return linkVariantSpecification(type, copyDetailVariantSpecification(tpl));
    }

    return NULL;
}

static texturevariantspecification_t* getVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    static texturevariantspecification_t tpl;
    static colorpalettetranslationspecification_t cptTpl;
    boolean haveCpt = false;
    assert(initedOk);

    tpl.type = TST_GENERAL;
    if(0 != tClass || 0 != tMap)
    {   // A color palette translation spec is required.
        applyColorPaletteTranslationSpecification(&cptTpl, tClass, tMap);
        haveCpt = true;
    }

    applyVariantSpecification(TS_GENERAL(&tpl), tc, flags, border, haveCpt? &cptTpl : NULL,
        wrapS, wrapT, minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection,
        noStretch, toAlpha);

    // Retrieve a concrete version of the rationalized specification.
    return findVariantSpecification(tpl.type, &tpl, true);
}

static texturevariantspecification_t* getDetailVariantSpecificationForContext(
    float contrast)
{
    static texturevariantspecification_t tpl;
    assert(initedOk);

    tpl.type = TST_DETAIL;
    applyDetailVariantSpecification(TS_DETAIL(&tpl), contrast);
    return findVariantSpecification(tpl.type, &tpl, true);
}

static void emptyVariantSpecificationList(variantspecificationlist_t* list)
{
    texturevariantspecificationlist_node_t* node = (texturevariantspecificationlist_node_t*) list;
    assert(initedOk);

    while(node)
    {
        texturevariantspecificationlist_node_t* next = node->next;
        destroyVariantSpecification(node->spec);
        node = next;
    }
}

static int compareTextureVariantWithVariantSpecification(TextureVariant* tex, void* paramaters)
{
    texturevariantspecification_t* spec = (texturevariantspecification_t*)paramaters;
    return (TextureVariant_Spec(tex) == spec? 1 : 0);
}

static int findTextureUsingVariantSpecificationWorker(Texture* tex, void* paramaters)
{
    if(Texture_IterateVariants(tex, compareTextureVariantWithVariantSpecification, paramaters))
    {
        return Textures_Id(tex);
    }
    return 0; // Continue iteration.
}

static int pruneUnusedVariantSpecificationsInList(variantspecificationlist_t* list)
{
    texturevariantspecificationlist_node_t* node = list;
    int numPruned = 0;
    while(node)
    {
        texturevariantspecificationlist_node_t* next = node->next;
        if(!Textures_Iterate2(TN_ANY, findTextureUsingVariantSpecificationWorker, (void*)node->spec))
        {
            destroyVariantSpecification(node->spec);
            ++numPruned;
        }
        node = next;
    }
    return numPruned;
}

static int pruneUnusedVariantSpecifications(texturevariantspecificationtype_t specType)
{
    assert(initedOk);
    switch(specType)
    {
    case TST_GENERAL: return pruneUnusedVariantSpecificationsInList(variantSpecs);
    case TST_DETAIL: {
        int i, numPruned = 0;
        for(i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            numPruned += pruneUnusedVariantSpecificationsInList(detailVariantSpecs[i]);
        }
        return numPruned;
      }
    default:
        Con_Error("Textures::pruneUnusedVariantSpecifications: Invalid variant spec type %i.", (int) specType);
        exit(1); // Unreachable.
    }
}

static void destroyVariantSpecifications(void)
{
    int i;
    assert(initedOk);

    emptyVariantSpecificationList(variantSpecs); variantSpecs = NULL;
    for(i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
    {
        emptyVariantSpecificationList(detailVariantSpecs[i]); detailVariantSpecs[i] = NULL;
    }
}

static uploadcontentmethod_t chooseContentUploadMethod(const texturecontent_t* content)
{
    // Must the operation be carried out immediately?
    if((content->flags & TXCF_NEVER_DEFER) || !Con_IsBusy())
        return METHOD_IMMEDIATE;
    // We can defer.
    return METHOD_DEFERRED;
}

typedef enum {
    METHOD_MATCH = 0,
    METHOD_FUZZY
} choosevariantmethod_t;

typedef struct {
    choosevariantmethod_t method;
    const texturevariantspecification_t* spec;
    TextureVariant* chosen;
} choosevariantworker_paramaters_t;

static int chooseVariantWorker(TextureVariant* variant, void* context)
{
    choosevariantworker_paramaters_t* p = (choosevariantworker_paramaters_t*) context;
    const texturevariantspecification_t* cand = TextureVariant_Spec(variant);
    assert(p);

    switch(p->method)
    {
    case METHOD_MATCH:
        if(cand == p->spec)
        {
            // This is the one we're looking for.
            p->chosen = variant;
            return 1; // Stop iteration.
        }
        break;
    case METHOD_FUZZY:
        if(GL_CompareTextureVariantSpecifications(cand, p->spec))
        {
            // This will do fine.
            p->chosen = variant;
            return 1; // Stop iteration.
        }
        break;
    }
    return 0; // Continue iteration.
}

static TextureVariant* chooseVariant(choosevariantmethod_t method, Texture* tex,
    const texturevariantspecification_t* spec)
{
    choosevariantworker_paramaters_t params;
    assert(initedOk && tex && spec);

    params.method = method;
    params.spec = spec;
    params.chosen = NULL;
    Texture_IterateVariants(tex, chooseVariantWorker, &params);
    return params.chosen;
}

static int releaseVariantGLTexture(TextureVariant* variant, void* paramaters)
{
    texturevariantspecification_t* spec = (texturevariantspecification_t*)paramaters;
    if(!spec || spec == TextureVariant_Spec(variant))
    {
        if(TextureVariant_IsUploaded(variant))
        {
            // Delete and mark it not-loaded.
            DGLuint glName = TextureVariant_GLName(variant);
            glDeleteTextures(1, (const GLuint*) &glName);
            TextureVariant_SetGLName(variant, 0);
            TextureVariant_FlagUploaded(variant, false);
        }

        if(spec) return true; // We're done.
    }
    return 0; // Continue iteration.
}

static void uploadContent(uploadcontentmethod_t uploadMethod, const texturecontent_t* content)
{
    assert(content);
    if(METHOD_IMMEDIATE == uploadMethod)
    {   // Do this right away. No need to take a copy.
        GL_UploadTextureContent(content);
        return;
    }
    GL_DeferTextureUpload(content);
}

static uploadcontentmethod_t uploadContentForVariant(uploadcontentmethod_t uploadMethod,
    const texturecontent_t* content, TextureVariant* variant)
{
    assert(content && variant);
    if(!novideo)
    {
        uploadContent(uploadMethod, content);
    }
    TextureVariant_FlagUploaded(variant, true);
    return uploadMethod;
}

static void uploadContentUnmanaged(uploadcontentmethod_t uploadMethod,
    const texturecontent_t* content)
{
    assert(content);
    if(novideo) return;
    if(METHOD_IMMEDIATE == uploadMethod)
    {
#ifdef _DEBUG
        VERBOSE( Con_Message("Uploading unmanaged texture (%i:%ix%i) while not busy! Should be "
            "precached in busy mode?\n", content->name, content->width, content->height) )
#endif
    }
    uploadContent(uploadMethod, content);
}

static TexSource loadSourceImage(Texture* tex, const texturevariantspecification_t* baseSpec,
    image_t* image)
{
    const variantspecification_t* spec;
    TexSource source = TEXS_NONE;
    assert(tex && baseSpec && image);

    spec = TS_GENERAL(baseSpec);
    switch(Textures_Namespace(Textures_Id(tex)))
    {
    case TN_FLATS:
        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !Texture_IsCustom(tex)))
        {
            const ddstring_t suffix = { "-ck" };
            ddstring_t* path = Textures_ComposePath(Textures_Id(tex));
            ddstring_t searchPath;

            // First try the flats namespace then the old-fashioned "flat-name"
            // in the textures namespace.
            Str_Init(&searchPath);
            Str_Appendf(&searchPath,
                FLATS_RESOURCE_NAMESPACE_NAME":%s;"
                TEXTURES_RESOURCE_NAMESPACE_NAME":flat-%s;", Str_Text(path), Str_Text(path));

            source = GL_LoadExtTextureEX(image, Str_Text(&searchPath), Str_Text(&suffix), true/*quiet please*/);
            Str_Free(&searchPath);
            Str_Delete(path);
        }
        if(source == TEXS_NONE)
        {
            const Uri* resourcePath = Textures_ResourcePath(Textures_Id(tex));
            lumpnum_t lumpNum = -1;
            if(!Str_CompareIgnoreCase(Uri_Scheme(resourcePath), "Lumps"))
            {
                lumpNum = F_CheckLumpNumForName2(Str_Text(Uri_Path(resourcePath)), true/*quiet please*/);
            }
            else if(!Str_CompareIgnoreCase(Uri_Scheme(resourcePath), "LumpDir"))
            {
                lumpNum = strtol(Str_Text(Uri_Path(resourcePath)), NULL, 0);
            }

            if(F_IsValidLumpNum(lumpNum))
            {
                DFile* file = F_OpenLump(lumpNum);
                source = GL_LoadFlatLump(image, file);
                F_Delete(file);
            }
        }
        break;

    case TN_PATCHES: {
        int tclass = 0, tmap = 0;
        if(spec->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            assert(spec->translated);
            tclass = spec->translated->tClass;
            tmap   = spec->translated->tMap;
        }

        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !Texture_IsCustom(tex)))
        {
            const ddstring_t suffix = { "-ck" };
            ddstring_t* path = Textures_ComposePath(Textures_Id(tex));
            ddstring_t searchPath;

            Str_Init(&searchPath);
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", Str_Text(path));

            source = GL_LoadExtTextureEX(image, Str_Text(&searchPath), Str_Text(&suffix), true/*quiet please*/);
            Str_Free(&searchPath);
            Str_Delete(path);
        }
        if(source == TEXS_NONE)
        {
            const Uri* resourcePath = Textures_ResourcePath(Textures_Id(tex));
            if(!Str_CompareIgnoreCase(Uri_Scheme(resourcePath), "Lumps"))
            {
                lumpnum_t lumpNum = F_CheckLumpNumForName2(Str_Text(Uri_Path(resourcePath)), true/*quiet please*/);
                if(F_IsValidLumpNum(lumpNum))
                {
                    DFile* file = F_OpenLump(lumpNum);
                    source = GL_LoadPatchLumpAsPatch(image, file, tclass, tmap, spec->border, tex);
                    F_Delete(file);
                }
            }
        }
        break;
      }
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
            ddstring_t searchPath, suffix = { "-ck" };
            ddstring_t* path = Textures_ComposePath(Textures_Id(tex));

            // Prefer psprite or translated versions if available.
            Str_Init(&searchPath);
            if(TC_PSPRITE_DIFFUSE == spec->context)
            {
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-hud;", Str_Text(path));
            }
            else if(tclass || tmap)
            {
                Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s-table%i%i;",
                    Str_Text(path), tclass, tmap);
            }
            Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s", Str_Text(path));

            source = GL_LoadExtTextureEX(image, Str_Text(&searchPath), Str_Text(&suffix), true/*quiet please*/);
            Str_Free(&searchPath);
            Str_Delete(path);
        }
        if(source == TEXS_NONE)
        {
            const Uri* resourcePath = Textures_ResourcePath(Textures_Id(tex));
            if(!Str_CompareIgnoreCase(Uri_Scheme(resourcePath), "Lumps"))
            {
                lumpnum_t lumpNum = F_CheckLumpNumForName2(Str_Text(Uri_Path(resourcePath)), true/*quiet please*/);
                if(F_IsValidLumpNum(lumpNum))
                {
                    DFile* file = F_OpenLump(lumpNum);
                    source = GL_LoadPatchLumpAsPatch(image, file, tclass, tmap, spec->border, tex);
                    F_Delete(file);
                }
            }
        }
        break;
      }
    case TN_DETAILS: {
        const Uri* resourcePath = Textures_ResourcePath(Textures_Id(tex));
        if(Str_CompareIgnoreCase(Uri_Scheme(resourcePath), "Lumps"))
        {
            ddstring_t* searchPath = Uri_Compose(resourcePath);
            source = GL_LoadExtTextureEX(image, Str_Text(searchPath), NULL, true/*quiet please*/);
            Str_Delete(searchPath);
        }
        else
        {
            lumpnum_t lumpNum = F_CheckLumpNumForName2(Str_Text(Uri_Path(resourcePath)), true/*quiet please*/);
            if(lumpNum >= 0)
            {
                DFile* file = F_OpenLump(lumpNum);
                source = GL_LoadDetailTextureLump(image, file);
                F_Delete(file);
            }
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
        const Uri* resourcePath = Textures_ResourcePath(Textures_Id(tex));
        ddstring_t* path = Uri_Compose(resourcePath);
        source = GL_LoadExtTextureEX(image, Str_Text(path), NULL, true/*quiet please*/);
        Str_Delete(path);
        break;
      }
    default:
        Con_Error("Textures::loadSourceImage: Unknown texture namespace %i.", (int) Textures_Namespace(Textures_Id(tex)));
        exit(1); // Unreachable.
    }
    return source;
}

static uploadcontentmethod_t prepareVariantFromImage(TextureVariant* tex, image_t* image)
{
    const variantspecification_t* spec = TS_GENERAL(TextureVariant_Spec(tex));
    boolean monochrome    = (spec->flags & TSF_MONOCHROME) != 0;
    boolean noCompression = (spec->flags & TSF_NO_COMPRESSION) != 0;
    boolean scaleSharp    = (spec->flags & TSF_UPSCALE_AND_SHARPEN) != 0;
    int wrapS = spec->wrapS, wrapT = spec->wrapT;
    int magFilter, minFilter, anisoFilter, flags = 0;
    boolean noSmartFilter = false;
    dgltexformat_t dglFormat;
    texturecontent_t c;
    float s, t;
    assert(image);

    if(spec->toAlpha)
    {
        if(0 != image->paletteId)
        {   // Paletted.
            uint8_t* newPixels = GL_ConvertBuffer(image->pixels, image->size.width, image->size.height,
                ((image->flags & IMGF_IS_MASKED)? 2 : 1), R_ToColorPalette(image->paletteId), 3);
            free(image->pixels);
            image->pixels = newPixels;
            image->pixelSize = 3;
            image->paletteId = 0;
            image->flags &= ~IMGF_IS_MASKED;
        }

        Image_ConvertToLuminance(image, false);
        { long i, total = image->size.width * image->size.height;
        for(i = 0; i < total; ++i)
        {
            image->pixels[total + i] = image->pixels[i];
            image->pixels[i] = 255;
        }}
        image->pixelSize = 2;
    }
    else if(0 != image->paletteId)
    {
        if(fillOutlines && (image->flags & IMGF_IS_MASKED))
            ColorOutlinesIdx(image->pixels, image->size.width, image->size.height);

        if(monochrome && !scaleSharp)
            GL_DeSaturatePalettedImage(image->pixels, R_ToColorPalette(image->paletteId), image->size.width, image->size.height);

        if(scaleSharp)
        {
            int scaleMethod = GL_ChooseSmartFilter(image->size.width, image->size.height, 0);
            boolean origMasked = (image->flags & IMGF_IS_MASKED) != 0;
            colorpaletteid_t origPaletteId = image->paletteId;
            uint8_t* newPixels;

            newPixels = GL_ConvertBuffer(image->pixels, image->size.width, image->size.height,
                ((image->flags & IMGF_IS_MASKED)? 2 : 1), R_ToColorPalette(image->paletteId), 4);
            if(newPixels != image->pixels)
            {
                free(image->pixels);
                image->pixels = newPixels;
                image->pixelSize = 4;
                image->paletteId = 0;
                image->flags &= ~IMGF_IS_MASKED;
            }

            if(monochrome)
                Desaturate(image->pixels, image->size.width, image->size.height, image->pixelSize);

            newPixels = GL_SmartFilter(scaleMethod, image->pixels, image->size.width, image->size.height,
                0, &image->size.width, &image->size.height);
            if(newPixels != image->pixels)
            {
                free(image->pixels);
                image->pixels = newPixels;
            }

            EnhanceContrast(image->pixels, image->size.width, image->size.height, image->pixelSize);
            //SharpenPixels(image->pixels, image->size.width, image->size.height, image->pixelSize);
            //BlackOutlines(image->pixels, image->size.width, image->size.height, image->pixelSize);

            // Back to paletted+alpha?
            if(monochrome)
            {
                // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                Image_ConvertToLuminance(image, true);
                AmplifyLuma(image->pixels, image->size.width, image->size.height, image->pixelSize == 2);
            }
            else
            {   // Yes. Quantize down from RGA(+A) to Paletted(+A), replacing the old image->
                newPixels = GL_ConvertBuffer(image->pixels, image->size.width, image->size.height,
                    (origMasked? 2 : 1), R_ToColorPalette(origPaletteId), 4);
                if(newPixels != image->pixels)
                {
                    free(image->pixels);
                    image->pixels = newPixels;
                    image->pixelSize = (origMasked? 2 : 1);
                    image->paletteId = origPaletteId;
                    if(origMasked)
                        image->flags |= IMGF_IS_MASKED;
                }
            }

            // Lets not do this again.
            noSmartFilter = true;
        }
    }
    else if(image->pixelSize > 2)
    {
        if(monochrome)
        {
            Image_ConvertToLuminance(image, true);
            AmplifyLuma(image->pixels, image->size.width, image->size.height, image->pixelSize == 2);
        }
    }

    if(noCompression || (image->size.width < 128 || image->size.height < 128))
        flags |= TXCF_NO_COMPRESSION;

    if(spec->gammaCorrection) flags |= TXCF_APPLY_GAMMACORRECTION;
    if(spec->noStretch)       flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
    if(spec->mipmapped)       flags |= TXCF_MIPMAP;
    if(noSmartFilter)         flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

    if(monochrome)
    {
        dglFormat = ( image->pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
    }
    else
    {
        if(0 != image->paletteId)
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

    minFilter = glMinFilterForVariantSpec(spec);
    magFilter = glMagFilterForVariantSpec(spec);
    anisoFilter = logicalAnisoLevelForVariantSpec(spec);

    /**
     * Calculate texture coordinates based on the image dimensions. The
     * coordinates are calculated as width/CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     *
     * @todo Image dimensions may not be the same as the final uploaded texture!
     */
    if((flags & TXCF_UPLOAD_ARG_NOSTRETCH) &&
       (!GL_state.features.texNonPowTwo || spec->mipmapped))
    {
        int pw = M_CeilPow2(image->size.width), ph = M_CeilPow2(image->size.height);
        s =  image->size.width / (float) pw;
        t = image->size.height / (float) ph;
    }
    else
    {
        s = t = 1;
    }

    TextureVariant_SetCoords(tex, s, t);
    TextureVariant_FlagMasked(tex, (image->flags & IMGF_IS_MASKED) != 0);

    GL_InitTextureContent(&c);
    c.name = TextureVariant_GLName(tex);
    c.format = dglFormat;
    c.width = image->size.width;
    c.height = image->size.height;
    c.pixels = image->pixels;
    c.paletteId = image->paletteId;
    c.flags = flags;
    c.magFilter = magFilter;
    c.minFilter = minFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;

    return uploadContentForVariant(chooseContentUploadMethod(&c), &c, tex);
}

static uploadcontentmethod_t prepareDetailVariantFromImage(TextureVariant* tex, image_t* image)
{
    const detailvariantspecification_t* spec = TS_DETAIL(TextureVariant_Spec(tex));
    float baMul, hiMul, loMul, s, t;
    int grayMipmapFactor, pw, ph, flags = 0;
    texturecontent_t c;
    assert(image);

    grayMipmapFactor = spec->contrast;

    // We only want a luminance map.
    if(image->pixelSize > 2)
    {
        Image_ConvertToLuminance(image, false);
    }

    // Try to normalize the luminance data so it works expectedly as a detail texture.
    EqualizeLuma(image->pixels, image->size.width, image->size.height, &baMul, &hiMul, &loMul);
    if(baMul != 1 || hiMul != 1 || loMul != 1)
    {
        // Integrate the normalization factor with contrast.
        const float hiContrast = 1 - 1. / hiMul;
        const float loContrast = 1 - loMul;
        const float shift = ((hiContrast + loContrast) / 2);
        grayMipmapFactor = (uint8_t)(255 * MINMAX_OF(0, spec->contrast / 255.f - shift, 1));

        // Announce the normalization.
        VERBOSE(
            Uri* uri = Textures_ComposeUri(Textures_Id(TextureVariant_GeneralCase(tex)));
            ddstring_t* path = Uri_ToString(uri);
            Con_Message("Normalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).\n",
                        Str_Text(path), baMul, hiMul, loMul);
            Str_Delete(path);
            Uri_Delete(uri);
        )
    }

    // Disable compression?
    if(image->size.width < 128 || image->size.height < 128)
        flags |= TXCF_NO_COMPRESSION;

    // Calculate prepared texture coordinates.
    pw = M_CeilPow2(image->size.width);
    ph = M_CeilPow2(image->size.height);
    s =  image->size.width / (float) pw;
    t = image->size.height / (float) ph;
    TextureVariant_SetCoords(tex, s, t);

    GL_InitTextureContent(&c);
    c.name = TextureVariant_GLName(tex);
    c.format = DGL_LUMINANCE;
    c.flags = flags | TXCF_GRAY_MIPMAP | TXCF_UPLOAD_ARG_NOSMARTFILTER;
    c.grayMipmap = grayMipmapFactor;
    c.width = image->size.width;
    c.height = image->size.height;
    c.pixels = image->pixels;
    c.anisoFilter = texAniso;
    c.magFilter = glmode[texMagMode];
    c.minFilter = GL_LINEAR_MIPMAP_LINEAR;
    c.wrap[0] = GL_REPEAT;
    c.wrap[1] = GL_REPEAT;

    return uploadContentForVariant(chooseContentUploadMethod(&c), &c, tex);
}

void GL_EarlyInitTextureManager(void)
{
    GL_InitSmartFilterHQ2x();
    calcGammaTable();

    variantSpecs = NULL;
    memset(detailVariantSpecs, 0, sizeof(detailVariantSpecs));

    Textures_Init();
}

void GL_InitTextureManager(void)
{
    if(initedOk)
    {
        GL_LoadSystemTextures();
        return; // Already been here.
    }

    // Disable the use of 'high resolution' textures and/or patches?
    noHighResTex = CommandLine_Exists("-nohightex");
    noHighResPatches = CommandLine_Exists("-nohighpat");

    // Should we allow using external resources with PWAD textures?
    highResWithPWAD = CommandLine_Exists("-pwadtex");

    // System textures loaded in GL_LoadSystemTextures.
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));
    memset(lightingTextures, 0, sizeof(lightingTextures));

    // Initialization done.
    initedOk = true;
}

void GL_ResetTextureManager(void)
{
    if(!initedOk) return;
    GL_ReleaseTextures();
    GL_PruneTextureVariantSpecifications();
    GL_LoadSystemTextures();
}

int GL_CompareTextureVariantSpecifications(const texturevariantspecification_t* a,
    const texturevariantspecification_t* b)
{
    assert(a && b);
    if(a == b) return 1;
    if(a->type != b->type) return 0;
    switch(a->type)
    {
    case TST_GENERAL: return compareVariantSpecifications(TS_GENERAL(a), TS_GENERAL(b));
    case TST_DETAIL:  return compareDetailVariantSpecifications(TS_DETAIL(a), TS_DETAIL(b));
    }
    Con_Error("GL_CompareTextureVariantSpecifications: Invalid type %i.", (int) a->type);
    exit(1); // Unreachable.
}

void GL_PrintTextureVariantSpecification(const texturevariantspecification_t* baseSpec)
{
    static const char* textureUsageContextNames[1+TEXTUREVARIANTUSAGECONTEXT_COUNT] = {
        /* TC_UNKNOWN */                    "unknown",
        /* TC_UI */                         "ui",
        /* TC_MAPSURFACE_DIFFUSE */         "mapsurface_diffuse",
        /* TC_MAPSURFACE_REFLECTION */      "mapsurface_reflection",
        /* TC_MAPSURFACE_REFLECTIONMASK */  "mapsurface_reflectionmask",
        /* TC_MAPSURFACE_LIGHTMAP */        "mapsurface_lightmap",
        /* TC_SPRITE_DIFFUSE */             "sprite_diffuse",
        /* TC_MODELSKIN_DIFFUSE */          "modelskin_diffuse",
        /* TC_MODELSKIN_REFLECTION */       "modelskin_reflection",
        /* TC_HALO_LUMINANCE */             "halo_luminance",
        /* TC_PSPRITE_DIFFUSE */            "psprite_diffuse",
        /* TC_SKYSPHERE_DIFFUSE */          "skysphere_diffuse"
    };
    static const char* textureSpecificationTypeNames[TEXTUREVARIANTSPECIFICATIONTYPE_COUNT] = {
        /* TST_GENERAL */   "general",
        /* TST_DETAIL */    "detail"
    };
    static const char* filterModeNames[] = { "ui", "sprite", "noclass", "const" };
    static const char* glFilterNames[] = {
        "nearest", "linear", "nearest_mipmap_nearest", "linear_mipmap_nearest",
        "nearest_mipmap_linear", "linear_mipmap_linear"
    };

    if(!baseSpec) return;

    Con_Printf("type:%s", textureSpecificationTypeNames[baseSpec->type]);

    switch(baseSpec->type)
    {
    case TST_DETAIL: {
        const detailvariantspecification_t* spec = TS_DETAIL(baseSpec);
        Con_Printf(" contrast:%i%%\n", (int)(.5f + spec->contrast / 255.f * 100));
        break;
      }
    case TST_GENERAL: {
        const variantspecification_t* spec = TS_GENERAL(baseSpec);
        texturevariantusagecontext_t tc = spec->context;
        int glMinFilterNameIdx, glMagFilterNameIdx;
        assert(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

        if(spec->minFilter >= 0) // Constant logical value.
        {
            glMinFilterNameIdx = (spec->mipmapped? 2 : 0) + spec->minFilter;
        }
        else // "No class" preference.
        {
            glMinFilterNameIdx = spec->mipmapped? mipmapping : 1;
        }

        if(spec->magFilter >= 0) // Constant logical value.
        {
            glMagFilterNameIdx = spec->magFilter;
        }
        else
        {
            // Preference for texture class id.
            switch(abs(spec->magFilter)-1)
            {
            default: // "No class" preference.
                glMagFilterNameIdx = texMagMode; break;

            case 1: // "Sprite" class.
                glMagFilterNameIdx = filterSprites; break;

            case 2: // "UI" class.
                glMagFilterNameIdx = filterUI; break;
            }
        }

        Con_Printf(" context:%s flags:%i border:%i\n"
            "    minFilter:(%s|%s) magFilter:(%s|%s) anisoFilter:%i",
            textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1],
            (spec->flags & ~TSF_INTERNAL_MASK), spec->border,
            filterModeNames[3 + MINMAX_OF(-1, spec->minFilter, 0)],
            glFilterNames[glMinFilterNameIdx],
            filterModeNames[3 + MINMAX_OF(-3, spec->magFilter, 0)],
            glFilterNames[glMagFilterNameIdx],
            spec->anisoFilter);
        if(spec->flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            const colorpalettetranslationspecification_t* cpt = spec->translated;
            assert(cpt);
            Con_Printf(" translated:(tclass:%i tmap:%i)", cpt->tClass, cpt->tMap);
        }
        Con_Printf("\n");
        break;
      }
    }
}

texturevariantspecification_t* GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    if(!initedOk)
        Con_Error("GL_TextureVariantSpecificationForContext: GL texture manager not yet initialized.");
    return getVariantSpecificationForContext(tc, flags, border, tClass, tMap, wrapS,
        wrapT, minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection, noStretch, toAlpha);
}

texturevariantspecification_t* GL_DetailTextureVariantSpecificationForContext(
    float contrast)
{
    if(!initedOk)
        Con_Error("GL_DetailTextureVariantSpecificationForContext: GL texture manager not yet initialized.");
    return getDetailVariantSpecificationForContext(contrast);
}

void GL_ShutdownTextureManager(void)
{
    if(!initedOk) return;

    destroyVariantSpecifications();
    initedOk = false;
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
    if(novideo || !initedOk) return;

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
}

void GL_ReleaseSystemTextures(void)
{
    int i;
    if(novideo || !initedOk) return;

    VERBOSE( Con_Message("Releasing System textures...\n") )

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    for(i = 0; i < NUM_LIGHTING_TEXTURES; ++i)
    {
        glDeleteTextures(1, (const GLuint*) &lightingTextures[i].tex);
    }
    memset(lightingTextures, 0, sizeof(lightingTextures));

    for(i = 0; i < NUM_SYSFLARE_TEXTURES; ++i)
    {
        glDeleteTextures(1, (const GLuint*) &sysFlareTextures[i].tex);
    }
    memset(sysFlareTextures, 0, sizeof(sysFlareTextures));

    GL_ReleaseTexturesByNamespace(TN_SYSTEM);

    UI_ReleaseTextures();
    Rend_ParticleReleaseSystemTextures();
    Fonts_ReleaseSystemTextures();

    GL_PruneTextureVariantSpecifications();
}

void GL_ReleaseRuntimeTextures(void)
{
    if(novideo || !initedOk) return;

    VERBOSE( Con_Message("Releasing Runtime textures...\n") )

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    // texture-wrapped GL textures; textures, flats, sprites...
    GL_ReleaseTexturesByNamespace(TN_FLATS);
    GL_ReleaseTexturesByNamespace(TN_TEXTURES);
    GL_ReleaseTexturesByNamespace(TN_PATCHES);
    GL_ReleaseTexturesByNamespace(TN_SPRITES);
    GL_ReleaseTexturesByNamespace(TN_DETAILS);
    GL_ReleaseTexturesByNamespace(TN_REFLECTIONS);
    GL_ReleaseTexturesByNamespace(TN_MASKS);
    GL_ReleaseTexturesByNamespace(TN_MODELSKINS);
    GL_ReleaseTexturesByNamespace(TN_MODELREFLECTIONSKINS);
    GL_ReleaseTexturesByNamespace(TN_LIGHTMAPS);
    GL_ReleaseTexturesByNamespace(TN_FLAREMAPS);
    GL_ReleaseTexturesForRawImages();

    Rend_ParticleReleaseExtraTextures();
    Fonts_ReleaseRuntimeTextures();

    GL_PruneTextureVariantSpecifications();
}

void GL_ReleaseTextures(void)
{
    if(!initedOk) return;
    GL_ReleaseRuntimeTextures();
    GL_ReleaseSystemTextures();
}

void GL_PruneTextureVariantSpecifications(void)
{
    int numPruned = 0;
    if(!initedOk || Sys_IsShuttingDown()) return;

    numPruned += pruneUnusedVariantSpecifications(TST_GENERAL);
    numPruned += pruneUnusedVariantSpecifications(TST_DETAIL);
#if _DEBUG
    VERBOSE( Con_Message("Pruned %i unused texture variant %s.\n", numPruned, numPruned == 1? "specification" : "specifications") )
#endif
}

void GL_InitImage(image_t* img)
{
    assert(img);
    img->size.width = 0;
    img->size.height = 0;
    img->pixelSize = 0;
    img->flags = 0;
    img->paletteId = 0;
    img->pixels = 0;
}

void GL_PrintImageMetadata(const image_t* image)
{
    assert(image);
    Con_Printf("dimensions:[%ix%i] flags:%i %s:%i\n", image->size.width, image->size.height,
        image->flags, 0 != image->paletteId? "colorpalette" : "pixelsize",
        0 != image->paletteId? image->paletteId : image->pixelSize);
}

static boolean tryLoadPCX(image_t* img, DFile* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = PCX_Load(file, &img->size.width, &img->size.height, &img->pixelSize);
    return (0 != img->pixels);
}

static boolean tryLoadJPG(image_t* img, DFile* file)
{
    assert(img && file);
    return Image_LoadFromFileWithFormat(img, "JPG", file);
}

static boolean tryLoadPNG(image_t* img, DFile* file)
{
    assert(img && file);
    /*
    GL_InitImage(img);
    img->pixels = PNG_Load(file, &img->size.width, &img->size.height, &img->pixelSize);
    return (0 != img->pixels);
    */
    return Image_LoadFromFileWithFormat(img, "PNG", file);
}

static boolean tryLoadTGA(image_t* img, DFile* file)
{
    assert(img && file);
    GL_InitImage(img);
    img->pixels = TGA_Load(file, &img->size.width, &img->size.height, &img->pixelSize);
    return (0 != img->pixels);
}

const imagehandler_t* findHandlerFromFileName(const char* filePath)
{
    const imagehandler_t* hdlr = 0; // No handler for this format.
    const char* p = F_FindFileExtension(filePath);
    if(p)
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

/// @return  @c true if the given path name is formed with the "color keyed" suffix.
static boolean isColorKeyed(const char* path)
{
    boolean result = false;
    ddstring_t buf;
    if(path && path[0])
    {
        Str_Init(&buf);
        F_FileName(&buf, path);
        if(Str_Length(&buf) > 3)
        {
            result = (0 == stricmp(Str_Text(&buf) + Str_Length(&buf) - 3, "-ck"));
        }
        Str_Free(&buf);
    }
    return result;
}

uint8_t* Image_LoadFromFile(image_t* img, DFile* file)
{
    const imagehandler_t* hdlr;
    const char* fileName;
    assert(img && file);

    GL_InitImage(img);

    fileName = Str_Text(AbstractFile_Path(DFile_File_Const(file)));

    // Firstly try the expected format given the file name.
    hdlr = findHandlerFromFileName(fileName);
    if(hdlr)
    {
        hdlr->loadFunc(img, file);
    }

    if(!img->pixels)
    {
        // Try each recognisable format instead.
        /// \todo Order here should be determined by the resource locator.
        int i;
        for(i = 0; handlers[i].name && !img->pixels; ++i)
        {
            if(&handlers[i] == hdlr) continue; // We already know its not in this format.
            handlers[i].loadFunc(img, file);
        }
    }

    if(!img->pixels)
    {
        VERBOSE2( Con_Message("Image_LoadFromFile: \"%s\" unrecognized, trying fallback loader...\n",
                              F_PrettyPath(fileName)) )
        return NULL; // Not a recognised format. It may still be loadable, however.
    }

    // How about some color-keying?
    if(isColorKeyed(fileName))
    {
        uint8_t* out = ApplyColorKeying(img->pixels, img->size.width, img->size.height, img->pixelSize);
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
    if(Image_HasAlpha(img))
        img->flags |= IMGF_IS_MASKED;

    VERBOSE( Con_Message("Image_LoadFromFile: \"%s\" (%ix%i)\n",
                         F_PrettyPath(fileName), img->size.width, img->size.height) )

    return img->pixels;
}

uint8_t* GL_LoadImage(image_t* img, const char* filePath)
{
    DFile* file = F_Open(filePath, "rb");
    uint8_t* pixels = NULL;
    assert(img);

    if(file)
    {
        pixels = Image_LoadFromFile(img, file);
        F_Delete(file);
    }
    return pixels;
}

uint8_t* GL_LoadImageStr(image_t* img, const ddstring_t* filePath)
{
    if(!filePath)return NULL;
    return GL_LoadImage(img, Str_Text(filePath));
}

void GL_DestroyImage(image_t* img)
{
    assert(img);
    if(!img->pixels) return;
    free(img->pixels);
    img->pixels = NULL;
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
 * @pre Input data is of GL_UNSIGNED_BYTE type.
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

boolean GL_UploadTextureGrayMipmap(int glFormat, int loadFormat, const uint8_t* pixels,
    int width, int height, float grayFactor)
{
    int i, w, h, numpels = width * height, numLevels, pixelSize;
    uint8_t* image, *faded, *out;
    const uint8_t* in;
    float invFactor;
    assert(pixels);

    if(!(GL_RGB == loadFormat || GL_LUMINANCE == loadFormat))
        Con_Error("GL_UploadTextureGrayMipmap: Unsupported load format %i.", (int) loadFormat);

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
    for(i = 0; i < numpels; ++i)
    {
        *out++ = (uint8_t) MINMAX_OF(0, (*in * grayFactor + 127 * invFactor), 255);
        in += pixelSize;
    }

    // Upload the first level right away.
    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, (GLint)loadFormat,
        GL_UNSIGNED_BYTE, image);

    // Generate all mipmaps levels.
    w = width;
    h = height;
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
    }

    // Do we need to free the temp buffer?
    free(faded);
    free(image);

    assert(!Sys_GLCheckError());
    return true;
}

boolean GL_UploadTexture(int glFormat, int loadFormat, const uint8_t* pixels,
    int width,  int height, int genMipmaps)
{
    const int packRowLength = 0, packAlignment = 1, packSkipRows = 0, packSkipPixels = 0;
    const int unpackRowLength = 0, unpackAlignment = 1, unpackSkipRows = 0, unpackSkipPixels = 0;
    int mipLevel = 0;
    assert(pixels);

    if(!(GL_LUMINANCE_ALPHA == loadFormat || GL_LUMINANCE == loadFormat ||
         GL_RGB == loadFormat || GL_RGBA == loadFormat))
         Con_Error("GL_UploadTexture: Unsupported load format %i.", (int) loadFormat);

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

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

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
            Con_Error("GL_UploadTexture: Unknown GL format %i.\n", (int) loadFormat);

        GL_OptimalTextureSize(width, height, false, true, &w, &h);

        if(w != width || h != height)
        {
            // Must rescale image to get "top" mipmap texture image.
            image = GL_ScaleBufferEx(pixels, width, height, bpp, /*GL_UNSIGNED_BYTE,*/
                unpackRowLength, unpackAlignment, unpackSkipRows, unpackSkipPixels,
                w, h, /*GL_UNSIGNED_BYTE,*/ packRowLength, packAlignment, packSkipRows,
                packSkipPixels);
            if(NULL == image)
                Con_Error("GL_UploadTexture: Unknown error resizing mipmap level #0.");
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
            if(!newimage)
                Con_Error("GL_UploadTexture: Unknown error resizing mipmap level #%i.", mipLevel);

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

/// @note Texture parameters will NOT be set here!
void GL_UploadTextureContent(const texturecontent_t* content)
{
    assert(content);
    {
    boolean generateMipmaps = ((content->flags & (TXCF_MIPMAP|TXCF_GRAY_MIPMAP)) != 0);
    boolean applyTexGamma   = ((content->flags & TXCF_APPLY_GAMMACORRECTION) != 0);
    boolean noCompression   = ((content->flags & TXCF_NO_COMPRESSION) != 0);
    boolean noSmartFilter   = ((content->flags & TXCF_UPLOAD_ARG_NOSMARTFILTER) != 0);
    boolean noStretch       = ((content->flags & TXCF_UPLOAD_ARG_NOSTRETCH) != 0);
    int loadWidth = content->width, loadHeight = content->height;
    const uint8_t* loadPixels = content->pixels;
    dgltexformat_t dglFormat = content->format;

    if(DGL_COLOR_INDEX_8 == dglFormat || DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat)
    {
        // Convert a paletted source image to truecolor.
        uint8_t* newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight,
            DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 2 : 1, R_ToColorPalette(content->paletteId),
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
            long i, numPels = loadWidth * loadHeight;
            const uint8_t* src;

            src = loadPixels;
            if(loadPixels == content->pixels)
            {
                localBuffer = (uint8_t*) malloc(comps * numPels);
                if(!localBuffer)
                    Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for"
                              "tex-gamma translation buffer.", (unsigned long) (comps * numPels));
                dst = localBuffer;
            }
            else
            {
                dst = (uint8_t*)loadPixels;
            }

            for(i = 0; i < numPels; ++i)
            {
                dst[CR] = gammaTable[src[CR]];
                dst[CG] = gammaTable[src[CG]];
                dst[CB] = gammaTable[src[CB]];
                if(comps == 4)
                    dst[CA] = src[CA];
                dst += comps;
                src += comps;
            }

            if(localBuffer)
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
            {
                // Need to add an alpha channel.
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
    {
        // Needs converting. This adds some overhead.
        long i, numPixels = content->width * content->height;
        uint8_t* pixel, *localBuffer;

        localBuffer = (uint8_t*) malloc(2 * numPixels);
        if(!localBuffer)
            Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for luminance conversion buffer.", (unsigned long) (2 * numPixels));

        pixel = localBuffer;
        for(i = 0; i < numPixels; ++i)
        {
            pixel[0] = loadPixels[i];
            pixel[1] = loadPixels[numPixels + i];
            pixel += 2;
        }

        if(loadPixels != content->pixels)
            free((uint8_t*)loadPixels);
        loadPixels = localBuffer;
    }

    if(DGL_LUMINANCE == dglFormat && (content->flags & TXCF_CONVERT_8BIT_TO_ALPHA))
    {
        // Needs converting. This adds some overhead.
        long i, numPixels = content->width * content->height;
        uint8_t* pixel, *localBuffer;

        localBuffer = (uint8_t*) malloc(2 * numPixels);
        if(!localBuffer)
            Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for luminance conversion buffer.", (unsigned long) (2 * numPixels));

        // Move the average color to the alpha channel, make the actual color white.
        pixel = localBuffer;
        for(i = 0; i < numPixels; ++i)
        {
            pixel[0] = 255;
            pixel[1] = loadPixels[i];
            pixel += 2;
        }

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
        {
            // Copy the texture into a power-of-two canvas.
            uint8_t* localBuffer = (uint8_t*) calloc(1, comps * loadWidth * loadHeight);
            int i;

            if(!localBuffer)
                Con_Error("GL_UploadTextureContent: Failed on allocation of %lu bytes for upscale buffer.", (unsigned long) (comps * loadWidth * loadHeight));

            // Copy line by line.
            for(i = 0; i < height; ++i)
            {
                memcpy(localBuffer + loadWidth * comps * i,
                       loadPixels  + width     * comps * i, comps * width);
            }

            if(loadPixels != content->pixels)
                free((uint8_t*)loadPixels);
            loadPixels = localBuffer;
        }
        else
        {
            // Stretch into a new power-of-two texture.
            uint8_t* newPixels = GL_ScaleBuffer(loadPixels, width, height, comps, loadWidth, loadHeight);
            if(loadPixels != content->pixels)
                free((uint8_t*)loadPixels);
            loadPixels = newPixels;
        }
    }
    }

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, content->name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content->minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content->magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, content->wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, content->wrap[1]);
    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(content->anisoFilter));

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
            exit(1);
        }

        glFormat = ChooseTextureFormat(dglFormat, !noCompression);

        if(!GL_UploadTexture(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                             generateMipmaps ? true : false))
        {
            Con_Error("GL_UploadTextureContent: TexImage failed (%u:%ix%i fmt%i).",
                      content->name, loadWidth, loadHeight, (int) dglFormat);
        }
    }
    else
    {
        // Special fade-to-gray luminance texture (used for details).
        GLint glFormat, loadFormat;

        switch(dglFormat)
        {
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB; break;
        default:
            Con_Error("GL_UploadTextureContent: Unknown format %i.", (int) dglFormat);
            exit(1); // Unreachable.
        }

        glFormat = ChooseTextureFormat(DGL_LUMINANCE, !noCompression);

        if(!GL_UploadTextureGrayMipmap(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                                       content->grayMipmap * reciprocal255))
        {
            Con_Error("GL_UploadTextureContent: TexImageGrayMipmap failed (%u:%ix%i fmt%i).",
                      content->name, loadWidth, loadHeight, (int) dglFormat);
        }
    }

    if(loadPixels != content->pixels)
        free((uint8_t*)loadPixels);
    }
}

TexSource GL_LoadExtTextureEX(image_t* image, const char* searchPath,
    const char* optionalSuffix, boolean silent)
{
    ddstring_t foundPath;
    assert(image && searchPath);

    Str_Init(&foundPath);
    if(!F_FindResource4(RC_GRAPHIC, searchPath, &foundPath, RLF_DEFAULT, optionalSuffix))
    {
        Str_Free(&foundPath);
        if(!silent) Con_Message("GL_LoadExtTextureEX: Warning, failed to locate \"%s\"\n", searchPath);
        return TEXS_NONE;
    }

    if(GL_LoadImage(image, Str_Text(&foundPath)))
    {
        Str_Free(&foundPath);
        return TEXS_EXTERNAL;
    }
    Str_Free(&foundPath);
    if(!silent) Con_Message("GL_LoadExtTextureEX: Warning, failed to load \"%s\"\n", F_PrettyPath(searchPath));
    return TEXS_NONE;
}

DGLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    static const struct lstex_s {
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

    if(novideo || which < 0 || which >= NUM_LIGHTING_TEXTURES) return 0;

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
    if(novideo || flare < 0 || flare >= NUM_SYSFLARE_TEXTURES) return 0;

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

TexSource GL_LoadExtTexture(image_t* image, const char* name, gfxmode_t mode)
{
    TexSource source = TEXS_NONE;
    ddstring_t foundPath;

    Str_Init(&foundPath);
    if(F_FindResource2(RC_GRAPHIC, name, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        // Force it to grayscale?
        if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
        {
            Image_ConvertToAlpha(image, mode == LGM_WHITE_ALPHA);
        }
        else if(mode == LGM_GRAYSCALE)
        {
            Image_ConvertToLuminance(image, true);
        }
        source = TEXS_EXTERNAL;
    }
    Str_Free(&foundPath);
    return source;
}

// Posts are runs of non masked source pixels.
typedef struct {
    byte topOffset; // @c 0xff is the last post in a column.
    byte length;
    // Length palette indices follow.
} post_t;

// column_t is a list of 0 or more post_t, (uint8_t)-1 terminated
typedef post_t column_t;

static boolean validPatch(const uint8_t* buffer, size_t len)
{
    const doompatch_header_t* hdr = (const doompatch_header_t*)buffer;
    const int32_t* colOffsets;
    int width, height, col;
    int32_t offset;

    if(len <= sizeof *hdr) return false;

    width  = SHORT(hdr->width);
    height = SHORT(hdr->height);
    if(width <= 0 || height <= 0 || len <= sizeof *hdr + width * sizeof(int32_t)) return false;

    // Validate column map.
    // Column offsets begin immediately following the header.
    colOffsets = (const int32_t*)(buffer + sizeof *hdr);
    for(col = 0; col < width; ++col)
    {
        offset = LONG(colOffsets[col]);
        if(offset < 0 || (unsigned)offset >= len) return false;
    }
    return true;
}

/**
 * @note The buffer must have room for the new alpha data!
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
    int count, col = 0, trans = -1;
    int w, x = origx, y, top; // Keep track of pos (clipping).
    size_t bufsize = texwidth * texheight;
    const column_t* column;
    const uint8_t* source;
    uint8_t* dest1, *dest2, *destTop, *destAlphaTop;
    // Column offsets begin immediately following the header.
    const int32_t* columnOfs = (const int32_t*)((const uint8_t*) patch + sizeof(doompatch_header_t));
    // \todo Validate column offset is within the Patch!
    assert(buffer && texwidth > 0 && texheight > 0 && patch);

    w = SHORT(patch->width);
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
        while(column->topOffset != 0xff)
        {
            source = (uint8_t*) column + 3;

            if(x < 0 || x >= texwidth)
                break; // Out of bounds.

            if(column->topOffset <= top)
                top += column->topOffset;
            else
                top = column->topOffset;

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
                        /// @todo Check bounds!
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

static boolean palettedIsMasked(const uint8_t* pixels, int width, int height)
{
    int i;
    assert(pixels);
    // Jump to the start of the alpha data.
    pixels += width * height;
    for(i = 0; i < width * height; ++i)
    {
        if(255 != pixels[i]) return true;
    }
    return false;
}

TexSource GL_LoadDetailTextureLump(image_t* image, DFile* file)
{
    TexSource source = TEXS_NONE;
    assert(image && file);

    if(Image_LoadFromFile(image, file))
    {
        source = TEXS_ORIGINAL;
    }
    else
    {   // It must be an old-fashioned "raw" image.
        size_t bufSize, fileLength = DFile_Length(file);

        GL_InitImage(image);

        /**
         * @todo Do not fatal error here if the not a known format!
         * Perform this check much earlier, when the definitions are
         * read and mark which are valid.
         */

        // How big is it?
        switch(fileLength)
        {
        case 256 * 256: image->size.width = image->size.height = 256; break;
        case 128 * 128: image->size.width = image->size.height = 128; break;
        case  64 *  64: image->size.width = image->size.height =  64; break;
        default:
            Con_Error("GL_LoadDetailTextureLump: Must be 256x256, 128x128 or 64x64.\n");
            exit(1); // Unreachable.
        }

        image->pixelSize = 1;
        bufSize = (size_t)image->size.width * image->size.height;
        image->pixels = (uint8_t*) malloc(bufSize);
        if(!image->pixels)
            Con_Error("GL_LoadDetailTextureLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) bufSize);
        if(fileLength < bufSize)
            memset(image->pixels, 0, bufSize);

        // Load the raw image data.
        DFile_Read(file, image->pixels, fileLength);
        source = TEXS_ORIGINAL;
    }
    return source;
}

TexSource GL_LoadFlatLump(image_t* image, DFile* file)
{
    TexSource source = TEXS_NONE;
    assert(image && file);

    if(Image_LoadFromFile(image, file))
    {
        source = TEXS_EXTERNAL;
    }
    else
    {
        // A DOOM flat.
#define FLAT_WIDTH          64
#define FLAT_HEIGHT         64

        size_t bufSize, fileLength = DFile_Length(file);

        GL_InitImage(image);

        /// @todo not all flats are 64x64!
        image->size.width  = FLAT_WIDTH;
        image->size.height = FLAT_HEIGHT;
        image->pixelSize = 1;
        image->paletteId = defaultColorPalette;

        bufSize = MAX_OF(fileLength, (size_t)image->size.width * image->size.height);
        image->pixels = (uint8_t*) malloc(bufSize);
        if(!image->pixels)
            Con_Error("GL_LoadFlatLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) bufSize);
        if(fileLength < bufSize)
            memset(image->pixels, 0, bufSize);

        // Load the raw image data.
        DFile_Read(file, image->pixels, fileLength);
        source = TEXS_ORIGINAL;

#undef FLAT_HEIGHT
#undef FLAT_WIDTH
    }
    return source;
}

static TexSource loadPatchLump(image_t* image, DFile* file, int tclass, int tmap, int border)
{
    TexSource source = TEXS_NONE;
    assert(image && file);

    if(Image_LoadFromFile(image, file))
    {
        source = TEXS_EXTERNAL;
    }
    else
    {
        // A DOOM patch.
        size_t fileLength = DFile_Length(file);
        if(fileLength > sizeof(doompatch_header_t))
        {
            const doompatch_header_t* patch =
                (const doompatch_header_t*) F_CacheLump(DFile_File(file), 0, PU_APPSTATIC);

            if(validPatch((const uint8_t*)patch, fileLength))
            {
                GL_InitImage(image);
                image->size.width  = SHORT(patch->width)  + border*2;
                image->size.height = SHORT(patch->height) + border*2;
                image->pixelSize = 1;
                image->paletteId = defaultColorPalette;
                image->pixels = (uint8_t*) calloc(1, 2 * image->size.width * image->size.height);
                if(!image->pixels)
                    Con_Error("GL_LoadPatchLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) (2 * image->size.width * image->size.height));

                loadDoomPatch(image->pixels, image->size.width, image->size.height, patch,
                    border, border, tclass, tmap, false);
                if(palettedIsMasked(image->pixels, image->size.width, image->size.height))
                    image->flags |= IMGF_IS_MASKED;
                source = TEXS_ORIGINAL;
            }
            F_CacheChangeTag(DFile_File(file), 0, PU_CACHE);
        }

        if(source == TEXS_NONE)
        {
            Con_Message("Warning: Lump \"%s\" does not appear to be a valid Patch.\n", F_PrettyPath(Str_Text(AbstractFile_Path(DFile_File(file)))));
            return source;
        }
    }
    return source;
}

TexSource GL_LoadPatchLumpAsPatch(image_t* image, DFile* file, int tclass, int tmap, int border,
    Texture* tex)
{
    TexSource source = loadPatchLump(image, file, tclass, tmap, border);
    if(source == TEXS_ORIGINAL && tex)
    {
        // Loaded from a lump assumed to be in DOOM's Patch format.
        patchtex_t* pTex = (patchtex_t*)Texture_UserData(tex);
        if(pTex)
        {
            // Load the extended metadata from the lump.
            doompatch_header_t hdr;
            F_ReadLumpSection(DFile_File(file), 0, (uint8_t*)&hdr, 0, sizeof(hdr));
            pTex->offX = -SHORT(hdr.leftOffset);
            pTex->offY = -SHORT(hdr.topOffset);
        }
    }
    return source;
}

DGLuint GL_PrepareExtTexture(const char* name, gfxmode_t mode, int useMipmap,
    int minFilter, int magFilter, int anisoFilter, int wrapS, int wrapT, int otherFlags)
{
    image_t image;
    DGLuint texture = 0;

    if(GL_LoadExtTexture(&image, name, mode))
    {
        // Loaded successfully and converted accordingly.
        // Upload the image to GL.
        texture = GL_NewTextureWithParams2(
            ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
              image.pixelSize == 3 ? DGL_RGB :
              image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
            image.size.width, image.size.height, image.pixels,
            ( otherFlags | (useMipmap? TXCF_MIPMAP : 0) |
              (useMipmap == DDMAXINT? TXCF_GRAY_MIPMAP : 0) |
              (image.size.width < 128 && image.size.height < 128? TXCF_NO_COMPRESSION : 0) ),
            0, (useMipmap ? glmode[mipmapping] : GL_LINEAR),
            magFilter, texAniso, wrapS, wrapT);

        GL_DestroyImage(&image);
    }

    return texture;
}

TexSource GL_LoadPatchComposite(image_t* image, Texture* tex)
{
    patchcompositetex_t* texDef;
    int i;
    assert(image && tex);

#if _DEBUG
    if(Textures_Namespace(Textures_Id(tex)) != TN_TEXTURES)
        Con_Error("GL_LoadPatchComposite: Internal error, texture [%p id:%i] is not a PatchCompositeTex!", (void*)tex, Textures_Id(tex));
#endif
    texDef = (patchcompositetex_t*)Texture_UserData(tex);
    assert(texDef);

    GL_InitImage(image);
    image->pixelSize = 1;
    image->size.width = texDef->size.width;
    image->size.height = texDef->size.height;
    image->paletteId = defaultColorPalette;

    image->pixels = (uint8_t*) calloc(1, 2 * image->size.width * image->size.height);
    if(!image->pixels)
        Con_Error("GL_LoadPatchComposite: Failed on allocation of %lu bytes for new Image pixel data.", (unsigned long) (2 * image->size.width * image->size.height));

    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t* patchDef = &texDef->patches[i];
        int lumpIdx;
        abstractfile_t* fsObject = F_FindFileForLumpNum2(patchDef->lumpNum, &lumpIdx);
        const uint8_t* patch = F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);

        if(validPatch(patch, F_LumpInfo(fsObject, lumpIdx)->size))
        {
            // Draw the patch in the buffer.
            loadDoomPatch(image->pixels, image->size.width, image->size.height,
                (const doompatch_header_t*)patch, patchDef->offX, patchDef->offY, 0, 0, false);
        }
        F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);
    }

    if(palettedIsMasked(image->pixels, image->size.width, image->size.height))
        image->flags |= IMGF_IS_MASKED;

    return TEXS_ORIGINAL;
}

TexSource GL_LoadPatchCompositeAsSky(image_t* image, Texture* tex, boolean zeroMask)
{
    patchcompositetex_t* texDef;
    int i, width, height, offX, offY;
    assert(image && tex);

#if _DEBUG
    if(Textures_Namespace(Textures_Id(tex)) != TN_TEXTURES)
        Con_Error("GL_LoadPatchCompositeAsSky: Internal error, texture [%p id:%i] is not a PatchCompositeTex!", (void*)tex, Textures_Id(tex));
#endif
    texDef = (patchcompositetex_t*)Texture_UserData(tex);
    assert(texDef);

    /**
     * Heretic sky textures are reported to be 128 tall, despite the patch
     * data is 200. We'll adjust the real height of the texture up to
     * 200 pixels (remember Caldera?).
     */
    width  = texDef->size.width;
    height = texDef->size.height;
    if(texDef->patchCount == 1)
    {
        int lumpIdx;
        abstractfile_t* fsObject = F_FindFileForLumpNum2(texDef->patches[0].lumpNum, &lumpIdx);
        const doompatch_header_t* hdr = (const doompatch_header_t*) F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);
        int bufHeight = SHORT(hdr->height) > height ? SHORT(hdr->height) : height;
        if(bufHeight > height)
        {
            height = bufHeight;
            if(height > 200)
                height = 200;
        }
        F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);
    }

    GL_InitImage(image);
    image->pixelSize = 1;
    image->size.width  = width;
    image->size.height = height;
    image->paletteId = defaultColorPalette;

    image->pixels = (uint8_t*) calloc(1, 2 * image->size.width * image->size.height);
    if(!image->pixels)
        Con_Error("GL_LoadPatchCompositeAsSky: Failed on allocation of %lu bytes for new Image pixel data.", (unsigned long) (2 * image->size.width * image->size.height));

    for(i = 0; i < texDef->patchCount; ++i)
    {
        const texpatch_t* patchDef = &texDef->patches[i];
        int lumpIdx;
        abstractfile_t* fsObject = F_FindFileForLumpNum2(patchDef->lumpNum, &lumpIdx);
        const doompatch_header_t* patch = (const doompatch_header_t*) F_CacheLump(fsObject, lumpIdx, PU_APPSTATIC);

        if(validPatch((const uint8_t*)patch, F_LumpInfo(fsObject, lumpIdx)->size))
        {
            if(texDef->patchCount != 1)
            {
                offX = patchDef->offX;
                offY = patchDef->offY;
            }
            else
            {
                offX = offY = 0;
            }
            loadDoomPatch(image->pixels, image->size.width, image->size.height, patch, offX, offY, 0, 0, zeroMask);
        }
        F_CacheChangeTag(fsObject, lumpIdx, PU_CACHE);
    }

    if(zeroMask)
        image->flags |= IMGF_IS_MASKED;

    return TEXS_ORIGINAL;
}

TexSource GL_LoadRawTex(image_t* image, const rawtex_t* r)
{
    ddstring_t searchPath, foundPath;
    TexSource source = TEXS_NONE;

    assert(image);

    // First try to find an external resource.
    Str_Init(&searchPath); Str_Appendf(&searchPath, PATCHES_RESOURCE_NAMESPACE_NAME":%s;", Str_Text(&r->name));
    Str_Init(&foundPath);

    if(F_FindResourceStr2(RC_GRAPHIC, &searchPath, &foundPath) != 0 &&
       GL_LoadImage(image, Str_Text(&foundPath)))
    {
        // "External" image loaded.
        source = TEXS_EXTERNAL;
    }
    else if(r->lumpNum >= 0)
    {
        DFile* file = F_OpenLump(r->lumpNum);
        if(file)
        {
            if(Image_LoadFromFile(image, file))
            {
                source = TEXS_ORIGINAL;
            }
            else
            {   // It must be an old-fashioned "raw" image.
#define RAW_WIDTH           320
#define RAW_HEIGHT          200

                size_t fileLength = DFile_Length(file);
                size_t bufSize = 3 * RAW_WIDTH * RAW_HEIGHT;

                GL_InitImage(image);
                image->pixels = malloc(bufSize);
                if(fileLength < bufSize)
                    memset(image->pixels, 0, bufSize);

                // Load the raw image data.
                DFile_Read(file, image->pixels, fileLength);
                image->size.width = RAW_WIDTH;
                image->size.height = (int) (fileLength / image->size.width);
                image->pixelSize = 1;
                source = TEXS_ORIGINAL;

#undef RAW_HEIGHT
#undef RAW_WIDTH
            }
            F_Delete(file);
        }
    }

    Str_Free(&searchPath);
    Str_Free(&foundPath);

    return source;
}

DGLuint GL_PrepareRawTexture(rawtex_t* raw)
{
    if(!raw) return 0;
    if(raw->lumpNum < 0 || raw->lumpNum >= F_LumpCount()) return 0;

    if(!raw->tex)
    {
        image_t image;

        // Clear any old values.
        memset(&image, 0, sizeof(image));

        if(GL_LoadRawTex(&image, raw) == TEXS_EXTERNAL)
        {
            // Loaded an external raw texture.
            raw->tex = GL_NewTextureWithParams2(image.pixelSize == 4? DGL_RGBA : DGL_RGB,
                image.size.width, image.size.height, image.pixels, 0, 0,
                GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }
        else
        {
            raw->tex = GL_NewTextureWithParams2(
                (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 :
                          image.pixelSize == 4? DGL_RGBA :
                          image.pixelSize == 3? DGL_RGB : DGL_COLOR_INDEX_8,
                image.size.width, image.size.height, image.pixels, 0, 0,
                GL_NEAREST, (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        }

        raw->width  = image.size.width;
        raw->height = image.size.height;
        GL_DestroyImage(&image);
    }

    return raw->tex;
}

DGLuint GL_PrepareLightMap(const Uri* filePath)
{
    if(filePath)
    {
        Texture* tex;
        if(!Str_CompareIgnoreCase(Uri_Path(filePath), "-")) return 0;

        tex = R_FindLightMapForResourcePath(filePath);
        if(tex)
        {
            texturevariantspecification_t* texSpec =
                GL_TextureVariantSpecificationForContext(TC_MAPSURFACE_LIGHTMAP,
                    0, 0, 0, 0, GL_CLAMP, GL_CLAMP, 1, -1, -1, false, false, false, true);
            return GL_PrepareTexture(tex, texSpec);
        }
    }
    // Return the default texture name.
    return GL_PrepareLSTexture(LST_DYNAMIC);
}

DGLuint GL_PrepareFlareTexture(const Uri* uri, int oldIdx)
{
    if(uri)
    {
        const ddstring_t* path = Uri_Path(uri);
        Texture* tex;

        if(Str_At(path, 0) == '-' || (Str_At(path, 0) == '0' && !Str_At(path, 1)))
            return 0; // Use the automatic selection logic.

        if(Str_At(path, 0) >= '1' && Str_At(path, 0) <= '4' && !Str_At(path, 1))
            return GL_PrepareSysFlareTexture(Str_At(path, 0) - '1');

        tex = R_FindFlareTextureForResourcePath(uri);
        if(tex)
        {
            texturevariantspecification_t* texSpec =
                GL_TextureVariantSpecificationForContext(TC_HALO_LUMINANCE,
                    TSF_NO_COMPRESSION, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                    1, 1, 0, false, false, false, true);
            return GL_PrepareTexture(tex, texSpec);
        }
    }
    else if(oldIdx > 0 && oldIdx < NUM_SYSFLARE_TEXTURES)
    {
        return GL_PrepareSysFlareTexture(oldIdx-1);
    }
    return 0; // Use the automatic selection logic.
}

TextureVariant* GL_PreparePatchTexture2(Texture* tex, int wrapS, int wrapT)
{
    texturevariantspecification_t* texSpec;
    patchtex_t* pTex;
    if(novideo || !tex) return 0;
    if(Textures_Namespace(Textures_Id(tex)) != TN_PATCHES)
    {
#if _DEBUG
        Con_Message("Warning:GL_PreparePatchTexture: Attempted to prepare non-patch [%p].\n", (void*)tex);
#endif
        return 0;
    }
    pTex = (patchtex_t*)Texture_UserData(tex);
    assert(pTex);

    texSpec = GL_TextureVariantSpecificationForContext(TC_UI,
            0 | ((pTex->flags & PF_MONOCHROME)         ? TSF_MONOCHROME : 0)
              | ((pTex->flags & PF_UPSCALE_AND_SHARPEN)? TSF_UPSCALE_AND_SHARPEN : 0),
            0, 0, 0, wrapS, wrapT, 0, -3, 0, false, false, false, false);
    return GL_PrepareTextureVariant(tex, texSpec);
}

TextureVariant* GL_PreparePatchTexture(Texture* tex)
{
    return GL_PreparePatchTexture2(tex, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
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

void GL_SetRawTextureParams(int minMode)
{
    rawtex_t** rawTexs, **ptr;

    rawTexs = R_CollectRawTexs(NULL);
    for(ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t* r = (*ptr);
        if(r->tex) // Is the texture loaded?
        {
            LIBDENG_ASSERT_IN_MAIN_THREAD();
            LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

            glBindTexture(GL_TEXTURE_2D, r->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMode);
        }
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

void GL_DoUpdateTexParams(void)
{
    GL_SetTextureParams(glmode[mipmapping], true, true);
}

static int reloadTextures(void* parameters)
{
    boolean usingBusyMode = *((boolean*) parameters);

    /// \todo re-upload ALL textures currently in use.
    GL_LoadSystemTextures();
    Rend_ParticleLoadExtraTextures();

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

    GL_ReleaseTextures();
    Con_Printf("All DGL textures deleted.\n");

    if(useBusyMode)
    {
        Con_InitProgress(200);
        Con_Busy(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Reseting textures...", reloadTextures, &useBusyMode);
    }
    else
    {
        reloadTextures(&useBusyMode);
    }
}

void GL_DoUpdateTexGamma(void)
{
    if(initedOk)
    {
        calcGammaTable();
        GL_TexReset();
    }

    Con_Printf("Gamma correction set to %f.\n", texGamma);
}

void GL_DoTexReset(void)
{
    GL_TexReset();
}

void GL_DoResetDetailTextures(void)
{
    GL_ReleaseTexturesByNamespace(TN_DETAILS);
}

void GL_ReleaseTexturesForRawImages(void)
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

#if 0
static int setVariantMinFilter(TextureVariant* tex, void* paramaters)
{
    DGLuint glName = TextureVariant_GLName(tex);
    if(glName)
    {
        int minFilter = *((int*)paramaters);
        glBindTexture(GL_TEXTURE_2D, glName);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    }
    return 0; // Continue iteration.
}

static int setVariantMinFilterWorker(Texture* tex, void* paramaters)
{
    Texture_IterateVariants(tex, setVariantMinFilter, paramaters);
    return 0; // Continue iteration.
}
#endif

void GL_SetAllTexturesMinFilter(int minFilter)
{
    int localMinFilter = minFilter;
    /// @todo This is no longer correct logic. Changing the global minification
    ///        filter should not modify the uploaded texture content.
#if 0
    Textures_Iterate2(TN_ANY, setVariantMinFilterWorker, (void*)&localMinFilter);
#endif
}

static void performImageAnalyses(Texture* tex, const image_t* image,
    const texturevariantspecification_t* spec, boolean forceUpdate)
{
    assert(spec && image);

    // Do we need color palette info?
    if(TST_GENERAL == spec->type && image->paletteId != 0)
    {
        colorpalette_analysis_t* cp = (colorpalette_analysis_t*) Texture_Analysis(tex, TA_COLORPALETTE);
        boolean firstInit = (!cp);

        if(firstInit)
        {
            cp = (colorpalette_analysis_t*) malloc(sizeof *cp);
            if(!cp)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new ColorPaletteAnalysis.", (unsigned long) sizeof *cp);
            Texture_AttachAnalysis(tex, TA_COLORPALETTE, cp);
        }

        if(firstInit || forceUpdate)
            cp->paletteId = image->paletteId;
    }

    // Calculate a point light source for Dynlight and/or Halo?
    if(TST_GENERAL == spec->type && TC_SPRITE_DIFFUSE == TS_GENERAL(spec)->context)
    {
        pointlight_analysis_t* pl = (pointlight_analysis_t*) Texture_Analysis(tex, TA_SPRITE_AUTOLIGHT);
        boolean firstInit = (!pl);

        if(firstInit)
        {
            pl = (pointlight_analysis_t*) malloc(sizeof *pl);
            if(!pl)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new PointLightAnalysis.", (unsigned long) sizeof *pl);
            Texture_AttachAnalysis(tex, TA_SPRITE_AUTOLIGHT, pl);
        }

        if(firstInit || forceUpdate)
            GL_CalcLuminance(image->pixels, image->size.width, image->size.height, image->pixelSize,
                R_ToColorPalette(image->paletteId), &pl->originX, &pl->originY, &pl->color, &pl->brightMul);
    }

    // Average alpha?
    if(spec->type == TST_GENERAL &&
       (TS_GENERAL(spec)->context == TC_SPRITE_DIFFUSE) ||
       (TS_GENERAL(spec)->context == TC_UI))
    {
        averagealpha_analysis_t* aa = (averagealpha_analysis_t*) Texture_Analysis(tex, TA_ALPHA);
        boolean firstInit = (!aa);

        if(firstInit)
        {
            aa = (averagealpha_analysis_t*) malloc(sizeof *aa);
            if(!aa)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new AverageAlphaAnalysis.", (unsigned long) sizeof *aa);
            Texture_AttachAnalysis(tex, TA_ALPHA, aa);
        }

        if(firstInit || forceUpdate)
        {
            if(!image->paletteId)
            {
                FindAverageAlpha(image->pixels, image->size.width, image->size.height,
                                 image->pixelSize, &aa->alpha, &aa->coverage);
            }
            else
            {
                if(image->flags & IMGF_IS_MASKED)
                {
                    FindAverageAlphaIdx(image->pixels, image->size.width, image->size.height,
                                        R_ToColorPalette(image->paletteId), &aa->alpha, &aa->coverage);
                }
                else
                {
                    // It has no mask, so it must be opaque.
                    aa->alpha = 1;
                    aa->coverage = 0;
                }
            }
        }
    }

    // Average color for sky ambient color?
    if(TST_GENERAL == spec->type && TC_SKYSPHERE_DIFFUSE == TS_GENERAL(spec)->context)
    {
        averagecolor_analysis_t* ac = (averagecolor_analysis_t*) Texture_Analysis(tex, TA_COLOR);
        boolean firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t*) malloc(sizeof *ac);
            if(!ac)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof *ac);
            Texture_AttachAnalysis(tex, TA_COLOR, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image->paletteId)
            {
                FindAverageColor(image->pixels, image->size.width, image->size.height,
                    image->pixelSize, &ac->color);
            }
            else
            {
                FindAverageColorIdx(image->pixels, image->size.width, image->size.height,
                    R_ToColorPalette(image->paletteId), false, &ac->color);
            }
        }
    }

    // Amplified average color for plane glow?
    if(TST_GENERAL == spec->type && TC_MAPSURFACE_DIFFUSE == TS_GENERAL(spec)->context)
    {
        averagecolor_analysis_t* ac = (averagecolor_analysis_t*) Texture_Analysis(tex, TA_COLOR_AMPLIFIED);
        boolean firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t*) malloc(sizeof *ac);
            if(!ac)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof *ac);
            Texture_AttachAnalysis(tex, TA_COLOR_AMPLIFIED, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image->paletteId)
            {
                FindAverageColor(image->pixels, image->size.width, image->size.height,
                    image->pixelSize, &ac->color);
            }
            else
            {
                FindAverageColorIdx(image->pixels, image->size.width, image->size.height,
                    R_ToColorPalette(image->paletteId), false, &ac->color);
            }
            R_AmplifyColor(ac->color.rgb);
        }
    }

    // Average top line color for sky sphere fadeout?
    if(TST_GENERAL == spec->type && TC_SKYSPHERE_DIFFUSE == TS_GENERAL(spec)->context)
    {
        averagecolor_analysis_t* ac = (averagecolor_analysis_t*) Texture_Analysis(tex, TA_LINE_TOP_COLOR);
        boolean firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t*) malloc(sizeof *ac);
            if(!ac)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof *ac);
            Texture_AttachAnalysis(tex, TA_LINE_TOP_COLOR, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image->paletteId)
            {
                FindAverageLineColor(image->pixels, image->size.width, image->size.height,
                    image->pixelSize, 0, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image->pixels, image->size.width, image->size.height,
                    0, R_ToColorPalette(image->paletteId), false, &ac->color);
            }
        }
    }

    // Average bottom line color for sky sphere fadeout?
    if(TST_GENERAL == spec->type && TC_SKYSPHERE_DIFFUSE == TS_GENERAL(spec)->context)
    {
        averagecolor_analysis_t* ac = (averagecolor_analysis_t*) Texture_Analysis(tex, TA_LINE_BOTTOM_COLOR);
        boolean firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t*) malloc(sizeof *ac);
            if(!ac)
                Con_Error("Textures::performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof *ac);
            Texture_AttachAnalysis(tex, TA_LINE_BOTTOM_COLOR, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image->paletteId)
            {
                FindAverageLineColor(image->pixels, image->size.width, image->size.height,
                    image->pixelSize, image->size.height-1, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image->pixels, image->size.width, image->size.height,
                    image->size.height-1, R_ToColorPalette(image->paletteId), false, &ac->color);
            }
        }
    }
}

static boolean tryLoadImageAndPrepareVariant(Texture* tex,
    texturevariantspecification_t* spec, TextureVariant** variant)
{
    uploadcontentmethod_t uploadMethod;
    TexSource source = TEXS_NONE;
    image_t image;
    assert(initedOk && spec);

    // Load the source image data.
    if(TN_TEXTURES == Textures_Namespace(Textures_Id(tex)))
    {
        // Try to load a replacement version of this texture?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !Texture_IsCustom(tex)))
        {
            patchcompositetex_t* texDef = (patchcompositetex_t*)Texture_UserData(tex);
            const ddstring_t suffix = { "-ck" };
            ddstring_t searchPath;
            assert(texDef);

            Str_Init(&searchPath);
            Str_Appendf(&searchPath, TEXTURES_RESOURCE_NAMESPACE_NAME":%s;", Str_Text(&texDef->name));
            source = GL_LoadExtTextureEX(&image, Str_Text(&searchPath), Str_Text(&suffix), true);
            Str_Free(&searchPath);
        }

        if(source == TEXS_NONE)
        {
            if(TC_SKYSPHERE_DIFFUSE != TS_GENERAL(spec)->context)
            {
                source = GL_LoadPatchComposite(&image, tex);
            }
            else
            {
                const boolean zeroMask = !!(TS_GENERAL(spec)->flags & TSF_ZEROMASK);
                source = GL_LoadPatchCompositeAsSky(&image, tex, zeroMask);
            }
        }
    }
    else
    {
        source = loadSourceImage(tex, spec, &image);
    }

    if(source == TEXS_NONE)
    {
        // No image found/failed to load.
        //Con_Message("Warning:Textures::tryLoadImageAndPrepareVariant: No image found for "
        //            "\"%s\"\n", Texture_Name(tex));
        return false;
    }

    // Are we setting the logical dimensions to the actual pixel dimensions?
    if(0 == Texture_Width(tex) || 0 == Texture_Height(tex))
    {
#if _DEBUG
        Uri* uri = Textures_ComposeUri(Textures_Id(tex));
        ddstring_t* path = Uri_ToString(uri);
        VERBOSE2( Con_Message("Logical dimensions for \"%s\" taken from image pixels [%ix%i].\n",
            Str_Text(path), image.size.width, image.size.height) );
        Str_Delete(path);
        Uri_Delete(uri);
#endif
        Texture_SetSize(tex, &image.size);
    }

    performImageAnalyses(tex, &image, spec, true /*Always update*/);

    // Do we need to allocate a variant?
    if(!*variant)
    {
        DGLuint newGLName = GL_GetReservedTextureName();
        *variant = TextureVariant_New(tex, source, spec);
        TextureVariant_SetGLName(*variant, newGLName);
        Texture_AddVariant(tex, *variant);
    }
    // Are we re-preparing a released texture?
    else if(0 == TextureVariant_GLName(*variant))
    {
        DGLuint newGLName = GL_GetReservedTextureName();
        TextureVariant_SetSource(*variant, source);
        TextureVariant_SetGLName(*variant, newGLName);
    }

    // (Re)Prepare the variant according to specification.
    switch(spec->type)
    {
    case TST_GENERAL: uploadMethod = prepareVariantFromImage(*variant, &image); break;
    case TST_DETAIL:  uploadMethod = prepareDetailVariantFromImage(*variant, &image); break;
    default:
        Con_Error("tryLoadImageAndPrepareVariant: Invalid spec type %i.", spec->type);
        exit(1); // Unreachable.
    }

    // We're done with the image data.
    GL_DestroyImage(&image);

#ifdef _DEBUG
    VERBOSE(
        Uri* uri = Textures_ComposeUri(Textures_Id(tex));
        ddstring_t* path = Uri_ToString(uri);
        Con_Printf("Prepared TextureVariant (name:\"%s\" glName:%u)%s\n",
            Str_Text(path), (unsigned int) TextureVariant_GLName(*variant),
            (METHOD_IMMEDIATE == uploadMethod)? " while not busy!" : "");
        Str_Delete(path);
        Uri_Delete(uri);
        )
    VERBOSE2(
        Con_Printf("  Content: ");
        GL_PrintImageMetadata(&image);
        Con_Printf("  Specification: ");
        GL_PrintTextureVariantSpecification(spec);
        )
#endif

    return true;
}

static TextureVariant* findVariantForSpec(Texture* tex,
    const texturevariantspecification_t* spec)
{
    // Look for an exact match.
    TextureVariant* variant = chooseVariant(METHOD_MATCH, tex, spec);
#if _DEBUG
    // 07/04/2011 dj: The "fuzzy selection" features are yet to be implemented.
    // As such, the following should NOT return a valid variant iff the rest of
    // this subsystem has been implemented correctly.
    //
    // Presently this is used as a sanity check.
    if(!variant)
    {
        /// No luck, try fuzzy.
        variant = chooseVariant(METHOD_FUZZY, tex, spec);
        assert(NULL == variant);
    }
#endif
    return variant;
}

TextureVariant* GL_PrepareTextureVariant2(Texture* tex, texturevariantspecification_t* spec,
    preparetextureresult_t* outcome)
{
    // Have we already prepared something suitable?
    TextureVariant* variant = findVariantForSpec(tex, spec);

    if(variant && TextureVariant_IsPrepared(variant))
    {
        if(outcome) *outcome = PTR_FOUND;
        return variant;
    }

    // Suffer the cache miss and attempt to (re)prepare a variant.
    { boolean loadedOk = tryLoadImageAndPrepareVariant(tex, spec, &variant);
    if(outcome)
    {
        if(loadedOk)
        {
            switch(TextureVariant_Source(variant))
            {
            case TEXS_ORIGINAL: *outcome = PTR_UPLOADED_ORIGINAL; break;
            case TEXS_EXTERNAL: *outcome = PTR_UPLOADED_EXTERNAL; break;
            default:
                Con_Error("GL_PrepareTextureVariant2: Unknown TexSource %i.",
                          (int)TextureVariant_Source(variant));
                exit(1); // Unreachable.
            }
        }
        else
        {
            *outcome = PTR_NOTFOUND;
        }
    }}
    return variant;
}

TextureVariant* GL_PrepareTextureVariant(Texture* tex, texturevariantspecification_t* spec)
{
    return GL_PrepareTextureVariant2(tex, spec, NULL);
}

DGLuint GL_PrepareTexture2(struct texture_s* tex, texturevariantspecification_t* spec,
    preparetextureresult_t* returnOutcome)
{
    const TextureVariant* variant = GL_PrepareTextureVariant2(tex, spec, returnOutcome);
    if(!variant) return 0;
    return TextureVariant_GLName(variant);
}

DGLuint GL_PrepareTexture(struct texture_s* tex, texturevariantspecification_t* spec)
{
    return GL_PrepareTexture2(tex, spec, NULL);
}

void GL_BindTexture(TextureVariant* tex)
{
    texturevariantspecification_t* spec = NULL;

    if(Con_InBusyWorker()) return;

    if(tex)
    {
        spec = TextureVariant_Spec(tex);
        // Ensure we've prepared this.
        if(!TextureVariant_IsPrepared(tex))
        {
            TextureVariant** hndl = &tex;
            if(!tryLoadImageAndPrepareVariant(TextureVariant_GeneralCase(tex), spec, hndl))
            {
                tex = NULL;
            }
        }
    }

    // Bind our chosen texture.
    if(!tex)
    {
        GL_SetNoTexture();
        return;
    }

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, TextureVariant_GLName(tex));
    Sys_GLCheckError();

    // Apply dynamic adjustments to the GL texture state according to our spec.
    if(spec->type == TST_GENERAL)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, TS_GENERAL(spec)->wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, TS_GENERAL(spec)->wrapT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glMagFilterForVariantSpec(TS_GENERAL(spec)));
        if(GL_state.features.texFilterAniso)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            GL_GetTexAnisoMul(logicalAnisoLevelForVariantSpec(TS_GENERAL(spec))));
        }
    }
}

int GL_ReleaseGLTexturesByTexture2(Texture* tex, void* paramaters)
{
    if(tex)
    {
        int i;
        Texture_IterateVariants(tex, releaseVariantGLTexture, NULL/*no paramaters*/);
        for(i = (int) TEXTURE_ANALYSIS_FIRST; i < (int) TEXTURE_ANALYSIS_COUNT; ++i)
        {
            void* data = Texture_DetachAnalysis(tex, (texture_analysisid_t) i);
            if(data) free(data);
        }
    }
    return 0; // Continue iteration.
}

int GL_ReleaseGLTexturesByTexture(Texture* tex)
{
    return GL_ReleaseGLTexturesByTexture2(tex, NULL);
}

void GL_ReleaseTexturesByNamespace(texturenamespaceid_t texNamespace)
{
    Textures_Iterate(texNamespace, GL_ReleaseGLTexturesByTexture2);
}

void GL_ReleaseVariantTexturesBySpec(Texture* tex, texturevariantspecification_t* spec)
{
    if(!tex) return;
    Texture_IterateVariants(tex, releaseVariantGLTexture, (void*)spec);
}

void GL_ReleaseVariantTexture(TextureVariant* tex)
{
    if(!tex) return;
    releaseVariantGLTexture(tex, NULL);
}

static int releaseGLTexturesByColorPaletteWorker(Texture* tex, void* paramaters)
{
    colorpalette_analysis_t* cp = (colorpalette_analysis_t*) Texture_Analysis(tex, TA_COLORPALETTE);
    colorpaletteid_t paletteId = *(colorpaletteid_t*)paramaters;
    assert(tex && paramaters);

    if(cp && cp->paletteId == paletteId)
    {
        GL_ReleaseGLTexturesByTexture(tex);
    }
    return 0; // Continue iteration.
}

void GL_ReleaseTexturesByColorPalette(colorpaletteid_t paletteId)
{
    Textures_Iterate2(TN_ANY, releaseGLTexturesByColorPaletteWorker, (void*)&paletteId);
}

void GL_InitTextureContent(texturecontent_t* content)
{
    assert(content);
    content->format = 0;
    content->name = 0;
    content->pixels = 0;
    content->paletteId = 0;
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
    texturecontent_t* c;
    uint8_t* pixels;
    int bytesPerPixel;
    size_t bufferSize;
    assert(other);

    c = (texturecontent_t*)malloc(sizeof *c);
    if(!c)
        Con_Error("GL_ConstructTextureContentCopy: Failed on allocation of %lu bytes for new TextureContent.", (unsigned long) sizeof *c);

    memcpy(c, other, sizeof(*c));

    // Duplicate the image buffer.
    bytesPerPixel = BytesPerPixelFmt(other->format);
    bufferSize = bytesPerPixel * other->width * other->height;
    pixels = malloc(bufferSize);
    memcpy(pixels, other->pixels, bufferSize);
    c->pixels = pixels;
    return c;
}

void GL_DestroyTextureContent(texturecontent_t* content)
{
    assert(content);
    if(content->pixels) free((uint8_t*)content->pixels);
    free(content);
}

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags)
{
    texturecontent_t c;

    GL_InitTextureContent(&c);
    c.name = GL_GetReservedTextureName();
    c.format = format;
    c.width = width;
    c.height = height;
    c.pixels = pixels;
    c.flags = flags;

    uploadContentUnmanaged(chooseContentUploadMethod(&c), &c);
    return c.name;
}

DGLuint GL_NewTextureWithParams2(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int grayMipmap, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT)
{
    texturecontent_t c;

    GL_InitTextureContent(&c);
    c.name = GL_GetReservedTextureName();
    c.format = format;
    c.width = width;
    c.height = height;
    c.pixels = pixels;
    c.flags = flags;
    c.grayMipmap = grayMipmap;
    c.minFilter = minFilter;
    c.magFilter = magFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;

    uploadContentUnmanaged(chooseContentUploadMethod(&c), &c);
    return c.name;
}

D_CMD(LowRes)
{
    GL_LowRes();
    return true;
}

D_CMD(TexReset)
{
    if(argc == 2 && !stricmp(argv[1], "raw"))
    {
        // Reset just raw images.
        GL_ReleaseTexturesForRawImages();
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
    int newMipMode = strtol(argv[1], NULL, 0);
    if(newMipMode < 0 || newMipMode > 5)
    {
        Con_Message("Invalid mipmapping mode %i specified. Valid range is [0...5].\n", newMipMode);
        return false;
    }

    mipmapping = newMipMode;
    GL_SetTextureParams(glmode[mipmapping], true, false);
    return true;
}
