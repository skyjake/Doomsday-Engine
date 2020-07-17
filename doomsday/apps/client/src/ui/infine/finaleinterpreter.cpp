/** @file finaleinterpreter.cpp  InFine animation system Finale script interpreter.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "ui/infine/finaleinterpreter.h"

#include <de/list.h>
#include <de/legacy/memory.h>
#include <de/legacy/timer.h>
#include <de/logbuffer.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/world/materials.h>
#include <doomsday/game.h>

#include "dd_def.h"
#include "def_main.h"  // ::defs

#include "api_material.h"
#include "api_render.h"
#include "api_resource.h"

#include "network/net_main.h"

#include "ui/infine/finalewidget.h"
#include "ui/infine/finaleanimwidget.h"
#include "ui/infine/finalepagewidget.h"
#include "ui/infine/finaletextwidget.h"

#ifdef __CLIENT__
#  include "api_fontrender.h"
#  include "client/cl_infine.h"

#  include "gl/gl_main.h"
#  include "gl/gl_texmanager.h"
#  include "gl/texturecontent.h"
#  include "gl/sys_opengl.h" // TODO: get rid of this
#endif

#ifdef __SERVER__
#  include "server/sv_infine.h"
#endif

#define MAX_TOKEN_LENGTH        (8192)

#define FRACSECS_TO_TICKS(sec)  (int(sec * TICSPERSEC + 0.5))

using namespace de;
using namespace res;

enum fi_operand_type_t
{
    FVT_INT,
    FVT_FLOAT,
    FVT_STRING,
    FVT_URI
};

static fi_operand_type_t operandTypeForCharCode(char code)
{
    switch (code)
    {
    case 'i': return FVT_INT;
    case 'f': return FVT_FLOAT;
    case 's': return FVT_STRING;
    case 'u': return FVT_URI;

    default: throw Error("operandTypeForCharCode", stringf("Unknown char-code '%c'", code));
    }
}

// Helper macro for accessing the value of an operand.
#define OP_INT(n)           (ops[n].data.integer)
#define OP_FLOAT(n)         (ops[n].data.flt)
#define OP_CSTRING(n)       (ops[n].data.cstring)
#define OP_URI(n)           (ops[n].data.uri)

struct fi_operand_t
{
    fi_operand_type_t type;
    union {
        int         integer;
        float       flt;
        const char *cstring;
        uri_s      *uri;
    } data;
};

// Helper macro for defining infine command functions.
#define DEFFC(name) void FIC_##name(const command_t &cmd, const fi_operand_t *ops, FinaleInterpreter &fi)

/**
 * @defgroup finaleInterpreterCommandDirective Finale Interpreter Command Directive
 * @ingroup infine
 */
/*@{*/
#define FID_NORMAL          0
#define FID_ONLOAD          0x1
/*@}*/

struct command_t
{
    const char *token;
    const char *operands;

    typedef void (*Func) (const command_t &, const fi_operand_t *, FinaleInterpreter &);
    Func func;

    struct command_flags_s {
        char when_skipping:1;
        char when_condition_skipping:1; // Skipping because condition failed.
    } flags;

    /// Command execution directives NOT supported by this command.
    /// @see finaleInterpreterCommandDirective
    int excludeDirectives;
};

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
static const command_t *findCommand(const char *name)
{
    static command_t const commands[] = {
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
        { "del",        "s", FIC_Delete }, // del (wi)
        { "x",          "sf", FIC_ObjectOffX }, // x (wi) (x)
        { "y",          "sf", FIC_ObjectOffY }, // y (wi) (y)
        { "z",          "sf", FIC_ObjectOffZ }, // z (wi) (z)
        { "sx",         "sf", FIC_ObjectScaleX }, // sx (wi) (x)
        { "sy",         "sf", FIC_ObjectScaleY }, // sy (wi) (y)
        { "sz",         "sf", FIC_ObjectScaleZ }, // sz (wi) (z)
        { "scale",      "sf", FIC_ObjectScale }, // scale (wi) (factor)
        { "scalexy",    "sff", FIC_ObjectScaleXY }, // scalexy (wi) (x) (y)
        { "scalexyz",   "sfff", FIC_ObjectScaleXYZ }, // scalexyz (wi) (x) (y) (z)
        { "rgb",        "sfff", FIC_ObjectRGB }, // rgb (wi) (r) (g) (b)
        { "alpha",      "sf", FIC_ObjectAlpha }, // alpha (wi) (alpha)
        { "angle",      "sf", FIC_ObjectAngle }, // angle (wi) (degrees)

        // Rects
        { "rect",       "sffff", FIC_Rect }, // rect (hndl) (x) (y) (w) (h)
        { "fillcolor",  "ssffff", FIC_FillColor }, // fillcolor (wi) (top/bottom/both) (r) (g) (b) (a)
        { "edgecolor",  "ssffff", FIC_EdgeColor }, // edgecolor (wi) (top/bottom/both) (r) (g) (b) (a)

        // Pics
        { "image",      "ss", FIC_Image }, // image (id) (raw-image-lump)
        { "imageat",    "sffs", FIC_ImageAt }, // imageat (id) (x) (y) (raw)
        { "ximage",     "ss", FIC_XImage }, // ximage (id) (ext-gfx-filename)
        { "patch",      "sffs", FIC_Patch }, // patch (id) (x) (y) (patch)
        { "set",        "ss", FIC_SetPatch }, // set (id) (lump)
        { "clranim",    "s", FIC_ClearAnim }, // clranim (wi)
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
        { "delpic",     "s", FIC_Delete }, // delpic (wi)

        // Deprecated Text commands
        { "deltext",    "s", FIC_DeleteText }, // deltext (wi)
        { "textrgb",    "sfff", FIC_TextRGB }, // textrgb (id) (r) (g) (b)
        { "textalpha",  "sf", FIC_TextAlpha }, // textalpha (id) (alpha)
        { "tx",         "sf", FIC_TextOffX }, // tx (id) (x)
        { "ty",         "sf", FIC_TextOffY }, // ty (id) (y)
        { "tsx",        "sf", FIC_TextScaleX }, // tsx (id) (x)
        { "tsy",        "sf", FIC_TextScaleY }, // tsy (id) (y)
        { "textscale",  "sf", FIC_TextScale }, // textscale (id) (x) (y)

        { nullptr, 0, nullptr } // Terminate.
    };
    for (size_t i = 0; commands[i].token; ++i)
    {
        const command_t *cmd = &commands[i];
        if (!iCmpStrCase(cmd->token, name))
        {
            return cmd;
        }
    }
    return nullptr; // Not found.
}

