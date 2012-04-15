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
    typedef struct bspnode_s { int type; } BspNode;
    typedef struct vertex_s {int type; } Vertex;
    typedef struct linedef_s { int type; } LineDef;
    typedef struct sidedef_s { int type; } SideDef;
    typedef struct hedge_s { int type; } HEdge;
    typedef struct bspleaf_s { int type; } BspLeaf;
    typedef struct sector_s { int type; } Sector;
    typedef struct plane_s { int type; } Plane;
    typedef struct material_s { int type; } material_t;
#endif

struct font_s;

#include "dd_share.h"
#include "dd_plugin.h"
#include "dfile.h"
#include "point.h"
#include "reader.h"
#include "rect.h"
#include "size.h"
#include "smoother.h"
#include "stringpool.h"
#include "writer.h"

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
gameid_t DD_DefineGame(const GameDef* definition);

/**
 * Retrieves the game identifier for a previously defined game.
 * @see DD_DefineGame().
 *
 * @param identityKey  Identity key of the game.
 * @return Game identifier.
 */
gameid_t DD_GameIdForKey(const char* identityKey);

/**
 * Registers a new resource for the specified game.
 *
 * @param game          Unique identifier/name of the game.
 * @param rclass        Class of resource being added.
 * @param rflags        Resource flags (see @ref resourceFlags).
 * @param names         One or more known potential names, seperated by semicolon e.g., "name1;name2".
 *                      Names may include valid absolute, or relative file paths. These paths include
 *                      valid symbolbolic escape tokens, predefined symbols into the virtual file system.
 * @param params        Additional parameters.
 *
 * @note Resource registration order defines the load order of resources
 * (among those of the same type).
 */
void DD_AddGameResource(gameid_t game, resourceclass_t rclass, int rflags, const char* names, void* params);

/**
 * Retrieve extended info about the current game.
 *
 * @param info          Info structure to be populated.
 * @return              @c true if successful else @c false (i.e., no game loaded).
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

/// @addtogroup memzone
///@{
    // Base: Zone.
    void* _DECALL   Z_Malloc(size_t size, int tag, void* ptr);
    void* _DECALL   Z_Calloc(size_t size, int tag, void* user);
    void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
    void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
    void _DECALL    Z_Free(void* ptr);
    void            Z_FreeTags(int lowTag, int highTag);
    void            Z_ChangeTag2(void* ptr, int tag);
    void            Z_CheckHeap(void);
///@}

//------------------------------------------------------------------------
//
// Console.
//
//------------------------------------------------------------------------

/// @addtogroup console
///@{
    int             Con_Busy(int flags, const char* taskName, int (*workerFunc)(void*), void* workerData);
    void            Con_BusyWorkerEnd(void);
    boolean         Con_IsBusy(void);
    void            Con_Open(int yes);
    void            Con_AddCommand(const ccmdtemplate_t* cmd);
    void            Con_AddVariable(const cvartemplate_t* var);
    void            Con_AddCommandList(const ccmdtemplate_t* cmdList);
    void            Con_AddVariableList(const cvartemplate_t* varList);

cvartype_t Con_GetVariableType(const char* name);

byte Con_GetByte(const char* name);
int Con_GetInteger(const char* name);
float Con_GetFloat(const char* name);
char* Con_GetString(const char* name);
Uri* Con_GetUri(const char* name);

void Con_SetInteger2(const char* name, int value, int svflags);
void Con_SetInteger(const char* name, int value);

void Con_SetFloat2(const char* name, float value, int svflags);
void Con_SetFloat(const char* name, float value);

void Con_SetString2(const char* name, const char* text, int svflags);
void Con_SetString(const char* name, const char* text);

void Con_SetUri2(const char* name, const Uri* uri, int svflags);
void Con_SetUri(const char* name, const Uri* uri);

void Con_Printf(const char* format, ...) PRINTF_F(1,2);
void Con_FPrintf(int flags, const char* format, ...) PRINTF_F(2,3);
void Con_PrintRuler(void);
void Con_Message(const char* message, ...) PRINTF_F(1,2);
void Con_Error(const char* error, ...) PRINTF_F(1,2);

void Con_SetPrintFilter(con_textfilter_t filter);

    int             DD_Execute(int silent, const char* command);
    int             DD_Executef(int silent, const char* command, ...);
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
    uint            MPE_SidedefCreate(uint sector, short flags, materialid_t topMaterial, float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue, materialid_t middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed, float middleGreen, float middleBlue, float middleAlpha, materialid_t bottomMaterial, float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
    uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide, int flags);
    uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
    uint            MPE_PlaneCreate(uint sector, float height, materialid_t materialId, float matOffsetX, float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ);
    uint            MPE_PolyobjCreate(uint* lines, uint linecount, int tag, int sequenceType, float anchorX, float anchorY);
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

/// @addtogroup math
///@{
    float           P_AccurateDistance(float dx, float dy);
    float           P_ApproxDistance(float dx, float dy);
    float           P_ApproxDistance3(float dx, float dy, float dz);
///@}

/**
 * @defgroup playsim Playsim
 * @ingroup game
 */
