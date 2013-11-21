/** @file gl_texmanager.cpp GL-Texture management
 *
 * @todo This file needs to be split into smaller portions.
 *
 * @authors Copyright &copy; 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2002 Graham Jackson <no contact email published>
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

#include "de_platform.h"
#include "gl/gl_texmanager.h"

#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_resource.h"
#include "de_play.h"
#include "de_ui.h"
#include "clientapp.h"

#include "def_main.h"
#include "resource/hq2x.h"

#include <QSize>
#include <de/ByteRefArray>
#include <de/mathutil.h>
#include <de/memory.h>
#include <de/memoryzone.h>
#include <cstdlib>
#include <cstring> // memset, memcpy
#include <cctype>
#include <cmath>

using namespace de;

int mipmapping = 5;
int filterUI   = 1;
int texQuality = TEXQ_BEST;

struct texturevariantspecificationlist_node_t
{
    texturevariantspecificationlist_node_t *next;
    texturevariantspecification_t *spec;
};

typedef texturevariantspecificationlist_node_t variantspecificationlist_t;

D_CMD(LowRes);
D_CMD(MipMap);
D_CMD(TexReset);

void GL_DoResetDetailTextures();
void GL_DoTexReset();
void GL_DoUpdateTexGamma();
void GL_DoUpdateTexParams();

static int hashDetailVariantSpecification(detailvariantspecification_t const &spec);

static res::Source loadRaw(image_t &image, rawtex_t const &raw);

int ratioLimit = 0; // Zero if none.
boolean fillOutlines = true;
int useSmartFilter = 0; // Smart filter mode (cvar: 1=hq2x)
int filterSprites = true;
int texMagMode = 1; // Linear.
int texAniso = -1; // Use best.

boolean noHighResTex = false;
boolean noHighResPatches = false;
boolean highResWithPWAD = false;
byte loadExtAlways = false; // Always check for extres (cvar)

float texGamma = 0;

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
DGLuint lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
DGLuint sysFlareTextures[NUM_SYSFLARE_TEXTURES];

static boolean initedOk = false; // Init done.

static variantspecificationlist_t *variantSpecs;

/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)
static variantspecificationlist_t *detailVariantSpecs[DETAILVARIANT_CONTRAST_HASHSIZE];

void GL_TexRegister()
{
    C_VAR_INT   ("rend-tex",                    &renderTextures,     CVF_NO_ARCHIVE, 0, 2);
    C_VAR_INT   ("rend-tex-detail",             &r_detail,           0, 0, 1);
    C_VAR_INT   ("rend-tex-detail-multitex",    &useMultiTexDetails, 0, 0, 1);
    C_VAR_FLOAT ("rend-tex-detail-scale",       &detailScale,        CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-tex-detail-strength",    &detailFactor,       0, 0, 5, GL_DoResetDetailTextures);
    C_VAR_BYTE2 ("rend-tex-external-always",    &loadExtAlways,      0, 0, 1, GL_DoTexReset);
    C_VAR_INT   ("rend-tex-filter-anisotropic", &texAniso,           0, -1, 4);
    C_VAR_INT   ("rend-tex-filter-mag",         &texMagMode,         0, 0, 1);
    C_VAR_INT2  ("rend-tex-filter-smart",       &useSmartFilter,     0, 0, 1, GL_DoTexReset);
    C_VAR_INT   ("rend-tex-filter-sprite",      &filterSprites,      0, 0, 1);
    C_VAR_INT   ("rend-tex-filter-ui",          &filterUI,           0, 0, 1);
    C_VAR_FLOAT2("rend-tex-gamma",              &texGamma,           0, 0, 1, GL_DoUpdateTexGamma);
    C_VAR_INT2  ("rend-tex-mipmap",             &mipmapping,         CVF_PROTECTED, 0, 5, GL_DoTexReset);
    C_VAR_INT2  ("rend-tex-quality",            &texQuality,         0, 0, 8, GL_DoTexReset);

    C_CMD_FLAGS ("lowres",      "",     LowRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS ("mipmap",      "i",    MipMap, CMDF_NO_DEDICATED);
    C_CMD_FLAGS ("texreset",    "",     TexReset, CMDF_NO_DEDICATED);
}

static void unlinkVariantSpecification(texturevariantspecification_t &spec)
{
    variantspecificationlist_t **listHead;
    DENG_ASSERT(initedOk);

    // Select list head according to variant specification type.
    switch(spec.type)
    {
    case TST_GENERAL:   listHead = &variantSpecs; break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(spec));
        listHead = &detailVariantSpecs[hash];
        break; }

    default:
        Con_Error("unlinkVariantSpecification: Invalid spec type %i.", spec.type);
        exit(1); // Unreachable.
    }

    if(*listHead)
    {
        texturevariantspecificationlist_node_t *node = 0;
        if((*listHead)->spec == &spec)
        {
            node = (*listHead);
            *listHead = (*listHead)->next;
        }
        else
        {
            // Find the previous node.
            texturevariantspecificationlist_node_t *prevNode = (*listHead);
            while(prevNode->next && prevNode->next->spec != &spec)
            {
                prevNode = prevNode->next;
            }
            if(prevNode)
            {
                node = prevNode->next;
                prevNode->next = prevNode->next->next;
            }
        }
        M_Free(node);
    }
}

static void destroyVariantSpecification(texturevariantspecification_t &spec)
{
    unlinkVariantSpecification(spec);
    if(spec.type == TST_GENERAL && (TS_GENERAL(spec).flags & TSF_HAS_COLORPALETTE_XLAT))
    {
        M_Free(TS_GENERAL(spec).translated);
    }
    M_Free(&spec);
}

static texturevariantspecification_t *copyVariantSpecification(
    texturevariantspecification_t const &tpl)
{
    texturevariantspecification_t *spec = (texturevariantspecification_t *) M_Malloc(sizeof(*spec));
    if(!spec) Con_Error("Textures::copyVariantSpecification: Failed on allocation of %lu bytes for new TextureVariantSpecification.", (unsigned long) sizeof(*spec));

    std::memcpy(spec, &tpl, sizeof(texturevariantspecification_t));
    if(TS_GENERAL(tpl).flags & TSF_HAS_COLORPALETTE_XLAT)
    {
        colorpalettetranslationspecification_t *cpt = (colorpalettetranslationspecification_t *) M_Malloc(sizeof(*cpt));
        if(!cpt) Con_Error("Textures::copyVariantSpecification: Failed on allocation of %lu bytes for new ColorPaletteTranslationSpecification.", (unsigned long) sizeof(*cpt));

        std::memcpy(cpt, TS_GENERAL(tpl).translated, sizeof(colorpalettetranslationspecification_t));
        TS_GENERAL(*spec).translated = cpt;
    }
    return spec;
}

static texturevariantspecification_t *copyDetailVariantSpecification(
    texturevariantspecification_t const &tpl)
{
    texturevariantspecification_t *spec = (texturevariantspecification_t *) M_Malloc(sizeof(*spec));
    if(!spec) Con_Error("Textures::copyDetailVariantSpecification: Failed on allocation of %lu bytes for new TextureVariantSpecification.", (unsigned long) sizeof(*spec));

    std::memcpy(spec, &tpl, sizeof(texturevariantspecification_t));
    return spec;
}

static colorpalettetranslationspecification_t *applyColorPaletteTranslationSpecification(
    colorpalettetranslationspecification_t *spec, int tClass, int tMap)
{
    DENG_ASSERT(initedOk && spec);
    LOG_AS("applyColorPaletteTranslationSpecification");

    spec->tClass = MAX_OF(0, tClass);
    spec->tMap   = MAX_OF(0, tMap);

#if _DEBUG
    if(0 == tClass && 0 == tMap)
        LOG_WARNING("Applied unnecessary zero-translation (tClass:0 tMap:0).");
#endif

    return spec;
}

static variantspecification_t &applyVariantSpecification(
    variantspecification_t &spec, texturevariantusagecontext_t tc, int flags,
    byte border, colorpalettetranslationspecification_t *colorPaletteTranslationSpec,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter, boolean mipmapped,
    boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    DENG_ASSERT(initedOk && (tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc)));

    flags &= ~TSF_INTERNAL_MASK;

    spec.context = tc;
    spec.flags = flags;
    spec.border = (flags & TSF_UPSCALE_AND_SHARPEN)? 1 : border;
    spec.mipmapped = mipmapped;
    spec.wrapS = wrapS;
    spec.wrapT = wrapT;
    spec.minFilter = MINMAX_OF(-1, minFilter, spec.mipmapped? 3:1);
    spec.magFilter = MINMAX_OF(-3, magFilter, 1);
    spec.anisoFilter = MINMAX_OF(-1, anisoFilter, 4);
    spec.gammaCorrection = gammaCorrection;
    spec.noStretch = noStretch;
    spec.toAlpha = toAlpha;
    if(colorPaletteTranslationSpec)
    {
        spec.flags |= TSF_HAS_COLORPALETTE_XLAT;
        spec.translated = colorPaletteTranslationSpec;
    }
    else
    {
        spec.translated = NULL;
    }

    return spec;
}

static detailvariantspecification_t &applyDetailVariantSpecification(
    detailvariantspecification_t &spec, float contrast)
{
    int const quantFactor = DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR;

    spec.contrast = 255 * (int)MINMAX_OF(0, contrast * quantFactor + .5f, quantFactor) * (1 / float(quantFactor));
    return spec;
}

static int hashDetailVariantSpecification(detailvariantspecification_t const &spec)
{
    return (spec.contrast * (1/255.f) * DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR + .5f);
}

static texturevariantspecification_t &linkVariantSpecification(
    texturevariantspecificationtype_t type, texturevariantspecification_t &spec)
{
    texturevariantspecificationlist_node_t *node;
    DENG_ASSERT(initedOk && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type));

    node = (texturevariantspecificationlist_node_t *) M_Malloc(sizeof(*node));
    if(!node) Con_Error("Textures::linkVariantSpecification: Failed on allocation of %lu bytes for new TextureVariantSpecificationListNode.", (unsigned long) sizeof(*node));

    node->spec = &spec;
    switch(type)
    {
    case TST_GENERAL:
        node->next = variantSpecs;
        variantSpecs = (variantspecificationlist_t *)node;
        break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(spec));
        node->next = detailVariantSpecs[hash];
        detailVariantSpecs[hash] = (variantspecificationlist_t *)node;
        break; }
    }
    return spec;
}

static texturevariantspecification_t *findVariantSpecification(
    texturevariantspecificationtype_t type, texturevariantspecification_t const &tpl,
    bool canCreate)
{
    texturevariantspecificationlist_node_t *node;
    DENG_ASSERT(initedOk && VALID_TEXTUREVARIANTSPECIFICATIONTYPE(type));

    // Select list head according to variant specification type.
    switch(type)
    {
    case TST_GENERAL:   node = variantSpecs; break;
    case TST_DETAIL: {
        int hash = hashDetailVariantSpecification(TS_DETAIL(tpl));
        node = detailVariantSpecs[hash];
        break; }

    default:
        Con_Error("findVariantSpecification: Invalid spec type %i.", type);
        exit(1); // Unreachable.
    }

    // Do we already have a concrete version of the template specification?
    for(; node; node = node->next)
    {
        if(TextureVariantSpec_Compare(node->spec, &tpl))
            return node->spec;
    }

    // Not found, can we create?
    if(canCreate)
    switch(type)
    {
    case TST_GENERAL:   return &linkVariantSpecification(type, *copyVariantSpecification(tpl));
    case TST_DETAIL:    return &linkVariantSpecification(type, *copyDetailVariantSpecification(tpl));
    }

    return NULL;
}

static texturevariantspecification_t *getVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass,
    int tMap, int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    static texturevariantspecification_t tpl;
    static colorpalettetranslationspecification_t cptTpl;
    boolean haveCpt = false;
    DENG_ASSERT(initedOk);

    tpl.type = TST_GENERAL;
    if(0 != tClass || 0 != tMap)
    {
        // A color palette translation spec is required.
        applyColorPaletteTranslationSpecification(&cptTpl, tClass, tMap);
        haveCpt = true;
    }

    applyVariantSpecification(TS_GENERAL(tpl), tc, flags, border, haveCpt? &cptTpl : NULL,
        wrapS, wrapT, minFilter, magFilter, anisoFilter, mipmapped, gammaCorrection,
        noStretch, toAlpha);

    // Retrieve a concrete version of the rationalized specification.
    return findVariantSpecification(tpl.type, tpl, true);
}

static texturevariantspecification_t *getDetailVariantSpecificationForContext(
    float contrast)
{
    static texturevariantspecification_t tpl;
    DENG_ASSERT(initedOk);

    tpl.type = TST_DETAIL;
    applyDetailVariantSpecification(TS_DETAIL(tpl), contrast);
    return findVariantSpecification(tpl.type, tpl, true);
}

static void emptyVariantSpecificationList(variantspecificationlist_t *list)
{
    texturevariantspecificationlist_node_t* node = (texturevariantspecificationlist_node_t *) list;
    DENG_ASSERT(initedOk);

    while(node)
    {
        texturevariantspecificationlist_node_t *next = node->next;
        destroyVariantSpecification(*node->spec);
        node = next;
    }
}

static bool variantSpecInUse(texturevariantspecification_t const &spec)
{
    foreach(Texture *texture, App_Textures().all())
    foreach(TextureVariant *variant, texture->variants())
    {
        if(&variant->spec() == &spec)
        {
            return true; // Found one; stop.
        }
    }
    return false;
}

static int pruneUnusedVariantSpecificationsInList(variantspecificationlist_t *list)
{
    texturevariantspecificationlist_node_t *node = list;
    int numPruned = 0;
    while(node)
    {
        texturevariantspecificationlist_node_t *next = node->next;

        if(!variantSpecInUse(*node->spec))
        {
            destroyVariantSpecification(*node->spec);
            ++numPruned;
        }

        node = next;
    }
    return numPruned;
}

static int pruneUnusedVariantSpecifications(texturevariantspecificationtype_t specType)
{
    DENG_ASSERT(initedOk);
    switch(specType)
    {
    case TST_GENERAL: return pruneUnusedVariantSpecificationsInList(variantSpecs);
    case TST_DETAIL: {
        int numPruned = 0;
        for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
        {
            numPruned += pruneUnusedVariantSpecificationsInList(detailVariantSpecs[i]);
        }
        return numPruned; }

    default:
        Con_Error("Textures::pruneUnusedVariantSpecifications: Invalid variant spec type %i.", (int) specType);
        exit(1); // Unreachable.
    }
}

static void destroyVariantSpecifications()
{
    DENG_ASSERT(initedOk);

    emptyVariantSpecificationList(variantSpecs); variantSpecs = 0;
    for(int i = 0; i < DETAILVARIANT_CONTRAST_HASHSIZE; ++i)
    {
        emptyVariantSpecificationList(detailVariantSpecs[i]); detailVariantSpecs[i] = 0;
    }
}

static void uploadContentUnmanaged(texturecontent_t const &content)
{
    LOG_AS("uploadContentUnmanaged");
    if(novideo) return;

    gl::UploadMethod uploadMethod = GL_ChooseUploadMethod(&content);
    if(uploadMethod == gl::Immediate)
    {
        LOG_DEBUG("Uploading texture (%i:%ix%i) while not busy! Should be precached in busy mode?")
                << content.name << content.width << content.height;
    }

    GL_UploadTextureContent(content, uploadMethod);
}

void GL_InitTextureManager()
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
    std::memset(sysFlareTextures, 0, sizeof(sysFlareTextures));
    std::memset(lightingTextures, 0, sizeof(lightingTextures));

    variantSpecs = NULL;
    memset(detailVariantSpecs, 0, sizeof(detailVariantSpecs));

    GL_InitSmartFilterHQ2x();

    // Initialization done.
    initedOk = true;
}

void GL_ResetTextureManager()
{
    if(!initedOk) return;
    ResourceSystem().releaseAllGLTextures();
    GL_PruneTextureVariantSpecifications();
    GL_LoadSystemTextures();
}

static String nameForGLTextureWrapMode(int mode)
{
    if(mode == GL_REPEAT) return "repeat";
    if(mode == GL_CLAMP) return "clamp";
    if(mode == GL_CLAMP_TO_EDGE) return "clamp_edge";
    return "(unknown)";
}

String texturevariantspecification_t::asText() const
{
    static String const textureUsageContextNames[1 + TEXTUREVARIANTUSAGECONTEXT_COUNT] = {
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
    static String const textureSpecificationTypeNames[TEXTUREVARIANTSPECIFICATIONTYPE_COUNT] = {
        /* TST_GENERAL */   "general",
        /* TST_DETAIL */    "detail"
    };
    static String const filterModeNames[] = { "ui", "sprite", "noclass", "const" };
    static String const glFilterNames[] = {
        "nearest", "linear", "nearest_mipmap_nearest", "linear_mipmap_nearest",
        "nearest_mipmap_linear", "linear_mipmap_linear"
    };

    String text = String("Type:%1").arg(textureSpecificationTypeNames[type]);

    switch(type)
    {
    case TST_DETAIL: {
        detailvariantspecification_t const &spec = data.detailvariant;
        text += " Contrast:" + String::number(int(.5f + spec.contrast / 255.f * 100)) + "%";
        break; }

    case TST_GENERAL: {
        variantspecification_t const &spec = data.variant;
        texturevariantusagecontext_t tc = spec.context;
        DENG_ASSERT(tc == TC_UNKNOWN || VALID_TEXTUREVARIANTUSAGECONTEXT(tc));

        int glMinFilterNameIdx;
        if(spec.minFilter >= 0) // Constant logical value.
        {
            glMinFilterNameIdx = (spec.mipmapped? 2 : 0) + spec.minFilter;
        }
        else // "No class" preference.
        {
            glMinFilterNameIdx = spec.mipmapped? mipmapping : 1;
        }

        int glMagFilterNameIdx;
        if(spec.magFilter >= 0) // Constant logical value.
        {
            glMagFilterNameIdx = spec.magFilter;
        }
        else
        {
            // Preference for texture class id.
            switch(abs(spec.magFilter)-1)
            {
            // "No class" preference.
            default: glMagFilterNameIdx = texMagMode; break;

            // "Sprite" class.
            case 1:  glMagFilterNameIdx = filterSprites; break;

            // "UI" class.
            case 2:  glMagFilterNameIdx = filterUI; break;
            }
        }

        text += " Context:" + textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1]
              + " Flags:" + String::number(spec.flags & ~TSF_INTERNAL_MASK)
              + " Border:" + String::number(spec.border)
              + " MinFilter:" + filterModeNames[3 + de::clamp(-1, spec.minFilter, 0)]
                              + "|" + glFilterNames[glMinFilterNameIdx]
              + " MagFilter:" + filterModeNames[3 + de::clamp(-3, spec.magFilter, 0)]
                              + "|" + glFilterNames[glMagFilterNameIdx]
              + " AnisoFilter:" + String::number(spec.anisoFilter)
              + " WrapS:" + nameForGLTextureWrapMode(spec.wrapS)
              + " WrapT:" + nameForGLTextureWrapMode(spec.wrapT)
              + " CorrectGamma:" + (spec.gammaCorrection? "yes" : "no")
              + " NoStretch:" + (spec.noStretch? "yes" : "no")
              + " ToAlpha:" + (spec.toAlpha? "yes" : "no");

        if(spec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            colorpalettetranslationspecification_t const *cpt = spec.translated;
            DENG_ASSERT(cpt);
            text += " Translated:(tclass:" + String::number(cpt->tClass)
                                           + " tmap:" + String::number(cpt->tMap) + ")";
        }
        break; }
    }

    return text;
}

