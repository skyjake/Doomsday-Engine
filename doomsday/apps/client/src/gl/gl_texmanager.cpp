/** @file gl_texmanager.cpp  GL-Texture management.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2002 Graham Jackson <no contact email published>
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

#include "de_base.h"
#include "gl/gl_texmanager.h"

#include <cstring>
#include <de/legacy/concurrency.h>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <de/glinfo.h>
#include <doomsday/res/textures.h>
#include <doomsday/filesys/fs_main.h>

#include "clientapp.h"
#include "dd_main.h"  // App_Resources()
#include "def_main.h"

#include "sys_system.h"

#include "gl/gl_main.h"  // DE_ASSERT_GL_CONTEXT_ACTIVE
#include "gl/texturecontent.h"

//#include "render/r_main.h"
#include "render/rend_halo.h"  // haloRealistic
#include "render/rend_main.h"  // misc global vars
#include "render/rend_particle.h"  // Rend_ParticleLoadSystemTextures()

#include "resource/hq2x.h"

#include "ui/progress.h"

using namespace de;

static bool initedOk = false; // Init done.

// Names of the dynamic light textures.
static DGLuint lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
static DGLuint sysFlareTextures[NUM_SYSFLARE_TEXTURES];

void GL_InitTextureManager()
{
    if (initedOk)
    {
        GL_LoadLightingSystemTextures();
        GL_LoadFlareTextures();

        Rend_ParticleLoadSystemTextures();
        return; // Already been here.
    }

    auto &cfg = R_Config();
    // Disable the use of 'high resolution' textures and/or patches?
    if (CommandLine_Exists("-nohightex"))
    {
        cfg.noHighResTex->set(NumberValue(true));
    }
    if (CommandLine_Exists("-nohighpat"))
    {
        cfg.noHighResPatches->set(NumberValue(true));
    }
    // Should we allow using external resources with PWAD textures?
    if (CommandLine_Exists("-pwadtex"))
    {
        cfg.highResWithPWAD->set(NumberValue(true));
    }

    // System textures.
    zap(sysFlareTextures);
    zap(lightingTextures);

    GL_InitSmartFilterHQ2x();

    // Initialization done.
    initedOk = true;
}

/*
static int reloadTextures(void *context)
{
    const bool usingBusyMode = *static_cast<bool *>(context);

    /// @todo re-upload ALL textures currently in use.
    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();

    Rend_ParticleLoadSystemTextures();
    Rend_ParticleLoadExtraTextures();

    if (usingBusyMode)
    {
        Con_SetProgress(200);
    }
    return 0;
}*/

void GL_TexReset()
{
    if (!initedOk) return;

    Rend_ResetLookups();

    App_Resources().releaseAllGLTextures();
    LOG_GL_VERBOSE("Released all GL textures");

//    bool useBusyMode = !BusyMode_Active();
//    if (useBusyMode)
//    {
//        BusyMode_FreezeGameForBusyMode();

//        Con_InitProgress(200);
//        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
//                                    reloadTextures, &useBusyMode, "Reseting textures...");
//    }
//    else
//    {
//        reloadTextures(&useBusyMode);
//    }

    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();

    Rend_ParticleLoadSystemTextures();
    Rend_ParticleLoadExtraTextures();
}

void GL_LoadLightingSystemTextures()
{
    if (novideo || !initedOk) return;

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);
    GL_PrepareLSTexture(LST_CAMERA_VIGNETTE);
}

void GL_ReleaseAllLightingSystemTextures()
{
    if (novideo || !initedOk) return;

    Deferred_glDeleteTextures(NUM_LIGHTING_TEXTURES, (const GLuint *) lightingTextures);
    zap(lightingTextures);
}

GLuint GL_PrepareLSTexture(lightingtexid_t which)
{
    if (novideo) return 0;
    if (which < 0 || which >= NUM_LIGHTING_TEXTURES) return 0;

    static const struct TexDef {
        const char *name;
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
    const struct TexDef &def = texDefs[which];

    if (!lightingTextures[which])
    {
        image_t image;

        if (GL_LoadExtImage(image, def.name, def.mode))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            DGLuint glName = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.x, image.size.y, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR, -1 /*best anisotropy*/,
                ( which == LST_GRADIENT? GL_REPEAT : GL_CLAMP_TO_EDGE ), GL_CLAMP_TO_EDGE);

            lightingTextures[which] = glName;
        }

        Image_ClearPixelData(image);
    }

    DE_ASSERT(lightingTextures[which] != 0);
    return lightingTextures[which];
}

void GL_LoadFlareTextures()
{
    if (novideo || !initedOk) return;

    GL_PrepareSysFlaremap(FXT_ROUND);
    GL_PrepareSysFlaremap(FXT_FLARE);
    if (!haloRealistic)
    {
        GL_PrepareSysFlaremap(FXT_BRFLARE);
        GL_PrepareSysFlaremap(FXT_BIGFLARE);
    }
}

void GL_ReleaseAllFlareTextures()
{
    if (novideo || !initedOk) return;

    Deferred_glDeleteTextures(NUM_SYSFLARE_TEXTURES, (const GLuint *) sysFlareTextures);
    zap(sysFlareTextures);
}

