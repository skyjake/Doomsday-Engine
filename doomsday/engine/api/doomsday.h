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

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use Sector to identify to sector to
 * change with the Map Update API.
 *
 * Define @c __INTERNAL_MAP_DATA_ACCESS__ if access to the internal map data
 * structures is needed.
 */
#ifndef __INTERNAL_MAP_DATA_ACCESS__
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

struct font_s;

#ifdef __cplusplus
} // extern "C"
#endif

#include "dd_share.h"
#include "dd_plugin.h"
#include "filehandle.h"
#include "point.h"
#include "rect.h"
#include "size.h"
#include <de/reader.h>
#include <de/writer.h>
#include <de/memoryzone.h>
#include <de/smoother.h>

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

/// @addtogroup base
///@{
    boolean         BusyMode_Active(void);
    timespan_t      BusyMode_ElapsedTime(void);
    int             BusyMode_RunTask(BusyTask* task);
    int             BusyMode_RunTasks(BusyTask* tasks, int numTasks);
    int             BusyMode_RunNewTask(int flags, busyworkerfunc_t worker, void* workerData);
    int             BusyMode_RunNewTaskWithName(int flags, busyworkerfunc_t worker, void* workerData, const char* taskName);

    void            BusyMode_WorkerEnd(void);
    void            BusyMode_WorkerError(const char* message);
///@}

/// @addtogroup game
///@{
/**
 * Register a new game.
 *
 * @param definition  GameDef structure defining the new game.
 * @return  Unique identifier/name assigned to resultant game.
 *
 * @note Game registration order defines the order of the automatic game
 * identification/selection logic.
 */
gameid_t DD_DefineGame(GameDef const* definition);

/**
 * Retrieves the game identifier for a previously defined game.
 * @see DD_DefineGame().
 *
 * @param identityKey  Identity key of the game.
 * @return Game identifier.
 */
gameid_t DD_GameIdForKey(char const* identityKey);

/**
 * Adds a new resource to the list for the identified @a game.
 *
 * @note Resource order defines the load order of resources (among those of
 * the same type). Resources are loaded from most recently added to least
 * recent.
 *
 * @param game      Unique identifier/name of the game.
 * @param rclass    Class of resource being added.
 * @param rflags    Resource flags (see @ref resourceFlags).
 * @param names     One or more known potential names, seperated by semicolon
 *                  (e.g., <pre> "name1;name2" </pre>). Valid names include
 *                  absolute or relative file paths, possibly with encoded
 *                  symbolic tokens, or predefined symbols into the virtual
 *                  file system.
 * @param params    Additional parameters. Usage depends on resource type.
 *                  For package resources this may be C-String containing a
 *                  semicolon delimited list of identity keys.
 */
void DD_AddGameResource(gameid_t game, resourceclass_t rclass, int rflags,
                        const char* names, void* params);

/**
 * Retrieve extended info about the current game.
 *
 * @param info      Info structure to be populated.
 * @return          @c true if successful else @c false (i.e., no game loaded).
 */
boolean DD_GameInfo(GameInfo* info);

    int _DECALL     DD_GetInteger(int ddvalue);
    void            DD_SetInteger(int ddvalue, int parm);
    void            DD_SetVariable(int ddvalue, void* ptr);
    void*           DD_GetVariable(int ddvalue);
    ddplayer_t*     DD_GetPlayer(int number);
///@}

/// @addtogroup namespace
///@{
texturenamespaceid_t DD_ParseTextureNamespace(const char* str);
materialnamespaceid_t DD_ParseMaterialNamespace(const char* str);
///@}

/**
 * @defgroup material Materials
 * @ingroup resource
 */
///@{
materialid_t DD_MaterialForTextureUniqueId(texturenamespaceid_t texNamespaceId, int uniqueId);
///@}

/// @addtogroup defs
///@{
    // Base: Definitions.
    int             Def_Get(int type, const char* id, void* out);
    int             Def_Set(int type, int index, int value, const void* ptr);
    int             Def_EvalFlags(char* flags);
///@}

/// @addtogroup input
///@{
    // Base: Input.
    void            DD_ClearKeyRepeaters(void);
    int             DD_GetKeyCode(const char* name);
///@}