texturevariantspecification_t &GL_TextureVariantSpec(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    if(!initedOk) Con_Error("GL_TextureVariantSpec: GL texture manager not yet initialized.");

    texturevariantspecification_t *tvs
        = getVariantSpecificationForContext(tc, flags, border, tClass, tMap, wrapS, wrapT,
                                            minFilter, magFilter, anisoFilter,
                                            mipmapped, gammaCorrection, noStretch, toAlpha);

#ifdef _DEBUG
    if(tClass || tMap)
    {
        DENG_ASSERT(tvs->data.variant.flags & TSF_HAS_COLORPALETTE_XLAT);
        DENG_ASSERT(tvs->data.variant.translated);
        DENG_ASSERT(tvs->data.variant.translated->tClass == tClass);
        DENG_ASSERT(tvs->data.variant.translated->tMap == tMap);
    }
#endif

    return *tvs;
}

texturevariantspecification_t &GL_DetailTextureSpec(
    float contrast)
{
    if(!initedOk) Con_Error("GL_DetailTextureVariantSpecificationForContext: GL texture manager not yet initialized.");
    return *getDetailVariantSpecificationForContext(contrast);
}

void GL_ShutdownTextureManager()
{
    if(!initedOk) return;

    destroyVariantSpecifications();
    initedOk = false;
}