DE_PIMPL(FinaleInterpreter)
{
    struct Flags
    {
        char stopped:1;
        char suspended:1;
        char paused:1;
        char can_skip:1;
        char eat_events:1; /// Script will eat all input events.
        char show_menu:1;
    } flags;

    finaleid_t id = 0;                  ///< Unique identifier.

    ddstring_t *script      = nullptr;  ///< The script to be interpreted.
    const char *scriptBegin = nullptr;  ///< Beginning of the script (after any directive blocks).
    const char *cp          = nullptr;  ///< Current position in the script.
    char token[MAX_TOKEN_LENGTH];       ///< Script token read/parse buffer.

    /// Pages containing the widgets used to visualize the script objects.
    std::unique_ptr<FinalePageWidget> pages[2];

    bool cmdExecuted = false;  ///< Set to true after first command is executed.

    bool skipping    = false;
    bool lastSkipped = false;
    bool skipNext    = false;
    bool gotoEnd     = false;
    bool gotoSkip    = false;
    String gotoTarget;

    int doLevel = 0;  ///< Level of DO-skipping.
    uint timer  = 0;
    int wait    = 0;
    int inTime  = 0;

    FinaleAnimWidget *waitAnim = nullptr;
    FinaleTextWidget *waitText = nullptr;

#ifdef __CLIENT__
    struct EventHandler
    {
        ddevent_t ev; // Template.
        String gotoMarker;

        explicit EventHandler(const ddevent_t *evTemplate = nullptr,
                              const String &gotoMarker    = "")
            : gotoMarker(gotoMarker) {
            std::memcpy(&ev, &evTemplate, sizeof(ev));
        }
        EventHandler(const EventHandler &other)
            : gotoMarker(other.gotoMarker) {
            std::memcpy(&ev, &other.ev, sizeof(ev));
        }
    };

    typedef List<EventHandler> EventHandlers;
    EventHandlers eventHandlers;
#endif // __CLIENT__

    Impl(Public *i, finaleid_t id) : Base(i), id(id)
    {
        de::zap(flags);
        de::zap(token);
    }

    ~Impl()
    {
        stop();
        releaseScript();
    }

    void initDefaultState()
    {
        flags.suspended = false;
        flags.paused    = false;
        flags.show_menu = true; // Unhandled events will show a menu.
        flags.can_skip  = true; // By default skipping is enabled.

        cmdExecuted = false; // Nothing is drawn until a cmd has been executed.
        skipping    = false;
        wait        = 0;     // Not waiting for anything.
        inTime      = 0;     // Interpolation is off.
        timer       = 0;
        gotoSkip    = false;
        gotoEnd     = false;
        skipNext    = false;
        waitText    = nullptr;
        waitAnim    = nullptr;
        gotoTarget.clear();

#ifdef __CLIENT__
        eventHandlers.clear();
#endif
    }

    void releaseScript()
    {
        Str_Delete(script); script = nullptr;
        scriptBegin = nullptr;
        cp          = nullptr;
    }

    void stop()
    {
        if (flags.stopped) return;

        flags.stopped = true;
        LOGDEV_SCR_MSG("Finale End - id:%i '%.30s'") << id << scriptBegin;

#ifdef __SERVER__
        if (netState.isServer && !(FI_ScriptFlags(id) & FF_LOCAL))
        {
            // Tell clients to stop the finale.
            Sv_Finale(id, FINF_END, 0);
        }
#endif

        // Any hooks?
        DoomsdayApp::plugins().callAllHooks(HOOK_FINALE_SCRIPT_STOP, id);
    }

    bool atEnd() const
    {
        DE_ASSERT(script);
        return (cp - Str_Text(script)) >= Str_Length(script);
    }

    void findBegin()
    {
        const char *tok;
        while (!gotoEnd && 0 != (tok = nextToken()) && iCmpStrCase(tok, "{")) {}
    }

    void findEnd()
    {
        const char *tok;
        while (!gotoEnd && 0 != (tok = nextToken()) && iCmpStrCase(tok, "}")) {}
    }

    const char *nextToken()
    {
        // Skip whitespace.
        while (!atEnd() && isspace(*cp)) { cp++; }

        // Have we reached the end?
        if (atEnd()) return nullptr;

        char *out = token;
        if (*cp == '"') // A string?
        {
            for (cp++; !atEnd(); cp++)
            {
                if (*cp == '"')
                {
                    cp++;
                    // Convert double quotes to single ones.
                    if (*cp != '"') break;
                }
                *out++ = *cp;
            }
        }
        else
        {
            while (!isspace(*cp) && !atEnd()) { *out++ = *cp++; }
        }
        *out++ = 0;

        return token;
    }

    /// @return  @c true if the end of the script was reached.
    bool executeNextCommand()
    {
        if (const char *tok = nextToken())
        {
            executeCommand(tok, FID_NORMAL);
            // Time to unhide the object page(s)?
            if (cmdExecuted)
            {
                pages[Anims]->makeVisible();
                pages[Texts]->makeVisible();
            }
            return false;
        }
        return true;
    }

    static inline const char *findDefaultValueEnd(const char *str)
    {
        const char *defaultValueEnd;
        for (defaultValueEnd = str; defaultValueEnd && *defaultValueEnd != ')'; defaultValueEnd++)
        {}
        DE_ASSERT(defaultValueEnd < str + strlen(str));
        return defaultValueEnd;
    }

    static const char *nextOperand(const char *operands)
    {
        if (operands && operands[0])
        {
            // Some operands might include a default value.
            int len = strlen(operands);
            if (len > 1 && operands[1] == '(')
            {
                // A default value begins. Find the end.
                return findDefaultValueEnd(operands + 2) + 1;
            }
            return operands + 1;
        }
        return nullptr; // No more operands.
    }

    /// @return Total number of command operands in the control string @a operands.
    static int countCommandOperands(const char *operands)
    {
        int count = 0;
        while (operands && operands[0])
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
     * @return  Array of @c fi_operand_t else @c nullptr. Must be free'd with M_Free().
     */
    fi_operand_t *prepareCommandOperands(const command_t *cmd, int *count)
    {
        DE_ASSERT(cmd);

        const char *origCursorPos = cp;
        const int operandCount    = countCommandOperands(cmd->operands);
        if (operandCount <= 0) return nullptr;

        fi_operand_t *operands = (fi_operand_t *) M_Malloc(sizeof(*operands) * operandCount);
        const char *opRover    = cmd->operands;
        for (fi_operand_t *op = operands; opRover && opRover[0]; opRover = nextOperand(opRover), op++)
        {
            const char charCode = *opRover;

            op->type = operandTypeForCharCode(charCode);
            const bool opHasDefaultValue = (opRover < cmd->operands + (strlen(cmd->operands) - 2) && opRover[1] == '(');
            const bool haveValue         = !!nextToken();

            if (!haveValue && !opHasDefaultValue)
            {
                cp = origCursorPos;

                if (operands) M_Free(operands);
                if (count) *count = 0;

                App_Error("prepareCommandOperands: Too few operands for command '%s'.\n", cmd->token);
                return 0; // Unreachable.
            }

            switch (op->type)
            {
            case FVT_INT: {
                const char *valueStr = haveValue? token : nullptr;
                if (!valueStr)
                {
                    // Use the default.
                    const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    valueStr = Str_Text(defaultValue);
                }
                op->data.integer = strtol(valueStr, nullptr, 0);
                break; }

            case FVT_FLOAT: {
                const char *valueStr = haveValue? token : nullptr;
                if (!valueStr)
                {
                    // Use the default.
                    const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    valueStr = Str_Text(defaultValue);
                }
                op->data.flt = strtod(valueStr, nullptr);
                break; }

            case FVT_STRING: {
                const char *valueStr = haveValue? token : nullptr;
                int valueLen         = haveValue? strlen(token) : 0;
                if (!valueStr)
                {
                    // Use the default.
                    const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    valueStr = Str_Text(defaultValue);
                    valueLen = defaultValueLen;
                }
                op->data.cstring = (char *)M_Malloc(valueLen + 1);
                strcpy((char *)op->data.cstring, valueStr);
                break; }

            case FVT_URI: {
                uri_s *uri = Uri_New();
                // Always apply the default as it may contain a default scheme.
                if (opHasDefaultValue)
                {
                    const int defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    Uri_SetUri2(uri, Str_Text(defaultValue), RC_NULL);
                }
                if (haveValue)
                {
                    Uri_SetUri2(uri, token, RC_NULL);
                }
                op->data.uri = uri;
                break; }

            default: break; // Unreachable.
            }
        }

        if (count) *count = operandCount;

        return operands;
    }

    bool skippingCommand(const command_t *cmd)
    {
        DE_ASSERT(cmd);
        if ((skipNext && !cmd->flags.when_condition_skipping) ||
           ((skipping || gotoSkip) && !cmd->flags.when_skipping))
        {
            // While not DO-skipping, the condskip has now been done.
            if (!doLevel)
            {
                if (skipNext)
                    lastSkipped = true;
                skipNext = false;
            }
            return true;
        }
        return false;
    }

    /**
     * Execute one (the next) command, advance script cursor.
     */
    bool executeCommand(const char *commandString, int directive)
    {
        DE_ASSERT(commandString);
        bool didSkip = false;

        // Semicolon terminates DO-blocks.
        if (!iCmpStr(commandString, ";"))
        {
            if (doLevel > 0)
            {
                if (--doLevel == 0)
                {
                    // The DO-skip has been completed.
                    skipNext    = false;
                    lastSkipped = true;
                }
            }
            return true; // Success!
        }

        // We're now going to execute a command.
        cmdExecuted = true;

        // Is this a command we know how to execute?
        if (const command_t *cmd = findCommand(commandString))
        {
            const bool requiredOperands = (cmd->operands && cmd->operands[0]);

            // Is this command supported for this directive?
            if (directive != 0 && cmd->excludeDirectives != 0 &&
               (cmd->excludeDirectives & directive) == 0)
                App_Error("executeCommand: Command \"%s\" is not supported for directive %i.",
                          cmd->token, directive);

            // Check that there are enough operands.
            /// @todo Dynamic memory allocation during script interpretation should be avoided.
            int numOps        = 0;
            fi_operand_t *ops = nullptr;
            if (!requiredOperands || (ops = prepareCommandOperands(cmd, &numOps)))
            {
                // Should we skip this command?
                if (!(didSkip = skippingCommand(cmd)))
                {
                    // Execute forthwith!
                    cmd->func(*cmd, ops, self());
                }
            }

            if (!didSkip)
            {
                if (gotoEnd)
                {
                    wait = 1;
                }
                else
                {
                    // Now we've executed the latest command.
                    lastSkipped = false;
                }
            }

            if (ops)
            {
                for (int i = 0; i < numOps; ++i)
                {
                    fi_operand_t *op = &ops[i];
                    switch (op->type)
                    {
                    case FVT_STRING: M_Free((char *)op->data.cstring); break;
                    case FVT_URI:    Uri_Delete(op->data.uri);         break;

                    default: break;
                    }
                }
                M_Free(ops);
            }
        }

        return !didSkip;
    }

    static inline PageIndex choosePageFor(FinaleWidget &widget)
    {
        return is<FinaleAnimWidget>(widget)? Anims : Texts;
    }

    static inline PageIndex choosePageFor(fi_obtype_e type)
    {
        return type == FI_ANIM? Anims : Texts;
    }

    FinaleWidget *locateWidget(fi_obtype_e type, const String &name)
    {
        if (!name.isEmpty())
        {
            const FinalePageWidget::Children &children = pages[choosePageFor(type)]->children();
            for (FinaleWidget *widget : children)
            {
                if (!widget->name().compareWithoutCase(name))
                {
                    return widget;
                }
            }
        }
        return nullptr; // Not found.
    }

    FinaleWidget *makeWidget(fi_obtype_e type, const String &name)
    {
        if (type == FI_ANIM)
        {
            return new FinaleAnimWidget(name);
        }
        if (type == FI_TEXT)
        {
            auto *wi = new FinaleTextWidget(name);
            // Configure the text to use the Page's font and color.
            wi->setPageFont(1)
               .setPageColor(1);
            return wi;
        }
        return nullptr;
    }

#if __CLIENT__
    EventHandler *findEventHandler(const ddevent_t &ev) const
    {
        for (const EventHandler &eh : eventHandlers)
        {
            if (eh.ev.device != ev.device && eh.ev.type != ev.type)
                continue;

            switch (eh.ev.type)
            {
            case E_TOGGLE:
                if (eh.ev.toggle.id != ev.toggle.id)
                    continue;
                break;

            case E_AXIS:
                if (eh.ev.axis.id != ev.axis.id)
                    continue;
                break;

            case E_ANGLE:
                if (eh.ev.angle.id != ev.angle.id)
                    continue;
                break;

            default:
                App_Error("Internal error: Invalid event template (type=%i) in finale event handler.", int(eh.ev.type));
            }
            return &const_cast<EventHandler &>(eh);
        }
        return nullptr;
    }
#endif // __CLIENT__
};

FinaleInterpreter::FinaleInterpreter(finaleid_t id) : d(new Impl(this, id))
{}

finaleid_t FinaleInterpreter::id() const
{
    return d->id;
}

void FinaleInterpreter::loadScript(const char *script)
{
    DE_ASSERT(script && script[0]);

    d->pages[Anims].reset(new FinalePageWidget);
    d->pages[Texts].reset(new FinalePageWidget);

    // Hide our pages until command interpretation begins.
    d->pages[Anims]->makeVisible(false);
    d->pages[Texts]->makeVisible(false);

    // Take a copy of the script.
    d->script      = Str_Set(Str_NewStd(), script);
    d->scriptBegin = Str_Text(d->script);
    d->cp          = Str_Text(d->script); // Init cursor.

    d->initDefaultState();

    // Locate the start of the script.
    if (d->nextToken())
    {
        // The start of the script may include blocks of event directive
        // commands. These commands are automatically executed in response
        // to their associated events.
        if (!iCmpStrCase(d->token, "OnLoad"))
        {
            d->findBegin();
            for (;;)
            {
                d->nextToken();
                if (!iCmpStrCase(d->token, "}"))
                    goto end_read;

                if (!d->executeCommand(d->token, FID_ONLOAD))
                    App_Error("FinaleInterpreter::LoadScript: Unknown error"
                              "occured executing directive \"OnLoad\".");
            }
            d->findEnd();
end_read:

            // Skip over any trailing whitespace to position the read cursor
            // on the first token.
            while (*d->cp && isspace(*d->cp)) { d->cp++; }

            // Calculate the new script entry point and restore default state.
            d->scriptBegin = Str_Text(d->script) + (d->cp - Str_Text(d->script));
            d->cp          = d->scriptBegin;
            d->initDefaultState();
        }
    }

    // Any hooks?
    DoomsdayApp::plugins().callAllHooks(HOOK_FINALE_SCRIPT_BEGIN, d->id);
}

void FinaleInterpreter::resume()
{
    if (!d->flags.suspended) return;

    d->flags.suspended = false;
    d->pages[Anims]->pause(false);
    d->pages[Texts]->pause(false);
    // Do we need to unhide any pages?
    if (d->cmdExecuted)
    {
        d->pages[Anims]->makeVisible();
        d->pages[Texts]->makeVisible();
    }
}

void FinaleInterpreter::suspend()
{
    LOG_AS("FinaleInterpreter");

    if (d->flags.suspended) return;

    d->flags.suspended = true;
    // While suspended, all pages will be paused and hidden.
    d->pages[Anims]->pause();
    d->pages[Anims]->makeVisible(false);
    d->pages[Texts]->pause();
    d->pages[Texts]->makeVisible(false);
}

void FinaleInterpreter::terminate()
{
    d->stop();
#ifdef __CLIENT__
    d->eventHandlers.clear();
#endif
    d->releaseScript();
}

bool FinaleInterpreter::isMenuTrigger() const
{
    if (d->flags.paused || d->flags.can_skip)
    {
        // We want events to be used for unpausing/skipping.
        return false;
    }
    // If skipping is not allowed, we should show the menu, too.
    return (d->flags.show_menu != 0);
}

bool FinaleInterpreter::isSuspended() const
{
    return (d->flags.suspended != 0);
}

void FinaleInterpreter::allowSkip(bool yes)
{
    d->flags.can_skip = yes;
}

bool FinaleInterpreter::canSkip() const
{
    return (d->flags.can_skip != 0);
}

bool FinaleInterpreter::commandExecuted() const
{
    return d->cmdExecuted;
}

static bool runOneTick(FinaleInterpreter &fi)
{
    ddhook_finale_script_ticker_paramaters_t parm;
    de::zap(parm);
    parm.runTick = true;
    parm.canSkip = fi.canSkip();
    DoomsdayApp::plugins().callAllHooks(HOOK_FINALE_SCRIPT_TICKER, fi.id(), &parm);
    return parm.runTick;
}

bool FinaleInterpreter::runTicks(timespan_t timeDelta, bool processCommands)
{
    LOG_AS("FinaleInterpreter");

    // All pages tick unless paused.
    page(Anims).runTicks(timeDelta);
    page(Texts).runTicks(timeDelta);

    if (!processCommands)   return false;
    if (d->flags.stopped)   return false;
    if (d->flags.suspended) return false;

    d->timer++;

    if (!runOneTick(*this))
        return false;

    // If waiting do not execute commands.
    if (d->wait && --d->wait)
        return false;

    // If paused there is nothing further to do.
    if (d->flags.paused)
        return false;

    // If waiting on a text to finish typing, do nothing.
    if (d->waitText)
    {
        if (!d->waitText->animationComplete())
            return false;

        d->waitText = nullptr;
    }

    // Waiting for an animation to reach its end?
    if (d->waitAnim)
    {
        if (!d->waitAnim->animationComplete())
            return false;

        d->waitAnim = nullptr;
    }

    // Execute commands until a wait time is set or we reach the end of
    // the script. If the end is reached, the finale really ends (terminates).
    bool foundEnd = false;
    while (!d->gotoEnd && !d->wait && !d->waitText && !d->waitAnim && !foundEnd)
    {
        foundEnd = d->executeNextCommand();
    }
    return (d->gotoEnd || (foundEnd && d->flags.can_skip));
}

bool FinaleInterpreter::skip()
{
    LOG_AS("FinaleInterpreter");

    if (d->waitText && d->flags.can_skip && !d->flags.paused)
    {
        // Instead of skipping, just complete the text.
        d->waitText->accelerate();
        return true;
    }

    // Stop waiting for objects.
    d->waitText = nullptr;
    d->waitAnim = nullptr;
    if (d->flags.paused)
    {
        d->flags.paused = false;
        d->wait = 0;
        return true;
    }

    if (d->flags.can_skip)
    {
        d->skipping = true; // Start skipping ahead.
        d->wait     = 0;
        return true;
    }

    return (d->flags.eat_events != 0);
}

bool FinaleInterpreter::skipToMarker(const String &marker)
{
    LOG_AS("FinaleInterpreter");

    if (marker.isEmpty()) return false;

    d->gotoTarget = marker;
    d->gotoSkip   = true;    // Start skipping until the marker is found.
    d->wait       = 0;       // Stop any waiting.
    d->waitText   = nullptr;
    d->waitAnim   = nullptr;

    // Rewind the script so we can jump anywhere.
    d->cp = d->scriptBegin;
    return true;
}

bool FinaleInterpreter::skipInProgress() const
{
    return d->skipNext;
}

bool FinaleInterpreter::lastSkipped() const
{
    return d->lastSkipped;
}

int FinaleInterpreter::handleEvent(const ddevent_t &ev)
{
    LOG_AS("FinaleInterpreter");

    if (d->flags.suspended)
        return false;

    // During the first ~second disallow all events/skipping.
    if (d->timer < 20)
        return false;

    if (!netState.isClient)
    {
#ifdef __CLIENT__
        // Any handlers for this event?
        if (IS_KEY_DOWN(&ev))
        {
            if (Impl::EventHandler *eh = d->findEventHandler(ev))
            {
                skipToMarker(eh->gotoMarker);

                // Never eat up events.
                if (IS_TOGGLE_UP(&ev)) return false;

                return (d->flags.eat_events != 0);
            }
        }
#endif
    }

    // If we can't skip, there's no interaction of any kind.
    if (!d->flags.can_skip && !d->flags.paused)
        return false;

    // We are only interested in key/button down presses.
    if (!IS_TOGGLE_DOWN(&ev))
        return false;

#ifdef __CLIENT__
    if (netState.isClient)
    {
        // Request skip from the server.
        Cl_RequestFinaleSkip();
        return true;
    }
#endif
#ifdef __SERVER__
    // Tell clients to skip.
    Sv_Finale(d->id, FINF_SKIP, 0);
#endif
    return skip();
}

#ifdef __CLIENT__
void FinaleInterpreter::addEventHandler(const ddevent_t &evTemplate, const String &gotoMarker)
{
    // Does a handler already exist for this?
    if (d->findEventHandler(evTemplate)) return;

    d->eventHandlers.append(Impl::EventHandler(&evTemplate, gotoMarker));
}

void FinaleInterpreter::removeEventHandler(const ddevent_t &evTemplate)
{
    if (Impl::EventHandler *eh = d->findEventHandler(evTemplate))
    {
        int index = 0;
        while (&d->eventHandlers.at(index) != eh) { index++; }
        d->eventHandlers.removeAt(index);
    }
}
#endif // __CLIENT__

FinalePageWidget &FinaleInterpreter::page(PageIndex index)
{
    if (index >= Anims && index <= Texts)
    {
        DE_ASSERT(d->pages[index]);
        return *d->pages[index];
    }
    throw MissingPageError("FinaleInterpreter::page", "Unknown page #" + String::asText(int(index)));
}

const FinalePageWidget &FinaleInterpreter::page(PageIndex index) const
{
    return const_cast<const FinalePageWidget &>(const_cast<FinaleInterpreter *>(this)->page(index));
}

FinaleWidget *FinaleInterpreter::tryFindWidget(const String &name)
{
    // Perhaps an Anim?
    if (FinaleWidget *found = d->locateWidget(FI_ANIM, name))
    {
        return found;
    }
    // Perhaps a Text?
    if (FinaleWidget *found = d->locateWidget(FI_TEXT, name))
    {
        return found;
    }
    return nullptr;
}

FinaleWidget &FinaleInterpreter::findWidget(fi_obtype_e type, const String &name)
{
    if (FinaleWidget *foundWidget = d->locateWidget(type, name))
    {
        return *foundWidget;
    }
    throw MissingWidgetError("FinaleInterpeter::findWidget", "Failed locating widget for name:'" + name + "'");
}

FinaleWidget &FinaleInterpreter::findOrCreateWidget(fi_obtype_e type, const String &name)
{
    DE_ASSERT(type >= FI_ANIM && type <= FI_TEXT);
    DE_ASSERT(!name.isEmpty());
    if (FinaleWidget *foundWidget = d->locateWidget(type, name))
    {
        return *foundWidget;
    }

    FinaleWidget *newWidget = d->makeWidget(type, name);
    if (!newWidget) throw Error("FinaleInterpreter::findOrCreateWidget", "Failed making widget for type:" + String::asText(int(type)));

    return *page(d->choosePageFor(*newWidget)).addChild(newWidget);
}

void FinaleInterpreter::beginDoSkipMode()
{
    if (!skipInProgress()) return;

    // A conditional skip has been issued.
    // We'll go into DO-skipping mode. skipnext won't be cleared
    // until the matching semicolon is found.
    d->doLevel++;
}

void FinaleInterpreter::gotoEnd()
{
    d->gotoEnd = true;
}

void FinaleInterpreter::pause()
{
    d->flags.paused = true;
    wait(1);
}

void FinaleInterpreter::wait(int ticksToWait)
{
    d->wait = ticksToWait;
}

void FinaleInterpreter::foundSkipHere()
{
    d->skipping = false;
}

void FinaleInterpreter::foundSkipMarker(const String &marker)
{
    // Does it match the current goto torget?
    if (!d->gotoTarget.compareWithoutCase(marker))
    {
        d->gotoSkip = false;
    }
}

int FinaleInterpreter::inTime() const
{
    return d->inTime;
}

void FinaleInterpreter::setInTime(int seconds)
{
    d->inTime = seconds;
}

void FinaleInterpreter::setHandleEvents(bool yes)
{
    d->flags.eat_events = yes;
}

void FinaleInterpreter::setShowMenu(bool yes)
{
    d->flags.show_menu = yes;
}

void FinaleInterpreter::setSkip(bool allowed)
{
    d->flags.can_skip = allowed;
}

void FinaleInterpreter::setSkipNext(bool yes)
{
   d->skipNext = yes;
}

void FinaleInterpreter::setWaitAnim(FinaleAnimWidget *newWaitAnim)
{
    d->waitAnim = newWaitAnim;
}

void FinaleInterpreter::setWaitText(FinaleTextWidget *newWaitText)
{
    d->waitText = newWaitText;
}

/// @note This command is called even when condition-skipping.
DEFFC(Do)
{
    DE_UNUSED(cmd, ops);
    fi.beginDoSkipMode();
}

DEFFC(End)
{
    DE_UNUSED(cmd, ops);
    fi.gotoEnd();
}

static void changePageBackground(FinalePageWidget &page, world::Material *newMaterial)
{
    // If the page does not yet have a background set we must setup the color+alpha.
    if (newMaterial && !page.backgroundMaterial())
    {
        page.setBackgroundTopColorAndAlpha   (Vec4f(1))
            .setBackgroundBottomColorAndAlpha(Vec4f(1));
    }
    page.setBackgroundMaterial(newMaterial);
}

DEFFC(BGMaterial)
{
    DE_UNUSED(cmd);

    // First attempt to resolve as a Values URI (which defines the material URI).
    world::Material *material = nullptr;
    try
    {
        if (ded_value_t *value = DED_Definitions()->getValueByUri(*reinterpret_cast<const res::Uri *>(OP_URI(0))))
        {
            material = &world::Materials::get().material(res::makeUri(value->text));
        }
        else
        {
            material = &world::Materials::get().material(*reinterpret_cast<const res::Uri *>(OP_URI(0)));
        }
    }
    catch (const world::MaterialManifest::MissingMaterialError &)
    {} // Ignore this error.
    catch (const Resources::MissingResourceManifestError &)
    {} // Ignore this error.

    changePageBackground(fi.page(FinaleInterpreter::Anims), material);
}

DEFFC(NoBGMaterial)
{
    DE_UNUSED(cmd, ops);
    changePageBackground(fi.page(FinaleInterpreter::Anims), 0);
}

DEFFC(InTime)
{
    DE_UNUSED(cmd);
    fi.setInTime(FRACSECS_TO_TICKS(OP_FLOAT(0)));
}

DEFFC(Tic)
{
    DE_UNUSED(cmd, ops);
    fi.wait();
}

DEFFC(Wait)
{
    DE_UNUSED(cmd);
    fi.wait(FRACSECS_TO_TICKS(OP_FLOAT(0)));
}

DEFFC(WaitText)
{
    DE_UNUSED(cmd);
    fi.setWaitText(&fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>());
}

DEFFC(WaitAnim)
{
    DE_UNUSED(cmd);
    fi.setWaitAnim(&fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>());
}

DEFFC(Color)
{
    DE_UNUSED(cmd);
    fi.page(FinaleInterpreter::Anims)
            .setBackgroundTopColor   (Vec3f(OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2)), fi.inTime())
            .setBackgroundBottomColor(Vec3f(OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2)), fi.inTime());
}

