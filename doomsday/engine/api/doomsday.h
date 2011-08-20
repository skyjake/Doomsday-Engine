/**\file doomsday.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Doomsday Engine API (Routines exported from Doomsday.exe).
 *
 * Games and plugins need to include this to gain access to the engine's
 * features.
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
extern          "C" {
#endif

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use sector_t to identify to sector to
 * change with the Map Update API.
 *
 * Define __INTERNAL_MAP_DATA_ACCESS__ if access to the internal map data
 * structures is needed.
 */
#ifndef __INTERNAL_MAP_DATA_ACCESS__
    typedef struct node_s { int type; } node_t;
    typedef struct vertex_s {int type; } vertex_t;
    typedef struct linedef_s { int type; } linedef_t;
    typedef struct sidedef_s { int type; } sidedef_t;
    typedef struct seg_s { int type; } seg_t;
    typedef struct subsector_s { int type; } subsector_t;
    typedef struct sector_s { int type; } sector_t;
    typedef struct plane_s { int type; } plane_t;
    typedef struct material_s { int type; } material_t;
    typedef struct font_s { int type; } font_t;
#endif

#include "dd_share.h"
#include "dd_plugin.h"
#include "smoother.h"
#include "reader.h"
#include "writer.h"

//------------------------------------------------------------------------
//
// Base.
//
//------------------------------------------------------------------------

/**
 * Registers a new game.
 *
 * \note Game registration order defines the order of the automatic game identification/selection logic.
 *
 * @param identityKey   Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
 *                      - Used during resource location for mode-specific assets.
 *                      - Sent out in netgames (a client can't connect unless mode strings match).
 * @param dataPath      The base directory for all data-class resources.
 * @param defsPath      The base directory for all defs-class resources.
 * @param mainConfig    The name of the main game config file. Can be @c NULL.
 * @param defaultTitle  Default game title. May be overridden later.
 * @param defaultAuthor Default game author. May be overridden later. Used for (e.g.) the map author name
 *                      if not specified in a Map Info definition.
 * @param cmdlineFlag   Command-line game selection override argument (e.g., "ultimate"). Can be @c NULL.
 * @param cmdlineFlag2  Alternative override. Can be @c NULL.
 *
 * @return              Unique identifier/name assigned to the game.
 */
gameid_t DD_AddGame(const char* identityKey, const char* dataPath, const char* defsPath,
    const char* mainConfig, const char* defaultTitle, const char* defaultAuthor, const char* cmdlineFlag,
    const char* cmdlineFlag2);

/**
 * Registers a new resource for the specified game.
 *
 * \note Resource registration order defines the load order of resources (among those of the same type).
 *
 * @param game          Unique identifier/name of the game.
 * @param rclass        Class of resource being added.
 * @param rflags        @see resourceFlags
 * @param names         One or more known potential names, seperated by semicolon e.g., "name1;name2".
 *                      Names may include valid absolute, or relative file paths. These paths include
 *                      valid symbolbolic escape tokens, predefined symbols into the virtual file system.
 */
void DD_AddGameResource(gameid_t game, resourceclass_t rclass, int rflags, const char* names, void* params);

/**
 * Retrieve extended info about the specified game.
 *
 * @param game          Unique identifier/name of the game.
 * @param info          Info structure to be populated.
 */
void DD_GetGameInfo2(gameid_t game, ddgameinfo_t* info);

/**
 * Retrieve extended info about the current game.
 *
 * @param info          Info structure to be populated.
 * @return              @c true if successful else @c false (i.e., no game loaded).
 */
boolean DD_GetGameInfo(ddgameinfo_t* info);

    int _DECALL     DD_GetInteger(int ddvalue);
    void            DD_SetInteger(int ddvalue, int parm);
    void            DD_SetVariable(int ddvalue, void* ptr);
    void*           DD_GetVariable(int ddvalue);
    ddplayer_t*     DD_GetPlayer(int number);

texturenamespaceid_t DD_ParseTextureNamespace(const char* str);
materialnamespaceid_t DD_ParseMaterialNamespace(const char* str);

