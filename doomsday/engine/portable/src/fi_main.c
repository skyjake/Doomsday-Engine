/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * The "In Fine" finale sequence system.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_network.h"
#include "de_audio.h"
#include "de_infine.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define STACK_SIZE          (16) // Size of the InFine state stack.
#define MAX_TOKEN_LEN       (8192)
#define MAX_PICS            (128)
#define MAX_TEXT            (64)
#define MAX_HANDLERS        (128)

#define FI_REPEAT           (-2)

// Helper macro for defining infine command functions.
#define DEFFC(name) void FIC_##name(ficmd_t* cmd, fistate_t* s)

// TYPES -------------------------------------------------------------------

typedef struct fihandler_s {
    int             code;
    fi_objectname_t marker;
} fihandler_t;

typedef struct fistate_s {
    char*           script; // A copy of the script.
    const char*     cp; // The command cursor.
    finale_mode_t   mode;
    int             initialGameState; // Game state before the script began.
    int             overlayGameState; // Overlay scripts run only in one gameMode.
    int             timer;
    void*           conditions;
    int             inTime;
    boolean         canSkip, skipping;
    int             doLevel; // Level of DO-skipping.
    int             wait;
    boolean         suspended, paused, eatEvents, showMenu;
    boolean         gotoSkip, skipNext, lastSkipped;
    fi_objectname_t gotoTarget;
    fidata_text_t*  waitingText;
    fidata_pic_t*   waitingPic;
    fihandler_t     keyHandlers[MAX_HANDLERS];
    material_t*     bgMaterial;
    fi_value4_t     bgColor;
    fi_value4_t     imgColor;
    fi_value2_t     imgOffset;
    fi_value4_t     filter;
    fi_value3_t     textColor[9];
    fidata_pic_t    pics[MAX_PICS];
    fidata_text_t   text[MAX_TEXT];
} fistate_t;

typedef struct ficmd_s {
    char*           token;
    int             operands;
    void          (*func) (struct ficmd_s* cmd, fistate_t* s);
    boolean         whenSkipping;
    boolean         whenCondSkipping; // Skipping because condition failed.
} ficmd_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// Command functions.
DEFFC(Do);
DEFFC(End);
DEFFC(BGFlat);
DEFFC(BGTexture);
DEFFC(NoBGMaterial);
DEFFC(Wait);
DEFFC(WaitText);
DEFFC(WaitAnim);
DEFFC(Tic);
DEFFC(InTime);
DEFFC(Color);
DEFFC(ColorAlpha);
DEFFC(Pause);
DEFFC(CanSkip);
DEFFC(NoSkip);
DEFFC(SkipHere);
DEFFC(Events);
DEFFC(NoEvents);
DEFFC(OnKey);
DEFFC(UnsetKey);
DEFFC(If);
DEFFC(IfNot);
DEFFC(Else);
DEFFC(GoTo);
DEFFC(Marker);
DEFFC(Image);
DEFFC(ImageAt);
DEFFC(XImage);
DEFFC(Delete);
DEFFC(Patch);
DEFFC(SetPatch);
DEFFC(Anim);
DEFFC(AnimImage);
DEFFC(StateAnim);
DEFFC(Repeat);
DEFFC(ClearAnim);
DEFFC(PicSound);
DEFFC(ObjectOffX);
DEFFC(ObjectOffY);
DEFFC(ObjectScaleX);
DEFFC(ObjectScaleY);
DEFFC(ObjectScale);
DEFFC(ObjectScaleXY);
DEFFC(ObjectRGB);
DEFFC(ObjectAlpha);
DEFFC(ObjectAngle);
DEFFC(Rect);
DEFFC(FillColor);
DEFFC(EdgeColor);
DEFFC(OffsetX);
DEFFC(OffsetY);
DEFFC(Sound);
DEFFC(SoundAt);
DEFFC(SeeSound);
DEFFC(DieSound);
DEFFC(Music);
DEFFC(MusicOnce);
DEFFC(Filter);
DEFFC(Text);
DEFFC(TextFromDef);
DEFFC(TextFromLump);
DEFFC(SetText);
DEFFC(SetTextDef);
DEFFC(DeleteText);
DEFFC(Font);
DEFFC(FontA);
DEFFC(FontB);
DEFFC(TextColor);
DEFFC(TextRGB);
DEFFC(TextAlpha);
DEFFC(TextOffX);
DEFFC(TextOffY);
DEFFC(TextCenter);
DEFFC(TextNoCenter);
DEFFC(TextScroll);
DEFFC(TextPos);
DEFFC(TextRate);
DEFFC(TextLineHeight);
DEFFC(NoMusic);
DEFFC(TextScaleX);
DEFFC(TextScaleY);
DEFFC(TextScale);
DEFFC(PlayDemo);
DEFFC(Command);
DEFFC(ShowMenu);
DEFFC(NoShowMenu);