///@{

    int             P_PointOnLineDefSide(float const xy[2], const struct linedef_s* lineDef);
    int             P_PointXYOnLineDefSide(float x, float y, const struct linedef_s* lineDef);

    int             P_BoxOnLineSide(const AABoxf* box, const struct linedef_s* ld);
    void            P_MakeDivline(struct linedef_s* li, divline_t* dl);
    int             P_PointOnDivlineSide(float x, float y, const divline_t* line);
    float           P_InterceptVector(divline_t* v2, divline_t* v1);

    const divline_t* P_TraceLOS(void);
    TraceOpening*   P_TraceOpening(void);
    void            P_SetTraceOpening(struct linedef_s* linedef);

    struct bspleaf_s* P_BspLeafAtPointXY(float x, float y);

    // Object in bounding box iterators.
    int             P_MobjsBoxIterator(const AABoxf* box,
                                       int (*func) (struct mobj_s*, void*),
                                       void* data);
    int             P_LinesBoxIterator(const AABoxf* box,
                                       int (*func) (struct linedef_s*, void*),
                                       void* data);
    int             P_AllLinesBoxIterator(const AABoxf* box,
                                          int (*func) (struct linedef_s*, void*),
                                          void* data);
    int             P_BspLeafsBoxIterator(const AABoxf* box, Sector* sector,
                                           int (*func) (BspLeaf*, void*),
                                           void* data);
    int             P_PolyobjsBoxIterator(const AABoxf* box,
                                          int (*func) (struct polyobj_s*, void*),
                                          void* data);

    // Object type touching mobjs iterators.
    int             P_LineMobjsIterator(struct linedef_s* line,
                                        int (*func) (struct mobj_s*, void *), void* data);
    int             P_SectorTouchingMobjsIterator
                        (Sector* sector, int (*func) (struct mobj_s*, void*),
                         void* data);

    int             P_PathTraverse(float const from[2], float const to[2], int flags, traverser_t callback); /*paramaters=NULL*/
    int             P_PathTraverse2(float const from[2], float const to[2], int flags, traverser_t callback, void* paramaters);

    int             P_PathXYTraverse(float fromX, float fromY, float toX, float toY, int flags, traverser_t callback); /*paramaters=NULL*/
    int             P_PathXYTraverse2(float fromX, float fromY, float toX, float toY, int flags, traverser_t callback, void* paramaters);

    boolean         P_CheckLineSight(const float from[3], const float to[3], float bottomSlope, float topSlope, int flags);
///@}

/**
 * @defgroup controls Controls
 * @ingroup input
 */
///@{
    // Play: Controls.
    void            P_NewPlayerControl(int id, controltype_t type, const char* name, const char* bindContext);
    void            P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset);
    int             P_GetImpulseControlState(int playerNum, int control);
    void            P_Impulse(int playerNum, int control);
///@}

/// @addtogroup map
///@{
// Play: Setup.
boolean P_MapExists(const char* uri);
boolean P_MapIsCustom(const char* uri);
const char* P_MapSourceFile(const char* uri);

boolean P_LoadMap(const char* uri);
///@}

// Play: World data access (Map Data Updates and access to other information).
#include "dd_world.h"

/// @addtogroup playsim
///@{
// Play: Misc.
void            P_SpawnDamageParticleGen(struct mobj_s* mo, struct mobj_s* inflictor, int amount);
///@}