/// @addtogroup fs
///@{
    // Base: File system.
    int             F_Access(const char* path);
    int             F_FileExists(const char* path);
    unsigned int    F_GetLastModified(const char* path);
    boolean         F_MakePath(const char* path);

    size_t          M_ReadFile(const char* path, char** buffer);
    boolean         M_WriteFile(const char* path, const char* source, size_t length);

    // Base: File system path/name utilities.
    void            F_FileName(ddstring_t* dst, const char* src);
    void            F_ExtractFileBase(char* dst, const char* path, size_t len);
    const char*     F_FindFileExtension(const char* path);

    boolean         F_TranslatePath(ddstring_t* dst, const ddstring_t* src);
    const char*     F_PrettyPath(const char* path);
///@}

//------------------------------------------------------------------------
//
// Console.
//
//------------------------------------------------------------------------

/// @addtogroup console
///@{
void Con_Open(int yes);
void Con_AddCommand(ccmdtemplate_t const* cmd);
void Con_AddVariable(cvartemplate_t const* var);
void Con_AddCommandList(ccmdtemplate_t const* cmdList);
void Con_AddVariableList(cvartemplate_t const* varList);

cvartype_t Con_GetVariableType(char const* name);

byte Con_GetByte(char const* name);
int Con_GetInteger(char const* name);
float Con_GetFloat(char const* name);
char const* Con_GetString(char const* name);
Uri const* Con_GetUri(char const* name);

void Con_SetInteger2(char const* name, int value, int svflags);
void Con_SetInteger(char const* name, int value);

void Con_SetFloat2(char const* name, float value, int svflags);
void Con_SetFloat(char const* name, float value);

void Con_SetString2(char const* name, char const* text, int svflags);
void Con_SetString(char const* name, char const* text);

void Con_SetUri2(char const* name, Uri const* uri, int svflags);
void Con_SetUri(char const* name, Uri const* uri);

void Con_Printf(char const* format, ...) PRINTF_F(1,2);
void Con_FPrintf(int flags, char const* format, ...) PRINTF_F(2,3);
void Con_PrintRuler(void);
void Con_Message(char const* message, ...) PRINTF_F(1,2);
void Con_Error(char const* error, ...) PRINTF_F(1,2);

void Con_SetPrintFilter(con_textfilter_t filter);

int DD_Execute(int silent, char const* command);
int DD_Executef(int silent, char const* command, ...);
///@}

/// @addtogroup bindings
///@{
    // Console: Bindings.
    void            B_SetContextFallback(const char* name, int (*responderFunc)(event_t*));
    int             B_BindingsForCommand(const char* cmd, char* buf, size_t bufSize);
    int             B_BindingsForControl(int localPlayer, const char* controlName, int inverse, char* buf, size_t bufSize);
///@}
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
///@{
    void            Sys_TicksPerSecond(float num);
    int             Sys_GetTime(void);
    double          Sys_GetSeconds(void);
    uint            Sys_GetRealTime(void);
    void            Sys_Sleep(int millisecs);
    int             Sys_CriticalMessage(char* msg);
    void            Sys_Quit(void);
///@}

//------------------------------------------------------------------------
//
// Map Edit.
//
//------------------------------------------------------------------------

/// @defgroup mapEdit Map Editor
/// @ingroup map
///@{
boolean MPE_Begin(const char* mapUri);
boolean MPE_End(void);

    uint            MPE_VertexCreate(coord_t x, coord_t y);
    boolean         MPE_VertexCreatev(size_t num, coord_t* values, uint* indices);
    uint            MPE_SidedefCreate(short flags, const ddstring_t* topMaterial, float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue, const ddstring_t* middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed, float middleGreen, float middleBlue, float middleAlpha, const ddstring_t* bottomMaterial, float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
    uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSector, uint backSector, uint frontSide, uint backSide, int flags);
    uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
    uint            MPE_PlaneCreate(uint sector, coord_t height, const ddstring_t* materialUri, float matOffsetX, float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ);
    uint            MPE_PolyobjCreate(uint* lines, uint linecount, int tag, int sequenceType, coord_t originX, coord_t originY);
    boolean         MPE_GameObjProperty(const char* objName, uint idx, const char* propName, valuetype_t type, void* data);
///@}

/// @addtogroup mobj
///@{
    // Custom map object data types.
    boolean         P_RegisterMapObj(int identifier, const char* name);
    boolean         P_RegisterMapObjProperty(int identifier, int propIdentifier, const char* propName, valuetype_t type);
///@}

//------------------------------------------------------------------------
//
// Networking.
//
//------------------------------------------------------------------------

/// @addtogroup network
///@{

/**
 * Send a packet over the network.
 *
 * @param to_player  Player number to send to. The server is number zero.
 *                   May include @ref netSendFlags.
 * @param type       Type of the packet.
 * @param data       Data of the packet.
 * @param length     Length of the data.
 */