D_CMD(StartFinale);
D_CMD(StopFinale);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Default color for Text objects.
float fiDefaultTextRGB[3] = { 1, 1, 1 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean active = false;
static boolean cmdExecuted = false; // Set to true after first command.

// Time is measured in seconds.
// Colors are floating point and [0,1].
static ficmd_t commands[] = {
    // Run Control
    { "DO",         0, FIC_Do, true, true },
    { "END",        0, FIC_End },
    { "IF",         1, FIC_If }, // if (value-id)
    { "IFNOT",      1, FIC_IfNot }, // ifnot (value-id)
    { "ELSE",       0, FIC_Else },
    { "GOTO",       1, FIC_GoTo }, // goto (marker)
    { "MARKER",     1, FIC_Marker, true },
    { "in",         1, FIC_InTime }, // in (time)
    { "pause",      0, FIC_Pause },
    { "tic",        0, FIC_Tic },
    { "wait",       1, FIC_Wait }, // wait (time)
    { "waittext",   1, FIC_WaitText }, // waittext (id)
    { "waitanim",   1, FIC_WaitAnim }, // waitanim (id)
    { "canskip",    0, FIC_CanSkip },
    { "noskip",     0, FIC_NoSkip },
    { "skiphere",   0, FIC_SkipHere, true },
    { "events",     0, FIC_Events },
    { "noevents",   0, FIC_NoEvents },
    { "onkey",      2, FIC_OnKey }, // onkey (keyname) (marker)
    { "unsetkey",   1, FIC_UnsetKey }, // unsetkey (keyname)

    // Screen Control
    { "color",      3, FIC_Color }, // color (red) (green) (blue)
    { "coloralpha", 4, FIC_ColorAlpha }, // coloralpha (r) (g) (b) (a)
    { "flat",       1, FIC_BGFlat }, // flat (flat-id)
    { "texture",    1, FIC_BGTexture }, // texture (texture-id)
    { "noflat",     0, FIC_NoBGMaterial },
    { "notexture",  0, FIC_NoBGMaterial },
    { "offx",       1, FIC_OffsetX }, // offx (x)
    { "offy",       1, FIC_OffsetY }, // offy (y)
    { "filter",     4, FIC_Filter }, // filter (r) (g) (b) (a)

    // Audio
    { "sound",      1, FIC_Sound }, // sound (snd)
    { "soundat",    2, FIC_SoundAt }, // soundat (snd) (vol:0-1)
    { "seesound",   1, FIC_SeeSound }, // seesound (mobjtype)
    { "diesound",   1, FIC_DieSound }, // diesound (mobjtype)
    { "music",      1, FIC_Music }, // music (musicname)
    { "musiconce",  1, FIC_MusicOnce }, // musiconce (musicname)
    { "nomusic",    0, FIC_NoMusic },

    // Objects
    { "del",        1, FIC_Delete }, // del (id)
    { "x",          2, FIC_ObjectOffX }, // x (id) (x)
    { "y",          2, FIC_ObjectOffY }, // y (id) (y)
    { "sx",         2, FIC_ObjectScaleX }, // sx (id) (x)
    { "sy",         2, FIC_ObjectScaleY }, // sy (id) (y)
    { "scale",      2, FIC_ObjectScale }, // scale (id) (factor)
    { "scalexy",    3, FIC_ObjectScaleXY }, // scalexy (id) (x) (y)
    { "rgb",        4, FIC_ObjectRGB }, // rgb (id) (r) (g) (b)
    { "alpha",      2, FIC_ObjectAlpha }, // alpha (id) (alpha)
    { "angle",      2, FIC_ObjectAngle }, // angle (id) (degrees)

    // Rects
    { "rect",       5, FIC_Rect }, // rect (hndl) (x) (y) (w) (h)
    { "fillcolor",  6, FIC_FillColor }, // fillcolor (h) (top/bottom/both) (r) (g) (b) (a)
    { "edgecolor",  6, FIC_EdgeColor }, // edgecolor (h) (top/bottom/both) (r) (g) (b) (a)

    // Pics
    { "image",      2, FIC_Image }, // image (id) (raw-image-lump)
    { "imageat",    4, FIC_ImageAt }, // imageat (id) (x) (y) (raw)
    { "ximage",     2, FIC_XImage }, // ximage (id) (ext-gfx-filename)
    { "patch",      4, FIC_Patch }, // patch (id) (x) (y) (patch)
    { "set",        2, FIC_SetPatch }, // set (id) (lump)
    { "clranim",    1, FIC_ClearAnim }, // clranim (id)
    { "anim",       3, FIC_Anim }, // anim (id) (patch) (time)
    { "imageanim",  3, FIC_AnimImage }, // imageanim (hndl) (raw-img) (time)
    { "picsound",   2, FIC_PicSound }, // picsound (hndl) (sound)
    { "repeat",     1, FIC_Repeat }, // repeat (id)
    { "states",     3, FIC_StateAnim }, // states (id) (state) (count)

    // Text
    { "text",       4, FIC_Text }, // text (hndl) (x) (y) (string)
    { "textdef",    4, FIC_TextFromDef }, // textdef (hndl) (x) (y) (txt-id)
    { "textlump",   4, FIC_TextFromLump }, // textlump (hndl) (x) (y) (lump)
    { "settext",    2, FIC_SetText }, // settext (id) (newtext)
    { "settextdef", 2, FIC_SetTextDef }, // settextdef (id) (txt-id)
    { "precolor",   4, FIC_TextColor }, // precolor (num) (r) (g) (b)
    { "center",     1, FIC_TextCenter }, // center (id)
    { "nocenter",   1, FIC_TextNoCenter }, // nocenter (id)
    { "scroll",     2, FIC_TextScroll }, // scroll (id) (speed)
    { "pos",        2, FIC_TextPos }, // pos (id) (pos)
    { "rate",       2, FIC_TextRate }, // rate (id) (rate)
    { "font",       2, FIC_Font }, // font (id) (font)
    { "fonta",      1, FIC_FontA }, // fonta (id)
    { "fontb",      1, FIC_FontB }, // fontb (id)
    { "linehgt",    2, FIC_TextLineHeight }, // linehgt (hndl) (hgt)

    // Game Control
    { "playdemo",   1, FIC_PlayDemo }, // playdemo (filename)
    { "cmd",        1, FIC_Command }, // cmd (console command)
    { "trigger",    0, FIC_ShowMenu },
    { "notrigger",  0, FIC_NoShowMenu },

    // Deprecated Pic commands
    { "delpic",     1, FIC_Delete }, // delpic (id)

    // Deprecated Text commands
    { "deltext",    1, FIC_DeleteText }, // deltext (hndl)
    { "textrgb",    4, FIC_TextRGB }, // textrgb (id) (r) (g) (b)
    { "textalpha",  2, FIC_TextAlpha }, // textalpha (id) (alpha)
    { "tx",         2, FIC_TextOffX }, // tx (id) (x)
    { "ty",         2, FIC_TextOffY }, // ty (id) (y)
    { "tsx",        2, FIC_TextScaleX }, // tsx (id) (x)
    { "tsy",        2, FIC_TextScaleY }, // tsy (id) (y)
    { "textscale",  3, FIC_TextScale }, // textscale (id) (x) (y)

    { NULL, 0, NULL } // Terminate.
};

static fistate_t stateStack[STACK_SIZE];
static fistate_t* fi; // Pointer to the current state in the stack.
static char token[MAX_TOKEN_LEN];
static fidata_pic_t dummyPic;
static fidata_text_t dummyText;

static void* defaultState = 0;

// Allow stretching to fill the screen at near 4:3 aspect ratios?
static byte noStretch = false;

// CODE --------------------------------------------------------------------

/**
 * Called during pre-init to register cvars and ccmds for the finale system.
 */
void FI_Register(void)
{
    C_VAR_BYTE("finale-nostretch",  &noStretch, 0, 0, 1);

    C_CMD("startfinale",    "s",    StartFinale);
    C_CMD("startinf",       "s",    StartFinale);
    C_CMD("stopfinale",     "",     StopFinale);
    C_CMD("stopinf",        "",     StopFinale);
}

static ficmd_t* findCommand(const char* name)
{
    int i;
    for(i = 0; commands[i].token; ++i)
    {
        ficmd_t* cmd = &commands[i];
        if(!stricmp(name, cmd->token))
        {
            return cmd;
        }
    }
    return 0; // Not found.
}

static void useColor(fi_value_t *color, int components)
{
    if(components == 3)
    {
        glColor3f(color[0].value, color[1].value, color[2].value);
    }
    else if(components == 4)
    {
        glColor4f(color[0].value, color[1].value, color[2].value, color[3].value);
    }
}

static DGLuint loadGraphics(ddresourceclass_t resClass, const char* name,
    gfxmode_t mode, int useMipmap, boolean clamped, int otherFlags)
{
    return GL_PrepareExtTexture(resClass, name, mode, useMipmap,
                                GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                                clamped? GL_CLAMP_TO_EDGE : GL_REPEAT,
                                clamped? GL_CLAMP_TO_EDGE : GL_REPEAT,
                                otherFlags);
}

static void valueInit(fi_value_t* v, float value)
{
    v->target = v->value = value;
    v->steps = 0;
}

static void value2Init(fi_value2_t val, float x, float y)
{
    valueInit(&val[0], x);
    valueInit(&val[1], y);
}

static void value3Init(fi_value3_t val, float x, float y, float z)
{
    value2Init(val, x, y);
    valueInit(&val[2], z);
}

static void value4Init(fi_value4_t val, float x, float y, float z, float w)
{
    value3Init(val, x, y, z);
    valueInit(&val[3], w);
}

static void valueSet(fi_value_t* v, float value, int steps)
{
    v->target = value;
    v->steps = steps;
    if(!v->steps)
        v->value = v->target;
}

static void value2Set(fi_value2_t v, float x, float y, int steps)
{
    valueSet(&v[0], x, steps);
    valueSet(&v[1], y, steps);
}

static void value3Set(fi_value3_t v, float x, float y, float z, int steps)
{
    value2Set(v, x, y, steps);
    valueSet(&v[2], z, steps);
}

static void value4Set(fi_value4_t v, float x, float y, float z, float w, int steps)
{
    value3Set(v, x, y, z, steps);
    valueSet(&v[3], w, steps);
}

static void valueThink(fi_value_t* val)
{
    if(val->steps <= 0)
    {
        val->steps = 0;
        val->value = val->target;
        return;
    }

    val->value += (val->target - val->value) / val->steps;
    val->steps--;
}

static void value2Think(fi_value2_t val)
{
    valueThink(&val[0]);
    valueThink(&val[1]);
}

static void value3Think(fi_value3_t val)
{
    value2Think(val);
    valueThink(&val[2]);
}

static void value4Think(fi_value4_t val)
{
    value3Think(val);
    valueThink(&val[3]);
}

static void objectSetUniqueName(fi_object_t* obj, const char* name)
{
    strncpy(obj->name, name, sizeof(obj->name) - 1);
}

void FIObject_Think(fi_object_t* obj)
{
    assert(obj);

    value2Think(obj->pos);
    value2Think(obj->scale);
    value4Think(obj->color);
    valueThink(&obj->angle);
}

static fistate_t* allocState(void)
{
    fistate_t* s;

    if(!fi)
    {   // Start from the bottom of the stack.
        s = fi = stateStack;
    }
    else
    {   // Get the next state from the stack.
        s = fi += 1;

        if(fi == stateStack + STACK_SIZE)
            Con_Error("InFine state stack overflow.\n");
    }

#ifdef _DEBUG // Is the stack leaking?
Con_Printf("InFine state assigned index %i.\n", fi - stateStack);
#endif

    memset(s, 0, sizeof(*s));
    return s;
}

static fistate_t* newState(const char* script)
{
    fistate_t* s = allocState();
    size_t size;

    // Take a copy of the script.
    size = strlen(script);
    s->script = Z_Malloc(size + 1, PU_STATIC, 0);
    memcpy(s->script, script, size);
    s->script[size] = '\0';
    
    s->cp = s->script; // Init cursor.

    return s;
}

static fidata_pic_t* stateFindPic(fistate_t* s, const char* name)
{
    assert(name && name[0]);
    {int i;
    for(i = 0; i < MAX_PICS; ++i)
    {
        fi_object_t* obj = &s->pics[i].object;
        if(obj->used && !stricmp(obj->name, name))
        {
            return &s->pics[i];
        }
    }}
    return 0;
}

static fidata_pic_t* stateFindUnusedPic(fistate_t* s)
{
    {int i;
    for(i = 0; i < MAX_PICS; ++i)
    {
        fidata_pic_t* pic = &s->pics[i];
        fi_object_t* obj = &pic->object;
        if(!obj->used)
        {
            memset(pic, 0, sizeof(*pic));
            return pic;
        }
    }}
    return 0; /// @todo Oh noes! Remove fixed limit.
}

static fidata_text_t* stateFindText(fistate_t* s, const char* name)
{
    assert(name && name[0]);
    {int i;
    for(i = 0; i < MAX_TEXT; ++i)
    {
        fi_object_t* obj = &s->text[i].object;
        if(obj->used && !stricmp(obj->name, name))
        {
            return &s->text[i];
        }
    }}
    return NULL;
}

static fidata_text_t* stateFindUnusedText(fistate_t* s)
{
    {int i;
    for(i = 0; i < MAX_TEXT; ++i)
    {
        fidata_text_t* text = &s->text[i];
        fi_object_t* obj = &text->object;
        if(!obj->used)
        {
            memset(text, 0, sizeof(*text));
            return text;
        }
    }}
    return 0; /// @todo Oh noes! Remove fixed limit.
}

static fi_object_t* stateFindObject(fistate_t* s, const char* name)
{
    {fidata_pic_t* pic; // First check all pics.
    if((pic = stateFindPic(s, name)) != NULL)
        return &pic->object;
    }
    
    {fidata_text_t* text; // Then check text objects.
    if((text = stateFindText(s, name)) != NULL)
        return &text->object;
    }
    return NULL;
}

static void stateChangeMode(fistate_t* s, finale_mode_t mode)
{
    s->mode = mode;
}

static void stateSetData(fistate_t* s, const void* data)
{
    if(!data)
        return;
    if(!s->conditions || !(INFINE_CLIENTSTATE_SIZE > 0))
        return;
    memcpy(s->conditions, data, INFINE_CLIENTSTATE_SIZE);
}

static void stateSetInitialGameState(fistate_t* s, int gameState, const void* clientState)
{
    s->initialGameState = gameState;
    s->showMenu = true; // Enabled by default.

    if(INFINE_CLIENTSTATE_SIZE > 0)
    {
        stateSetData(s, &defaultState);
        if(clientState)
            stateSetData(s, clientState);
    }

    if(s->mode == FIMODE_OVERLAY)
    {   // Overlay scripts stop when the gameMode changes.
        s->overlayGameState = gameState;
    }
}

static void stateReleaseScript(fistate_t* s)
{
    // Free the current state.
    if(s->script)
        Z_Free(s->script);
    s->script = 0;
    s->cp = 0;
}

/**
 * Clear the specified state to the default, blank state.
 */
static void stateClear(fistate_t* s)
{
    int i;

    active = true;
    cmdExecuted = false; // Nothing is drawn until a cmd has been executed.

    s->suspended = false;
    s->timer = 0;
    s->canSkip = true; // By default skipping is enabled.
    s->skipping = false;
    s->wait = 0; // Not waiting for anything.
    s->inTime = 0; // Interpolation is off.
    s->bgMaterial = NULL; // No background material.
    s->paused = false;
    s->gotoSkip = false;
    s->skipNext = false;

    s->waitingText = NULL;
    s->waitingPic = NULL;
    memset(s->gotoTarget, 0, sizeof(s->gotoTarget));
    memset(s->imgOffset, 0, sizeof(s->imgOffset));
    memset(s->filter, 0, sizeof(s->filter));
    
    value4Init(s->bgColor, 1, 1, 1, 1);

    // Delete external images.
    for(i = 0; i < MAX_PICS; ++i)
    {
        fidata_pic_t* pic = &s->pics[i];
        if(pic->flags.is_ximage)
        {
            FIData_PicDeleteXImage(pic);
        }
    }
    memset(s->pics, 0, sizeof(s->pics));

    // Free all text strings.
    for(i = 0; i < MAX_TEXT; ++i)
    {
        if(s->text[i].text)
        {
            Z_Free(s->text[i].text);
        }
    }
    memset(s->text, 0, sizeof(s->text));

    for(i = 0; i < 9; ++i)
    {
        value3Init(s->textColor[i], 1, 1, 1);
    }
}

static void stateInit(fistate_t* s, finale_mode_t mode, int gameState, const void* clientState)
{
    stateClear(s);
    stateChangeMode(s, mode);
    stateSetInitialGameState(s, gameState, clientState);
}

static __inline fistate_t* stackTop(void)
{
    return fi;
}

static void stackPop(void)
{
    fistate_t* s = stackTop();

/*#ifdef _DEBUG
Con_Printf("InFine: s=%p (%i)\n", s, s - stateStack);
#endif*/

    if(!s)
    {
#ifdef _DEBUG
Con_Printf("InFine: Pop in NULL state!\n");
#endif
        return;
    }

    stateClear(s);
    stateReleaseScript(s);

    // Should we go back to NULL?
    if(fi == stateStack)
    {
        fi = NULL;
        active = false;
    }
    else
    {   // Return to previous state.
        fi--;
    }
}

static void scriptBegin(fistate_t* s)
{
    if(s->mode != FIMODE_LOCAL)
    {
        int flags = FINF_BEGIN | (s->mode == FIMODE_AFTER ? FINF_AFTER : s->mode == FIMODE_OVERLAY ? FINF_OVERLAY : 0);
        ddhook_serializeconditions_paramaters_t p;
        boolean haveExtraData = false;

        memset(&p, 0, sizeof(p));

        if(s->conditions)
        {
            p.conditions = s->conditions;
            p.outBuf = 0;
            p.outBufSize = 0;
            haveExtraData = Plug_DoHook(HOOK_INFINE_STATE_SERIALIZE_EXTRADATA, 0, &p);
        }

        // Tell clients to start this script.
        Sv_Finale(flags, s->script, (haveExtraData? p.outBuf : 0), (haveExtraData? p.outBufSize : 0));
    }

    memset(&dummyText, 0, sizeof(dummyText));

    // Any hooks?
    Plug_DoHook(HOOK_INFINE_SCRIPT_BEGIN, (int) s->mode, s->conditions);
}

static void scriptCanSkip(fistate_t* s, boolean yes)
{
    s->canSkip = yes;
}

/**
 * Stop playing the script and go to next game state.
 */
static void scriptTerminate(fistate_t* s)
{
    ddhook_scriptstop_paramaters_t p;
    int oldMode;

    if(!active)
        return;
    if(!s->canSkip)
        return;

    oldMode = s->mode;

    memset(&p, 0, sizeof(p));
    p.initialGameState = s->initialGameState;
    p.conditions = s->conditions;

    // This'll set fi to NULL.
    stackPop();

    if(oldMode != FIMODE_LOCAL)
    {
        // Tell clients to stop the finale.
        Sv_Finale(FINF_END, 0, NULL, 0);
    }

    Plug_DoHook(HOOK_INFINE_SCRIPT_TERMINATE, oldMode, &p);
}

static const char* scriptNextToken(fistate_t* s)
{
    char* out;

    // Skip whitespace.
    while(*s->cp && isspace(*s->cp))
        s->cp++;
    if(!*s->cp)
        return NULL; // The end has been reached.

    out = token;
    if(*s->cp == '"') // A string?
    {
        for(s->cp++; *s->cp; s->cp++)
        {
            if(*s->cp == '"')
            {
                s->cp++;
                // Convert double quotes to single ones.
                if(*s->cp != '"')
                    break;
            }

            *out++ = *s->cp;
        }
    }
    else
    {
        while(!isspace(*s->cp) && *s->cp)
            *out++ = *s->cp++;
    }
    *out++ = 0;

    return token;
}

static int scriptParseInteger(fistate_t* s)
{
    return strtol(scriptNextToken(s), NULL, 0);
}

static float scriptParseFloat(fistate_t* s)
{
    return strtod(scriptNextToken(s), NULL);
}

/**
 * Reads the next token, which should be floating point number. It is
 * considered seconds, and converted to tics.
 */
static int scriptReadTics(fistate_t* s)
{
    return (int) (scriptParseFloat(s) * 35 + 0.5);
}

static boolean parseOperands(int operands, const char* token, fistate_t* s)
{
    const char* oldcp;
    int i;

    // i stays at zero if the number of operands is correct.
    oldcp = s->cp;
    for(i = operands; i > 0; i--)
    {
        if(!scriptNextToken(s))
        {
            s->cp = oldcp;
            Con_Message("InFine: \"%s\" has too few operands.\n", token);
            break;
        }
    }

    // If there were enough operands, execute the command.
    s->cp = oldcp;
    return !i;
}

static boolean stateSkipRequest(fistate_t* s)
{
    s->waitingText = NULL; // Stop waiting for things.
    s->waitingPic = NULL;
    if(s->paused)
    {
        // Un-pause.
        s->paused = false;
        s->wait = 0;
        return true;
    }

    if(s->canSkip)
    {
        // Start skipping ahead.
        s->skipping = true;
        s->wait = 0;
        return true;
    }

    return s->eatEvents;
}

static boolean scriptSkipCommand(fistate_t* s, const ficmd_t* cmd)
{
    if((s->skipNext && !cmd->whenCondSkipping) ||
       ((s->skipping || s->gotoSkip) && !cmd->whenSkipping))
    {
        // While not DO-skipping, the condskip has now been done.
        if(!s->doLevel)
        {
            if(s->skipNext)
                s->lastSkipped = true;
            s->skipNext = false;
        }
        return true;
    }
    return false;
}

/**
 * Execute one (the next) command, advance script cursor.
 */
static boolean scriptExecuteCommand(fistate_t* s, const char* commandString)
{
    // Semicolon terminates DO-blocks.
    if(!strcmp(commandString, ";"))
    {
        if(s->doLevel > 0)
        {
            if(--s->doLevel == 0)
            {
                // The DO-skip has been completed.
                s->skipNext = false;
                s->lastSkipped = true;
            }
        }
        return false;
    }

    // We're now going to execute a command.
    cmdExecuted = true;

    // Is this a command we know how to execute?
    {ficmd_t* cmd;
    if((cmd = findCommand(commandString)))
    {
        // Check that there are enough operands.
        if(parseOperands(cmd->operands, cmd->token, s))
        {
            // Should we skip this command?
            if(scriptSkipCommand(s, cmd))
                return false;

            // Execute forthwith!
            cmd->func(cmd, s);
        }

        // The END command may clear the current state.
        if((s = stackTop()))
        {   // Now we've executed the latest command.
            s->lastSkipped = false;
        }
        return true; // Success!
    }}
    return false; // The command was not found.
}

static boolean scriptExecuteNextCommand(fistate_t* s)
{
    const char* cmd;
    if((cmd = scriptNextToken(s)))
    {
        return scriptExecuteCommand(s, cmd);
    }
    return false;
}

static fidata_pic_t* createPic(fistate_t* s, const char* name)
{
    fidata_pic_t* pic;
    if((pic = stateFindUnusedPic(s)))
    {
        fi_object_t* obj = &pic->object;

        // Mark it used.
        obj->used = true;

        // Initialize it.
        objectSetUniqueName(obj, name);
        value4Init(obj->color, 1, 1, 1, 1);
        value2Init(obj->scale, 1, 1);

        FIData_PicClearAnimation(pic);
        return pic;
    }
    return 0;
}

static fidata_text_t* createText(fistate_t* s, const char* name)
{
    fidata_text_t* text;
    if((text = stateFindUnusedText(s)))
    {
#define LEADING             (11.f/7-1)

        fi_object_t* obj = &text->object;
        float rgba[4];

        {int i;
        for(i = 0; i < 3; ++i)
            rgba[i] = fiDefaultTextRGB[i];
        }
        rgba[CA] = 1; // Opaque.

        // Mark it used.
        obj->used = true;

        // Initialize it.
        objectSetUniqueName(obj, name);
        value4Init(obj->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
        value2Init(obj->scale, 1, 1);

        text->wait = 3;
        text->font = R_CompositeFontNumForName("a");
        text->lineheight = LEADING;

        return text;

#undef LEADING
    }
    return 0;
}

/**
 * Find a @c fihandler_t for the specified ddkey code.
 * @param code              Unique id code of the ddkey event handler to look for.
 * @return                  Ptr to @c fihandler_t object. Either:
 *                          1) Existing handler associated with unique @a code.
 *                          2) New object with unique @a code.
 *                          3) @c NULL - No more objects.
 */
static fihandler_t* stateGetHandler(fistate_t* s, int code)
{
    fihandler_t* unused = NULL;
    {int i;
    for(i = 0; i < MAX_HANDLERS; ++i)
    {
        fihandler_t* hnd = &s->keyHandlers[i];

        // Use this if a suitable handler is not already set?
        if(!unused && !hnd->code)
            unused = hnd;

        if(hnd->code == code)
        {
            return hnd;
        }
    }}
    // May be NULL, if no more handlers available.
    return unused;
}

/**
 * Find a @c fidata_pic_t with the unique name @.
 * @param name              Unique name of the object we are looking for.
 * @return                  Ptr to @c fidata_pic_t object. Either:
 *                          1) Existing object associated with unique @a name.
 *                          2) New object with unique @a name.
 *                          3) @c NULL - No more objects.
 */
static fidata_pic_t* stateGetPic(fistate_t* s, const char* name)
{
    assert(name && name[0]);
    {
    fidata_pic_t* pic;
    // An exisiting Pic?
    if((pic = stateFindPic(s, name)))
        return pic;
    
    // Try allocate another?
    if((pic = createPic(s, name)))
        return pic;

    Con_Message("InFine: No room for Pic \"%s\".\n", name);
    return &dummyPic; // No more space.
    }
}

static fidata_text_t* stateGetText(fistate_t* s, const char* name)
{
    assert(name && name);
    {
    fidata_text_t* text;
    // An existing Text?
    if((text = stateFindText(s, name)))
        return text;

    // Try allocate another?
    if((text = createText(s, name)))
        return text;

    Con_Message("InFine: No room for Text \"%s\".\n", name);
    return &dummyText; // No more space.
    }
}

static void scriptTick(fistate_t* s)
{
    ddhook_scriptticker_paramaters_t p;
    int i, last = 0;

    memset(&p, 0, sizeof(p));
    p.runTick = true;
    p.canSkip = s->canSkip;
    p.gameState = (s->mode == FIMODE_OVERLAY? s->overlayGameState : s->initialGameState);
    p.conditions = s->conditions;

    Plug_DoHook(HOOK_INFINE_SCRIPT_TICKER, s->mode, &p);

    if(!p.runTick)
        return;

    // Test again - a call to scriptTerminate in a hook may have stopped this script.
    if(!active)
        return;

    s->timer++;

    // Interpolateable values.
    value4Think(s->bgColor);
    value2Think(s->imgOffset);
    value4Think(s->filter);
    for(i = 0; i < 9; ++i)
        value3Think(s->textColor[i]);

    for(i = 0; i < MAX_PICS; ++i)
    {
        fidata_pic_t* pic = &s->pics[i];
        if(!pic->object.used)
            continue;
        FIData_PicThink(pic);
    }

    // Text objects.
    for(i = 0; i < MAX_TEXT; ++i)
    {
        fidata_text_t* text = &s->text[i];
        if(!text->object.used)
            continue;
        FIData_TextThink(text);
    }

    // If we're waiting, don't execute any commands.
    if(s->wait && --s->wait)
        return;

    // If we're paused we can't really do anything.
    if(s->paused)
        return;

    // If we're waiting for a text to finish typing, do nothing.
    if(s->waitingText)
    {
        if(!s->waitingText->flags.all_visible)
            return;

        s->waitingText = NULL;
    }

    // Waiting for an animation to reach its end?
    if(s->waitingPic)
    {
        if(!s->waitingPic->flags.done)
            return;

        s->waitingPic = NULL;
    }

    // Execute commands until a wait time is set or we reach the end of
    // the script. If the end is reached, the finale really ends (terminates).
    while(active && !s->wait && !s->waitingText && !s->waitingPic && !last)
        last = !scriptExecuteNextCommand(s);

    // The script has ended!
    if(last)
    {
        scriptTerminate(s);
    }
}

static void scriptSkipTo(fistate_t* s, const char* marker)
{
    memset(s->gotoTarget, 0, sizeof(s->gotoTarget));
    strncpy(s->gotoTarget, marker, sizeof(s->gotoTarget) - 1);

    // Start skipping until the marker is found.
    s->gotoSkip = true;

    // Stop any waiting.
    s->wait = 0;
    s->waitingText = NULL;
    s->waitingPic = NULL;

    // Rewind the script so we can jump anywhere.
    s->cp = s->script;
}

static void rotate(float angle)
{
    // Counter the VGA aspect ratio.
    glScalef(1, 200.0f / 240.0f, 1);
    glRotatef(angle, 0, 0, 1);
    glScalef(1, 240.0f / 200.0f, 1);
}

static void stateDraw(fistate_t* s)
{
    // Draw the background.
    if(s->bgMaterial)
    {
        useColor(s->bgColor, 4);
        DGL_SetMaterial(s->bgMaterial);
        DGL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
    }
    else if(s->bgColor[3].value > 0)
    {
        // Just clear the screen, then.
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, SCREENWIDTH, SCREENHEIGHT, s->bgColor[0].value, s->bgColor[1].value, s->bgColor[2].value, s->bgColor[3].value);
        DGL_Enable(DGL_TEXTURING);
    }

    // Draw images.
    {int i;
    for(i = 0; i < MAX_PICS; ++i)
    {
        fidata_pic_t* pic = &s->pics[i];
        fi_object_t* obj = &pic->object;
        float mid[2];
        int sq;

        // Fully transparent pics will not be drawn.
        if(!obj->used || obj->color[3].value == 0)
            continue;

        sq = pic->seq;

        DGL_SetNoMaterial(); // Hmm...
        useColor(obj->color, 4);
        FIData_PicRotationOrigin(pic, mid);

        // Setup the transformation.
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(obj->pos[0].value - s->imgOffset[0].value, obj->pos[1].value - s->imgOffset[1].value, 0);
        glTranslatef(mid[VX], mid[VY], 0);

        rotate(obj->angle.value);

        // Move to origin.
        glTranslatef(-mid[VX], -mid[VY], 0);
        glScalef((pic->flip[sq] ? -1 : 1) * obj->scale[0].value, obj->scale[1].value, 1);

        // Draw it.
        if(pic->flags.is_rect)
        {
            if(pic->flags.is_ximage)
            {
                DGL_Enable(DGL_TEXTURING);
                DGL_Bind((DGLuint)pic->tex[sq]);
            }
            else
            {
                // The fill.
                DGL_Disable(DGL_TEXTURING);
            }

            glBegin(GL_QUADS);
                useColor(obj->color, 4);
                glTexCoord2f(0, 0);
                glVertex2f(0, 0);

                glTexCoord2f(1, 0);
                glVertex2f(1, 0);

                useColor(pic->otherColor, 4);
                glTexCoord2f(1, 1);
                glVertex2f(1, 1);

                glTexCoord2f(0, 1);
                glVertex2f(0, 1);
            glEnd();

            // The edges never have a texture.
            DGL_Disable(DGL_TEXTURING);

            glBegin(GL_LINES);
                useColor(pic->edgeColor, 4);
                glVertex2f(0, 0);
                glVertex2f(1, 0);
                glVertex2f(1, 0);

                useColor(pic->otherEdgeColor, 4);
                glVertex2f(1, 1);
                glVertex2f(1, 1);
                glVertex2f(0, 1);
                glVertex2f(0, 1);

                useColor(pic->edgeColor, 4);
                glVertex2f(0, 0);
            glEnd();

            DGL_Enable(DGL_TEXTURING);
        }
        else if(pic->flags.is_patch)
        {
            R_DrawPatch(R_FindPatchTex((patchid_t)pic->tex[sq]), 0, 0);
        }
        else
        {
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            /// \fixme What about rotaton?
            glTranslatef(obj->pos[0].value - s->imgOffset[0].value, obj->pos[1].value - s->imgOffset[1].value, 0);
            glScalef((pic->flip[sq]? -1 : 1) * obj->scale[0].value, obj->scale[1].value, 1);

            DGL_DrawRawScreen(pic->tex[sq], 0, 0);

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }

        // Restore original transformation.
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }}

    // Draw text.
    {int i;
    for(i = 0; i < MAX_TEXT; ++i)
    {
        fidata_text_t* tex = &s->text[i];
        if(!tex->object.used || !tex->text)
            continue;
        FIData_TextDraw(tex);
    }}

    // Filter on top of everything.
    if(s->filter[3].value > 0)
    {
        // Only draw if necessary.
        DGL_Disable(DGL_TEXTURING);
        useColor(s->filter, 4);

        glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(SCREENWIDTH, 0);
            glVertex2f(SCREENWIDTH, SCREENHEIGHT);
            glVertex2f(0, SCREENHEIGHT);
        glEnd();

        DGL_Enable(DGL_TEXTURING);
    }
}

static void scriptUseTextColor(fistate_t* s, fidata_text_t* text, int idx)
{
    if(!idx)
    {
        // The default color of the text.
        useColor(text->object.color, 4);
    }
    else
    {
        glColor4f(s->textColor[idx - 1][0].value,
                  s->textColor[idx - 1][1].value,
                  s->textColor[idx - 1][2].value,
                  text->object.color[3].value);
    }
}

static void demoEnds(void)
{
    fistate_t* s = stackTop();

    if(!s || !s->suspended)
        return;

    // Restore the InFine state.
    s->suspended = false;
    active = true;

    gx.FI_DemoEnds();
}

boolean FI_Active(void)
{
    return active;
}

boolean FI_CmdExecuted(void)
{
    return cmdExecuted; // Set to true after first command.
}

/**
 * Reset the entire InFine state stack.
 */
void FI_Reset(void)
{
    fistate_t* s = stackTop();

    // The state is suspended when the PlayDemo command is used.
    // Being suspended means that InFine is currently not active, but
    // will be restored at a later time.
    if(s && s->suspended)
        return;

    // Pop all the states.
    while(fi)
        stackPop();

    active = false;
}

/**
 * Start playing the given script.
 */
void FI_ScriptBegin(const char* scriptSrc, finale_mode_t mode, int gameState, const void* clientState)
{
    fistate_t* s;

    if(mode == FIMODE_LOCAL && isDedicated)
    {
        // Dedicated servers don't play local scripts.
#ifdef _DEBUG
        Con_Printf("FI_ScriptBegin: No local scripts in dedicated mode.\n");
#endif
        return;
    }

#ifdef _DEBUG
    Con_Printf("FI_ScriptBegin: mode=%i '%.30s'\n", mode, scriptSrc);
#endif

    // Init InFine state.
    s = newState(scriptSrc);
    stateInit(s, mode, gameState, clientState);
    scriptBegin(s);
}

void FI_ScriptTerminate(void)
{
    fistate_t* s;
    if(!active)
        return;
    if((s = stackTop()))
    {
        scriptCanSkip(s, true);
        scriptTerminate(s);
    }
}

void* FI_GetClientsideDefaultState(void)
{
    return &defaultState;
}

/**
 * Set the truth conditions.
 * Used by clients after they've received a PSV_FINALE2 packet.
 */
void FI_SetClientsideDefaultState(const void* data)
{
    assert(data);
    memcpy(&defaultState, data, sizeof(INFINE_CLIENTSTATE_SIZE));
}

/**
 * @return              @c true, if a command was found.
 *                      @c false if there are no more commands in the script.
 */
fihandler_t* FI_GetHandler(int code)
{
    fistate_t* s;
    if((s = stackTop()))
    {
        return stateGetHandler(s, code);
    }
    return 0;
}

void FI_Ticker(timespan_t ticLength)
{
    // Always tic.
    R_TextTicker(ticLength);

    if(!active)
        return;

    if(!M_CheckTrigger(&sharedFixedTrigger, ticLength))
        return;

    // A new 35 Hz tick has begun.
    {fistate_t* s;
    if((s = stackTop()))
    {
        scriptTick(s);
    }}
}

/**
 * The user has requested a skip. Returns true if the skip was done.
 */
int FI_SkipRequest(void)
{
    fistate_t* s;
    if((s = stackTop()))
    {
        return stateSkipRequest(s);
    }
    return false;
}

/**
 * @return              @c true, if the event should open the menu.
 */
boolean FI_IsMenuTrigger(void)
{
    fistate_t* s;
    if(!active)
        return false;
    if((s = stackTop()))
        return !s->showMenu;
    return false;
}

int FI_AteEvent(ddevent_t* ev)
{
    fistate_t* s;
    // We'll never eat up events.
    if(IS_TOGGLE_UP(ev))
        return false;
    if((s = stackTop()))
        return s->eatEvents;
    return false;
}

int FI_Responder(ddevent_t* ev)
{
    fistate_t* s = stackTop();

    if(!active || isClient)
        return false;

    // During the first ~second disallow all events/skipping.
    if(s->timer < 20)
        return FI_AteEvent(ev);

    if(IS_KEY_DOWN(ev))
    {
        // Any handlers for this key event?
        int i;
        for(i = 0; i < MAX_HANDLERS; ++i)
        {
            if(s->keyHandlers[i].code == ev->toggle.id)
            {
                scriptSkipTo(s, s->keyHandlers[i].marker);
                return FI_AteEvent(ev);
            }
        }
    }

    // If we can't skip, there's no interaction of any kind.
    if(!s->canSkip && !s->paused)
        return FI_AteEvent(ev);

    // We are only interested in key/button down presses.
    if(!IS_TOGGLE_DOWN(ev))
        return FI_AteEvent(ev);

    // Servers tell clients to skip.
    Sv_Finale(FINF_SKIP, 0, NULL, 0);
    return FI_SkipRequest();
}

int FI_CharWidth(int ch, compositefontid_t font)
{
    return GL_CharWidth(ch, font);
}

int FI_GetLineWidth(char* text, compositefontid_t font)
{
    int width = 0;

    for(; *text; text++)
    {
        if(*text == '\\')
        {
            if(!*++text)
                break;
            if(*text == 'n')
                break;
            if(*text >= '0' && *text <= '9')
                continue;
            if(*text == 'w' || *text == 'W' || *text == 'p' || *text == 'P')
                continue;
        }
        width += FI_CharWidth(*text, font);
    }

    return width;
}

/**
 * Drawing is the most complex task here.
 */
void FI_Drawer(void)
{
    // Don't draw anything until we are sure the script has started.
    if(!active || !cmdExecuted)
        return;

    {fistate_t* s;
    if((s = stackTop()))
    {
        stateDraw(s);
    }}
}

void FIData_PicThink(fidata_pic_t* p)
{
    assert(p);

    // Call parent thinker.
    FIObject_Think(&p->object);

    value4Think(p->otherColor);
    value4Think(p->edgeColor);
    value4Think(p->otherEdgeColor);

    // If animating, decrease the sequence timer.
    if(p->seqWait[p->seq])
    {
        if(--p->seqTimer <= 0)
        {
            // Advance the sequence position. k = next pos.
            int next = p->seq + 1;
            if(next == FIDATA_PIC_MAX_SEQUENCE || p->tex[next] == FI_REPEAT)
            {
                // Rewind back to beginning.
                next = 0;
                p->flags.done = true;
            }
            else if(p->tex[next] <= 0)
            {
                // This is the end. Stop sequence.
                p->seqWait[next = p->seq] = 0;
                p->flags.done = true;
            }

            // Advance to the next pos.
            p->seq = next;
            p->seqTimer = p->seqWait[next];

            // Play a sound?
            if(p->sound[next] > 0)
                S_LocalSound(p->sound[next], NULL);
        }
    }
}

void FIData_PicInit(fidata_pic_t* pic)
{
    assert(pic);

    value2Init(pic->object.pos, 0, 0);
    value2Init(pic->object.scale, 1, 1);

    // Default colors.
    value4Init(pic->object.color, 1, 1, 1, 1);
    value4Init(pic->otherColor, 1, 1, 1, 1);

    // Edge alpha is zero by default.
    value4Init(pic->edgeColor, 1, 1, 1, 0);
    value4Init(pic->otherEdgeColor, 1, 1, 1, 0);
}

void FIData_PicDeleteXImage(fidata_pic_t* pic)
{
    assert(pic);

    DGL_DeleteTextures(1, (DGLuint*)&pic->tex[0]);
    pic->tex[0] = 0;
    pic->flags.is_ximage = false;
}

void FIData_PicClearAnimation(fidata_pic_t* pic)
{
    assert(pic);

    // Kill the old texture.
    if(pic->flags.is_ximage)
        FIData_PicDeleteXImage(pic);

    memset(pic->tex, 0, sizeof(pic->tex));
    memset(pic->flip, 0, sizeof(pic->flip));
    memset(pic->sound, -1, sizeof(pic->sound));
    memset(pic->seqWait, 0, sizeof(pic->seqWait));
    pic->seq = 0;
    pic->flags.done = true;
}

int FIData_PicNextFrame(fidata_pic_t* pic)
{
    assert(pic);
    {
    int i;
    for(i = 0; i < FIDATA_PIC_MAX_SEQUENCE; ++i)
    {
        if(pic->tex[i] <= 0)
            break;
    }
    return i;
    }
}

void FIData_PicRotationOrigin(const fidata_pic_t* pic, float center[2])
{
    assert(pic);

    if(pic->flags.is_rect)
    {
        center[VX] = center[VY] = .5f;
    }
    else if(pic->flags.is_patch)
    {
        patchinfo_t info;

        if(R_GetPatchInfo(pic->tex[pic->seq], &info))
        {
            /// \fixme what about extraOffset?
            center[VX] = info.width / 2 - info.offset;
            center[VY] = info.height / 2 - info.topOffset;
        }
        else
        {
            center[VX] = center[VY] = 0;
        }
    }
    else
    {
        center[VX] = 160;
        center[VY] = 100;
    }

    center[VX] *= pic->object.scale[VX].value;
    center[VY] *= pic->object.scale[VY].value;
}

void FIData_TextThink(fidata_text_t* t)
{
    assert(t);

    // Call parent thinker.
    FIObject_Think(&t->object);

    if(t->wait)
    {
        if(--t->timer <= 0)
        {
            t->timer = t->wait;
            t->cursorPos++;
        }
    }

    if(t->scrollWait)
    {
        if(--t->scrollTimer <= 0)
        {
            t->scrollTimer = t->scrollWait;
            t->object.pos[1].target -= 1;
            t->object.pos[1].steps = t->scrollWait;
        }
    }

    // Is the text object fully visible?
    t->flags.all_visible = (!t->wait || t->cursorPos >= FIData_TextLength(t));
}

void FIData_TextDraw(fidata_text_t* tex)
{
    assert(tex);
    {
    fistate_t* s = stackTop();
    int cnt, x = 0, y = 0;
    int ch, linew = -1;
    char* ptr;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(tex->object.pos[0].value, tex->object.pos[1].value, 0);

    rotate(tex->object.angle.value);
    glScalef(tex->object.scale[0].value, tex->object.scale[1].value, 1);

    // Draw it.
    // Set color zero (the normal color).
    scriptUseTextColor(s, tex, 0);
    for(cnt = 0, ptr = tex->text; *ptr && (!tex->wait || cnt < tex->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = FI_GetLineWidth(ptr, tex->font);

        ch = *ptr;
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;

            // Change of color.
            if(*ptr >= '0' && *ptr <= '9')
            {
                scriptUseTextColor(s, tex, *ptr - '0');
                continue;
            }

            // 'w' = half a second wait, 'W' = second's wait
            if(*ptr == 'w' || *ptr == 'W') // Wait?
            {
                if(tex->wait)
                    cnt += (int) ((float)TICRATE / tex->wait / (*ptr == 'w' ? 2 : 1));
                continue;
            }

            // 'p' = 5 second wait, 'P' = 10 second wait
            if(*ptr == 'p' || *ptr == 'P') // Longer pause?
            {
                if(tex->wait)
                    cnt += (int) ((float)TICRATE / tex->wait * (*ptr == 'p' ? 5 : 10));
                continue;
            }

            if(*ptr == 'n' || *ptr == 'N') // Newline?
            {
                x = 0;
                y += GL_CharHeight('A', tex->font) * (1+tex->lineheight);
                linew = -1;
                cnt++; // Include newlines in the wait count.
                continue;
            }

            if(*ptr == '_')
                ch = ' ';
        }

        // Let's do Y-clipping (in case of tall text blocks).
        if(tex->object.scale[1].value * y + tex->object.pos[1].value >= -tex->object.scale[1].value * tex->lineheight &&
           tex->object.scale[1].value * y + tex->object.pos[1].value < SCREENHEIGHT)
        {
            GL_DrawChar2(ch, tex->flags.centered ? x - linew / 2 : x, y, tex->font);
            x += FI_CharWidth(ch, tex->font);
        }

        cnt++; // Actual character drawn.
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

/**
 * @return              The length as a counter.
 */
int FIData_TextLength(fidata_text_t* tex)
{
    float secondLen = (tex->wait ? 35.0f / tex->wait : 0);
    int cnt = 0;

    {char* ptr;
    for(ptr = tex->text; *ptr; ptr++)
    {
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;
            if(*ptr == 'w')
                cnt += secondLen / 2;
            if(*ptr == 'W')
                cnt += secondLen;
            if(*ptr == 'p')
                cnt += 5 * secondLen;
            if(*ptr == 'P')
                cnt += 10 * secondLen;
            if((*ptr >= '0' && *ptr <= '9') || *ptr == 'n' || *ptr == 'N')
                continue;
        }

        cnt++; // An actual character.
    }}

    return cnt;
}

void FIData_TextCopy(fidata_text_t* t, const char* str)
{
    assert(t);
    assert(str && str[0]);
    {
    size_t len = strlen(str) + 1;
    if(t->text)
        Z_Free(t->text); // Free any previous text.
    t->text = Z_Malloc(len, PU_STATIC, 0);
    memcpy(t->text, str, len);
    }
}

/**
 * Command functions can only call FI_GetToken once for each operand.
 * Otherwise the script cursor ends up in the wrong place.
 */
DEFFC(Do)
{
    // This command is called even when (cond)skipping.
    if(s->skipNext)
    {
        // A conditional skip has been issued.
        // We'll go into DO-skipping mode. skipnext won't be cleared
        // until the matching semicolon is found.
        s->doLevel++;
    }
}

DEFFC(End)
{
    s->wait = 1;
    scriptTerminate(s);
}

DEFFC(BGFlat)
{
    s->bgMaterial = Materials_ToMaterial(Materials_CheckNumForName(scriptNextToken(s), MN_FLATS));
}

DEFFC(BGTexture)
{
    s->bgMaterial = Materials_ToMaterial(Materials_CheckNumForName(scriptNextToken(s), MN_TEXTURES));
}

DEFFC(NoBGMaterial)
{
    s->bgMaterial = NULL;
}

DEFFC(InTime)
{
    s->inTime = scriptReadTics(s);
}

DEFFC(Tic)
{
    s->wait = 1;
}

DEFFC(Wait)
{
    s->wait = scriptReadTics(s);
}

DEFFC(WaitText)
{
    s->waitingText = stateGetText(s, scriptNextToken(s));
}

DEFFC(WaitAnim)
{
    s->waitingPic = stateGetPic(s, scriptNextToken(s));
}

DEFFC(Color)
{
    value3Set(s->bgColor, scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), s->inTime);
}

DEFFC(ColorAlpha)
{
    value4Set(s->bgColor, scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), s->inTime);
}

DEFFC(Pause)
{
    s->paused = true;
    s->wait = 1;
}

DEFFC(CanSkip)
{
    scriptCanSkip(s, true);
}

DEFFC(NoSkip)
{
    scriptCanSkip(s, false);
}

DEFFC(SkipHere)
{
    s->skipping = false;
}

DEFFC(Events)
{
    // Script will eat all input events.
    s->eatEvents = true;
}

DEFFC(NoEvents)
{
    // Script will pass unprocessed events to other responders.
    s->eatEvents = false;
}

DEFFC(OnKey)
{
    int code;
    fihandler_t* handler;

    // First argument is the key identifier.
    code = DD_GetKeyCode(scriptNextToken(s));

    // Read the marker name into token.
    scriptNextToken(s);

    // Find an empty handler.
    if((handler = FI_GetHandler(code)) != NULL)
    {
        handler->code = code;
        strncpy(handler->marker, token, sizeof(handler->marker) - 1);
    }
}

DEFFC(UnsetKey)
{
    fihandler_t* handler = FI_GetHandler(DD_GetKeyCode(scriptNextToken(s)));

    if(handler)
    {
        handler->code = 0;
        memset(handler->marker, 0, sizeof(handler->marker));
    }
}

DEFFC(If)
{
    boolean val = false;

    scriptNextToken(s);

    // Built-in conditions.
    if(!stricmp(token, "netgame"))
    {
        val = netGame;
    }
    // Any hooks?
    else if(Plug_CheckForHook(HOOK_INFINE_EVAL_IF))
    {
        ddhook_evalif_paramaters_t p;

        memset(&p, 0, sizeof(p));
        p.conditions = s->conditions;
        p.token = token;
        p.returnVal = 0;

        if(Plug_DoHook(HOOK_INFINE_EVAL_IF, 0, (void*) &p))
        {
            val = p.returnVal;
        }
    }
    else
    {
        Con_Message("FIC_If: Unknown condition \"%s\".\n", token);
    }

    // Skip the next command if the value is false.
    s->skipNext = !val;
}

DEFFC(IfNot)
{
    // This is the same as "if" but the skip condition is the opposite.
    FIC_If(cmd, s);
    s->skipNext = !s->skipNext;
}

DEFFC(Else)
{
    // The only time the ELSE condition doesn't skip is immediately
    // after a skip.
    s->skipNext = !s->lastSkipped;
}

DEFFC(GoTo)
{
    scriptSkipTo(s, scriptNextToken(s));
}

DEFFC(Marker)
{
    scriptNextToken(s);
    // Does it match the goto string?
    if(!stricmp(s->gotoTarget, token))
        s->gotoSkip = false;
}

DEFFC(Delete)
{
    fi_object_t* obj;
    if((obj = stateFindObject(s, scriptNextToken(s))))
    {
        obj->used = false;
    }
}

DEFFC(Image)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* name = scriptNextToken(s);

    FIData_PicClearAnimation(pic);

    if((pic->tex[0] = W_CheckNumForName(name)) == -1)
        Con_Message("FIC_Image: Warning, missing lump \"%s\".\n", name);

    pic->flags.is_patch = false;
    pic->flags.is_rect = false;
    pic->flags.is_ximage = false;
}

