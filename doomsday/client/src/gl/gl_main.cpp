/** @file gl_main.cpp GL-Graphics Subsystem
 * @ingroup gl
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"
#include "de_ui.h"
#include "de_defs.h"

#include "clientapp.h"
#include "world/map.h"
#include "world/p_object.h"
#include "gl/gl_texmanager.h"
#include "gl/texturecontent.h"
#include "ui/clientwindowsystem.h"
#include "resource/hq2x.h"
#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "Texture"
#include "api_render.h"
#include "render/vr.h"

#include <de/DisplayMode>
#include <de/GLInfo>
#include <de/GLState>
#include <de/App>

D_CMD(Fog);
D_CMD(SetBPP);
D_CMD(SetRes);
D_CMD(SetFullRes);
D_CMD(SetWinRes);
D_CMD(ToggleFullscreen);
D_CMD(ToggleMaximized);
D_CMD(ToggleCentered);
D_CMD(CenterWindow);
D_CMD(DisplayModeInfo);
D_CMD(ListDisplayModes);

using namespace de;

void GL_SetGamma();

extern int maxnumnodes;
extern dd_bool fillOutlines;

int     numTexUnits = 1;
dd_bool envModAdd; // TexEnv: modulate and add is available.
int     test3dfx = 0;
int     r_detail = true; // Render detail textures (if available).

float   vid_gamma = 1.0f, vid_bright = 0, vid_contrast = 1.0f;
float   glNearClip, glFarClip;

static dd_bool initGLOk = false;
static dd_bool initFullGLOk = false;

static dd_bool gamma_support = false;
static float oldgamma, oldcontrast, oldbright;

static int fogModeDefault = 0;

static byte fsaaEnabled   = true; // default value also specified in CanvasWindow
static byte vsyncEnabled  = true;

static viewport_t currentView;

static void videoFSAAChanged()
{
    if(novideo || !WindowSystem::mainExists()) return;
    ClientWindow::main().updateCanvasFormat();
}

static void videoVsyncChanged()
{
    if(novideo || !WindowSystem::mainExists()) return;

#ifdef WIN32
    ClientWindow::main().updateCanvasFormat();
#else
    GL_SetVSync(Con_GetByte("vid-vsync") != 0);
#endif
}

void GL_Register()
{
    // Update values from saved Config.
    fsaaEnabled = App::config().getb("window.fsaa", true);

    // Cvars
    C_VAR_INT  ("rend-dev-wireframe",    &renderWireframe,  CVF_NO_ARCHIVE, 0, 2);
    C_VAR_INT  ("rend-fog-default",      &fogModeDefault,   0, 0, 2);

    // * Render-HUD
    C_VAR_FLOAT("rend-hud-offset-scale", &weaponOffsetScale,CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-hud-fov-shift",    &weaponFOVShift,   CVF_NO_MAX, 0, 1);
    C_VAR_BYTE ("rend-hud-stretch",      &weaponScaleMode,  0, SCALEMODE_FIRST, SCALEMODE_LAST);

    // * Render-Mobj
    C_VAR_INT  ("rend-mobj-smooth-move", &useSRVO,          0, 0, 2);
    C_VAR_INT  ("rend-mobj-smooth-turn", &useSRVOAngle,     0, 0, 1);

    // * video
    C_VAR_BYTE2("vid-vsync",             &vsyncEnabled,     0, 0, 1, videoVsyncChanged);
    C_VAR_BYTE2("vid-fsaa",              &fsaaEnabled,      0, 0, 1, videoFSAAChanged);
    C_VAR_FLOAT("vid-gamma",             &vid_gamma,        0, 0.1f, 4);
    C_VAR_FLOAT("vid-contrast",          &vid_contrast,     0, 0, 2.5f);
    C_VAR_FLOAT("vid-bright",            &vid_bright,       0, -1, 1);

    // Ccmds
    C_CMD_FLAGS("fog",              NULL,   Fog,                CMDF_NO_NULLGAME|CMDF_NO_DEDICATED);
    C_CMD      ("displaymode",      "",     DisplayModeInfo);
    C_CMD      ("listdisplaymodes", "",     ListDisplayModes);
    C_CMD      ("setcolordepth",    "i",    SetBPP);
    C_CMD      ("setbpp",           "i",    SetBPP);
    C_CMD      ("setres",           "ii",   SetRes);
    C_CMD      ("setfullres",       "ii",   SetFullRes);
    C_CMD      ("setwinres",        "ii",   SetWinRes);
    C_CMD      ("setvidramp",       "",     UpdateGammaRamp);
    C_CMD      ("togglefullscreen", "",     ToggleFullscreen);
    C_CMD      ("togglemaximized",  "",     ToggleMaximized);
    C_CMD      ("togglecentered",   "",     ToggleCentered);
    C_CMD      ("centerwindow",     "",     CenterWindow);
}

dd_bool GL_IsInited()
{
    return initGLOk;
}

dd_bool GL_IsFullyInited()
{
    return initFullGLOk;
}

#if defined(WIN32) || defined(MACOSX)
void GL_AssertContextActive()
{
#ifdef WIN32
    assert(wglGetCurrentContext() != 0);
#else
    assert(CGLGetCurrentContext() != 0);
#endif
}
#endif

void GL_DoUpdate()
{
    if(ClientApp::vr().mode() == VRConfig::OculusRift) return;

    // Check for color adjustment changes.
    if(oldgamma != vid_gamma || oldcontrast != vid_contrast || oldbright != vid_bright)
    {
        GL_SetGamma();
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Wait until the right time to show the frame so that the realized
    // frame rate is exactly right.
    glFlush();
    DD_WaitForOptimalUpdateTime();

    // Blit screen to video.
    ClientWindow::main().swapBuffers();
}

void GL_GetGammaRamp(DisplayColorTransfer *ramp)
{
    if(!gamma_support) return;

    DisplayMode_GetColorTransfer(ramp);
}

void GL_SetGammaRamp(DisplayColorTransfer const *ramp)
{
    if(!gamma_support) return;

    DisplayMode_SetColorTransfer(ramp);
}

/**
 * Calculate a gamma ramp and write the result to the location pointed to.
 *
 * @todo  Allow for finer control of the curves (separate red, green, blue).
 *
 * @param ramp          Ptr to the ramp table to write to. Must point to a ushort[768] area of memory.
 * @param gamma         Non-linear factor (curvature; >1.0 multiplies).
 * @param contrast      Steepness.
 * @param bright        Brightness, uniform offset.
 */