void Net_SendPacket(int to_player, int type, const void* data, size_t length);

/**
 * @return The name of player @a player.
 */
const char* Net_GetPlayerName(int player);

/**
 * @return Client identifier for player @a player.
 */
ident_t Net_GetPlayerID(int player);

/**
 * Provides access to the player's movement smoother.
 */
Smoother* Net_PlayerSmoother(int player);

/**
 * Determines whether the coordinates sent by a player are valid at the moment.
 */
boolean Sv_CanTrustClientPos(int player);

/**
 * Searches through the client mobj hash table for the CURRENT map and
 * returns the clmobj with the specified ID, if that exists. Note that
 * client mobjs are also linked to the thinkers list.
 *
 * @param id  Mobj identifier.
 *
 * @return  Pointer to the mobj.
 */
struct mobj_s* ClMobj_Find(thid_t id);

/**
 * Enables or disables local action function execution on the client.
 *
 * @param mo  Client mobj.
 * @param enable  @c true to enable local actions, @c false to disable.
 */
void ClMobj_EnableLocalActions(struct mobj_s* mo, boolean enable);

/**
 * Determines if local action functions are enabled for client mobj @a mo.
 */
boolean ClMobj_LocalActionsEnabled(struct mobj_s* mo);

///@}

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

int LineDef_BoxOnSide(LineDef* line, const AABoxd* box);

coord_t LineDef_PointDistance(LineDef* lineDef, coord_t const point[2], coord_t* offset);
coord_t LineDef_PointXYDistance(LineDef* lineDef, coord_t x, coord_t y, coord_t* offset);

coord_t LineDef_PointOnSide(LineDef* lineDef, coord_t const point[2]);
coord_t LineDef_PointXYOnSide(LineDef* lineDef, coord_t x, coord_t y);

void LineDef_SetDivline(LineDef* lineDef, divline_t* dl);

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

/**
 * @defgroup controls Controls
 * @ingroup input
 */
///@{
// Play: Controls.
void P_NewPlayerControl(int id, controltype_t type, const char* name, const char* bindContext);
void P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset);
int P_GetImpulseControlState(int playerNum, int control);
void P_Impulse(int playerNum, int control);
///@}

/// @addtogroup map
///@{
// Play: Setup.
boolean P_MapExists(char const* uri);
boolean P_MapIsCustom(char const* uri);
AutoStr* P_MapSourceFile(char const* uri);

boolean P_LoadMap(const char* uri);
///@}

// Play: World data access (Map Data Updates and access to other information).
#include "dd_world.h"

/// @addtogroup playsim
///@{
// Play: Misc.
void P_SpawnDamageParticleGen(struct mobj_s* mo, struct mobj_s* inflictor, int amount);
///@}

/// @addtogroup mobj
///@{
// Play: Mobjs.
struct mobj_s* P_MobjCreateXYZ(think_t function, coord_t x, coord_t y, coord_t z, angle_t angle, coord_t radius, coord_t height, int ddflags);

void P_MobjDestroy(struct mobj_s* mo);

void P_MobjSetState(struct mobj_s* mo, int statenum);

void P_MobjLink(struct mobj_s* mo, byte flags);

int P_MobjUnlink(struct mobj_s* mo);

struct mobj_s* P_MobjForID(int id);

void Mobj_OriginSmoothed(struct mobj_s* mobj, coord_t origin[3]);

angle_t Mobj_AngleSmoothed(struct mobj_s* mobj);

boolean ClMobj_IsValid(struct mobj_s* mo);

struct mobj_s* ClPlayer_ClMobj(int plrNum);

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
Uri* Materials_ComposeUri(materialid_t materialId);

///@}

//------------------------------------------------------------------------
//
// UI.
//
//------------------------------------------------------------------------

/// @addtogroup gl
///@{

fontid_t Fonts_ResolveUri(const Uri* uri);

///@}

//------------------------------------------------------------------------
//
// Refresh.
//
//------------------------------------------------------------------------

/**
 * Determines whether the current run of the thinkers should be considered a
 * "sharp" tick. Sharp ticks occur exactly 35 times per second. Thinkers may be
 * called at any rate faster than this; in order to retain compatibility with
 * the original Doom engine game logic that ran at 35 Hz, such logic should
 * only be executed on sharp ticks.
 *
 * @return @c true, if a sharp tick is currently in effect.
 *
 * @ingroup playsim
 */
boolean DD_IsSharpTick(void);

/**
 * @defgroup render Renderer
 */
///@{

int DD_GetFrameRate(void);

