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
#define MAX_HANDLERS        (128)

#define FRACSECS_TO_TICKS(sec) ((int)(sec * TICSPERSEC + 0.5))

// Helper macro for defining infine command functions.
#define DEFFC(name) void FIC_##name(const fi_cmd_t* cmd, const fi_operand_t* ops, fi_state_t* s)

// TYPES -------------------------------------------------------------------

typedef struct {
    cvartype_t      type;
    union {
        int         integer;
        float       flt;
        const char* cstring;
    } data;
} fi_operand_t;

typedef struct fi_handler_s {
    int             ddkey;
    fi_objectname_t marker;
} fi_handler_t;

typedef struct fi_object_collection_s {
    uint            num;
    fi_object_t**   vector;
} fi_object_collection_t;

typedef struct fi_state_s {
    struct fi_state_flags_s {
        char            can_skip:1;
        char            suspended:1;
        char            paused:1;
        char            eat_events:1; // Script will eat all input events.
        char            show_menu:1;
    } flags;
    finale_mode_t   mode;
    fi_object_collection_t objects;
    fi_handler_t    eventHandlers[MAX_HANDLERS];
    char*           script; // A copy of the script.
    const char*     cp; // The command cursor.
    int             timer;
    int             inTime;
    boolean         cmdExecuted, skipping, lastSkipped, gotoSkip, skipNext;
    int             doLevel; // Level of DO-skipping.
    int             wait;
    fi_objectname_t gotoTarget;
    fi_object_t*    waitingText;
    fi_object_t*    waitingPic;
    material_t*     bgMaterial;
    animatorvector4_t bgColor;
    animatorvector4_t imgColor;
    animatorvector2_t imgOffset;
    animatorvector4_t filter;
    animatorvector3_t textColor[9];

    int             initialGameState; // Game state before the script began.
    int             overlayGameState; // Overlay scripts run only in one gameMode.
    void*           extraData;
} fi_state_t;

typedef struct fi_cmd_s {
    char*           token;
    const char*     operands;
    void          (*func) (struct fi_cmd_s* cmd, fi_operand_t* ops, fi_state_t* s);
    struct fi_cmd_flags_s {
        char            when_skipping:1;
        char            when_condition_skipping:1; // Skipping because condition failed.
    } flags;
} fi_cmd_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

fidata_text_t*  P_CreateText(fi_objectid_t id, const char* name);
void            P_DestroyText(fidata_text_t* text);

fidata_pic_t*   P_CreatePic(fi_objectid_t id, const char* name);
void            P_DestroyPic(fidata_pic_t* pic);

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
DEFFC(ObjectOffZ);
DEFFC(ObjectScaleX);
DEFFC(ObjectScaleY);
DEFFC(ObjectScaleZ);
DEFFC(ObjectScale);
DEFFC(ObjectScaleXY);
DEFFC(ObjectScaleXYZ);
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
DEFFC(PredefinedTextColor);
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

// Time is measured in seconds.
// Colors are floating point and [0,1].
static fi_cmd_t commands[] = {
    // Run Control
    { "DO",         "", FIC_Do, true, true },
    { "END",        "", FIC_End },
    { "IF",         "s", FIC_If }, // if (value-id)
    { "IFNOT",      "s", FIC_IfNot }, // ifnot (value-id)
    { "ELSE",       "", FIC_Else },
    { "GOTO",       "s", FIC_GoTo }, // goto (marker)
    { "MARKER",     "s", FIC_Marker, true },
    { "in",         "f", FIC_InTime }, // in (time)
    { "pause",      "", FIC_Pause },
    { "tic",        "", FIC_Tic },
    { "wait",       "f", FIC_Wait }, // wait (time)
    { "waittext",   "s", FIC_WaitText }, // waittext (id)
    { "waitanim",   "s", FIC_WaitAnim }, // waitanim (id)
    { "canskip",    "", FIC_CanSkip },
    { "noskip",     "", FIC_NoSkip },
    { "skiphere",   "", FIC_SkipHere, true },
    { "events",     "", FIC_Events },
    { "noevents",   "", FIC_NoEvents },
    { "onkey",      "ss", FIC_OnKey }, // onkey (keyname) (marker)
    { "unsetkey",   "s", FIC_UnsetKey }, // unsetkey (keyname)

    // Screen Control
    { "color",      "fff", FIC_Color }, // color (red) (green) (blue)
    { "coloralpha", "ffff", FIC_ColorAlpha }, // coloralpha (r) (g) (b) (a)
    { "flat",       "s", FIC_BGFlat }, // flat (flat-id)
    { "texture",    "s", FIC_BGTexture }, // texture (texture-id)
    { "noflat",     "", FIC_NoBGMaterial },
    { "notexture",  "", FIC_NoBGMaterial },
    { "offx",       "f", FIC_OffsetX }, // offx (x)
    { "offy",       "f", FIC_OffsetY }, // offy (y)
    { "filter",     "ffff", FIC_Filter }, // filter (r) (g) (b) (a)

    // Audio
    { "sound",      "s", FIC_Sound }, // sound (snd)
    { "soundat",    "sf", FIC_SoundAt }, // soundat (snd) (vol:0-1)
    { "seesound",   "s", FIC_SeeSound }, // seesound (mobjtype)
    { "diesound",   "s", FIC_DieSound }, // diesound (mobjtype)
    { "music",      "s", FIC_Music }, // music (musicname)
    { "musiconce",  "s", FIC_MusicOnce }, // musiconce (musicname)
    { "nomusic",    "", FIC_NoMusic },

    // Objects
    { "del",        "s", FIC_Delete }, // del (id)
    { "x",          "sf", FIC_ObjectOffX }, // x (id) (x)
    { "y",          "sf", FIC_ObjectOffY }, // y (id) (y)
    { "z",          "sf", FIC_ObjectOffZ }, // z (id) (z)
    { "sx",         "sf", FIC_ObjectScaleX }, // sx (id) (x)
    { "sy",         "sf", FIC_ObjectScaleY }, // sy (id) (y)
    { "sz",         "sf", FIC_ObjectScaleZ }, // sz (id) (z)
    { "scale",      "sf", FIC_ObjectScale }, // scale (id) (factor)
    { "scalexy",    "sff", FIC_ObjectScaleXY }, // scalexy (id) (x) (y)
    { "scalexyz",   "sfff", FIC_ObjectScaleXYZ }, // scalexyz (id) (x) (y) (z)
    { "rgb",        "sfff", FIC_ObjectRGB }, // rgb (id) (r) (g) (b)
    { "alpha",      "sf", FIC_ObjectAlpha }, // alpha (id) (alpha)
    { "angle",      "sf", FIC_ObjectAngle }, // angle (id) (degrees)

    // Rects
    { "rect",       "sffff", FIC_Rect }, // rect (hndl) (x) (y) (w) (h)
    { "fillcolor",  "ssffff", FIC_FillColor }, // fillcolor (h) (top/bottom/both) (r) (g) (b) (a)
    { "edgecolor",  "ssffff", FIC_EdgeColor }, // edgecolor (h) (top/bottom/both) (r) (g) (b) (a)

    // Pics
    { "image",      "ss", FIC_Image }, // image (id) (raw-image-lump)
    { "imageat",    "sffs", FIC_ImageAt }, // imageat (id) (x) (y) (raw)
    { "ximage",     "ss", FIC_XImage }, // ximage (id) (ext-gfx-filename)
    { "patch",      "sffs", FIC_Patch }, // patch (id) (x) (y) (patch)
    { "set",        "ss", FIC_SetPatch }, // set (id) (lump)
    { "clranim",    "s", FIC_ClearAnim }, // clranim (id)
    { "anim",       "ssf", FIC_Anim }, // anim (id) (patch) (time)
    { "imageanim",  "ssf", FIC_AnimImage }, // imageanim (hndl) (raw-img) (time)
    { "picsound",   "ss", FIC_PicSound }, // picsound (hndl) (sound)
    { "repeat",     "s", FIC_Repeat }, // repeat (id)
    { "states",     "ssi", FIC_StateAnim }, // states (id) (state) (count)

    // Text
    { "text",       "sffs", FIC_Text }, // text (hndl) (x) (y) (string)
    { "textdef",    "sffs", FIC_TextFromDef }, // textdef (hndl) (x) (y) (txt-id)
    { "textlump",   "sffs", FIC_TextFromLump }, // textlump (hndl) (x) (y) (lump)
    { "settext",    "ss", FIC_SetText }, // settext (id) (newtext)
    { "settextdef", "ss", FIC_SetTextDef }, // settextdef (id) (txt-id)
    { "center",     "s", FIC_TextCenter }, // center (id)
    { "nocenter",   "s", FIC_TextNoCenter }, // nocenter (id)
    { "scroll",     "sf", FIC_TextScroll }, // scroll (id) (speed)
    { "pos",        "si", FIC_TextPos }, // pos (id) (pos)
    { "rate",       "si", FIC_TextRate }, // rate (id) (rate)
    { "font",       "ss", FIC_Font }, // font (id) (font)
    { "fonta",      "s", FIC_FontA }, // fonta (id)
    { "fontb",      "s", FIC_FontB }, // fontb (id)
    { "linehgt",    "sf", FIC_TextLineHeight }, // linehgt (hndl) (hgt)

    // Game Control
    { "playdemo",   "s", FIC_PlayDemo }, // playdemo (filename)
    { "cmd",        "s", FIC_Command }, // cmd (console command)
    { "trigger",    "", FIC_ShowMenu },
    { "notrigger",  "", FIC_NoShowMenu },

    // Misc.
    { "precolor",   "ifff", FIC_PredefinedTextColor }, // precolor (num) (r) (g) (b)

    // Deprecated Pic commands
    { "delpic",     "s", FIC_Delete }, // delpic (id)

    // Deprecated Text commands
    { "deltext",    "s", FIC_DeleteText }, // deltext (hndl)
    { "textrgb",    "sfff", FIC_TextRGB }, // textrgb (id) (r) (g) (b)
    { "textalpha",  "sf", FIC_TextAlpha }, // textalpha (id) (alpha)
    { "tx",         "sf", FIC_TextOffX }, // tx (id) (x)
    { "ty",         "sf", FIC_TextOffY }, // ty (id) (y)
    { "tsx",        "sf", FIC_TextScaleX }, // tsx (id) (x)
    { "tsy",        "sf", FIC_TextScaleY }, // tsy (id) (y)
    { "textscale",  "sf", FIC_TextScale }, // textscale (id) (x) (y)

    { NULL, 0, NULL } // Terminate.
};