materialnum_t DD_MaterialForTextureIndex(uint index, texturenamespaceid_t texNamespace);

    // Base: Definitions.
    int             Def_Get(int type, const char* id, void* out);
    int             Def_Set(int type, int index, int value, const void* ptr);
    int             Def_EvalFlags(char* flags);

    // Base: Input.
    void            DD_ClearKeyRepeaters(void);
    int             DD_GetKeyCode(const char* name);

    // Base: File system.
    int             F_Access(const char* path);
    int             F_FileExists(const char* path);
    unsigned int    F_LastModified(const char* path);
    boolean         F_MakePath(const char* path);

    size_t          M_ReadFile(const char* path, char** buffer);
    boolean         M_WriteFile(const char* path, const char* source, size_t length);

    lumpnum_t       W_CheckLumpNumForName(const char* path);
    lumpnum_t       W_CheckLumpNumForName2(const char* path, boolean silent);
    lumpnum_t       W_GetLumpNumForName(const char* path);

    void            W_ReadLump(lumpnum_t lumpNum, char* dest);
    void            W_ReadLumpSection(lumpnum_t lumpNum, char* dest, size_t startOffset, size_t length);
    const char*     W_CacheLump(lumpnum_t lumpNum, int tag);
    void            W_CacheChangeTag(lumpnum_t lumpNum, int tag);

    size_t          W_LumpLength(lumpnum_t lumpNum);
    const char*     W_LumpName(lumpnum_t lumpNum);
    const char*     W_LumpSourceFile(lumpnum_t lumpNum);
    boolean         W_LumpIsFromIWAD(lumpnum_t lumpNum);

    // Base: File system path/name utilities.
    void            F_ExtractFileBase(char* dst, const char* path, size_t len);
    const char*     F_FindFileExtension(const char* path);

    boolean         F_TranslatePath(ddstring_t* dst, const ddstring_t* src);
    const char*     F_PrettyPath(const char* path);

    // Base: Zone.
    void* _DECALL   Z_Malloc(size_t size, int tag, void* ptr);
    void* _DECALL   Z_Calloc(size_t size, int tag, void* user);
    void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
    void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
    void _DECALL    Z_Free(void* ptr);
    void            Z_FreeTags(int lowTag, int highTag);
    void            Z_ChangeTag2(void* ptr, int tag);
    void            Z_CheckHeap(void);

//------------------------------------------------------------------------
//
// Console.
//
//------------------------------------------------------------------------

    int             Con_Busy(int flags, const char* taskName, int (*workerFunc)(void*), void* workerData);
    void            Con_BusyWorkerEnd(void);
    boolean         Con_IsBusy(void);
    void            Con_Open(int yes);
    void            Con_AddCommand(const ccmdtemplate_t* cmd);
    void            Con_AddVariable(const cvartemplate_t* var);
    void            Con_AddCommandList(const ccmdtemplate_t* cmdList);
    void            Con_AddVariableList(const cvartemplate_t* varList);
cvartype_t Con_GetVariableType(const char* name);
    byte            Con_GetByte(const char* name);
    int             Con_GetInteger(const char* name);
    float           Con_GetFloat(const char* name);
    char*           Con_GetString(const char* name);
void Con_SetInteger2(const char* name, int value, int svflags);
void Con_SetInteger(const char* name, int value);

void Con_SetFloat2(const char* name, float value, int svflags);
void Con_SetFloat(const char* name, float value);

void Con_SetString2(const char* name, const char* text, int svflags);
void Con_SetString(const char* name, const char* text);
    void            Con_Printf(const char* format, ...) PRINTF_F(1,2);
    void            Con_FPrintf(int flags, const char* format, ...) PRINTF_F(2,3);
    void            Con_Message(const char* message, ...) PRINTF_F(1,2);
    void            Con_Error(const char* error, ...) PRINTF_F(1,2);

void Con_SetPrintFilter(con_textfilter_t filter);

    int             DD_Execute(int silent, const char* command);
    int             DD_Executef(int silent, const char* command, ...);

    // Console: Bindings.
    void            B_SetContextFallback(const char* name, int (*responderFunc)(event_t*));
    int             B_BindingsForCommand(const char* cmd, char* buf, size_t bufSize);
    int             B_BindingsForControl(int localPlayer, const char* controlName, int inverse, char* buf, size_t bufSize);

