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
 * InFine: "Finale" script interpreter.
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

#include "finaleinterpreter.h"

// MACROS ------------------------------------------------------------------

#define MAX_TOKEN_LEN       (8192)

#define FRACSECS_TO_TICKS(sec) ((int)(sec * TICSPERSEC + 0.5))

// Helper macro for defining infine command functions.
#define DEFFC(name) void FIC_##name(const struct command_s* cmd, const fi_operand_t* ops, finaleinterpreter_t* fi)

// TYPES -------------------------------------------------------------------

typedef enum {
    FVT_INT,
    FVT_FLOAT,
    FVT_SCRIPT_STRING, // ptr points to a char*, which points to the string.
    FVT_OBJECT
} fi_operand_type_t;

typedef struct {
    fi_operand_type_t type;
    union {
        int         integer;
        float       flt;
        const char* cstring;
        fi_object_t* obj;
    } data;
} fi_operand_t;

typedef struct command_s {
    char*           token;
    const char*     operands;
    void          (*func) (const struct command_s* cmd, const fi_operand_t* ops, finaleinterpreter_t* fi);
    struct command_flags_s {
        char            when_skipping:1;
        char            when_condition_skipping:1; // Skipping because condition failed.
    } flags;
} command_t;

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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Time is measured in seconds.
// Colors are floating point and [0,1].
static command_t commands[] = {
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
    { "del",        "o", FIC_Delete }, // del (obj)
    { "x",          "of", FIC_ObjectOffX }, // x (obj) (x)
    { "y",          "of", FIC_ObjectOffY }, // y (obj) (y)
    { "z",          "of", FIC_ObjectOffZ }, // z (obj) (z)
    { "sx",         "of", FIC_ObjectScaleX }, // sx (obj) (x)
    { "sy",         "of", FIC_ObjectScaleY }, // sy (obj) (y)
    { "sz",         "of", FIC_ObjectScaleZ }, // sz (obj) (z)
    { "scale",      "of", FIC_ObjectScale }, // scale (obj) (factor)
    { "scalexy",    "off", FIC_ObjectScaleXY }, // scalexy (obj) (x) (y)
    { "scalexyz",   "offf", FIC_ObjectScaleXYZ }, // scalexyz (obj) (x) (y) (z)
    { "rgb",        "offf", FIC_ObjectRGB }, // rgb (obj) (r) (g) (b)
    { "alpha",      "of", FIC_ObjectAlpha }, // alpha (obj) (alpha)
    { "angle",      "of", FIC_ObjectAngle }, // angle (obj) (degrees)

    // Rects
    { "rect",       "sffff", FIC_Rect }, // rect (hndl) (x) (y) (w) (h)
    { "fillcolor",  "osffff", FIC_FillColor }, // fillcolor (obj) (top/bottom/both) (r) (g) (b) (a)
    { "edgecolor",  "osffff", FIC_EdgeColor }, // edgecolor (obj) (top/bottom/both) (r) (g) (b) (a)

    // Pics
    { "image",      "ss", FIC_Image }, // image (id) (raw-image-lump)
    { "imageat",    "sffs", FIC_ImageAt }, // imageat (id) (x) (y) (raw)
    { "ximage",     "ss", FIC_XImage }, // ximage (id) (ext-gfx-filename)
    { "patch",      "sffs", FIC_Patch }, // patch (id) (x) (y) (patch)
    { "set",        "ss", FIC_SetPatch }, // set (id) (lump)
    { "clranim",    "o", FIC_ClearAnim }, // clranim (obj)
    { "anim",       "ssf", FIC_Anim }, // anim (id) (patch) (time)
    { "imageanim",  "ssf", FIC_AnimImage }, // imageanim (id) (raw-img) (time)
    { "picsound",   "ss", FIC_PicSound }, // picsound (id) (sound)
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
    { "delpic",     "o", FIC_Delete }, // delpic (obj)

    // Deprecated Text commands
    { "deltext",    "o", FIC_DeleteText }, // deltext (obj)
    { "textrgb",    "sfff", FIC_TextRGB }, // textrgb (id) (r) (g) (b)
    { "textalpha",  "sf", FIC_TextAlpha }, // textalpha (id) (alpha)
    { "tx",         "sf", FIC_TextOffX }, // tx (id) (x)
    { "ty",         "sf", FIC_TextOffY }, // ty (id) (y)
    { "tsx",        "sf", FIC_TextScaleX }, // tsx (id) (x)
    { "tsy",        "sf", FIC_TextScaleY }, // tsy (id) (y)
    { "textscale",  "sf", FIC_TextScale }, // textscale (id) (x) (y)

    { NULL, 0, NULL } // Terminate.
};

static char token[MAX_TOKEN_LEN]; /// \todo token should be allocated (and owned) by the script parser!
static void* defaultState = 0;

// CODE --------------------------------------------------------------------

static void changeMode(finaleinterpreter_t* fi, finale_mode_t mode)
{
    fi->mode = mode;
}

static void setExtraData(finaleinterpreter_t* fi, const void* data)
{
    if(!data)
        return;
    if(!fi->extraData || !(FINALE_SCRIPT_EXTRADATA_SIZE > 0))
        return;
    memcpy(fi->extraData, data, FINALE_SCRIPT_EXTRADATA_SIZE);
}