static fi_state_t stateStack[STACK_SIZE];
static fi_state_t* fi; // Pointer to the current state in the stack.
static char token[MAX_TOKEN_LEN];

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

static fi_cmd_t* findCommand(const char* name)
{
    int i;
    for(i = 0; commands[i].token; ++i)
    {
        fi_cmd_t* cmd = &commands[i];
        if(!stricmp(name, cmd->token))
        {
            return cmd;
        }
    }
    return 0; // Not found.
}

static void useColor(animator_t *color, int components)
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

static void objectSetName(fi_object_t* obj, const char* name)
{
    strncpy(obj->name, name, sizeof(obj->name) - 1);
}

static fi_state_t* allocState(void)
{
    fi_state_t* s;

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
Con_Printf("  State index: %i.\n", fi - stateStack);
#endif

    memset(s, 0, sizeof(*s));
    return s;
}

static fi_state_t* newState(const char* script)
{
    fi_state_t* s = allocState();
    size_t size;

    // Take a copy of the script.
    size = strlen(script);
    s->script = Z_Malloc(size + 1, PU_STATIC, 0);
    memcpy(s->script, script, size);
    s->script[size] = '\0';
    
    s->cp = s->script; // Init cursor.

    return s;
}

static void objectsThink(fi_object_collection_t* c)
{
    uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* obj = c->vector[i];
        switch(obj->type)
        {
        case FI_PIC:    FIData_PicThink((fidata_pic_t*)obj);    break;
        case FI_TEXT:   FIData_TextThink((fidata_text_t*)obj);  break;
        default: break;
        }
    }
}

static void objectsDraw2(fi_object_collection_t* c, fi_obtype_e type, float xOffset, float yOffset)
{
    uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* obj = c->vector[i];
        if(obj->type != type)
            continue;
        switch(obj->type)
        {
        case FI_PIC:    FIData_PicDraw((fidata_pic_t*)obj, xOffset, yOffset);   break;
        case FI_TEXT:   FIData_TextDraw((fidata_text_t*)obj, xOffset, yOffset); break;
        default: break;
        }
    }
}

static void objectsDraw(fi_object_collection_t* c, float picXOffset, float picYOffset)
{
    // Images first, then text.
    objectsDraw2(c, FI_PIC, picXOffset, picYOffset);
    objectsDraw2(c, FI_TEXT, 0, 0);
}

static void objectsClear(fi_object_collection_t* c)
{
    // Delete external images, text strings etc.
    if(c->vector)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* obj = c->vector[i];
            switch(obj->type)
            {
            case FI_PIC:    P_DestroyPic((fidata_pic_t*)obj);   break;
            case FI_TEXT:   P_DestroyText((fidata_text_t*)obj); break;
            default:
                break;
            }
        }
        Z_Free(c->vector);
    }
    c->vector = NULL;
    c->num = 0;
}

static fi_objectid_t toObjectId(fi_object_collection_t* c, const char* name, fi_obtype_e type)
{
    assert(name && name[0]);
    {uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* obj = c->vector[i];
        if((type == FI_NONE || obj->type == type) && !stricmp(obj->name, name))
            return obj->id;
    }}
    return 0;
}

static fi_object_t* objectsById(fi_object_collection_t* c, fi_objectid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* obj = c->vector[i];
            if(obj->id == id)
                return obj;
        }
    }
    return NULL;
}

static fi_objectid_t objectsFind(fi_object_collection_t* c, const char* name)
{
    fi_objectid_t id;
    // First check all pics.
    id = toObjectId(c, name, FI_PIC);
    // Then check text objects.
    if(!id)
        id = toObjectId(c, name, FI_TEXT);
    return 0;
}

static fi_object_t* objectsAdd(fi_object_collection_t* c, fi_object_t* obj)
{
    // Link with this state.
    c->vector = Z_Realloc(c->vector, sizeof(obj) * ++c->num, PU_STATIC);
    c->vector[c->num-1] = obj;
    return obj;
}

static void objectsRemove(fi_object_collection_t* c, fi_object_t* obj)
{
    uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* other = c->vector[i];
        if(other == obj)
        {
            if(i != c->num-1)
                memmove(&c->vector[i], &c->vector[i+1], sizeof(*c->vector) * (c->num-i));

            if(c->num > 1)
            {
                c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * --c->num, PU_STATIC);
            }
            else
            {
                Z_Free(c->vector);
                c->vector = NULL;
                c->num = 0;
            }
            return;
        }
    }
}

static void stateChangeMode(fi_state_t* s, finale_mode_t mode)
{
    s->mode = mode;
}

static void stateSetExtraData(fi_state_t* s, const void* data)
{
    if(!data)
        return;
    if(!s->extraData || !(FINALE_SCRIPT_EXTRADATA_SIZE > 0))
        return;
    memcpy(s->extraData, data, FINALE_SCRIPT_EXTRADATA_SIZE);
}

static void stateSetInitialGameState(fi_state_t* s, int gameState, const void* clientState)
{
    s->initialGameState = gameState;
    s->flags.show_menu = true; // Enabled by default.

    if(FINALE_SCRIPT_EXTRADATA_SIZE > 0)
    {
        stateSetExtraData(s, &defaultState);
        if(clientState)
            stateSetExtraData(s, clientState);
    }

    if(s->mode == FIMODE_OVERLAY)
    {   // Overlay scripts stop when the gameMode changes.
        s->overlayGameState = gameState;
    }
}

static void stateReleaseScript(fi_state_t* s)
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
static void stateClear(fi_state_t* s)
{
    int i;

    active = true;
    s->flags.suspended = false;
    s->flags.can_skip = true; // By default skipping is enabled.
    s->flags.paused = false;

    s->timer = 0;
    s->cmdExecuted = false; // Nothing is drawn until a cmd has been executed.
    s->skipping = false;
    s->wait = 0; // Not waiting for anything.
    s->inTime = 0; // Interpolation is off.
    s->bgMaterial = NULL; // No background material.
    s->gotoSkip = false;
    s->skipNext = false;

    s->waitingText = NULL;
    s->waitingPic = NULL;
    memset(s->gotoTarget, 0, sizeof(s->gotoTarget));
    memset(s->imgOffset, 0, sizeof(s->imgOffset));
    memset(s->filter, 0, sizeof(s->filter));
    
    AnimatorVector4_Init(s->bgColor, 1, 1, 1, 1);

    objectsClear(&s->objects);

    for(i = 0; i < 9; ++i)
    {
        AnimatorVector3_Init(s->textColor[i], 1, 1, 1);
    }
}

static void stateInit(fi_state_t* s, finale_mode_t mode, int gameState, const void* clientState)
{
    stateClear(s);
    stateChangeMode(s, mode);
    stateSetInitialGameState(s, gameState, clientState);
}

static __inline fi_state_t* stackTop(void)
{
    return fi;
}

