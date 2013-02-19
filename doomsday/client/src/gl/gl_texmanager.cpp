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

#include <cstdlib>
#include <cstring> // memset, memcpy
#include <cctype>
#include <cmath>

#ifdef UNIX
#   include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_resource.h"
#include "de_play.h"
#include "de_ui.h"

#include "def_main.h"

#include <QSize>
#include <de/ByteRefArray>
#include <de/mathutil.h>
#include <de/memory.h>
#include <de/memoryzone.h>

using namespace de;

int monochrome = 0; // desaturate a patch (average colours)
int mipmapping = 5;
int filterUI   = 1;
int texQuality = TEXQ_BEST;
int upscaleAndSharpenPatches = 0;

#ifdef __CLIENT__

enum uploadcontentmethod_t
{
    METHOD_IMMEDIATE = 0,
    METHOD_DEFERRED
};

struct GraphicFileType
{
    /// Symbolic name of the resource type.
    String name;

    /// Known file extension.
    String ext;

    bool (*interpretFunc)(de::FileHandle &hndl, String filePath, image_t &img);

    char const *(*getLastErrorFunc)(); ///< Can be NULL.
};

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

static bool interpretPcx(de::FileHandle &hndl, String filePath, image_t &img);
static bool interpretPng(de::FileHandle &hndl, String filePath, image_t &img);
static bool interpretJpg(de::FileHandle &hndl, String filePath, image_t &img);
static bool interpretTga(de::FileHandle &hndl, String filePath, image_t &img);

static TexSource loadExternalTexture(image_t &image, String searchPath, String optionalSuffix = "");

static TexSource loadFlat(image_t &image, de::FileHandle &file);
static TexSource loadPatch(image_t &image, de::FileHandle &file, int tclass = 0, int tmap = 0, int border = 0);
static TexSource loadDetail(image_t &image, de::FileHandle &file);

static TexSource loadRaw(image_t &image, rawtex_t const &raw);
static TexSource loadPatchComposite(image_t &image, Texture &tex,
    bool zeroMask = false, bool useZeroOriginIfOneComponent = false);

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

// Names of the UI textures.
DGLuint uiTextures[NUM_UITEXTURES];

static boolean initedOk = false; // Init done.

// Graphic resource types.
static GraphicFileType const graphicTypes[] = {
    { "PNG",    "png",      interpretPng, 0 },
    { "JPG",    "jpg",      interpretJpg, 0 }, // TODO: add alternate "jpeg" extension
    { "TGA",    "tga",      interpretTga, TGA_LastError },
    { "PCX",    "pcx",      interpretPcx, PCX_LastError },
    { "",       "",         0,            0 } // Terminate.
};

static variantspecificationlist_t *variantSpecs;

/// @c TST_DETAIL type specifications are stored separately into a set of
/// buckets. Bucket selection is determined by their quantized contrast value.
#define DETAILVARIANT_CONTRAST_HASHSIZE     (DETAILTEXTURE_CONTRAST_QUANTIZATION_FACTOR+1)
static variantspecificationlist_t *detailVariantSpecs[DETAILVARIANT_CONTRAST_HASHSIZE];

