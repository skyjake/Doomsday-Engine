/** @file gl_main.cpp GL-Graphics Subsystem
 * @ingroup gl
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifdef UNIX
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#endif

#include "de_base.h"
#include "gl/gl_main.h"
#include "api_gl.h"

#include <de/legacy/concurrency.h>
#include <de/app.h>
#include <de/config.h>
#include <de/glinfo.h>
#include <de/glstate.h>
#include <de/logbuffer.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/mapinfo.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/res/colorpalettes.h>
#include "clientapp.h"
#include "sys_system.h"  // novideo

#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"

#include "gl/gl_tex.h"
#include "gl/gl_texmanager.h"
#include "gl/texturecontent.h"

#include "resource/hq2x.h"
#include "resource/materialanimator.h"
#include "resource/materialvariantspec.h"
#include "resource/clienttexture.h"

#include "api_render.h"
#include "render/rend_main.h"
#include "render/r_main.h"
#include "render/cameralensfx.h"
#include "render/rendersystem.h"
#include "render/rend_font.h"
#include "render/rend_model.h"
#include "render/rend_particle.h"
#include "render/vr.h"
#include "misc/r_util.h"

#include "ui/ui_main.h"

#include <SDL_video.h>

using namespace de;
using namespace res;

extern dint maxnumnodes;
extern dd_bool fillOutlines;

dint test3dfx;
dint r_detail = true;  ///< Draw detail textures (if available).

dfloat vid_gamma = 1.0f, vid_bright, vid_contrast = 1.0f;
dfloat glNearClip, glFarClip;

static dd_bool initGLOk;
static dd_bool initFullGLOk;

static dd_bool isGammaRampEnabled;
//static dfloat oldgamma, oldcontrast, oldbright;

static dint fogModeDefault;

//static viewport_t currentView;

static inline ClientResources &resSys()
{
    return App_Resources();
}

dd_bool GL_IsInited()
{
    return initGLOk;
}

dd_bool GL_IsFullyInited()
{
    return initFullGLOk;
}

#if 0
struct GammaRamp
{
    duint16 red[256];
    duint16 green[256];
    duint16 blue[256];
};

//void GL_GetGammaRamp(GammaRamp *ramp)
//{
//    if (!isGammaRampEnabled) return;

//    DE_ASSERT(GLWindow::mainExists());

//    SDL_GetWindowGammaRamp(reinterpret_cast<SDL_Window *>(GLWindow::getMain().sdlWindow()),
//                           ramp->red,
//                           ramp->green,
//                           ramp->blue);
//}

void GL_SetGammaRamp(const GammaRamp &ramp)
{
    if (isGammaRampEnabled && GLWindow::mainExists())
    {
        SDL_SetWindowGammaRamp(reinterpret_cast<SDL_Window *>(GLWindow::getMain().sdlWindow()),
                               ramp.red,
                               ramp.green,
                               ramp.blue);
    }
}

/**
 * Calculate a gamma ramp and write the result to the location pointed to.
 *
 * @todo  Allow for finer control of the curves (separate red, green, blue).
 *
 * @param ramp      Ptr to the ramp table to write to. Must point to a ushort[768] area of memory.
 * @param gamma     Non-linear factor (curvature; >1.0 multiplies).
 * @param contrast  Steepness.
 * @param bright    Brightness, uniform offset.
 */
void GL_MakeGammaRamp(GammaRamp &ramp, dfloat gamma, dfloat contrast, dfloat bright)
{
    double ideal[256]; // After processing clamped to unsigned short.

    // Don't allow stupid values.
    if (contrast < 0.1f) contrast = 0.1f;

    if (bright > 0.8f) bright = 0.8f;
    if (bright < -0.8f) bright = -0.8f;

    // Init the ramp as a line with the steepness defined by contrast.
    for (int i = 0; i < 256; ++i)
    {
        ideal[i] = i * contrast - (contrast - 1) * 127;
    }

    // Apply the gamma curve.
    if (gamma != 1)
    {
        if (gamma <= 0.1f) gamma = 0.1f;

        double norm = pow(255, 1 / double(gamma) - 1); // Normalizing factor.
        for (int i = 0; i < 256; ++i)
        {
            ideal[i] = pow(ideal[i], 1 / double(gamma)) / norm;
        }
    }

    // The last step is to add the brightness offset.
    for (int i = 0; i < 256; ++i)
    {
        ideal[i] += bright * 128;
    }

    // Clamp it and write the ramp table.
    for (int i = 0; i < 256; ++i)
    {
        ideal[i] *= 0x100; // Byte => word
        if (ideal[i] < 0) ideal[i] = 0;
        if (ideal[i] > 0xffff) ideal[i] = 0xffff;

        ramp.red[i] = ramp.green[i] = ramp.blue[i] = duint16(ideal[i]);
    }
}

/**
 * Updates the gamma ramp based on vid_gamma, vid_contrast and vid_bright.
 */
void GL_SetGamma()
{
    GammaRamp myramp;

    oldgamma    = vid_gamma;
    oldcontrast = vid_contrast;
    oldbright   = vid_bright;

    GL_MakeGammaRamp(myramp, vid_gamma, vid_contrast, vid_bright);
    GL_SetGammaRamp(myramp);
}
#endif


