/** @file finaleinterpreter.cpp  InFine animation system Finale script interpreter.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include <QList>
#include <de/memory.h>
#include <de/timer.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>

#include "de_base.h"
#include "ui/finaleinterpreter.h"

#include "de_filesys.h"
#include "de_ui.h"

#include "dd_main.h"
#include "dd_def.h"
#include "Game"

#include "api_material.h"
#include "api_render.h"
#include "api_resource.h"

#include "audio/s_main.h"
#include "network/net_main.h"

#ifdef __CLIENT__
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

enum fi_operand_type_t
{
    FVT_INT,
    FVT_FLOAT,
    FVT_SCRIPT_STRING,
    FVT_OBJECT,
    FVT_URI
};

static fi_operand_type_t operandTypeForCharCode(char code)
{
    switch(code)
    {
    case 'i': return FVT_INT;
    case 'f': return FVT_FLOAT;
    case 's': return FVT_SCRIPT_STRING;
    case 'o': return FVT_OBJECT;
    case 'u': return FVT_URI;

    default:
        App_Error("Error: operandTypeForCharCode: Unknown char-code %c", code);
        exit(1); // Unreachable.
    }
}

// Helper macro for accessing the value of an operand.
#define OP_INT(n)           (ops[n].data.integer)
#define OP_FLOAT(n)         (ops[n].data.flt)
#define OP_CSTRING(n)       (ops[n].data.cstring)
#define OP_OBJECT(n)        (ops[n].data.ob)
#define OP_URI(n)           (ops[n].data.uri)

struct fi_operand_t
{
    fi_operand_type_t type;
    union {
        int         integer;
        float       flt;
        char const *cstring;
        fi_object_t *ob;
        uri_s       *uri;
    } data;
};

// Helper macro for defining infine command functions.
#define DEFFC(name) void FIC_##name(command_t const &cmd, fi_operand_t const *ops, FinaleInterpreter &fi)

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
    char const *token;
    char const *operands;

    typedef void (*Func) (command_t const &, fi_operand_t const *, FinaleInterpreter &);
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
static command_t const *findCommand(char const *name)
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
        { "del",        "o", FIC_Delete }, // del (ob)
        { "x",          "of", FIC_ObjectOffX }, // x (ob) (x)
        { "y",          "of", FIC_ObjectOffY }, // y (ob) (y)
        { "z",          "of", FIC_ObjectOffZ }, // z (ob) (z)
        { "sx",         "of", FIC_ObjectScaleX }, // sx (ob) (x)
        { "sy",         "of", FIC_ObjectScaleY }, // sy (ob) (y)
        { "sz",         "of", FIC_ObjectScaleZ }, // sz (ob) (z)
        { "scale",      "of", FIC_ObjectScale }, // scale (ob) (factor)
        { "scalexy",    "off", FIC_ObjectScaleXY }, // scalexy (ob) (x) (y)
        { "scalexyz",   "offf", FIC_ObjectScaleXYZ }, // scalexyz (ob) (x) (y) (z)
        { "rgb",        "offf", FIC_ObjectRGB }, // rgb (ob) (r) (g) (b)
        { "alpha",      "of", FIC_ObjectAlpha }, // alpha (ob) (alpha)
        { "angle",      "of", FIC_ObjectAngle }, // angle (ob) (degrees)

        // Rects
        { "rect",       "sffff", FIC_Rect }, // rect (hndl) (x) (y) (w) (h)
        { "fillcolor",  "osffff", FIC_FillColor }, // fillcolor (ob) (top/bottom/both) (r) (g) (b) (a)
        { "edgecolor",  "osffff", FIC_EdgeColor }, // edgecolor (ob) (top/bottom/both) (r) (g) (b) (a)

        // Pics
        { "image",      "ss", FIC_Image }, // image (id) (raw-image-lump)
        { "imageat",    "sffs", FIC_ImageAt }, // imageat (id) (x) (y) (raw)
        { "ximage",     "ss", FIC_XImage }, // ximage (id) (ext-gfx-filename)
        { "patch",      "sffs", FIC_Patch }, // patch (id) (x) (y) (patch)
        { "set",        "ss", FIC_SetPatch }, // set (id) (lump)
        { "clranim",    "o", FIC_ClearAnim }, // clranim (ob)
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
        { "delpic",     "o", FIC_Delete }, // delpic (ob)

        // Deprecated Text commands
        { "deltext",    "o", FIC_DeleteText }, // deltext (ob)
        { "textrgb",    "sfff", FIC_TextRGB }, // textrgb (id) (r) (g) (b)
        { "textalpha",  "sf", FIC_TextAlpha }, // textalpha (id) (alpha)
        { "tx",         "sf", FIC_TextOffX }, // tx (id) (x)
        { "ty",         "sf", FIC_TextOffY }, // ty (id) (y)
        { "tsx",        "sf", FIC_TextScaleX }, // tsx (id) (x)
        { "tsy",        "sf", FIC_TextScaleY }, // tsy (id) (y)
        { "textscale",  "sf", FIC_TextScale }, // textscale (id) (x) (y)

        { nullptr, 0, nullptr } // Terminate.
    };
    for(size_t i = 0; commands[i].token; ++i)
    {
        command_t const *cmd = &commands[i];
        if(!qstricmp(cmd->token, name))
        {
            return cmd;
        }
    }
    return 0; // Not found.
}

DENG2_PIMPL(FinaleInterpreter)
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

    finaleid_t id;       ///< Unique identifier.
    ddstring_t *script;  ///< The script to be interpreted.
    char *scriptBegin;   ///< Beginning of the script (after any directive blocks).
    char const *cp;      ///< Current position in the script.

    /// Script token read/parse buffer.
    char token[MAX_TOKEN_LENGTH];

#ifdef __CLIENT__
    struct EventHandler
    {
        ddevent_t ev; // Template.
        fi_objectname_t gotoMarker;

        explicit EventHandler(ddevent_t const *evTemplate = nullptr,
                              char const *gotoMarker      = nullptr) {
            std::memcpy(&ev, &evTemplate, sizeof(ev));
            setGotoMarker(gotoMarker);
        }
        EventHandler(EventHandler const &other) {
            std::memcpy(&ev, &other.ev, sizeof(ev));
            setGotoMarker(other.gotoMarker);
        }

        void setGotoMarker(char const *newGotoMarker) {
            de::zap(gotoMarker);
            if(newGotoMarker) {
                dd_snprintf(gotoMarker, FI_NAME_MAX_LENGTH, "%s", newGotoMarker);
            }
        }
    };

    typedef QList<EventHandler> EventHandlers;
    EventHandlers eventHandlers;
#endif // __CLIENT__

    struct KnownObjects
    {
        struct Item
        {
            fi_objectname_t name; ///< Unique among objects of the same type.
            fi_objectid_t   id;

            explicit Item(fi_objectid_t id = 0, char const *name = nullptr)
                : id(id) {
                setName(name);
            }
            Item(Item const &other) : id(other.id) {
                setName(other.name);
            }

            void setName(char const *newName = nullptr) {
                de::zap(name);
                if(newName) {
                    dd_snprintf(name, FI_NAME_MAX_LENGTH, "%s", newName);
                }
            }
        };
        typedef QList<Item> AllItems;
        AllItems items;

        ~KnownObjects() { clear(); }

        void clear()
        {
            items.clear();
        }

        /**
         * @param type  Type of object if known. Use FI_NONE for priority-based search.
         */
        fi_objectid_t toId(char const *name, fi_obtype_e type = FI_NONE)
        {
            DENG2_ASSERT(name && name[0]);

            if(type == FI_NONE)
            {
                // First check all pics.
                fi_objectid_t id = toId(name, FI_PIC);
                // Then check text objects.
                if(!id) id = toId(name, FI_TEXT);
                return id;
            }

            for(Item const &item : items)
            {
                if(qstricmp(item.name, name)) continue;

                fi_object_t *ob = FI_Object(item.id);
                DENG2_ASSERT(ob);
                if(ob->type == type)
                {
                    return item.id;
                }
            }
            return 0;
        }

        int indexOf(fi_object_t *ob)
        {
            if(ob)
            {
                for(int i = 0; i < items.count(); ++i)
                {
                    if(items.at(i).id == ob->id) return i + 1; // 1-based
                }
            }
            return 0;
        }

        /**
         * @note Does not check if the object already exists in this scope.
         */
        fi_object_t *add(fi_object_t *ob, char const *name)
        {
            DENG2_ASSERT(ob && name);
            items.append(Item(ob->id, name));
            return ob;
        }

        /**
         * @pre There is at most one reference to the object in this scope.
         */
        fi_object_t *remove(fi_object_t *ob)
        {
            if(int index = indexOf(ob))
            {
                items.removeAt(index);
            }
            return ob;
        }
    } names;

    /// Pages on which objects created by this interpeter are visible.
    fi_page_t *pages[2];

    /// Set to true after first command is executed.
    bool cmdExecuted;
    bool skipping;
    bool lastSkipped;
    bool gotoSkip;
    bool gotoEnd;
    bool skipNext;

    int doLevel; ///< Level of DO-skipping.

    uint timer;
    int wait;
    int inTime;

    fi_objectname_t gotoTarget;

    fi_object_t *waitText;
    fi_object_t *waitAnim;

    Instance(Public *i, finaleid_t id)
        : Base(i)
        , id         (id)
        , script     (nullptr)
        , scriptBegin(nullptr)
        , cp         (nullptr)
        , cmdExecuted(false)
        , skipping   (false)
        , lastSkipped(false)
        , gotoSkip   (false)
        , gotoEnd    (false)
        , skipNext   (false)
        , doLevel    (0)
        , timer      (0)
        , wait       (0)
        , inTime     (0)
        , waitText   (nullptr)
        , waitAnim   (nullptr)
    {
        de::zap(flags);
        de::zap(token);
        de::zap(pages);
        de::zap(gotoTarget);
    }

    ~Instance()
    {
        stop();
        releaseScript();
        deleteAllObjects();
        FI_DeletePage(pages[Anims]);
        FI_DeletePage(pages[Texts]);
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
        de::zap(gotoTarget);

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
        if(flags.stopped) return;

        flags.stopped = true;
        LOGDEV_SCR_MSG("Finale End - id:%i '%.30s'") << id << scriptBegin;

#ifdef __SERVER__
        if(::isServer && !(FI_ScriptFlags(id) & FF_LOCAL))
        {
            // Tell clients to stop the finale.
            Sv_Finale(id, FINF_END, 0);
        }
#endif

        // Any hooks?
        DD_CallHooks(HOOK_FINALE_SCRIPT_STOP, id, 0);
    }

    bool atEnd() const
    {
        DENG2_ASSERT(script);
        return (cp - Str_Text(script)) >= Str_Length(script);
    }

    void findBegin()
    {
        char const *tok;
        while(!gotoEnd && 0 != (tok = nextToken()) && qstricmp(tok, "{")) {}
    }

    void findEnd()
    {
        char const *tok;
        while(!gotoEnd && 0 != (tok = nextToken()) && qstricmp(tok, "}")) {}
    }

    char const *nextToken()
    {
        // Skip whitespace.
        while(!atEnd() && isspace(*cp)) { cp++; }

        // Have we reached the end?
        if(atEnd()) return nullptr;

        char *out = token;
        if(*cp == '"') // A string?
        {
            for(cp++; !atEnd(); cp++)
            {
                if(*cp == '"')
                {
                    cp++;
                    // Convert double quotes to single ones.
                    if(*cp != '"') break;
                }
                *out++ = *cp;
            }
        }
        else
        {
            while(!isspace(*cp) && !atEnd()) { *out++ = *cp++; }
        }
        *out++ = 0;

        return token;
    }

    /// @return  @c true if the end of the script was reached.
    bool executeNextCommand()
    {
        if(char const *tok = nextToken())
        {
            executeCommand(tok, FID_NORMAL);
            // Time to unhide the object page(s)?
            if(cmdExecuted)
            {
                FIPage_MakeVisible(pages[Anims], true);
                FIPage_MakeVisible(pages[Texts], true);
            }
            return false;
        }
        return true;
    }

    static inline char const *findDefaultValueEnd(char const *str)
    {
        char const *defaultValueEnd;
        for(defaultValueEnd = str; defaultValueEnd && *defaultValueEnd != ')'; defaultValueEnd++)
        {}
        DENG2_ASSERT(defaultValueEnd < str + qstrlen(str));
        return defaultValueEnd;
    }

    static char const *nextOperand(char const *operands)
    {
        if(operands && operands[0])
        {
            // Some operands might include a default value.
            int len = qstrlen(operands);
            if(len > 1 && operands[1] == '(')
            {
                // A default value begins. Find the end.
                return findDefaultValueEnd(operands + 2) + 1;
            }
            return operands + 1;
        }
        return nullptr; // No more operands.
    }

    /// @return Total number of command operands in the control string @a operands.
    static int countCommandOperands(char const *operands)
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
     * @return  Array of @c fi_operand_t else @c nullptr. Must be free'd with M_Free().
     */
    fi_operand_t *prepareCommandOperands(command_t const *cmd, int *count)
    {
        DENG2_ASSERT(cmd);

        char const *origCursorPos = cp;
        int const operandCount    = countCommandOperands(cmd->operands);
        if(operandCount <= 0) return nullptr;

        fi_operand_t *operands = (fi_operand_t *) M_Malloc(sizeof(*operands) * operandCount);
        char const *opRover    = cmd->operands;
        for(fi_operand_t *op = operands; opRover && opRover[0]; opRover = nextOperand(opRover), op++)
        {
            char const charCode = *opRover;

            op->type = operandTypeForCharCode(charCode);
            bool const opHasDefaultValue = (opRover < cmd->operands + (qstrlen(cmd->operands) - 2) && opRover[1] == '(');
            bool const haveValue         = !!nextToken();

            if(!haveValue && !opHasDefaultValue)
            {
                cp = origCursorPos;

                if(operands) M_Free(operands);
                if(count) *count = 0;

                App_Error("prepareCommandOperands: Too few operands for command '%s'.\n", cmd->token);
                return 0; // Unreachable.
            }

            switch(op->type)
            {
            case FVT_INT: {
                char const *valueStr = haveValue? token : nullptr;
                if(!valueStr)
                {
                    // Use the default.
                    int const defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    valueStr = Str_Text(defaultValue);
                }
                op->data.integer = strtol(valueStr, nullptr, 0);
                break; }

            case FVT_FLOAT: {
                char const *valueStr = haveValue? token : nullptr;
                if(!valueStr)
                {
                    // Use the default.
                    int const defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    valueStr = Str_Text(defaultValue);
                }
                op->data.flt = strtod(valueStr, nullptr);
                break; }

            case FVT_SCRIPT_STRING: {
                char const *valueStr = haveValue? token : nullptr;
                int valueLen         = haveValue? qstrlen(token) : 0;
                if(!valueStr)
                {
                    // Use the default.
                    int const defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    valueStr = Str_Text(defaultValue);
                    valueLen = defaultValueLen;
                }
                op->data.cstring = (char *)M_Malloc(valueLen + 1);
                qstrcpy((char *)op->data.cstring, token);
                break; }

            case FVT_OBJECT: {
                char const *obName = haveValue? token : nullptr;
                if(!obName)
                {
                    // Use the default.
                    int const defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    obName = Str_Text(defaultValue);
                }
                op->data.ob = FI_Object(names.toId(obName, FI_NONE));
                break; }

            case FVT_URI: {
                uri_s *uri = Uri_New();
                // Always apply the default as it may contain a default scheme.
                if(opHasDefaultValue)
                {
                    int const defaultValueLen = (findDefaultValueEnd(opRover + 2) - opRover) - 1;
                    AutoStr *defaultValue     = Str_PartAppend(AutoStr_NewStd(), opRover + 2, 0, defaultValueLen);
                    Uri_SetUri2(uri, Str_Text(defaultValue), RC_NULL);
                }
                if(haveValue)
                {
                    Uri_SetUri2(uri, token, RC_NULL);
                }
                op->data.uri = uri;
                break; }

            default: break; // Unreachable.
            }
        }

        if(count) *count = operandCount;

        return operands;
    }

    bool skippingCommand(command_t const *cmd)
    {
        DENG2_ASSERT(cmd);
        if((skipNext && !cmd->flags.when_condition_skipping) ||
           ((skipping || gotoSkip) && !cmd->flags.when_skipping))
        {
            // While not DO-skipping, the condskip has now been done.
            if(!doLevel)
            {
                if(skipNext)
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
    bool executeCommand(char const *commandString, int directive)
    {
        DENG2_ASSERT(commandString);
        bool didSkip = false;

        // Semicolon terminates DO-blocks.
        if(!qstrcmp(commandString, ";"))
        {
            if(doLevel > 0)
            {
                if(--doLevel == 0)
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
        if(command_t const *cmd = findCommand(commandString))
        {
            bool const requiredOperands = (cmd->operands && cmd->operands[0]);

            // Is this command supported for this directive?
            if(directive != 0 && cmd->excludeDirectives != 0 &&
               (cmd->excludeDirectives & directive) == 0)
                App_Error("executeCommand: Command \"%s\" is not supported for directive %i.",
                          cmd->token, directive);

            // Check that there are enough operands.
            /// @todo Dynamic memory allocation during script interpretation should be avoided.
            int numOps        = 0;
            fi_operand_t *ops = nullptr;
            if(!requiredOperands || (ops = prepareCommandOperands(cmd, &numOps)))
            {
                // Should we skip this command?
                if(!(didSkip = skippingCommand(cmd)))
                {
                    // Execute forthwith!
                    cmd->func(*cmd, ops, self);
                }
            }

            if(!didSkip)
            {
                if(gotoEnd)
                {
                    wait = 1;
                }
                else
                {   // Now we've executed the latest command.
                    lastSkipped = false;
                }
            }

            if(ops)
            {
                for(int i = 0; i < numOps; ++i)
                {
                    fi_operand_t *op = &ops[i];
                    switch(op->type)
                    {
                    case FVT_SCRIPT_STRING: M_Free((char *)op->data.cstring); break;
                    case FVT_URI:           Uri_Delete(op->data.uri);         break;

                    default: break;
                    }
                }
                M_Free(ops);
            }
        }

        return !didSkip;
    }

    static inline PageIndex choosePageForObjectType(fi_obtype_e type)
    {
        return (type == FI_TEXT? Texts : Anims);
    }

    void deleteAllObjects()
    {
        for(KnownObjects::Item const &item : names.items)
        {
            FI_DeleteObject(FI_Object(item.id));
        }
        names.clear();
    }

#if __CLIENT__
    EventHandler *findEventHandler(ddevent_t const &ev) const
    {
        for(EventHandler const &eh : eventHandlers)
        {
            if(eh.ev.device != ev.device && eh.ev.type != ev.type)
                continue;

            switch(eh.ev.type)
            {
            case E_TOGGLE:
                if(eh.ev.toggle.id != ev.toggle.id)
                    continue;
                break;

            case E_AXIS:
                if(eh.ev.axis.id != ev.axis.id)
                    continue;
                break;

            case E_ANGLE:
                if(eh.ev.angle.id != ev.angle.id)
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

FinaleInterpreter::FinaleInterpreter(finaleid_t id) : d(new Instance(this, id))
{}

finaleid_t FinaleInterpreter::id() const
{
    return d->id;
}

void FinaleInterpreter::loadScript(char const *script)
{
    DENG2_ASSERT(script && script[0]);

    d->pages[Anims] = FI_NewPage(nullptr);
    d->pages[Texts] = FI_NewPage(nullptr);

    // Hide our pages until command interpretation begins.
    FIPage_MakeVisible(d->pages[Anims], false);
    FIPage_MakeVisible(d->pages[Texts], false);

    // Take a copy of the script.
    d->script      = Str_Set(Str_NewStd(), script);
    d->scriptBegin = Str_Text(d->script);
    d->cp          = Str_Text(d->script); // Init cursor.

    d->initDefaultState();

    // Locate the start of the script.
    if(d->nextToken())
    {
        // The start of the script may include blocks of event directive
        // commands. These commands are automatically executed in response
        // to their associated events.
        if(!qstricmp(d->token, "OnLoad"))
        {
            d->findBegin();
            forever
            {
                d->nextToken();
                if(!qstricmp(d->token, "}"))
                    goto end_read;

                if(!d->executeCommand(d->token, FID_ONLOAD))
                    App_Error("FinaleInterpreter::LoadScript: Unknown error"
                              "occured executing directive \"OnLoad\".");
            }
            d->findEnd();
end_read:

            // Skip over any trailing whitespace to position the read cursor
            // on the first token.
            while(*d->cp && isspace(*d->cp)) { d->cp++; }

            // Calculate the new script entry point and restore default state.
            d->scriptBegin = Str_Text(d->script) + (d->cp - Str_Text(d->script));
            d->cp          = d->scriptBegin;
            d->initDefaultState();
        }
    }

    // Any hooks?
    DD_CallHooks(HOOK_FINALE_SCRIPT_BEGIN, d->id, 0);
}

void FinaleInterpreter::resume()
{
    if(!d->flags.suspended) return;

    d->flags.suspended = false;
    FIPage_Pause(d->pages[Anims], false);
    FIPage_Pause(d->pages[Texts], false);
    // Do we need to unhide any pages?
    if(d->cmdExecuted)
    {
        FIPage_MakeVisible(d->pages[Anims], true);
        FIPage_MakeVisible(d->pages[Texts], true);
    }
}

void FinaleInterpreter::suspend()
{
    LOG_AS("FinaleInterpreter");

    if(d->flags.suspended) return;

    d->flags.suspended = true;
    // While suspended, all pages will be paused and hidden.
    FIPage_Pause(d->pages[Anims], true);
    FIPage_MakeVisible(d->pages[Anims], false);
    FIPage_Pause(d->pages[Texts], true);
    FIPage_MakeVisible(d->pages[Texts], false);
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
    if(d->flags.paused || d->flags.can_skip)
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
    DD_CallHooks(HOOK_FINALE_SCRIPT_TICKER, fi.id(), &parm);
    return parm.runTick;
}

bool FinaleInterpreter::runTicks()
{
    LOG_AS("FinaleInterpreter");

    if(d->flags.stopped || d->flags.suspended)
        return false;

    d->timer++;

    if(!runOneTick(*this))
        return false;

    // If waiting do not execute commands.
    if(d->wait && --d->wait)
        return false;

    // If paused there is nothing further to do.
    if(d->flags.paused)
        return false;

    // If waiting on a text to finish typing, do nothing.
    if(d->waitText && d->waitText->type == FI_TEXT)
    {
        if(!((fidata_text_t *)d->waitText)->animComplete)
            return false;

        d->waitText = nullptr;
    }

    // Waiting for an animation to reach its end?
    if(d->waitAnim && d->waitAnim->type == FI_PIC)
    {
        if(!((fidata_pic_t *)d->waitAnim)->animComplete)
            return false;

        d->waitAnim = nullptr;
    }

    // Execute commands until a wait time is set or we reach the end of
    // the script. If the end is reached, the finale really ends (terminates).
    bool foundEnd = false;
    while(!d->gotoEnd && !d->wait && !d->waitText && !d->waitAnim && !foundEnd)
    {
        foundEnd = d->executeNextCommand();
    }
    return (d->gotoEnd || (foundEnd && d->flags.can_skip));
}

bool FinaleInterpreter::skip()
{
    LOG_AS("FinaleInterpreter");

    if(d->waitText && d->flags.can_skip && !d->flags.paused)
    {
        // Instead of skipping, just complete the text.
        FIData_TextAccelerate(d->waitText);
        return true;
    }

    // Stop waiting for objects.
    d->waitText = nullptr;
    d->waitAnim = nullptr;
    if(d->flags.paused)
    {
        d->flags.paused = false;
        d->wait = 0;
        return true;
    }

    if(d->flags.can_skip)
    {
        d->skipping = true; // Start skipping ahead.
        d->wait     = 0;
        return true;
    }

    return (d->flags.eat_events != 0);
}

bool FinaleInterpreter::skipToMarker(char const *marker)
{
    LOG_AS("FinaleInterpreter");
    DENG2_ASSERT(marker);

    if(!marker[0]) return false;

    de::zap(d->gotoTarget);
    qstrncpy(d->gotoTarget, marker, sizeof(d->gotoTarget) - 1);
    d->gotoSkip = true;    // Start skipping until the marker is found.
    d->wait     = 0;       // Stop any waiting.
    d->waitText = nullptr;
    d->waitAnim = nullptr;

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

int FinaleInterpreter::handleEvent(ddevent_t const &ev)
{
    LOG_AS("FinaleInterpreter");

    if(d->flags.suspended)
        return false;

    // During the first ~second disallow all events/skipping.
    if(d->timer < 20)
        return false;

    if(!::isClient)
    {
#ifdef __CLIENT__
        // Any handlers for this event?
        if(IS_KEY_DOWN(&ev))
        {
            if(Instance::EventHandler *eh = d->findEventHandler(ev))
            {
                skipToMarker(eh->gotoMarker);

                // Never eat up events.
                if(IS_TOGGLE_UP(&ev)) return false;

                return (d->flags.eat_events != 0);
            }
        }
#endif
    }

    // If we can't skip, there's no interaction of any kind.
    if(!d->flags.can_skip && !d->flags.paused)
        return false;

    // We are only interested in key/button down presses.
    if(!IS_TOGGLE_DOWN(&ev))
        return false;

#ifdef __CLIENT__
    if(::isClient)
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
void FinaleInterpreter::addEventHandler(ddevent_t const &evTemplate, char const *gotoMarker)
{
    // Does a handler already exist for this?
    if(d->findEventHandler(evTemplate)) return;

    d->eventHandlers.append(Instance::EventHandler(&evTemplate, gotoMarker));
}

void FinaleInterpreter::removeEventHandler(ddevent_t const &evTemplate)
{
    if(Instance::EventHandler *eh = d->findEventHandler(evTemplate))
    {
        int index = 0;
        while(&d->eventHandlers.at(index) != eh) { index++; }
        d->eventHandlers.removeAt(index);
    }
}
#endif // __CLIENT__

fi_page_t &FinaleInterpreter::page(PageIndex index)
{
    DENG2_ASSERT(d->pages[index]);
    return *d->pages[index];
}

fi_object_t *FinaleInterpreter::findObject(fi_obtype_e type, char const *name)
{
    DENG2_ASSERT(name && name[0]);

    // An existing object?
    if(fi_objectid_t id = d->names.toId(name, type))
    {
        return FI_Object(id);
    }

    // A new object.
    fi_object_t *ob = FI_NewObject(type, name);
    PageIndex const pageIndex = d->choosePageForObjectType(type);
    switch(type)
    {
    case FI_TEXT:
        FIData_TextSetFont(ob, FIPage_PredefinedFont(d->pages[pageIndex], 0));
        FIData_TextSetPreColor(ob, 1);
        break;

    default: break;
    }
    return FIPage_AddObject(d->pages[pageIndex], d->names.add(ob, name));
}

void FinaleInterpreter::deleteObject(fi_object_t *ob)
{
    if(!ob) return;
    FI_DeleteObject(d->names.remove(ob));
}

void FinaleInterpreter::beginDoSkipMode()
{
    if(!skipInProgress()) return;

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

void FinaleInterpreter::foundSkipMarker(char const *marker)
{
    DENG2_ASSERT(marker);
    // Does it match the current goto torget?
    if(!qstricmp(d->gotoTarget, marker))
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

void FinaleInterpreter::setWaitAnim(fi_object_t *newWaitAnim)
{
    d->waitAnim = newWaitAnim;
}

void FinaleInterpreter::setWaitText(fi_object_t *newWaitText)
{
    d->waitText = newWaitText;
}

/// @note This command is called even when condition-skipping.
DEFFC(Do)
{
    DENG2_UNUSED2(cmd, ops);
    fi.beginDoSkipMode();
}

DEFFC(End)
{
    DENG2_UNUSED2(cmd, ops);
    fi.gotoEnd();
}

static void changePageBackground(fi_page_t &page, Material *newMaterial)
{
    // If the page does not yet have a background set we must setup the color+alpha.
    if(newMaterial && !FIPage_BackgroundMaterial(&page))
    {
        FIPage_SetBackgroundTopColorAndAlpha   (&page, 1, 1, 1, 1, 0);
        FIPage_SetBackgroundBottomColorAndAlpha(&page, 1, 1, 1, 1, 0);
    }
    FIPage_SetBackgroundMaterial(&page, newMaterial);
}

DEFFC(BGMaterial)
{
    DENG2_UNUSED(cmd);

    // First attempt to resolve as a Values URI (which defines the material URI).
    Material *material = nullptr;
    try
    {
        if(ded_value_t *value = Def_GetValueByUri(OP_URI(0)))
        {
            material = &App_ResourceSystem().material(de::Uri(value->text, RC_NULL));
        }
        else
        {
            material = &App_ResourceSystem().material(*reinterpret_cast<de::Uri const *>(OP_URI(0)));
        }
    }
    catch(MaterialManifest::MissingMaterialError const &)
    {} // Ignore this error.
    catch(ResourceSystem::MissingManifestError const &)
    {} // Ignore this error.

    changePageBackground(fi.page(FinaleInterpreter::Anims), material);
}

DEFFC(NoBGMaterial)
{
    DENG2_UNUSED2(cmd, ops);
    changePageBackground(fi.page(FinaleInterpreter::Anims), 0);
}

DEFFC(InTime)
{
    DENG2_UNUSED(cmd);
    fi.setInTime(FRACSECS_TO_TICKS(OP_FLOAT(0)));
}

DEFFC(Tic)
{
    DENG2_UNUSED2(cmd, ops);
    fi.wait();
}

DEFFC(Wait)
{
    DENG2_UNUSED(cmd);
    fi.wait(FRACSECS_TO_TICKS(OP_FLOAT(0)));
}

DEFFC(WaitText)
{
    DENG2_UNUSED(cmd);
    fi.setWaitText(fi.findObject(FI_TEXT, OP_CSTRING(0)));
}

DEFFC(WaitAnim)
{
    DENG2_UNUSED(cmd);
    fi.setWaitAnim(fi.findObject(FI_PIC, OP_CSTRING(0)));
}

DEFFC(Color)
{
    DENG2_UNUSED(cmd);
    FIPage_SetBackgroundTopColor   (&fi.page(FinaleInterpreter::Anims), OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), fi.inTime());
    FIPage_SetBackgroundBottomColor(&fi.page(FinaleInterpreter::Anims), OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), fi.inTime());
}

DEFFC(ColorAlpha)
{
    DENG2_UNUSED(cmd);
    FIPage_SetBackgroundTopColorAndAlpha   (&fi.page(FinaleInterpreter::Anims), OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
    FIPage_SetBackgroundBottomColorAndAlpha(&fi.page(FinaleInterpreter::Anims), OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
}

DEFFC(Pause)
{
    DENG2_UNUSED2(cmd, ops);
    fi.pause();
}

DEFFC(CanSkip)
{
    DENG2_UNUSED2(cmd, ops);
    fi.setSkip(true);
}

DEFFC(NoSkip)
{
    DENG2_UNUSED2(cmd, ops);
    fi.setSkip(false);
}

DEFFC(SkipHere)
{
    DENG2_UNUSED2(cmd, ops);
    fi.foundSkipHere();
}

DEFFC(Events)
{
    DENG2_UNUSED2(cmd, ops);
    fi.setHandleEvents();
}

DEFFC(NoEvents)
{
    DENG2_UNUSED2(cmd, ops);
    fi.setHandleEvents(false);
}

DEFFC(OnKey)
{
#ifdef __CLIENT__
    DENG2_UNUSED(cmd);

    // Construct a template event for this handler.
    ddevent_t ev; de::zap(ev);
    ev.device = IDEV_KEYBOARD;
    ev.type   = E_TOGGLE;
    ev.toggle.id    = DD_GetKeyCode(OP_CSTRING(0));
    ev.toggle.state = ETOG_DOWN;

    fi.addEventHandler(ev, OP_CSTRING(1));
#else
    DENG2_UNUSED3(cmd, ops, fi);
#endif
}

DEFFC(UnsetKey)
{
#ifdef __CLIENT__
    DENG2_UNUSED(cmd);

    // Construct a template event for what we want to "unset".
    ddevent_t ev; de::zap(ev);
    ev.device = IDEV_KEYBOARD;
    ev.type   = E_TOGGLE;
    ev.toggle.id    = DD_GetKeyCode(OP_CSTRING(0));
    ev.toggle.state = ETOG_DOWN;

    fi.removeEventHandler(ev);
#else
    DENG2_UNUSED3(cmd, ops, fi);
#endif
}

DEFFC(If)
{
    DENG2_UNUSED2(cmd, ops);
    LOG_AS("FIC_If");

    char const *token = OP_CSTRING(0);
    bool val          = false;

    // Built-in conditions.
    if(!qstricmp(token, "netgame"))
    {
        val = netGame;
    }
    else if(!qstrnicmp(token, "mode:", 5))
    {
        if(App_GameLoaded())
            val = !String(token + 5).compareWithoutCase(App_CurrentGame().identityKey());
        else
            val = 0;
    }
    // Any hooks?
    else if(Plug_CheckForHook(HOOK_FINALE_EVAL_IF))
    {
        ddhook_finale_script_evalif_paramaters_t p; de::zap(p);
        p.token     = token;
        p.returnVal = 0;
        if(DD_CallHooks(HOOK_FINALE_EVAL_IF, fi.id(), (void *) &p))
        {
            val = p.returnVal;
            LOG_SCR_XVERBOSE("HOOK_FINALE_EVAL_IF: %s => %i") << token << val;
        }
        else
        {
            LOG_SCR_XVERBOSE("HOOK_FINALE_EVAL_IF: no hook (for %s)") << token;
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
    DENG2_UNUSED2(cmd, ops);
    fi.setSkipNext(!fi.lastSkipped());
}

DEFFC(GoTo)
{
    DENG2_UNUSED(cmd);
    fi.skipToMarker(OP_CSTRING(0));
}

DEFFC(Marker)
{
    DENG2_UNUSED(cmd);
    fi.foundSkipMarker(OP_CSTRING(0));
}

DEFFC(Delete)
{
    DENG2_UNUSED(cmd);
    fi.deleteObject(OP_OBJECT(0));
}

DEFFC(Image)
{
    DENG2_UNUSED(cmd);
    LOG_AS("FIC_Image");

    fi_object_t *ob   = fi.findObject(FI_PIC, OP_CSTRING(0));
    char const *name  = OP_CSTRING(1);
    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);

    FIData_PicClearAnimation(ob);

    if(rawtex_t *rawTex = App_ResourceSystem().declareRawTexture(lumpNum))
    {
        FIData_PicAppendFrame(ob, PFT_RAW, -1, &rawTex->lumpNum, 0, false);
        return;
    }

    LOG_SCR_WARNING("Missing lump '%s'") << name;
}

DEFFC(ImageAt)
{
    DENG2_UNUSED(cmd);
    LOG_AS("FIC_ImageAt");

    fi_object_t *ob   = fi.findObject(FI_PIC, OP_CSTRING(0));
    float x           = OP_FLOAT(1);
    float y           = OP_FLOAT(2);
    char const *name  = OP_CSTRING(3);
    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(name);

    AnimatorVector3_Init(ob->pos, x, y, 0);
    FIData_PicClearAnimation(ob);

    if(rawtex_t *rawTex = App_ResourceSystem().declareRawTexture(lumpNum))
    {
        FIData_PicAppendFrame(ob, PFT_RAW, -1, &rawTex->lumpNum, 0, false);
        return;
    }

    LOG_SCR_WARNING("Missing lump '%s'") << name;
}

#ifdef __CLIENT__
static DGLuint loadAndPrepareExtTexture(char const *fileName)
{
    image_t image;
    DGLuint glTexName = 0;

    if(GL_LoadExtImage(image, fileName, LGM_NORMAL))
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
    DENG2_UNUSED(cmd);

    LOG_AS("FIC_XImage");

    fi_object_t *ob      = fi.findObject(FI_PIC, OP_CSTRING(0));
#ifdef __CLIENT__
    char const *fileName = OP_CSTRING(1);
#endif

    FIData_PicClearAnimation(ob);

#ifdef __CLIENT__
    // Load the external resource.
    if(DGLuint tex = loadAndPrepareExtTexture(fileName))
    {
        FIData_PicAppendFrame(ob, PFT_XIMAGE, -1, &tex, 0, false);
    }
    else
    {
        LOG_SCR_WARNING("Missing graphic '%s'") << fileName;
    }
#endif // __CLIENT__
}

DEFFC(Patch)
{
    DENG2_UNUSED(cmd);

    fi_object_t *ob        = fi.findObject(FI_PIC, OP_CSTRING(0));
    char const *encodedName = OP_CSTRING(3);

    LOG_AS("FIC_Patch");
    AnimatorVector3_Init(ob->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    FIData_PicClearAnimation(ob);

    patchid_t patchId = R_DeclarePatch(encodedName);
    if(patchId)
    {
        FIData_PicAppendFrame(ob, PFT_PATCH, -1, (void *)&patchId, 0, 0);
    }
    else
    {
        LOG_SCR_WARNING("Missing Patch '%s'") << encodedName;
    }
}

DEFFC(SetPatch)
{
    DENG2_UNUSED(cmd);

    fi_object_t *ob        = fi.findObject(FI_PIC, OP_CSTRING(0));
    char const *encodedName = OP_CSTRING(1);

    LOG_AS("FIC_SetPatch");
    patchid_t patchId = R_DeclarePatch(encodedName);
    if(patchId == 0)
    {
        LOG_SCR_WARNING("Missing Patch '%s'") << encodedName;
        return;
    }

    if(!((fidata_pic_t *)ob)->numFrames)
    {
        FIData_PicAppendFrame(ob, PFT_PATCH, -1, (void *)&patchId, 0, false);
        return;
    }

    // Convert the first frame.
    fidata_pic_frame_t *f = ((fidata_pic_t *)ob)->frames[0];
    f->type  = PFT_PATCH;
    f->texRef.patch = patchId;
    f->tics  = -1;
    f->sound = 0;
}

DEFFC(ClearAnim)
{
    DENG2_UNUSED2(cmd, fi);
    if(OP_OBJECT(0) && OP_OBJECT(0)->type == FI_PIC)
    {
        FIData_PicClearAnimation(OP_OBJECT(0));
    }
}

DEFFC(Anim)
{
    DENG2_UNUSED(cmd);
    LOG_AS("FIC_Anim");

    fi_object_t *ob        = fi.findObject(FI_PIC, OP_CSTRING(0));
    char const *encodedName = OP_CSTRING(1);
    int const tics          = FRACSECS_TO_TICKS(OP_FLOAT(2));

    patchid_t patchId = R_DeclarePatch(encodedName);
    if(!patchId)
    {
        LOG_SCR_WARNING("Patch '%s' not found") << encodedName;
        return;
    }

    FIData_PicAppendFrame(ob, PFT_PATCH, tics, (void *)&patchId, 0, false);
    ((fidata_pic_t *)ob)->animComplete = false;
}

DEFFC(AnimImage)
{
    DENG2_UNUSED(cmd);
    LOG_AS("FIC_AnimImage");

    fi_object_t *ob        = fi.findObject(FI_PIC, OP_CSTRING(0));
    char const *encodedName = OP_CSTRING(1);
    int const tics          = FRACSECS_TO_TICKS(OP_FLOAT(2));

    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(encodedName);
    if(rawtex_t *rawTex = App_ResourceSystem().declareRawTexture(lumpNum))
    {
        FIData_PicAppendFrame(ob, PFT_RAW, tics, &rawTex->lumpNum, 0, false);
        ((fidata_pic_t *)ob)->animComplete = false;
        return;
    }
    LOG_SCR_WARNING("Lump '%s' not found") << encodedName;
}

DEFFC(Repeat)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_PIC, OP_CSTRING(0));
    ((fidata_pic_t *)ob)->flags.looping = true;
}

DEFFC(StateAnim)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_PIC, OP_CSTRING(0));
    int stateId      = Def_Get(DD_DEF_STATE, OP_CSTRING(1), 0);
    int count        = OP_INT(2);

    // Animate N states starting from the given one.
    ((fidata_pic_t *)ob)->animComplete = false;
    for(; count > 0 && stateId > 0; count--)
    {
        state_t *st = &runtimeDefs.states[stateId];
#ifdef __CLIENT__
        spriteinfo_t sinf;
        R_GetSpriteInfo(st->sprite, st->frame & 0x7fff, &sinf);
        FIData_PicAppendFrame(ob, PFT_MATERIAL, (st->tics <= 0? 1 : st->tics), sinf.material, 0, sinf.flip);
#endif

        // Go to the next state.
        stateId = st->nextState;
    }
}

DEFFC(PicSound)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_PIC, OP_CSTRING(0));
    int const sound  = Def_Get(DD_DEF_SOUND, OP_CSTRING(1), 0);

    if(!((fidata_pic_t *)ob)->numFrames)
    {
        FIData_PicAppendFrame(ob, PFT_MATERIAL, -1, 0, sound, false);
        return;
    }

    fidata_pic_frame_t *f = ((fidata_pic_t *)ob)->frames[((fidata_pic_t *)ob)->numFrames - 1];
    f->sound = sound;
}

DEFFC(ObjectOffX)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->pos[0], OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectOffY)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->pos[1], OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectOffZ)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->pos[2], OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectRGB)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = OP_OBJECT(0);

    if(!ob || !(ob->type == FI_TEXT || ob->type == FI_PIC))
        return;

    float rgb[3];
    rgb[CR] = OP_FLOAT(1);
    rgb[CG] = OP_FLOAT(2);
    rgb[CB] = OP_FLOAT(3);
    switch(ob->type)
    {
    case FI_TEXT:
        FIData_TextSetColor(ob, rgb[CR], rgb[CG], rgb[CB], fi.inTime());
        break;

    case FI_PIC: {
        fidata_pic_t *p = (fidata_pic_t *)ob;
        AnimatorVector3_Set(p->color,          rgb[CR], rgb[CG], rgb[CB], fi.inTime());
        // This affects all the colors.
        AnimatorVector3_Set(p->otherColor,     rgb[CR], rgb[CG], rgb[CB], fi.inTime());
        AnimatorVector3_Set(p->edgeColor,      rgb[CR], rgb[CG], rgb[CB], fi.inTime());
        AnimatorVector3_Set(p->otherEdgeColor, rgb[CR], rgb[CG], rgb[CB], fi.inTime());
        break; }

    default: DENG_ASSERT(!"FIC_ObjectRGB: Unknown object type");
    }
}

DEFFC(ObjectAlpha)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = OP_OBJECT(0);

    if(!ob || !(ob->type == FI_TEXT || ob->type == FI_PIC))
        return;

    float alpha = OP_FLOAT(1);
    switch(ob->type)
    {
    case FI_TEXT:
        FIData_TextSetAlpha(ob, alpha, fi.inTime());
        break;

    case FI_PIC: {
        fidata_pic_t *p = (fidata_pic_t *)ob;
        Animator_Set(&p->color[3],      alpha, fi.inTime());
        Animator_Set(&p->otherColor[3], alpha, fi.inTime());
        break; }

    default: DENG_ASSERT(!"FIC_ObjectAlpha: Unknown object type");
    }
}

DEFFC(ObjectScaleX)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->scale[0], OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScaleY)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->scale[1], OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScaleZ)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->scale[2], OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScale)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        AnimatorVector2_Set(ob->scale, OP_FLOAT(1), OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(ObjectScaleXY)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        AnimatorVector2_Set(ob->scale, OP_FLOAT(1), OP_FLOAT(2), fi.inTime());
    }
}