void GL_TexRegister()
{
#ifdef __CLIENT__

    C_VAR_INT   ("rend-tex",                    &renderTextures,     CVF_NO_ARCHIVE, 0, 2);
    C_VAR_INT   ("rend-tex-detail",             &r_detail,           0, 0, 1);
    C_VAR_INT   ("rend-tex-detail-multitex",    &useMultiTexDetails, 0, 0, 1);
    C_VAR_FLOAT ("rend-tex-detail-scale",       &detailScale,        CVF_NO_MIN | CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT2("rend-tex-detail-strength",    &detailFactor,       0, 0, 10, GL_DoResetDetailTextures);
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

#endif

    Textures::consoleRegister();
}

GLint GL_MinFilterForVariantSpec(variantspecification_t const &spec)
{
    if(spec.minFilter >= 0) // Constant logical value.
    {
        return (spec.mipmapped? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST) + spec.minFilter;
    }
    // "No class" preference.
    return spec.mipmapped? glmode[mipmapping] : GL_LINEAR;
}

GLint GL_MagFilterForVariantSpec(variantspecification_t const &spec)
{
    if(spec.magFilter >= 0) // Constant logical value.
    {
        return GL_NEAREST + spec.magFilter;
    }

    // Preference for texture class id.
    switch(abs(spec.magFilter)-1)
    {
    case 1: // Sprite class.
        return filterSprites? GL_LINEAR : GL_NEAREST;
    case 2: // UI class.
        return filterUI? GL_LINEAR : GL_NEAREST;
    default: // "No class" preference.
        return glmode[texMagMode];
    }
}

int GL_LogicalAnisoLevelForVariantSpec(variantspecification_t const &spec)
{
    return spec.anisoFilter < 0? texAniso : spec.anisoFilter;
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

static int findTextureUsingVariantSpecificationWorker(Texture &texture, void *parameters)
{
    texturevariantspecification_t *spec = (texturevariantspecification_t *)parameters;
    foreach(TextureVariant *variant, texture.variants())
    {
        if(&variant->spec() == spec)
        {
            return 1; // Found one; stop.
        }
    }
    return 0; // Continue iteration.
}

static int pruneUnusedVariantSpecificationsInList(variantspecificationlist_t *list)
{
    texturevariantspecificationlist_node_t *node = list;
    int numPruned = 0;
    while(node)
    {
        texturevariantspecificationlist_node_t *next = node->next;
        if(!App_Textures().iterate(findTextureUsingVariantSpecificationWorker, (void *)node->spec))
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

static uploadcontentmethod_t chooseContentUploadMethod(texturecontent_t const &content)
{
    // Must the operation be carried out immediately?
    if((content.flags & TXCF_NEVER_DEFER) || !BusyMode_Active())
    {
        return METHOD_IMMEDIATE;
    }
    // We can defer.
    return METHOD_DEFERRED;
}

static int releaseVariantGLTexture(TextureVariant &variant, texturevariantspecification_t *spec = 0)
{
    if(!spec || spec == &variant.spec())
    {
        if(variant.isUploaded())
        {
            // Delete and mark it not-loaded.
            DGLuint glName = variant.glName();
            glDeleteTextures(1, (GLuint const *) &glName);
            variant.setGLName(0);
            variant.setFlags(TextureVariant::Uploaded, false);
        }

        if(spec) return true; // We're done.
    }
    return 0; // Continue iteration.
}

static void uploadContent(uploadcontentmethod_t uploadMethod, texturecontent_t const &content)
{
    if(METHOD_IMMEDIATE == uploadMethod)
    {
        // Do this right away. No need to take a copy.
        GL_UploadTextureContent(content);
        return;
    }
    GL_DeferTextureUpload(&content);
}

static uploadcontentmethod_t uploadContentForVariant(uploadcontentmethod_t uploadMethod,
    texturecontent_t const &content, TextureVariant &variant)
{
    if(!novideo)
    {
        uploadContent(uploadMethod, content);
    }
    variant.setFlags(TextureVariant::Uploaded, true);
    return uploadMethod;
}

static void uploadContentUnmanaged(uploadcontentmethod_t uploadMethod,
    texturecontent_t const &content)
{
    LOG_AS("uploadContentUnmanaged");
    if(novideo) return;
    if(METHOD_IMMEDIATE == uploadMethod)
    {
#ifdef _DEBUG
        LOG_VERBOSE("Uploading texture (%i:%ix%i) while not busy! Should be precached in busy mode?")
            << content.name << content.width << content.height;
#endif
    }
    uploadContent(uploadMethod, content);
}

static TexSource loadSourceImage(de::Texture &tex, texturevariantspecification_t const &baseSpec,
    image_t &image)
{
    TexSource source = TEXS_NONE;
    variantspecification_t const &spec = TS_GENERAL(baseSpec);
    if(!tex.manifest().schemeName().compareWithoutCase("Textures"))
    {
        // Attempt to load an external replacement for this composite texture?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.flags().testFlag(de::Texture::Custom)))
        {
            // First try the textures scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if(source == TEXS_NONE)
        {
            if(TC_SKYSPHERE_DIFFUSE != spec.context)
            {
                source = loadPatchComposite(image, tex);
            }
            else
            {
                bool const zeroMask = !!(spec.flags & TSF_ZEROMASK);
                bool const useZeroOriginIfOneComponent = true;
                source = loadPatchComposite(image, tex, zeroMask, useZeroOriginIfOneComponent);
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Flats"))
    {
        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.flags().testFlag(de::Texture::Custom)))
        {
            // First try the flats scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");

            if(source == TEXS_NONE)
            {
                // How about the old-fashioned "flat-name" in the textures scheme?
                source = loadExternalTexture(image, "Textures:flat-" + uri.path().toStringRef(), "-ck");
            }
        }

        if(source == TEXS_NONE)
        {
            de::Uri const &resourceUri = tex.manifest().resourceUri();
            if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
            {
                try
                {
                    lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                    de::File1 &lump = App_FileSystem()->nameIndex().lump(lumpNum);
                    de::FileHandle &hndl = App_FileSystem()->openLump(lump);

                    source = loadFlat(image, hndl);

                    App_FileSystem()->releaseFile(hndl.file());
                    delete &hndl;
                }
                catch(LumpIndex::NotFoundError const&)
                {} // Ignore this error.
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Patches"))
    {
        int tclass = 0, tmap = 0;
        if(spec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            DENG_ASSERT(spec.translated);
            tclass = spec.translated->tClass;
            tmap   = spec.translated->tMap;
        }

        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.flags().testFlag(de::Texture::Custom)))
        {
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if(source == TEXS_NONE)
        {
            de::Uri const &resourceUri = tex.manifest().resourceUri();
            if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
            {
                try
                {
                    lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                    de::File1 &lump = App_FileSystem()->nameIndex().lump(lumpNum);
                    de::FileHandle &hndl = App_FileSystem()->openLump(lump);

                    source = loadPatch(image, hndl, tclass, tmap, spec.border);

                    App_FileSystem()->releaseFile(hndl.file());
                    delete &hndl;
                }
                catch(LumpIndex::NotFoundError const&)
                {} // Ignore this error.
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Sprites"))
    {
        int tclass = 0, tmap = 0;
        if(spec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            DENG_ASSERT(spec.translated);
            tclass = spec.translated->tClass;
            tmap   = spec.translated->tMap;
        }

        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            de::Uri uri = tex.manifest().composeUri();

            // Prefer psprite or translated versions if available.
            if(TC_PSPRITE_DIFFUSE == spec.context)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path() + "-hud", "-ck");
            }
            else if(tclass || tmap)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path() + String("-table%1%2").arg(tclass).arg(tmap), "-ck");
            }

            if(!source)
            {
                source = loadExternalTexture(image, "Patches:" + uri.path(), "-ck");
            }
        }

        if(source == TEXS_NONE)
        {
            de::Uri const &resourceUri = tex.manifest().resourceUri();
            if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
            {
                try
                {
                    lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                    de::File1 &lump = App_FileSystem()->nameIndex().lump(lumpNum);
                    de::FileHandle &hndl = App_FileSystem()->openLump(lump);

                    source = loadPatch(image, hndl, tclass, tmap, spec.border);

                    App_FileSystem()->releaseFile(hndl.file());
                    delete &hndl;
                }
                catch(LumpIndex::NotFoundError const&)
                {} // Ignore this error.
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Details"))
    {
        de::Uri const &resourceUri = tex.manifest().resourceUri();
        if(resourceUri.scheme().compareWithoutCase("Lumps"))
        {
            source = loadExternalTexture(image, resourceUri.compose());
        }
        else
        {
            lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(resourceUri.path());
            try
            {
                de::File1 &lump = App_FileSystem()->nameIndex().lump(lumpNum);
                de::FileHandle &hndl = App_FileSystem()->openLump(lump);

                source = loadDetail(image, hndl);

                App_FileSystem()->releaseFile(hndl.file());
                delete &hndl;
            }
            catch(LumpIndex::NotFoundError const&)
            {} // Ignore this error.
        }
    }
    else
    {
        de::Uri const &resourceUri = tex.manifest().resourceUri();
        source = loadExternalTexture(image, resourceUri.compose());
    }
    return source;
}

static uploadcontentmethod_t prepareVariantFromImage(TextureVariant &tex, image_t &image)
{
    variantspecification_t const &spec = TS_GENERAL(tex.spec());
    bool monochrome    = (spec.flags & TSF_MONOCHROME) != 0;
    bool noCompression = (spec.flags & TSF_NO_COMPRESSION) != 0;
    bool scaleSharp    = (spec.flags & TSF_UPSCALE_AND_SHARPEN) != 0;
    int wrapS = spec.wrapS, wrapT = spec.wrapT;
    int magFilter, minFilter, anisoFilter, flags = 0;
    boolean noSmartFilter = false;
    dgltexformat_t dglFormat;
    texturecontent_t c;
    float s, t;

    if(spec.toAlpha)
    {
        if(0 != image.paletteId)
        {
            // Paletted.
            uint8_t *newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
                                                  ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                                                  R_ToColorPalette(image.paletteId), 3);
            M_Free(image.pixels);
            image.pixels = newPixels;
            image.pixelSize = 3;
            image.paletteId = 0;
            image.flags &= ~IMGF_IS_MASKED;
        }

        Image_ConvertToLuminance(&image, false);
        long total = image.size.width * image.size.height;
        for(long i = 0; i < total; ++i)
        {
            image.pixels[total + i] = image.pixels[i];
            image.pixels[i] = 255;
        }
        image.pixelSize = 2;
    }
    else if(0 != image.paletteId)
    {
        if(fillOutlines && (image.flags & IMGF_IS_MASKED))
            ColorOutlinesIdx(image.pixels, image.size.width, image.size.height);

        if(monochrome && !scaleSharp)
            GL_DeSaturatePalettedImage(image.pixels, R_ToColorPalette(image.paletteId),
                                       image.size.width, image.size.height);

        if(scaleSharp)
        {
            int scaleMethod = GL_ChooseSmartFilter(image.size.width, image.size.height, 0);
            bool origMasked = (image.flags & IMGF_IS_MASKED) != 0;
            colorpaletteid_t origPaletteId = image.paletteId;

            uint8_t* newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
                                                  ((image.flags & IMGF_IS_MASKED)? 2 : 1),
                                                  R_ToColorPalette(image.paletteId), 4);
            if(newPixels != image.pixels)
            {
                M_Free(image.pixels);
                image.pixels = newPixels;
                image.pixelSize = 4;
                image.paletteId = 0;
                image.flags &= ~IMGF_IS_MASKED;
            }

            if(monochrome)
                Desaturate(image.pixels, image.size.width, image.size.height, image.pixelSize);

            newPixels = GL_SmartFilter(scaleMethod, image.pixels, image.size.width, image.size.height,
                                       0, &image.size.width, &image.size.height);
            if(newPixels != image.pixels)
            {
                M_Free(image.pixels);
                image.pixels = newPixels;
            }

            EnhanceContrast(image.pixels, image.size.width, image.size.height, image.pixelSize);
            //SharpenPixels(image.pixels, image.size.width, image.size.height, image.pixelSize);
            //BlackOutlines(image.pixels, image.size.width, image.size.height, image.pixelSize);

            // Back to paletted+alpha?
            if(monochrome)
            {
                // No. We'll convert from RGB(+A) to Luminance(+A) and upload as is.
                // Replace the old buffer.
                Image_ConvertToLuminance(&image, true);
                AmplifyLuma(image.pixels, image.size.width, image.size.height, image.pixelSize == 2);
            }
            else
            {   // Yes. Quantize down from RGA(+A) to Paletted(+A), replacing the old image.
                newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
                                             (origMasked? 2 : 1), R_ToColorPalette(origPaletteId), 4);
                if(newPixels != image.pixels)
                {
                    M_Free(image.pixels);
                    image.pixels = newPixels;
                    image.pixelSize = (origMasked? 2 : 1);
                    image.paletteId = origPaletteId;
                    if(origMasked)
                        image.flags |= IMGF_IS_MASKED;
                }
            }

            // Lets not do this again.
            noSmartFilter = true;
        }
    }
    else if(image.pixelSize > 2)
    {
        if(monochrome)
        {
            Image_ConvertToLuminance(&image, true);
            AmplifyLuma(image.pixels, image.size.width, image.size.height, image.pixelSize == 2);
        }
    }

    if(noCompression || (image.size.width < 128 || image.size.height < 128))
        flags |= TXCF_NO_COMPRESSION;

    if(spec.gammaCorrection) flags |= TXCF_APPLY_GAMMACORRECTION;
    if(spec.noStretch)       flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
    if(spec.mipmapped)       flags |= TXCF_MIPMAP;
    if(noSmartFilter)         flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

    if(monochrome)
    {
        dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE );
    }
    else
    {
        if(0 != image.paletteId)
        {   // Paletted.
            dglFormat = (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8;
        }
        else
        {
            dglFormat = ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                          image.pixelSize == 3 ? DGL_RGB :
                          image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE );
        }
    }

    minFilter   = GL_MinFilterForVariantSpec(spec);
    magFilter   = GL_MagFilterForVariantSpec(spec);
    anisoFilter = GL_LogicalAnisoLevelForVariantSpec(spec);

    /**
     * Calculate texture coordinates based on the image dimensions. The
     * coordinates are calculated as width/CeilPow2(width), or 1 if larger
     * than the maximum texture size.
     *
     * @todo Image dimensions may not be the same as the final uploaded texture!
     */
    if((flags & TXCF_UPLOAD_ARG_NOSTRETCH) &&
       (!GL_state.features.texNonPowTwo || spec.mipmapped))
    {
        int pw = M_CeilPow2(image.size.width), ph = M_CeilPow2(image.size.height);
        s =  image.size.width / (float) pw;
        t = image.size.height / (float) ph;
    }
    else
    {
        s = t = 1;
    }

    tex.setCoords(s, t);
    tex.setFlags(TextureVariant::Masked, !!(image.flags & IMGF_IS_MASKED));

    GL_InitTextureContent(&c);
    c.name = tex.glName();
    c.format = dglFormat;
    c.width = image.size.width;
    c.height = image.size.height;
    c.pixels = image.pixels;
    c.paletteId = image.paletteId;
    c.flags = flags;
    c.magFilter = magFilter;
    c.minFilter = minFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0] = wrapS;
    c.wrap[1] = wrapT;

    return uploadContentForVariant(chooseContentUploadMethod(c), c, tex);
}

static uploadcontentmethod_t prepareDetailVariantFromImage(TextureVariant &tex, image_t &image)
{
    detailvariantspecification_t const &spec = TS_DETAIL(tex.spec());
    float baMul, hiMul, loMul, s, t;
    int grayMipmapFactor, pw, ph, flags = 0;
    texturecontent_t c;

    grayMipmapFactor = spec.contrast;

    // We only want a luminance map.
    if(image.pixelSize > 2)
    {
        Image_ConvertToLuminance(&image, false);
    }

    // Try to normalize the luminance data so it works expectedly as a detail texture.
    EqualizeLuma(image.pixels, image.size.width, image.size.height, &baMul, &hiMul, &loMul);
    if(baMul != 1 || hiMul != 1 || loMul != 1)
    {
        // Integrate the normalization factor with contrast.
        float const hiContrast = 1 - 1. / hiMul;
        float const loContrast = 1 - loMul;
        float const shift = ((hiContrast + loContrast) / 2);
        grayMipmapFactor = (uint8_t)(255 * MINMAX_OF(0, spec.contrast / 255.f - shift, 1));

        // Announce the normalization.
        de::Uri uri = tex.generalCase().manifest().composeUri();
        LOG_VERBOSE("Normalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).")
            << uri << baMul << hiMul << loMul;
    }

    // Disable compression?
    if(image.size.width < 128 || image.size.height < 128)
        flags |= TXCF_NO_COMPRESSION;

    // Calculate prepared texture coordinates.
    pw = M_CeilPow2(image.size.width);
    ph = M_CeilPow2(image.size.height);
    s =  image.size.width / (float) pw;
    t = image.size.height / (float) ph;
    tex.setCoords(s, t);

    GL_InitTextureContent(&c);
    c.name = tex.glName();
    c.format = DGL_LUMINANCE;
    c.flags = flags | TXCF_GRAY_MIPMAP | TXCF_UPLOAD_ARG_NOSMARTFILTER;
    c.grayMipmap = grayMipmapFactor;
    c.width = image.size.width;
    c.height = image.size.height;
    c.pixels = image.pixels;
    c.anisoFilter = texAniso;
    c.magFilter = glmode[texMagMode];
    c.minFilter = GL_LINEAR_MIPMAP_LINEAR;
    c.wrap[0] = GL_REPEAT;
    c.wrap[1] = GL_REPEAT;

    return uploadContentForVariant(chooseContentUploadMethod(c), c, tex);
}

void GL_EarlyInitTextureManager()
{
    GL_InitSmartFilterHQ2x();

    variantSpecs = NULL;
    memset(detailVariantSpecs, 0, sizeof(detailVariantSpecs));
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
    std::memset(uiTextures, 0, sizeof(uiTextures));

    // Initialization done.
    initedOk = true;
}

void GL_ResetTextureManager()
{
    if(!initedOk) return;
    GL_ReleaseTextures();
    GL_PruneTextureVariantSpecifications();
    GL_LoadSystemTextures();
}

void GL_PrintTextureVariantSpecification(texturevariantspecification_t const &baseSpec)
{
    static char const *textureUsageContextNames[1 + TEXTUREVARIANTUSAGECONTEXT_COUNT] = {
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
    static char const *textureSpecificationTypeNames[TEXTUREVARIANTSPECIFICATIONTYPE_COUNT] = {
        /* TST_GENERAL */   "general",
        /* TST_DETAIL */    "detail"
    };
    static char const *filterModeNames[] = { "ui", "sprite", "noclass", "const" };
    static char const *glFilterNames[] = {
        "nearest", "linear", "nearest_mipmap_nearest", "linear_mipmap_nearest",
        "nearest_mipmap_linear", "linear_mipmap_linear"
    };

    Con_Printf("type:%s", textureSpecificationTypeNames[baseSpec.type]);

    switch(baseSpec.type)
    {
    case TST_DETAIL: {
        detailvariantspecification_t const &spec = TS_DETAIL(baseSpec);
        Con_Printf(" contrast:%i%%\n", int(.5f + spec.contrast / 255.f * 100));
        break; }

    case TST_GENERAL: {
        variantspecification_t const &spec = TS_GENERAL(baseSpec);
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

        Con_Printf(" context:%s flags:%i border:%i\n"
                   "    minFilter:(%s|%s) magFilter:(%s|%s) anisoFilter:%i",
                   textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1],
                   (spec.flags & ~TSF_INTERNAL_MASK), spec.border,
                   filterModeNames[3 + MINMAX_OF(-1, spec.minFilter, 0)],
                   glFilterNames[glMinFilterNameIdx],
                   filterModeNames[3 + MINMAX_OF(-3, spec.magFilter, 0)],
                   glFilterNames[glMagFilterNameIdx],
                   spec.anisoFilter);

        if(spec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            colorpalettetranslationspecification_t const *cpt = spec.translated;
            DENG_ASSERT(cpt);
            Con_Printf(" translated:(tclass:%i tmap:%i)", cpt->tClass, cpt->tMap);
        }

        Con_Printf("\n");
        break; }
    }
}

texturevariantspecification_t &GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    if(!initedOk) Con_Error("GL_TextureVariantSpecificationForContext: GL texture manager not yet initialized.");

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

texturevariantspecification_t &GL_DetailTextureVariantSpecificationForContext(
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

    // Preload all UI textures.
    for(int i = 0; i < NUM_UITEXTURES; ++i)
    {
        GL_PrepareUITexture(uitexid_t(i));
    }

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

void GL_ReleaseSystemTextures()
{
    if(novideo || !initedOk) return;

    LOG_VERBOSE("Releasing System textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    glDeleteTextures(NUM_LIGHTING_TEXTURES, (GLuint const *) lightingTextures);
    std::memset(lightingTextures, 0, sizeof(lightingTextures));

    glDeleteTextures(NUM_SYSFLARE_TEXTURES, (GLuint const *) sysFlareTextures);
    std::memset(sysFlareTextures, 0, sizeof(sysFlareTextures));

    glDeleteTextures(NUM_UITEXTURES, (GLuint const *) uiTextures);
    std::memset(uiTextures, 0, sizeof(uiTextures));

    GL_ReleaseTexturesByScheme("System");
    Rend_ParticleReleaseSystemTextures();
    Fonts_ReleaseSystemTextures();

    GL_PruneTextureVariantSpecifications();
}

void GL_ReleaseRuntimeTextures()
{
    if(novideo || !initedOk) return;

    LOG_VERBOSE("Releasing Runtime textures...");

    // The rendering lists contain persistent references to texture names.
    // Which, obviously, can't persist any longer...
    RL_DeleteLists();

    // texture-wrapped GL textures; textures, flats, sprites...
    GL_ReleaseTexturesByScheme("Flats");
    GL_ReleaseTexturesByScheme("Textures");
    GL_ReleaseTexturesByScheme("Patches");
    GL_ReleaseTexturesByScheme("Sprites");
    GL_ReleaseTexturesByScheme("Details");
    GL_ReleaseTexturesByScheme("Reflections");
    GL_ReleaseTexturesByScheme("Masks");
    GL_ReleaseTexturesByScheme("ModelSkins");
    GL_ReleaseTexturesByScheme("ModelReflectionSkins");
    GL_ReleaseTexturesByScheme("Lightmaps");
    GL_ReleaseTexturesByScheme("Flaremaps");
    GL_ReleaseTexturesForRawImages();

    Rend_ParticleReleaseExtraTextures();
    Fonts_ReleaseRuntimeTextures();

    GL_PruneTextureVariantSpecifications();
}

void GL_ReleaseTextures()
{
    if(!initedOk) return;
    GL_ReleaseRuntimeTextures();
    GL_ReleaseSystemTextures();
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

static bool interpretPcx(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(&img);
    img.pixels = PCX_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
}

static bool interpretJpg(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    return Image_LoadFromFileWithFormat(&img, "JPG", reinterpret_cast<filehandle_s *>(&hndl));
}

static bool interpretPng(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    /*
    Image_Init(&img);
    img.pixels = PNG_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
    */
    return Image_LoadFromFileWithFormat(&img, "PNG", reinterpret_cast<filehandle_s *>(&hndl));
}

static bool interpretTga(de::FileHandle &hndl, String /*filePath*/, image_t &img)
{
    Image_Init(&img);
    img.pixels = TGA_Load(reinterpret_cast<filehandle_s *>(&hndl),
                          &img.size.width, &img.size.height, &img.pixelSize);
    return (0 != img.pixels);
}

GraphicFileType const *guessGraphicFileTypeFromFileName(String fileName)
{
    // The path must have an extension for this.
    String ext = fileName.fileNameExtension();
    if(!ext.isEmpty())
    {
        for(int i = 0; !graphicTypes[i].ext.isEmpty(); ++i)
        {
            GraphicFileType const &type = graphicTypes[i];
            if(!ext.compareWithoutCase(type.ext))
            {
                return &type;
            }
        }
    }
    return 0; // Unknown.
}

static void interpretGraphic(de::FileHandle &hndl, String filePath, image_t &img)
{
    // Firstly try the interpreter for the guessed resource types.
    GraphicFileType const *rtypeGuess = guessGraphicFileTypeFromFileName(filePath);
    if(rtypeGuess)
    {
        rtypeGuess->interpretFunc(hndl, filePath, img);
    }

    // If not yet interpreted - try each recognisable format in order.
    if(!img.pixels)
    {
        // Try each recognisable format instead.
        /// @todo Order here should be determined by the resource locator.
        for(int i = 0; !graphicTypes[i].name.isEmpty(); ++i)
        {
            GraphicFileType const *graphicType = &graphicTypes[i];

            // Already tried this?
            if(graphicType == rtypeGuess) continue;

            graphicTypes[i].interpretFunc(hndl, filePath, img);
            if(img.pixels) break;
        }
    }
}

/// @return  @c true if the file name in @a path ends with the "color key" suffix.
static inline bool isColorKeyed(String path)
{
    return path.fileNameWithoutExtension().endsWith("-ck", Qt::CaseInsensitive);
}

uint8_t *Image_LoadFromFile(image_t *img, filehandle_s *_file)
{
#ifdef __CLIENT__

    DENG_ASSERT(img && _file);
    de::FileHandle &file = reinterpret_cast<de::FileHandle &>(*_file);
    LOG_AS("Image_LoadFromFile");

    String filePath = file.file().composePath();

    Image_Init(img);
    interpretGraphic(file, filePath, *img);

    // Still not interpreted?
    if(!img->pixels)
    {
        LOG_VERBOSE("\"%s\" unrecognized, trying fallback loader...")
            << NativePath(filePath).pretty();
        return NULL; // Not a recognised format. It may still be loadable, however.
    }

    // How about some color-keying?
    if(isColorKeyed(filePath))
    {
        uint8_t *out = ApplyColorKeying(img->pixels, img->size.width, img->size.height, img->pixelSize);
        if(out != img->pixels)
        {
            // Had to allocate a larger buffer, free the old and attach the new.
            M_Free(img->pixels);
            img->pixels = out;
        }

        // Color keying is done; now we have 4 bytes per pixel.
        img->pixelSize = 4;
    }

    // Any alpha pixels?
    if(Image_HasAlpha(img))
    {
        img->flags |= IMGF_IS_MASKED;
    }

    LOG_VERBOSE("\"%s\" (%ix%i)")
        << NativePath(filePath).pretty() << img->size.width << img->size.height;

    return img->pixels;

#else
    DENG_UNUSED(img);
    DENG_UNUSED(_file);
    return NULL;
#endif
}

uint8_t *GL_LoadImage(image_t &image, String nativePath)
{
    try
    {
        // Relative paths are relative to the native working directory.
        String path = (NativePath::workPath() / NativePath(nativePath).expand()).withSeparators('/');

        de::FileHandle &hndl = App_FileSystem()->openFile(path, "rb");
        uint8_t *pixels = Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl));

        App_FileSystem()->releaseFile(hndl.file());
        delete &hndl;

        return pixels;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.

    return 0; // Not loaded.
}

#ifdef __CLIENT__

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

/// @note Texture parameters will NOT be set here!
void GL_UploadTextureContent(texturecontent_t const &content)
{
    bool generateMipmaps = !!(content.flags & (TXCF_MIPMAP|TXCF_GRAY_MIPMAP));
    bool applyTexGamma   = !!(content.flags & TXCF_APPLY_GAMMACORRECTION);
    bool noCompression   = !!(content.flags & TXCF_NO_COMPRESSION);
    bool noSmartFilter   = !!(content.flags & TXCF_UPLOAD_ARG_NOSMARTFILTER);
    bool noStretch       = !!(content.flags & TXCF_UPLOAD_ARG_NOSTRETCH);

    int loadWidth = content.width, loadHeight = content.height;
    uint8_t const *loadPixels = content.pixels;
    dgltexformat_t dglFormat = content.format;

    if(DGL_COLOR_INDEX_8 == dglFormat || DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat)
    {
        // Convert a paletted source image to truecolor.
        uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight,
            DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 2 : 1, R_ToColorPalette(content.paletteId),
            DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 4 : 3);
        if(loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = newPixels;
        dglFormat = DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? DGL_RGBA : DGL_RGB;
    }

    if(DGL_RGBA == dglFormat || DGL_RGB == dglFormat)
    {
        int comps = (DGL_RGBA == dglFormat ? 4 : 3);

        if(applyTexGamma && texGamma > .0001f)
        {
            uint8_t* dst, *localBuffer = 0;
            long const numPels = loadWidth * loadHeight;

            uint8_t const *src = loadPixels;
            if(loadPixels == content.pixels)
            {
                localBuffer = (uint8_t *) M_Malloc(comps * numPels);
                dst = localBuffer;
            }
            else
            {
                dst = const_cast<uint8_t *>(loadPixels);
            }

            for(long i = 0; i < numPels; ++i)
            {
                dst[CR] = texGammaLut[src[CR]];
                dst[CG] = texGammaLut[src[CG]];
                dst[CB] = texGammaLut[src[CB]];
                if(comps == 4)
                    dst[CA] = src[CA];

                dst += comps;
                src += comps;
            }

            if(localBuffer)
            {
                if(loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = localBuffer;
            }
        }

        if(useSmartFilter && !noSmartFilter)
        {
            if(comps == 3)
            {
                // Need to add an alpha channel.
                uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight, 3, 0, 4);
                if(loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = newPixels;
                dglFormat = DGL_RGBA;
            }

            uint8_t *filtered = GL_SmartFilter(GL_ChooseSmartFilter(loadWidth, loadHeight, 0),
                                               loadPixels, loadWidth, loadHeight, ICF_UPSCALE_SAMPLE_WRAP,
                                               &loadWidth, &loadHeight);
            if(filtered != loadPixels)
            {
                if(loadPixels != content.pixels)
                {
                    M_Free(const_cast<uint8_t *>(loadPixels));
                }
                loadPixels = filtered;
            }
        }
    }

    if(DGL_LUMINANCE_PLUS_A8 == dglFormat)
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(2 * numPixels);

        uint8_t *pixel = localBuffer;
        for(long i = 0; i < numPixels; ++i)
        {
            pixel[0] = loadPixels[i];
            pixel[1] = loadPixels[numPixels + i];
            pixel += 2;
        }

        if(loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
    }

    if(DGL_LUMINANCE == dglFormat && (content.flags & TXCF_CONVERT_8BIT_TO_ALPHA))
    {
        // Needs converting. This adds some overhead.
        long const numPixels = content.width * content.height;
        uint8_t *localBuffer = (uint8_t *) M_Malloc(2 * numPixels);

        // Move the average color to the alpha channel, make the actual color white.
        uint8_t *pixel = localBuffer;
        for(long i = 0; i < numPixels; ++i)
        {
            pixel[0] = 255;
            pixel[1] = loadPixels[i];
            pixel += 2;
        }

        if(loadPixels != content.pixels)
        {
            M_Free(const_cast<uint8_t *>(loadPixels));
        }
        loadPixels = localBuffer;
        dglFormat = DGL_LUMINANCE_PLUS_A8;
    }

    // Calculate the final dimensions for the texture, as required by
    // the graphics hardware and/or engine configuration.
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
            uint8_t *localBuffer = (uint8_t *) M_Calloc(comps * loadWidth * loadHeight);

            // Copy line by line.
            for(int i = 0; i < height; ++i)
            {
                std::memcpy(localBuffer + loadWidth * comps * i,
                            loadPixels  + width     * comps * i, comps * width);
            }

            if(loadPixels != content.pixels)
            {
                M_Free(const_cast<uint8_t *>(loadPixels));
            }
            loadPixels = localBuffer;
        }
        else
        {
            // Stretch into a new power-of-two texture.
            uint8_t *newPixels = GL_ScaleBuffer(loadPixels, width, height, comps, loadWidth, loadHeight);
            if(loadPixels != content.pixels)
            {
                M_Free(const_cast<uint8_t *>(loadPixels));
            }
            loadPixels = newPixels;
        }
    }

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, content.name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, content.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, content.magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, content.wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, content.wrap[1]);
    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(content.anisoFilter));

    if(!(content.flags & TXCF_GRAY_MIPMAP))
    {
        GLint loadFormat;
        switch(dglFormat)
        {
        case DGL_LUMINANCE_PLUS_A8: loadFormat = GL_LUMINANCE_ALPHA; break;
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB; break;
        case DGL_RGBA:              loadFormat = GL_RGBA; break;
        default:
            Con_Error("GL_UploadTextureContent: Unknown format %i.", int(dglFormat));
            exit(1);
        }

        GLint glFormat = ChooseTextureFormat(dglFormat, !noCompression);

        if(!GL_UploadTexture(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                             generateMipmaps ? true : false))
        {
            Con_Error("GL_UploadTextureContent: TexImage failed (%u:%ix%i fmt%i).",
                      content.name, loadWidth, loadHeight, int(dglFormat));
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
            Con_Error("GL_UploadTextureContent: Unknown format %i.", int(dglFormat));
            exit(1); // Unreachable.
        }

        glFormat = ChooseTextureFormat(DGL_LUMINANCE, !noCompression);

        if(!GL_UploadTextureGrayMipmap(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                                       content.grayMipmap * reciprocal255))
        {
            Con_Error("GL_UploadTextureContent: TexImageGrayMipmap failed (%u:%ix%i fmt%i).",
                      content.name, loadWidth, loadHeight, int(dglFormat));
        }
    }

    if(loadPixels != content.pixels)
    {
        M_Free(const_cast<uint8_t *>(loadPixels));
    }
}

static TexSource loadExternalTexture(image_t &image, String searchPath,
    String optionalSuffix)
{
    AutoStr *foundPath = AutoStr_NewStd();
    // First look for a version with an optional suffix.
    de::Uri search = de::Uri(Path(searchPath) + optionalSuffix, RC_GRAPHIC);
    bool found = F_FindPath(RC_GRAPHIC, reinterpret_cast<uri_s *>(&search), foundPath);

    // Try again without the suffix?
    if(!found && !optionalSuffix.empty())
    {
        search.setUri(Path(searchPath), RC_GRAPHIC);
        found = F_FindPath(RC_GRAPHIC, reinterpret_cast<uri_s *>(&search), foundPath);
    }

    if(!found || !GL_LoadImage(image, Str_Text(foundPath)))
    {
        return TEXS_NONE;
    }

    return TEXS_EXTERNAL;
}

DGLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    if(novideo) return 0;
    if(which < 0 || which >= NUM_LIGHTING_TEXTURES) return 0;

    static const struct TexDef {
        char const *name;
        gfxmode_t mode;
        int wrapS, wrapT;
    } texDefs[NUM_LIGHTING_TEXTURES] = {
        /* LST_DYNAMIC */         { "dlight",     LGM_WHITE_ALPHA,    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        /* LST_GRADIENT */        { "wallglow",   LGM_WHITE_ALPHA,    GL_REPEAT,          GL_CLAMP_TO_EDGE },
        /* LST_RADIO_CO */        { "radioco",    LGM_WHITE_ALPHA,    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        /* LST_RADIO_CC */        { "radiocc",    LGM_WHITE_ALPHA,    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        /* LST_RADIO_OO */        { "radiooo",    LGM_WHITE_ALPHA,    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        /* LST_RADIO_OE */        { "radiooe",    LGM_WHITE_ALPHA,    GL_CLAMP_TO_EDGE,   GL_CLAMP_TO_EDGE },
        /* LST_CAMERA_VIGNETTE */ { "vignette",   LGM_NORMAL,         GL_REPEAT,          GL_CLAMP_TO_EDGE }
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
                def.wrapS, def.wrapT);

            lightingTextures[which] = glName;
        }

        Image_Destroy(&image);
    }

    DENG_ASSERT(lightingTextures[which] != 0);
    return lightingTextures[which];
}

DGLuint GL_PrepareUITexture(uitexid_t which)
{
    if(novideo) return 0;
    if(which < 0 || which >= NUM_UITEXTURES) return 0;

    static const struct TexDef {
        char const *name;
        gfxmode_t mode;
    } texDefs[NUM_UITEXTURES] = {
        /* UITEX_MOUSE */       { "Mouse",      LGM_NORMAL },
        /* UITEX_CORNER */      { "BoxCorner",  LGM_NORMAL },
        /* UITEX_FILL */        { "BoxFill",    LGM_NORMAL },
        /* UITEX_SHADE */       { "BoxShade",   LGM_NORMAL },
        /* UITEX_HINT */        { "Hint",       LGM_NORMAL },
        /* UITEX_LOGO */        { "Logo",       LGM_NORMAL },
        /* UITEX_BACKGROUND */  { "Background", LGM_GRAYSCALE }
    };
    struct TexDef const &def = texDefs[which];

    if(!uiTextures[which])
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
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR,
                0 /*no anisotropy*/, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            uiTextures[which] = glName;
        }

        Image_Destroy(&image);
    }

    //DENG_ASSERT(uiTextures[which] != 0);
    return uiTextures[which];
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

TexSource GL_LoadExtImage(image_t &image, char const *_searchPath, gfxmode_t mode)
{
    DENG_ASSERT(_searchPath);

    try
    {
        String foundPath = App_FileSystem()->findPath(de::Uri(RC_GRAPHIC, _searchPath),
                                                      RLF_DEFAULT, DD_ResourceClassById(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        if(GL_LoadImage(image, foundPath))
        {
            // Force it to grayscale?
            if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
            {
                Image_ConvertToAlpha(&image, mode == LGM_WHITE_ALPHA);
            }
            else if(mode == LGM_GRAYSCALE)
            {
                Image_ConvertToLuminance(&image, true);
            }

            return TEXS_EXTERNAL;
        }
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    return TEXS_NONE;
}

#endif // __CLIENT__

static boolean palettedIsMasked(const uint8_t* pixels, int width, int height)
{
    int i;
    DENG_ASSERT(pixels);
    // Jump to the start of the alpha data.
    pixels += width * height;
    for(i = 0; i < width * height; ++i)
    {
        if(255 != pixels[i]) return true;
    }
    return false;
}

static TexSource loadDetail(image_t& image, de::FileHandle &hndl)
{
    if(Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl)))
    {
        return TEXS_ORIGINAL;
    }

    // It must be an old-fashioned "raw" image.
    Image_Init(&image);

    // How big is it?
    de::File1 &file = hndl.file();
    size_t fileLength = hndl.length();
    switch(fileLength)
    {
    case 256 * 256: image.size.width = image.size.height = 256; break;
    case 128 * 128: image.size.width = image.size.height = 128; break;
    case  64 *  64: image.size.width = image.size.height =  64; break;
    default:
        Con_Error("GL_LoadDetailTextureLump: Must be 256x256, 128x128 or 64x64.\n");
        exit(1); // Unreachable.
    }

    image.pixelSize = 1;
    size_t bufSize = (size_t) image.size.width * image.size.height;
    image.pixels = (uint8_t *) M_Malloc(bufSize);
    if(!image.pixels) Con_Error("GL_LoadDetailTextureLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) bufSize);

    if(fileLength < bufSize)
        std::memset(image.pixels, 0, bufSize);

    // Load the raw image data.
    file.read(image.pixels, fileLength);
    return TEXS_ORIGINAL;
}

static TexSource loadFlat(image_t &image, de::FileHandle &hndl)
{
    if(Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl)))
    {
        return TEXS_EXTERNAL;
    }

    // A DOOM flat.
#define FLAT_WIDTH          64
#define FLAT_HEIGHT         64

    Image_Init(&image);

    /// @todo not all flats are 64x64!
    image.size.width  = FLAT_WIDTH;
    image.size.height = FLAT_HEIGHT;
    image.pixelSize = 1;
    image.paletteId = defaultColorPalette;

    de::File1 &file   = hndl.file();
    size_t fileLength = hndl.length();

    size_t bufSize = MAX_OF(fileLength, (size_t) image.size.width * image.size.height);
    image.pixels = (uint8_t *) M_Malloc(bufSize);
    if(!image.pixels) Con_Error("GL_LoadFlatLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) bufSize);

    if(fileLength < bufSize)
        std::memset(image.pixels, 0, bufSize);

    // Load the raw image data.
    file.read(image.pixels, 0, fileLength);
    return TEXS_ORIGINAL;

#undef FLAT_HEIGHT
#undef FLAT_WIDTH
}

/**
 * Draw the component image @a src into the composite @a dst.
 *
 * @param dst               The composite buffer (drawn to).
 * @param dstDimensions     Pixel dimensions of @a dst.
 * @param src               The component image to be composited (read from).
 * @param srcDimensions     Pixel dimensions of @a src.
 * @param origin            Coordinates (topleft) in @a dst to draw @a src.
 *
 * @todo Optimize: Should be redesigned to composite whole rows -ds
 */
static void compositePaletted(dbyte *dst, QSize const &dstDimensions,
    IByteArray const &src, QSize const &srcDimensions, QPoint const &origin)
{
    if(dstDimensions.isEmpty()) return;
    if(srcDimensions.isEmpty()) return;

    int const    srcW = srcDimensions.width();
    int const    srcH = srcDimensions.height();
    size_t const srcPels = srcW * srcH;

    int const    dstW = dstDimensions.width();
    int const    dstH = dstDimensions.height();
    size_t const dstPels = dstW * dstH;

    int dstX, dstY;

    for(int srcY = 0; srcY < srcH; ++srcY)
    for(int srcX = 0; srcX < srcW; ++srcX)
    {
        dstX = origin.x() + srcX;
        dstY = origin.y() + srcY;
        if(dstX < 0 || dstX >= dstW) continue;
        if(dstY < 0 || dstY >= dstH) continue;

        dbyte srcAlpha;
        src.get(srcY * srcW + srcX + srcPels, &srcAlpha, 1);
        if(srcAlpha)
        {
            src.get(srcY * srcW + srcX, &dst[dstY * dstW + dstX], 1);
            dst[dstY * dstW + dstX + dstPels] = srcAlpha;
        }
    }
}

static Block loadAndTranslatePatch(IByteArray const &data, int tclass = 0, int tmap = 0)
{
    if(dbyte const *xlatTable = R_TranslationTable(tclass, tmap))
    {
        return Patch::load(data, ByteRefArray(xlatTable, 256), Patch::ClipToLogicalDimensions);
    }
    else
    {
        return Patch::load(data, Patch::ClipToLogicalDimensions);
    }
}

static TexSource loadPatch(image_t &image, de::FileHandle &hndl, int tclass, int tmap, int border)
{
    LOG_AS("GL_LoadPatchLump");

    if(Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl)))
    {
        return TEXS_EXTERNAL;
    }

    de::File1 &file = hndl.file();
    ByteRefArray fileData = ByteRefArray(file.cache(), file.size());

    // A DOOM patch?
    if(Patch::recognize(fileData))
    {
        try
        {
            Block patchImg = loadAndTranslatePatch(fileData, tclass, tmap);
            Patch::Metadata info = Patch::loadMetadata(fileData);

            Image_Init(&image);
            image.size.width  = info.logicalDimensions.width()  + border*2;
            image.size.height = info.logicalDimensions.height() + border*2;
            image.pixelSize   = 1;
            image.paletteId   = defaultColorPalette;

            image.pixels = (uint8_t*) M_Calloc(2 * image.size.width * image.size.height);
            if(!image.pixels) Con_Error("GL_LoadPatchLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) (2 * image.size.width * image.size.height));

            compositePaletted(image.pixels, QSize(image.size.width, image.size.height),
                              patchImg, info.logicalDimensions, QPoint(border, border));

            if(palettedIsMasked(image.pixels, image.size.width, image.size.height))
            {
                image.flags |= IMGF_IS_MASKED;
            }

            return TEXS_ORIGINAL;
        }
        catch(IByteArray::OffsetError const &)
        {
            LOG_WARNING("File \"%s:%s\" does not appear to be a valid Patch.")
                << NativePath(file.container().composePath()).pretty()
                << NativePath(file.composePath()).pretty();
        }
    }

    file.unlock();
    return TEXS_NONE;
}

static TexSource loadPatchComposite(image_t &image, de::Texture &tex, bool maskZero,
    bool useZeroOriginIfOneComponent)
{
    LOG_AS("GL_LoadPatchComposite");

    Image_Init(&image);
    image.pixelSize = 1;
    image.size.width  = tex.width();
    image.size.height = tex.height();
    image.paletteId = defaultColorPalette;

    image.pixels = (uint8_t*) M_Calloc(2 * image.size.width * image.size.height);
    if(!image.pixels) Con_Error("GL_LoadPatchComposite: Failed on allocation of %lu bytes for new Image pixel data.", (unsigned long) (2 * image.size.width * image.size.height));

    CompositeTexture const &texDef = *reinterpret_cast<CompositeTexture *>(tex.userDataPointer());
    DENG2_FOR_EACH_CONST(CompositeTexture::Components, i, texDef.components())
    {
        de::File1 &file = App_FileSystem()->nameIndex().lump(i->lumpNum());
        ByteRefArray fileData = ByteRefArray(file.cache(), file.size());

        // A DOOM patch?
        if(Patch::recognize(fileData))
        {
            try
            {
                Patch::Flags loadFlags;
                if(maskZero) loadFlags |= Patch::MaskZero;
                Block patchImg = Patch::load(fileData, loadFlags);
                Patch::Metadata info = Patch::loadMetadata(fileData);

                QPoint origin = i->origin();
                if(useZeroOriginIfOneComponent && texDef.componentCount() == 1)
                    origin = QPoint(0, 0);

                // Draw the patch in the buffer.
                compositePaletted(image.pixels, QSize(image.size.width, image.size.height),
                                  patchImg, info.dimensions, origin);
            }
            catch(IByteArray::OffsetError const &)
            {} // Ignore this error.
        }

        file.unlock();
    }

    if(maskZero || palettedIsMasked(image.pixels, image.size.width, image.size.height))
        image.flags |= IMGF_IS_MASKED;

    // For debug:
    // GL_DumpImage(&image, Str_Text(GL_ComposeCacheNameForTexture(tex)));

    return TEXS_ORIGINAL;
}

static TexSource loadRaw(image_t &image, rawtex_t const &raw)
{
    AutoStr *foundPath = AutoStr_NewStd();
    TexSource source = TEXS_NONE;

    // First try to find an external resource.
    de::Uri searchPath("Patches", Path(Str_Text(&raw.name)));

    if(F_FindPath(RC_GRAPHIC, reinterpret_cast<uri_s *>(&searchPath), foundPath) &&
       GL_LoadImage(image, Str_Text(foundPath)))
    {
        // "External" image loaded.
        source = TEXS_EXTERNAL;
    }
    else if(raw.lumpNum >= 0)
    {
        filehandle_s *file = F_OpenLump(raw.lumpNum);
        if(file)
        {
            if(Image_LoadFromFile(&image, file))
            {
                source = TEXS_ORIGINAL;
            }
            else
            {
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

                source = TEXS_ORIGINAL;

#undef RAW_HEIGHT
#undef RAW_WIDTH
            }

            F_Delete(file);
        }
    }

    return source;
}

DGLuint GL_PrepareRawTexture(rawtex_t &raw)
{
    if(raw.lumpNum < 0 || raw.lumpNum >= F_LumpCount()) return 0;

    if(!raw.tex)
    {
        image_t image;
        Image_Init(&image);

        if(loadRaw(image, raw) == TEXS_EXTERNAL)
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

boolean GL_OptimalTextureSize(int width, int height, boolean noStretch, boolean isMipMapped,
    int* optWidth, int* optHeight)
{
    DENG_ASSERT(optWidth && optHeight);
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

void GL_DoUpdateTexParams()
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
        BusyMode_WorkerEnd();
    }
    return 0;
}

void GL_TexReset()
{
    boolean useBusyMode = !BusyMode_Active();

    GL_ReleaseTextures();
    Con_Printf("All DGL textures deleted.\n");

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

    Con_Printf("Gamma correction set to %f.\n", texGamma);
}

void GL_DoTexReset()
{
    GL_TexReset();
}

void GL_DoResetDetailTextures()
{
    GL_ReleaseTexturesByScheme("Details");
}

void GL_ReleaseTexturesForRawImages()
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

void GL_SetAllTexturesMinFilter(int /*minFilter*/)
{
    /// @todo This is no longer correct logic. Changing the global minification
    ///       filter should not modify the uploaded texture content.
}

static void performImageAnalyses(de::Texture &tex, image_t const *image,
    texturevariantspecification_t const &spec, bool forceUpdate)
{
    DENG_ASSERT(image);

    // Do we need color palette info?
    if(TST_GENERAL == spec.type && image->paletteId != 0)
    {
        colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(tex.analysisDataPointer(Texture::ColorPaletteAnalysis));
        bool firstInit = (!cp);

        if(firstInit)
        {
            cp = (colorpalette_analysis_t*) M_Malloc(sizeof(*cp));
            if(!cp) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new ColorPaletteAnalysis.", (unsigned long) sizeof(*cp));
            tex.setAnalysisDataPointer(Texture::ColorPaletteAnalysis, cp);
        }

        if(firstInit || forceUpdate)
            cp->paletteId = image->paletteId;
    }

    // Calculate a point light source for Dynlight and/or Halo?
    if(TST_GENERAL == spec.type && TC_SPRITE_DIFFUSE == TS_GENERAL(spec).context)
    {
        pointlight_analysis_t *pl = reinterpret_cast<pointlight_analysis_t*>(tex.analysisDataPointer(Texture::BrightPointAnalysis));
        bool firstInit = (!pl);

        if(firstInit)
        {
            pl = (pointlight_analysis_t *) M_Malloc(sizeof *pl);
            if(!pl) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new PointLightAnalysis.", (unsigned long) sizeof(*pl));
            tex.setAnalysisDataPointer(Texture::BrightPointAnalysis, pl);
        }

        if(firstInit || forceUpdate)
            GL_CalcLuminance(image->pixels, image->size.width, image->size.height, image->pixelSize,
                             R_ToColorPalette(image->paletteId), &pl->originX, &pl->originY,
                             &pl->color, &pl->brightMul);
    }

    // Average alpha?
    if(spec.type == TST_GENERAL &&
       (TS_GENERAL(spec).context == TC_SPRITE_DIFFUSE ||
        TS_GENERAL(spec).context == TC_UI))
    {
        averagealpha_analysis_t *aa = reinterpret_cast<averagealpha_analysis_t*>(tex.analysisDataPointer(Texture::AverageAlphaAnalysis));
        bool firstInit = (!aa);

        if(firstInit)
        {
            aa = (averagealpha_analysis_t *) M_Malloc(sizeof(*aa));
            if(!aa) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new AverageAlphaAnalysis.", (unsigned long) sizeof(*aa));
            tex.setAnalysisDataPointer(Texture::AverageAlphaAnalysis, aa);
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
    if(TST_GENERAL == spec.type && TC_SKYSPHERE_DIFFUSE == TS_GENERAL(spec).context)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            if(!ac) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageColorAnalysis, ac);
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
    if(TST_GENERAL == spec.type && TC_MAPSURFACE_DIFFUSE == TS_GENERAL(spec).context)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageColorAmplifiedAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            if(!ac) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageColorAmplifiedAnalysis, ac);
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
    if(TST_GENERAL == spec.type && TC_SKYSPHERE_DIFFUSE == TS_GENERAL(spec).context)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageTopColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            if(!ac) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageTopColorAnalysis, ac);
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
                FindAverageLineColorIdx(image->pixels, image->size.width, image->size.height, 0,
                                        R_ToColorPalette(image->paletteId), false, &ac->color);
            }
        }
    }

    // Average bottom line color for sky sphere fadeout?
    if(TST_GENERAL == spec.type && TC_SKYSPHERE_DIFFUSE == TS_GENERAL(spec).context)
    {
        averagecolor_analysis_t *ac = reinterpret_cast<averagecolor_analysis_t *>(tex.analysisDataPointer(Texture::AverageBottomColorAnalysis));
        bool firstInit = (!ac);

        if(firstInit)
        {
            ac = (averagecolor_analysis_t *) M_Malloc(sizeof(*ac));
            if(!ac) Con_Error("performImageAnalyses: Failed on allocation of %lu bytes for new AverageColorAnalysis.", (unsigned long) sizeof(*ac));
            tex.setAnalysisDataPointer(Texture::AverageBottomColorAnalysis, ac);
        }

        if(firstInit || forceUpdate)
        {
            if(0 == image->paletteId)
            {
                FindAverageLineColor(image->pixels, image->size.width, image->size.height,
                                     image->pixelSize, image->size.height - 1, &ac->color);
            }
            else
            {
                FindAverageLineColorIdx(image->pixels, image->size.width, image->size.height,
                                        image->size.height - 1, R_ToColorPalette(image->paletteId),
                                        false, &ac->color);
            }
        }
    }
}