void GL_FinishFrame()
{
    if (ClientApp::vr().mode() == VRConfig::OculusRift) return;

    DE_ASSERT_IN_RENDER_THREAD();
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

#if 0
    // Check for color adjustment changes.
    if (oldgamma != vid_gamma || oldcontrast != vid_contrast || oldbright != vid_bright)
    {
        GL_SetGamma();
    }
#endif

#if !defined (DE_MOBILE)
    // Wait until the right time to show the frame so that the realized
    // frame rate is exactly right.
    glFlush();
    DD_WaitForOptimalUpdateTime();
#endif
}

static void printConfiguration()
{
    LOG_GL_VERBOSE(_E(b) "Render configuration:");

    //LOG_GL_VERBOSE("  Multisampling: %b") << GL_state.features.multisample;
    /*if(GL_state.features.multisample)
    {
        LOG_GL_VERBOSE("  Multisampling format: %i") << GL_state.multisampleFormat;
    }*/
    //LOG_GL_VERBOSE("  Multitexturing: %s") << (numTexUnits > 1? (envModAdd? "full" : "partial") : "not available");
    LOG_GL_VERBOSE("  Texture Anisotropy: %s") << (GL_state.features.texFilterAniso? "variable" : "fixed");
    LOG_GL_VERBOSE("  Texture Compression: %b") << GL_state.features.texCompression;
}

void GL_EarlyInit()
{
    if(novideo) return;
    if(initGLOk) return;  // Already initialized.

    LOG_GL_VERBOSE("Initializing Render subsystem...");

    ClientApp::render().glInit();

    isGammaRampEnabled = !CommandLine_Check("-noramp");

    GL_InitDeferredTask();

    // Model renderer must be initialized early as it may need to configure
    // gl-element arrays.
    Rend_ModelInit();

    // Check the maximum texture size.
    if(GLInfo::limits().maxTexSize == 256)
    {
        LOG_GL_WARNING("Using restricted texture w/h ratio (1:8)");
        ratioLimit = 8;
    }
    if(CommandLine_Check("-outlines"))
    {
        fillOutlines = false;
        LOG_GL_NOTE("Textures have outlines");
    }

    renderTextures = !CommandLine_Exists("-notex");

    printConfiguration();

    // Initialize the renderer into a 2D state.
    GL_Init2DState();

    GL_SetVSync(App::config().getb("window.main.vsync"));

    initGLOk = true;
}

void GL_Init()
{
    if(novideo) return;

    if(!initGLOk)
    {
        App_Error("GL_Init: GL_EarlyInit has not been done yet.\n");
    }

#if 0
    // Set the gamma in accordance with vid-gamma, vid-bright and vid-contrast.
    GL_SetGamma();
#endif

    // Initialize one viewport.
    R_SetupDefaultViewWindow(0);
    R_SetViewGrid(1, 1);
}

void GL_InitRefresh()
{
    if(novideo) return;

    GL_InitTextureManager();

    initFullGLOk = true;
}

void GL_ShutdownRefresh()
{
    initFullGLOk = false;
}

void GL_Shutdown()
{
    if(!initGLOk || !ClientWindow::mainExists())
        return; // Not yet initialized fully.

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // We won't be drawing anything further but we don't want to shutdown
    // with the previous frame still visible as this can lead to unwanted
    // artefacts during video context switches on some displays.
    //
    // Render a few black frames before we continue.
    /*if(!novideo)
    {
        dint i = 0;
        do
        {
            glClear(GL_COLOR_BUFFER_BIT);
            GL_FinishFrame();
        } while(++i < 3);
    }*/
    ClientApp::render().glDeinit();
    GL_ShutdownDeferredTask();
    FR_Shutdown();
    Rend_ModelShutdown();
    LensFx_Shutdown();
    Rend_Reset();
    GL_ShutdownRefresh();
    DGL_Shutdown();

    // Shutdown OpenGL.
    Sys_GLShutdown();

    initGLOk = false;
}

void GL_Init2DState()
{
    // The variables.
    glNearClip = 5;
    glFarClip  = 16500;

    DE_ASSERT_IN_MAIN_THREAD();

    //DGL_SetInteger(DGL_FLUSH_BACKTRACE, true);
    
    // Here we configure the OpenGL state and set the projection matrix.
    DGL_CullFace(DGL_NONE);
    DGL_Disable(DGL_DEPTH_TEST);
    DGL_Disable(DGL_TEXTURE_2D);

    // The projection matrix.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, 320, 200, -1, 1);

    // Default state for the white fog is off.
    {
        fogParams.usingFog    = false;
        fogParams.fogColor[0] = DEFAULT_FOG_COLOR_RED;
        fogParams.fogColor[1] = DEFAULT_FOG_COLOR_GREEN;
        fogParams.fogColor[2] = DEFAULT_FOG_COLOR_BLUE;
        fogParams.fogColor[3] = 1.f;
    }
    DGL_Disable(DGL_FOG);
    DGL_Fogi(DGL_FOG_MODE, (fogModeDefault == 0 ? DGL_LINEAR :
                            fogModeDefault == 1 ? DGL_EXP    : DGL_EXP2));
    DGL_Fogf(DGL_FOG_START,   DEFAULT_FOG_START);
    DGL_Fogf(DGL_FOG_END,     DEFAULT_FOG_END);
    DGL_Fogf(DGL_FOG_DENSITY, DEFAULT_FOG_DENSITY);
    DGL_Fogfv(DGL_FOG_COLOR, fogParams.fogColor);

    LIBGUI_ASSERT_GL_OK();
}

Rangef GL_DepthClipRange()
{
    return Rangef(glNearClip, glFarClip);
}