void GL_MakeGammaRamp(ushort *ramp, float gamma, float contrast, float bright)
{
    DENG_ASSERT(ramp);

    double ideal[256]; // After processing clamped to unsigned short.

    // Don't allow stupid values.
    if(contrast < 0.1f)
        contrast = 0.1f;

    if(bright > 0.8f)  bright = 0.8f;
    if(bright < -0.8f) bright = -0.8f;

    // Init the ramp as a line with the steepness defined by contrast.
    for(int i = 0; i < 256; ++i)
    {
        ideal[i] = i * contrast - (contrast - 1) * 127;
    }

    // Apply the gamma curve.
    if(gamma != 1)
    {
        if(gamma <= 0.1f) gamma = 0.1f;

        double norm = pow(255, 1 / double(gamma) - 1); // Normalizing factor.
        for(int i = 0; i < 256; ++i)
        {
            ideal[i] = pow(ideal[i], 1 / double(gamma)) / norm;
        }
    }

    // The last step is to add the brightness offset.
    for(int i = 0; i < 256; ++i)
    {
        ideal[i] += bright * 128;
    }

    // Clamp it and write the ramp table.
    for(int i = 0; i < 256; ++i)
    {
        ideal[i] *= 0x100; // Byte => word
        if(ideal[i] < 0)        ideal[i] = 0;
        if(ideal[i] > 0xffff)   ideal[i] = 0xffff;

        ramp[i] = ramp[i + 256] = ramp[i + 512] = (unsigned short) ideal[i];
    }
}

/**
 * Updates the gamma ramp based on vid_gamma, vid_contrast and vid_bright.
 */
void GL_SetGamma()
{
    DisplayColorTransfer myramp;

    oldgamma    = vid_gamma;
    oldcontrast = vid_contrast;
    oldbright   = vid_bright;

    GL_MakeGammaRamp(myramp.table, vid_gamma, vid_contrast, vid_bright);
    GL_SetGammaRamp(&myramp);
}

static void printConfiguration()
{
    LOG_GL_VERBOSE(_E(b) "Render configuration:");

    LOG_GL_VERBOSE("  Multisampling: %b") << GL_state.features.multisample;
    if(GL_state.features.multisample)
    {
        LOG_GL_VERBOSE("  Multisampling format: %i") << GL_state.multisampleFormat;
    }
    LOG_GL_VERBOSE("  Multitexturing: %s") << (numTexUnits > 1? (envModAdd? "full" : "partial") : "not available");
    LOG_GL_VERBOSE("  Texture Anisotropy: %s") << (GL_state.features.texFilterAniso? "variable" : "fixed");
    LOG_GL_VERBOSE("  Texture Compression: %b") << GL_state.features.texCompression;
    LOG_GL_VERBOSE("  Texture NPOT: %b") << GL_state.features.texNonPowTwo;
}