preparetextureresult_t GL_PrepareTexture(TextureVariant &variant)
{
    DENG_ASSERT(initedOk);
    LOG_AS("GL_PrepareTexture");

    // Already been here?
    if(variant.isPrepared()) return PTR_FOUND;

    Texture &tex = variant.generalCase();
    texturevariantspecification_t const &spec = variant.spec();

    // Load the source image data.
    image_t image;
    TexSource source = loadSourceImage(tex, spec, image);
    if(source == TEXS_NONE) return PTR_NOTFOUND;

    // Are we setting the logical dimensions to the actual pixel dimensions?
    if(tex.dimensions().isEmpty())
    {
#if _DEBUG
        LOG_VERBOSE("World dimensions for \"%s\" taken from image pixels (%ix%i).")
            << tex.manifest().composeUri() << image.size.width << image.size.height;
#endif
        tex.setDimensions(QSize(image.size.width, image.size.height));
    }

    performImageAnalyses(tex, &image, spec, true /*Always update*/);

    // Are we re-preparing a released texture?
    if(0 == variant.glName())
    {
        DGLuint newGLName = GL_GetReservedTextureName();
        variant.setSource(source);
        variant.setGLName(newGLName);
    }

    // (Re)Prepare the variant according to specification.
    uploadcontentmethod_t uploadMethod;
    switch(spec.type)
    {
    case TST_GENERAL: uploadMethod = prepareVariantFromImage(variant, image); break;
    case TST_DETAIL:  uploadMethod = prepareDetailVariantFromImage(variant, image); break;
    default:
        Con_Error("GL_PrepareTexture: Invalid spec type %i.", spec.type);
        exit(1); // Unreachable.
    }

    // We're done with the image data.
    Image_Destroy(&image);

#ifdef _DEBUG
    LOG_VERBOSE("Prepared \"%s\" variant (glName:%u)%s")
        << tex.manifest().composeUri() << uint(variant.glName())
        << (METHOD_IMMEDIATE == uploadMethod? " while not busy!" : "");

    VERBOSE2(
        Con_Printf("  Content: ");
        Image_PrintMetadata(&image);
        Con_Printf("  Specification [%p]: ", de::dintptr(&spec));
        GL_PrintTextureVariantSpecification(spec);
        )
#endif

    return source == TEXS_ORIGINAL? PTR_UPLOADED_ORIGINAL : PTR_UPLOADED_EXTERNAL;
}

