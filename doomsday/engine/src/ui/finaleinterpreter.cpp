/**
 * @file finaleinterpreter.c
 * InFine "Finale" script interpreter.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <de/memory.h>
#include <de/memoryzone.h>

#include "de_console.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_infine.h"
#include "de_misc.h"
#include "de_infine.h"
#include "de_ui.h"
#include "de_filesys.h"
#include "de_resource.h"
#include "dd_main.h"
#include "game.h"

#include "api_material.h"
#include "api_render.h"

#include "audio/s_main.h"
#include "network/net_main.h"
#include "client/cl_infine.h"
#include "server/sv_infine.h"
#include "gl/sys_opengl.h" // TODO: get rid of this

#define FRACSECS_TO_TICKS(sec) ((int)(sec * TICSPERSEC + 0.5))

// Helper macro for defining infine command functions.
#define DEFFC(name) void FIC_##name(const struct command_s* cmd, const fi_operand_t* ops, finaleinterpreter_t* fi)

// Helper macro for accessing the value of an operand.
#define OP_INT(n)           (ops[n].data.integer)
#define OP_FLOAT(n)         (ops[n].data.flt)
#define OP_CSTRING(n)       (ops[n].data.cstring)
#define OP_OBJECT(n)        (ops[n].data.obj)
#define OP_URI(n)           (ops[n].data.uri)

typedef enum {
    FVT_INT,
    FVT_FLOAT,
    FVT_SCRIPT_STRING,
    FVT_OBJECT,
    FVT_URI
} fi_operand_type_t;

typedef struct {
    fi_operand_type_t type;
    union {
        int         integer;
        float       flt;
        const char* cstring;
        fi_object_t* obj;
        Uri*        uri;
    } data;
} fi_operand_t;

typedef struct command_s {
    char* token;
    const char* operands;

    void (*func) (const struct command_s* cmd, const fi_operand_t* ops, finaleinterpreter_t* fi);

    struct command_flags_s {
        char when_skipping:1;
        char when_condition_skipping:1; // Skipping because condition failed.
    } flags;

    /// Command execution directives NOT supported by this command.
    /// @see finaleInterpreterCommandDirective
    int excludeDirectives;
} command_t;

typedef struct fi_namespace_record_s {
    fi_objectname_t name; // Unique among objects of the same type and spawned by the same script.
    fi_objectid_t   objectId;
} fi_namespace_record_t;

// Command functions.
DEFFC(Do);
DEFFC(End);
DEFFC(BGMaterial);
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
DEFFC(PredefinedColor);
DEFFC(PredefinedFont);
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

static fi_objectid_t toObjectId(fi_namespace_t* names, const char* name, fi_obtype_e type);

/**
 * Time is measured in seconds.
 * Colors are floating point and [0..1].
 *
 * @todo This data should be pre-processed (i.e., parsed) during module init
 *       and any symbolic references or other indirections resolved once
 *       rather than repeatedly during script interpretation.
 *
 *       At this time the command names could also be hashed and chained to
 *       improve performance. -ds
 */
static const command_t commands[] = {
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
    { "flat",       "u(flats:)", FIC_BGMaterial }, // flat (flat-id)
    { "texture",    "u(textures:)", FIC_BGMaterial }, // texture (texture-id)
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
    { "font",       "su", FIC_Font }, // font (id) (font)
    { "linehgt",    "sf", FIC_TextLineHeight }, // linehgt (hndl) (hgt)

    // Game Control
    { "playdemo",   "s", FIC_PlayDemo }, // playdemo (filename)
    { "cmd",        "s", FIC_Command }, // cmd (console command)
    { "trigger",    "", FIC_ShowMenu },
    { "notrigger",  "", FIC_NoShowMenu },

    // Misc.
    { "precolor",   "ifff", FIC_PredefinedColor }, // precolor (num) (r) (g) (b)
    { "prefont",    "iu", FIC_PredefinedFont }, // prefont (num) (font)

    // Deprecated Font commands
    { "fonta",      "s", FIC_FontA }, // fonta (id)
    { "fontb",      "s", FIC_FontB }, // fontb (id)

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

static __inline fi_operand_type_t operandTypeForCharCode(char code)
{
    switch(code)
    {
    case 'i': return FVT_INT;
    case 'f': return FVT_FLOAT;
    case 's': return FVT_SCRIPT_STRING;
    case 'o': return FVT_OBJECT;
    case 'u': return FVT_URI;
    default:
        Con_Error("Error: operandTypeForCharCode: Unknown char-code %c", code);
        exit(1); // Unreachable.
    }
}

static fi_objectid_t findIdForName(fi_namespace_t* names, const char* name)
{
    fi_objectid_t id;
    // First check all pics.
    id = toObjectId(names, name, FI_PIC);
    // Then check text objects.
    if(!id)
        id = toObjectId(names, name, FI_TEXT);
    return id;
}

static fi_objectid_t toObjectId(fi_namespace_t* names, const char* name, fi_obtype_e type)
{
    assert(name && name[0]);
    if(type == FI_NONE)
    {   // Use a priority-based search.
        return findIdForName(names, name);
    }

    {uint i;
    for(i = 0; i < names->num; ++i)
    {
        fi_namespace_record_t* rec = &names->vector[i];
        if(!stricmp(rec->name, name) && FI_Object(rec->objectId)->type == type)
            return rec->objectId;
    }}
    return 0;
}

static void destroyObjectsInScope(fi_namespace_t* names)
{
    // Delete external images, text strings etc.
    if(names->vector)
    {
        uint i;
        for(i = 0; i < names->num; ++i)
        {
            fi_namespace_record_t* rec = &names->vector[i];
            FI_DeleteObject(FI_Object(rec->objectId));
        }
        Z_Free(names->vector);
    }
    names->vector = NULL;
    names->num = 0;
}

static uint objectIndexInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    if(obj)
    {
        uint i;
        for(i = 0; i < names->num; ++i)
        {
            fi_namespace_record_t* rec = &names->vector[i];
            if(rec->objectId == obj->id)
                return i+1;
        }
    }
    return 0;
}

static __inline boolean objectInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    return objectIndexInNamespace(names, obj) != 0;
}

/**
 * @note Does not check if the object already exists in this scope.
 */
static fi_object_t* addObjectToNamespace(fi_namespace_t* names, const char* name, fi_object_t* obj)
{
    fi_namespace_record_t* rec;
    names->vector = Z_Realloc(names->vector, sizeof(*names->vector) * ++names->num, PU_APPSTATIC);
    rec = &names->vector[names->num-1];

    rec->objectId = obj->id;
    memset(rec->name, 0, sizeof(rec->name));
    dd_snprintf(rec->name, FI_NAME_MAX_LENGTH, "%s", name);

    return obj;
}

/**
 * @pre There is at most one reference to the object in this scope.
 */