DEFFC(ObjectScaleXYZ)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        AnimatorVector3_Set(ob->scale, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
    }
}

DEFFC(ObjectAngle)
{
    DENG2_UNUSED(cmd);
    if(OP_OBJECT(0))
    {
        fi_object_t *ob = OP_OBJECT(0);
        Animator_Set(&ob->angle, OP_FLOAT(1), fi.inTime());
    }
}

DEFFC(Rect)
{
    DENG2_UNUSED(cmd);
    fidata_pic_t *ob = (fidata_pic_t *) fi.findObject(FI_PIC, OP_CSTRING(0));

    /**
     * We may be converting an existing Pic to a Rect, so re-init the expected
     * default state accordingly.
     *
     * danij: This seems rather error-prone to me. How about we turn them into
     * seperate object classes instead (Pic inheriting from Rect).
     */
    ob->animComplete = true;
    ob->flags.looping = false; // Yeah?

    AnimatorVector3_Init(ob->pos,   OP_FLOAT(1), OP_FLOAT(2), 0);
    AnimatorVector3_Init(ob->scale, OP_FLOAT(3), OP_FLOAT(4), 1);

    // Default colors.
    AnimatorVector4_Init(ob->color,      1, 1, 1, 1);
    AnimatorVector4_Init(ob->otherColor, 1, 1, 1, 1);

    // Edge alpha is zero by default.
    AnimatorVector4_Init(ob->edgeColor,      1, 1, 1, 0);
    AnimatorVector4_Init(ob->otherEdgeColor, 1, 1, 1, 0);
}

