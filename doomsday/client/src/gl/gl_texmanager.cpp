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

#include "de_filesys.h"
#include "de_resource.h"
#include "clientapp.h"
#include "dd_main.h" // App_ResourceSystem()
#include "con_main.h"
#include "con_bar.h"
#include "def_main.h"

#include "gl/gl_main.h" // DENG_ASSERT_GL_CONTEXT_ACTIVE
#include "gl/texturecontent.h"

#include "render/r_main.h" // R_BuildTexGammaLut
#include "render/rend_halo.h" // haloRealistic
#include "render/rend_main.h" // misc global vars
#include "render/rend_particle.h" // Rend_ParticleLoadSystemTextures()

#include "resource/hq2x.h"

#include <de/memory.h>
#include <de/memoryzone.h>
#include <QList>
#include <QMutableListIterator>
#include <cstring>

using namespace de;

static bool initedOk = false; // Init done.

// Names of the dynamic light textures.
static DGLuint lightingTextures[NUM_LIGHTING_TEXTURES];

// Names of the flare textures (halos).
static DGLuint sysFlareTextures[NUM_SYSFLARE_TEXTURES];

void GL_InitTextureManager()
{
    if(initedOk)
    {
        GL_LoadLightingSystemTextures();
        GL_LoadFlareTextures();

        Rend_ParticleLoadSystemTextures();
        return; // Already been here.
    }

    // Disable the use of 'high resolution' textures and/or patches?
    noHighResTex     = CommandLine_Exists("-nohightex");
    noHighResPatches = CommandLine_Exists("-nohighpat");
    // Should we allow using external resources with PWAD textures?
    highResWithPWAD  = CommandLine_Exists("-pwadtex");

    // System textures.
    zap(sysFlareTextures);
    zap(lightingTextures);

    GL_InitSmartFilterHQ2x();

    // Initialization done.
    initedOk = true;
}

static int reloadTextures(void *context)
{
    bool const usingBusyMode = *static_cast<bool *>(context);

    /// @todo re-upload ALL textures currently in use.
    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();

    Rend_ParticleLoadSystemTextures();
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
    if(!initedOk) return;

    App_ResourceSystem().releaseAllGLTextures();
    LOG_MSG("All GL textures deleted.");

    bool useBusyMode = !BusyMode_Active();
    if(useBusyMode)
    {
        BusyMode_FreezeGameForBusyMode();

        Con_InitProgress(200);
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    reloadTextures, &useBusyMode, "Reseting textures...");
    }
    else
    {
        reloadTextures(&useBusyMode);
    }
}

void GL_LoadLightingSystemTextures()
{
    if(novideo || !initedOk) return;

    // Preload lighting system textures.
    GL_PrepareLSTexture(LST_DYNAMIC);
    GL_PrepareLSTexture(LST_GRADIENT);
    GL_PrepareLSTexture(LST_CAMERA_VIGNETTE);
}

void GL_ReleaseAllLightingSystemTextures()
{
    if(novideo || !initedOk) return;

    glDeleteTextures(NUM_LIGHTING_TEXTURES, (GLuint const *) lightingTextures);
    zap(lightingTextures);
}

GLuint GL_PrepareLSTexture(lightingtexid_t which)
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

    DENG2_ASSERT(lightingTextures[which] != 0);
    return lightingTextures[which];
}

void GL_LoadFlareTextures()
{
    if(novideo || !initedOk) return;

    GL_PrepareSysFlaremap(FXT_ROUND);
    GL_PrepareSysFlaremap(FXT_FLARE);
    if(!haloRealistic)
    {
        GL_PrepareSysFlaremap(FXT_BRFLARE);
        GL_PrepareSysFlaremap(FXT_BIGFLARE);
    }
}

void GL_ReleaseAllFlareTextures()
{
    if(novideo || !initedOk) return;

    glDeleteTextures(NUM_SYSFLARE_TEXTURES, (GLuint const *) sysFlareTextures);
    zap(sysFlareTextures);
}

GLuint GL_PrepareSysFlaremap(flaretexid_t which)
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

    DENG2_ASSERT(sysFlareTextures[which] != 0);
    return sysFlareTextures[which];
}

GLuint GL_PrepareFlaremap(de::Uri const &resourceUri)
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
        String foundPath = App_FileSystem().findPath(de::Uri("Patches", Path(raw.name)),
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
            {
                std::memset(image.pixels, 0, bufSize);
            }

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

GLuint GL_PrepareRawTexture(rawtex_t &raw)
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
    foreach(rawtex_t *raw, App_ResourceSystem().collectRawTextures())
    {
        if(raw->tex) // Is the texture loaded?
        {
            DENG_ASSERT_IN_MAIN_THREAD();
            DENG_ASSERT_GL_CONTEXT_ACTIVE();

            glBindTexture(GL_TEXTURE_2D, raw->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, newMinFilter);
        }
    }
}

void GL_ReleaseTexturesForRawImages()
{
    foreach(rawtex_t *raw, App_ResourceSystem().collectRawTextures())
    {
        if(raw->tex)
        {
            glDeleteTextures(1, (GLuint const *) &raw->tex);
            raw->tex = 0;
        }
    }
    LOG_MSG("All GL textures for RawTextures deleted.");
}