//------------------------------------------------------------------------
//
// System.
//
//------------------------------------------------------------------------

    void            Sys_TicksPerSecond(float num);
    int             Sys_GetTime(void);
    double          Sys_GetSeconds(void);
    uint            Sys_GetRealTime(void);
    void            Sys_Sleep(int millisecs);
    int             Sys_CriticalMessage(char* msg);
    void            Sys_Quit(void);

//------------------------------------------------------------------------
//
// Map Edit.
//
//------------------------------------------------------------------------

    boolean         MPE_Begin(const char* name);
    boolean         MPE_End(void);

    uint            MPE_VertexCreate(float x, float y);
    boolean         MPE_VertexCreatev(size_t num, float* values, uint* indices);
    uint            MPE_SidedefCreate(uint sector, short flags, materialnum_t topMaterial, float topOffsetX, float topOffsetY, float topRed, float topGreen, float topBlue, materialnum_t middleMaterial, float middleOffsetX, float middleOffsetY, float middleRed, float middleGreen, float middleBlue, float middleAlpha, materialnum_t bottomMaterial, float bottomOffsetX, float bottomOffsetY, float bottomRed, float bottomGreen, float bottomBlue);
    uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide, uint backSide, int flags);
    uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
    uint            MPE_PlaneCreate(uint sector, float height, materialnum_t num, float matOffsetX, float matOffsetY, float r, float g, float b, float a, float normalX, float normalY, float normalZ);
    uint            MPE_PolyobjCreate(uint* lines, uint linecount, int tag, int sequenceType, float anchorX, float anchorY);
    boolean         MPE_GameObjProperty(const char* objName, uint idx, const char* propName, valuetype_t type, void* data);

    // Custom map object data types.
    boolean         P_RegisterMapObj(int identifier, const char* name);
    boolean         P_RegisterMapObjProperty(int identifier, int propIdentifier, const char* propName, valuetype_t type);

//------------------------------------------------------------------------
//
// Networking.
//
//------------------------------------------------------------------------

    void            Net_SendPacket(int to_player, int type, const void* data, size_t length);
    int             Net_GetTicCmd(void* command, int player);
    const char*     Net_GetPlayerName(int player);
    ident_t         Net_GetPlayerID(int player);
    Smoother*       Net_PlayerSmoother(int player);
    boolean         Sv_CanTrustClientPos(int player);

//------------------------------------------------------------------------
//
// Playsim.
//
//------------------------------------------------------------------------

    float           P_AccurateDistance(float dx, float dy);
    float           P_ApproxDistance(float dx, float dy);
    float           P_ApproxDistance3(float dx, float dy, float dz);
    int             P_PointOnLinedefSide(float x, float y, const struct linedef_s* line);
    int             P_BoxOnLineSide(const float* tmbox, const struct linedef_s* ld);
    void            P_MakeDivline(struct linedef_s* li, divline_t* dl);
    int             P_PointOnDivlineSide(float x, float y, const divline_t* line);
    float           P_InterceptVector(divline_t* v2, divline_t* v1);
    void            P_LineOpening(struct linedef_s* linedef);

    // Object in bounding box iterators.
    boolean         P_MobjsBoxIterator(const float box[4], boolean (*func) (struct mobj_s*, void*), void* data);
    boolean         P_LinesBoxIterator(const float box[4], boolean (*func) (struct linedef_s*, void*), void* data);
    boolean         P_AllLinesBoxIterator(const float box[4], boolean (*func) (struct linedef_s*, void*), void* data);
    boolean         P_SubsectorsBoxIterator(const float box[4], sector_t* sector, boolean (*func) (subsector_t*, void*), void* data);
    boolean         P_PolyobjsBoxIterator(const float box[4], boolean (*func) (struct polyobj_s*, void*), void* data);

    // Object type touching mobjs iterators.
    boolean         P_LineMobjsIterator(struct linedef_s* line, boolean (*func) (struct mobj_s*, void*), void* data);
    boolean         P_SectorTouchingMobjsIterator(sector_t* sector, boolean (*func) (struct mobj_s*, void*), void* data);

    boolean         P_PathTraverse(float x1, float y1, float x2, float y2, int flags, boolean (*trav) (intercept_t*));
    boolean         P_CheckLineSight(const float from[3], const float to[3], float bottomSlope, float topSlope, int flags);

    // Play: Controls.
    void            P_NewPlayerControl(int id, controltype_t type, const char* name, const char* bindContext);
    void            P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset);
    int             P_GetImpulseControlState(int playerNum, int control);

    // Play: Setup.
    boolean         P_LoadMap(const char* mapID);

    // Play: World data access (Map Data Updates and access to other information).