DEFFC(FillColor)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = OP_OBJECT(0);

    if(!ob || ob->type != FI_PIC)
        return;

    // Which colors to modify?
    int which = 0;
    if(!qstricmp(OP_CSTRING(1), "top"))         which |= 1;
    else if(!qstricmp(OP_CSTRING(1), "bottom")) which |= 2;
    else                                        which = 3;

    float rgba[4];
    for(uint i = 0; i < 4; ++i)
    {
        rgba[i] = OP_FLOAT(2+i);
    }

    if(which & 1)
        AnimatorVector4_Set(((fidata_pic_t *)ob)->color,      rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi.inTime());
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t *)ob)->otherColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi.inTime());
}

DEFFC(EdgeColor)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = OP_OBJECT(0);

    if(!ob || ob->type != FI_PIC)
        return;

    // Which colors to modify?
    int which = 0;
    if(!qstricmp(OP_CSTRING(1), "top"))         which |= 1;
    else if(!qstricmp(OP_CSTRING(1), "bottom")) which |= 2;
    else                                        which = 3;

    float rgba[4];
    for(uint i = 0; i < 4; ++i)
    {
        rgba[i] = OP_FLOAT(2+i);
    }

    if(which & 1)
        AnimatorVector4_Set(((fidata_pic_t *)ob)->edgeColor,      rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi.inTime());
    if(which & 2)
        AnimatorVector4_Set(((fidata_pic_t *)ob)->otherEdgeColor, rgba[CR], rgba[CG], rgba[CB], rgba[CA], fi.inTime());
}