dd_bool GL_EarlyInit()
{
    if(novideo) return true;
    if(initGLOk) return true; // Already initialized.

    LOG_GL_VERBOSE("Initializing Render subsystem...");

    ClientApp::renderSystem().glInit();

    gamma_support = !CommandLine_Check("-noramp");

    // We are simple people; two texture units is enough.
    numTexUnits = MIN_OF(GLInfo::limits().maxTexUnits, MAX_TEX_UNITS);
    envModAdd = (GLInfo::extensions().NV_texture_env_combine4 ||
                 GLInfo::extensions().ATI_texture_env_combine3);

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

    GL_SetVSync(true); // will be overridden from vid-vsync

    initGLOk = true;
    return true;
}

void GL_Init()
{
    if(novideo) return;

    if(!initGLOk)
    {
        App_Error("GL_Init: GL_EarlyInit has not been done yet.\n");
    }

    // Set the gamma in accordance with vid-gamma, vid-bright and vid-contrast.
    GL_SetGamma();

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

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // We won't be drawing anything further but we don't want to shutdown
    // with the previous frame still visible as this can lead to unwanted
    // artefacts during video context switches on some displays.
    //
    // Render a few black frames before we continue.
    if(!novideo)
    {
        int i = 0;
        do
        {
            glClear(GL_COLOR_BUFFER_BIT);
            GL_DoUpdate();
        } while(++i < 3);
    }
    ClientApp::renderSystem().glDeinit();
    GL_ShutdownDeferredTask();
    FR_Shutdown();
    Rend_ModelShutdown();
    LensFx_Shutdown();
    Rend_Reset();

    GL_ShutdownRefresh();

    // Shutdown OpenGL.
    Sys_GLShutdown();

    initGLOk = false;
}

void GL_Init2DState()
{
    // The variables.
    glNearClip = 5;
    glFarClip = 16500;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Here we configure the OpenGL state and set the projection matrix.
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_CUBE_MAP);

    // Default, full area viewport.
    //glViewport(0, 0, DENG_WINDOW->width(), DENG_WINDOW->height());

    // The projection matrix.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 320, 200, 0, -1, 1);

    // Default state for the white fog is off.
    usingFog = false;
    glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, (fogModeDefault == 0 ? GL_LINEAR :
                         fogModeDefault == 1 ? GL_EXP    : GL_EXP2));
    glFogf(GL_FOG_START, DEFAULT_FOG_START);
    glFogf(GL_FOG_END, DEFAULT_FOG_END);
    glFogf(GL_FOG_DENSITY, DEFAULT_FOG_DENSITY);
    fogColor[0] = DEFAULT_FOG_COLOR_RED;
    fogColor[1] = DEFAULT_FOG_COLOR_GREEN;
    fogColor[2] = DEFAULT_FOG_COLOR_BLUE;
    fogColor[3] = 1;
    glFogfv(GL_FOG_COLOR, fogColor);
}

void GL_SwitchTo3DState(dd_bool push_state, viewport_t const *port, viewdata_t const *viewData)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(push_state)
    {
        // Push the 2D matrices on the stack.
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    std::memcpy(&currentView, port, sizeof(currentView));

    viewpx = port->geometry.topLeft.x + viewData->window.topLeft.x;
    viewpy = port->geometry.topLeft.y + viewData->window.topLeft.y;
    viewpw = de::min(port->geometry.width(), viewData->window.width());
    viewph = de::min(port->geometry.height(), viewData->window.height());

    ClientWindow::main().game().glApplyViewport(Rectanglei::fromSize(Vector2i(viewpx, viewpy),
                                                                     Vector2ui(viewpw, viewph)));

    // The 3D projection matrix.
    GL_ProjectionMatrix();
}