#include "dd_world.h"

    // Play: Misc.
    void            P_SpawnDamageParticleGen(struct mobj_s* mo,
                                             struct mobj_s* inflictor,
                                             int amount);

    // Play: Mobjs.
    struct mobj_s*  P_MobjCreate(think_t function, float x, float y, float z, angle_t angle, float radius, float height, int ddflags);
    void            P_MobjDestroy(struct mobj_s* mo);
    void            P_MobjSetState(struct mobj_s* mo, int statenum);
    void            P_MobjLink(struct mobj_s* mo, byte flags);
    int             P_MobjUnlink(struct mobj_s* mo);
    struct mobj_s*  P_MobjForID(int id);
    boolean         ClMobj_IsValid(struct mobj_s* mo);
    struct mobj_s*  ClPlayer_ClMobj(int plrNum);

    // Mobj linked object iterators.
    boolean         P_MobjLinesIterator(struct mobj_s* mo, boolean (*func) (struct linedef_s*, void*), void*);
    boolean         P_MobjSectorsIterator(struct mobj_s* mo, boolean (*func) (sector_t*, void*), void* data);

    // Play: Polyobjs.
    boolean         P_PolyobjMove(struct polyobj_s* po, float x, float y);
    boolean         P_PolyobjRotate(struct polyobj_s* po, angle_t angle);
    void            P_PolyobjLink(struct polyobj_s* po);
    void            P_PolyobjUnLink(struct polyobj_s* po);

    struct polyobj_s* P_GetPolyobj(uint num);
    void            P_SetPolyobjCallback(void (*func)(struct mobj_s*, void*, void*));

    // Play: Materials.
    materialnum_t   Materials_IndexForUri(const dduri_t* uri);
    materialnum_t   Materials_IndexForName(const char* path);
    dduri_t*        Materials_GetUri(struct material_s* mat);

    const ddstring_t* Materials_GetSymbolicName(struct material_s* mat);
    int             Materials_CreateAnimGroup(int flags);
    void            Materials_AddAnimGroupFrame(int groupNum, materialnum_t num, int tics, int randomTics);

    // Play: Thinkers.
    void            DD_InitThinkers(void);
    void            DD_RunThinkers(void);
    void            DD_ThinkerAdd(thinker_t* th);
    void            DD_ThinkerRemove(thinker_t* th);
    void            DD_ThinkerSetStasis(thinker_t* th, boolean on);

    boolean         DD_IterateThinkers(think_t type, boolean (*func) (thinker_t *th, void*), void* data);

//------------------------------------------------------------------------
//
// UI.
//
//------------------------------------------------------------------------

fontnum_t Fonts_IndexForUri(const dduri_t* uri);
fontnum_t Fonts_IndexForName(const char* path);
dduri_t* Fonts_GetUri(struct font_s* font);
const ddstring_t* Fonts_GetSymbolicName(struct font_s* font);

//------------------------------------------------------------------------
//
// Refresh.
//
//------------------------------------------------------------------------

    boolean         DD_IsSharpTick(void);
    int             DD_GetFrameRate(void);

    void            R_SetupMap(int mode, int flags);
    void            R_PrecacheMobjNum(int mobjtypeNum);
    patchid_t       R_PrecachePatch(const char* name, patchinfo_t* info);
    void            R_PrecacheSkinsForState(int stateIndex);

    void            R_RenderPlayerView(int num);