void GL_ProjectionMatrix(bool useFixedFov)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_LoadMatrix(Rend_GetProjectionMatrix(useFixedFov ? weaponFixedFOV : 0.f).values());
}

void GL_SetupFogFromMapInfo(const Record *mapInfo)
{
    if(mapInfo)
    {
        R_SetupFog(mapInfo->getf("fogStart"), mapInfo->getf("fogEnd"),
                   mapInfo->getf("fogDensity"),
                   Vec3f(mapInfo->get("fogColor")).data().baseAs<float>());
    }
    else
    {
        R_SetupFogDefaults();
    }

    if(mapInfo && !(mapInfo->geti("flags") & MIF_FOG))
    {
        GL_UseFog(false);
    }

    String fadeTable = (mapInfo? mapInfo->gets("fadeTable") : "");
    if(!fadeTable.isEmpty())
    {
        const LumpIndex &lumps = App_FileSystem().nameIndex();
        dint lumpNum = lumps.findLast(fadeTable + ".lmp");
        if(lumpNum == lumps.findLast("COLORMAP.lmp"))
        {
            // We don't want fog in this case.
            GL_UseFog(false);
        }
        // Probably fog ... don't use fullbright sprites.
        else if(lumpNum == lumps.findLast("FOGMAP.lmp"))
        {
            GL_UseFog(true);
        }
    }
}

#undef GL_UseFog
DE_EXTERN_C void GL_UseFog(dint yes)
{
    fogParams.usingFog = yes;
}

void GL_SelectTexUnits(dint count)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    for (dint i = MAX_TEX_UNITS - 1; i >= count; i--)
    {
        DGL_Disable(DGL_TEXTURE0 + i);
    }

    // Enable the selected units.
    for (dint i = count - 1; i >= 0; i--)
    {
        if (i >= MAX_TEX_UNITS) continue;

        DGL_Enable(DGL_TEXTURE0 + i);
    }
}

void GL_TotalReset()
{
    if(isDedicated) return;

    // Release all texture memory.
    resSys().releaseAllGLTextures();
    GL_LoadLightingSystemTextures();
    GL_LoadFlareTextures();
    Rend_ParticleLoadSystemTextures();

    GL_ReleaseReservedNames();

#if _DEBUG
    Z_CheckHeap();
#endif
}

void GL_TotalRestore()
{
    if(isDedicated) return;

    // Getting back up and running.
    GL_ReserveNames();
    GL_Init2DState();

    // Choose fonts again.
    UI_LoadFonts();
    //Con_Resize();

    // Restore the fog settings.
    GL_SetupFogFromMapInfo(world::World::get().hasMap() ? &App_World().map().mapInfo() : nullptr);

#ifdef DE_DEBUG
    Z_CheckHeap();
#endif
}

void GL_BlendMode(blendmode_t mode)
{
    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    switch(mode)
    {
    case BM_ZEROALPHA:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_ONE, DGL_ZERO);
        break;

    case BM_ADD:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE);
        break;

    case BM_DARK:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_DST_COLOR, DGL_ONE_MINUS_SRC_ALPHA);
        break;

    case BM_SUBTRACT:
        DGL_BlendOp(DGL_SUBTRACT);
        DGL_BlendFunc(DGL_ONE, DGL_SRC_ALPHA);
        break;

    case BM_ALPHA_SUBTRACT:
        DGL_BlendOp(DGL_SUBTRACT);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE);
        break;

    case BM_REVERSE_SUBTRACT:
        DGL_BlendOp(DGL_REVERSE_SUBTRACT);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE);
        break;

    case BM_MUL:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_ZERO, DGL_SRC_COLOR);
        break;

    case BM_INVERSE:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_ONE_MINUS_DST_COLOR, DGL_ONE_MINUS_SRC_COLOR);
        break;

    case BM_INVERSE_MUL:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
        break;

    default:
        DGL_BlendOp(DGL_ADD);
        DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

GLenum GL_Filter(gfx::Filter f)
{
    switch(f)
    {
    case gfx::Nearest: return GL_NEAREST;
    case gfx::Linear:  return GL_LINEAR;
    }
    return GL_REPEAT;
}

GLenum GL_Wrap(gfx::Wrapping w)
{
    switch(w)
    {
    case gfx::Repeat:         return GL_REPEAT;
    case gfx::RepeatMirrored: return GL_MIRRORED_REPEAT;
    case gfx::ClampToEdge:    return GL_CLAMP_TO_EDGE;
    case gfx::ClampToBorder:  return GL_CLAMP_TO_BORDER;
    }
    return GL_REPEAT;
}

dint GL_NumMipmapLevels(dint width, dint height)
{
    dint numLevels = 0;
    while(width > 1 || height > 1)
    {
        width  /= 2;
        height /= 2;
        ++numLevels;
    }
    return numLevels;
}