DEFFC(OffsetX)
{
    DENG2_UNUSED(cmd);
    FIPage_SetOffsetX(&fi.page(FinaleInterpreter::Anims), OP_FLOAT(0), fi.inTime());
}

DEFFC(OffsetY)
{
    DENG2_UNUSED(cmd);
    FIPage_SetOffsetY(&fi.page(FinaleInterpreter::Anims), OP_FLOAT(0), fi.inTime());
}

DEFFC(Sound)
{
    DENG2_UNUSED2(cmd, fi);
    S_LocalSound(Def_Get(DD_DEF_SOUND, OP_CSTRING(0), nullptr), nullptr);
}

DEFFC(SoundAt)
{
    DENG2_UNUSED2(cmd, fi);
    int const soundId = Def_Get(DD_DEF_SOUND, OP_CSTRING(0), nullptr);
    float vol = de::min(OP_FLOAT(1), 1.f);
    S_LocalSoundAtVolume(soundId, nullptr, vol);
}

DEFFC(SeeSound)
{
    DENG2_UNUSED2(cmd, fi);
    int num = Def_Get(DD_DEF_MOBJ, OP_CSTRING(0), nullptr);
    if(num < 0 || runtimeDefs.mobjInfo[num].seeSound <= 0)
        return;
    S_LocalSound(runtimeDefs.mobjInfo[num].seeSound, nullptr);
}