void GL_LoadSystemTextures()
{
    if(novideo || !initedOk) return;

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);
    GL_PrepareLSTexture(LST_CAMERA_VIGNETTE);

    GL_PrepareSysFlaremap(FXT_ROUND);
    GL_PrepareSysFlaremap(FXT_FLARE);
    if(!haloRealistic)
    {
        GL_PrepareSysFlaremap(FXT_BRFLARE);
        GL_PrepareSysFlaremap(FXT_BIGFLARE);
    }

    Rend_ParticleLoadSystemTextures();
}

void GL_DeleteAllLightingSystemTextures()
{
    if(novideo || !initedOk) return;

    glDeleteTextures(NUM_LIGHTING_TEXTURES, (GLuint const *) lightingTextures);
    std::memset(lightingTextures, 0, sizeof(lightingTextures));

    glDeleteTextures(NUM_SYSFLARE_TEXTURES, (GLuint const *) sysFlareTextures);
    std::memset(sysFlareTextures, 0, sizeof(sysFlareTextures));
}

void GL_PruneTextureVariantSpecifications()
{
    if(!initedOk || Sys_IsShuttingDown()) return;

    int numPruned = 0;
    numPruned += pruneUnusedVariantSpecifications(TST_GENERAL);
    numPruned += pruneUnusedVariantSpecifications(TST_DETAIL);

#if _DEBUG
    LOG_VERBOSE("Pruned %i unused texture variant %s.")
        << numPruned << (numPruned == 1? "specification" : "specifications");
#endif
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

boolean GL_UploadTextureGrayMipmap(int glFormat, int loadFormat, const uint8_t* pixels,
    int width, int height, float grayFactor)
{
    int i, w, h, numpels = width * height, numLevels, pixelSize;
    uint8_t* image, *faded, *out;
    const uint8_t* in;
    float invFactor;
    DENG_ASSERT(pixels);

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
    faded = (uint8_t*) M_Malloc(numpels / 4);
    image = (uint8_t*) M_Malloc(numpels);

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
    M_Free(faded);
    M_Free(image);

    DENG_ASSERT(!Sys_GLCheckError());
    return true;
}

boolean GL_UploadTexture(int glFormat, int loadFormat, const uint8_t* pixels,
    int width,  int height, int genMipmaps)
{
    const int packRowLength = 0, packAlignment = 1, packSkipRows = 0, packSkipPixels = 0;
    const int unpackRowLength = 0, unpackAlignment = 1, unpackSkipRows = 0, unpackSkipPixels = 0;
    int mipLevel = 0;
    DENG_ASSERT(pixels);

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

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

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
                M_Free(image);
            image = newimage;

            w = neww;
            h = newh;
        }

        if(image != pixels)
            M_Free(image);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, mipLevel, (GLint)glFormat, (GLsizei)width,
            (GLsizei)height, 0, (GLint)loadFormat, GL_UNSIGNED_BYTE, pixels);
    }

    glPopClientAttrib();
    DENG_ASSERT(!Sys_GLCheckError());

    return true;
}

DGLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    if(novideo) return 0;
    if(which < 0 || which >= NUM_LIGHTING_TEXTURES) return 0;

    static const struct TexDef {
        char const *name;
        gfxmode_t mode;
    } texDefs[NUM_LIGHTING_TEXTURES] = {
        /* LST_DYNAMIC */         { "dlight",     LGM_WHITE_ALPHA },
        /* LST_GRADIENT */        { "wallglow",   LGM_WHITE_ALPHA },
        /* LST_RADIO_CO */        { "radioco",    LGM_WHITE_ALPHA },
        /* LST_RADIO_CC */        { "radiocc",    LGM_WHITE_ALPHA },
        /* LST_RADIO_OO */        { "radiooo",    LGM_WHITE_ALPHA },
        /* LST_RADIO_OE */        { "radiooe",    LGM_WHITE_ALPHA },
        /* LST_CAMERA_VIGNETTE */ { "vignette",   LGM_NORMAL }
    };
    struct TexDef const &def = texDefs[which];

    if(!lightingTextures[which])
    {
        image_t image;

        if(GL_LoadExtImage(image, def.name, def.mode))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            DGLuint glName = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.width, image.size.height, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR, -1 /*best anisotropy*/,
                ( which == LST_GRADIENT? GL_REPEAT : GL_CLAMP_TO_EDGE ), GL_CLAMP_TO_EDGE);

            lightingTextures[which] = glName;
        }

        Image_Destroy(&image);
    }

    DENG_ASSERT(lightingTextures[which] != 0);
    return lightingTextures[which];
}