void GL_ReleaseGLTexturesByTexture(Texture &texture)
{
    foreach(TextureVariant *variant, texture.variants())
    {
        releaseVariantGLTexture(*variant);
    }
}

void GL_ReleaseTexturesByScheme(char const *schemeName)
{
    if(!schemeName) return;
    PathTreeIterator<Textures::Scheme::Index> iter(App_Textures().scheme(schemeName).index().leafNodes());
    while(iter.hasNext())
    {
        TextureManifest &manifest = iter.next();
        if(manifest.hasTexture())
        {
            GL_ReleaseGLTexturesByTexture(manifest.texture());
        }
    }
}

void GL_ReleaseVariantTexturesBySpec(Texture &texture, texturevariantspecification_t &spec)
{
    foreach(TextureVariant *variant, texture.variants())
    {
        if(releaseVariantGLTexture(*variant, &spec)) break;
    }
}

void GL_ReleaseVariantTexture(TextureVariant &tex)
{
    releaseVariantGLTexture(tex);
}

static int releaseGLTexturesByColorPaletteWorker(Texture &tex, void *parameters)
{
    DENG_ASSERT(parameters);
    colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(tex.analysisDataPointer(Texture::ColorPaletteAnalysis));
    colorpaletteid_t paletteId = *(colorpaletteid_t *)parameters;

    if(cp && cp->paletteId == paletteId)
    {
        GL_ReleaseGLTexturesByTexture(tex);
    }
    return 0; // Continue iteration.
}