DEFFC(DieSound)
{
    DENG2_UNUSED2(cmd, fi);
    int num = Def_Get(DD_DEF_MOBJ, OP_CSTRING(0), nullptr);
    if(num < 0 || runtimeDefs.mobjInfo[num].deathSound <= 0)
        return;
    S_LocalSound(runtimeDefs.mobjInfo[num].deathSound, nullptr);
}

DEFFC(Music)
{
    DENG2_UNUSED3(cmd, ops, fi);
    S_StartMusic(OP_CSTRING(0), true);
}

DEFFC(MusicOnce)
{
    DENG2_UNUSED3(cmd, ops, fi);
    S_StartMusic(OP_CSTRING(0), false);
}

DEFFC(Filter)
{
    DENG2_UNUSED(cmd);
    FIPage_SetFilterColorAndAlpha(&fi.page(FinaleInterpreter::Texts), OP_FLOAT(0), OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
}

DEFFC(Text)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    AnimatorVector3_Init(ob->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    FIData_TextCopy(ob, OP_CSTRING(3));
    ((fidata_text_t *)ob)->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromDef)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    AnimatorVector3_Init(ob->pos, OP_FLOAT(1), OP_FLOAT(2), 0);
    char *str;
    if(Def_Get(DD_DEF_TEXT, (char*)OP_CSTRING(3), &str))
    {
        FIData_TextCopy(ob, str);
    }
    else
    {
        FIData_TextCopy(ob, "(undefined)"); // Not found!
    }
    ((fidata_text_t *)ob)->cursorPos = 0; // Restart the text.
}