DEFFC(ImageAt)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* name;

    value2Init(pic->object.pos, scriptParseFloat(s), scriptParseFloat(s));
    FIData_PicClearAnimation(pic);

    name = scriptNextToken(s);
    if((pic->tex[0] = W_CheckNumForName(name)) == -1)
        Con_Message("FIC_ImageAt: Warning, missing lump \"%s\".\n", name);

    pic->flags.is_patch = false;
    pic->flags.is_rect = false;
    pic->flags.is_ximage = false;
}

DEFFC(XImage)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* fileName;

    FIData_PicClearAnimation(pic);

    // Load the external resource.
    fileName = scriptNextToken(s);
    if((pic->tex[0] = loadGraphics(DDRC_GRAPHICS, fileName, LGM_NORMAL, false, true, 0)) == 0)
        Con_Message("FIC_XImage: Warning, missing graphic \"%s\".\n", fileName);

    pic->flags.is_patch = false;
    pic->flags.is_rect = true;
    pic->flags.is_ximage = true;
}

DEFFC(Patch)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* name;
    patchid_t patch;

    value2Init(pic->object.pos, scriptParseFloat(s), scriptParseFloat(s));
    FIData_PicClearAnimation(pic);

    name = scriptNextToken(s);
    if((patch = R_PrecachePatch(name, NULL)) == 0)
        Con_Message("FIC_Patch: Warning, missing Patch \"%s\".\n", name);

    pic->tex[0] = patch;
    pic->flags.is_patch = true;
    pic->flags.is_rect = false;
}

