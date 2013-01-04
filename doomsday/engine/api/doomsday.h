/**
 * @file doomsday.h
 * Primary header file for the Doomsday Engine Public API
 *
 * @todo Break this header file up into group-specific ones.
 * Including doomsday.h should include all of the public API headers.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * @mainpage libdeng API
 *
 * This documentation covers all the functions and data that Doomsday makes
 * available for games and other plugins.
 *
 * @section Overview
 * The documentation has been organized into <a href="modules.html">modules</a>.
 * The primary ones are listed below:
 * - @ref base
 * - @ref console
 * - @ref input
 * - @ref network
 * - @ref resource
 * - @ref render
 */

#ifndef LIBDENG_EXPORTS_H
#define LIBDENG_EXPORTS_H

// The calling convention.
#if defined(WIN32)
#   define _DECALL  __cdecl
#elif defined(UNIX)
#   define _DECALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct font_s;

#ifdef __cplusplus
} // extern "C"
#endif

#include "dd_share.h"
#include "api_base.h"
#include "api_busy.h"
#include "api_plugin.h"
#include "api_def.h"
#include "api_event.h"
#include "api_filesys.h"
#include "api_console.h"
#include "api_material.h"
#include "api_fontrender.h"
#include "api_svg.h"
#include "api_sound.h"
#include "api_map.h"
#include "api_mapedit.h"
#include "api_client.h"
#include "api_server.h"

#include "filehandle.h"
#include <de/memoryzone.h>
#include <de/point.h>
#include <de/reader.h>
#include <de/rect.h>
#include <de/size.h>
#include <de/smoother.h>
#include <de/vector1.h>
#include <de/writer.h>

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use Sector to identify to sector to
 * change with the Map Update API.
 *
 * Define @c DENG_INTERNAL_MAP_DATA_ACCESS if access to the internal map data
 * structures is needed.
 */
#ifndef DENG_INTERNAL_MAP_DATA_ACCESS
typedef struct bspnode_s  { int type; } BspNode;
typedef struct vertex_s   { int type; } Vertex;
typedef struct linedef_s  { int type; } LineDef;
typedef struct sidedef_s  { int type; } SideDef;
typedef struct hedge_s    { int type; } HEdge;
typedef struct bspleaf_s  { int type; } BspLeaf;
typedef struct sector_s   { int type; } Sector;
typedef struct plane_s    { int type; } Plane;
typedef struct material_s { int type; } material_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------
//
// Base.
//
//------------------------------------------------------------------------

/**
 * @defgroup base Base
 */

/**
 * @defgroup defs Definitions
 * @ingroup base
 */

/**
 * @defgroup fs File System
 * @ingroup base
 */

//------------------------------------------------------------------------
//
// System.
//
//------------------------------------------------------------------------

/**
 * @defgroup system System Routines
 * @ingroup base
 * Functionality provided by or related to the operating system.
 */

//------------------------------------------------------------------------
//
// Playsim.
//
//------------------------------------------------------------------------

/**
 * @defgroup playsim Playsim
 * @ingroup game
 */
///@{

int Divline_PointOnSide(const divline_t* line, coord_t const point[2]);
int Divline_PointXYOnSide(const divline_t* line, coord_t x, coord_t y);

fixed_t Divline_Intersection(divline_t* v1, divline_t* v2);

const divline_t* P_TraceLOS(void);

TraceOpening* P_TraceOpening(void);

void P_SetTraceOpening(struct linedef_s* linedef);

BspLeaf* P_BspLeafAtPoint(coord_t const point[2]);
BspLeaf* P_BspLeafAtPointXY(coord_t x, coord_t y);

// Object in bounding box iterators.
int P_MobjsBoxIterator(const AABoxd* box, int (*callback) (struct mobj_s*, void*), void* parameters);

int P_LinesBoxIterator(const AABoxd* box, int (*callback) (struct linedef_s*, void*), void* parameters);

int P_AllLinesBoxIterator(const AABoxd* box, int (*callback) (struct linedef_s*, void*), void* parameters);

int P_PolyobjLinesBoxIterator(const AABoxd* box, int (*callback) (struct linedef_s*, void*), void* parameters);

int P_BspLeafsBoxIterator(const AABoxd* box, Sector* sector, int (*callback) (struct bspleaf_s*, void*), void* parameters);

int P_PolyobjsBoxIterator(const AABoxd* box, int (*callback) (struct polyobj_s*, void*), void* parameters);

// Object type touching mobjs iterators.
int P_LineMobjsIterator(struct linedef_s* line, int (*callback) (struct mobj_s*, void *), void* parameters);

int P_SectorTouchingMobjsIterator(struct sector_s* sector, int (*callback) (struct mobj_s*, void*), void* parameters);

int P_PathTraverse2(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback, void* parameters);
int P_PathTraverse(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback/*parameters=NULL*/);

int P_PathXYTraverse2(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback, void* parameters);
int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback/*parameters=NULL*/);

boolean P_CheckLineSight(coord_t const from[3], coord_t const to[3], coord_t bottomSlope, coord_t topSlope, int flags);
///@}

/// @addtogroup map
///@{
// Play: Setup.
boolean P_MapExists(char const* uri);
boolean P_MapIsCustom(char const* uri);
AutoStr* P_MapSourceFile(char const* uri);