DEFFC(TextFromLump)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));

    AnimatorVector3_Init(ob->pos, OP_FLOAT(1), OP_FLOAT(2), 0);

    lumpnum_t lumpNum = App_FileSystem().lumpNumForName(OP_CSTRING(3));
    if(lumpNum >= 0)
    {
        File1 &lump            = App_FileSystem().lump(lumpNum);
        uint8_t const *rawText = lump.cache();

        AutoStr *text = AutoStr_NewStd();
        Str_Reserve(text, lump.size() * 2);

        char *out = Str_Text(text);
        for(size_t i = 0; i < lump.size(); ++i)
        {
            char ch = (char)(rawText[i]);
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
        lump.unlock();

        FIData_TextCopy(ob, Str_Text(text));
    }
    else
    {
        FIData_TextCopy(ob, "(not found)");
    }
    ((fidata_text_t *)ob)->cursorPos = 0; // Restart.
}

DEFFC(SetText)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    FIData_TextCopy(ob, OP_CSTRING(1));
}

DEFFC(SetTextDef)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    char *str;
    if(Def_Get(DD_DEF_TEXT, OP_CSTRING(1), &str))
    {
        FIData_TextCopy(ob, str);
    }
    else
    {
        FIData_TextCopy(ob, "(undefined)"); // Not found!
    }
}