DEFFC(SetPatch)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* name = scriptNextToken(s);
    patchid_t patch;

    if((patch = R_PrecachePatch(name, NULL)) != 0)
    {
        pic->tex[0] = patch;
        pic->flags.is_patch = true;
        pic->flags.is_rect = false;
    }
    else
    {
        Con_Message("FIC_SetPatch: Warning, missing Patch \"%s\".\n", name);
    }
}

DEFFC(ClearAnim)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    FIData_PicClearAnimation(pic);
}

DEFFC(Anim)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* name = scriptNextToken(s);
    patchid_t patch;
    int i, time;

    if((patch = R_PrecachePatch(name, NULL)) == 0)
        Con_Message("FIC_Anim: Warning, Patch \"%s\" not found.\n", name);

    time = scriptReadTics(s);
    // Find the next sequence spot.
    i = FIData_PicNextFrame(pic);
    if(i == FIDATA_PIC_MAX_SEQUENCE)
    {
        Con_Message("FIC_Anim: Warning, too many frames in anim sequence (max %i).\n", FIDATA_PIC_MAX_SEQUENCE);
        return; // Can't do it...
    }
    pic->tex[i] = patch;
    pic->seqWait[i] = time;
    pic->flags.is_patch = true;
    pic->flags.done = false;
}