static void setInitialGameState(finaleinterpreter_t* fi, int gameState, const void* extraData)
{
    fi->initialGameState = gameState;

    if(FINALE_SCRIPT_EXTRADATA_SIZE > 0)
    {
        setExtraData(fi, &defaultState);
        if(extraData)
            setExtraData(fi, extraData);
    }

    if(fi->mode == FIMODE_OVERLAY)
    {   // Overlay scripts stop when the gameMode changes.
        fi->overlayGameState = gameState;
    }
}

static command_t* findCommand(const char* name)
{
    int i;
    for(i = 0; commands[i].token; ++i)
    {
        command_t* cmd = &commands[i];
        if(!stricmp(name, cmd->token))
        {
            return cmd;
        }
    }
    return 0; // Not found.
}

static void releaseScript(finaleinterpreter_t* fi)
{
    if(fi->script)
        Z_Free(fi->script);
    fi->script = 0;
    fi->cp = 0;
}

static const char* nextToken(finaleinterpreter_t* fi)
{
    char* out;

    // Skip whitespace.
    while(*fi->cp && isspace(*fi->cp))
        fi->cp++;
    if(!*fi->cp)
        return NULL; // The end has been reached.

    out = token;
    if(*fi->cp == '"') // A string?
    {
        for(fi->cp++; *fi->cp; fi->cp++)
        {
            if(*fi->cp == '"')
            {
                fi->cp++;
                // Convert double quotes to single ones.
                if(*fi->cp != '"')
                    break;
            }

            *out++ = *fi->cp;
        }
    }
    else
    {
        while(!isspace(*fi->cp) && *fi->cp)
            *out++ = *fi->cp++;
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
static fi_operand_t* parseCommandArguments(finaleinterpreter_t* fi, const command_t* cmd, uint* count)
{
    const char* origCursorPos;
    uint numOperands;
    fi_operand_t* ops = 0;

    if(!cmd->operands || !cmd->operands[0])
        return NULL;

    origCursorPos = fi->cp;
    numOperands = (uint)strlen(cmd->operands);

    // Operands are read sequentially. This is to potentially allow for
    // command overloading at a later time.
    {uint i = 0;
    do
    {
        char typeSymbol = cmd->operands[i];
        fi_operand_t* op;
        fi_operand_type_t type;

        if(!nextToken(fi))
        {
            fi->cp = origCursorPos;
            if(ops)
                free(ops);
            if(count)
                *count = 0;
            Con_Message("parseCommandArguments: Too few operands for command '%s'.\n", cmd->token);
            return NULL;
        }

        switch(typeSymbol)
        {
        // Supported operand type symbols:
        case 'i': type = FVT_INT;           break;
        case 'f': type = FVT_FLOAT;         break;
        case 's': type = FVT_SCRIPT_STRING; break;
        case 'o': type = FVT_OBJECT;        break;
        default:
            Con_Error("parseCommandArguments: Invalid symbol '%c' in operand list for command '%s'.", typeSymbol, cmd->token);
        }

        if(!ops)
            ops = malloc(sizeof(*ops));
        else
            ops = realloc(ops, sizeof(*ops) * (i+1));
        op = &ops[i];

        op->type = type;
        switch(type)
        {
        case FVT_INT:
            op->data.integer = strtol(token, NULL, 0);
            break;

        case FVT_FLOAT:
            op->data.flt = strtod(token, NULL);
            break;
        case FVT_SCRIPT_STRING:
            {
            size_t len = strlen(token)+1;
            char* str = malloc(len);
            dd_snprintf(str, len, "%s", token);
            op->data.cstring = str;
            break;
            }
        case FVT_OBJECT:
            op->data.obj = FI_Object(FIPage_ObjectIdForName(fi->_page, token, FI_NONE));
            break;
        }
    } while(++i < numOperands);}

    fi->cp = origCursorPos;

    if(count)
        *count = numOperands;
    return ops;
}

static boolean skippingCommand(finaleinterpreter_t* fi, const command_t* cmd)
{
    if((fi->skipNext && !cmd->flags.when_condition_skipping) ||
       ((fi->skipping || fi->gotoSkip) && !cmd->flags.when_skipping))
    {
        // While not DO-skipping, the condskip has now been done.
        if(!fi->doLevel)
        {
            if(fi->skipNext)
                fi->lastSkipped = true;
            fi->skipNext = false;
        }
        return true;
    }
    return false;
}

/**
 * Execute one (the next) command, advance script cursor.
 */
static boolean executeCommand(finaleinterpreter_t* fi, const char* commandString)
{
    // Semicolon terminates DO-blocks.
    if(!strcmp(commandString, ";"))
    {
        if(fi->doLevel > 0)
        {
            if(--fi->doLevel == 0)
            {
                // The DO-skip has been completed.
                fi->skipNext = false;
                fi->lastSkipped = true;
            }
        }
        return true; // Success!
    }

    // We're now going to execute a command.
    fi->cmdExecuted = true;

    // Is this a command we know how to execute?
    {command_t* cmd;
    if((cmd = findCommand(commandString)))
    {
        boolean requiredOperands = (cmd->operands && cmd->operands[0]);
        fi_operand_t* ops = NULL;
        uint count;

        // Check that there are enough operands.
        if(!requiredOperands || (ops = parseCommandArguments(fi, cmd, &count)))
        {
            // Should we skip this command?
            if(skippingCommand(fi, cmd))
                return false;

            // Execute forthwith!
            cmd->func(cmd, ops, fi);
        }

        if(fi->gotoEnd)
        {
            fi->wait = 1;
            FI_ScriptTerminate();
            /// \note @var fi may now be invalid!
        }
        else
        {   // Now we've executed the latest command.
            fi->lastSkipped = false;
        }

        if(ops)
        {   // Destroy.
            {uint i;
            for(i = 0; i < count; ++i)
            {
                fi_operand_t* op = &ops[i];
                if(op->type == FVT_SCRIPT_STRING)
                    free((char*)op->data.cstring);
            }}
            free(ops);
        }
    }}
    return true; // Success!
}

static boolean executeNextCommand(finaleinterpreter_t* fi)
{
    const char* token;
    if((token = nextToken(fi)))
    {
        executeCommand(fi, token);
        return true;
    }
    return false;
}

/**
 * Find an @c fi_object_t of type with the type-unique name.
 * @param name              Unique name of the object we are looking for.
 * @return                  Ptr to @c fi_object_t Either:
 *                          a) Existing object associated with unique @a name.
 *                          b) New object with unique @a name.
 */
static fi_object_t* getObject(fi_page_t* p, fi_obtype_e type, const char* name)
{
    assert(name && name);
    {
    fi_objectid_t id;
    // An existing object?
    if((id = FIPage_ObjectIdForName(p, name, type)))
        return FI_Object(id);
    return FIPage_AddObject(p, FI_NewObject(type, name));
    }
}

static void demoEnds(finaleinterpreter_t* fi)
{
    if(!fi->flags.suspended)
        return;

    // Restore the InFine state.
    fi->flags.suspended = false;

    gx.FI_DemoEnds();
}

static void clearEventHandlers(finaleinterpreter_t* fi)
{
    if(fi->numEventHandlers)
        Z_Free(fi->eventHandlers);
    fi->eventHandlers = 0;
    fi->numEventHandlers = 0;
}

static fi_handler_t* findEventHandler(finaleinterpreter_t* fi, const ddevent_t* ev)
{
    uint i;
    for(i = 0; i < fi->numEventHandlers; ++i)
    {
        fi_handler_t* h = &fi->eventHandlers[i];
        if(h->ev.device != ev->device && h->ev.type != ev->type)
            continue;
        switch(h->ev.type)
        {
        case E_TOGGLE:
            if(h->ev.toggle.id != ev->toggle.id)
                continue;
            break;
        case E_AXIS:
            if(h->ev.axis.id != ev->axis.id)
                continue;
            break;
        case E_ANGLE:
            if(h->ev.angle.id != ev->angle.id)
                continue;
            break;
        default:
            Con_Error("Internal error: Invalid event template (type=%i) in finale event handler.", (int) h->ev.type);
        }
        return h;
    }
    return 0;
}

static fi_handler_t* createEventHandler(finaleinterpreter_t* fi, const ddevent_t* ev, const char* marker)
{
    // First, try to find an existing handler.
    fi_handler_t* h;
    fi->eventHandlers = Z_Realloc(fi->eventHandlers, sizeof(*h) * ++fi->numEventHandlers, PU_STATIC);
    h = &fi->eventHandlers[fi->numEventHandlers-1];
    memset(h, 0, sizeof(*h));
    memcpy(&h->ev, ev, sizeof(h->ev));
    dd_snprintf(h->marker, FI_NAME_MAX_LENGTH, "%s", marker);
    return h;
}

static void destroyEventHandler(finaleinterpreter_t* fi, fi_handler_t* h)
{
    assert(fi && h);
    {uint i;
    for(i = 0; i < fi->numEventHandlers; ++i)
    {
        fi_handler_t* other = &fi->eventHandlers[i];

        if(h != other)
            continue;

        if(i != fi->numEventHandlers-1)
            memmove(&fi->eventHandlers[i], &fi->eventHandlers[i+1], sizeof(*fi->eventHandlers) * (fi->numEventHandlers-i));

        // Resize storage?
        if(fi->numEventHandlers > 1)
        {
            fi->eventHandlers = Z_Realloc(fi->eventHandlers, sizeof(*fi->eventHandlers) * --fi->numEventHandlers, PU_STATIC);
        }
        else
        {
            Z_Free(fi->eventHandlers);
            fi->eventHandlers = NULL;
            fi->numEventHandlers = 0;
        }
        return;
    }}
}

/**
 * Find a @c fi_handler_t for the specified ddkey code.
 * @param ev                Input event to find a handler for.
 * @return                  Ptr to @c fi_handler_t object. Either:
 *                          a) Existing handler associated with unique @a code.
 *                          b) New object with unique @a code.
 */
static boolean getEventHandler(finaleinterpreter_t* fi, const ddevent_t* ev, const char* marker)
{
    // First, try to find an existing handler.
    if(findEventHandler(fi, ev))
        return true;
    // Allocate and attach another.
    createEventHandler(fi, ev, marker);
    return true;
}

static void stopScript(finaleinterpreter_t* fi)
{
#ifdef _DEBUG
    Con_Printf("Finale End: mode=%i '%.30s'\n", fi->mode, fi->script);
#endif

    fi->flags.stopped = true;

    if(isServer && fi->mode != FIMODE_LOCAL)
    {   // Tell clients to stop the finale.
        Sv_Finale(FINF_END, 0, NULL, 0);
    }

    // Any hooks?
    {ddhook_finale_script_stop_paramaters_t params;
    memset(&params, 0, sizeof(params));
    params.initialGameState = fi->initialGameState;
    params.extraData = fi->extraData;
    Plug_DoHook(HOOK_FINALE_SCRIPT_TERMINATE, fi->mode, &params);} 
}

finaleinterpreter_t* P_CreateFinaleInterpreter(void)
{
    return Z_Calloc(sizeof(finaleinterpreter_t), PU_STATIC, 0);
}

void P_DestroyFinaleInterpreter(finaleinterpreter_t* fi)
{
    assert(fi);
    stopScript(fi);
    clearEventHandlers(fi);
    releaseScript(fi);
    Z_Free(fi);
}

void FinaleInterpreter_LoadScript(finaleinterpreter_t* fi, finale_mode_t mode,
    const char* script, int gameState, const void* extraData)
{
    assert(fi && script && script[0]);
    {
    size_t size = strlen(script);

    changeMode(fi, mode);
    setInitialGameState(fi, gameState, extraData);

    fi->_page = FI_NewPage();

    // Take a copy of the script.
    fi->script = Z_Realloc(fi->script, size + 1, PU_STATIC);
    memcpy(fi->script, script, size);
    fi->script[size] = '\0';
    fi->cp = fi->script; // Init cursor.
    fi->flags.show_menu = true; // Enabled by default.
    fi->flags.suspended = false;
    fi->flags.can_skip = true; // By default skipping is enabled.
    fi->flags.paused = false;

    fi->cmdExecuted = false; // Nothing is drawn until a cmd has been executed.
    fi->skipping = false;
    fi->wait = 0; // Not waiting for anything.
    fi->inTime = 0; // Interpolation is off.
    fi->timer = 0;
    fi->gotoSkip = false;
    fi->gotoEnd = false;
    fi->skipNext = false;

    fi->waitingText = NULL;
    fi->waitingPic = NULL;
    memset(fi->gotoTarget, 0, sizeof(fi->gotoTarget));

    clearEventHandlers(fi);

    if(fi->mode != FIMODE_LOCAL)
    {
        int flags = FINF_BEGIN | (fi->mode == FIMODE_AFTER ? FINF_AFTER : fi->mode == FIMODE_OVERLAY ? FINF_OVERLAY : 0);
        ddhook_finale_script_serialize_extradata_t params;
        boolean haveExtraData = false;

        memset(&params, 0, sizeof(params));

        if(fi->extraData)
        {
            params.extraData = fi->extraData;
            params.outBuf = 0;
            params.outBufSize = 0;
            haveExtraData = Plug_DoHook(HOOK_FINALE_SCRIPT_SERIALIZE_EXTRADATA, 0, &params);
        }

        // Tell clients to start this script.
        Sv_Finale(flags, fi->script, (haveExtraData? params.outBuf : 0), (haveExtraData? params.outBufSize : 0));
    }

    // Any hooks?
    Plug_DoHook(HOOK_FINALE_SCRIPT_BEGIN, (int) fi->mode, fi->extraData);
    }
}

void FinaleInterpreter_ReleaseScript(finaleinterpreter_t* fi)
{
    assert(fi);
    if(!fi->flags.stopped)
    {
        stopScript(fi);
    }
    clearEventHandlers(fi);
    releaseScript(fi);
}

void* FinaleInterpreter_ExtraData(finaleinterpreter_t* fi)
{
    assert(fi);
    return fi->extraData;
}

boolean FinaleInterpreter_IsMenuTrigger(finaleinterpreter_t* fi)
{
    assert(fi);
    return (fi->flags.show_menu != 0);
}

boolean FinaleInterpreter_IsSuspended(finaleinterpreter_t* fi)
{
    assert(fi);
    return (fi->flags.suspended != 0);
}

void FinaleInterpreter_AllowSkip(finaleinterpreter_t* fi, boolean yes)
{
    assert(fi);
    fi->flags.can_skip = yes;
}

boolean FinaleInterpreter_CanSkip(finaleinterpreter_t* fi)
{
    assert(fi);
    return (fi->flags.can_skip != 0);
}

boolean FinaleInterpreter_CommandExecuted(finaleinterpreter_t* fi)
{
    assert(fi);
    return fi->cmdExecuted;
}

static boolean runTic(finaleinterpreter_t* fi)
{
    ddhook_finale_script_ticker_paramaters_t params;
    memset(&params, 0, sizeof(params));
    params.runTick = true;
    params.canSkip = FinaleInterpreter_CanSkip(fi);
    params.gameState = (fi->mode == FIMODE_OVERLAY? fi->overlayGameState : fi->initialGameState);
    params.extraData = fi->extraData;
    Plug_DoHook(HOOK_FINALE_SCRIPT_TICKER, fi->mode, &params);
    return params.runTick;
}

boolean FinaleInterpreter_RunTic(finaleinterpreter_t* fi)
{
    assert(fi);

    if(fi->flags.stopped || fi->flags.suspended)
        return false;

    fi->timer++;

    if(!runTic(fi))
        return false;

    // If we're waiting, don't execute any commands.
    if(fi->wait && --fi->wait)
        return false;

    // If we're paused we can't really do anything.
    if(fi->flags.paused)
        return false;

    // If we're waiting for a text to finish typing, do nothing.
    if(fi->waitingText && fi->waitingText->type == FI_TEXT)
    {
        if(!((fidata_text_t*)fi->waitingText)->animComplete)
            return false;

        fi->waitingText = NULL;
    }

    // Waiting for an animation to reach its end?
    if(fi->waitingPic && fi->waitingPic->type == FI_PIC)
    {
        if(!((fidata_pic_t*)fi->waitingPic)->animComplete)
            return false;

        fi->waitingPic = NULL;
    }

    // Execute commands until a wait time is set or we reach the end of
    // the script. If the end is reached, the finale really ends (terminates).
    {int last = 0;
    while(FI_Active() && !fi->wait && !fi->waitingText && !fi->waitingPic && !last)
        last = !executeNextCommand(fi);
    return last;}
}

boolean FinaleInterpreter_SkipToMarker(finaleinterpreter_t* fi, const char* marker)
{
    assert(fi && marker);

    if(!FI_Active() || !marker[0])
        return false;

    memset(fi->gotoTarget, 0, sizeof(fi->gotoTarget));
    strncpy(fi->gotoTarget, marker, sizeof(fi->gotoTarget) - 1);

    // Start skipping until the marker is found.
    fi->gotoSkip = true;

    // Stop any waiting.
    fi->wait = 0;
    fi->waitingText = NULL;
    fi->waitingPic = NULL;

    // Rewind the script so we can jump anywhere.
    fi->cp = fi->script;
    return true;
}

boolean FinaleInterpreter_Skip(finaleinterpreter_t* fi)
{
    assert(fi);

    fi->waitingText = NULL; // Stop waiting for things.
    fi->waitingPic = NULL;
    if(fi->flags.paused)
    {
        fi->flags.paused = false; // Un-pause.
        fi->wait = 0;
        return true;
    }

    if(fi->flags.can_skip)
    {
        fi->skipping = true; // Start skipping ahead.
        fi->wait = 0;
        return true;
    }

    return (fi->flags.eat_events != 0);
}

int FinaleInterpreter_Responder(finaleinterpreter_t* fi, ddevent_t* ev)
{
    assert(fi);

    if(isClient)
        return false;

    if(fi->flags.suspended)
        return false; // Busy playing a demo.

    // During the first ~second disallow all events/skipping.
    if(fi->timer < 20)
        return false;

    // Any handlers for this event?
    if(IS_KEY_DOWN(ev))
    {
        fi_handler_t* h;
        if((h = findEventHandler(fi, ev)))
        {
            FinaleInterpreter_SkipToMarker(fi, h->marker);
            // We'll never eat up events.
            if(IS_TOGGLE_UP(ev))
                return false;
            return (fi->flags.eat_events != 0);
        }
    }

    // If we can't skip, there'fi no interaction of any kind.
    if(!fi->flags.can_skip && !fi->flags.paused)
        return false;

    // We are only interested in key/button down presses.
    if(!IS_TOGGLE_DOWN(ev))
        return false;

    // Servers tell clients to skip.
    Sv_Finale(FINF_SKIP, 0, NULL, 0);
    return FinaleInterpreter_Skip(fi);
}

void* FI_GetClientsideDefaultState(void)
{
    return &defaultState;
}

/**
 * Set the truth conditions.
 * Used by clients after they've received a PSV_FINALE2 packet.
 */
void FI_SetClientsideDefaultState(void* data)
{
    assert(data);
    memcpy(&defaultState, data, sizeof(FINALE_SCRIPT_EXTRADATA_SIZE));
}

DEFFC(Do)
{
    // This command is called even when (cond)skipping.
    if(fi->skipNext)
    {
        // A conditional skip has been issued.
        // We'll go into DO-skipping mode. skipnext won't be cleared
        // until the matching semicolon is found.
        fi->doLevel++;
    }
}

DEFFC(End)
{
    fi->gotoEnd = true;
}

DEFFC(BGFlat)
{
    FIPage_SetBackground(fi->_page, Materials_ToMaterial(Materials_CheckNumForName(ops[0].data.cstring, MN_FLATS)));
}

DEFFC(BGTexture)
{
    FIPage_SetBackground(fi->_page, Materials_ToMaterial(Materials_CheckNumForName(ops[0].data.cstring, MN_TEXTURES)));
}

DEFFC(NoBGMaterial)
{
    FIPage_SetBackground(fi->_page, NULL);
}

DEFFC(InTime)
{
    fi->inTime = FRACSECS_TO_TICKS(ops[0].data.flt);
}

DEFFC(Tic)
{
    fi->wait = 1;
}

DEFFC(Wait)
{
    fi->wait = FRACSECS_TO_TICKS(ops[0].data.flt);
}

DEFFC(WaitText)
{
    fi->waitingText = getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
}

DEFFC(WaitAnim)
{
    fi->waitingPic = getObject(fi->_page, FI_PIC, ops[0].data.cstring);
}

DEFFC(Color)
{
    FIPage_SetBackgroundColor(fi->_page, ops[0].data.flt, ops[1].data.flt, ops[2].data.flt, fi->inTime);
}

DEFFC(ColorAlpha)
{
    FIPage_SetBackgroundColorAndAlpha(fi->_page, ops[0].data.flt, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, fi->inTime);
}

DEFFC(Pause)
{
    fi->flags.paused = true;
    fi->wait = 1;
}

DEFFC(CanSkip)
{
    fi->flags.can_skip = true;
}

DEFFC(NoSkip)
{
    fi->flags.can_skip = false;
}

DEFFC(SkipHere)
{
    fi->skipping = false;
}

DEFFC(Events)
{
    fi->flags.eat_events = true;
}

DEFFC(NoEvents)
{
    fi->flags.eat_events = false;
}

DEFFC(OnKey)
{
    ddevent_t ev;

    // Construct a template event for this handler.
    memset(&ev, 0, sizeof(ev));
    ev.device = IDEV_KEYBOARD;
    ev.type = E_TOGGLE;
    ev.toggle.id = DD_GetKeyCode(ops[0].data.cstring);
    ev.toggle.state = ETOG_DOWN;

    // First, try to find an existing handler.
    if(findEventHandler(fi, &ev))
        return;
    // Allocate and attach another.
    createEventHandler(fi, &ev, ops[1].data.cstring);
}

DEFFC(UnsetKey)
{
    ddevent_t ev;
    fi_handler_t* h;

    // Construct a template event for what we want to "unset".
    memset(&ev, 0, sizeof(ev));
    ev.device = IDEV_KEYBOARD;
    ev.type = E_TOGGLE;
    ev.toggle.id = DD_GetKeyCode(ops[0].data.cstring);
    ev.toggle.state = ETOG_DOWN;

    if((h = findEventHandler(fi, &ev)))
    {
        destroyEventHandler(fi, h);
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
        p.extraData = FI_ScriptExtraData();
        p.token = token;
        p.returnVal = 0;

        if(Plug_DoHook(HOOK_FINALE_EVAL_IF, 0, (void*) &p))
        {
            val = p.returnVal;
        }
    }
    else
    {
        Con_Message("FIC_If: Unknown condition '%s'.\n", token);
    }

    // Skip the next command if the value is false.
    fi->skipNext = !val;
}

DEFFC(IfNot)
{
    // This is the same as "if" but the skip condition is the opposite.
    FIC_If(cmd, ops, fi);
    fi->skipNext = !fi->skipNext;
}

DEFFC(Else)
{
    // The only time the ELSE condition doesn't skip is immediately
    // after a skip.
    fi->skipNext = !fi->lastSkipped;
}

DEFFC(GoTo)
{
    FinaleInterpreter_SkipToMarker(fi, ops[0].data.cstring);
}

DEFFC(Marker)
{
    // Does it match the goto string?
    if(!stricmp(fi->gotoTarget, ops[0].data.cstring))
        fi->gotoSkip = false;
}

DEFFC(Delete)
{
    if(ops[0].data.obj)
        FI_DeleteObject(ops[0].data.obj);
}

DEFFC(Image)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    const char* name = ops[1].data.cstring;
    lumpnum_t lumpNum;

    FIData_PicClearAnimation(obj);

    if((lumpNum = W_CheckNumForName(name)) != -1)
    {
        FIData_PicAppendFrame(obj, PFT_RAW, -1, &lumpNum, 0, false);
    }
    else
        Con_Message("FIC_Image: Warning, missing lump '%s'.\n", name);
}

DEFFC(ImageAt)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    float x = ops[1].data.flt;
    float y = ops[2].data.flt;
    const char* name = ops[3].data.cstring;
    lumpnum_t lumpNum;

    AnimatorVector3_Init(obj->pos, x, y, 0);
    FIData_PicClearAnimation(obj);

    if((lumpNum = W_CheckNumForName(name)) != -1)
    {
        FIData_PicAppendFrame(obj, PFT_RAW, -1, &lumpNum, 0, false);
    }
    else
        Con_Message("FIC_ImageAt: Warning, missing lump '%s'.\n", name);
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

DEFFC(XImage)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    const char* fileName = ops[1].data.cstring;
    DGLuint tex;

    FIData_PicClearAnimation(obj);

    // Load the external resource.
    if((tex = loadGraphics(DDRC_GRAPHICS, fileName, LGM_NORMAL, false, true, 0)))
    {
        FIData_PicAppendFrame(obj, PFT_XIMAGE, -1, &tex, 0, false);
    }
    else
        Con_Message("FIC_XImage: Warning, missing graphic '%s'.\n", fileName);
}