static fi_object_t* removeObjectInNamespace(fi_namespace_t* names, fi_object_t* obj)
{
    uint idx;
    if((idx = objectIndexInNamespace(names, obj)))
    {
        idx -= 1; // Indices are 1-based.

        if(idx != names->num-1)
            memmove(&names->vector[idx], &names->vector[idx+1], sizeof(*names->vector) * (names->num-idx));

        if(names->num > 1)
        {
            names->vector = Z_Realloc(names->vector, sizeof(*names->vector) * --names->num, PU_APPSTATIC);
        }
        else
        {
            Z_Free(names->vector);
            names->vector = NULL;
            names->num = 0;
        }
    }
    return obj;
}

static fi_objectid_t findObjectIdForName(fi_namespace_t* names, const char* name, fi_obtype_e type)
{
    if(!name || !name[0])
        return 0;
    return toObjectId(names, name, type);
}

static const command_t* findCommand(const char* name)
{
    size_t i;
    for(i = 0; commands[i].token; ++i)
    {
        const command_t* cmd = &commands[i];
        if(!stricmp(cmd->token, name))
            return cmd;
    }
    return 0; // Not found.
}

static void releaseScript(finaleinterpreter_t* fi)
{
    if(fi->_script)
        Z_Free(fi->_script);
    fi->_script = 0;
    fi->_scriptBegin = 0;
    fi->_cp = 0;
}

static const char* nextToken(finaleinterpreter_t* fi)
{
    char* out;

    // Skip whitespace.
    while(*fi->_cp && isspace(*fi->_cp))
        fi->_cp++;
    if(!*fi->_cp)
        return NULL; // The end has been reached.

    out = fi->_token;
    if(*fi->_cp == '"') // A string?
    {
        for(fi->_cp++; *fi->_cp; fi->_cp++)
        {
            if(*fi->_cp == '"')
            {
                fi->_cp++;
                // Convert double quotes to single ones.
                if(*fi->_cp != '"')
                    break;
            }
            *out++ = *fi->_cp;
        }
    }
    else
    {
        while(!isspace(*fi->_cp) && *fi->_cp)
            *out++ = *fi->_cp++;
    }
    *out++ = 0;

    return fi->_token;
}

static __inline const char* findDefaultValueEnd(const char* str)
{
    const char* defaultValueEnd;
    for(defaultValueEnd = str; defaultValueEnd && *defaultValueEnd != ')'; defaultValueEnd++)
    {}
    DENG_ASSERT(defaultValueEnd < str + strlen(str));
    return defaultValueEnd;
}

static const char* nextOperand(const char* operands)
{
    if(operands && operands[0])
    {
        // Some operands might include a default value.
        int len = strlen(operands);
        if(len > 1 && operands[1] == '(')
        {
            // A default value begins. Find the end.
            return findDefaultValueEnd(operands + 2) + 1;
        }
        return operands + 1;
    }
    return NULL; // No more operands.
}

/// @return Total number of command operands in the control string @a operands.
static int countCommandOperands(const char* operands)
{
    int count = 0;
    while(operands && operands[0])
    {
        count += 1;
        operands = nextOperand(operands);
    }
    return count;
}

/**
 * Prepare the command operands from the script. If successfull, a ptr to a new
 * vector of @c fi_operand_t objects is returned. Ownership of the vector is
 * given to the caller.
 *
 * @return  A new array of @c fi_operand_t else @c NULL
 */
static fi_operand_t* prepareCommandOperands(finaleinterpreter_t* fi, const command_t* cmd,
    int* count)
{
    const char* origCursorPos = fi->_cp;
    int operandCount = 0;
    fi_operand_t* operands = 0, *op;
    const char* opRover;

    DENG_ASSERT(fi && cmd);

    operandCount = countCommandOperands(cmd->operands);
    if(operandCount <= 0) return NULL;

    operands = M_Malloc(sizeof(*operands) * operandCount);
    opRover = cmd->operands;
    for(op = operands; opRover && opRover[0]; opRover = nextOperand(opRover), op++)
    {
        const char charCode = *opRover;
        boolean opHasDefaultValue, haveValue;

        op->type = operandTypeForCharCode(charCode);
        opHasDefaultValue = (opRover < cmd->operands + (strlen(cmd->operands) - 2) && opRover[1] == '(');

        haveValue = !!nextToken(fi);
        if(!haveValue && !opHasDefaultValue)
        {
            fi->_cp = origCursorPos;

            if(operands) free(operands);
            if(count) *count = 0;

            Con_Error("prepareCommandOperands: Too few operands for command '%s'.\n", cmd->token);
            return 0; // Unreachable.
        }

        switch(op->type)
        {
        case FVT_INT: {
            const char* valueStr = haveValue? fi->_token : 0;
            if(!valueStr)
            {
                // Use the default.
                const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                AutoStr* defaultValue = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                valueStr = Str_Text(defaultValue);
            }
            op->data.integer = strtol(valueStr, NULL, 0);
            break; }

        case FVT_FLOAT: {
            const char* valueStr = haveValue? fi->_token : 0;
            if(!valueStr)
            {
                // Use the default.
                const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                AutoStr* defaultValue = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                valueStr = Str_Text(defaultValue);
            }
            op->data.flt = strtod(valueStr, NULL);
            break; }

        case FVT_SCRIPT_STRING: {
            const char* valueStr = haveValue? fi->_token : 0;
            int valueLen         = haveValue? strlen(fi->_token) : 0;
            if(!valueStr)
            {
                // Use the default.
                const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                AutoStr* defaultValue = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                valueStr = Str_Text(defaultValue);
                valueLen = defaultValueLen;
            }
            op->data.cstring = (char*)malloc(valueLen+1);
            strcpy((char*)op->data.cstring, fi->_token);
            break; }

        case FVT_OBJECT: {
            const char* obName = haveValue? fi->_token : 0;
            if(!obName)
            {
                // Use the default.
                const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                AutoStr* defaultValue = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                obName = Str_Text(defaultValue);
            }
            op->data.obj = FI_Object(findObjectIdForName(&fi->_namespace, obName, FI_NONE));
            break; }

        case FVT_URI: {
            Uri* uri = Uri_New();
            // Always apply the default as it may contain a default scheme.
            if(opHasDefaultValue)
            {
                const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                AutoStr* defaultValue = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                Uri_SetUri2(uri, Str_Text(defaultValue), RC_NULL);
            }
            if(haveValue)
            {
                Uri_SetUri2(uri, fi->_token, RC_NULL);
            }
            op->data.uri = uri;
            break; }

        default: break; // Unreachable.
        }
    }

    if(count) *count = operandCount;

    return operands;
}

static boolean skippingCommand(finaleinterpreter_t* fi, const command_t* cmd)
{
    if((fi->_skipNext && !cmd->flags.when_condition_skipping) ||
       ((fi->_skipping || fi->_gotoSkip) && !cmd->flags.when_skipping))
    {
        // While not DO-skipping, the condskip has now been done.
        if(!fi->_doLevel)
        {
            if(fi->_skipNext)
                fi->_lastSkipped = true;
            fi->_skipNext = false;
        }
        return true;
    }
    return false;
}