void GL_Restore2DState(int step, viewport_t const *port, viewdata_t const *viewData)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(step)
    {
    case 1: { // After Restore Step 1 normal player sprites are rendered.
        int height = (float)(port->geometry.width() * viewData->window.height() / viewData->window.width()) / port->geometry.height() * SCREENHEIGHT;
        scalemode_t sm = R_ChooseScaleMode(SCREENWIDTH, SCREENHEIGHT,
                                           port->geometry.width(), port->geometry.height(),
                                           scalemode_t(weaponScaleMode));

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        if(SCALEMODE_STRETCH == sm)
        {
            glOrtho(0, SCREENWIDTH, height, 0, -1, 1);
        }
        else
        {
            /**
             * Use an orthographic projection in native screenspace. Then
             * translate and scale the projection to produce an aspect
             * corrected coordinate space at 4:3, aligned vertically to
             * the bottom and centered horizontally in the window.
             */
            glOrtho(0, port->geometry.width(), port->geometry.height(), 0, -1, 1);
            glTranslatef(port->geometry.width()/2, port->geometry.height(), 0);

            if(port->geometry.width() >= port->geometry.height())
                glScalef((float)port->geometry.height() / SCREENHEIGHT,
                         (float)port->geometry.height() / SCREENHEIGHT, 1);
            else
                glScalef((float)port->geometry.width() / SCREENWIDTH,
                         (float)port->geometry.width() / SCREENWIDTH, 1);

            // Special case: viewport height is greater than width.
            // Apply an additional scaling factor to prevent player sprites
            // looking too small.
            if(port->geometry.height() > port->geometry.width())
            {
                float extraScale = (((float)port->geometry.height()*2)/port->geometry.width()) / 2;
                glScalef(extraScale, extraScale, 1);
            }

            glTranslatef(-(SCREENWIDTH/2), -SCREENHEIGHT, 0);
            glScalef(1, (float)SCREENHEIGHT/height, 1);
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Depth testing must be disabled so that psprite 1 will be drawn
        // on top of psprite 0 (Doom plasma rifle fire).
        glDisable(GL_DEPTH_TEST);
        break; }

    case 2: // After Restore Step 2 we're back in 2D rendering mode.
        ClientWindow::main().game().glApplyViewport(currentView.geometry);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        break;

    default:
        App_Error("GL_Restore2DState: Invalid value, step = %i.", step);
        break;
    }
}

Matrix4f GL_GetProjectionMatrix()
{
    float const fov = Rend_FieldOfView();
    Vector2f const size(viewpw, viewph);
    yfov = vrCfg().verticalFieldOfView(fov, size);
    return vrCfg().projectionMatrix(Rend_FieldOfView(), size, glNearClip, glFarClip) *
           Matrix4f::scale(Vector3f(1, 1, -1));
}

void GL_ProjectionMatrix()
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Actually shift the player viewpoint
    // We'd like to have a left-handed coordinate system.
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(GL_GetProjectionMatrix().values());
}

#undef GL_UseFog
DENG_EXTERN_C void GL_UseFog(int yes)
{
    usingFog = yes;
}

void GL_SelectTexUnits(int count)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    for(int i = numTexUnits - 1; i >= count; i--)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glDisable(GL_TEXTURE_2D);
    }

    // Enable the selected units.
    for(int i = count - 1; i >= 0; i--)
    {
        if(i >= numTexUnits) continue;

        glActiveTexture(GL_TEXTURE0 + i);
        glEnable(GL_TEXTURE_2D);
    }
}

void GL_TotalReset()
{
    if(isDedicated) return;

    // Update the secondary title and the game status.
    //Rend_ConsoleUpdateTitle();

    // Release all texture memory.
    App_ResourceSystem().releaseAllGLTextures();
    App_ResourceSystem().pruneUnusedTextureSpecs();
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

    /// @todo fixme: Should this use the default MapInfo def if none found? -ds
    ded_mapinfo_t *mapInfo = 0;
    if(App_WorldSystem().hasMap())
    {
        if(MapDef *mapDef = App_WorldSystem().map().def())
        {
            de::Uri const mapUri = mapDef->composeUri();
            mapInfo = defs.getMapInfo(&mapUri);
        }
    }

    // Restore map's fog settings.
    if(!mapInfo || !(mapInfo->flags & MIF_FOG))
    {
        R_SetupFogDefaults();
    }
    else
    {
        R_SetupFog(mapInfo->fogStart, mapInfo->fogEnd, mapInfo->fogDensity, mapInfo->fogColor);
    }

#if _DEBUG
    Z_CheckHeap();
#endif
}

void GL_BlendMode(blendmode_t mode)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    switch(mode)
    {
    case BM_ZEROALPHA:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::One, gl::Zero)
                          .apply();
        break;

    case BM_ADD:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::SrcAlpha, gl::One)
                          .apply();
        break;

    case BM_DARK:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::DestColor, gl::OneMinusSrcAlpha)
                          .apply();
        break;

    case BM_SUBTRACT:
        GLState::current().setBlendOp(gl::Subtract)
                          .setBlendFunc(gl::One, gl::SrcAlpha)
                          .apply();
        break;

    case BM_ALPHA_SUBTRACT:
        GLState::current().setBlendOp(gl::Subtract)
                          .setBlendFunc(gl::SrcAlpha, gl::One)
                          .apply();
        break;

    case BM_REVERSE_SUBTRACT:
        GLState::current().setBlendOp(gl::ReverseSubtract)
                          .setBlendFunc(gl::SrcAlpha, gl::One)
                          .apply();
        break;

    case BM_MUL:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::Zero, gl::SrcColor)
                          .apply();
        break;

    case BM_INVERSE:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::OneMinusDestColor, gl::OneMinusSrcColor)
                          .apply();
        break;

    case BM_INVERSE_MUL:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::Zero, gl::OneMinusSrcColor)
                          .apply();
        break;

    default:
        GLState::current().setBlendOp(gl::Add)
                          .setBlendFunc(gl::SrcAlpha, gl::OneMinusSrcAlpha)
                          .apply();
        break;
    }
}