DEFFC(ColorAlpha)
{
    DE_UNUSED(cmd);
    fi.page(FinaleInterpreter::Anims)
            .setBackgroundTopColorAndAlpha   (Vec4f(OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime())
            .setBackgroundBottomColorAndAlpha(Vec4f(OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime());
}

DEFFC(Pause)
{
    DE_UNUSED(cmd, ops);
    fi.pause();
}

DEFFC(CanSkip)
{
    DE_UNUSED(cmd, ops);
    fi.setSkip(true);
}

DEFFC(NoSkip)
{
    DE_UNUSED(cmd, ops);
    fi.setSkip(false);
}

DEFFC(SkipHere)
{
    DE_UNUSED(cmd, ops);
    fi.foundSkipHere();
}

DEFFC(Events)
{
    DE_UNUSED(cmd, ops);
    fi.setHandleEvents();
}

DEFFC(NoEvents)
{
    DE_UNUSED(cmd, ops);
    fi.setHandleEvents(false);
}

DEFFC(OnKey)
{
#ifdef __CLIENT__
    DE_UNUSED(cmd);

    // Construct a template event for this handler.
    ddevent_t ev; de::zap(ev);
    ev.device = IDEV_KEYBOARD;
    ev.type   = E_TOGGLE;
    ev.toggle.id    = DD_GetKeyCode(OP_CSTRING(0));
    ev.toggle.state = ETOG_DOWN;

    fi.addEventHandler(ev, OP_CSTRING(1));
#else
    DE_UNUSED(cmd, ops, fi);
#endif
}

DEFFC(UnsetKey)
{
#ifdef __CLIENT__
    DE_UNUSED(cmd);

    // Construct a template event for what we want to "unset".
    ddevent_t ev; de::zap(ev);
    ev.device = IDEV_KEYBOARD;
    ev.type   = E_TOGGLE;
    ev.toggle.id    = DD_GetKeyCode(OP_CSTRING(0));
    ev.toggle.state = ETOG_DOWN;

    fi.removeEventHandler(ev);
#else
    DE_UNUSED(cmd, ops, fi);
#endif
}

DEFFC(If)
{
    DE_UNUSED(cmd, ops);
    LOG_AS("FIC_If");

    const char *token = OP_CSTRING(0);
    bool val          = false;

    // Built-in conditions.
    if (!iCmpStrCase(token, "netgame"))
    {
        val = netState.netGame;
    }
    else if (!iCmpStrNCase(token, "mode:", 5))
    {
        if (App_GameLoaded())
            val = !String(token + 5).compareWithoutCase(App_CurrentGame().id());
        else
            val = 0;
    }
    // Any hooks?
    else if (Plug_CheckForHook(HOOK_FINALE_EVAL_IF))
    {
        ddhook_finale_script_evalif_paramaters_t p; de::zap(p);
        p.token     = token;
        p.returnVal = 0;
        if (DoomsdayApp::plugins().callAllHooks(HOOK_FINALE_EVAL_IF, fi.id(), (void *) &p))
        {
            val = p.returnVal;
            LOG_SCR_XVERBOSE("HOOK_FINALE_EVAL_IF: %s => %i", token << val);
        }
        else
        {
            LOG_SCR_XVERBOSE("HOOK_FINALE_EVAL_IF: no hook (for %s)", token);
        }
    }
    else
    {
        LOG_SCR_WARNING("Unknown condition '%s'") << token;
    }

    // Skip the next command if the value is false.
    fi.setSkipNext(!val);
}

DEFFC(IfNot)
{
    FIC_If(cmd, ops, fi);
    fi.setSkipNext(!fi.skipInProgress());
}

/// @note The only time the ELSE condition does not skip is immediately after a skip.
DEFFC(Else)
{
    DE_UNUSED(cmd, ops);
    fi.setSkipNext(!fi.lastSkipped());
}

DEFFC(GoTo)
{
    DE_UNUSED(cmd);
    fi.skipToMarker(OP_CSTRING(0));
}

DEFFC(Marker)
{
    DE_UNUSED(cmd);
    fi.foundSkipMarker(OP_CSTRING(0));
}

DEFFC(Delete)
{
    DE_UNUSED(cmd);
    delete fi.tryFindWidget(OP_CSTRING(0));
}

DEFFC(Image)
{
    DE_UNUSED(cmd);
    LOG_AS("FIC_Image");

    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    anim.clearAllFrames();

#ifdef __CLIENT__
    const char *name  = OP_CSTRING(1);
    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);

    if (rawtex_t *rawTex = ClientResources::get().declareRawTexture(lumpNum))
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_RAW, -1, &rawTex->lumpNum, 0, false);
        return;
    }

    LOG_SCR_WARNING("Missing lump '%s'") << name;