DEFFC(AnimImage)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    const char* name = scriptNextToken(s);
    int i, lump, time;

    if((lump = W_CheckNumForName(name)) == -1)
        Con_Message("FIC_AnimImage: Warning, lump \"%s\" not found.\n", name);

    time = scriptReadTics(s);
    // Find the next sequence spot.
    i = FIData_PicNextFrame(pic);
    if(i == FIDATA_PIC_MAX_SEQUENCE)
    {
        Con_Message("FIC_AnimImage: Warning, too many frames in anim sequence "
                    "(max %i).\n", FIDATA_PIC_MAX_SEQUENCE);
        return; // Can't do it...
    }

    pic->tex[i] = lump;
    pic->seqWait[i] = time;
    pic->flags.is_patch = false;
    pic->flags.is_rect = false;
    pic->flags.done = false;
}

DEFFC(Repeat)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    int i = FIData_PicNextFrame(pic);

    if(i == FIDATA_PIC_MAX_SEQUENCE)
        return;
    pic->tex[i] = FI_REPEAT;
}

DEFFC(StateAnim)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    int stateId = Def_Get(DD_DEF_STATE, scriptNextToken(s), 0);
    int i, count = scriptParseInteger(s);
    spriteinfo_t sinf;

    // Animate N states starting from the given one.
    pic->flags.is_patch = true;
    pic->flags.is_rect = false;
    pic->flags.done = false;
    for(; count > 0 && stateId > 0; count--)
    {
        state_t* st = &states[stateId];

        i = FIData_PicNextFrame(pic);
        if(i == FIDATA_PIC_MAX_SEQUENCE)
            break; // No room!

        R_GetSpriteInfo(st->sprite, st->frame & 0x7fff, &sinf);
        pic->tex[i] = sinf.realLump;
        pic->flip[i] = sinf.flip;
        pic->seqWait[i] = st->tics;
        if(pic->seqWait[i] == 0)
            pic->seqWait[i] = 1;

        // Go to the next state.
        stateId = st->nextState;
    }
}