DEFFC(Patch)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    float x = ops[1].data.flt;
    float y = ops[2].data.flt;
    const char* name = ops[3].data.cstring;
    patchid_t patch;

    AnimatorVector3_Init(obj->pos, x, y, 0);
    FIData_PicClearAnimation(obj);

    if((patch = R_PrecachePatch(name, NULL)) != 0)
    {
        FIData_PicAppendFrame(obj, PFT_PATCH, -1, &patch, 0, 0);
    }
    else
        Con_Message("FIC_Patch: Warning, missing Patch '%s'.\n", name);
}

DEFFC(SetPatch)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    const char* name = ops[1].data.cstring;
    patchid_t patch;

    if((patch = R_PrecachePatch(name, NULL)) != 0)
    {
        if(!obj->numFrames)
        {
            FIData_PicAppendFrame(obj, PFT_PATCH, -1, &patch, 0, false);
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
        Con_Message("FIC_SetPatch: Warning, missing Patch '%s'.\n", name);
}

DEFFC(ClearAnim)
{
    if(ops[0].data.obj && ops[0].data.obj->type == FI_PIC)
    {
        FIData_PicClearAnimation((fidata_pic_t*)ops[0].data.obj);
    }
}

DEFFC(Anim)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    const char* name = ops[1].data.cstring;
    int tics = FRACSECS_TO_TICKS(ops[2].data.flt);
    patchid_t patch;

    if((patch = R_PrecachePatch(name, NULL)))
    {
        FIData_PicAppendFrame(obj, PFT_PATCH, tics, &patch, 0, false);
    }
    else
        Con_Message("FIC_Anim: Warning, Patch '%s' not found.\n", name);

    obj->animComplete = false;
}