/**
 * Execute one (the next) command, advance script cursor.
 */
static boolean executeCommand(finaleinterpreter_t* fi, const char* commandString,
    int directive)
{
    boolean didSkip = false;

    // Semicolon terminates DO-blocks.
    if(!strcmp(commandString, ";"))
    {
        if(fi->_doLevel > 0)
        {
            if(--fi->_doLevel == 0)
            {
                // The DO-skip has been completed.
                fi->_skipNext = false;
                fi->_lastSkipped = true;
            }
        }
        return true; // Success!
    }

    // We're now going to execute a command.
    fi->_cmdExecuted = true;

    // Is this a command we know how to execute?
    {const command_t* cmd;
    if((cmd = findCommand(commandString)))
    {
        boolean requiredOperands;
        fi_operand_t* ops = NULL;
        int numOps = 0;

        // Is this command supported for this directive?
        if(directive != 0 && cmd->excludeDirectives != 0 &&
           (cmd->excludeDirectives & directive) == 0)
            Con_Error("executeCommand: Command \"%s\" is not supported for directive %i.",
                      cmd->token, directive);

        // Check that there are enough operands.
        requiredOperands = (cmd->operands && cmd->operands[0]);
        /// @todo Dynamic memory allocation during script interpretation should be avoided.
        if(0 == requiredOperands || (ops = prepareCommandOperands(fi, cmd, &numOps)))
        {
            // Should we skip this command?
            if(!(didSkip = skippingCommand(fi, cmd)))
            {
                // Execute forthwith!
                cmd->func(cmd, ops, fi);
            }
        }

        if(!didSkip)
        {
            if(fi->_gotoEnd)
            {
                fi->_wait = 1;
            }
            else
            {   // Now we've executed the latest command.
                fi->_lastSkipped = false;
            }
        }

        if(ops)
        {
            int i;
            for(i = 0; i < numOps; ++i)
            {
                fi_operand_t* op = &ops[i];
                switch(op->type)
                {
                case FVT_SCRIPT_STRING:
                    free((char*)op->data.cstring);
                    break;
                case FVT_URI:
                    Uri_Delete(op->data.uri);
                    break;
                default: break;
                }
            }
            free(ops);
        }
    }}
    return !didSkip;
}

static boolean executeNextCommand(finaleinterpreter_t* fi)
{
    const char* token;
    if((token = nextToken(fi)))
    {
        executeCommand(fi, token, FID_NORMAL);
        // Time to unhide the object page(s)?
        if(fi->_cmdExecuted)
        {
            FIPage_MakeVisible(fi->_pages[PAGE_PICS], true);
            FIPage_MakeVisible(fi->_pages[PAGE_TEXT], true);
        }
        return true;
    }
    return false;
}

static __inline uint pageForObjectType(fi_obtype_e type)
{
    return (type == FI_TEXT? PAGE_TEXT : PAGE_PICS);
}

/**
 * Find an object of the specified type with the type-unique name.
 * @param name  Unique name of the object we are looking for.

 * @return  Ptr to @c fi_object_t Either:
 *          a) Existing object associated with unique @a name.
 *          b) New object with unique @a name.
 */
static fi_object_t* getObject(finaleinterpreter_t* fi, fi_obtype_e type, const char* name)
{
    fi_objectid_t id;
    fi_object_t* obj;
    uint pageIdx;
    assert(name && name);

    // An existing object?
    if((id = findObjectIdForName(&fi->_namespace, name, type)))
    {
        return FI_Object(id);
    }

    // A new object.
    obj = FI_NewObject(type, name);
    pageIdx = pageForObjectType(type);
    switch(type)
    {
    case FI_TEXT:
        FIData_TextSetFont(obj, FIPage_PredefinedFont(fi->_pages[pageIdx], 0));
        FIData_TextSetPreColor(obj, 1);
        break;
    default:
        // No additional pre-configuration.
        break;
    }
    return FIPage_AddObject(fi->_pages[pageIdx], addObjectToNamespace(&fi->_namespace, name, obj));
}

static void clearEventHandlers(finaleinterpreter_t* fi)
{
    if(fi->_numEventHandlers)
        Z_Free(fi->_eventHandlers);
    fi->_eventHandlers = 0;
    fi->_numEventHandlers = 0;
}