void GL_ReleaseTexturesByColorPalette(colorpaletteid_t paletteId)
{
    App_Textures().iterate(releaseGLTexturesByColorPaletteWorker, (void *)&paletteId);
}

void GL_InitTextureContent(texturecontent_t *content)
{
    DENG_ASSERT(content);
    content->format = dgltexformat_t(0);
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

texturecontent_t *GL_ConstructTextureContentCopy(texturecontent_t const *other)
{
    DENG_ASSERT(other);

    texturecontent_t *c = (texturecontent_t*) M_Malloc(sizeof(*c));
    if(!c) Con_Error("GL_ConstructTextureContentCopy: Failed on allocation of %lu bytes for new TextureContent.", (unsigned long) sizeof(*c));

    memcpy(c, other, sizeof(*c));

    // Duplicate the image buffer.
    int bytesPerPixel = BytesPerPixelFmt(other->format);
    size_t bufferSize = bytesPerPixel * other->width * other->height;
    uint8_t *pixels = (uint8_t*) M_Malloc(bufferSize);
    std::memcpy(pixels, other->pixels, bufferSize);
    c->pixels = pixels;
    return c;
}

void GL_DestroyTextureContent(texturecontent_t *content)
{
    DENG_ASSERT(content);
    if(content->pixels) M_Free((uint8_t *)content->pixels);
    M_Free(content);
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

    uploadContentUnmanaged(chooseContentUploadMethod(c), c);
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

    uploadContentUnmanaged(chooseContentUploadMethod(c), c);
    return c.name;
}

/**
 * @param tex  Texture instance to compose the cache name of.
 * @return The chosen cache name for this texture.
 */
Path GL_ComposeCachePathForTexture(de::Texture &tex)
{
    de::Uri uri = tex.manifest().composeUri();
    return String("texcache") / uri.scheme() / uri.path() + ".png";
}

boolean GL_DumpImage(image_t const *origImg, char const *filePath)
{
    DENG_ASSERT(origImg);

    // Do we need to convert to ABGR32 first?
    image_t imgABGR32;
    image_t const *img = origImg;
    if(img->pixelSize != 4 || img->paletteId)
    {
        Image_Init(&imgABGR32);
        imgABGR32.pixels = GL_ConvertBuffer(img->pixels, img->size.width, img->size.height,
                                            ((img->flags & IMGF_IS_MASKED)? 2 : 1),
                                            R_ToColorPalette(img->paletteId), 4);
        imgABGR32.pixelSize = 4;
        imgABGR32.size.width  = img->size.width;
        imgABGR32.size.height = img->size.height;
        img = &imgABGR32;
    }

    bool savedOK = Image_Save(img, filePath);

    if(img == &imgABGR32)
    {
        Image_Destroy(&imgABGR32);
    }
    return savedOK;
}

D_CMD(LowRes)
{
    DENG_UNUSED(src); DENG_UNUSED(argv); DENG_UNUSED(argc);
    GL_LowRes();
    return true;
}

D_CMD(TexReset)
{
    DENG_UNUSED(src);

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
    DENG_UNUSED(src); DENG_UNUSED(argc);

    int newMipMode = String(argv[1]).toInt();
    if(newMipMode < 0 || newMipMode > 5)
    {
        Con_Message("Invalid mipmapping mode %i specified. Valid range is [0..5).\n", newMipMode);
        return false;
    }

    mipmapping = newMipMode;
    GL_SetTextureParams(glmode[mipmapping], true, false);
    return true;
}

#endif // __CLIENT__