static void stackPop(void)
{
    fi_state_t* s = stackTop();

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

static void scriptBegin(fi_state_t* s)
{
    if(s->mode != FIMODE_LOCAL)
    {
        int flags = FINF_BEGIN | (s->mode == FIMODE_AFTER ? FINF_AFTER : s->mode == FIMODE_OVERLAY ? FINF_OVERLAY : 0);
        ddhook_finale_script_serialize_extradata_t p;
        boolean haveExtraData = false;

        memset(&p, 0, sizeof(p));

        if(s->extraData)
        {
            p.extraData = s->extraData;
            p.outBuf = 0;
            p.outBufSize = 0;
            haveExtraData = Plug_DoHook(HOOK_FINALE_SCRIPT_SERIALIZE_EXTRADATA, 0, &p);
        }

        // Tell clients to start this script.
        Sv_Finale(flags, s->script, (haveExtraData? p.outBuf : 0), (haveExtraData? p.outBufSize : 0));
    }

    // Any hooks?
    Plug_DoHook(HOOK_FINALE_SCRIPT_BEGIN, (int) s->mode, s->extraData);
}

static void scriptCanSkip(fi_state_t* s, boolean yes)
{
    s->flags.can_skip = yes;
}

/**
 * Stop playing the script and go to next game state.
 */
static void scriptTerminate(fi_state_t* s)
{
    ddhook_finale_script_stop_paramaters_t p;
    int oldMode;

    if(!active)
        return;
    if(!s->flags.can_skip)
        return;

#ifdef _DEBUG
    Con_Printf("Finale End: mode=%i '%.30s'\n", s->mode, s->script);
#endif

    oldMode = s->mode;

    memset(&p, 0, sizeof(p));
    p.initialGameState = s->initialGameState;
    p.extraData = s->extraData;

    // This'll set fi to NULL.
    stackPop();

    if(oldMode != FIMODE_LOCAL)
    {
        // Tell clients to stop the finale.
        Sv_Finale(FINF_END, 0, NULL, 0);
    }

    Plug_DoHook(HOOK_FINALE_SCRIPT_TERMINATE, oldMode, &p);
}

static const char* scriptNextToken(fi_state_t* s)
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

/**
 * Parse the command operands from the script. If successfull, a ptr to a new
 * vector of @c fi_operand_t objects is returned. Ownership of the vector is
 * given to the caller.
 *
 * @return                      Ptr to a new vector of @c fi_operand_t or @c NULL.
 */
static fi_operand_t* scriptParseCommandOperands(fi_state_t* s, const fi_cmd_t* cmd, uint* count)
{
    const char* origCursorPos;
    uint numOperands;
    fi_operand_t* ops = 0;

    if(!cmd->operands || !cmd->operands[0])
        return NULL;

    origCursorPos = s->cp;
    numOperands = (uint)strlen(cmd->operands);

    // Operands are read sequentially. This is to potentially allow for
    // command overloading at a later time.
    {uint i = 0;
    do
    {
        char typeSymbol = cmd->operands[i];
        fi_operand_t* op;
        cvartype_t type;

        if(!scriptNextToken(s))
        {
            s->cp = origCursorPos;
            if(ops)
                free(ops);
            if(count)
                *count = 0;
            Con_Message("scriptParseCommandOperands: Too few operands for command '%s'.\n", cmd->token);
            return NULL;
        }

        switch(typeSymbol)
        {
        // Supported operand type symbols:
        case 'i': type = CVT_INT;      break;
        case 'f': type = CVT_FLOAT;    break;
        case 's': type = CVT_CHARPTR;  break;
        default:
            Con_Error("scriptParseCommandOperands: Invalid symbol '%c' in operand list for command '%s'.", typeSymbol, cmd->token);
        }

        if(!ops)
            ops = malloc(sizeof(*ops));
        else
            ops = realloc(ops, sizeof(*ops) * (i+1));
        op = &ops[i];

        op->type = type;
        switch(type)
        {
        case CVT_INT:
            op->data.integer = strtol(token, NULL, 0);
            break;

        case CVT_FLOAT:
            op->data.flt = strtod(token, NULL);
            break;
        case CVT_CHARPTR:
            {
            size_t len = strlen(token)+1;
            char* str = malloc(len);
            dd_snprintf(str, len, "%s", token);
            op->data.cstring = str;
            break;
            }
        }
    } while(++i < numOperands);}

    s->cp = origCursorPos;

    if(count)
        *count = numOperands;
    return ops;
}

static boolean stateSkipRequest(fi_state_t* s)
{
    s->waitingText = NULL; // Stop waiting for things.
    s->waitingPic = NULL;
    if(s->flags.paused)
    {
        s->flags.paused = false; // Un-pause.
        s->wait = 0;
        return true;
    }

    if(s->flags.can_skip)
    {
        s->skipping = true; // Start skipping ahead.
        s->wait = 0;
        return true;
    }

    return (s->flags.eat_events != 0);
}

static boolean scriptSkipCommand(fi_state_t* s, const fi_cmd_t* cmd)
{
    if((s->skipNext && !cmd->flags.when_condition_skipping) ||
       ((s->skipping || s->gotoSkip) && !cmd->flags.when_skipping))
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
static boolean scriptExecuteCommand(fi_state_t* s, const char* commandString)
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
        return true; // Success!
    }

    // We're now going to execute a command.
    s->cmdExecuted = true;

    // Is this a command we know how to execute?
    {fi_cmd_t* cmd;
    if((cmd = findCommand(commandString)))
    {
        boolean requiredOperands = (cmd->operands && cmd->operands[0]);
        fi_operand_t* ops = NULL;
        size_t count;

        // Check that there are enough operands.
        if(!requiredOperands || (ops = scriptParseCommandOperands(s, cmd, &count)))
        {
            // Should we skip this command?
            if(scriptSkipCommand(s, cmd))
                return false;

            // Execute forthwith!
            cmd->func(cmd, ops, s);
        }

        if(ops)
        {   // Destroy.
            {uint i;
            for(i = 0; i < count; ++i)
            {
                fi_operand_t* op = &ops[i];
                if(op->type == CVT_CHARPTR)
                    free((char*)op->data.cstring);
            }}
            free(ops);
        }

        // The END command may clear the current state.
        if((s = stackTop()))
        {   // Now we've executed the latest command.
            s->lastSkipped = false;
        }
    }}
    return true; // Success!
}

static boolean scriptExecuteNextCommand(fi_state_t* s)
{
    const char* token;
    if((token = scriptNextToken(s)))
    {
        scriptExecuteCommand(s, token);
        return true;
    }
    return false;
}

static fi_handler_t* stateFindHandler(fi_state_t* s, int ddkey)
{
    uint i;
    for(i = 0; i < MAX_HANDLERS; ++i)
    {
        fi_handler_t* h = &s->eventHandlers[i];
        if(h->ddkey == ddkey)
            return h;
    }
    return 0;
}
/**
 * Find a @c fi_handler_t for the specified ddkey code.
 * @param ddkey             Unique id code of the ddkey event handler to look for.
 * @return                  Ptr to @c fi_handler_t object. Either:
 *                          1) Existing handler associated with unique @a code.
 *                          2) New object with unique @a code.
 *                          3) @c NULL - No more objects.
 */
static fi_handler_t* stateGetHandler(fi_state_t* s, int ddkey)
{
    // First, try to find an existing handler.
    fi_handler_t* h;
    if((h = stateFindHandler(s, ddkey)))
        return h;
    // Now lets try to create a new handler.
    {uint i = 0;
    while(!h && i++ < MAX_HANDLERS)
    {
        fi_handler_t* other = &s->eventHandlers[i];
        // Use this if a suitable handler is not already set?
        if(other->ddkey == 0)
            h = other;
    }}
    // May be NULL, if no more handlers available.
    return h;
}

/**
 * @return                  A new (unused) unique object id.
 */
static fi_objectid_t stateUniqueObjectId(fi_state_t* s)
{
    fi_objectid_t id = 0;
    while(objectsById(&s->objects, ++id));
    return id;
}

static fi_object_t* stateFindObject(fi_state_t*s, const char* name, fi_obtype_e type)
{
    return objectsById(&s->objects, toObjectId(&s->objects, name, type));
}

/**
 * Find an @c fi_object_t of type with the type-unique name.
 * @param name              Unique name of the object we are looking for.
 * @return                  Ptr to @c fi_object_t Either:
 *                          1) Existing object associated with unique @a name.
 *                          2) New object with unique @a name.
 *                          3) @c NULL - No more objects.
 */
static fi_object_t* stateGetObject(fi_state_t* s, const char* name, fi_obtype_e type)
{
    assert(name && name);
    assert(type > FI_NONE);
    {
    fi_object_t* obj;
    // An existing object?
    if((obj = stateFindObject(s, name, type)))
        return obj;
    // Allocate and attach another.
    switch(type)
    {
    case FI_TEXT: obj = (fi_object_t*) P_CreateText(stateUniqueObjectId(s), name); break;
    case FI_PIC:  obj = (fi_object_t*) P_CreatePic(stateUniqueObjectId(s), name); break;
    }
    return objectsAdd(&s->objects, obj);
    }
}