static fi_handler_t* findEventHandler(finaleinterpreter_t* fi, const ddevent_t* ev)
{
    uint i;
    for(i = 0; i < fi->_numEventHandlers; ++i)
    {
        fi_handler_t* h = &fi->_eventHandlers[i];
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
    fi_handler_t* h;
    fi->_eventHandlers = Z_Realloc(fi->_eventHandlers, sizeof(*h) * ++fi->_numEventHandlers, PU_APPSTATIC);
    h = &fi->_eventHandlers[fi->_numEventHandlers-1];
    memset(h, 0, sizeof(*h));
    memcpy(&h->ev, ev, sizeof(h->ev));
    dd_snprintf(h->marker, FI_NAME_MAX_LENGTH, "%s", marker);
    return h;
}

static void destroyEventHandler(finaleinterpreter_t* fi, fi_handler_t* h)
{
    assert(fi && h);
    {uint i;
    for(i = 0; i < fi->_numEventHandlers; ++i)
    {
        fi_handler_t* other = &fi->_eventHandlers[i];

        if(h != other)
            continue;

        if(i != fi->_numEventHandlers-1)
            memmove(&fi->_eventHandlers[i], &fi->_eventHandlers[i+1], sizeof(*fi->_eventHandlers) * (fi->_numEventHandlers-i));

        // Resize storage?
        if(fi->_numEventHandlers > 1)
        {
            fi->_eventHandlers = Z_Realloc(fi->_eventHandlers, sizeof(*fi->_eventHandlers) * --fi->_numEventHandlers, PU_APPSTATIC);
        }
        else
        {
            Z_Free(fi->_eventHandlers);
            fi->_eventHandlers = NULL;
            fi->_numEventHandlers = 0;
        }
        return;
    }}
}

/**
 * Find a @c fi_handler_t for the specified ddkey code.
 * @param ev  Input event to find a handler for.
 * @return  Ptr to @c fi_handler_t object. Either:
 *          a) Existing handler associated with unique @a code.
 *          b) New object with unique @a code.
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
    Con_Printf("Finale End - id:%i '%.30s'\n", fi->_id, fi->_scriptBegin);
#endif
    fi->flags.stopped = true;
    if(isServer && !(FI_ScriptFlags(fi->_id) & FF_LOCAL))
    {   // Tell clients to stop the finale.
        Sv_Finale(fi->_id, FINF_END, 0);
    }
    // Any hooks?
    DD_CallHooks(HOOK_FINALE_SCRIPT_STOP, fi->_id, 0);
}

static void changePageBackground(fi_page_t* p, material_t* mat)
{
    // If the page does not yet have a background set we must setup the color+alpha.
    if(mat && !FIPage_BackgroundMaterial(p))
    {
        FIPage_SetBackgroundTopColorAndAlpha(p, 1, 1, 1, 1, 0);
        FIPage_SetBackgroundBottomColorAndAlpha(p, 1, 1, 1, 1, 0);
    }
    FIPage_SetBackgroundMaterial(p, mat);
}

finaleinterpreter_t* P_CreateFinaleInterpreter(void)
{
    return Z_Calloc(sizeof(finaleinterpreter_t), PU_APPSTATIC, 0);
}

void P_DestroyFinaleInterpreter(finaleinterpreter_t* fi)
{
    assert(fi);
    stopScript(fi);
    clearEventHandlers(fi);
    releaseScript(fi);
    destroyObjectsInScope(&fi->_namespace);
    FI_DeletePage(fi->_pages[PAGE_PICS]);
    FI_DeletePage(fi->_pages[PAGE_TEXT]);
    Z_Free(fi);
}

static __inline void findBegin(finaleinterpreter_t* fi)
{
    const char* token;
    while(!fi->_gotoEnd && 0 != (token = nextToken(fi)) && stricmp(token, "{")) {}
}

static __inline void findEnd(finaleinterpreter_t* fi)
{
    const char* token;
    while(!fi->_gotoEnd && 0 != (token = nextToken(fi)) && stricmp(token, "}")) {}
}

static void initDefaultState(finaleinterpreter_t* fi)
{
    assert(fi);

    fi->flags.suspended = false;
    fi->flags.paused = false;
    fi->flags.show_menu = true; // Unhandled events will show a menu.
    fi->flags.can_skip = true; // By default skipping is enabled.

    fi->_cmdExecuted = false; // Nothing is drawn until a cmd has been executed.
    fi->_skipping = false;
    fi->_wait = 0; // Not waiting for anything.
    fi->_inTime = 0; // Interpolation is off.
    fi->_timer = 0;
    fi->_gotoSkip = false;
    fi->_gotoEnd = false;
    fi->_skipNext = false;

    fi->_waitingText = 0;
    fi->_waitingPic = 0;
    memset(fi->_gotoTarget, 0, sizeof(fi->_gotoTarget));

    clearEventHandlers(fi);
}

void FinaleInterpreter_LoadScript(finaleinterpreter_t* fi, const char* script)
{
    assert(fi && script && script[0]);
    {
    size_t size = strlen(script);

    /**
     * InFine imposes a strict object drawing order:
     *
     * 1: Background flat (or a single-color background).
     * 2: Picture objects (globally offseted with OffX and OffY), in the order in which they were created.
     * 3: Text objects, in the order in which they were created.
     * 4: Filter.
     *
     * For this we'll need two pages; one for it's background and for Pics and another for Text and it's filter.
     */
    fi->_pages[PAGE_PICS] = FI_NewPage(0);
    fi->_pages[PAGE_TEXT] = FI_NewPage(0);

    // Hide our pages until command interpretation begins.
    FIPage_MakeVisible(fi->_pages[PAGE_PICS], false);
    FIPage_MakeVisible(fi->_pages[PAGE_TEXT], false);

    // Take a copy of the script.
    fi->_script = Z_Realloc(fi->_script, size + 1, PU_APPSTATIC);
    memcpy(fi->_script, script, size);
    fi->_script[size] = '\0';
    fi->_scriptBegin = fi->_script;
    fi->_cp = fi->_script; // Init cursor.

    initDefaultState(fi);

    // Locate the start of the script.
    if(0 != nextToken(fi))
    {
        // The start of the script may include blocks of event directive
        // commands. These commands are automatically executed in response
        // to their associated events.
        if(!stricmp(fi->_token, "OnLoad"))
        {
            findBegin(fi);
            for(;;)
            {
                nextToken(fi);
                if(!stricmp(fi->_token, "}"))
                    goto end_read;
                if(!executeCommand(fi, fi->_token, FID_ONLOAD))
                    Con_Error("FinaleInterpreter::LoadScript: Unknown error"
                              "occured executing directive \"OnLoad\".");
            }
            findEnd(fi);
end_read:
            // Skip over any trailing whitespace to position the read cursor
            // on the first token.
            while(*fi->_cp && isspace(*fi->_cp))
                fi->_cp++;

            // Calculate the new script entry point and restore default state.
            fi->_scriptBegin = fi->_script + (fi->_cp - fi->_script);
            fi->_cp = fi->_scriptBegin;
            initDefaultState(fi);
        }
    }

    // Any hooks?
    DD_CallHooks(HOOK_FINALE_SCRIPT_BEGIN, fi->_id, 0);

    // Run a single tic so that the script can be drawn.
    FinaleInterpreter_RunTic(fi);
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

void FinaleInterpreter_Resume(finaleinterpreter_t* fi)
{
    assert(fi);
    if(!fi->flags.suspended)
        return;
    fi->flags.suspended = false;
    FIPage_Pause(fi->_pages[PAGE_PICS], false);
    FIPage_Pause(fi->_pages[PAGE_TEXT], false);
    // Do we need to unhide any pages?
    if(fi->_cmdExecuted)
    {
        FIPage_MakeVisible(fi->_pages[PAGE_PICS], true);
        FIPage_MakeVisible(fi->_pages[PAGE_TEXT], true);
    }
}

void FinaleInterpreter_Suspend(finaleinterpreter_t* fi)
{
    assert(fi);
    if(fi->flags.suspended)
        return;
    fi->flags.suspended = true;
    // While suspended, all pages will be paused and hidden.
    FIPage_Pause(fi->_pages[PAGE_PICS], true);
    FIPage_MakeVisible(fi->_pages[PAGE_PICS], false);
    FIPage_Pause(fi->_pages[PAGE_TEXT], true);
    FIPage_MakeVisible(fi->_pages[PAGE_TEXT], false);
}

boolean FinaleInterpreter_IsMenuTrigger(finaleinterpreter_t* fi)
{
    assert(fi);
    if(fi->flags.paused || fi->flags.can_skip)
    {
        // We want events to be used for unpausing/skipping.
        return false;
    }
    // If skipping is not allowed, we should show the menu, too.
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
    return fi->_cmdExecuted;
}

static boolean runTic(finaleinterpreter_t* fi)
{
    ddhook_finale_script_ticker_paramaters_t params;
    memset(&params, 0, sizeof(params));
    params.runTick = true;
    params.canSkip = FinaleInterpreter_CanSkip(fi);
    DD_CallHooks(HOOK_FINALE_SCRIPT_TICKER, fi->_id, &params);
    return params.runTick;
}

boolean FinaleInterpreter_RunTic(finaleinterpreter_t* fi)
{
    assert(fi);

    if(fi->flags.stopped || fi->flags.suspended)
        return false;

    fi->_timer++;

    if(!runTic(fi))
        return false;

    // If waiting do not execute commands.
    if(fi->_wait && --fi->_wait)
        return false;

    // If paused there is nothing further to do.
    if(fi->flags.paused)
        return false;

    // If waiting on a text to finish typing, do nothing.
    if(fi->_waitingText && fi->_waitingText->type == FI_TEXT)
    {
        if(!((fidata_text_t*)fi->_waitingText)->animComplete)
            return false;

        fi->_waitingText = NULL;
    }

    // Waiting for an animation to reach its end?
    if(fi->_waitingPic && fi->_waitingPic->type == FI_PIC)
    {
        if(!((fidata_pic_t*)fi->_waitingPic)->animComplete)
            return false;

        fi->_waitingPic = NULL;
    }

    // Execute commands until a wait time is set or we reach the end of
    // the script. If the end is reached, the finale really ends (terminates).
    {int last = 0;
    while(!fi->_gotoEnd && !fi->_wait && !fi->_waitingText && !fi->_waitingPic && !last)
        last = !executeNextCommand(fi);
    return (fi->_gotoEnd || (last && fi->flags.can_skip));}
}

boolean FinaleInterpreter_SkipToMarker(finaleinterpreter_t* fi, const char* marker)
{
    assert(fi && marker);

    if(!marker[0])
        return false;

    memset(fi->_gotoTarget, 0, sizeof(fi->_gotoTarget));
    strncpy(fi->_gotoTarget, marker, sizeof(fi->_gotoTarget) - 1);

    // Start skipping until the marker is found.
    fi->_gotoSkip = true;

    // Stop any waiting.
    fi->_wait = 0;
    fi->_waitingText = NULL;
    fi->_waitingPic = NULL;

    // Rewind the script so we can jump anywhere.
    fi->_cp = fi->_scriptBegin;
    return true;
}

boolean FinaleInterpreter_Skip(finaleinterpreter_t* fi)
{
    assert(fi);

    if(fi->_waitingText && fi->flags.can_skip && !fi->flags.paused)
    {
        // Instead of skipping, just complete the text.
        FIData_TextAccelerate(fi->_waitingText);
        return true;
    }

    // Stop waiting for objects.
    fi->_waitingText = NULL;
    fi->_waitingPic = NULL;
    if(fi->flags.paused)
    {
        fi->flags.paused = false;
        fi->_wait = 0;
        return true;
    }

    if(fi->flags.can_skip)
    {
        fi->_skipping = true; // Start skipping ahead.
        fi->_wait = 0;
        return true;
    }

    return (fi->flags.eat_events != 0);
}

int FinaleInterpreter_Responder(finaleinterpreter_t* fi, const ddevent_t* ev)
{
    assert(fi);

    DEBUG_VERBOSE2_Message(("FinaleInterpreter_Responder: fi %i, ev %i\n", fi->_id, ev->type));

    if(fi->flags.suspended)
        return false;

    // During the first ~second disallow all events/skipping.
    if(fi->_timer < 20)
        return false;

    if(!isClient)
    {
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
    }

    // If we can't skip, there's no interaction of any kind.
    if(!fi->flags.can_skip && !fi->flags.paused)
        return false;

    // We are only interested in key/button down presses.
    if(!IS_TOGGLE_DOWN(ev))
        return false;

#ifdef __CLIENT__
    if(isClient)
    {
        // Request skip from the server.
        Cl_RequestFinaleSkip();
        return true;
    }
#endif
#ifdef __SERVER__
    {
        // Tell clients to skip.
        Sv_Finale(fi->_id, FINF_SKIP, 0);
        return FinaleInterpreter_Skip(fi);
    }
#endif
    return false;
}

DEFFC(Do)
{   // This command is called even when (cond)skipping.
    if(!fi->_skipNext)
        return;

    // A conditional skip has been issued.
    // We'll go into DO-skipping mode. skipnext won't be cleared
    // until the matching semicolon is found.
    fi->_doLevel++;
}

DEFFC(End)
{
    fi->_gotoEnd = true;
}

DEFFC(BGMaterial)
{
    // First attempt to resolve as a Values URI (which defines the material URI).
    material_t* material;
    ded_value_t* value = Def_GetValueByUri(OP_URI(0));
    if(value)
    {
        material = Materials_ToMaterial(Materials_ResolveUriCString(value->text));
    }
    else
    {
        material = Materials_ToMaterial(Materials_ResolveUri(OP_URI(0)));
    }

    changePageBackground(fi->_pages[PAGE_PICS], material);
}

DEFFC(NoBGMaterial)
{
    changePageBackground(fi->_pages[PAGE_PICS], 0);
}

DEFFC(InTime)
{
    fi->_inTime = FRACSECS_TO_TICKS(OP_FLOAT(0));
}

DEFFC(Tic)
{
    fi->_wait = 1;
}

DEFFC(Wait)
{
    fi->_wait = FRACSECS_TO_TICKS(OP_FLOAT(0));
}

DEFFC(WaitText)
{
    fi->_waitingText = getObject(fi, FI_TEXT, OP_CSTRING(0));
}

DEFFC(WaitAnim)
{
    fi->_waitingPic = getObject(fi, FI_PIC, OP_CSTRING(0));
}

DEFFC(Color)
{
    FIPage_SetBackgroundTopColor(fi->_pages[PAGE_PICS], OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), fi->_inTime);
    FIPage_SetBackgroundBottomColor(fi->_pages[PAGE_PICS], OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), fi->_inTime);
}

