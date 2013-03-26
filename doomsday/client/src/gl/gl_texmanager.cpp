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
#include "resource/hq2x.h"

#include <QSize>
#include <de/ByteRefArray>
#include <de/mathutil.h>
#include <de/memory.h>
#include <de/memoryzone.h>

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

static TexSource loadExternalTexture(image_t &image, String searchPath, String optionalSuffix = "");

static TexSource loadFlat(image_t &image, de::FileHandle &file);
static TexSource loadPatch(image_t &image, de::FileHandle &file, int tclass = 0, int tmap = 0, int border = 0);
static TexSource loadDetail(image_t &image, de::FileHandle &file);

static TexSource loadRaw(image_t &image, rawtex_t const &raw);
static TexSource loadPatchComposite(image_t &image, Texture const &tex,
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

GLUploadMethod GL_ChooseUploadMethod(texturecontent_t const &content)
{
    // Must the operation be carried out immediately?
    if((content.flags & TXCF_NEVER_DEFER) || !BusyMode_Active())
    {
        return Immediate;
    }
    // We can defer.
    return Deferred;
}

static int releaseVariantGLTexture(TextureVariant &variant, texturevariantspecification_t *spec = 0)
{
    if(!spec || spec == &variant.spec())
    {
        variant.release();
        if(spec) return true; // We're done.
    }
    return 0; // Continue iteration.
}

static void uploadContentUnmanaged(texturecontent_t const &content)
{
    LOG_AS("uploadContentUnmanaged");
    if(novideo) return;

    GLUploadMethod uploadMethod = GL_ChooseUploadMethod(content);
    if(uploadMethod == Immediate)
    {
#ifdef DENG_DEBUG
        LOG_VERBOSE("Uploading texture (%i:%ix%i) while not busy! Should be precached in busy mode?")
            << content.name << content.width << content.height;
#endif
    }

    GL_UploadTextureContent(content, uploadMethod);
}

TexSource GL_LoadSourceImage(image_t &image, Texture const &tex,
    texturevariantspecification_t const &spec)
{
    TexSource source = TEXS_NONE;
    variantspecification_t const &vspec = TS_GENERAL(spec);
    if(!tex.manifest().schemeName().compareWithoutCase("Textures"))
    {
        // Attempt to load an external replacement for this composite texture?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.isFlagged(Texture::Custom)))
        {
            // First try the textures scheme.
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if(source == TEXS_NONE)
        {
            if(TC_SKYSPHERE_DIFFUSE != vspec.context)
            {
                source = loadPatchComposite(image, tex);
            }
            else
            {
                bool const zeroMask = (vspec.flags & TSF_ZEROMASK) != 0;
                bool const useZeroOriginIfOneComponent = true;
                source = loadPatchComposite(image, tex, zeroMask, useZeroOriginIfOneComponent);
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Flats"))
    {
        // Attempt to load an external replacement for this flat?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.isFlagged(Texture::Custom)))
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
            if(tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                        de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                        de::FileHandle &hndl = App_FileSystem().openLump(lump);

                        source = loadFlat(image, hndl);

                        App_FileSystem().releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch(LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Patches"))
    {
        int tclass = 0, tmap = 0;
        if(vspec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            DENG_ASSERT(vspec.translated);
            tclass = vspec.translated->tClass;
            tmap   = vspec.translated->tMap;
        }

        // Attempt to load an external replacement for this patch?
        if(!noHighResTex && (loadExtAlways || highResWithPWAD || !tex.isFlagged(Texture::Custom)))
        {
            de::Uri uri = tex.manifest().composeUri();
            source = loadExternalTexture(image, uri.compose(), "-ck");
        }

        if(source == TEXS_NONE)
        {
            if(tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                        de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                        de::FileHandle &hndl = App_FileSystem().openLump(lump);

                        source = loadPatch(image, hndl, tclass, tmap, vspec.border);

                        App_FileSystem().releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch(LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Sprites"))
    {
        int tclass = 0, tmap = 0;
        if(vspec.flags & TSF_HAS_COLORPALETTE_XLAT)
        {
            DENG_ASSERT(vspec.translated);
            tclass = vspec.translated->tClass;
            tmap   = vspec.translated->tMap;
        }

        // Attempt to load an external replacement for this sprite?
        if(!noHighResPatches)
        {
            de::Uri uri = tex.manifest().composeUri();

            // Prefer psprite or translated versions if available.
            if(TC_PSPRITE_DIFFUSE == vspec.context)
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
            if(tex.manifest().hasResourceUri())
            {
                de::Uri resourceUri = tex.manifest().resourceUri();
                if(!resourceUri.scheme().compareWithoutCase("LumpIndex"))
                {
                    try
                    {
                        lumpnum_t lumpNum = resourceUri.path().toString().toInt();
                        de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                        de::FileHandle &hndl = App_FileSystem().openLump(lump);

                        source = loadPatch(image, hndl, tclass, tmap, vspec.border);

                        App_FileSystem().releaseFile(hndl.file());
                        delete &hndl;
                    }
                    catch(LumpIndex::NotFoundError const&)
                    {} // Ignore this error.
                }
            }
        }
    }
    else if(!tex.manifest().schemeName().compareWithoutCase("Details"))
    {
        if(tex.manifest().hasResourceUri())
        {
            de::Uri resourceUri = tex.manifest().resourceUri();
            if(resourceUri.scheme().compareWithoutCase("Lumps"))
            {
                source = loadExternalTexture(image, resourceUri.compose());
            }
            else
            {
                lumpnum_t lumpNum = App_FileSystem().lumpNumForName(resourceUri.path());
                try
                {
                    de::File1 &lump = App_FileSystem().nameIndex().lump(lumpNum);
                    de::FileHandle &hndl = App_FileSystem().openLump(lump);

                    source = loadDetail(image, hndl);

                    App_FileSystem().releaseFile(hndl.file());
                    delete &hndl;
                }
                catch(LumpIndex::NotFoundError const&)
                {} // Ignore this error.
            }
        }
    }
    else
    {
        if(tex.manifest().hasResourceUri())
        {
            de::Uri resourceUri = tex.manifest().resourceUri();
            source = loadExternalTexture(image, resourceUri.compose());
        }
    }
    return source;
}

/**
 * Prepares the image for use as a GL texture in accordance with the given
 * specification.
 *
 * @param image     The image to prepare (in place).
 * @param spec      Specification describing any transformations which should
 *                  be applied to the image.
 *
 * @return  The DGL texture format determined for the image.
 */
static dgltexformat_t prepareImageAsTexture(image_t &image,
    variantspecification_t const &spec)
{
    DENG_ASSERT(image.pixels);

    bool const monochrome = (spec.flags & TSF_MONOCHROME) != 0;
    bool const scaleSharp = (spec.flags & TSF_UPSCALE_AND_SHARPEN) != 0;

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

            uint8_t *newPixels = GL_ConvertBuffer(image.pixels, image.size.width, image.size.height,
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

    /*
     * Choose the final GL texture format.
     */
    if(monochrome)
    {
        return image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8 : DGL_LUMINANCE;
    }
    if(image.paletteId)
    {
        return (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 : DGL_COLOR_INDEX_8;
    }
    return   image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8
           : image.pixelSize == 3 ? DGL_RGB
           : image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE;
}

/**
 * Prepares the image for use as a detail GL texture in accordance with the
 * given specification.
 *
 * @param image     The image to prepare (in place).
 * @param spec      Specification describing any transformations which should
 *                  be applied to the image.
 *
 * Return values:
 * @param baMul     Luminance equalization balance factor is written here.
 * @param hiMul     Luminance equalization white-shift factor is written here.
 * @param loMul     Luminance equalization black-shift factor is written here.
 *
 * @return  The DGL texture format determined for the image.
 */
static dgltexformat_t prepareImageAsDetailTexture(image_t &image,
    detailvariantspecification_t const &spec, float *baMul, float *hiMul, float *loMul)
{
    DENG_UNUSED(spec);

    // We want a luminance map.
    if(image.pixelSize > 2)
    {
        Image_ConvertToLuminance(&image, false);
    }

    // Try to normalize the luminance data so it works expectedly as a detail texture.
    EqualizeLuma(image.pixels, image.size.width, image.size.height, baMul, hiMul, loMul);

    return DGL_LUMINANCE;
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
    GL_ReleaseTextures();
    GL_PruneTextureVariantSpecifications();
    GL_LoadSystemTextures();
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

        text += " Context:" + textureUsageContextNames[tc-TEXTUREVARIANTUSAGECONTEXT_FIRST + 1];
              + " Flags:" + String::number(spec.flags & ~TSF_INTERNAL_MASK)
              + " Border:" + String::number(spec.border)
              + " MinFilter:" + filterModeNames[3 + de::clamp(-1, spec.minFilter, 0)]
                              + "|" + glFilterNames[glMinFilterNameIdx]
              + " MagFilter:" + filterModeNames[3 + de::clamp(-3, spec.magFilter, 0)]
                              + "|" + glFilterNames[glMagFilterNameIdx]
              + " AnisoFilter:" + String::number(spec.anisoFilter);

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

uint8_t *GL_LoadImage(image_t &image, String nativePath)
{
    try
    {
        // Relative paths are relative to the native working directory.
        String path = (NativePath::workPath() / NativePath(nativePath).expand()).withSeparators('/');

        de::FileHandle &hndl = App_FileSystem().openFile(path, "rb");
        uint8_t *pixels = Image_LoadFromFile(&image, reinterpret_cast<filehandle_s *>(&hndl));

        App_FileSystem().releaseFile(hndl.file());
        delete &hndl;

        return pixels;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.

    return 0; // Not loaded.
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
void GL_UploadTextureContent(texturecontent_t const &content, GLUploadMethod method)
{
    if(method == Deferred)
    {
        GL_DeferTextureUpload(&content);
        return;
    }

    if(novideo) return;

    // Do this right away. No need to take a copy.
    bool generateMipmaps = (content.flags & (TXCF_MIPMAP|TXCF_GRAY_MIPMAP)) != 0;
    bool applyTexGamma   = (content.flags & TXCF_APPLY_GAMMACORRECTION)     != 0;
    bool noCompression   = (content.flags & TXCF_NO_COMPRESSION)            != 0;
    bool noSmartFilter   = (content.flags & TXCF_UPLOAD_ARG_NOSMARTFILTER)  != 0;
    bool noStretch       = (content.flags & TXCF_UPLOAD_ARG_NOSTRETCH)      != 0;

    int loadWidth = content.width, loadHeight = content.height;
    uint8_t const *loadPixels = content.pixels;
    dgltexformat_t dglFormat = content.format;

    if(DGL_COLOR_INDEX_8 == dglFormat || DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat)
    {
        // Convert a paletted source image to truecolor.
        uint8_t *newPixels = GL_ConvertBuffer(loadPixels, loadWidth, loadHeight,
                                              DGL_COLOR_INDEX_8_PLUS_A8 == dglFormat ? 2 : 1,
                                              R_ToColorPalette(content.paletteId),
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
                                               loadPixels, loadWidth, loadHeight,
                                               ICF_UPSCALE_SAMPLE_WRAP,
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
            uint8_t *newPixels = GL_ScaleBuffer(loadPixels, width, height, comps,
                                                loadWidth, loadHeight);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     content.wrap[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     content.wrap[1]);
    if(GL_state.features.texFilterAniso)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(content.anisoFilter));

    if(!(content.flags & TXCF_GRAY_MIPMAP))
    {
        GLint loadFormat;
        switch(dglFormat)
        {
        case DGL_LUMINANCE_PLUS_A8: loadFormat = GL_LUMINANCE_ALPHA; break;
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE;       break;
        case DGL_RGB:               loadFormat = GL_RGB;             break;
        case DGL_RGBA:              loadFormat = GL_RGBA;            break;
        default:
            throw Error("GL_UploadTextureContent", QString("Unknown format %1").arg(int(dglFormat)));
        }

        GLint glFormat = ChooseTextureFormat(dglFormat, !noCompression);

        if(!GL_UploadTexture(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                             generateMipmaps ? true : false))
        {
            throw Error("GL_UploadTextureContent", QString("TexImage failed (%1:%2 fmt%3)")
                                                       .arg(content.name)
                                                       .arg(Vector2i(loadWidth, loadHeight).asText())
                                                       .arg(int(dglFormat)));
        }
    }
    else
    {
        // Special fade-to-gray luminance texture (used for details).
        GLint glFormat, loadFormat;

        switch(dglFormat)
        {
        case DGL_LUMINANCE:         loadFormat = GL_LUMINANCE; break;
        case DGL_RGB:               loadFormat = GL_RGB;       break;
        default:
            throw Error("GL_UploadTextureContent", QString("Unknown format %1").arg(int(dglFormat)));
        }

        glFormat = ChooseTextureFormat(DGL_LUMINANCE, !noCompression);

        if(!GL_UploadTextureGrayMipmap(glFormat, loadFormat, loadPixels, loadWidth, loadHeight,
                                       content.grayMipmap * reciprocal255))
        {
            throw Error("GL_UploadTextureContent", QString("TexImageGrayMipmap failed (%u:%ix%i fmt%i)")
                                                       .arg(content.name)
                                                       .arg(Vector2i(loadWidth, loadHeight).asText())
                                                       .arg(int(dglFormat)));
        }
    }

    if(loadPixels != content.pixels)
    {
        M_Free(const_cast<uint8_t *>(loadPixels));
    }
}

static TexSource loadExternalTexture(image_t &image, String encodedSearchPath,
    String optionalSuffix)
{
    // First look for a version with an optional suffix.
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri(encodedSearchPath + optionalSuffix, RC_GRAPHIC),
                                                     RLF_DEFAULT, DD_ResourceClassById(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? TEXS_EXTERNAL : TEXS_NONE;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore this error.

    // Try again without the suffix?
    if(!optionalSuffix.empty())
    {
        try
        {
            String foundPath = App_FileSystem().findPath(de::Uri(encodedSearchPath, RC_GRAPHIC),
                                                         RLF_DEFAULT, DD_ResourceClassById(RC_GRAPHIC));
            // Ensure the found path is absolute.
            foundPath = App_BasePath() / foundPath;

            return GL_LoadImage(image, foundPath)? TEXS_EXTERNAL : TEXS_NONE;
        }
        catch(FS1::NotFoundError const&)
        {} // Ignore this error.
    }

    return TEXS_NONE;
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

TexSource GL_LoadExtImage(image_t &image, char const *_searchPath, gfxmode_t mode)
{
    DENG_ASSERT(_searchPath);

    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri(RC_GRAPHIC, _searchPath),
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
static void compositePaletted(dbyte *dst, Vector2i const &dstDimensions,
    IByteArray const &src, Vector2i const &srcDimensions, Vector2i const &origin)
{
    if(dstDimensions.x == 0 && dstDimensions.y == 0) return;
    if(srcDimensions.x == 0 && srcDimensions.y == 0) return;

    int const    srcW = srcDimensions.x;
    int const    srcH = srcDimensions.y;
    size_t const srcPels = srcW * srcH;

    int const    dstW = dstDimensions.x;
    int const    dstH = dstDimensions.y;
    size_t const dstPels = dstW * dstH;

    int dstX, dstY;

    for(int srcY = 0; srcY < srcH; ++srcY)
    for(int srcX = 0; srcX < srcW; ++srcX)
    {
        dstX = origin.x + srcX;
        dstY = origin.y + srcY;
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
            image.size.width  = info.logicalDimensions.x + border*2;
            image.size.height = info.logicalDimensions.y + border*2;
            image.pixelSize   = 1;
            image.paletteId   = defaultColorPalette;

            image.pixels = (uint8_t*) M_Calloc(2 * image.size.width * image.size.height);
            if(!image.pixels) Con_Error("GL_LoadPatchLump: Failed on allocation of %lu bytes for Image pixel buffer.", (unsigned long) (2 * image.size.width * image.size.height));

            compositePaletted(image.pixels, Vector2i(image.size.width, image.size.height),
                              patchImg, info.logicalDimensions, Vector2i(border, border));

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

static TexSource loadPatchComposite(image_t &image, Texture const &tex,
    bool maskZero, bool useZeroOriginIfOneComponent)
{
    LOG_AS("GL_LoadPatchComposite");

    Image_Init(&image);
    image.pixelSize = 1;
    image.size.width  = tex.width();
    image.size.height = tex.height();
    image.paletteId = defaultColorPalette;

    image.pixels = (uint8_t *) M_Calloc(2 * image.size.width * image.size.height);

    CompositeTexture const &texDef = *reinterpret_cast<CompositeTexture *>(tex.userDataPointer());
    DENG2_FOR_EACH_CONST(CompositeTexture::Components, i, texDef.components())
    {
        de::File1 &file = App_FileSystem().nameIndex().lump(i->lumpNum());
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

                Vector2i origin = i->origin();
                if(useZeroOriginIfOneComponent && texDef.componentCount() == 1)
                    origin = Vector2i(0, 0);

                // Draw the patch in the buffer.
                compositePaletted(image.pixels, Vector2i(image.size.width, image.size.height),
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
    // First try an external resource.
    try
    {
        String foundPath = App_FileSystem().findPath(de::Uri("Patches", Path(Str_Text(&raw.name))),
                                                     RLF_DEFAULT, DD_ResourceClassById(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? TEXS_EXTERNAL : TEXS_NONE;
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
                return TEXS_ORIGINAL;
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
            return TEXS_ORIGINAL;

#undef RAW_HEIGHT
#undef RAW_WIDTH
        }
    }

    return TEXS_NONE;
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

void GL_SetRawTexturesMinFilter(int newMinFilter)
{
    rawtex_t **rawTexs = R_CollectRawTexs();
    for(rawtex_t **ptr = rawTexs; *ptr; ptr++)
    {
        rawtex_t *r = *ptr;
        if(r->tex) // Is the texture loaded?
        {
            LIBDENG_ASSERT_IN_MAIN_THREAD();
            LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

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

static int reloadTextures(void *parameters)
{
    boolean usingBusyMode = *((boolean *) parameters);

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
    boolean useBusyMode = !BusyMode_Active();

    GL_ReleaseTextures();
    Con_Message("All DGL textures deleted.");

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

    Con_Message("Gamma correction set to %f.", texGamma);
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

void GL_PrepareTextureContent(texturecontent_t &c, DGLuint glTexName,
    image_t &image, texturevariantspecification_t const &spec,
    TextureManifest const &textureManifest)
{
    DENG_ASSERT(glTexName != 0);
    DENG_ASSERT(image.pixels != 0);

    // Initialize and assign a GL name to the content.
    GL_InitTextureContent(&c);
    c.name = glTexName;

    switch(spec.type)
    {
    case TST_GENERAL: {
        variantspecification_t const &vspec = TS_GENERAL(spec);
        bool const noCompression = (vspec.flags & TSF_NO_COMPRESSION) != 0;
        // If the Upscale And Sharpen filter is enabled, scaling is applied
        // implicitly by prepareImageAsTexture(), so don't do it again.
        bool const noSmartFilter = (vspec.flags & TSF_UPSCALE_AND_SHARPEN) != 0;

        // Prepare the image for upload.
        dgltexformat_t dglFormat = prepareImageAsTexture(image, vspec);

        // Configure the texture content.
        c.format      = dglFormat;
        c.width       = image.size.width;
        c.height      = image.size.height;
        c.pixels      = image.pixels;
        c.paletteId   = image.paletteId;

        if(noCompression || (image.size.width < 128 || image.size.height < 128))
            c.flags |= TXCF_NO_COMPRESSION;
        if(vspec.gammaCorrection) c.flags |= TXCF_APPLY_GAMMACORRECTION;
        if(vspec.noStretch)       c.flags |= TXCF_UPLOAD_ARG_NOSTRETCH;
        if(vspec.mipmapped)       c.flags |= TXCF_MIPMAP;
        if(noSmartFilter)         c.flags |= TXCF_UPLOAD_ARG_NOSMARTFILTER;

        c.magFilter   = GL_MagFilterForVariantSpec(vspec);
        c.minFilter   = GL_MinFilterForVariantSpec(vspec);
        c.anisoFilter = GL_LogicalAnisoLevelForVariantSpec(vspec);
        c.wrap[0]     = vspec.wrapS;
        c.wrap[1]     = vspec.wrapT;
        break; }

    case TST_DETAIL: {
        detailvariantspecification_t const &dspec = TS_DETAIL(spec);

        // Prepare the image for upload.
        float baMul, hiMul, loMul;
        dgltexformat_t dglFormat = prepareImageAsDetailTexture(image, dspec, &baMul, &hiMul, &loMul);

        // Determine the gray mipmap factor.
        int grayMipmapFactor = dspec.contrast;
        if(baMul != 1 || hiMul != 1 || loMul != 1)
        {
            // Integrate the normalization factor with contrast.
            float const hiContrast = 1 - 1. / hiMul;
            float const loContrast = 1 - loMul;
            float const shift = ((hiContrast + loContrast) / 2);

            grayMipmapFactor = int(255 * de::clamp(0.f, dspec.contrast / 255.f - shift, 1.f));

            // Announce the normalization.
            de::Uri uri = textureManifest.composeUri();
            LOG_VERBOSE("Normalized detail texture \"%s\" (balance: %g, high amp: %g, low amp: %g).")
                << uri << baMul << hiMul << loMul;
        }

        // Configure the texture content.
        c.format      = dglFormat;
        c.flags       = TXCF_GRAY_MIPMAP | TXCF_UPLOAD_ARG_NOSMARTFILTER;

        // Disable compression?
        if(image.size.width < 128 || image.size.height < 128)
            c.flags |= TXCF_NO_COMPRESSION;

        c.grayMipmap  = grayMipmapFactor;
        c.width       = image.size.width;
        c.height      = image.size.height;
        c.pixels      = image.pixels;
        c.anisoFilter = texAniso;
        c.magFilter   = glmode[texMagMode];
        c.minFilter   = GL_LINEAR_MIPMAP_LINEAR;
        c.wrap[0]     = GL_REPEAT;
        c.wrap[1]     = GL_REPEAT;
        break; }

    default:
        // Invalid spec type.
        DENG_ASSERT(false);
    }
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
    PathTreeIterator<TextureScheme::Index> iter(App_Textures().scheme(schemeName).index().leafNodes());
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

void GL_ReleaseTexturesByColorPalette(colorpaletteid_t paletteId)
{
    foreach(Texture *texture, App_Textures().all())
    {
        colorpalette_analysis_t *cp = reinterpret_cast<colorpalette_analysis_t *>(texture->analysisDataPointer(Texture::ColorPaletteAnalysis));
        if(cp && cp->paletteId == paletteId)
        {
            GL_ReleaseGLTexturesByTexture(*texture);
        }
    }
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

    std::memcpy(c, other, sizeof(*c));

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