#endif
}

DEFFC(ImageAt)
{
    DE_UNUSED(cmd);
    LOG_AS("FIC_ImageAt");

    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    float x = OP_FLOAT(1);
    float y = OP_FLOAT(2);

    anim.clearAllFrames()
        .setOrigin(Vec2f(x, y));

#ifdef __CLIENT__
    const char *name  = OP_CSTRING(3);
    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);

    if (rawtex_t *rawTex = App_Resources().declareRawTexture(lumpNum))
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_RAW, -1, &rawTex->lumpNum, 0, false);
        return;
    }

    LOG_SCR_WARNING("Missing lump '%s'") << name;
#endif
}

#ifdef __CLIENT__
static DGLuint loadAndPrepareExtTexture(const char *fileName)
{
    image_t image;
    DGLuint glTexName = 0;

    if (GL_LoadExtImage(image, fileName, LGM_NORMAL))
    {
        // Loaded successfully and converted accordingly.
        // Upload the image to GL.
        glTexName = GL_NewTextureWithParams(
            ( image.pixelSize == 2 ? DGL_LUMINANCE_PLUS_A8 :
              image.pixelSize == 3 ? DGL_RGB :
              image.pixelSize == 4 ? DGL_RGBA : DGL_LUMINANCE ),
            image.size.x, image.size.y, image.pixels,
            (image.size.x < 128 && image.size.y < 128? TXCF_NO_COMPRESSION : 0),
            0, GL_LINEAR, GL_LINEAR, 0 /*no anisotropy*/,
            GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        Image_ClearPixelData(image);
    }

    return glTexName;
}
#endif // __CLIENT__

DEFFC(XImage)
{
    DE_UNUSED(cmd);

    LOG_AS("FIC_XImage");

    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
#ifdef __CLIENT__
    const char *fileName  = OP_CSTRING(1);
#endif

    anim.clearAllFrames();

#ifdef __CLIENT__
    // Load the external resource.
    if (DGLuint tex = loadAndPrepareExtTexture(fileName))
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_XIMAGE, -1, &tex, 0, false);
    }
    else
    {
        LOG_SCR_WARNING("Missing graphic '%s'") << fileName;
    }