DEFFC(PicSound)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));
    int i;
    i = FIData_PicNextFrame(pic) - 1;
    if(i < 0)
        i = 0;
    pic->sound[i] = Def_Get(DD_DEF_SOUND, scriptNextToken(s), 0);
}

DEFFC(ObjectOffX)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float value = scriptParseFloat(s);

    if(obj)
    {
        valueSet(&obj->pos[0], value, s->inTime);
    }
}

DEFFC(ObjectOffY)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float value = scriptParseFloat(s);

    if(obj)
    {
        valueSet(&obj->pos[1], value, s->inTime);
    }
}

DEFFC(ObjectRGB)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    fidata_pic_t* pic = stateFindPic(s, obj ? obj->name : NULL);
    float rgb[3];
    
    {int i;
    for(i = 0; i < 3; ++i)
        rgb[i] = scriptParseFloat(s);
    }

    if(obj)
    {
        value3Set(obj->color, rgb[CR], rgb[CG], rgb[CB], s->inTime);

        if(pic && pic->flags.is_rect)
        {
            // This affects all the colors.
            value3Set(pic->otherColor, rgb[CR], rgb[CG], rgb[CB], s->inTime);
            value3Set(pic->edgeColor, rgb[CR], rgb[CG], rgb[CB], s->inTime);
            value3Set(pic->otherEdgeColor, rgb[CR], rgb[CG], rgb[CB], s->inTime);
        }
    }
}