GLuint GL_PrepareSysFlaremap(flaretexid_t which)
{
    if (novideo) return 0;
    if (which < 0 || which >= NUM_SYSFLARE_TEXTURES) return 0;

    static const struct TexDef {
        const char *name;
    } texDefs[NUM_SYSFLARE_TEXTURES] = {
        /* FXT_ROUND */     { "dlight" },
        /* FXT_FLARE */     { "flare" },
        /* FXT_BRFLARE */   { "brflare" },
        /* FXT_BIGFLARE */  { "bigflare" }
    };
    const struct TexDef &def = texDefs[which];

    if (!sysFlareTextures[which])
    {
        image_t image;

        if (GL_LoadExtImage(image, def.name, LGM_WHITE_ALPHA))
        {
            // Loaded successfully and converted accordingly.
            // Upload the image to GL.
            DGLuint glName = GL_NewTextureWithParams(
                ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
                  image.pixelSize == 3 ? DGL_RGB :
                  image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
                image.size.x, image.size.y, image.pixels,
                TXCF_NO_COMPRESSION, 0, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            sysFlareTextures[which] = glName;
        }

        Image_ClearPixelData(image);
    }

    DE_ASSERT(sysFlareTextures[which] != 0);
    return sysFlareTextures[which];
}

GLuint GL_PrepareFlaremap(const res::Uri &resourceUri)
{
    if (resourceUri.path().length() == 1)
    {
        // Select a system flare by numeric identifier?
        int number = resourceUri.path().toString().first().delta('0');
        if (number == 0) return 0; // automatic
        if (number >= 1 && number <= 4)
        {
            return GL_PrepareSysFlaremap(flaretexid_t(number - 1));
        }
    }
    if (auto *tex = res::Textures::get().tryFindTextureByResourceUri(DE_STR("Flaremaps"), resourceUri))
    {
        if (const TextureVariant *variant = static_cast<ClientTexture *>(tex)->prepareVariant(Rend_HaloTextureSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    return 0;
}

static res::Source loadRaw(image_t &image, const rawtex_t &raw)
{
    res::FS1 &fileSys = App_FileSystem();

    // First try an external resource.
    try
    {
        String foundPath = fileSys.findPath(res::Uri("Patches", Path(raw.name)),
                                             RLF_DEFAULT, App_ResourceClass(RC_GRAPHIC));
        // Ensure the found path is absolute.
        foundPath = App_BasePath() / foundPath;

        return GL_LoadImage(image, foundPath)? res::External : res::None;
    }
    catch (res::FS1::NotFoundError const&)
    {} // Ignore this error.

    try
    {
        res::FileHandle &file = fileSys.openLump(fileSys.lump(raw.lumpNum));
        if (Image_LoadFromFile(image, file))
        {
            fileSys.releaseFile(file.file());
            delete &file;

            return res::Original;
        }

        // It must be an old-fashioned "raw" image.
#define RAW_WIDTH           320
#define RAW_HEIGHT          200

        const size_t fileLength = file.length();

        Image_Init(image);
        image.size      = Vec2ui(RAW_WIDTH, fileLength / RAW_WIDTH);
        image.pixelSize = 1;

        // Load the raw image data.
        const size_t numPels = RAW_WIDTH * RAW_HEIGHT;
        image.pixels = (uint8_t *) M_Malloc(3 * numPels);
        if (fileLength < 3 * numPels)
        {
            std::memset(image.pixels, 0, 3 * numPels);
        }
        file.read(image.pixels, fileLength);

        fileSys.releaseFile(file.file());
        delete &file;

        return res::Original;

#undef RAW_HEIGHT
#undef RAW_WIDTH
    }
    catch (const res::LumpIndex::NotFoundError &)
    {} // Ignore error.

    return res::None;
}

GLuint GL_PrepareRawTexture(rawtex_t &raw)
{
    if (raw.lumpNum < 0 || raw.lumpNum >= App_FileSystem().lumpCount()) return 0;

    if (!raw.tex)
    {
        image_t image;
        Image_Init(image);

        if (loadRaw(image, raw) == res::External)
        {
            // Loaded an external raw texture.
            raw.tex = GL_NewTextureWithParams(image.pixelSize == 4? DGL_RGBA : DGL_RGB,
                image.size.x, image.size.y, image.pixels, 0, 0,
                GL_NEAREST, (filterUI ? GL_LINEAR : GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
        }
        else
        {
            raw.tex = GL_NewTextureWithParams(
                (image.flags & IMGF_IS_MASKED)? DGL_COLOR_INDEX_8_PLUS_A8 :
                          image.pixelSize == 4? DGL_RGBA :
                          image.pixelSize == 3? DGL_RGB : DGL_COLOR_INDEX_8,
                image.size.x, image.size.y, image.pixels, 0, 0,
                GL_NEAREST, (filterUI? GL_LINEAR:GL_NEAREST), 0 /*no anisotropy*/,
                GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        }

        raw.width  = image.size.x;
        raw.height = image.size.y;
        Image_ClearPixelData(image);
    }

    return raw.tex;
}

void GL_SetRawTexturesMinFilter(GLenum newMinFilter)
{
    for (rawtex_t *raw : App_Resources().collectRawTextures())
    {
        if (raw->tex) // Is the texture loaded?
        {
            DE_ASSERT_IN_MAIN_THREAD();
            LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

            glBindTexture(GL_TEXTURE_2D, raw->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, newMinFilter);
        }
    }
}

void GL_ReleaseTexturesForRawImages()
{
    for (rawtex_t *raw : App_Resources().collectRawTextures())
    {
        if (raw->tex)
        {
            Deferred_glDeleteTextures(1, (const GLuint *) &raw->tex);
            raw->tex = 0;
        }
    }
    LOG_GL_VERBOSE("Released all GL textures for raw images");
}