dd_bool GL_OptimalTextureSize(dint width, dint height, dd_bool noStretch, dd_bool isMipMapped,
    dint *optWidth, dint *optHeight)
{
    DE_ASSERT(optWidth && optHeight);
    if (!isMipMapped)
    {
        *optWidth  = width;
        *optHeight = height;
    }
    else if (noStretch)
    {
        *optWidth  = M_CeilPow2(width);
        *optHeight = M_CeilPow2(height);
    }
    else
    {
        // Determine the most favorable size for the texture.
        if(texQuality == TEXQ_BEST)
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
            *optWidth  = M_WeightPow2(width,  1 - texQuality / dfloat( TEXQ_BEST ));
            *optHeight = M_WeightPow2(height, 1 - texQuality / dfloat( TEXQ_BEST ));
        }
    }

    // Hardware limitations may force us to modify the preferred size.
    if(*optWidth > GLInfo::limits().maxTexSize)
    {
        *optWidth = GLInfo::limits().maxTexSize;
        noStretch = false;
    }
    if(*optHeight > GLInfo::limits().maxTexSize)
    {
        *optHeight = GLInfo::limits().maxTexSize;
        noStretch = false;
    }

    // Some GL drivers seem to have problems with VERY small textures.
    if(*optWidth < MINTEXWIDTH)
        *optWidth = MINTEXWIDTH;
    if(*optHeight < MINTEXHEIGHT)
        *optHeight = MINTEXHEIGHT;

    if(ratioLimit)
    {
        if(*optWidth > *optHeight)  // Wide texture.
        {
            if(*optHeight < *optWidth / ratioLimit)
                *optHeight = *optWidth / ratioLimit;
        }
        else  // Tall texture.
        {
            if(*optWidth < *optHeight / ratioLimit)
                *optWidth = *optHeight / ratioLimit;
        }
    }

    return noStretch;
}

dint GL_GetTexAnisoMul(dint level)
{
    // Should anisotropic filtering be used?
    if(!GL_state.features.texFilterAniso)
        return 1;

    if(level < 0)
    {
        // Go with the maximum!
        return GLInfo::limits().maxTexFilterAniso;
    }

    // Convert from a DGL aniso-level to a multiplier.
    dint mul;
    switch(level)
    {
    default: mul =  1; break;
    case 1:  mul =  2; break;
    case 2:  mul =  4; break;
    case 3:  mul =  8; break;
    case 4:  mul = 16; break;
    }

    // Clamp.
    return de::min(mul, GLInfo::limits().maxTexFilterAniso);
}

static void uploadContentUnmanaged(const texturecontent_t &content)
{
    LOG_AS("uploadContentUnmanaged");
    if(novideo) return;

    gfx::UploadMethod uploadMethod = GL_ChooseUploadMethod(&content);
    if(uploadMethod == gfx::Immediate)
    {
        LOGDEV_GL_XVERBOSE("Uploading texture (%i:%ix%i) while not busy! "
                           "Should have been precached in busy mode?",
                           content.name << content.width << content.height);
    }

    GL_UploadTextureContent(content, uploadMethod);
}

GLuint GL_NewTextureWithParams(dgltexformat_t format, dint width, dint height,
    const duint8 *pixels, dint flags)
{
    texturecontent_t c;
    GL_InitTextureContent(&c);
    c.name   = GL_GetReservedTextureName();
    c.format = format;
    c.width  = width;
    c.height = height;
    c.pixels = pixels;
    c.flags  = flags;

    uploadContentUnmanaged(c);
    return c.name;
}

GLuint GL_NewTextureWithParams(dgltexformat_t format,
                               dint           width,
                               dint           height,
                               const uint8_t *pixels,
                               dint           flags,
                               dint           grayMipmap,
                               GLenum         minFilter,
                               GLenum         magFilter,
                               dint           anisoFilter,
                               GLenum         wrapS,
                               GLenum         wrapT)
{
    texturecontent_t c;
    GL_InitTextureContent(&c);

    c.name        = GL_GetReservedTextureName();
    c.format      = format;
    c.width       = width;
    c.height      = height;
    c.pixels      = pixels;
    c.flags       = flags;
    c.grayMipmap  = grayMipmap;
    c.minFilter   = minFilter;
    c.magFilter   = magFilter;
    c.anisoFilter = anisoFilter;
    c.wrap[0]     = wrapS;
    c.wrap[1]     = wrapT;

    uploadContentUnmanaged(c);
    return c.name;
}

static inline const MaterialVariantSpec &uiMaterialSpec(gfx::Wrapping wrapS, gfx::Wrapping wrapT)
{
    return resSys().materialSpec(UiContext, 0, 1, 0, 0, GL_Wrap(wrapS), GL_Wrap(wrapT),
                                 0, 1, 0, false, false, false, false);
}

static inline const MaterialVariantSpec &pspriteMaterialSpec(dint tClass, dint tMap)
{
    return resSys().materialSpec(PSpriteContext, 0, 1, tClass, tMap, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                 0, -2, 0, false, true, true, false);
}

void GL_SetMaterialUI2(world::Material *material, gfx::Wrapping wrapS, gfx::Wrapping wrapT)
{
    if(!material) return; // @todo we need a "NULL material".

    MaterialAnimator &matAnimator = static_cast<ClientMaterial *>(material)->getAnimator(uiMaterialSpec(wrapS, wrapT));

    // Ensure we have up to date info about the material.
    matAnimator.prepare();

    GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
}

void GL_SetMaterialUI(world::Material *mat)
{
    GL_SetMaterialUI2(mat, gfx::ClampToEdge, gfx::ClampToEdge);
}

void GL_SetPSprite(world::Material *material, dint tClass, dint tMap)
{
    if (auto *clMat = maybeAs<ClientMaterial>(material))
    {
        MaterialAnimator &matAnimator = clMat->getAnimator(pspriteMaterialSpec(tClass, tMap));

        // Ensure we have up to date info about the material.
        matAnimator.prepare();

        GL_BindTexture(matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture);
    }
}