static void scriptTick(fi_state_t* s)
{
    ddhook_finale_script_ticker_paramaters_t p;
    int i, last = 0;

    memset(&p, 0, sizeof(p));
    p.runTick = true;
    p.canSkip = (s->flags.can_skip != 0);
    p.gameState = (s->mode == FIMODE_OVERLAY? s->overlayGameState : s->initialGameState);
    p.extraData = s->extraData;

    Plug_DoHook(HOOK_FINALE_SCRIPT_TICKER, s->mode, &p);

    if(!p.runTick)
        return;

    // Test again - a call to scriptTerminate in a hook may have stopped this script.
    if(!active)
        return;

    s->timer++;

    // Interpolateable values.
    AnimatorVector4_Think(s->bgColor);
    AnimatorVector2_Think(s->imgOffset);
    AnimatorVector4_Think(s->filter);
    for(i = 0; i < 9; ++i)
        AnimatorVector3_Think(s->textColor[i]);

    objectsThink(&s->objects);

    // If we're waiting, don't execute any commands.
    if(s->wait && --s->wait)
        return;

    // If we're paused we can't really do anything.
    if(s->flags.paused)
        return;

    // If we're waiting for a text to finish typing, do nothing.
    if(s->waitingText && s->waitingText->type == FI_TEXT)
    {
        if(!((fidata_text_t*)s->waitingText)->animComplete)
            return;

        s->waitingText = NULL;
    }

    // Waiting for an animation to reach its end?
    if(s->waitingPic && s->waitingPic->type == FI_PIC)
    {
        if(!((fidata_pic_t*)s->waitingPic)->animComplete)
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

static void scriptSkipTo(fi_state_t* s, const char* marker)
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

static void stateDraw(fi_state_t* s)
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

    objectsDraw(&s->objects, -s->imgOffset[0].value, -s->imgOffset[1].value);

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

static void demoEnds(void)
{
    fi_state_t* s = stackTop();

    if(!s || !s->flags.suspended)
        return;

    // Restore the InFine state.
    s->flags.suspended = false;
    active = true;

    gx.FI_DemoEnds();
}

static void picFrameDeleteXImage(fidata_pic_frame_t* f)
{
    DGL_DeleteTextures(1, (DGLuint*)&f->texRef.tex);
    f->texRef.tex = 0;
}

static fidata_pic_frame_t* createPicFrame(int type, int tics, void* texRef, short sound)
{
    fidata_pic_frame_t* f = (fidata_pic_frame_t*) Z_Malloc(sizeof(*f), PU_STATIC, 0);
    f->flags.flip = false;
    f->type = type;
    f->tics = tics;
    switch(f->type)
    {
    case PFT_MATERIAL:  f->texRef.material = ((material_t*)texRef);     break;
    case PFT_PATCH:     f->texRef.patch = *((patchid_t*)texRef);    break;
    case PFT_RAW:       f->texRef.lump  = *((lumpnum_t*)texRef);    break;
    case PFT_XIMAGE:    f->texRef.tex = *((DGLuint*)texRef);        break;
    default:
        Con_Error("Error - InFine: unknown frame type %i.", (int)type);
    }
    f->sound = sound;
    return f;
}

static void destroyPicFrame(fidata_pic_frame_t* f)
{
    if(f->type == PFT_XIMAGE)
        picFrameDeleteXImage(f);
    Z_Free(f);
}

static fidata_pic_frame_t* picAddFrame(fidata_pic_t* p, fidata_pic_frame_t* f)
{
    p->frames = Z_Realloc(p->frames, sizeof(*p->frames) * ++p->numFrames, PU_STATIC);
    p->frames[p->numFrames-1] = f;
    return f;
}

static void picRotationOrigin(const fidata_pic_t* p, uint frame, float center[3])
{
    if(p->numFrames && frame < p->numFrames)
    {
        fidata_pic_frame_t* f = p->frames[frame];
        switch(f->type)
        {
        case PFT_PATCH:
            {
            patchinfo_t info;
            if(R_GetPatchInfo(f->texRef.patch, &info))
            {
                /// \fixme what about extraOffset?
                center[VX] = info.width / 2 - info.offset;
                center[VY] = info.height / 2 - info.topOffset;
                center[VZ] = 0;
            }
            else
            {
                center[VX] = center[VY] = center[VZ] = 0;
            }
            break;
            }
        case PFT_RAW:
        case PFT_XIMAGE:
            center[VX] = SCREENWIDTH/2;
            center[VY] = SCREENHEIGHT/2;
            center[VZ] = 0;
            break;
        case PFT_MATERIAL:
            center[VX] = center[VY] = center[VZ] = 0;
            break;
        }
    }
    else
    {
        center[VX] = center[VY] = .5f;
        center[VZ] = 0;
    }

    center[VX] *= p->scale[VX].value;
    center[VY] *= p->scale[VY].value;
    center[VZ] *= p->scale[VZ].value;
}

fidata_pic_t* P_CreatePic(fi_objectid_t id, const char* name)
{
    fidata_pic_t* p = Z_Calloc(sizeof(*p), PU_STATIC, 0);

    p->id = id;
    p->type = FI_PIC;
    objectSetName((fi_object_t*)p, name);
    AnimatorVector4_Init(p->color, 1, 1, 1, 1);
    AnimatorVector3_Init(p->scale, 1, 1, 1);

    FIData_PicClearAnimation(p);
    return p;
}

void P_DestroyPic(fidata_pic_t* p)
{
    assert(p);
    FIData_PicClearAnimation(p);
    Z_Free(p);
}

fidata_text_t* P_CreateText(fi_objectid_t id, const char* name)
{
#define LEADING             (11.f/7-1)

    fidata_text_t* t = Z_Calloc(sizeof(*t), PU_STATIC, 0);
    float rgba[4];

    {int i;
    for(i = 0; i < 3; ++i)
        rgba[i] = fiDefaultTextRGB[i];
    }
    rgba[CA] = 1; // Opaque.

    // Initialize it.
    t->id = id;
    t->type = FI_TEXT;
    t->textFlags = DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS;
    objectSetName((fi_object_t*)t, name);
    AnimatorVector4_Init(t->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
    AnimatorVector3_Init(t->scale, 1, 1, 1);

    t->wait = 3;
    t->font = R_CompositeFontNumForName("a");
    t->lineheight = LEADING;

    return t;

#undef LEADING
}

void P_DestroyText(fidata_text_t* text)
{
    assert(text);
    if(text->text)
    {
        // Free the memory allocated for the text string.
        Z_Free(text->text);
        text->text = NULL;
    }
    Z_Free(text);
}

void FIObject_Think(fi_object_t* obj)
{
    assert(obj);

    AnimatorVector3_Think(obj->pos);
    AnimatorVector3_Think(obj->scale);
    Animator_Think(&obj->angle);
}

boolean FI_Active(void)
{
    return active;
}

boolean FI_CmdExecuted(void)
{
    fi_state_t* s;
    if(active && (s = stackTop()))
    {
        return s->cmdExecuted; // Set to true after first command.
    }
    return false;
}

/**
 * Reset the entire InFine state stack.
 */
void FI_Reset(void)
{
    fi_state_t* s;
    if(active && (s = stackTop()))
    {
        // The state is suspended when the PlayDemo command is used.
        // Being suspended means that InFine is currently not active, but
        // will be restored at a later time.
        if(s && s->flags.suspended)
            return;

        // Pop all the states.
        while(fi)
            stackPop();
    }
    active = false;
}

/**
 * Start playing the given script.
 */
void FI_ScriptBegin(const char* scriptSrc, finale_mode_t mode, int gameState, const void* clientState)
{
    fi_state_t* s;

    if(mode == FIMODE_LOCAL && isDedicated)
    {
        // Dedicated servers don't play local scripts.
#ifdef _DEBUG
        Con_Printf("Finale Begin: No local scripts in dedicated mode.\n");
#endif
        return;
    }

#ifdef _DEBUG
    Con_Printf("Finale Begin: mode=%i '%.30s'\n", mode, scriptSrc);
#endif

    // Init InFine state.
    s = newState(scriptSrc);
    stateInit(s, mode, gameState, clientState);
    scriptBegin(s);
}

void FI_ScriptTerminate(void)
{
    fi_state_t* s;
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
    memcpy(&defaultState, data, sizeof(FINALE_SCRIPT_EXTRADATA_SIZE));
}

void FI_Ticker(timespan_t ticLength)
{
    // Always tic.
    R_TextTicker(ticLength);

    if(!active)
        return;

    if(!M_CheckTrigger(&sharedFixedTrigger, ticLength))
        return;

    // A new 'sharp' tick has begun.
    {fi_state_t* s;
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
    fi_state_t* s;
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
    fi_state_t* s;
    if(!active)
        return false;
    if((s = stackTop()))
        return (s->flags.show_menu != 0);
    return false;
}

static int willEatEvent(ddevent_t* ev)
{
    fi_state_t* s;
    // We'll never eat up events.
    if(IS_TOGGLE_UP(ev))
        return false;
    if((s = stackTop()))
        return (s->flags.eat_events != 0);
    return false;
}

int FI_Responder(ddevent_t* ev)
{
    fi_state_t* s = stackTop();

    if(!active || isClient)
        return false;

    // During the first ~second disallow all events/skipping.
    if(s->timer < 20)
        return willEatEvent(ev);

    // Any handlers for this event?
    if(IS_KEY_DOWN(ev))
    {   
        fi_handler_t* h;
        if((h = stateFindHandler(s, ev->toggle.id)))
        {
            scriptSkipTo(s, h->marker);
            return willEatEvent(ev);
        }
    }

    // If we can't skip, there's no interaction of any kind.
    if(!s->flags.can_skip && !s->flags.paused)
        return willEatEvent(ev);

    // We are only interested in key/button down presses.
    if(!IS_TOGGLE_DOWN(ev))
        return willEatEvent(ev);

    // Servers tell clients to skip.
    Sv_Finale(FINF_SKIP, 0, NULL, 0);
    return FI_SkipRequest();
}

/**
 * Drawing is the most complex task here.
 */
void FI_Drawer(void)
{
    if(!active)
        return;

    {fi_state_t* s;
    if((s = stackTop()))
    {
        // Don't draw anything until we are sure the script has started.
        if(!s->cmdExecuted)
            return;

        stateDraw(s);
    }}
}

void FIData_PicThink(fidata_pic_t* p)
{
    assert(p);

    // Call parent thinker.
    FIObject_Think((fi_object_t*)p);

    AnimatorVector4_Think(p->color);
    AnimatorVector4_Think(p->otherColor);
    AnimatorVector4_Think(p->edgeColor);
    AnimatorVector4_Think(p->otherEdgeColor);

    if(!(p->numFrames > 1))
        return;

    // If animating, decrease the sequence timer.
    if(p->frames[p->curFrame]->tics > 0)
    {
        if(--p->tics <= 0)
        {
            fidata_pic_frame_t* f;
            // Advance the sequence position. k = next pos.
            int next = p->curFrame + 1;

            if(next == p->numFrames)
            {   // This is the end.
                p->animComplete = true;

                // Stop the sequence?
                if(p->flags.looping)
                    next = 0; // Rewind back to beginning.
                else // Yes.
                    p->frames[next = p->curFrame]->tics = 0;
            }

            // Advance to the next pos.
            f = p->frames[p->curFrame = next];
            p->tics = f->tics;

            // Play a sound?
            if(f->sound > 0)
                S_LocalSound(f->sound, NULL);
        }
    }
}

static void drawRect(fidata_pic_t* p, uint frame, float angle, const float worldOffset[3])
{
    assert(p->numFrames && frame < p->numFrames);
    {
    float mid[3];
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : NULL);

    assert(f->type == PFT_MATERIAL);

    mid[VX] = mid[VY] = mid[VZ] = 0;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(p->pos[0].value + worldOffset[VX], p->pos[1].value + worldOffset[VY], p->pos[2].value);
    glTranslatef(mid[VX], mid[VY], mid[VZ]);

    // Counter the VGA aspect ratio.
    if(p->angle.value != 0)
    {
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(p->angle.value, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    // Move to origin.
    glTranslatef(-mid[VX], -mid[VY], -mid[VZ]);
    glScalef((p->numFrames && p->frames[p->curFrame]->flags.flip ? -1 : 1) * p->scale[0].value, p->scale[1].value, p->scale[2].value);

    {
    float offset[2] = { 0, 0 }, scale[2] = { 1, 1 }, color[4], bottomColor[4];
    int magMode = DGL_LINEAR, width = 1, height = 1;
    DGLuint tex = 0;
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : NULL);
    material_t* mat;

    if((mat = f->texRef.material))
    {
        material_load_params_t params;
        material_snapshot_t ms;
        surface_t suf;

        suf.header.type = DMU_SURFACE; /// \fixme: perhaps use the dummy object system?
        suf.owner = 0;
        suf.decorations = 0;
        suf.numDecorations = 0;
        suf.flags = suf.oldFlags = (f->flags.flip? DDSUF_MATERIAL_FLIPH : 0);
        suf.inFlags = SUIF_PVIS|SUIF_BLEND;
        suf.material = mat;
        suf.normal[VX] = suf.oldNormal[VX] = 0;
        suf.normal[VY] = suf.oldNormal[VY] = 0;
        suf.normal[VZ] = suf.oldNormal[VZ] = 1; // toward the viewer.
        suf.offset[0] = suf.visOffset[0] = suf.oldOffset[0][0] = suf.oldOffset[1][0] = worldOffset[VX];
        suf.offset[1] = suf.visOffset[1] = suf.oldOffset[0][1] = suf.oldOffset[1][1] = worldOffset[VY];
        suf.visOffsetDelta[0] = suf.visOffsetDelta[1] = 0;
        suf.rgba[CR] = p->color[0].value;
        suf.rgba[CG] = p->color[1].value;
        suf.rgba[CB] = p->color[2].value;
        suf.rgba[CA] = p->color[3].value;
        suf.blendMode = BM_NORMAL;

        memset(&params, 0, sizeof(params));
        params.pSprite = false;
        params.tex.border = 0; // Need to allow for repeating.
        Materials_Prepare(&ms, suf.material, (suf.inFlags & SUIF_BLEND), &params);

        {int i;
        for(i = 0; i < 4; ++i)
            color[i] = bottomColor[i] = suf.rgba[i];
        }

        if(ms.units[MTU_PRIMARY].texInst)
        {
            tex = ms.units[MTU_PRIMARY].texInst->id;
            magMode = ms.units[MTU_PRIMARY].magMode;
            offset[0] = ms.units[MTU_PRIMARY].offset[0];
            offset[1] = ms.units[MTU_PRIMARY].offset[1];
            scale[0] = 1;//ms.units[MTU_PRIMARY].scale[0];
            scale[1] = 1;//ms.units[MTU_PRIMARY].scale[1];
            color[CA] *= ms.units[MTU_PRIMARY].alpha;
            bottomColor[CA] *= ms.units[MTU_PRIMARY].alpha;
            width = ms.width;
            height = ms.height;
        }
    }

    // The fill.
    if(tex)
    {
        /// \fixme: do not override the value taken from the Material snapshot.
        magMode = (filterUI ? GL_LINEAR : GL_NEAREST);

        GL_BindTexture(tex, magMode);

        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glTranslatef(offset[0], offset[1], 0);
        glScalef(scale[0], scale[1], 0);
    }
    else
        DGL_Disable(DGL_TEXTURING);

    glBegin(GL_QUADS);
        glColor4fv(color);
        glTexCoord2f(0, 0);
        glVertex2f(0, 0);

        glTexCoord2f(1, 0);
        glVertex2f(0+width, 0);

        glColor4fv(bottomColor);
        glTexCoord2f(1, 1);
        glVertex2f(0+width, 0+height);

        glTexCoord2f(0, 1);
        glVertex2f(0, 0+height);
    glEnd();

    if(tex)
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }
    else
    {
        DGL_Enable(DGL_TEXTURING);
    }
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

static __inline boolean useRect(const fidata_pic_t* p, uint frame)
{
    fidata_pic_frame_t* f;
    if(!p->numFrames)
        return false;
    if(frame >= p->numFrames)
        return true;
    f = p->frames[frame];
    if(f->type == PFT_MATERIAL)
        return true;
    return false;
}

/**
 * Vertex layout:
 *
 * 0 - 1
 * | / |
 * 2 - 3
 */
static size_t buildGeometry(DGLuint tex, const float rgba[4], const float rgba2[4],
    boolean flagTexFlip, rvertex_t** verts, rcolor_t** colors, rtexcoord_t** coords)
{
    static rvertex_t rvertices[4];
    static rcolor_t rcolors[4];
    static rtexcoord_t rcoords[4];

    V3_Set(rvertices[0].pos, 0, 0, 0);
    V3_Set(rvertices[1].pos, 1, 0, 0);
    V3_Set(rvertices[2].pos, 0, 1, 0);
    V3_Set(rvertices[3].pos, 1, 1, 0);

    if(tex)
    {
        V2_Set(rcoords[0].st, (flagTexFlip? 1:0), 0);
        V2_Set(rcoords[1].st, (flagTexFlip? 0:1), 0);
        V2_Set(rcoords[2].st, (flagTexFlip? 1:0), 1);
        V2_Set(rcoords[3].st, (flagTexFlip? 0:1), 1);
    }

    V4_Copy(rcolors[0].rgba, rgba);
    V4_Copy(rcolors[1].rgba, rgba);
    V4_Copy(rcolors[2].rgba, rgba2);
    V4_Copy(rcolors[3].rgba, rgba2);

    *verts = rvertices;
    *coords = (tex!=0? rcoords : 0);
    *colors = rcolors;
    return 4;
}

static void drawGeometry(DGLuint tex, size_t numVerts, const rvertex_t* verts,
    const rcolor_t* colors, const rtexcoord_t* coords)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    if(tex)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filterUI ? GL_LINEAR : GL_NEAREST));
    }
    else
        DGL_Disable(DGL_TEXTURING);

    glBegin(GL_TRIANGLE_STRIP);
    {size_t i;
    for(i = 0; i < numVerts; ++i)
    {
        if(coords) glTexCoord2fv(coords[i].st);
        if(colors) glColor4fv(colors[i].rgba);
        glVertex3fv(verts[i].pos);
    }}
    glEnd();

    if(!tex)
        DGL_Enable(DGL_TEXTURING);
}

static void drawPicFrame(fidata_pic_t* p, uint frame, const float _origin[3],
    const float scale[3], const float rgba[4], const float rgba2[4], float angle,
    const float worldOffset[3])
{
    if(useRect(p, frame))
    {
        drawRect(p, frame, angle, worldOffset);
        return;
    }

    {
    vec3_t offset = { 0, 0, 0 }, dimensions, origin, originOffset, center;
    boolean showEdges = true, flipTextureS = false;
    DGLuint tex = 0;
    size_t numVerts;
    rvertex_t* rvertices;
    rcolor_t* rcolors;
    rtexcoord_t* rcoords;

    if(p->numFrames)
    {
        fidata_pic_frame_t* f = p->frames[frame];
        patchtex_t* patch;
        rawtex_t* rawTex;

        flipTextureS = (f->flags.flip != 0);
        showEdges = false;

        if(f->type == PFT_RAW && (rawTex = R_GetRawTex(f->texRef.lump)))
        {   
            tex = GL_PrepareRawTex(rawTex);
            V3_Set(offset, SCREENWIDTH/2, SCREENHEIGHT/2, 0);
            V3_Set(dimensions, rawTex->width, rawTex->height, 1);
        }
        else if(f->type == PFT_XIMAGE)
        {
            tex = (DGLuint)f->texRef.tex;
            V3_Set(offset, SCREENWIDTH/2, SCREENHEIGHT/2, 0);
            V3_Set(dimensions, 1, 1, 1); /// \fixme.
        }
        /*else if(f->type == PFT_MATERIAL)
        {
            V3_Set(dimensions, 1, 1, 1);
        }*/
        else if(f->type == PFT_PATCH && (patch = R_FindPatchTex(f->texRef.patch)))
        {
            tex = (renderTextures==1? GL_PreparePatch(patch) : 0);
            V3_Set(offset, patch->offX, patch->offY, 0);
            /// \todo need to decide what if any significance what depth will mean here.
            V3_Set(dimensions, patch->width, patch->height, 1);
        }
    }

    // If we've not chosen a texture by now set some defaults.
    if(!tex)
    {
        V3_Set(dimensions, 1, 1, 1);
    }

    V3_Set(center, dimensions[VX] / 2, dimensions[VY] / 2, dimensions[VZ] / 2);

    V3_Sum(origin, _origin, center);
    V3_Subtract(origin, origin, offset);
    V3_Sum(origin, origin, worldOffset);

    V3_Subtract(originOffset, offset, center);              
    offset[VX] *= scale[VX]; offset[VY] *= scale[VY]; offset[VZ] *= scale[VZ];
    V3_Sum(originOffset, originOffset, offset);

    numVerts = buildGeometry(tex, rgba, rgba2, flipTextureS, &rvertices, &rcolors, &rcoords);

    // Setup the transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Move to the object origin.
    glTranslatef(origin[VX], origin[VY], origin[VZ]);

    if(angle != 0)
    {
        // With rotation we must counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(angle, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    // Translate to the object center.
    glTranslatef(originOffset[VX], originOffset[VY], originOffset[VZ]);
    glScalef(scale[VX] * dimensions[VX], scale[VY] * dimensions[VY], scale[VZ] * dimensions[VZ]);

    drawGeometry(tex, numVerts, rvertices, rcolors, rcoords);

    if(showEdges)
    {
        // The edges never have a texture.
        DGL_Disable(DGL_TEXTURING);

        glBegin(GL_LINES);
            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
            glVertex2f(1, 0);
            glVertex2f(1, 0);

            useColor(p->otherEdgeColor, 4);
            glVertex2f(1, 1);
            glVertex2f(1, 1);
            glVertex2f(0, 1);
            glVertex2f(0, 1);

            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
        glEnd();
        
        DGL_Enable(DGL_TEXTURING);
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

void FIData_PicDraw(fidata_pic_t* p, float xOffset, float yOffset)
{
    assert(p);
    {
    vec3_t scale, origin;
    vec4_t rgba, rgba2;
    vec3_t worldOffset;

    // Fully transparent pics will not be drawn.
    if(!(p->color[CA].value > 0))
        return;

    V3_Set(worldOffset, xOffset, yOffset, 0); /// \todo What is this for?

    V3_Set(origin, p->pos[VX].value, p->pos[VY].value, p->pos[VZ].value);
    V3_Set(scale, p->scale[VX].value, p->scale[VY].value, p->scale[VZ].value);
    V4_Set(rgba, p->color[CR].value, p->color[CG].value, p->color[CB].value, p->color[CA].value);
    if(p->numFrames == 0)
        V4_Set(rgba2, p->otherColor[CR].value, p->otherColor[CG].value, p->otherColor[CB].value, p->otherColor[CA].value);

    drawPicFrame(p, p->curFrame, origin, scale, rgba, (p->numFrames==0? rgba2 : rgba), p->angle.value, worldOffset);
    }
}

void FIData_PicClearAnimation(fidata_pic_t* p)
{
    assert(p);
    if(p->frames)
    {
        uint i;
        for(i = 0; i < p->numFrames; ++i)
            destroyPicFrame(p->frames[i]);
        Z_Free(p->frames);
    }
    p->flags.looping = false; // Yeah?
    p->frames = 0;
    p->numFrames = 0;
    p->curFrame = 0;
    p->animComplete = true;
}

void FIData_TextThink(fidata_text_t* t)
{
    assert(t);

    // Call parent thinker.
    FIObject_Think((fi_object_t*)t);

    AnimatorVector4_Think(t->color);

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
            t->pos[1].target -= 1;
            t->pos[1].steps = t->scrollWait;
        }
    }

    // Is the text object fully visible?
    t->animComplete = (!t->wait || t->cursorPos >= FIData_TextLength(t));
}

static int textLineWidth(const char* text, compositefontid_t font)
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
        width += GL_CharWidth(*text, font);
    }

    return width;
}

void FIData_TextDraw(fidata_text_t* tex, float xOffset, float yOffset)
{
    assert(tex);
    {
    fi_state_t* s = stackTop();
    int cnt, x = 0, y = 0;
    int ch, linew = -1;
    char* ptr;

    if(!tex->text)
        return;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(tex->pos[0].value + xOffset, tex->pos[1].value + yOffset, tex->pos[2].value);

    rotate(tex->angle.value);
    glScalef(tex->scale[0].value, tex->scale[1].value, tex->scale[2].value);

    // Draw it.
    // Set color zero (the normal color).
    useColor(tex->color, 4);
    for(cnt = 0, ptr = tex->text; *ptr && (!tex->wait || cnt < tex->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = textLineWidth(ptr, tex->font);

        ch = *ptr;
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;

            // Change of color.
            if(*ptr >= '0' && *ptr <= '9')
            {
                animatorvector3_t* color;
                uint colorIdx = *ptr - '0';

                if(!colorIdx)
                    color = (animatorvector3_t*) &tex->color; // Use the default color.
                else
                    color = &s->textColor[colorIdx-1];

                glColor4f((*color)[0].value, (*color)[1].value, (*color)[2].value, tex->color[3].value);
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
        if(tex->scale[1].value * y + tex->pos[1].value >= -tex->scale[1].value * tex->lineheight &&
           tex->scale[1].value * y + tex->pos[1].value < SCREENHEIGHT)
        {
            GL_DrawChar2(ch, (tex->textFlags & DTF_ALIGN_LEFT) ? x : x - linew / 2, y, tex->font);
            x += GL_CharWidth(ch, tex->font);
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
    float secondLen = (tex->wait ? TICRATE / tex->wait : 0);
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
    s->bgMaterial = Materials_ToMaterial(Materials_CheckNumForName(ops[0].data.cstring, MN_FLATS));
}

DEFFC(BGTexture)
{
    s->bgMaterial = Materials_ToMaterial(Materials_CheckNumForName(ops[0].data.cstring, MN_TEXTURES));
}

DEFFC(NoBGMaterial)
{
    s->bgMaterial = NULL;
}

DEFFC(InTime)
{
    s->inTime = FRACSECS_TO_TICKS(ops[0].data.flt);
}

DEFFC(Tic)
{
    s->wait = 1;
}

DEFFC(Wait)
{
    s->wait = FRACSECS_TO_TICKS(ops[0].data.flt);
}

DEFFC(WaitText)
{
    s->waitingText = stateGetObject(s, ops[0].data.cstring, FI_TEXT);
}

DEFFC(WaitAnim)
{
    s->waitingPic = stateGetObject(s, ops[0].data.cstring, FI_PIC);
}

DEFFC(Color)
{
    AnimatorVector3_Set(s->bgColor, ops[0].data.flt, ops[1].data.flt, ops[2].data.flt, s->inTime);
}

DEFFC(ColorAlpha)
{
    AnimatorVector4_Set(s->bgColor, ops[0].data.flt, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, s->inTime);
}

DEFFC(Pause)
{
    s->flags.paused = true;
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
    s->flags.eat_events = true;
}

DEFFC(NoEvents)
{
    s->flags.eat_events = false;
}

DEFFC(OnKey)
{
    int ddkey = DD_GetKeyCode(ops[0].data.cstring);
    const char* markerLabel = ops[1].data.cstring;
    fi_handler_t* h;
    // Find an empty handler.
    if((h = stateGetHandler(s, ddkey)))
    {
        h->ddkey = ddkey;
        dd_snprintf(h->marker, FI_NAME_MAX_LENGTH, "%s", markerLabel);
    }
}

DEFFC(UnsetKey)
{
    int ddkey = DD_GetKeyCode(ops[0].data.cstring);
    fi_handler_t* h;
    if((h = stateFindHandler(s, ddkey)))
    {
        h->ddkey = 0;
        memset(h->marker, 0, sizeof(h->marker));
    }
}

DEFFC(If)
{
    const char* token = ops[0].data.cstring;
    boolean val = false;

    // Built-in conditions.
    if(!stricmp(token, "netgame"))
    {
        val = netGame;
    }
    // Any hooks?
    else if(Plug_CheckForHook(HOOK_FINALE_EVAL_IF))
    {
        ddhook_finale_script_evalif_paramaters_t p;

        memset(&p, 0, sizeof(p));
        p.extraData = s->extraData;
        p.token = token;
        p.returnVal = 0;

        if(Plug_DoHook(HOOK_FINALE_EVAL_IF, 0, (void*) &p))
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
    FIC_If(cmd, ops, s);
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
    scriptSkipTo(s, ops[0].data.cstring);
}

DEFFC(Marker)
{
    // Does it match the goto string?
    if(!stricmp(s->gotoTarget, ops[0].data.cstring))
        s->gotoSkip = false;
}

DEFFC(Delete)
{
    fi_object_t* obj;
    if((obj = stateFindObject(s, ops[0].data.cstring, FI_NONE)))
    {
        objectsRemove(&s->objects, obj);
        switch(obj->type)
        {
        case FI_PIC:    P_DestroyPic((fidata_pic_t*)obj);   break;
        case FI_TEXT:   P_DestroyText((fidata_text_t*)obj); break;
        default: break;
        }
    }
}

DEFFC(Image)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    const char* name = ops[1].data.cstring;
    lumpnum_t lumpNum;

    FIData_PicClearAnimation(obj);

    if((lumpNum = W_CheckNumForName(name)) != -1)
    {
        picAddFrame(obj, createPicFrame(PFT_RAW, -1, &lumpNum, 0));
    }
    else
        Con_Message("FIC_Image: Warning, missing lump \"%s\".\n", name);
}

DEFFC(ImageAt)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    float x = ops[1].data.flt;
    float y = ops[2].data.flt;
    const char* name = ops[3].data.cstring;
    lumpnum_t lumpNum;

    AnimatorVector3_Init(obj->pos, x, y, 0);
    FIData_PicClearAnimation(obj);

    if((lumpNum = W_CheckNumForName(name)) != -1)
    {
        picAddFrame(obj, createPicFrame(PFT_RAW, -1, &lumpNum, 0));
    }
    else
        Con_Message("FIC_ImageAt: Warning, missing lump \"%s\".\n", name);
}

DEFFC(XImage)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    const char* fileName = ops[1].data.cstring;
    DGLuint tex;

    FIData_PicClearAnimation(obj);

    // Load the external resource.
    if((tex = loadGraphics(DDRC_GRAPHICS, fileName, LGM_NORMAL, false, true, 0)))
    {
        picAddFrame(obj, createPicFrame(PFT_XIMAGE, -1, &tex, 0));
    }
    else
        Con_Message("FIC_XImage: Warning, missing graphic \"%s\".\n", fileName);
}

DEFFC(Patch)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    float x = ops[1].data.flt;
    float y = ops[2].data.flt;
    const char* name = ops[3].data.cstring;
    patchid_t patch;

    AnimatorVector3_Init(obj->pos, x, y, 0);
    FIData_PicClearAnimation(obj);

    if((patch = R_PrecachePatch(name, NULL)) != 0)
    {
        picAddFrame(obj, createPicFrame(PFT_PATCH, -1, &patch, 0));
    }
    else
        Con_Message("FIC_Patch: Warning, missing Patch \"%s\".\n", name);
}

DEFFC(SetPatch)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    const char* name = ops[1].data.cstring;
    patchid_t patch;

    if((patch = R_PrecachePatch(name, NULL)) != 0)
    {
        if(!obj->numFrames)
        {
            picAddFrame(obj, createPicFrame(PFT_PATCH, -1, &patch, 0));
            return;
        }

        // Convert the first frame.
        {
        fidata_pic_frame_t* f = obj->frames[0];
        f->type = PFT_PATCH;
        f->texRef.patch = patch;
        f->tics = -1;
        f->sound = 0;
        }
    }
    else
        Con_Message("FIC_SetPatch: Warning, missing Patch \"%s\".\n", name);
}

DEFFC(ClearAnim)
{
    fi_object_t* obj;
    if((obj = stateFindObject(s, ops[0].data.cstring, FI_PIC)))
    {
        FIData_PicClearAnimation((fidata_pic_t*)obj);
    }
}

DEFFC(Anim)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    const char* name = ops[1].data.cstring;
    int tics = FRACSECS_TO_TICKS(ops[2].data.flt);
    patchid_t patch;

    if((patch = R_PrecachePatch(name, NULL)))
    {
        picAddFrame(obj, createPicFrame(PFT_PATCH, tics, &patch, 0));
    }
    else
        Con_Message("FIC_Anim: Warning, Patch \"%s\" not found.\n", name);

    obj->animComplete = false;
}

DEFFC(AnimImage)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    const char* name = ops[1].data.cstring;
    int tics = FRACSECS_TO_TICKS(ops[2].data.flt);
    lumpnum_t lumpNum;

    if((lumpNum = W_CheckNumForName(name)) != -1)
    {
        picAddFrame(obj, createPicFrame(PFT_RAW, tics, &lumpNum, 0));
        obj->animComplete = false;
    }
    else
        Con_Message("FIC_AnimImage: Warning, lump \"%s\" not found.\n", name);
}

DEFFC(Repeat)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    obj->flags.looping = true;
}