DEFFC(DeleteText)
{
    DENG2_UNUSED(cmd);
    fi.deleteObject(OP_OBJECT(0));
}

DEFFC(PredefinedColor)
{
    DENG2_UNUSED(cmd);
    FIPage_SetPredefinedColor(&fi.page(FinaleInterpreter::Texts), de::clamp(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_COLORS) - 1, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
    FIPage_SetPredefinedColor(&fi.page(FinaleInterpreter::Anims), de::clamp(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_COLORS) - 1, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
}

DEFFC(PredefinedFont)
{
#ifdef __CLIENT__
    DENG2_UNUSED(cmd);
    LOG_AS("FIC_PredefinedFont");

    fontid_t const fontNum = Fonts_ResolveUri(OP_URI(1));
    if(fontNum)
    {
        int const idx = de::clamp(1, OP_INT(0), FIPAGE_NUM_PREDEFINED_FONTS) - 1;
        FIPage_SetPredefinedFont(&fi.page(FinaleInterpreter::Texts), idx, fontNum);
        FIPage_SetPredefinedFont(&fi.page(FinaleInterpreter::Anims), idx, fontNum);
        return;
    }

    AutoStr *fontPath = Uri_ToString(OP_URI(1));
    LOG_SCR_WARNING("Unknown font '%s'") << Str_Text(fontPath);
#else
    DENG2_UNUSED3(cmd, ops, fi);
#endif
}

DEFFC(TextRGB)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    FIData_TextSetColor(ob, OP_FLOAT(1), OP_FLOAT(2), OP_FLOAT(3), fi.inTime());
}

DEFFC(TextAlpha)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    FIData_TextSetAlpha(ob, OP_FLOAT(1), fi.inTime());
}