void GL_SetRawImage(lumpnum_t lumpNum, gfx::Wrapping wrapS, gfx::Wrapping wrapT)
{
    if(rawtex_t *rawTex = ClientResources::get().declareRawTexture(lumpNum))
    {
        GL_BindTextureUnmanaged(GL_PrepareRawTexture(*rawTex), wrapS, wrapT,
                                (filterUI ? gfx::Linear : gfx::Nearest));
    }
}

void GL_BindTexture(TextureVariant *vtexture)
{
#if defined (DE_HAVE_BUSYRUNNER)
    if (ClientApp::busyRunner().inWorkerThread()) return;
#endif

    // Ensure we have a prepared texture.
    duint glTexName = vtexture? vtexture->prepare() : 0;
    if(glTexName == 0)
    {
        GL_SetNoTexture();
        return;
    }

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, glTexName);
    LIBGUI_ASSERT_GL_OK();

    // Apply dynamic adjustments to the GL texture state according to our spec.
    const TextureVariantSpec &spec = vtexture->spec();
    if(spec.type == TST_GENERAL)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.variant.wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.variant.wrapT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spec.variant.glMagFilter());
        if(GL_state.features.texFilterAniso)
        {
            glTexParameteri(GL_TEXTURE_2D, gl33ext::GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            GL_GetTexAnisoMul(spec.variant.logicalAnisoLevel()));
        }
        LIBGUI_ASSERT_GL_OK();
    }
}

void GL_BindTextureUnmanaged(GLuint glName, gfx::Wrapping wrapS, gfx::Wrapping wrapT,
                             gfx::Filter filter)
{
#if defined (DE_HAVE_BUSYRUNNER)
    if (ClientApp::busyRunner().inWorkerThread()) return;
#endif

    LIBGUI_ASSERT_GL_OK();

    if(glName == 0)
    {
        GL_SetNoTexture();
        return;
    }

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, glName);
    LIBGUI_ASSERT_GL_OK();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_Wrap(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_Wrap(wrapT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_Filter(filter));
    if(GL_state.features.texFilterAniso)
    {
        glTexParameteri(
            GL_TEXTURE_2D, gl33ext::GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(texAniso));
    }
    LIBGUI_ASSERT_GL_OK();
}

void GL_Bind(const GLTextureUnit &glTU)
{
    if(!glTU.hasTexture()) return;

    if(!renderTextures)
    {
        GL_SetNoTexture();
        return;
    }

    if(glTU.texture)
    {
        GL_BindTexture(glTU.texture);
    }
    else
    {
        GL_BindTextureUnmanaged(glTU.unmanaged.glName, glTU.unmanaged.wrapS,
                                glTU.unmanaged.wrapT, glTU.unmanaged.filter);
    }
}

void GL_BindTo(const GLTextureUnit &glTU, dint unit)
{
    if(!glTU.hasTexture()) return;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();
    DGL_SetInteger(DGL_ACTIVE_TEXTURE, unit);
    GL_Bind(glTU);
}

void GL_SetNoTexture()
{
#if defined (DE_HAVE_BUSYRUNNER)
    if(ClientApp::busyRunner().inWorkerThread()) return;
#endif

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    /// @todo Don't actually change the current binding. Instead we should disable
    ///       all currently enabled texture types.
    glBindTexture(GL_TEXTURE_2D, 0);
}

dint GL_ChooseSmartFilter(dint width, dint height, dint /*flags*/)
{
    if(width >= MINTEXWIDTH && height >= MINTEXHEIGHT)
        return 2;  // hq2x
    return 1;  // nearest neighbor.
}

duint8 *GL_SmartFilter(dint method, const duint8 *src, dint width, dint height,
    dint flags, dint *outWidth, dint *outHeight)
{
    dint newWidth, newHeight;
    duint8 *out = nullptr;

    switch(method)
    {
    default:  // linear interpolation.
        newWidth  = width  * 2;
        newHeight = height * 2;
        out = GL_ScaleBuffer(src, width, height, 4, newWidth, newHeight);
        break;

    case 1:  // nearest neighbor.
        newWidth  = width  * 2;
        newHeight = height * 2;
        out = GL_ScaleBufferNearest(src, width, height, 4, newWidth, newHeight);
        break;

    case 2:  // hq2x
        newWidth  = width  * 2;
        newHeight = height * 2;
        out = GL_SmartFilterHQ2x(src, width, height, flags);
        break;
    };

    if(!out)
    {
        // Unchanged, return the source image.
        if(outWidth)  *outWidth  = width;
        if(outHeight) *outHeight = height;
        return const_cast<duint8 *>(src);
    }

    if(outWidth)  *outWidth  = newWidth;
    if(outHeight) *outHeight = newHeight;
    return out;
}

duint8 *GL_ConvertBuffer(const duint8 *in, dint width, dint height, dint informat,
                         colorpaletteid_t paletteId, dint outformat)
{
    DE_ASSERT(in);

    if(informat == outformat)
    {
        // No conversion necessary.
        return const_cast<duint8 *>(in);
    }

    if(width <= 0 || height <= 0)
    {
        App_Error("GL_ConvertBuffer: Attempt to convert zero-sized image.");
        exit(1); // Unreachable.
    }

    res::ColorPalette *palette = (informat <= 2? &resSys().colorPalettes().colorPalette(paletteId) : nullptr);

    auto *out = (duint8 *) M_Malloc(outformat * width * height);

    // Conversion from pal8(a) to RGB(A).
    if(informat <= 2 && outformat >= 3)
    {
        GL_PalettizeImage(out, outformat, palette, false, in, informat, width, height);
        return out;
    }

    // Conversion from RGB(A) to pal8(a), using pal18To8.
    if(informat >= 3 && outformat <= 2)
    {
        GL_QuantizeImageToPalette(out, outformat, palette, in, informat, width, height);
        return out;
    }

    if(informat == 3 && outformat == 4)
    {
        const long numPels = width * height;
        const duint8 *src  = in;
        duint8 *dst        = out;
        for(long i = 0; i < numPels; ++i)
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;  // Opaque.

            src += informat;
            dst += outformat;
        }
    }
    return out;
}