DEFFC(AnimImage)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    const char* name = ops[1].data.cstring;
    int tics = FRACSECS_TO_TICKS(ops[2].data.flt);
    lumpnum_t lumpNum;

    if((lumpNum = W_CheckNumForName(name)) != -1)
    {
        FIData_PicAppendFrame(obj, PFT_RAW, tics, &lumpNum, 0, false);
        obj->animComplete = false;
    }
    else
        Con_Message("FIC_AnimImage: Warning, lump '%s' not found.\n", name);
}

DEFFC(Repeat)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    obj->flags.looping = true;
}

DEFFC(StateAnim)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    int stateId = Def_Get(DD_DEF_STATE, ops[1].data.cstring, 0);
    int count = ops[2].data.integer;

    // Animate N states starting from the given one.
    obj->animComplete = false;
    for(; count > 0 && stateId > 0; count--)
    {
        state_t* st = &states[stateId];
        spriteinfo_t sinf;

        R_GetSpriteInfo(st->sprite, st->frame & 0x7fff, &sinf);
        FIData_PicAppendFrame(obj, PFT_MATERIAL, (st->tics <= 0? 1 : st->tics), sinf.material, 0, sinf.flip);

        // Go to the next state.
        stateId = st->nextState;
    }
}

DEFFC(PicSound)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);
    int sound = Def_Get(DD_DEF_SOUND, ops[1].data.cstring, 0);
    if(!obj->numFrames)
    {
        FIData_PicAppendFrame(obj, PFT_MATERIAL, -1, 0, sound, false);
        return;
    }
    {fidata_pic_frame_t* f = obj->frames[obj->numFrames-1];
    f->sound = sound;
    }
}