#endif // __CLIENT__
}

DEFFC(Patch)
{
    DE_UNUSED(cmd);
    LOG_AS("FIC_Patch");

    FinaleAnimWidget &anim  = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    const char *encodedName = OP_CSTRING(3);

    anim.setOrigin(Vec2f(OP_FLOAT(1), OP_FLOAT(2)));
    anim.clearAllFrames();

    patchid_t patchId = R_DeclarePatch(encodedName);
    if (patchId)
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_PATCH, -1, (void *)&patchId, 0, 0);
    }
    else
    {
        LOG_SCR_WARNING("Missing Patch '%s'") << encodedName;
    }
}

DEFFC(SetPatch)
{
    DE_UNUSED(cmd);
    LOG_AS("FIC_SetPatch");

    FinaleAnimWidget &anim  = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    const char *encodedName = OP_CSTRING(1);

    patchid_t patchId = R_DeclarePatch(encodedName);
    if (patchId == 0)
    {
        LOG_SCR_WARNING("Missing Patch '%s'") << encodedName;
        return;
    }

    if (!anim.frameCount())
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_PATCH, -1, (void *)&patchId, 0, false);
        return;
    }

    // Convert the first frame.
    FinaleAnimWidget::Frame *f = anim.allFrames().first();
    f->type  = FinaleAnimWidget::Frame::PFT_PATCH;
    f->texRef.patch = patchId;
    f->tics  = -1;
    f->sound = 0;
}

