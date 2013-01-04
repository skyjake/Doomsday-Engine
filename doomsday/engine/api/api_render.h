#ifndef DOOMSDAY_API_RENDER_H
#define DOOMSDAY_API_RENDER_H

#include "apis.h"
#include "api_gl.h"

DENG_API_TYPEDEF(Rend)
{
    de_api_t api;

    /**
     * Called by the game at various points in the map setup process.
     */
    void (*SetupMap)(int mode, int flags);
    void (*SetupFogDefaults)(void);
    void (*SetupFog)(float start, float end, float density, float* rgb);

    /**
     * Prepare all texture resources for the specified mobjtype.
     */
    void (*CacheForMobjType)(int mobjtypeNum);

    void (*CacheModelsForState)(int stateIndex);

    /**
     * Draw the view of the player inside the view window.
     */
    void (*RenderPlayerView)(int num);

    /**
     * Update the view origin position for player @a consoleNum.
     *
     * @param consoleNum  Console number.
     */
    void (*SetViewOrigin)(int player, coord_t const origin[3]);

    /**
     * Update the view yaw angle for player @a consoleNum.
     *
     * @param consoleNum  Console number.
     */
    void (*SetViewAngle)(int player, angle_t angle);

    /**
     * Update the view pitch angle for player @a consoleNum.
     *
     * @param consoleNum  Console number.
     */
    void (*SetViewPitch)(int player, float pitch);

    /**
     * Retrieve the geometry of the specified viewwindow by console player num.
     */
    int (*ViewWindowGeometry)(int player, RectRaw* geometry);
    int (*ViewWindowOrigin)(int player, Point2Raw* origin);
    int (*ViewWindowSize)(int player, Size2Raw* size);

    void (*SetViewWindowGeometry)(int player, const RectRaw* geometry, boolean interpolate);

    void (*SetBorderGfx)(const Uri* const* paths);

    /**
     * Retrieve the geometry of the specified viewport by console player num.
     */
    int (*ViewPortGeometry)(int player, RectRaw* geometry);
    int (*ViewPortOrigin)(int player, Point2Raw* origin);
    int (*ViewPortSize)(int player, Size2Raw* size);

    /**
     * Change the view player for the specified viewport by console player num.
     *
     * @param consoleNum  Console player number of whose viewport to change.
     * @param viewPlayer  Player that will be viewed by @a player.
     */
    void (*SetViewPortPlayer)(int consoleNum, int viewPlayer);

    /**
     * Choose an alignment mode and/or calculate the appropriate scaling factor
     * for fitting an element within the bounds of the "available" region.
     * The aspect ratio of the element is respected.
     *
     * @param scale  If not @c NULL the calculated scale factor is written here.
     * @param width  Width of the element to fit into the available region.
     * @param height  Height of the element to fit into the available region.
     * @param availWidth  Width of the available region.
     * @param availHeight  Height of the available region.
     * @param scaleMode  @ref scaleModes
     *
     * @return  @c true if aligning to the horizontal axis else the vertical.
     */
    boolean (*ChooseAlignModeAndScaleFactor)(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);

    /**
     * Choose a scale mode by comparing the dimensions of the two, two-dimensional
     * regions. The aspect ratio is respected when fitting to the bounds of the
     * "available" region.
     *
     * @param width  Width of the element to fit into the available region.
     * @param height  Height of the element to fit into the available region.
     * @param availWidth  Width of the available region.
     * @param availHeight  Height of the available region.
     * @param overrideMode  Scale mode override, for caller-convenience. @ref scaleModes
     * @param stretchEpsilon  Range within which aspect ratios are considered
     *      identical for "smart stretching".
     *
     * @return  Chosen scale mode @ref scaleModes.
     */
    scalemode_t (*ChooseScaleMode2)(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);

    scalemode_t (*ChooseScaleMode)(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

    boolean (*GetSpriteInfo)(int sprite, int frame, spriteinfo_t* sprinfo);

    /**
     * Alternative interface for manipulating Sky (layer) properties by name/id.
     */
    void (*SkyParams)(int layer, int param, void* data);

    /**
     * Grabs the current contents of the frame buffer and outputs it in a file.
     * Will create/overwrite as necessary.
     *
     * @param filename  Local file path to write to.
     */
    int (*ScreenShot)(const char* filename, int bits);
}
DENG_API_T(Rend);

#ifndef DENG_NO_API_MACROS_RENDER
#define R_SetupMap                  _api_Rend.SetupMap
#define R_SetupFogDefaults          _api_Rend.SetupFogDefaults
#define R_SetupFog                  _api_Rend.SetupFog
#define Rend_CacheForMobjType		_api_Rend.CacheForMobjType
#define Models_CacheForState		_api_Rend.CacheModelsForState
#define R_RenderPlayerView          _api_Rend.RenderPlayerView
#define R_SetViewOrigin             _api_Rend.SetViewOrigin
#define R_SetViewAngle              _api_Rend.SetViewAngle
#define R_SetViewPitch              _api_Rend.SetViewPitch
#define R_ViewWindowGeometry		_api_Rend.ViewWindowGeometry
#define R_ViewWindowOrigin          _api_Rend.ViewWindowOrigin
#define R_ViewWindowSize            _api_Rend.ViewWindowSize
#define R_SetViewWindowGeometry		_api_Rend.SetViewWindowGeometry
#define R_SetBorderGfx              _api_Rend.SetBorderGfx
#define R_ViewPortGeometry          _api_Rend.ViewPortGeometry
#define R_ViewPortOrigin            _api_Rend.ViewPortOrigin
#define R_ViewPortSize              _api_Rend.ViewPortSize
#define R_SetViewPortPlayer         _api_Rend.SetViewPortPlayer
#define R_ChooseAlignModeAndScaleFactor		_api_Rend.ChooseAlignModeAndScaleFactor
#define R_ChooseScaleMode2          _api_Rend.ChooseScaleMode2
#define R_ChooseScaleMode           _api_Rend.ChooseScaleMode
#define R_GetSpriteInfo             _api_Rend.GetSpriteInfo
#define R_SkyParams                 _api_Rend.SkyParams
#define M_ScreenShot                _api_Rend.ScreenShot
#endif

#if defined __DOOMSDAY__ && defined __CLIENT__
DENG_USING_API(Rend);
#endif

#endif // DOOMSDAY_API_RENDER_H