void GL_CalcLuminance(const duint8 *buffer, dint width, dint height, dint pixelSize,
    colorpaletteid_t paletteId, dfloat *retBrightX, dfloat *retBrightY,
    ColorRawf *retColor, dfloat *retLumSize)
{
    DE_ASSERT(buffer && retBrightX && retBrightY && retColor && retLumSize);

    static const duint8 sizeLimit = 192, brightLimit = 224, colLimit = 192;

    res::ColorPalette *palette = (pixelSize == 1? &resSys().colorPalettes().colorPalette(paletteId) : nullptr);

    // Apply the defaults.
    // Default to the center of the texture.
    *retBrightX = *retBrightY = .5f;

    // Default to black (i.e., no light).
    for(dint c = 0; c < 3; ++c)
    {
        retColor->rgb[c] = 0;
    }
    retColor->alpha = 1;

    // Default to a zero-size light.
    *retLumSize = 0;

    dint region[4];
    FindClipRegionNonAlpha(buffer, width, height, pixelSize, region);
    dd_bool zeroAreaRegion = (region[0] > region[1] || region[2] > region[3]);
    if(zeroAreaRegion) return;

    //
    // Image contains at least one non-transparent pixel.
    //

    long bright[2];
    for(dint i = 0; i < 2; ++i)
    {
        bright[i] = 0;
    }

    long average[3], lowAvg[3];
    for(dint i = 0; i < 3; ++i)
    {
        average[i] = 0;
        lowAvg[i]  = 0;
    }

    const duint8 *src = buffer;
    // In paletted mode, the alpha channel follows the actual image.
    const duint8 *alphaSrc = &buffer[width * height];

    // Skip to the start of the first column.
    if(region[2] > 0)
    {
        src      += pixelSize * width * region[2];
        alphaSrc += width * region[2];
    }

    duint8 rgb[3] = { 0, 0, 0 };
    dint avgCnt = 0, lowCnt = 0;
    dint cnt = 0, posCnt = 0;
    for(dint y = region[2]; y <= region[3]; ++y)
    {
        // Skip to the beginning of the row.
        if(region[0] > 0)
        {
            src      += pixelSize * region[0];
            alphaSrc += region[0];
        }

        for(dint x = region[0]; x <= region[1]; ++x, src += pixelSize, alphaSrc++)
        {
            // Alpha pixels don't count. Why? -ds
            const dd_bool pixelIsTransparent = (pixelSize == 1? *alphaSrc < 255 :
                                                pixelSize == 4?    src[3] < 255 : false);

            if(pixelIsTransparent) continue;

            if(pixelSize == 1)
            {
                Vec3ub palColor = palette->color(*src);
                rgb[0] = palColor.x;
                rgb[1] = palColor.y;
                rgb[2] = palColor.z;
            }
            else if(pixelSize >= 3)
            {
                std::memcpy(rgb, src, 3);
            }

            // Bright enough?
            if(rgb[0] > brightLimit || rgb[1] > brightLimit || rgb[2] > brightLimit)
            {
                // This pixel will participate in calculating the average center point.
                posCnt++;
                bright[0] += x;
                bright[1] += y;
            }

            // Bright enough to affect size?
            if(rgb[0] > sizeLimit || rgb[1] > sizeLimit || rgb[2] > sizeLimit)
                cnt++;

            // How about the color of the light?
            if(rgb[0] > colLimit || rgb[1] > colLimit || rgb[2] > colLimit)
            {
                avgCnt++;
                for(dint c = 0; c < 3; ++c)
                {
                    average[c] += rgb[c];
                }
            }
            else
            {
                lowCnt++;
                for(dint c = 0; c < 3; ++c)
                {
                    lowAvg[c] += rgb[c];
                }
            }
        }

        // Skip to the end of this row.
        if(region[1] < width - 1)
        {
            src      += pixelSize * (width - 1 - region[1]);
            alphaSrc += (width - 1 - region[1]);
        }
    }

    if(posCnt)
    {
        // Calculate the average of the bright pixels.
        *retBrightX = (long double) bright[0] / posCnt;
        *retBrightY = (long double) bright[1] / posCnt;
    }
    else
    {
        // No bright pixels - Place the origin at the center of the non-alpha region.
        *retBrightX = region[0] + (region[1] - region[0]) / 2.0f;
        *retBrightY = region[2] + (region[3] - region[2]) / 2.0f;
    }

    // Determine rounding (to the nearest pixel center).
    dint roundXDir = dint( *retBrightX + .5f ) == dint( *retBrightX )? 1 : -1;
    dint roundYDir = dint( *retBrightY + .5f ) == dint( *retBrightY )? 1 : -1;

    // Apply all rounding and output as decimal.
    *retBrightX = (ROUND(*retBrightX) + .5f * roundXDir) / dfloat( width );
    *retBrightY = (ROUND(*retBrightY) + .5f * roundYDir) / dfloat( height );

    if(avgCnt || lowCnt)
    {
        // The color.
        if(!avgCnt)
        {
            // Low-intensity color average.
            for(dint c = 0; c < 3; ++c)
            {
                retColor->rgb[c] = lowAvg[c] / lowCnt / 255.f;
            }
        }
        else
        {
            // High-intensity color average.
            for(dint c = 0; c < 3; ++c)
            {
                retColor->rgb[c] = average[c] / avgCnt / 255.f;
            }
        }

        Vec3f color(retColor->rgb);
        R_AmplifyColor(color);
        for(dint i = 0; i < 3; ++i)
        {
            retColor->rgb[i] = color[i];
        }

        // How about the size of the light source?
        /// @todo These factors should be cvars.
        *retLumSize = MIN_OF(((2 * cnt + avgCnt) / 3.0f / 70.0f), 1);
    }

    /*
    DEBUG_Message(("GL_CalcLuminance: width %dpx, height %dpx, bits %d\n"
                   "  cell region X[%d, %d] Y[%d, %d]\n"
                   "  flare X= %g Y=%g %s\n"
                   "  flare RGB[%g, %g, %g] %s\n",
                   width, height, pixelSize,
                   region[0], region[1], region[2], region[3],
                   *retBrightX, *retBrightY,
                   (posCnt? "(average)" : "(center)"),
                   retColor->red, retColor->green, retColor->blue,
                   (avgCnt? "(hi-intensity avg)" :
                    lowCnt? "(low-intensity avg)" : "(white light)")));
    */
}