DEFFC(ClearAnim)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        if (FinaleAnimWidget *anim = maybeAs<FinaleAnimWidget>(wi))
        {
            anim->clearAllFrames();
        }
    }
}

DEFFC(Anim)
{
    DE_UNUSED(cmd);
    LOG_AS("FIC_Anim");

    FinaleAnimWidget &anim  = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    const char *encodedName = OP_CSTRING(1);
    const int tics          = FRACSECS_TO_TICKS(OP_FLOAT(2));

    patchid_t patchId = R_DeclarePatch(encodedName);
    if (!patchId)
    {
        LOG_SCR_WARNING("Patch '%s' not found") << encodedName;
        return;
    }

    anim.newFrame(FinaleAnimWidget::Frame::PFT_PATCH, tics, (void *)&patchId, 0, false);
}

DEFFC(AnimImage)
{
    DE_UNUSED(cmd);
    LOG_AS("FIC_AnimImage");

    FinaleAnimWidget &anim  = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();

#ifdef __CLIENT__
    const char *encodedName = OP_CSTRING(1);
    const int tics          = FRACSECS_TO_TICKS(OP_FLOAT(2));
    lumpnum_t lumpNum       = App_FileSystem().lumpNumForName(encodedName);
    if (rawtex_t *rawTex = App_Resources().declareRawTexture(lumpNum))
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_RAW, tics, &rawTex->lumpNum, 0, false);
        return;
    }
    LOG_SCR_WARNING("Lump '%s' not found") << encodedName;