int R_ViewWindowDimensions(int player, int* x, int* y, int* w, int* h);
void R_SetViewWindowDimensions(int player, int x, int y, int w, int h, boolean interpolate);

int R_ViewportDimensions(int player, int* x, int* y, int* w, int* h);

boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);
scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

    void            R_SetBorderGfx(const dduri_t* const* paths);
    boolean         R_GetSpriteInfo(int sprite, int frame, spriteinfo_t* sprinfo);
boolean R_GetPatchInfo(patchid_t id, patchinfo_t* info);
const ddstring_t* R_GetPatchName(patchid_t id);
    void            R_SetViewPortPlayer(int consoleNum, int viewPlayer);
    int             R_CreateAnimGroup(int flags);
    void            R_AddToAnimGroup(int groupNum, materialnum_t num,
                                     int tics, int randomTics);
    void            R_HSVToRGB(float* rgb, float h, float s, float v);
    angle_t         R_PointToAngle2(float x1, float y1, float x2, float y2);
    struct subsector_s* R_PointInSubsector(float x, float y);

colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name, const uint8_t* colorData, int colorCount);
colorpaletteid_t R_GetColorPaletteNumForName(const char* name);
const char* R_GetColorPaletteNameForNum(colorpaletteid_t id);

void R_GetColorPaletteRGBubv(colorpaletteid_t id, int colorIdx, uint8_t rgb[3], boolean applyTexGamma);
void R_GetColorPaletteRGBf(colorpaletteid_t id, int colorIdx, float rgb[3], boolean applyTexGamma);

//------------------------------------------------------------------------
//
// Renderer.
//
//------------------------------------------------------------------------

    void            Rend_SkyParams(int layer, int param, void* data);

//------------------------------------------------------------------------
//
// Graphics.
//
//------------------------------------------------------------------------

    void            GL_UseFog(int yes);
    byte*           GL_GrabScreen(void);
    void            GL_SetFilter(boolean enable);
    void            GL_SetFilterColor(float r, float g, float b, float a);

void GL_ConfigureBorderedProjection2(borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
void GL_ConfigureBorderedProjection(borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);
void GL_BeginBorderedProjection(borderedprojectionstate_t* bp);
void GL_EndBorderedProjection(borderedprojectionstate_t* bp);

uint GL_TextureIndexForUri(const dduri_t* uri);
uint GL_TextureIndexForUri2(const dduri_t* uri, boolean silent);

//------------------------------------------------------------------------
//
// Audio.
//
//------------------------------------------------------------------------

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

//------------------------------------------------------------------------
//
// Miscellaneous.
//
//------------------------------------------------------------------------

char* M_SkipWhite(char* str);
char* M_FindWhite(char* str);
char* M_StrCatQuoted(char* dest, const char* src, size_t len);
boolean M_IsStringValidInt(const char* str);
boolean M_IsStringValidByte(const char* str);
boolean M_IsStringValidFloat(const char* str);

    int             M_ScreenShot(const char* filename, int bits);

    void            M_ClearBox(fixed_t* box);
    void            M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
    int             M_CeilPow2(int num);
    int             M_NumDigits(int value);
    int             M_RatioReduce(int* numerator, int* denominator);

    // Miscellaneous: Random Number Generator facilities.
    byte            RNG_RandByte(void);
    float           RNG_RandFloat(void);

    // Miscellaneous: Time utilities.
    boolean         M_RunTrigger(trigger_t* trigger, timespan_t advanceTime);
    boolean         M_CheckTrigger(const trigger_t* trigger, timespan_t advanceTime);

    // Miscellaneous: Math.
    void            V2_Rotate(float vec[2], float radians);
    float           V2_Intersection(const float* p1, const float* delta1, const float* p2, const float* delta2, float point[2]);

    int             P_PointOnLineSide(float x, float y, float lX, float lY, float lDX, float lDY);
    float           M_PointLineDistance(const float* a, const float* b, const float* c);
    float           M_ProjectPointOnLine(const float* point, const float* linepoint, const float* delta, float gap, float* result);

    binangle_t      bamsAtan2(int y, int x);

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

#ifdef __cplusplus
}
#endif
#endif /* LIBDENG_EXPORTS_H */