DEFFC(TextOffX)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    Animator_Set(&ob->pos[0], OP_FLOAT(1), fi.inTime());
}

DEFFC(TextOffY)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    Animator_Set(&ob->pos[1], OP_FLOAT(1), fi.inTime());
}

DEFFC(TextCenter)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t *)ob)->alignFlags &= ~(ALIGN_LEFT|ALIGN_RIGHT);
}

DEFFC(TextNoCenter)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t *)ob)->alignFlags |= ALIGN_LEFT;
}

DEFFC(TextScroll)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t *)ob)->scrollWait = OP_INT(1);
    ((fidata_text_t *)ob)->scrollTimer = 0;
}

DEFFC(TextPos)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t *)ob)->cursorPos = OP_INT(1);
}

DEFFC(TextRate)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t *)ob)->wait = OP_INT(1);
}

DEFFC(TextLineHeight)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    ((fidata_text_t *)ob)->lineHeight = OP_FLOAT(1);
}

DEFFC(Font)
{
#ifdef __CLIENT__
    DENG2_UNUSED(cmd);
    LOG_AS("FIC_Font");

    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    fontid_t fontNum = Fonts_ResolveUri(OP_URI(1));
    if(fontNum)
    {
        FIData_TextSetFont(ob, fontNum);
        return;
    }

    AutoStr *fontPath = Uri_ToString(OP_URI(1));
    LOG_SCR_WARNING("Unknown font '%s'") << Str_Text(fontPath);
#else
    DENG2_UNUSED3(cmd, ops, fi);
#endif
}

DEFFC(FontA)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    FIData_TextSetFont(ob, FIPage_PredefinedFont(&fi.page(FinaleInterpreter::Texts), 0));
}

DEFFC(FontB)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    FIData_TextSetFont(ob, FIPage_PredefinedFont(&fi.page(FinaleInterpreter::Texts), 1));
}

DEFFC(NoMusic)
{
    DENG2_UNUSED3(cmd, ops, fi);
    S_StopMusic();
}

DEFFC(TextScaleX)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    Animator_Set(&ob->scale[0], OP_FLOAT(1), fi.inTime());
}

DEFFC(TextScaleY)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    Animator_Set(&ob->scale[1], OP_FLOAT(1), fi.inTime());
}

DEFFC(TextScale)
{
    DENG2_UNUSED(cmd);
    fi_object_t *ob = fi.findObject(FI_TEXT, OP_CSTRING(0));
    AnimatorVector2_Set(ob->scale, OP_FLOAT(1), OP_FLOAT(2), fi.inTime());
}

DEFFC(PlayDemo)
{
    /// @todo Demos are not supported at the moment. -jk
#if 0
    // While playing a demo we suspend command interpretation.
    fi.suspend();

    // Start the demo.
    if(!Con_Executef(CMDS_DDAY, true, "playdemo \"%s\"", OP_CSTRING(0)))
    {
        // Demo playback failed. Here we go again...
        fi.resume();
    }
#else
    DENG2_UNUSED3(cmd, ops, fi);
#endif
}

DEFFC(Command)
{
    DENG2_UNUSED2(cmd, fi);
    Con_Executef(CMDS_SCRIPT, false, "%s", OP_CSTRING(0));
}

DEFFC(ShowMenu)
{
    DENG2_UNUSED2(cmd, ops);
    fi.setShowMenu();
}

DEFFC(NoShowMenu)
{
    DENG2_UNUSED2(cmd, ops);
    fi.setShowMenu(false);
}