GLenum GL_Filter(gl::Filter f)
{
    switch(f)
    {
    case gl::Nearest: return GL_NEAREST;
    case gl::Linear:  return GL_LINEAR;
    }
    return GL_REPEAT;
}

GLenum GL_Wrap(gl::Wrapping w)
{
    switch(w)
    {
    case gl::Repeat:         return GL_REPEAT;
    case gl::RepeatMirrored: return GL_MIRRORED_REPEAT;
    case gl::ClampToEdge:    return GL_CLAMP_TO_EDGE;
    }
    return GL_REPEAT;
}

int GL_NumMipmapLevels(int width, int height)
{
    int numLevels = 0;
    while(width > 1 || height > 1)
    {
        width  /= 2;
        height /= 2;
        ++numLevels;
    }
    return numLevels;
}

dd_bool GL_OptimalTextureSize(int width, int height, dd_bool noStretch, dd_bool isMipMapped,
    int *optWidth, int *optHeight)
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

int GL_GetTexAnisoMul(int level)
{
    int mul = 1;

    // Should anisotropic filtering be used?
    if(GL_state.features.texFilterAniso)
    {
        if(level < 0)
        {   // Go with the maximum!
            mul = GLInfo::limits().maxTexFilterAniso;
        }
        else
        {   // Convert from a DGL aniso level to a multiplier.
            // i.e 0 > 1, 1 > 2, 2 > 4, 3 > 8, 4 > 16
            switch(level)
            {
            case 0: mul = 1; break; // x1 (normal)
            case 1: mul = 2; break; // x2
            case 2: mul = 4; break; // x4
            case 3: mul = 8; break; // x8
            case 4: mul = 16; break; // x16

            default: // Wha?
                mul = 1;
                break;
            }

            // Clamp.
            mul = MIN_OF(mul, GLInfo::limits().maxTexFilterAniso);
        }
    }

    return mul;
}

static void uploadContentUnmanaged(texturecontent_t const &content)
{
    LOG_AS("uploadContentUnmanaged");
    if(novideo) return;

    gl::UploadMethod uploadMethod = GL_ChooseUploadMethod(&content);
    if(uploadMethod == gl::Immediate)
    {
        LOGDEV_GL_XVERBOSE("Uploading texture (%i:%ix%i) while not busy! "
                           "Should have been precached in busy mode?")
                << content.name << content.width << content.height;
    }

    GL_UploadTextureContent(content, uploadMethod);
}

GLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
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

GLuint GL_NewTextureWithParams(dgltexformat_t format, int width, int height,
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

void GL_SetMaterialUI2(Material *mat, gl::Wrapping wrapS, gl::Wrapping wrapT)
{
    if(!mat) return; // @todo we need a "NULL material".

    MaterialVariantSpec const &spec =
        App_ResourceSystem().materialSpec(UiContext, 0, 1, 0, 0, GL_Wrap(wrapS),
                                          GL_Wrap(wrapT), 0, 1, 0, false, false,
                                          false, false);
    GL_BindTexture(&mat->prepare(spec).texture(MTU_PRIMARY));
}

void GL_SetMaterialUI(Material *mat)
{
    GL_SetMaterialUI2(mat, gl::ClampToEdge, gl::ClampToEdge);
}

void GL_SetPSprite(Material *mat, int tClass, int tMap)
{
    if(!mat) return; // @todo we need a "NULL material".

    MaterialVariantSpec const &spec =
        App_ResourceSystem().materialSpec(PSpriteContext, 0, 1, tClass, tMap,
                                          GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                          0, 1, 0, false, true, true, false);
    GL_BindTexture(&mat->prepare(spec).texture(MTU_PRIMARY));
}

void GL_SetRawImage(lumpnum_t lumpNum, gl::Wrapping wrapS, gl::Wrapping wrapT)
{
    if(rawtex_t *rawTex = App_ResourceSystem().declareRawTexture(lumpNum))
    {
        GL_BindTextureUnmanaged(GL_PrepareRawTexture(*rawTex), wrapS, wrapT,
                                (filterUI ? gl::Linear : gl::Nearest));
    }
}

void GL_BindTexture(TextureVariant *vtexture)
{
    if(BusyMode_InWorkerThread()) return;

    // Ensure we have a prepared texture.
    uint glTexName = vtexture? vtexture->prepare() : 0;
    if(glTexName == 0)
    {
        GL_SetNoTexture();
        return;
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, glTexName);
    Sys_GLCheckError();

    // Apply dynamic adjustments to the GL texture state according to our spec.
    TextureVariantSpec const &spec = vtexture->spec();
    if(spec.type == TST_GENERAL)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.variant.wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.variant.wrapT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spec.variant.glMagFilter());
        if(GL_state.features.texFilterAniso)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                            GL_GetTexAnisoMul(spec.variant.logicalAnisoLevel()));
        }
    }
}