#else
    DE_UNUSED(anim);
#endif
}

DEFFC(Repeat)
{
    DE_UNUSED(cmd);
    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    anim.setLooping();
}

DEFFC(StateAnim)
{
    DE_UNUSED(cmd);
    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
#if !defined(__CLIENT__)
    DE_UNUSED(anim);
#endif

    // Animate N states starting from the given one.
    dint stateId = DED_Definitions()->getStateNum(OP_CSTRING(1));
    for (dint count = OP_INT(2); count > 0 && stateId > 0; count--)
    {
        state_t *st = &runtimeDefs.states[stateId];
#ifdef __CLIENT__
        spriteinfo_t sinf;
        R_GetSpriteInfo(st->sprite, st->frame & 0x7fff, &sinf);
        anim.newFrame(FinaleAnimWidget::Frame::PFT_MATERIAL, (st->tics <= 0? 1 : st->tics), sinf.material, 0, sinf.flip);
#endif

        // Go to the next state.
        stateId = st->nextState;
    }
}

DEFFC(PicSound)
{
    DE_UNUSED(cmd);
    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();
    const int sound        = DED_Definitions()->getSoundNum(OP_CSTRING(1));

    if (!anim.frameCount())
    {
        anim.newFrame(FinaleAnimWidget::Frame::PFT_MATERIAL, -1, 0, sound, false);
        return;
    }

    anim.allFrames().at(anim.frameCount() - 1)->sound = sound;
}

DEFFC(ObjectOffX)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setOriginX(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectOffY)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setOriginY(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectOffZ)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setOriginZ(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectRGB)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        Vec3f const color(OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3));
        if (FinaleTextWidget *text = maybeAs<FinaleTextWidget>(wi))
        {
            text->setColor(color, fi.inTime());
        }
        if (FinaleAnimWidget *anim = maybeAs<FinaleAnimWidget>(wi))
        {
            // This affects all the colors.
            anim->setColor         (color, fi.inTime())
                 .setEdgeColor     (color, fi.inTime())
                 .setOtherColor    (color, fi.inTime())
                 .setOtherEdgeColor(color, fi.inTime());
        }
    }
}

DEFFC(ObjectAlpha)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        const float alpha = OP_FLOAT(1);
        if (FinaleTextWidget *text = maybeAs<FinaleTextWidget>(wi))
        {
            text->setAlpha(alpha, fi.inTime());
        }
        if (FinaleAnimWidget *anim = maybeAs<FinaleAnimWidget>(wi))
        {
            anim->setAlpha     (alpha, fi.inTime())
                 .setOtherAlpha(alpha, fi.inTime());
        }
    }
}

DEFFC(ObjectScaleX)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setScaleX(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScaleY)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setScaleY(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScaleZ)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setScaleZ(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScale)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setScaleX(OP_FLOAT(1), fi.inTime())
           .setScaleY(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScaleXY)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setScaleX(OP_FLOAT(1), fi.inTime())
           .setScaleY(OP_FLOAT(2), fi.inTime());
    }
}

DEFFC(ObjectScaleXYZ)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setScale(Vec3f(OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime());
    }
}

DEFFC(ObjectAngle)
{
    DE_UNUSED(cmd);
    if (FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0)))
    {
        wi->setAngle(OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(Rect)
{
    DE_UNUSED(cmd);
    FinaleAnimWidget &anim = fi.findOrCreateWidget(FI_ANIM, OP_CSTRING(0)).as<FinaleAnimWidget>();

    /// @note We may be converting an existing Pic to a Rect, so re-init the expected
    /// default state accordingly.

    anim.clearAllFrames()
        .resetAllColors()
        .setLooping(false) // Yeah?
        .setOrigin(Vec3f(OP_FLOAT(1), OP_FLOAT(2), 0))
        .setScale(Vec3f(OP_FLOAT(3), OP_FLOAT(4), 1));
}

DEFFC(FillColor)
{
    DE_UNUSED(cmd);
    FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0));
    if (!wi || !is<FinaleAnimWidget>(wi)) return;
    FinaleAnimWidget &anim = wi->as<FinaleAnimWidget>();

    // Which colors to modify?
    int which = 0;
    if (!iCmpStrCase(OP_CSTRING(1), "top"))         which |= 1;
    else if (!iCmpStrCase(OP_CSTRING(1), "bottom")) which |= 2;
    else                                         which = 3;

    Vec4f color;
    for (int i = 0; i < 4; ++i)
    {
        color[i] = OP_FLOAT(2 + i);
    }

    if (which & 1)
        anim.setColorAndAlpha(color, fi.inTime());
    if (which & 2)
        anim.setOtherColorAndAlpha(color, fi.inTime());
}

DEFFC(EdgeColor)
{
    DE_UNUSED(cmd);
    FinaleWidget *wi = fi.tryFindWidget(OP_CSTRING(0));
    if (!wi || !is<FinaleAnimWidget>(wi)) return;
    FinaleAnimWidget &anim = wi->as<FinaleAnimWidget>();

    // Which colors to modify?
    int which = 0;
    if (!iCmpStrCase(OP_CSTRING(1), "top"))         which |= 1;
    else if (!iCmpStrCase(OP_CSTRING(1), "bottom")) which |= 2;
    else                                        which = 3;

    Vec4f color;
    for (int i = 0; i < 4; ++i)
    {
        color[i] = OP_FLOAT(2 + i);
    }

    if (which & 1)
        anim.setEdgeColorAndAlpha(color, fi.inTime());
    if (which & 2)
        anim.setOtherEdgeColorAndAlpha(color, fi.inTime());
}

DEFFC(OffsetX)
{
    DE_UNUSED(cmd);
    fi.page(FinaleInterpreter::Anims).setOffsetX(OP_FLOAT(0), fi.inTime());
}

DEFFC(OffsetY)
{
    DE_UNUSED(cmd);
    fi.page(FinaleInterpreter::Anims).setOffsetY(OP_FLOAT(0), fi.inTime());
}

DEFFC(Sound)
{
    DE_UNUSED(cmd, fi);
    S_LocalSound(DED_Definitions()->getSoundNum(OP_CSTRING(0)), nullptr);
}

DEFFC(SoundAt)
{
    DE_UNUSED(cmd, fi);
    const dint soundId = DED_Definitions()->getSoundNum(OP_CSTRING(0));
    const dfloat vol   = de::min(OP_FLOAT(1), 1.f);
    S_LocalSoundAtVolume(soundId, nullptr, vol);
}

DEFFC(SeeSound)
{
    DE_UNUSED(cmd, fi);
    dint num = DED_Definitions()->getMobjNum(OP_CSTRING(0));
    if (num >= 0 && ::runtimeDefs.mobjInfo[num].seeSound > 0)
    {
        S_LocalSound(runtimeDefs.mobjInfo[num].seeSound, nullptr);
    }
}

DEFFC(DieSound)
{
    DE_UNUSED(cmd, fi);
    dint num = DED_Definitions()->getMobjNum(OP_CSTRING(0));
    if (num >= 0 && ::runtimeDefs.mobjInfo[num].deathSound > 0)
    {
        S_LocalSound(runtimeDefs.mobjInfo[num].deathSound, nullptr);
    }
}

DEFFC(Music)
{
    DE_UNUSED(cmd, ops, fi);
    S_StartMusic(OP_CSTRING(0), true);
}