#if !defined (DE_MOBILE)

D_CMD(SetRes)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    bool isFull = win->isFullScreen();

    dint attribs[] = {
        isFull? ClientWindow::FullscreenWidth  : ClientWindow::Width,  String(argv[1]).toInt(),
        isFull? ClientWindow::FullscreenHeight : ClientWindow::Height, String(argv[2]).toInt(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(SetFullRes)
{
    DE_UNUSED(src, argc);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::FullscreenWidth,  String(argv[1]).toInt(),
        ClientWindow::FullscreenHeight, String(argv[2]).toInt(),
        ClientWindow::Fullscreen,       true,
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(SetWinRes)
{
    DE_UNUSED(src, argc);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::Width,      String(argv[1]).toInt(),
        ClientWindow::Height,     String(argv[2]).toInt(),
        ClientWindow::Fullscreen, false,
        ClientWindow::Maximized,  false,
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(ToggleFullscreen)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::Fullscreen, !win->isFullScreen(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(ToggleMaximized)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::Maximized, !win->isMaximized(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(ToggleCentered)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::Centered, !win->isCentered(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(CenterWindow)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::Centered, true,
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(SetBPP)
{
    DE_UNUSED(src, argc);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    dint attribs[] = {
        ClientWindow::ColorDepthBits, String(argv[1]).toInt(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

#endif // !DE_MOBILE

D_CMD(DisplayModeInfo)
{
    DE_UNUSED(src, argc, argv);

    ClientWindow *win = ClientApp::mainWindow();
    if(!win) return false;

    SDL_DisplayMode disp;
    Vec2i ratio = de::ratio({disp.w, disp.h});
    SDL_GetCurrentDisplayMode(win->displayIndex(), &disp);

    String str = Stringf("Current display mode:%ix%i depth:%i (%i:%i",
                         disp.w,
                         disp.h,
                         SDL_BITSPERPIXEL(disp.format),
                         ratio.x,
                         ratio.y);
    if (disp.refresh_rate > 0)
    {
        str += Stringf(", refresh: %d Hz", disp.refresh_rate);
    }
    str += Stringf(")\nMain window:\n  current origin:%s size:%s"
                  "\n  windowed origin:%s size:%s"
                  "\n  fullscreen size:%s",
               win->pos().asText().c_str(),
               win->pointSize().asText().c_str(),
               win->windowRect().topLeft.asText().c_str(),
               win->windowRect().size().asText().c_str(),
               win->fullscreenSize().asText().c_str());

#if !defined (DE_MOBILE)
    str += Stringf("\n  fullscreen:%s centered:%s maximized:%s",
                   DE_BOOL_YESNO(win->isFullScreen()),
                   DE_BOOL_YESNO(win->isCentered()),
                   DE_BOOL_YESNO(win->isMaximized()));
#endif

    LOG_GL_MSG("%s") << str;
    return true;
}

D_CMD(ListDisplayModes)
{
    DE_UNUSED(src, argc, argv);

    auto &win = GLWindow::getMain();
    const auto modes = GLWindow::displayModes(win.displayIndex());

    LOG_GL_MSG("There are %i display modes available:") << modes.size();
    for (const auto &mode : modes)
    {
        if (mode.refreshRate > 0)
        {
            LOG_GL_MSG("  %i x %i x %i " _E(>) "(%i:%i, refresh: %d Hz)")
                    << mode.resolution.x << mode.resolution.y << mode.bitDepth
                    << mode.ratio().x << mode.ratio().y << mode.refreshRate;
        }
        else
        {
            LOG_GL_MSG("  %i x %i x %i (%i:%i)")
                    << mode.resolution.x << mode.resolution.y<< mode.bitDepth
                    << mode.ratio().x << mode.ratio().y;
        }
    }
    return true;
}

#if 0
D_CMD(UpdateGammaRamp)
{
    DE_UNUSED(src, argc, argv);

    GL_SetGamma();
    LOG_GL_VERBOSE("Gamma ramp set");
    return true;
}
#endif

D_CMD(Fog)
{
    DE_UNUSED(src);

    if(argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s (cmd) (args)") << argv[0];
        LOG_SCR_MSG("Commands: on, off, mode, color, start, end, density");
        LOG_SCR_MSG("Modes: linear, exp, exp2");
        LOG_SCR_MSG("Color is given as RGB (0-255)");
        LOG_SCR_MSG("Start and end are for linear fog, density for exponential fog.");
        return true;
    }

    if(!stricmp(argv[1], "on"))
    {
        GL_UseFog(true);
        LOG_GL_VERBOSE("Fog is now active");
        return true;
    }
    if(!stricmp(argv[1], "off"))
    {
        GL_UseFog(false);
        LOG_GL_VERBOSE("Fog is now disabled");
        return true;
    }
    if(!stricmp(argv[1], "color") && argc == 5)
    {
        for(dint i = 0; i < 3; ++i)
        {
            fogParams.fogColor[i] = strtol(argv[2 + i], nullptr, 0) / 255.0f;
        }
        fogParams.fogColor[3] = 1;

        DGL_Fogfv(DGL_FOG_COLOR, fogParams.fogColor);
        LOG_GL_VERBOSE("Fog color set");
        return true;
    }
    if(!stricmp(argv[1], "start") && argc == 3)
    {
        fogParams.fogStart = (GLfloat) strtod(argv[2], nullptr);

        DGL_Fogf(DGL_FOG_START, fogParams.fogStart);
        LOG_GL_VERBOSE("Fog start distance set");
        return true;
    }
    if(!stricmp(argv[1], "end") && argc == 3)
    {
        fogParams.fogEnd = (GLfloat) strtod(argv[2], nullptr);
        DGL_Fogf(DGL_FOG_END, fogParams.fogEnd);
        LOG_GL_VERBOSE("Fog end distance set");
        return true;
    }
    if(!stricmp(argv[1], "density") && argc == 3)
    {
        DGL_Fogf(DGL_FOG_DENSITY, (GLfloat) strtod(argv[2], nullptr));
        LOG_GL_VERBOSE("Fog density set");
        return true;
    }
    if(!stricmp(argv[1], "mode") && argc == 3)
    {
        if(!stricmp(argv[2], "linear"))
        {
            DGL_Fogi(DGL_FOG_MODE, DGL_LINEAR);
            LOG_GL_VERBOSE("Fog mode set to linear");
            return true;
        }
        if(!stricmp(argv[2], "exp"))
        {
            DGL_Fogi(DGL_FOG_MODE, DGL_EXP);
            LOG_GL_VERBOSE("Fog mode set to exp");
            return true;
        }
        if(!stricmp(argv[2], "exp2"))
        {
            DGL_Fogi(DGL_FOG_MODE, DGL_EXP2);
            LOG_GL_VERBOSE("Fog mode set to exp2");
            return true;
        }
    }

    return false;
}

void GL_Register()
{
    // Cvars
#if defined (DE_OPENGL)
    C_VAR_INT  ("rend-dev-wireframe",    &renderWireframe,  CVF_NO_ARCHIVE, 0, 2);
#endif
    C_VAR_INT  ("rend-fog-default",      &fogModeDefault,   0, 0, 2);

    // * Render-HUD
    C_VAR_FLOAT("rend-hud-offset-scale", &weaponOffsetScale,CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-hud-fov-shift",    &weaponFOVShift,   CVF_NO_MAX, 0, 1);
    C_VAR_BYTE ("rend-hud-stretch",      &weaponScaleMode,  0, SCALEMODE_FIRST, SCALEMODE_LAST);

    // * Render-Mobj
    C_VAR_INT  ("rend-mobj-smooth-move", &useSRVO,          0, 0, 2);
    C_VAR_INT  ("rend-mobj-smooth-turn", &useSRVOAngle,     0, 0, 1);

    // * video
    C_VAR_FLOAT("vid-gamma",             &vid_gamma,        0, 0.1f, 3);
    C_VAR_FLOAT("vid-contrast",          &vid_contrast,     0, 0, 2.5f);
    C_VAR_FLOAT("vid-bright",            &vid_bright,       0, -1, 1);

    Con_AddMappedConfigVariable("vid-vsync", "i", "window.main.vsync");
    Con_AddMappedConfigVariable("vid-fsaa",  "i", "window.main.fsaa");
    Con_AddMappedConfigVariable("vid-fps",   "i", "window.main.showFps");

    // Ccmds
    C_CMD_FLAGS("fog",              nullptr,   Fog,                CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD      ("displaymode",      "",     DisplayModeInfo);
    C_CMD      ("listdisplaymodes", "",     ListDisplayModes);
#if !defined (DE_MOBILE)
    C_CMD      ("setcolordepth",    "i",    SetBPP);
    C_CMD      ("setbpp",           "i",    SetBPP);
    C_CMD      ("setres",           "ii",   SetRes);
    C_CMD      ("setfullres",       "ii",   SetFullRes);
    C_CMD      ("setwinres",        "ii",   SetWinRes);
//    C_CMD      ("setvidramp",       "",     UpdateGammaRamp);
    C_CMD      ("togglefullscreen", "",     ToggleFullscreen);
    C_CMD      ("togglemaximized",  "",     ToggleMaximized);
    C_CMD      ("togglecentered",   "",     ToggleCentered);
    C_CMD      ("centerwindow",     "",     CenterWindow);
#endif
}