DEFFC(ColorAlpha)
{
    FIPage_SetBackgroundTopColorAndAlpha(fi->_pages[PAGE_PICS], OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
    FIPage_SetBackgroundBottomColorAndAlpha(fi->_pages[PAGE_PICS], OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
}

DEFFC(Pause)
{
    fi->flags.paused = true;
    fi->_wait = 1;
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
    fi->_skipping = false;
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
    ev.toggle.id = DD_GetKeyCode(OP_CSTRING(0));
    ev.toggle.state = ETOG_DOWN;

    // First, try to find an existing handler.
    if(findEventHandler(fi, &ev))
        return;
    // Allocate and attach another.
    createEventHandler(fi, &ev, OP_CSTRING(1));
}

DEFFC(UnsetKey)
{
    ddevent_t ev;
    fi_handler_t* h;

    // Construct a template event for what we want to "unset".
    memset(&ev, 0, sizeof(ev));
    ev.device = IDEV_KEYBOARD;
    ev.type = E_TOGGLE;
    ev.toggle.id = DD_GetKeyCode(OP_CSTRING(0));
    ev.toggle.state = ETOG_DOWN;

    if((h = findEventHandler(fi, &ev)))
    {
        destroyEventHandler(fi, h);
    }
}

DEFFC(If)
{
    const char* token = OP_CSTRING(0);
    boolean val = false;

    // Built-in conditions.
    if(!stricmp(token, "netgame"))
    {
        val = netGame;
    }
    else if(!strnicmp(token, "mode:", 5))
    {
        if(DD_GameLoaded())
            val = !stricmp(token + 5, Str_Text(Game_IdentityKey(App_CurrentGame())));
        else
            val = 0;
    }
    // Any hooks?
    else if(Plug_CheckForHook(HOOK_FINALE_EVAL_IF))
    {
        ddhook_finale_script_evalif_paramaters_t p;
        memset(&p, 0, sizeof(p));
        p.token = token;
        p.returnVal = 0;
        if(DD_CallHooks(HOOK_FINALE_EVAL_IF, fi->_id, (void*) &p))
            val = p.returnVal;
    }
    else
    {
        Con_Message("FIC_If: Unknown condition '%s'.\n", token);
    }

    // Skip the next command if the value is false.
    fi->_skipNext = !val;
}

DEFFC(IfNot)
{
    FIC_If(cmd, ops, fi);
    fi->_skipNext = !fi->_skipNext;
}

DEFFC(Else)
{   // The only time the ELSE condition does not skip is immediately after a skip.
    fi->_skipNext = !fi->_lastSkipped;
}

DEFFC(GoTo)
{
    FinaleInterpreter_SkipToMarker(fi, OP_CSTRING(0));
}

DEFFC(Marker)
{
    // Does it match the goto string?
    if(!stricmp(fi->_gotoTarget, OP_CSTRING(0)))
        fi->_gotoSkip = false;
}

DEFFC(Delete)
{
    if(OP_OBJECT(0))
        FI_DeleteObject(removeObjectInNamespace(&fi->_namespace, OP_OBJECT(0)));
}

DEFFC(Image)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    const char* name = OP_CSTRING(1);
    lumpnum_t lumpNum = F_LumpNumForName(name);
    rawtex_t* rawTex;

    FIData_PicClearAnimation(obj);

    rawTex = R_GetRawTex(lumpNum);
    if(NULL != rawTex)
    {
        FIData_PicAppendFrame(obj, PFT_RAW, -1, &rawTex->lumpNum, 0, false);
        return;
    }
    Con_Message("FIC_Image: Warning, missing lump '%s'.\n", name);
}

DEFFC(ImageAt)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    float x = OP_FLOAT(1);
    float y = OP_FLOAT(2);
    const char* name = OP_CSTRING(3);
    lumpnum_t lumpNum = F_LumpNumForName(name);
    rawtex_t* rawTex;

    AnimatorVector3_Init(obj->pos, x, y, 0);
    FIData_PicClearAnimation(obj);

    rawTex = R_GetRawTex(lumpNum);
    if(NULL != rawTex)
    {
        FIData_PicAppendFrame(obj, PFT_RAW, -1, &rawTex->lumpNum, 0, false);
        return;
    }
    Con_Message("FIC_ImageAt: Warning, missing lump '%s'.\n", name);
}