DEFFC(MusicOnce)
{
    DE_UNUSED(cmd, ops, fi);
    S_StartMusic(OP_CSTRING(0), false);
}

DEFFC(Filter)
{
    DE_UNUSED(cmd);
    fi.page(FinaleInterpreter::Texts).setFilterColorAndAlpha(Vec4f(OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime());
}

DEFFC(Text)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();

    text.setText(OP_CSTRING(3))
        .setCursorPos(0) // Restart the text.
        .setOrigin(Vec3f(OP_FLOAT(1), OP_FLOAT(2), 0));
}

DEFFC(TextFromDef)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    int textIdx          = DED_Definitions()->getTextNum((char *)OP_CSTRING(3));

    text.setText(textIdx >= 0? DED_Definitions()->text[textIdx].text : "(undefined)")
        .setCursorPos(0) // Restart the type-in animation (if any).
        .setOrigin(Vec3f(OP_FLOAT(1), OP_FLOAT(2), 0));
}

DEFFC(TextFromLump)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();

    text.setOrigin(Vec3f(OP_FLOAT(1), OP_FLOAT(2), 0));

    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(OP_CSTRING(3));
    if (lumpNum >= 0)
    {
        File1 &lump            = App_FileSystem().lump(lumpNum);
        const uint8_t *rawStr = lump.cache();

        AutoStr *str = AutoStr_NewStd();
        Str_Reserve(str, lump.size() * 2);

        char *out = Str_Text(str);
        for (size_t i = 0; i < lump.size(); ++i)
        {
            char ch = (char)(rawStr[i]);
            if (ch == '\r') continue;
            if (ch == '\n')
            {
                *out++ = '\\';
                *out++ = 'n';
            }
            else
            {
                *out++ = ch;
            }
        }
        lump.unlock();

        text.setText(Str_Text(str));
    }
    else
    {
        text.setText("(not found)");
    }
    text.setCursorPos(0); // Restart.
}

DEFFC(SetText)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setText(OP_CSTRING(1));
}

DEFFC(SetTextDef)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    int textIdx            = DED_Definitions()->getTextNum((char *)OP_CSTRING(1));

    text.setText(textIdx >= 0? DED_Definitions()->text[textIdx].text : "(undefined)");
}

DEFFC(DeleteText)
{
    DE_UNUSED(cmd);
    delete fi.tryFindWidget(OP_CSTRING(0));
}

DEFFC(PredefinedColor)
{
    DE_UNUSED(cmd);
    fi.page(FinaleInterpreter::Texts)
            .setPredefinedColor(de::clamp(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_COLORS) - 1,
                                Vec3f(OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime());
    fi.page(FinaleInterpreter::Anims)
            .setPredefinedColor(de::clamp(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_COLORS) - 1,
                                Vec3f(OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime());
}

DEFFC(PredefinedFont)
{
#ifdef __CLIENT__
    DE_UNUSED(cmd);
    LOG_AS("FIC_PredefinedFont");

    const fontid_t fontNum = Fonts_ResolveUri(OP_URI(1));
    if (fontNum)
    {
        const int idx = de::clamp(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_FONTS) - 1;
        fi.page(FinaleInterpreter::Anims).setPredefinedFont(idx, fontNum);
        fi.page(FinaleInterpreter::Texts).setPredefinedFont(idx, fontNum);
        return;
    }

    AutoStr *fontPath = Uri_ToString(OP_URI(1));
    LOG_SCR_WARNING("Unknown font '%s'") << Str_Text(fontPath);
#else
    DE_UNUSED(cmd, ops, fi);
#endif
}

DEFFC(TextRGB)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setColor(Vec3f(OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3)), fi.inTime());
}

DEFFC(TextAlpha)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setAlpha(OP_FLOAT(1), fi.inTime());
}

DEFFC(TextOffX)
{
    DE_UNUSED(cmd);
    FinaleWidget &wi = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0));
    wi.setOriginX(OP_FLOAT(1), fi.inTime());
}

DEFFC(TextOffY)
{
    DE_UNUSED(cmd);
    FinaleWidget &wi = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0));
    wi.setOriginY(OP_FLOAT(1), fi.inTime());
}

DEFFC(TextCenter)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setAlignment(text.alignment() & ~(ALIGN_LEFT | ALIGN_RIGHT));
}

DEFFC(TextNoCenter)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setAlignment(text.alignment() | ALIGN_LEFT);
}

DEFFC(TextScroll)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setScrollRate(OP_INT(1));
}

DEFFC(TextPos)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setCursorPos(OP_INT(1));
}

DEFFC(TextRate)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setTypeInRate(OP_INT(1));
}

DEFFC(TextLineHeight)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setLineHeight(OP_FLOAT(1));
}

DEFFC(Font)
{
#ifdef __CLIENT__
    DE_UNUSED(cmd);
    LOG_AS("FIC_Font");

    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    fontid_t fontNum       = Fonts_ResolveUri(OP_URI(1));
    if (fontNum)
    {
        text.setFont(fontNum);
        return;
    }

    AutoStr *fontPath = Uri_ToString(OP_URI(1));
    LOG_SCR_WARNING("Unknown font '%s'") << Str_Text(fontPath);
#else
    DE_UNUSED(cmd, ops, fi);
#endif
}

DEFFC(FontA)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setFont(fi.page(FinaleInterpreter::Texts).predefinedFont(0));
}

DEFFC(FontB)
{
    DE_UNUSED(cmd);
    FinaleTextWidget &text = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0)).as<FinaleTextWidget>();
    text.setFont(fi.page(FinaleInterpreter::Texts).predefinedFont(1));
}

DEFFC(NoMusic)
{
    DE_UNUSED(cmd, ops, fi);
    S_StopMusic();
}

DEFFC(TextScaleX)
{
    DE_UNUSED(cmd);
    FinaleWidget &wi = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0));
    wi.setScaleX(OP_FLOAT(1), fi.inTime());
}

DEFFC(TextScaleY)
{
    DE_UNUSED(cmd);
    FinaleWidget &wi = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0));
    wi.setScaleY(OP_FLOAT(1), fi.inTime());
}

DEFFC(TextScale)
{
    DE_UNUSED(cmd);
    FinaleWidget &wi = fi.findOrCreateWidget(FI_TEXT, OP_CSTRING(0));
    wi.setScaleX(OP_FLOAT(1), fi.inTime())
      .setScaleY(OP_FLOAT(2), fi.inTime());
}

DEFFC(PlayDemo)
{
    /// @todo Demos are not supported at the moment. -jk
#if 0
    // While playing a demo we suspend command interpretation.
    fi.suspend();

    // Start the demo.
    if (!Con_Executef(CMDS_DDAY, true, "playdemo \"%s\"", OP_CSTRING(0)))
    {
        // Demo playback failed. Here we go again...
        fi.resume();
    }
#else
    DE_UNUSED(cmd, ops, fi);
#endif
}

DEFFC(Command)
{
    DE_UNUSED(cmd, fi);
    Con_Executef(CMDS_SCRIPT, false, "%s", OP_CSTRING(0));
}

DEFFC(ShowMenu)
{
    DE_UNUSED(cmd, ops);
    fi.setShowMenu();
}

DEFFC(NoShowMenu)
{
    DE_UNUSED(cmd, ops);
    fi.setShowMenu(false);
}