void GL_BindTextureUnmanaged(GLuint glName, gl::Wrapping wrapS, gl::Wrapping wrapT,
    gl::Filter filter)
{
    if(BusyMode_InWorkerThread()) return;

    if(glName == 0)
    {
        GL_SetNoTexture();
        return;
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBindTexture(GL_TEXTURE_2D, glName);
    Sys_GLCheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_Wrap(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_Wrap(wrapT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_Filter(filter));
    if(GL_state.features.texFilterAniso)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GL_GetTexAnisoMul(texAniso));
    }
}

void GL_Bind(GLTextureUnit const &glTU)
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

void GL_BindTo(GLTextureUnit const &glTU, int unit)
{
    if(!glTU.hasTexture()) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();
    glActiveTexture(GL_TEXTURE0 + byte(unit));
    GL_Bind(glTU);
}

void GL_SetNoTexture()
{
    if(BusyMode_InWorkerThread()) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    /// @todo Don't actually change the current binding. Instead we should disable
    ///       all currently enabled texture types.
    glBindTexture(GL_TEXTURE_2D, 0);
}

int GL_ChooseSmartFilter(int width, int height, int /*flags*/)
{
    if(width >= MINTEXWIDTH && height >= MINTEXHEIGHT)
        return 2; // hq2x
    return 1; // nearest neighbor.
}

uint8_t* GL_SmartFilter(int method, const uint8_t* src, int width, int height,
    int flags, int* outWidth, int* outHeight)
{
    int newWidth, newHeight;
    uint8_t* out = NULL;

    switch(method)
    {
    default: // linear interpolation.
        newWidth  = width  * 2;
        newHeight = height * 2;
        out = GL_ScaleBuffer(src, width, height, 4, newWidth, newHeight);
        break;

    case 1: // nearest neighbor.
        newWidth  = width  * 2;
        newHeight = height * 2;
        out = GL_ScaleBufferNearest(src, width, height, 4, newWidth, newHeight);
        break;

    case 2: // hq2x
        newWidth  = width  * 2;
        newHeight = height * 2;
        out = GL_SmartFilterHQ2x(src, width, height, flags);
        break;
    };

    if(NULL == out)
    {   // Unchanged, return the source image.
        if(outWidth)  *outWidth  = width;
        if(outHeight) *outHeight = height;
        return (uint8_t*)src;
    }

    if(outWidth)  *outWidth  = newWidth;
    if(outHeight) *outHeight = newHeight;
    return out;
}

uint8_t *GL_ConvertBuffer(uint8_t const *in, int width, int height, int informat,
    colorpaletteid_t paletteId, int outformat)
{
    DENG2_ASSERT(in != 0);

    if(informat == outformat)
    {
        // No conversion necessary.
        return (uint8_t*)in;
    }

    if(width <= 0 || height <= 0)
    {
        App_Error("GL_ConvertBuffer: Attempt to convert zero-sized image.");
        exit(1); // Unreachable.
    }

    ColorPalette *palette = (informat <= 2? &App_ResourceSystem().colorPalette(paletteId) : 0);

    uint8_t *out = (uint8_t*) M_Malloc(outformat * width * height);

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
        long i, numPels = width * height;
        uint8_t const *src = in;
        uint8_t *dst = out;
        for(i = 0; i < numPels; ++i)
        {
            dst[CR] = src[CR];
            dst[CG] = src[CG];
            dst[CB] = src[CB];
            dst[CA] = 255; // Opaque.

            src += informat;
            dst += outformat;
        }
    }
    return out;
}