static DGLuint loadGraphics(const char* name, gfxmode_t mode, int useMipmap, boolean clamped, int otherFlags)
{
#ifdef __CLIENT__
    return GL_PrepareExtTexture(name, mode, useMipmap,
                                GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
                                clamped? GL_CLAMP_TO_EDGE : GL_REPEAT,
                                clamped? GL_CLAMP_TO_EDGE : GL_REPEAT,
                                otherFlags);
#else
    return 0;
#endif
}

DEFFC(XImage)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    const char* fileName = OP_CSTRING(1);
    DGLuint tex;

    FIData_PicClearAnimation(obj);

    // Load the external resource.
    if((tex = loadGraphics(fileName, LGM_NORMAL, false, true, 0)))
    {
        FIData_PicAppendFrame(obj, PFT_XIMAGE, -1, &tex, 0, false);
    }
    else
        Con_Message("FIC_XImage: Warning, missing graphic '%s'.\n", fileName);
}

DEFFC(Patch)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    const char* name = OP_CSTRING(3);
    patchid_t patchId;

    AnimatorVector3_Init(obj->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    FIData_PicClearAnimation(obj);

    patchId = R_DeclarePatch(name);
    if(patchId != 0)
    {
        FIData_PicAppendFrame(obj, PFT_PATCH, -1, (void*)&patchId, 0, 0);
    }
    else
    {
        Con_Message("FIC_Patch: Warning, missing Patch '%s'.\n", name);
    }
}

DEFFC(SetPatch)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    const char* name = OP_CSTRING(1);
    fidata_pic_frame_t* f;
    patchid_t patchId;

    patchId = R_DeclarePatch(name);
    if(patchId == 0)
    {
        Con_Message("FIC_SetPatch: Warning, missing Patch '%s'.\n", name);
        return;
    }

    if(!((fidata_pic_t*)obj)->numFrames)
    {
        FIData_PicAppendFrame(obj, PFT_PATCH, -1, (void*)&patchId, 0, false);
        return;
    }

    // Convert the first frame.
    f = ((fidata_pic_t*)obj)->frames[0];
    f->type = PFT_PATCH;
    f->texRef.patch = patchId;
    f->tics = -1;
    f->sound = 0;
}

DEFFC(ClearAnim)
{
    if(OP_OBJECT(0) && OP_OBJECT(0)->type == FI_PIC)
    {
        FIData_PicClearAnimation(OP_OBJECT(0));
    }
}

DEFFC(Anim)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    const char* name = OP_CSTRING(1);
    int tics = FRACSECS_TO_TICKS(OP_FLOAT(2));
    patchid_t patchId;

    patchId = R_DeclarePatch(name);
    if(patchId == 0)
    {
        Con_Message("FIC_Anim: Warning, Patch '%s' not found.\n", name);
        return;
    }

    FIData_PicAppendFrame(obj, PFT_PATCH, tics, (void*)&patchId, 0, false);
    ((fidata_pic_t*)obj)->animComplete = false;
}

DEFFC(AnimImage)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    const char* name = OP_CSTRING(1);
    int tics = FRACSECS_TO_TICKS(OP_FLOAT(2));
    lumpnum_t lumpNum = F_LumpNumForName(name);
    rawtex_t* rawTex = R_GetRawTex(lumpNum);
    if(NULL != rawTex)
    {
        FIData_PicAppendFrame(obj, PFT_RAW, tics, &rawTex->lumpNum, 0, false);
        ((fidata_pic_t*)obj)->animComplete = false;
        return;
    }
    Con_Message("FIC_AnimImage: Warning, lump '%s' not found.\n", name);
}

DEFFC(Repeat)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    ((fidata_pic_t*)obj)->flags.looping = true;
}

DEFFC(StateAnim)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    int stateId = Def_Get(DD_DEF_STATE, OP_CSTRING(1), 0);
    int count = OP_INT(2);

    // Animate N states starting from the given one.
    ((fidata_pic_t*)obj)->animComplete = false;
    for(; count > 0 && stateId > 0; count--)
    {
        state_t* st = &states[stateId];
#ifdef __CLIENT__
        spriteinfo_t sinf;
        R_GetSpriteInfo(st->sprite, st->frame & 0x7fff, &sinf);
        FIData_PicAppendFrame(obj, PFT_MATERIAL, (st->tics <= 0? 1 : st->tics), sinf.material, 0, sinf.flip);
#endif

        // Go to the next state.
        stateId = st->nextState;
    }
}