DEFFC(ObjectAlpha)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    fidata_pic_t* pic = stateFindPic(s, obj ? obj->name : NULL);
    float value = scriptParseFloat(s);
    if(obj)
    {
        valueSet(&obj->color[3], value, s->inTime);

        if(pic && pic->flags.is_rect)
        {
            valueSet(&pic->otherColor[3], value, s->inTime);
            /*valueSet(&pic->edgeColor[3], value, s->inTime);
            valueSet(&pic->otherEdgeColor[3], value, s->inTime); */
        }
    }
}

DEFFC(ObjectScaleX)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float value = scriptParseFloat(s);
    if(obj)
    {
        valueSet(&obj->scale[0], value, s->inTime);
    }
}

DEFFC(ObjectScaleY)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float value = scriptParseFloat(s);
    if(obj)
    {
        valueSet(&obj->scale[1], value, s->inTime);
    }
}

DEFFC(ObjectScale)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float value = scriptParseFloat(s);
    if(obj)
    {
        value2Set(obj->scale, value, value, s->inTime);
    }
}

DEFFC(ObjectScaleXY)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float x = scriptParseFloat(s);
    float y = scriptParseFloat(s);
    if(obj)
    {
        value2Set(obj->scale, x, y, s->inTime);
    }
}

DEFFC(ObjectAngle)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    float value = scriptParseFloat(s);
    if(obj)
    {
        valueSet(&obj->angle, value, s->inTime);
    }
}