DEFFC(ObjectOffX)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->pos[0], ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectOffY)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->pos[1], ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectOffZ)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->pos[2], ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectRGB)
{
    fi_object_t* obj = ops[0].data.obj;
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
        AnimatorVector3_Set(t->color, rgb[CR], rgb[CG], rgb[CB], fi->inTime);
        break;
        }
    case FI_PIC:
        {
        fidata_pic_t* p = (fidata_pic_t*)obj;
        AnimatorVector3_Set(p->color, rgb[CR], rgb[CG], rgb[CB], fi->inTime);
        // This affects all the colors.
        AnimatorVector3_Set(p->otherColor, rgb[CR], rgb[CG], rgb[CB], fi->inTime);
        AnimatorVector3_Set(p->edgeColor, rgb[CR], rgb[CG], rgb[CB], fi->inTime);
        AnimatorVector3_Set(p->otherEdgeColor, rgb[CR], rgb[CG], rgb[CB], fi->inTime);
        break;
        }
    }
}

DEFFC(ObjectAlpha)
{
    fi_object_t* obj = ops[0].data.obj;
    float alpha;
    if(!obj || !(obj->type == FI_TEXT || obj->type == FI_PIC))
        return;
    alpha = ops[1].data.flt;
    switch(obj->type)
    {
    case FI_TEXT:
        {
        fidata_text_t* t = (fidata_text_t*)obj;
        Animator_Set(&t->color[3], alpha, fi->inTime);
        break;
        }
    case FI_PIC:
        {
        fidata_pic_t* p = (fidata_pic_t*)obj;
        Animator_Set(&p->color[3], alpha, fi->inTime);
        Animator_Set(&p->otherColor[3], alpha, fi->inTime);
        /*Animator_Set(&p->edgeColor[3], alpha, fi->inTime);
        Animator_Set(&p->otherEdgeColor[3], alpha, fi->inTime); */
        break;
        }
    }
}