DEFFC(StateAnim)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    int stateId = Def_Get(DD_DEF_STATE, ops[1].data.cstring, 0);
    int count = ops[2].data.integer;

    // Animate N states starting from the given one.
    obj->animComplete = false;
    for(; count > 0 && stateId > 0; count--)
    {
        state_t* st = &states[stateId];
        fidata_pic_frame_t* f;
        spriteinfo_t sinf;

        R_GetSpriteInfo(st->sprite, st->frame & 0x7fff, &sinf);
        f = picAddFrame(obj, createPicFrame(PFT_MATERIAL, (st->tics <= 0? 1 : st->tics), sinf.material, 0));
        f->flags.flip = sinf.flip;

        // Go to the next state.
        stateId = st->nextState;
    }
}

DEFFC(PicSound)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);
    int sound = Def_Get(DD_DEF_SOUND, ops[1].data.cstring, 0);
    if(!obj->numFrames)
    {
        picAddFrame(obj, createPicFrame(PFT_MATERIAL, -1, 0, sound));
        return;
    }
    {fidata_pic_frame_t* f = obj->frames[obj->numFrames-1];
    f->sound = sound;
    }
}

DEFFC(ObjectOffX)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->pos[0], ops[1].data.flt, s->inTime);
}

DEFFC(ObjectOffY)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->pos[1], ops[1].data.flt, s->inTime);
}