DEFFC(PicSound)
{
    fi_object_t* obj = getObject(fi, FI_PIC, OP_CSTRING(0));
    int sound = Def_Get(DD_DEF_SOUND, OP_CSTRING(1), 0);
    if(!((fidata_pic_t*)obj)->numFrames)
    {
        FIData_PicAppendFrame(obj, PFT_MATERIAL, -1, 0, sound, false);
        return;
    }
    {fidata_pic_frame_t* f = ((fidata_pic_t*)obj)->frames[((fidata_pic_t*)obj)->numFrames-1];
    f->sound = sound;
    }
}

DEFFC(ObjectOffX)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->pos[0], OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectOffY)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->pos[1], OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectOffZ)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->pos[2], OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectRGB)
{
    fi_object_t* obj = OP_OBJECT(0);
    float rgb[3];
    if(!obj || !(obj->type == FI_TEXT || obj->type == FI_PIC))
        return;
    rgb[CR] = OP_FLOAT(1);
    rgb[CG] = OP_FLOAT(2);
    rgb[CB] = OP_FLOAT(3);
    switch(obj->type)
    {
    default: Con_Error("FinaleInterpreter::FIC_ObjectRGB: Unknown type %i.", (int) obj->type);
    case FI_TEXT:
        FIData_TextSetColor(obj, rgb[CR], rgb[CG], rgb[CB], fi->_inTime);
        break;
    case FI_PIC: {
        fidata_pic_t* p = (fidata_pic_t*)obj;
        AnimatorVector3_Set(p->color, rgb[CR], rgb[CG], rgb[CB], fi->_inTime);
        // This affects all the colors.
        AnimatorVector3_Set(p->otherColor, rgb[CR], rgb[CG], rgb[CB], fi->_inTime);
        AnimatorVector3_Set(p->edgeColor, rgb[CR], rgb[CG], rgb[CB], fi->_inTime);
        AnimatorVector3_Set(p->otherEdgeColor, rgb[CR], rgb[CG], rgb[CB], fi->_inTime);
        break;
      }
    }
}

DEFFC(ObjectAlpha)
{
    fi_object_t* obj = OP_OBJECT(0);
    float alpha;
    if(!obj || !(obj->type == FI_TEXT || obj->type == FI_PIC))
        return;
    alpha = OP_FLOAT(1);
    switch(obj->type)
    {
    default: Con_Error("FinaleInterpreter::FIC_ObjectAlpha: Unknown type %i.", (int) obj->type);
    case FI_TEXT:
        FIData_TextSetAlpha(obj, alpha, fi->_inTime);
        break;
    case FI_PIC: {
        fidata_pic_t* p = (fidata_pic_t*)obj;
        Animator_Set(&p->color[3], alpha, fi->_inTime);
        Animator_Set(&p->otherColor[3], alpha, fi->_inTime);
        break;
      }
    }
}

DEFFC(ObjectScaleX)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->scale[0], OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectScaleY)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->scale[1], OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectScaleZ)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->scale[2], OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectScale)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        AnimatorVector2_Set(obj->scale, OP_FLOAT(1), OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(ObjectScaleXY)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        AnimatorVector2_Set(obj->scale, OP_FLOAT(1), OP_FLOAT(2), fi->_inTime);
    }
}

DEFFC(ObjectScaleXYZ)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        AnimatorVector3_Set(obj->scale, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
    }
}

DEFFC(ObjectAngle)
{
    if(OP_OBJECT(0))
    {
        fi_object_t* obj = OP_OBJECT(0);
        Animator_Set(&obj->angle, OP_FLOAT(1), fi->_inTime);
    }
}

DEFFC(Rect)
{
    fidata_pic_t* obj = (fidata_pic_t*) getObject(fi, FI_PIC, OP_CSTRING(0));

    /**
     * We may be converting an existing Pic to a Rect, so re-init the expected
     * default state accordingly.
     *
     * danij: This seems rather error-prone to me. How about we turn them into
     * seperate object classes instead (Pic inheriting from Rect).
     */
    obj->animComplete = true;
    obj->flags.looping = false; // Yeah?

    AnimatorVector3_Init(obj->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    AnimatorVector3_Init(obj->scale, OP_FLOAT(3), OP_FLOAT(4), 1);

    // Default colors.
    AnimatorVector4_Init(obj->color, 1, 1, 1, 1);
    AnimatorVector4_Init(obj->otherColor, 1, 1, 1, 1);

    // Edge alpha is zero by default.
    AnimatorVector4_Init(obj->edgeColor, 1, 1, 1, 0);
    AnimatorVector4_Init(obj->otherEdgeColor, 1, 1, 1, 0);
}

DEFFC(FillColor)
{
    fi_object_t* obj = OP_OBJECT(0);
    int which = 0;
    float rgba[4];

    if(!obj || obj->type != FI_PIC)
        return;

    // Which colors to modify?
    if(!stricmp(OP_CSTRING(1), "top"))
        which |= 1;
    else if(!stricmp(OP_CSTRING(1), "bottom"))
        which |= 2;
    else
        which = 3;

    {uint i;
    for(i = 0; i < 4; ++i)
        rgba[i] = OP_FLOAT(2+i);
    }

    if(which & 1)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->color, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->_inTime);
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->otherColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->_inTime);
}

DEFFC(EdgeColor)
{
    fi_object_t* obj = OP_OBJECT(0);
    int which = 0;
    float rgba[4];

    if(!obj || obj->type != FI_PIC)
        return;

    // Which colors to modify?
    if(!stricmp(OP_CSTRING(1), "top"))
        which |= 1;
    else if(!stricmp(OP_CSTRING(1), "bottom"))
        which |= 2;
    else
        which = 3;

    {uint i;
    for(i = 0; i < 4; ++i)
        rgba[i] = OP_FLOAT(2+i);
    }

    if(which & 1)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->edgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->_inTime);
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t*)obj)->otherEdgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi->_inTime);
}

DEFFC(OffsetX)
{
    FIPage_SetOffsetX(fi->_pages[PAGE_PICS], OP_FLOAT(0), fi->_inTime);
}

DEFFC(OffsetY)
{
    FIPage_SetOffsetY(fi->_pages[PAGE_PICS], OP_FLOAT(0), fi->_inTime);
}

DEFFC(Sound)
{
    int num = Def_Get(DD_DEF_SOUND, OP_CSTRING(0), NULL);
    if(num > 0)
        S_LocalSound(num, NULL);
}

DEFFC(SoundAt)
{
    int num = Def_Get(DD_DEF_SOUND, OP_CSTRING(0), NULL);
    float vol = MIN_OF(OP_FLOAT(1), 1);
    if(num > 0)
        S_LocalSoundAtVolume(num, NULL, vol);
}

DEFFC(SeeSound)
{
    int num = Def_Get(DD_DEF_MOBJ, OP_CSTRING(0), NULL);
    if(num < 0 || mobjInfo[num].seeSound <= 0)
        return;
    S_LocalSound(mobjInfo[num].seeSound, NULL);
}