boolean P_LoadMap(const char* uri);
///@}

/// @addtogroup playsim
///@{
// Play: Misc.
void P_SpawnDamageParticleGen(struct mobj_s* mo, struct mobj_s* inflictor, int amount);
///@}

/// @addtogroup mobj
///@{
// Play: Mobjs.
struct mobj_s* P_MobjCreateXYZ(thinkfunc_t function, coord_t x, coord_t y, coord_t z, angle_t angle, coord_t radius, coord_t height, int ddflags);

void P_MobjDestroy(struct mobj_s* mo);

void P_MobjSetState(struct mobj_s* mo, int statenum);

void P_MobjLink(struct mobj_s* mo, byte flags);

int P_MobjUnlink(struct mobj_s* mo);

struct mobj_s* P_MobjForID(int id);

void Mobj_OriginSmoothed(struct mobj_s* mobj, coord_t origin[3]);

angle_t Mobj_AngleSmoothed(struct mobj_s* mobj);

// Mobj linked object iterators.
int P_MobjLinesIterator(struct mobj_s* mo, int (*callback) (struct linedef_s*, void*), void* parameters);

int P_MobjSectorsIterator(struct mobj_s* mo, int (*callback) (Sector*, void*), void* parameters);

///@}

/**
 * @defgroup polyobj Polygon Objects
 * @ingroup map
 */
///@{

// Play: Polyobjs.
boolean P_PolyobjMoveXY(struct polyobj_s* po, coord_t x, coord_t y);
boolean P_PolyobjRotate(struct polyobj_s* po, angle_t angle);
void P_PolyobjLink(struct polyobj_s* po);
void P_PolyobjUnlink(struct polyobj_s* po);

struct polyobj_s* P_PolyobjByID(uint id);
struct polyobj_s* P_PolyobjByTag(int tag);
void P_SetPolyobjCallback(void (*func)(struct mobj_s*, void*, void*));

///@}

/// @addtogroup material
///@{

// Play: Materials.
materialid_t Materials_ResolveUri(const Uri* uri);
materialid_t Materials_ResolveUriCString(const char* path);

///@}

//------------------------------------------------------------------------
//
// Refresh.
//
//------------------------------------------------------------------------

/**
 * @defgroup render Renderer
 */
///@{

int DD_GetFrameRate(void);

void R_SetupMap(int mode, int flags);
void R_SetupFogDefaults(void);
void R_SetupFog(float start, float end, float density, float* rgb);

void Rend_CacheForMobjType(int mobjtypeNum);
void Models_CacheForState(int stateIndex);

void R_RenderPlayerView(int num);

void R_SetViewOrigin(int player, coord_t const origin[3]);
void R_SetViewAngle(int player, angle_t angle);
void R_SetViewPitch(int player, float pitch);

/**
 * Retrieve the geometry of the specified viewwindow by console player num.
 */
int R_ViewWindowGeometry(int player, RectRaw* geometry);
int R_ViewWindowOrigin(int player, Point2Raw* origin);
int R_ViewWindowSize(int player, Size2Raw* size);

void R_SetViewWindowGeometry(int player, const RectRaw* geometry, boolean interpolate);

void R_SetBorderGfx(const Uri* const* paths);

/**
 * Retrieve the geometry of the specified viewport by console player num.
 */
int R_ViewPortGeometry(int player, RectRaw* geometry);
int R_ViewPortOrigin(int player, Point2Raw* origin);
int R_ViewPortSize(int player, Size2Raw* size);

/**
 * Change the view player for the specified viewport by console player num.
 *
 * @param consoleNum  Console player number of whose viewport to change.
 * @param viewPlayer  Player that will be viewed by @a player.
 */
void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);
scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

boolean R_GetSpriteInfo(int sprite, int frame, spriteinfo_t* sprinfo);

patchid_t R_DeclarePatch(const char* name);
boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info);
Uri* R_ComposePatchUri(patchid_t id);
AutoStr* R_ComposePatchPath(patchid_t id);

int Textures_UniqueId2(const Uri* uri, boolean quiet);
int Textures_UniqueId(const Uri* uri); /*quiet=false*/

int R_CreateAnimGroup(int flags);
void R_AddAnimGroupFrame(int groupNum, const Uri* texture, int tics, int randomTics);

colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name, const uint8_t* colorData, int colorCount);
colorpaletteid_t R_GetColorPaletteNumForName(const char* name);
const char* R_GetColorPaletteNameForNum(colorpaletteid_t id);

void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], boolean applyTexGamma);
void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3], boolean applyTexGamma);

void R_HSVToRGB(float* rgb, float h, float s, float v);

///@}

//------------------------------------------------------------------------
//
// Renderer.
//
//------------------------------------------------------------------------

/// @addtogroup render
///@{

void R_SkyParams(int layer, int param, void* data);

///@}

//------------------------------------------------------------------------
//
// Miscellaneous.
//
//------------------------------------------------------------------------

/**
 * @ingroup render
 */
int M_ScreenShot(const char* filename, int bits);

/// @addtogroup math
///@{

binangle_t bamsAtan2(int y, int x);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_EXPORTS_H */