void R_SetupMap(int mode, int flags);
void R_SetupFogDefaults(void);
void R_SetupFog(float start, float end, float density, float* rgb);

void R_PrecacheMobjNum(int mobjtypeNum);
void R_PrecacheModelsForState(int stateIndex);

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
 * @param player  Console player number of whose viewport to change.
 * @param viewPlayer  Player that will be viewed by @a player.
 */
void R_SetViewPortPlayer(int player, int viewPlayer);

boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);
scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

boolean R_GetSpriteInfo(int sprite, int frame, spriteinfo_t* sprinfo);

patchid_t R_DeclarePatch(const char* name);
boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info);
Uri* R_ComposePatchUri(patchid_t id);
AutoStr* R_ComposePatchPath(patchid_t id);

int R_TextureUniqueId2(const Uri* uri, boolean quiet);
int R_TextureUniqueId(const Uri* uri); /*quiet=false*/

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
// Graphics.
//
//------------------------------------------------------------------------

/// @addtogroup gl
///@{

void GL_UseFog(int yes);
byte* GL_GrabScreen(void);
void GL_SetFilter(boolean enable);
void GL_SetFilterColor(float r, float g, float b, float a);

void GL_ConfigureBorderedProjection2(borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
void GL_ConfigureBorderedProjection(borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);
void GL_BeginBorderedProjection(borderedprojectionstate_t* bp);
void GL_EndBorderedProjection(borderedprojectionstate_t* bp);

///@}

//------------------------------------------------------------------------
//
// Audio.
//
//------------------------------------------------------------------------

/// @addtogroup audio
///@{

void S_MapChange(void);
int S_LocalSoundAtVolumeFrom(int sound_id, struct mobj_s* origin, coord_t* pos, float volume);
int S_LocalSoundAtVolume(int soundID, struct mobj_s* origin, float volume);
int S_LocalSound(int soundID, struct mobj_s* origin);
int S_LocalSoundFrom(int soundID, coord_t* fixedpos);
int S_StartSound(int soundId, struct mobj_s* origin);
int S_StartSoundEx(int soundId, struct mobj_s* origin);
int S_StartSoundAtVolume(int soundID, struct mobj_s* origin, float volume);
int S_ConsoleSound(int soundID, struct mobj_s* origin, int targetConsole);

void S_StopSound2(int soundID, struct mobj_s* origin, int flags);
void S_StopSound(int soundID, struct mobj_s* origin/*,flags=0*/);

int S_IsPlaying(int soundID, struct mobj_s* origin);
int S_StartMusic(const char* musicID, boolean looped);
int S_StartMusicNum(int id, boolean looped);
void S_StopMusic(void);
void S_PauseMusic(boolean doPause);

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

/// @addtogroup base
///@{

char* M_SkipWhite(char* str);
char* M_FindWhite(char* str);
char* M_StrCatQuoted(char* dest, const char* src, size_t len);
boolean M_IsStringValidInt(const char* str);
boolean M_IsStringValidByte(const char* str);
boolean M_IsStringValidFloat(const char* str);

///@}

/// @addtogroup math
///@{

double M_ApproxDistance(double dx, double dy);
double M_ApproxDistance3(double dx, double dy, double dz);

angle_t M_PointXYToAngle(double x, double y);

angle_t M_PointToAngle2(double const a[2], double const b[2]);
angle_t M_PointXYToAngle2(double x1, double y1, double x2, double y2);

void V2d_Copy(double dest[2], double const src[2]);
void V2d_Scale(double vector[2], double scalar);
void V2d_Sum(double dest[2], double const a[2], double const b[2]);
void V2d_Subtract(double dest[2], double const a[2], double const b[2]);
void V2d_Rotate(double vec[2], double radians);
double V2d_PointOnLineSide(double const point[2], double const lineOrigin[2], double const lineDirection[2]);
double V2d_PointLineDistance(double const point[2], double const linePoint[2], double const lineDirection[2], double* offset);
double V2d_ProjectOnLine(double dest[2], double const point[2], double const lineOrigin[2], double const lineDirection[2]);
double V2d_Intersection(double const linePointA[2], double const lineDirectionA[2], double const linePointB[2], double const lineDirectionB[2], double point[2]);

int M_RatioReduce(int* numerator, int* denominator);
int M_CeilPow2(int num);
int M_NumDigits(int value);

binangle_t bamsAtan2(int y, int x);

///@}

/// @addtogroup base
///@{

    // Miscellaneous: Random Number Generator facilities.
    byte            RNG_RandByte(void);
    float           RNG_RandFloat(void);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_EXPORTS_H */
