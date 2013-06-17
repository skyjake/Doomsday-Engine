#define DENG_NO_API_MACROS_RENDER

#include "de_base.h"
#include "de_console.h"

#include "api_render.h"

// m_misc.c
DENG_EXTERN_C int M_ScreenShot(const char* name, int bits);

// models.cpp
DENG_EXTERN_C void Models_CacheForState(int stateIndex);

// r_draw.cpp
DENG_EXTERN_C void R_SetBorderGfx(struct uri_s const *const *paths);

// r_main.cpp
DENG_EXTERN_C void Rend_CacheForMobjType(int mobjtypeNum);
DENG_EXTERN_C void R_RenderPlayerView(int num);
DENG_EXTERN_C void R_SetViewOrigin(int consoleNum, coord_t const origin[3]);
DENG_EXTERN_C void R_SetViewAngle(int consoleNum, angle_t angle);
DENG_EXTERN_C void R_SetViewPitch(int consoleNum, float pitch);
DENG_EXTERN_C int R_ViewWindowGeometry(int consoleNum, RectRaw *geometry);
DENG_EXTERN_C int R_ViewWindowOrigin(int consoleNum, Point2Raw *origin);
DENG_EXTERN_C int R_ViewWindowSize(int consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewWindowGeometry(int consoleNum, RectRaw const *geometry, boolean interpolate);
DENG_EXTERN_C int R_ViewPortGeometry(int consoleNum, RectRaw *geometry);
DENG_EXTERN_C int R_ViewPortOrigin(int consoleNum, Point2Raw *origin);
DENG_EXTERN_C int R_ViewPortSize(int consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

// sky.cpp
DENG_EXTERN_C void R_SkyParams(int layer, int param, void *data);

// r_things.cpp
DENG_EXTERN_C boolean R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *sprinfo);

// r_util.c
DENG_EXTERN_C boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);
DENG_EXTERN_C scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
DENG_EXTERN_C scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

#undef R_SetupFog
DENG_EXTERN_C void R_SetupFog(float start, float end, float density, float *rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

#undef R_SetupFogDefaults
DENG_EXTERN_C void R_SetupFogDefaults()
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

DENG_DECLARE_API(Rend) =
{
    { DE_API_RENDER },
    R_SetupFogDefaults,
    R_SetupFog,
    Rend_CacheForMobjType,
    Models_CacheForState,
    R_RenderPlayerView,
    R_SetViewOrigin,
    R_SetViewAngle,
    R_SetViewPitch,
    R_ViewWindowGeometry,
    R_ViewWindowOrigin,
    R_ViewWindowSize,
    R_SetViewWindowGeometry,
    R_SetBorderGfx,
    R_ViewPortGeometry,
    R_ViewPortOrigin,
    R_ViewPortSize,
    R_SetViewPortPlayer,
    R_ChooseAlignModeAndScaleFactor,
    R_ChooseScaleMode2,
    R_ChooseScaleMode,
    R_GetSpriteInfo,
    R_SkyParams,
    M_ScreenShot
};