DEFFC(ObjectScaleX)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->scale[0], ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectScaleY)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->scale[1], ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectScaleZ)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->scale[2], ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectScale)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        AnimatorVector2_Set(obj->scale, ops[1].data.flt, ops[1].data.flt, fi->inTime);
    }
}

DEFFC(ObjectScaleXY)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        AnimatorVector2_Set(obj->scale, ops[1].data.flt, ops[2].data.flt, fi->inTime);
    }
}

DEFFC(ObjectScaleXYZ)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        AnimatorVector3_Set(obj->scale, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, fi->inTime);
    }
}

DEFFC(ObjectAngle)
{
    if(ops[0].data.obj)
    {
        fi_object_t* obj = ops[0].data.obj;
        Animator_Set(&obj->angle, ops[1].data.flt, fi->inTime);
    }
}

DEFFC(Rect)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi->_page, FI_PIC, ops[0].data.cstring);

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
    fi_object_t* obj = ops[0].data.obj;
    int which = 0;
    float rgba[4];

    if(!obj || obj->type != FI_PIC)
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
        AnimatorVector4_Set(((fidata_pic_t*)obj)->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->inTime);
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->otherColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->inTime);
}

DEFFC(EdgeColor)
{
    fi_object_t* obj = ops[0].data.obj;
    int which = 0;
    float rgba[4];

    if(!obj || obj->type != FI_PIC)
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
        AnimatorVector4_Set(((fidata_pic_t*)obj)->edgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->inTime);
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->otherEdgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->inTime);
}