DEFFC(ObjectOffZ)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->pos[2], ops[1].data.flt, s->inTime);
}

DEFFC(ObjectRGB)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    float rgb[3];
    if(!obj || !(obj->type == FI_TEXT || obj->type == FI_PIC))
        return;
    rgb[CR] = ops[1].data.flt;
    rgb[CG] = ops[2].data.flt;
    rgb[CB] = ops[3].data.flt;
    switch(obj->type)
    {
    case FI_TEXT:
        {
        fidata_text_t* t = (fidata_text_t*)obj;
        AnimatorVector3_Set(t->color, rgb[CR], rgb[CG], rgb[CB], s->inTime);
        break;
        }
    case FI_PIC:
        {
        fidata_pic_t* p = (fidata_pic_t*)obj;
        AnimatorVector3_Set(p->color, rgb[CR], rgb[CG], rgb[CB], s->inTime);
        // This affects all the colors.
        AnimatorVector3_Set(p->otherColor, rgb[CR], rgb[CG], rgb[CB], s->inTime);
        AnimatorVector3_Set(p->edgeColor, rgb[CR], rgb[CG], rgb[CB], s->inTime);
        AnimatorVector3_Set(p->otherEdgeColor, rgb[CR], rgb[CG], rgb[CB], s->inTime);
        break;
        }
    }
}