DEFFC(DieSound)
{
    int num = Def_Get(DD_DEF_MOBJ, OP_CSTRING(0), NULL);
    if(num < 0 || mobjInfo[num].deathSound <= 0)
        return;
    S_LocalSound(mobjInfo[num].deathSound, NULL);
}

DEFFC(Music)
{
    S_StartMusic(OP_CSTRING(0), true);
}

DEFFC(MusicOnce)
{
    S_StartMusic(OP_CSTRING(0), false);
}

DEFFC(Filter)
{
    FIPage_SetFilterColorAndAlpha(fi->_pages[PAGE_TEXT], OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
}

DEFFC(Text)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    AnimatorVector3_Init(obj->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    FIData_TextCopy(obj, OP_CSTRING(3));
    ((fidata_text_t*)obj)->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromDef)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    char* str;
    AnimatorVector3_Init(obj->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    if(!Def_Get(DD_DEF_TEXT, (char*)OP_CSTRING(3), &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(obj, str);
    ((fidata_text_t*)obj)->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromLump)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    lumpnum_t lumpNum;

    AnimatorVector3_Init(obj->pos, OP_FLOAT(1), OP_FLOAT(2), 0);

    lumpNum = F_LumpNumForName(OP_CSTRING(3));
    if(lumpNum >= 0)
    {
        int lumpIdx;
        size_t lumpSize = F_LumpLength(lumpNum);
        struct file1_s* file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
        const uint8_t* lumpPtr = F_CacheLump(file, lumpIdx);
        size_t bufSize = 2 * lumpSize + 1, i;
        char* str, *out;

        str = (char*) calloc(1, bufSize);
        if(!str)
            Con_Error("FinaleInterpreter::FIC_TextFromLump: Failed on allocation of %lu bytes for text formatting translation buffer.", (unsigned long) bufSize);

        for(i = 0, out = str; i < lumpSize; ++i)
        {
            char ch = (char)(lumpPtr[i]);
            if(ch == '\r') continue;
            if(ch == '\n')
            {
                *out++ = '\\';
                *out++ = 'n';
            }
            else
            {
                *out++ = ch;
            }
        }
        F_UnlockLump(file, lumpIdx);

        FIData_TextCopy(obj, str);
        free(str);
    }
    else
    {
        FIData_TextCopy(obj, "(not found)");
    }
    ((fidata_text_t*)obj)->cursorPos = 0; // Restart.
}

DEFFC(SetText)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    FIData_TextCopy(obj, OP_CSTRING(1));
}

DEFFC(SetTextDef)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    char* str;
    if(!Def_Get(DD_DEF_TEXT, OP_CSTRING(1), &str))
        str = "(undefined)"; // Not found!
    FIData_TextCopy(obj, str);
}

DEFFC(DeleteText)
{
    if(OP_OBJECT(0))
        FI_DeleteObject(removeObjectInNamespace(&fi->_namespace, OP_OBJECT(0)));
}

DEFFC(PredefinedColor)
{
    FIPage_SetPredefinedColor(fi->_pages[PAGE_TEXT], MINMAX_OF(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_COLORS)-1, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
    FIPage_SetPredefinedColor(fi->_pages[PAGE_PICS], MINMAX_OF(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_COLORS)-1, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
}

DEFFC(PredefinedFont)
{
#ifdef __CLIENT__
    fontid_t fontNum = Fonts_ResolveUri(OP_URI(1));
    if(fontNum)
    {
        int idx = MINMAX_OF(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_FONTS)-1;
        FIPage_SetPredefinedFont(fi->_pages[PAGE_TEXT], idx, fontNum);
        FIPage_SetPredefinedFont(fi->_pages[PAGE_PICS], idx, fontNum);
        return;
    }

    { AutoStr* fontPath = Uri_ToString(OP_URI(1));
    Con_Message("FIC_PredefinedFont: Warning, unknown font '%s'.\n", Str_Text(fontPath));
    }
#endif
}

DEFFC(TextRGB)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    FIData_TextSetColor(obj, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi->_inTime);
}

DEFFC(TextAlpha)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    FIData_TextSetAlpha(obj, OP_FLOAT(1), fi->_inTime);
}

DEFFC(TextOffX)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    Animator_Set(&obj->pos[0], OP_FLOAT(1), fi->_inTime);
}

DEFFC(TextOffY)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    Animator_Set(&obj->pos[1], OP_FLOAT(1), fi->_inTime);
}

DEFFC(TextCenter)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t*)obj)->alignFlags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
}

DEFFC(TextNoCenter)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t*)obj)->alignFlags |= ALIGN_LEFT;
}

DEFFC(TextScroll)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t*)obj)->scrollWait = OP_INT(1);
    ((fidata_text_t*)obj)->scrollTimer = 0;
}

DEFFC(TextPos)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t*)obj)->cursorPos = OP_INT(1);
}

DEFFC(TextRate)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t*)obj)->wait = OP_INT(1);
}

DEFFC(TextLineHeight)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t*)obj)->lineHeight = OP_FLOAT(1);
}

DEFFC(Font)
{
#ifdef __CLIENT__
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    fontid_t fontNum = Fonts_ResolveUri(OP_URI(1));
    if(fontNum)
    {
        FIData_TextSetFont(obj, fontNum);
        return;
    }

    { AutoStr* fontPath = Uri_ToString(OP_URI(1));
    Con_Message("FIC_Font: Warning, unknown font '%s'.\n", Str_Text(fontPath));
    }
#endif
}

DEFFC(FontA)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    FIData_TextSetFont(obj, FIPage_PredefinedFont(fi->_pages[PAGE_TEXT], 0));
}

DEFFC(FontB)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    FIData_TextSetFont(obj, FIPage_PredefinedFont(fi->_pages[PAGE_TEXT], 1));
}

DEFFC(NoMusic)
{
    // Stop the currently playing song.
    S_StopMusic();
}

DEFFC(TextScaleX)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    Animator_Set(&obj->scale[0], OP_FLOAT(1), fi->_inTime);
}

DEFFC(TextScaleY)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    Animator_Set(&obj->scale[1], OP_FLOAT(1), fi->_inTime);
}

DEFFC(TextScale)
{
    fi_object_t* obj = getObject(fi, FI_TEXT, OP_CSTRING(0));
    AnimatorVector2_Set(obj->scale, OP_FLOAT(1), OP_FLOAT(2), fi->_inTime);
}

DEFFC(PlayDemo)
{
    // While playing a demo we suspend command interpretation.
    FinaleInterpreter_Suspend(fi);

    // Start the demo.
    if(!Con_Executef(CMDS_DDAY, true, "playdemo \"%s\"", OP_CSTRING(0)))
    {   // Demo playback failed. Here we go again...
        FinaleInterpreter_Resume(fi);
    }
}

DEFFC(Command)
{
    Con_Executef(CMDS_SCRIPT, false, "%s", OP_CSTRING(0));
}

DEFFC(ShowMenu)
{
    fi->flags.show_menu = true;
}

DEFFC(NoShowMenu)
{
    fi->flags.show_menu = false;
}