DEFFC(OffsetX)
{
    FIPage_SetImageOffsetX(fi->_page, ops[0].data.flt, fi->inTime);
}

DEFFC(OffsetY)
{
    FIPage_SetImageOffsetY(fi->_page, ops[0].data.flt, fi->inTime);
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
    FIPage_SetFilterColorAndAlpha(fi->_page, ops[0].data.flt, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, fi->inTime);
}

DEFFC(Text)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    AnimatorVector3_Init(tex->pos, ops[1].data.flt, ops[2].data.flt, 0);
    FIData_TextCopy(tex, ops[3].data.cstring);
    tex->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromDef)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    char* str;
    AnimatorVector3_Init(tex->pos, ops[1].data.flt, ops[2].data.flt, 0);
    if(!Def_Get(DD_DEF_TEXT, (char*)ops[3].data.cstring, &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(tex, str);
    tex->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromLump)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
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
        buflen = 2 * incount + 1;
        str = calloc(1, buflen);
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
        free(str);
    }
    tex->cursorPos = 0; // Restart.
}

DEFFC(SetText)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    FIData_TextCopy(tex, ops[1].data.cstring);
}

DEFFC(SetTextDef)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    char* str;
    if(!Def_Get(DD_DEF_TEXT, ops[1].data.cstring, &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(tex, str);
}

DEFFC(DeleteText)
{
    if(ops[0].data.obj)
        FI_DeleteObject(ops[0].data.obj);
}

DEFFC(PredefinedTextColor)
{
    FIPage_SetPredefinedColor(fi->_page, MINMAX_OF(1, ops[0].data.integer, 9)-1, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, fi->inTime);
}

DEFFC(TextRGB)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    AnimatorVector3_Set(tex->color, ops[1].data.flt, ops[2].data.flt, ops[3].data.flt, fi->inTime);
}