DEFFC(ObjectAlpha)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    float alpha;
    if(!obj || !(obj->type == FI_TEXT || obj->type == FI_PIC))
        return;
    alpha = ops[1].data.flt;
    switch(obj->type)
    {
    case FI_TEXT:
        {
        fidata_text_t* t = (fidata_text_t*)obj;
        Animator_Set(&t->color[3], alpha, s->inTime);
        break;
        }
    case FI_PIC:
        {
        fidata_pic_t* p = (fidata_pic_t*)obj;
        Animator_Set(&p->color[3], alpha, s->inTime);
        Animator_Set(&p->otherColor[3], alpha, s->inTime);
        /*Animator_Set(&p->edgeColor[3], alpha, s->inTime);
        Animator_Set(&p->otherEdgeColor[3], alpha, s->inTime); */
        break;
        }
    }
}

DEFFC(ObjectScaleX)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->scale[0], ops[1].data.flt, s->inTime);
}

DEFFC(ObjectScaleY)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->scale[1], ops[1].data.flt, s->inTime);
}

DEFFC(ObjectScaleZ)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->scale[2], ops[1].data.flt, s->inTime);
}

DEFFC(ObjectScale)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    AnimatorVector2_Set(obj->scale, ops[1].data.flt, ops[1].data.flt, s->inTime);
}