DGLuint GL_PrepareSysFlaremap(flaretexid_t which)
{
    if(novideo) return 0;
    if(which < 0 || which >= NUM_SYSFLARE_TEXTURES) return 0;

    static const struct TexDef {
        char const *name;
    } texDefs[NUM_SYSFLARE_TEXTURES] = {
        /* FXT_ROUND */     { "dlight" },
        /* FXT_FLARE */     { "flare" },
        /* FXT_BRFLARE */   { "brflare" },
        /* FXT_BIGFLARE */  { "bigflare" }
    };
    struct TexDef const &def = texDefs[which];

    if(!sysFlareTextures[which])
    {
        image_t image;

        if(GL_LoadExtImage(image, def.name, LGM_WHITE_ALPHA))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            DGLuint glName = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.width, image.size.height, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            sysFlareTextures[which] = glName;
        }

        Image_Destroy(&image);
    }

    DENG_ASSERT(sysFlareTextures[which] != 0);
    return sysFlareTextures[which];
}

DGLuint GL_PrepareFlaremap(de::Uri const &resourceUri)
{
    if(resourceUri.path().length() == 1)
    {
        // Select a system flare by numeric identifier?
        int number = resourceUri.path().toStringRef().first().digitValue();
        if(number == 0) return 0; // automatic
        if(number >= 1 && number <= 4)
        {
            return GL_PrepareSysFlaremap(flaretexid_t(number - 1));
        }
    }
    if(Texture *tex = App_ResourceSystem().texture("Flaremaps", &resourceUri))
    {
        if(TextureVariant const *variant = tex->prepareVariant(Rend_HaloTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    return 0;
}

static res::Source loadRaw(image_t &image, rawtex_t const &raw)
{
    // First try an external resource.
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri("Patches", Path(Str_Text(&raw.name))),
                                                     RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? res::External : res::None;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    if(raw.lumpNum >= 0)
    {
        filehandle_s *file = F_OpenLump(raw.lumpNum);
        if(file)
        {
            if(Image_LoadFromFile(&image, file))
            {
                F_Delete(file);
                return res::Original;
            }

            // It must be an old-fashioned "raw" image.
#define RAW_WIDTH           320
#define RAW_HEIGHT          200

            Image_Init(&image);

            size_t fileLength = FileHandle_Length(file);
            size_t bufSize = 3 * RAW_WIDTH * RAW_HEIGHT;

            image.pixels = (uint8_t *) M_Malloc(bufSize);
            if(fileLength < bufSize)
                std::memset(image.pixels, 0, bufSize);

            // Load the raw image data.
            FileHandle_Read(file, image.pixels, fileLength);
            image.size.width = RAW_WIDTH;
            image.size.height = int(fileLength / image.size.width);
            image.pixelSize = 1;

            F_Delete(file);
            return res::Original;

#undef RAW_HEIGHT
#undef RAW_WIDTH
        }
    }

    return res::None;
}

DGLuint GL_PrepareRawTexture(rawtex_t &raw)
{
    if(raw.lumpNum < 0 || raw.lumpNum >= F_LumpCount()) return 0;

    if(!raw.tex)
    {
        image_t image;
        Image_Init(&image);

        if(loadRaw(image, raw) == res::External)
        {
            // Loaded an external raw texture.
            raw.tex = GL_NewTextureWithParams(image.pixelSize == 4? DGL_RGBA : DGL_RGB,
                image.size.width, image.size.height, image.pixels, 0, 0,
                GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }
        else
        {
            raw.tex = GL_NewTextureWithParams(
                (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 :
                          image.pixelSize == 4? DGL_RGBA :
                          image.pixelSize == 3? DGL_RGB : DGL_COLOR_INDEX_8,
                image.size.width, image.size.height, image.pixels, 0, 0,
                GL_NEAREST, (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        }

        raw.width  = image.size.width;
        raw.height = image.size.height;
        Image_Destroy(&image);
    }

    return raw.tex;
}

void GL_SetRawTexturesMinFilter(int newMinFilter)
{
    rawtex_t **rawTexs = R_CollectRawTexs();
    for(rawtex_t **ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t *r = *ptr;
        if(r->tex) // Is the texture loaded?
        {
            DENG_ASSERT_IN_MAIN_THREAD();
            DENG_ASSERT_GL_CONTEXT_ACTIVE();

            glBindTexture(GL_TEXTURE_2D, r->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, newMinFilter);
        }
    }
    Z_Free(rawTexs);
}

void GL_DoUpdateTexParams()
{
    int newMinFilter = glmode[mipmapping];

    GL_SetAllTexturesMinFilter(newMinFilter);
    GL_SetRawTexturesMinFilter(newMinFilter);
}

static int reloadTextures(void *context)
{
    bool const usingBusyMode = *static_cast<bool *>(context);

    /// @todo re-upload ALL textures currently in use.
    GL_LoadSystemTextures();
    Rend_ParticleLoadExtraTextures();

    if(usingBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

void GL_TexReset()
{
    App_ResourceSystem().releaseAllGLTextures();
    LOG_MSG("All DGL textures deleted.");

    bool useBusyMode = !BusyMode_Active();
    if(useBusyMode)
    {
        Con_InitProgress(200);
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    reloadTextures, &useBusyMode, "Reseting textures...");
    }
    else
    {
        reloadTextures(&useBusyMode);
    }
}

void GL_DoUpdateTexGamma()
{
    if(initedOk)
    {
        R_BuildTexGammaLut();
        GL_TexReset();
    }

    LOG_MSG("Gamma correction set to %f.") << texGamma;
}

void GL_DoTexReset()
{
    GL_TexReset();
}

void GL_DoResetDetailTextures()
{
    App_ResourceSystem().releaseGLTexturesByScheme("Details");
}

void GL_ReleaseTexturesForRawImages()
{
    rawtex_t **rawTexs = R_CollectRawTexs();
    for(rawtex_t **ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t *r = (*ptr);
        if(r->tex)
        {
            glDeleteTextures(1, (GLuint const *) &r->tex);
            r->tex = 0;
        }
    }
    Z_Free(rawTexs);
}

void GL_SetAllTexturesMinFilter(int /*minFilter*/)
{
    /// @todo This is no longer correct logic. Changing the global minification
    ///       filter should not modify the uploaded texture content.
}

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    uint8_t const *pixels, int flags)
{
    texturecontent_t c;

    GL_InitTextureContent(&c);
    c.name = GL_GetReservedTextureName();
    c.format = format;
    c.width = width;
    c.height = height;
    c.pixels = pixels;
    c.flags = flags;

    uploadContentUnmanaged(c);
    return c.name;
}

DGLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    uint8_t const *pixels, int flags, int grayMipmap, int minFilter, int magFilter,
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

    uploadContentUnmanaged(c);
    return c.name;
}

D_CMD(LowRes)
{
    DENG2_UNUSED3(src, argv, argc);

    // Set everything as low as they go.
    filterSprites = 0;
    filterUI      = 0;
    texMagMode    = 0;

    GL_SetAllTexturesMinFilter(GL_NEAREST);
    GL_SetRawTexturesMinFilter(GL_NEAREST);

    // And do a texreset so everything is updated.
    GL_TexReset();
    return true;
}

D_CMD(TexReset)
{
    DENG2_UNUSED(src);

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
    DENG2_UNUSED2(src, argc);

    int newMipMode = String(argv[1]).toInt();
    if(newMipMode < 0 || newMipMode > 5)
    {
        Con_Message("Invalid mipmapping mode %i specified. Valid range is [0..5).", newMipMode);
        return false;
    }

    mipmapping = newMipMode;
    GL_SetAllTexturesMinFilter(glmode[mipmapping]);
    return true;
}