DEFFC(Rect)
{
    fidata_pic_t* pic = stateGetPic(s, scriptNextToken(s));

    FIData_PicInit(pic);

    // Position and size.
    value2Init(pic->object.pos, scriptParseFloat(s), scriptParseFloat(s));
    value2Init(pic->object.scale, scriptParseFloat(s), scriptParseFloat(s));

    pic->flags.is_rect = true;
    pic->flags.is_patch = false;
    pic->flags.is_ximage = false;
    pic->flags.done = true;
}

DEFFC(FillColor)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    fidata_pic_t* pic;
    int which = 0;
    float rgba[4];

    if(!obj)
    {   // Skip the parms.
        int i;
        for(i = 0; i < 5; ++i)
            scriptNextToken(s);
        return;
    }

    pic = stateGetPic(s, obj->name);

    // Which colors to modify?
    scriptNextToken(s);
    if(!stricmp(token, "top"))
        which |= 1;
    else if(!stricmp(token, "bottom"))
        which |= 2;
    else
        which = 3;

    {int i;
    for(i = 0; i < 4; ++i)
        rgba[i] = scriptParseFloat(s);
    }

    if(which & 1)
        value4Set(obj->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
    if(which & 2)
        value4Set(pic->otherColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
}

DEFFC(EdgeColor)
{
    fi_object_t* obj = stateFindObject(s, scriptNextToken(s));
    fidata_pic_t* pic;
    int which = 0;
    float rgba[4];

    if(!obj)
    {   // Skip the parms.
        int i;
        for(i = 0; i < 5; ++i)
            scriptNextToken(s);
        return;
    }

    pic = stateGetPic(s, obj->name);

    // Which colors to modify?
    scriptNextToken(s);
    if(!stricmp(token, "top"))
        which |= 1;
    else if(!stricmp(token, "bottom"))
        which |= 2;
    else
        which = 3;

    {int i;
    for(i = 0; i < 4; ++i)
        rgba[i] = scriptParseFloat(s);
    }

    if(which & 1)
        value4Set(pic->edgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
    if(which & 2)
        value4Set(pic->otherEdgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
}

DEFFC(OffsetX)
{
    valueSet(&s->imgOffset[0], scriptParseFloat(s), s->inTime);
}

DEFFC(OffsetY)
{
    valueSet(&s->imgOffset[1], scriptParseFloat(s), s->inTime);
}

DEFFC(Sound)
{
    int num = Def_Get(DD_DEF_SOUND, scriptNextToken(s), NULL);
    if(num > 0)
        S_LocalSound(num, NULL);
}

DEFFC(SoundAt)
{
    int num = Def_Get(DD_DEF_SOUND, scriptNextToken(s), NULL);
    float vol = scriptParseFloat(s);

    vol = MIN_OF(vol, 1);
    if(vol > 0 && num > 0)
        S_LocalSoundAtVolume(num, NULL, vol);
}

DEFFC(SeeSound)
{
    int num = Def_Get(DD_DEF_MOBJ, scriptNextToken(s), NULL);

    if(num < 0 || mobjInfo[num].seeSound <= 0)
        return;
    S_LocalSound(mobjInfo[num].seeSound, NULL);
}

DEFFC(DieSound)
{
    int num = Def_Get(DD_DEF_MOBJ, scriptNextToken(s), NULL);

    if(num < 0 || mobjInfo[num].deathSound <= 0)
        return;
    S_LocalSound(mobjInfo[num].deathSound, NULL);
}

DEFFC(Music)
{
    S_StartMusic(scriptNextToken(s), true);
}

DEFFC(MusicOnce)
{
    S_StartMusic(scriptNextToken(s), false);
}

DEFFC(Filter)
{
    value4Set(s->filter, scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), s->inTime);
}

DEFFC(Text)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));

    value2Init(tex->object.pos, scriptParseFloat(s), scriptParseFloat(s));
    FIData_TextCopy(tex, scriptNextToken(s));
    tex->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromDef)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    char* str;

    value2Init(tex->object.pos, scriptParseFloat(s), scriptParseFloat(s));
    if(!Def_Get(DD_DEF_TEXT, scriptNextToken(s), &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(tex, str);
    tex->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromLump)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    int lnum;

    value2Init(tex->object.pos, scriptParseFloat(s), scriptParseFloat(s));
    lnum = W_CheckNumForName(scriptNextToken(s));
    if(lnum < 0)
    {
        FIData_TextCopy(tex, "(not found)");
    }
    else
    {
        size_t i, incount, buflen;
        const char* data;
        char* str, *out;

        // Load the lump.
        data = W_CacheLumpNum(lnum, PU_STATIC);
        incount = W_LumpLength(lnum);
        str = Z_Malloc(buflen = 2 * incount + 1, PU_STATIC, 0);
        memset(str, 0, buflen);
        for(i = 0, out = str; i < incount; ++i)
        {
            if(data[i] == '\n')
            {
                *out++ = '\\';
                *out++ = 'n';
            }
            else
            {
                *out++ = data[i];
            }
        }
        W_ChangeCacheTag(lnum, PU_CACHE);
        FIData_TextCopy(tex, str);
        Z_Free(str);
    }
    tex->cursorPos = 0; // Restart.
}

DEFFC(SetText)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    FIData_TextCopy(tex, scriptNextToken(s));
}

DEFFC(SetTextDef)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    char* str;
    if(!Def_Get(DD_DEF_TEXT, scriptNextToken(s), &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(tex, str);
}

DEFFC(DeleteText)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->object.used = false;
    if(tex->text)
    {
        // Free the memory allocated for the text string.
        Z_Free(tex->text);
        tex->text = NULL;
    }
}

DEFFC(TextColor)
{
    int idx = scriptParseInteger(s);
    idx = MINMAX_OF(1, idx, 9);
    value3Set(s->textColor[idx - 1], scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), s->inTime);
}

DEFFC(TextRGB)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    value3Set(tex->object.color, scriptParseFloat(s), scriptParseFloat(s), scriptParseFloat(s), s->inTime);
}

DEFFC(TextAlpha)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    valueSet(&tex->object.color[CA], scriptParseFloat(s), s->inTime);
}

DEFFC(TextOffX)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    valueSet(&tex->object.pos[0], scriptParseFloat(s), s->inTime);
}

DEFFC(TextOffY)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    valueSet(&tex->object.pos[1], scriptParseFloat(s), s->inTime);
}

DEFFC(TextCenter)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->flags.centered = true;
}

DEFFC(TextNoCenter)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->flags.centered = false;
}

DEFFC(TextScroll)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->scrollTimer = 0;
    tex->scrollWait = scriptParseInteger(s);
}

DEFFC(TextPos)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->cursorPos = scriptParseInteger(s);
}

DEFFC(TextRate)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->wait = scriptParseInteger(s);
}

DEFFC(TextLineHeight)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->lineheight = scriptParseFloat(s);
}

DEFFC(Font)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    const char* fontName = scriptNextToken(s);
    compositefontid_t font;
    if((font = R_CompositeFontNumForName(fontName)))
    {
        tex->font = font;
        return;
    }
    Con_Message("FIC_Font: Warning, unknown font '%s'.\n", fontName);
}

DEFFC(FontA)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->font = R_CompositeFontNumForName("a");
}

DEFFC(FontB)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    tex->font = R_CompositeFontNumForName("b");
}

DEFFC(NoMusic)
{
    // Stop the currently playing song.
    S_StopMusic();
}

DEFFC(TextScaleX)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    valueSet(&tex->object.scale[0], scriptParseFloat(s), s->inTime);
}

DEFFC(TextScaleY)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    valueSet(&tex->object.scale[1], scriptParseFloat(s), s->inTime);
}

DEFFC(TextScale)
{
    fidata_text_t* tex = stateGetText(s, scriptNextToken(s));
    value2Set(tex->object.scale, scriptParseFloat(s), scriptParseFloat(s), s->inTime);
}

DEFFC(PlayDemo)
{
    // Mark the current state as suspended, so we know to resume it when the demo ends.
    s->suspended = true;
    active = false;

    // The only argument is the demo file name.
    // Start playing the demo.
    if(!Con_Executef(CMDS_DDAY, true, "playdemo \"%s\"", scriptNextToken(s)))
    {
        // Demo playback failed. Here we go again...
        demoEnds();
    }
}

DEFFC(Command)
{
    Con_Executef(CMDS_SCRIPT, false, scriptNextToken(s));
}

DEFFC(ShowMenu)
{
    s->showMenu = true;
}

DEFFC(NoShowMenu)
{
    s->showMenu = false;
}

D_CMD(StartFinale)
{
    finale_mode_t mode = (active? FIMODE_OVERLAY : FIMODE_LOCAL); //(G_GetGameState() == GS_MAP? FIMODE_OVERLAY : FIMODE_LOCAL);
    char* script;

    if(active)
        return false;

    if(!Def_Get(DD_DEF_FINALE, argv[1], &script))
    {
        Con_Printf("Script \"%s\" is not defined.\n", argv[1]);
        return false;
    }

    // The overlay mode doesn't affect the current game state.
    FI_ScriptBegin(script, mode, gx.FI_GetGameState(), 0);
    return true;
}

D_CMD(StopFinale)
{
    FI_ScriptTerminate();
    return true;
}