/// @addtogroup mobj
///@{
    // Play: Mobjs.
    struct mobj_s*  P_MobjCreate(think_t function, float x, float y, float z, angle_t angle, float radius, float height, int ddflags);
    void            P_MobjDestroy(struct mobj_s* mo);
    void            P_MobjSetState(struct mobj_s* mo, int statenum);
    void            P_MobjLink(struct mobj_s* mo, byte flags);
    int             P_MobjUnlink(struct mobj_s* mo);
    struct mobj_s*  P_MobjForID(int id);
    void            Mobj_OriginSmoothed(struct mobj_s* mobj, float origin[3]);
    angle_t         Mobj_AngleSmoothed(struct mobj_s* mobj);
    boolean         ClMobj_IsValid(struct mobj_s* mo);
    struct mobj_s*  ClPlayer_ClMobj(int plrNum);

    // Mobj linked object iterators.
    int             P_MobjLinesIterator(struct mobj_s* mo,
                                        int (*func) (struct linedef_s*,
                                                          void*), void*);
    int             P_MobjSectorsIterator(struct mobj_s* mo,
                                          int (*func) (Sector*, void*),
                                          void* data);
///@}

/**
 * @defgroup polyobj Polygon Objects
 * @ingroup map
 */
///@{

// Play: Polyobjs.
boolean P_PolyobjMoveXY(struct polyobj_s* po, float x, float y);
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

void R_PrecacheMobjNum(int mobjtypeNum);
void R_PrecacheModelsForState(int stateIndex);

void R_RenderPlayerView(int num);

void R_SetViewOrigin(int player, float const origin[3]);
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

angle_t R_PointToAngle2(float x1, float y1, float x2, float y2);

///@}

//------------------------------------------------------------------------
//
// Renderer.
//
//------------------------------------------------------------------------

/// @addtogroup render
///@{

    void            R_SkyParams(int layer, int param, void* data);

///@}

//------------------------------------------------------------------------
//
// Graphics.
//
//------------------------------------------------------------------------

/// @addtogroup gl
///@{

    void            GL_UseFog(int yes);
    byte*           GL_GrabScreen(void);
    void            GL_SetFilter(boolean enable);
    void            GL_SetFilterColor(float r, float g, float b, float a);

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

    void            S_MapChange(void);
    int             S_LocalSoundAtVolumeFrom(int sound_id, struct mobj_s* origin, float* pos, float volume);
    int             S_LocalSoundAtVolume(int soundID, struct mobj_s* origin, float volume);
    int             S_LocalSound(int soundID, struct mobj_s* origin);
    int             S_LocalSoundFrom(int soundID, float* fixedpos);
    int             S_StartSound(int soundId, struct mobj_s* origin);
    int             S_StartSoundEx(int soundId, struct mobj_s* origin);
    int             S_StartSoundAtVolume(int soundID, struct mobj_s* origin, float volume);
    int             S_ConsoleSound(int soundID, struct mobj_s* origin, int targetConsole);
    void            S_StopSound(int soundID, struct mobj_s* origin);
    int             S_IsPlaying(int soundID, struct mobj_s* origin);
    int             S_StartMusic(const char* musicID, boolean looped);
    int             S_StartMusicNum(int id, boolean looped);
    void            S_StopMusic(void);
    void            S_PauseMusic(boolean doPause);

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

    void            M_ClearBox(fixed_t* box);
    void            M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
    int             M_CeilPow2(int num);
    int             M_NumDigits(int value);
    int             M_RatioReduce(int* numerator, int* denominator);

    // Miscellaneous: Random Number Generator facilities.
    byte            RNG_RandByte(void);
    float           RNG_RandFloat(void);

    // Miscellaneous: Math.
    void            V2f_Sum(float dest[2], const float* srcA, const float* srcB);
    void            V2f_Subtract(float dest[2], const float* srcA, const float* srcB);
    void            V2f_Rotate(float vec[2], float radians);
    float           V2f_Intersection(const float* p1, const float* delta1, const float* p2, const float* delta2, float point[2]);

    float           P_PointOnLineSide(float x, float y, float lX, float lY, float lDX, float lDY);
    float           M_PointLineDistance(const float* a, const float* b, const float* c);
    float           M_ProjectPointOnLine(const float* point, const float* linepoint, const float* delta, float gap, float* result);

    binangle_t      bamsAtan2(int y, int x);

///@}

/// @addtogroup base
///@{

    // Miscellaneous: Command line.
    void _DECALL    ArgAbbreviate(const char* longName, const char* shortName);
    int _DECALL     Argc(void);
    const char* _DECALL Argv(int i);
    const char* const* _DECALL ArgvPtr(int i);
    const char* _DECALL ArgNext(void);
    int _DECALL     ArgCheck(const char* check);
    int _DECALL     ArgCheckWith(const char* check, int num);
    int _DECALL     ArgExists(const char* check);
    int _DECALL     ArgIsOption(int i);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_EXPORTS_H */