DEFFC(TextAlpha)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    Animator_Set(&tex->color[CA], ops[1].data.flt, fi->inTime);
}

DEFFC(TextOffX)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    Animator_Set(&tex->pos[0], ops[1].data.flt, fi->inTime);
}

DEFFC(TextOffY)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    Animator_Set(&tex->pos[1], ops[1].data.flt, fi->inTime);
}

DEFFC(TextCenter)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->textFlags &= ~(DTF_ALIGN_LEFT|DTF_ALIGN_RIGHT);
}

DEFFC(TextNoCenter)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->textFlags |= DTF_ALIGN_LEFT;
}

DEFFC(TextScroll)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->scrollWait = ops[1].data.integer;
    tex->scrollTimer = 0;
}

DEFFC(TextPos)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->cursorPos = ops[1].data.integer;
}

DEFFC(TextRate)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->wait = ops[1].data.integer;
}

DEFFC(TextLineHeight)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->lineheight = ops[1].data.flt;
}

DEFFC(Font)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
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
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->font = R_CompositeFontNumForName("a");
}

DEFFC(FontB)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    tex->font = R_CompositeFontNumForName("b");
}

DEFFC(NoMusic)
{
    // Stop the currently playing song.
    S_StopMusic();
}

DEFFC(TextScaleX)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    Animator_Set(&tex->scale[0], ops[1].data.flt, fi->inTime);
}

DEFFC(TextScaleY)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    Animator_Set(&tex->scale[1], ops[1].data.flt, fi->inTime);
}

DEFFC(TextScale)
{
    fidata_text_t* tex = (fidata_text_t*) getObject(fi->_page, FI_TEXT, ops[0].data.cstring);
    AnimatorVector2_Set(tex->scale, ops[1].data.flt, ops[2].data.flt, fi->inTime);
}

DEFFC(PlayDemo)
{
    // Mark the current state as suspended, so we know to resume it when the demo ends.
    fi->flags.suspended = true;

    // The only argument is the demo file name.
    // Start playing the demo.
    if(!Con_Executef(CMDS_DDAY, true, "playdemo \"%s\"", ops[0].data.cstring))
    {   // Demo playback failed. Here we go again...
        demoEnds(fi);
    }
}

DEFFC(Command)
{
    Con_Executef(CMDS_SCRIPT, false, ops[0].data.cstring);
}

DEFFC(ShowMenu)
{
    fi->flags.show_menu = true;
}

DEFFC(NoShowMenu)
{
    fi->flags.show_menu = false;
}