DEFFC(ObjectScaleXY)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    AnimatorVector2_Set(obj->scale, ops[1].data.flt, ops[2].data.flt, s->inTime);
}

DEFFC(ObjectScaleXYZ)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    AnimatorVector3_Set(obj->scale, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, s->inTime);
}

DEFFC(ObjectAngle)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    if(!obj)
        return;
    Animator_Set(&obj->angle, ops[1].data.flt, s->inTime);
}

DEFFC(Rect)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateGetObject(s, ops[0].data.cstring, FI_PIC);

    /**
     * We may be converting an existing Pic to a Rect, so re-init the expected
     * default state accordingly.
     *
     * danij: This seems rather error-prone to me. How about we turn them into
     * seperate object classes instead (Pic inheriting from Rect).
     */
    obj->animComplete = true;
    obj->flags.looping = false; // Yeah?

    AnimatorVector3_Init(obj->pos, ops[1].data.flt, ops[2].data.flt, 0);
    AnimatorVector3_Init(obj->scale, ops[3].data.flt, ops[4].data.flt, 1);

    // Default colors.
    AnimatorVector4_Init(obj->color, 1, 1, 1, 1);
    AnimatorVector4_Init(obj->otherColor, 1, 1, 1, 1);

    // Edge alpha is zero by default.
    AnimatorVector4_Init(obj->edgeColor, 1, 1, 1, 0);
    AnimatorVector4_Init(obj->otherEdgeColor, 1, 1, 1, 0);
}

DEFFC(FillColor)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_NONE);
    int which = 0;
    float rgba[4];

    if(!obj)
        return;

    // Which colors to modify?
    if(!stricmp(ops[1].data.cstring, "top"))
        which |= 1;
    else if(!stricmp(ops[1].data.cstring, "bottom"))
        which |= 2;
    else
        which = 3;

    {uint i;
    for(i = 0; i < 4; ++i)
        rgba[i] = ops[2+i].data.flt;
    }

    if(which & 1)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->otherColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
}

DEFFC(EdgeColor)
{
    fidata_pic_t* obj = (fidata_pic_t*) stateFindObject(s, ops[0].data.cstring, FI_PIC);
    int which = 0;
    float rgba[4];

    if(!obj)
        return;

    // Which colors to modify?
    if(!stricmp(ops[1].data.cstring, "top"))
        which |= 1;
    else if(!stricmp(ops[1].data.cstring, "bottom"))
        which |= 2;
    else
        which = 3;

    {uint i;
    for(i = 0; i < 4; ++i)
        rgba[i] = ops[2+i].data.flt;
    }

    if(which & 1)
        AnimatorVector4_Set(obj->edgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
    if(which & 2)
        AnimatorVector4_Set(obj->otherEdgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], s->inTime);
}

DEFFC(OffsetX)
{
    Animator_Set(&s->imgOffset[0], ops[0].data.flt, s->inTime);
}

DEFFC(OffsetY)
{
    Animator_Set(&s->imgOffset[1], ops[0].data.flt, s->inTime);
}

DEFFC(Sound)
{
    int num = Def_Get(DD_DEF_SOUND, ops[0].data.cstring, NULL);
    if(num > 0)
        S_LocalSound(num, NULL);
}

DEFFC(SoundAt)
{
    int num = Def_Get(DD_DEF_SOUND, ops[0].data.cstring, NULL);
    float vol = MIN_OF(ops[1].data.flt, 1);
    if(num > 0)
        S_LocalSoundAtVolume(num, NULL, vol);
}

DEFFC(SeeSound)
{
    int num = Def_Get(DD_DEF_MOBJ, ops[0].data.cstring, NULL);
    if(num < 0 || mobjInfo[num].seeSound <= 0)
        return;
    S_LocalSound(mobjInfo[num].seeSound, NULL);
}

DEFFC(DieSound)
{
    int num = Def_Get(DD_DEF_MOBJ, ops[0].data.cstring, NULL);
    if(num < 0 || mobjInfo[num].deathSound <= 0)
        return;
    S_LocalSound(mobjInfo[num].deathSound, NULL);
}

DEFFC(Music)
{
    S_StartMusic(ops[0].data.cstring, true);
}

DEFFC(MusicOnce)
{
    S_StartMusic(ops[0].data.cstring, false);
}

DEFFC(Filter)
{
    AnimatorVector4_Set(s->filter, ops[0].data.flt, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, s->inTime);
}

DEFFC(Text)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    AnimatorVector3_Init(tex->pos, ops[1].data.flt, ops[2].data.flt, 0);
    FIData_TextCopy(tex, ops[3].data.cstring);
    tex->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromDef)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    char* str;
    AnimatorVector3_Init(tex->pos, ops[1].data.flt, ops[2].data.flt, 0);
    if(!Def_Get(DD_DEF_TEXT, (char*)ops[3].data.cstring, &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(tex, str);
    tex->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromLump)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    int lnum;

    AnimatorVector3_Init(tex->pos, ops[1].data.flt, ops[2].data.flt, 0);
    lnum = W_CheckNumForName(ops[3].data.cstring);
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
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    FIData_TextCopy(tex, ops[1].data.cstring);
}

DEFFC(SetTextDef)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    char* str;
    if(!Def_Get(DD_DEF_TEXT, ops[1].data.cstring, &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(tex, str);
}

DEFFC(DeleteText)
{
    fi_object_t* obj = stateFindObject(s, ops[0].data.cstring, FI_TEXT);
    if(!obj)
        return;
    objectsRemove(&s->objects, obj);
    P_DestroyText((fidata_text_t*)obj);
}

DEFFC(PredefinedTextColor)
{
    uint idx = MINMAX_OF(1, ops[0].data.integer, 9);
    AnimatorVector3_Set(s->textColor[idx - 1], ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, s->inTime);
}

DEFFC(TextRGB)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    AnimatorVector3_Set(tex->color, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, s->inTime);
}

DEFFC(TextAlpha)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    Animator_Set(&tex->color[CA], ops[1].data.flt, s->inTime);
}

DEFFC(TextOffX)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    Animator_Set(&tex->pos[0], ops[1].data.flt, s->inTime);
}

DEFFC(TextOffY)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    Animator_Set(&tex->pos[1], ops[1].data.flt, s->inTime);
}

DEFFC(TextCenter)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->textFlags &= ~(DTF_ALIGN_LEFT|DTF_ALIGN_RIGHT);
}

DEFFC(TextNoCenter)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->textFlags |= DTF_ALIGN_LEFT;
}

DEFFC(TextScroll)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->scrollWait = ops[1].data.integer;
    tex->scrollTimer = 0;
}

DEFFC(TextPos)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->cursorPos = ops[1].data.integer;
}

DEFFC(TextRate)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->wait = ops[1].data.integer;
}

DEFFC(TextLineHeight)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->lineheight = ops[1].data.flt;
}

DEFFC(Font)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    const char* fontName = ops[1].data.cstring;
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
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->font = R_CompositeFontNumForName("a");
}

DEFFC(FontB)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    tex->font = R_CompositeFontNumForName("b");
}

DEFFC(NoMusic)
{
    // Stop the currently playing song.
    S_StopMusic();
}

DEFFC(TextScaleX)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    Animator_Set(&tex->scale[0], ops[1].data.flt, s->inTime);
}

DEFFC(TextScaleY)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    Animator_Set(&tex->scale[1], ops[1].data.flt, s->inTime);
}

DEFFC(TextScale)
{
    fidata_text_t* tex = (fidata_text_t*) stateGetObject(s, ops[0].data.cstring, FI_TEXT);
    AnimatorVector2_Set(tex->scale, ops[1].data.flt, ops[2].data.flt, s->inTime);
}

DEFFC(PlayDemo)
{
    // Mark the current state as suspended, so we know to resume it when the demo ends.
    s->flags.suspended = true;
    active = false;

    // The only argument is the demo file name.
    // Start playing the demo.
    if(!Con_Executef(CMDS_DDAY, true, "playdemo \"%s\"", ops[0].data.cstring))
    {   // Demo playback failed. Here we go again...
        demoEnds();
    }
}

DEFFC(Command)
{
    Con_Executef(CMDS_SCRIPT, false, ops[0].data.cstring);
}

DEFFC(ShowMenu)
{
    s->flags.show_menu = true;
}

DEFFC(NoShowMenu)
{
    s->flags.show_menu = false;
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

    FI_ScriptBegin(script, mode, gx.FI_GetGameState(), 0);
    return true;
}

D_CMD(StopFinale)
{
    FI_ScriptTerminate();
    return true;
}