void GL_CalcLuminance(uint8_t const *buffer, int width, int height, int pixelSize,
    colorpaletteid_t paletteId, float *retBrightX, float *retBrightY,
    ColorRawf *retColor, float *retLumSize)
{
    DENG_ASSERT(buffer && retBrightX && retBrightY && retColor && retLumSize);

    uint8_t const sizeLimit = 192, brightLimit = 224, colLimit = 192;
    uint8_t const *src, *alphaSrc;
    int avgCnt = 0, lowCnt = 0;
    int cnt = 0, posCnt = 0;
    int i, x, y, c;
    long average[3], lowAvg[3];
    long bright[2];
    uint8_t rgb[3];

    ColorPalette *palette = (pixelSize == 1? &App_ResourceSystem().colorPalette(paletteId) : 0);

    // Apply the defaults.
    // Default to the center of the texture.
    *retBrightX = *retBrightY = .5f;

    // Default to black (i.e., no light).
    for(c = 0; c < 3; ++c)
        retColor->rgb[c] = 0;
    retColor->alpha = 1;

    // Default to a zero-size light.
    *retLumSize = 0;

    int region[4];
    FindClipRegionNonAlpha(buffer, width, height, pixelSize, region);
    dd_bool zeroAreaRegion = (region[0] > region[1] || region[2] > region[3]);
    if(zeroAreaRegion) return;

    /*
     * Image contains at least one non-transparent pixel.
     */

    for(i = 0; i < 2; ++i)
    {
        bright[i] = 0;
    }
    for(i = 0; i < 3; ++i)
    {
        average[i] = 0;
        lowAvg[i]  = 0;
    }

    src = buffer;
    // In paletted mode, the alpha channel follows the actual image.
    alphaSrc = buffer + width * height;

    // Skip to the start of the first column.
    if(region[2] > 0)
    {
        src      += pixelSize * width * region[2];
        alphaSrc += width * region[2];
    }

    for(y = region[2]; y <= region[3]; ++y)
    {
        // Skip to the beginning of the row.
        if(region[0] > 0)
        {
            src      += pixelSize * region[0];
            alphaSrc += region[0];
        }

        for(x = region[0]; x <= region[1]; ++x, src += pixelSize, alphaSrc++)
        {
            // Alpha pixels don't count. Why? -ds
            dd_bool const pixelIsTransparent = (pixelSize == 1? *alphaSrc < 255 :
                                                pixelSize == 4?    src[3] < 255 : false);

            if(pixelIsTransparent) continue;

            if(pixelSize == 1)
            {
                Vector3ub palColor = palette->color(*src);
                rgb[CR] = palColor.x;
                rgb[CG] = palColor.y;
                rgb[CB] = palColor.z;
            }
            else if(pixelSize >= 3)
            {
                memcpy(rgb, src, 3);
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
                for(c = 0; c < 3; ++c)
                    average[c] += rgb[c];
            }
            else
            {
                lowCnt++;
                for(c = 0; c < 3; ++c)
                    lowAvg[c] += rgb[c];
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
    int roundXDir = int( *retBrightX + .5f ) == int( *retBrightX )? 1 : -1;
    int roundYDir = int( *retBrightY + .5f ) == int( *retBrightY )? 1 : -1;

    // Apply all rounding and output as decimal.
    *retBrightX = (ROUND(*retBrightX) + .5f * roundXDir) / float( width );
    *retBrightY = (ROUND(*retBrightY) + .5f * roundYDir) / float( height );

    if(avgCnt || lowCnt)
    {
        // The color.
        if(!avgCnt)
        {
            // Low-intensity color average.
            for(c = 0; c < 3; ++c)
                retColor->rgb[c] = lowAvg[c] / lowCnt / 255.f;
        }
        else
        {
            // High-intensity color average.
            for(c = 0; c < 3; ++c)
                retColor->rgb[c] = average[c] / avgCnt / 255.f;
        }

        Vector3f color(retColor->rgb);
        R_AmplifyColor(color);
        for(int i = 0; i < 3; ++i)
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

D_CMD(SetRes)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    bool isFull = win->isFullScreen();

    int attribs[] = {
        isFull? ClientWindow::FullscreenWidth : ClientWindow::Width,
        atoi(argv[1]),

        isFull? ClientWindow::FullscreenHeight : ClientWindow::Height,
        atoi(argv[2]),

        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(SetFullRes)
{
    DENG2_UNUSED2(src, argc);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::FullscreenWidth,  atoi(argv[1]),
        ClientWindow::FullscreenHeight, atoi(argv[2]),
        ClientWindow::Fullscreen,       true,
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(SetWinRes)
{
    DENG2_UNUSED2(src, argc);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::Width,      atoi(argv[1]),
        ClientWindow::Height,     atoi(argv[2]),
        ClientWindow::Fullscreen, false,
        ClientWindow::Maximized,  false,
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(ToggleFullscreen)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::Fullscreen, !win->isFullScreen(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(ToggleMaximized)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::Maximized, !win->isMaximized(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(ToggleCentered)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::Centered, !win->isCentered(),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(CenterWindow)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::Centered, true,
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(SetBPP)
{
    DENG2_UNUSED2(src, argc);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    int attribs[] = {
        ClientWindow::ColorDepthBits, atoi(argv[1]),
        ClientWindow::End
    };
    return win->changeAttributes(attribs);
}

D_CMD(DisplayModeInfo)
{
    DENG2_UNUSED3(src, argc, argv);

    ClientWindow *win = ClientWindowSystem::mainPtr();

    if(!win)
        return false;

    DisplayMode const *mode = DisplayMode_Current();

    QString str = QString("Current display mode:%1 depth:%2 (%3:%4")
                      .arg(de::Vector2i(mode->width, mode->height).asText())
                      .arg(mode->depth)
                      .arg(mode->ratioX)
                      .arg(mode->ratioY);
    if(mode->refreshRate > 0)
    {
        str += QString(", refresh: %1 Hz").arg(mode->refreshRate, 0, 'f', 1);
    }
    str += QString(")\nMain window:\n  current origin:%1 size:%2"
                   "\n  windowed origin:%3 size:%4"
                   "\n  fullscreen size:%5")
                .arg(win->pos().asText())
                .arg(win->size().asText())
                .arg(win->windowRect().topLeft.asText())
                .arg(win->windowRect().size().asText())
                .arg(win->fullscreenSize().asText());
    str += QString("\n  fullscreen:%1 centered:%2 maximized:%3")
                .arg(win->isFullScreen()     ? "yes" : "no")
                .arg(win->isCentered()       ? "yes" : "no")
                .arg(win->isMaximized()      ? "yes" : "no");

    LOG_GL_MSG("%s") << str;
    return true;
}

D_CMD(ListDisplayModes)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_GL_MSG("There are %i display modes available:") << DisplayMode_Count();
    for(int i = 0; i < DisplayMode_Count(); ++i)
    {
        DisplayMode const *mode = DisplayMode_ByIndex(i);
        if(mode->refreshRate > 0)
        {
            LOG_GL_MSG("  %i x %i x %i " _E(>) "(%i:%i, refresh: %.1f Hz)")
                    << mode->width << mode->height << mode->depth
                    << mode->ratioX << mode->ratioY << mode->refreshRate;
        }
        else
        {
            LOG_GL_MSG("  %i x %i x %i (%i:%i)")
                    << mode->width << mode->height << mode->depth
                    << mode->ratioX << mode->ratioY;
        }
    }
    return true;
}

D_CMD(UpdateGammaRamp)
{
    DENG2_UNUSED3(src, argc, argv);

    GL_SetGamma();
    LOG_GL_VERBOSE("Gamma ramp set");
    return true;
}

D_CMD(Fog)
{
    DENG_UNUSED(src);

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
        for(int i = 0; i < 3; i++)
        {
            fogColor[i] = strtol(argv[2 + i], NULL, 0) / 255.0f;
        }
        fogColor[3] = 1;

        glFogfv(GL_FOG_COLOR, fogColor);
        LOG_GL_VERBOSE("Fog color set");
        return true;
    }
    if(!stricmp(argv[1], "start") && argc == 3)
    {
        glFogf(GL_FOG_START, (GLfloat) strtod(argv[2], NULL));
        LOG_GL_VERBOSE("Fog start distance set");
        return true;
    }
    if(!stricmp(argv[1], "end") && argc == 3)
    {
        glFogf(GL_FOG_END, (GLfloat) strtod(argv[2], NULL));
        LOG_GL_VERBOSE("Fog end distance set");
        return true;
    }
    if(!stricmp(argv[1], "density") && argc == 3)
    {
        glFogf(GL_FOG_DENSITY, (GLfloat) strtod(argv[2], NULL));
        LOG_GL_VERBOSE("Fog density set");
        return true;
    }
    if(!stricmp(argv[1], "mode") && argc == 3)
    {
        if(!stricmp(argv[2], "linear"))
        {
            glFogi(GL_FOG_MODE, GL_LINEAR);
            LOG_GL_VERBOSE("Fog mode set to linear");
            return true;
        }
        if(!stricmp(argv[2], "exp"))
        {
            glFogi(GL_FOG_MODE, GL_EXP);
            LOG_GL_VERBOSE("Fog mode set to exp");
            return true;
        }
        if(!stricmp(argv[2], "exp2"))
        {
            glFogi(GL_FOG_MODE, GL_EXP2);
            LOG_GL_VERBOSE("Fog mode set to exp2");
            return true;
        }
    }

    return false;
}
