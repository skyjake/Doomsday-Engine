/** @file dedparser.cpp  Doomsday Engine Definition File Reader.
 *
 * A GHASTLY MESS!!! This should be rewritten.
 *
 * You have been warned!
 *
 * At the moment the idea is that a lot of macros are used to read a more
 * or less fixed structure of definitions, and if an error occurs then
 * "goto out_of_here;". It leads to a lot more code than an elegant parser
 * would require.
 *
 * The current implementation of the reader is a "structural" approach:
 * the definition file is parsed based on the structure implied by
 * the read tokens. A true parser would have syntax definitions for
 * a bunch of tokens, and the same parsing rules would be applied for
 * everything.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2010-2015 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_URI

#include "doomsday/defs/dedparser.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include <de/c_wrapper.h>
#include <de/legacy/memory.h>
#include <de/legacy/vector1.h>
#include <de/app.h>
#include <de/arrayvalue.h>
#include <de/nativepath.h>
#include <de/recordvalue.h>
#include <de/textvalue.h>

#include "doomsday/defs/decoration.h"
#include "doomsday/defs/ded.h"
#include "doomsday/defs/dedfile.h"
#include "doomsday/defs/episode.h"
#include "doomsday/defs/finale.h"
#include "doomsday/defs/mapgraphnode.h"
#include "doomsday/defs/mapinfo.h"
#include "doomsday/defs/material.h"
#include "doomsday/defs/model.h"
#include "doomsday/defs/music.h"
#include "doomsday/defs/sky.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/uri.h"
#include "doomsday/world/xgclass.h"

#ifdef WIN32
#  define stricmp _stricmp
#endif

using namespace de;
using namespace res;

#define MAX_RECUR_DEPTH     30
#define MAX_TOKEN_LEN       128

// Some macros.
#define STOPCHAR(x) (isspace(x) || x == ';' || x == '#' || x == '{' \
                    || x == '}' || x == '=' || x == '"' || x == '*' \
                    || x == '|')

#define ISTOKEN(X)  (!stricmp(token, X))

#define READSTR(X)  if (!ReadString(X, sizeof(X))) { \
                    setError("Syntax error in string value"); \
                    retVal = false; goto ded_end_read; }


#define READURI(X, SHM) if (!ReadUri(X, SHM)) { \
    setError("Syntax error parsing resource path"); \
    retVal = false; goto ded_end_read; }

#define MISSING_SC_ERROR    setError("Missing semicolon"); \
                            retVal = false; goto ded_end_read;

#define CHECKSC     if (source->version <= 5) { ReadToken(); if (!ISTOKEN(";")) { MISSING_SC_ERROR; } }
#define SKIPSC      { ReadToken(); if (!ISTOKEN(";")) { UnreadToken(token); }}

#define FINDBEGIN   while (!ISTOKEN("{") && !source->atEnd) ReadToken();
#define FINDEND     while (!ISTOKEN("}") && !source->atEnd) ReadToken();

#define ISLABEL(X)  (!stricmp(label, X))

#define READLABEL   if (!ReadLabel(label)) { retVal = false; goto ded_end_read; } \
                    if (ISLABEL("}")) break;

#define READLABEL_NOBREAK   if (!ReadLabel(label)) { retVal = false; goto ded_end_read; }

#define FAILURE     retVal = false; goto ded_end_read;
#define READBYTE(X) if (!ReadByte(&X)) { FAILURE }
#define READINT(X)  if (!ReadInt(&X, false)) { FAILURE }
#define READUINT(X) if (!ReadInt(&X, true)) { FAILURE }
#define READFLT(X)  if (!ReadFloat(&X)) { FAILURE }
#define READNBYTEVEC(X,N) if (!ReadNByteVector(X, N)) { FAILURE }
#define READFLAGS(X,P) if (!ReadFlags(&X, P)) { FAILURE }
#define READBLENDMODE(X) if (!ReadBlendmode(&X)) { FAILURE }
#define READSTRING(S,I) if (!ReadString(S, sizeof(S))) { I = strtol(token,0,0); }
#define READVECTOR(X,N) if (!ReadVector(X, N)) { FAILURE }

#define RV_BYTE(lab, X) if (ISLABEL(lab)) { READBYTE(X); } else
#define RV_INT(lab, X)  if (ISLABEL(lab)) { READINT(X); } else
#define RV_UINT(lab, X) if (ISLABEL(lab)) { READUINT(X); } else
#define RV_FLT(lab, X)  if (ISLABEL(lab)) { READFLT(X); } else
#define RV_VEC(lab, X, N)   if (ISLABEL(lab)) { int b; FINDBEGIN; \
                        for (b=0; b<N; ++b) {READFLT(X[b])} ReadToken(); } else
#define RV_VEC_VAR(lab, X, N) if (ISLABEL(lab)) { READVECTOR(X, N); } else
#define RV_IVEC(lab, X, N)  if (ISLABEL(lab)) { int b; FINDBEGIN; \
                        for (b=0; b<N; ++b) {READINT(X[b])} ReadToken(); } else
#define RV_NBVEC(lab, X, N) if (ISLABEL(lab)) { READNBYTEVEC(X,N); } else
#define RV_INT_ELEM(lab, ARRAY, ELEM) if (ISLABEL(lab)) { \
    int _value; READINT(_value); \
    (ARRAY).array().setElement(ELEM, _value); } else
#define RV_FLT_ELEM(lab, ARRAY, ELEM) if (ISLABEL(lab)) { \
    float _value; READFLT(_value); \
    (ARRAY).array().setElement(ELEM, _value); } else
#define RV_STR_ELEM(lab, ARRAY, ELEM) if (ISLABEL(lab)) { \
    String _value; if (!ReadString(_value)) { FAILURE } \
    (ARRAY).array().setElement(ELEM, _value); } else
#define RV_STR(lab, X)  if (ISLABEL(lab)) { READSTR(X); } else
#define RV_STR_INT(lab, S, I)   if (ISLABEL(lab)) { if (!ReadString(S,sizeof(S))) \
                                I = strtol(token,0,0); } else
#define RV_URI(lab, X, RN)  if (ISLABEL(lab)) { READURI(X, RN); } else
#define RV_FLAGS(lab, X, P) if (ISLABEL(lab)) { READFLAGS(X, P); } else
#define RV_FLAGS_ELEM(lab, X, ELEM, P) if (ISLABEL(lab)) { if (!ReadFlags(&X, P, ELEM)) { FAILURE } } else
#define RV_BLENDMODE(lab, X) if (ISLABEL(lab)) { READBLENDMODE(X); } else
#define RV_ANYSTR(lab, X)   if (ISLABEL(lab)) { if (!ReadAnyString(&X)) { FAILURE } } else
#define RV_END          { setError("Unknown label '" + String(label) + "'"); retVal = false; goto ded_end_read; }

static struct xgclass_s *xgClassLinks;

// Helper for managing a dummy definition allocated on the stack.
template <typename T>
struct Dummy : public T {
    Dummy() { zap(*this); }
    ~Dummy() { T::release(); }
    void clear() { T::release(); zap(*this); }
};

void DED_SetXGClassLinks(struct xgclass_s *links)
{
    xgClassLinks = links;
}

DE_PIMPL(DEDParser)
{
    ded_t *ded;

    struct dedsource_s
    {
        const char *buffer;
        const char *pos;
        dd_bool     atEnd;
        int         lineNumber;
        String      fileName;
        int         version;  ///< v6 does not require semicolons.
        bool        custom;   ///< @c true= source is a user supplied add-on.
    };

    typedef dedsource_s dedsource_t;

    dedsource_t sourceStack[MAX_RECUR_DEPTH];
    dedsource_t *source; // Points to the current source.

    char token[MAX_TOKEN_LEN+1];
    char unreadToken[MAX_TOKEN_LEN+1];

    Impl(Public *i) : Base(i), ded(0), source(0)
    {
        zap(token);
        zap(unreadToken);
    }

    void DED_InitReader(const char *buffer, String fileName, bool sourceIsCustom)
    {
        if (source && source - sourceStack >= MAX_RECUR_DEPTH)
        {
            App_FatalError("DED_InitReader: Include recursion is too deep.\n");
        }

        if (!source)
        {
            // This'll be the first source.
            source = sourceStack;
        }
        else
        {
            // Take the next entry in the stack.
            source++;
        }

        source->pos = source->buffer = buffer;

        source->atEnd      = false;
        source->lineNumber = 1;
        source->fileName   = fileName;
        source->version    = DED_VERSION;
        source->custom     = sourceIsCustom;
    }

    void DED_CloseReader()
    {
        if (source == sourceStack)
        {
            source = 0;
        }
        else
        {
            source--;
        }
    }

    String readPosAsText()
    {
        return "\"" + (source? source->fileName : "[buffered-data]") + "\""
               " on line #" + String::asText(source? source->lineNumber : 0);
    }

    void setError(const String &message)
    {
        DED_SetError("In " + readPosAsText() + "\n  " + message);
    }

    /**
     * Reads a single character from the input file. Increments the line
     * number counter if necessary.
     */
    int FGetC(void)
    {
        int ch = (unsigned char) *source->pos;

        if (ch)
            source->pos++;
        else
            source->atEnd = true;
        if (ch == '\n')
            source->lineNumber++;
        if (ch == '\r')
            return FGetC();

        return ch;
    }

    /**
     * Undoes an FGetC.
     */
    int FUngetC(int ch)
    {
        if (source->atEnd)
            return 0;
        if (ch == '\n')
            source->lineNumber--;
        if (source->pos > source->buffer)
            source->pos--;

        return ch;
    }

    /**
     * Reads stuff until a newline is found.
     */
    void SkipComment(void)
    {
        int                 ch = FGetC();
        dd_bool             seq = false;

        if (ch == '\n')
            return; // Comment ends right away.

        if (ch != '>') // Single-line comment?
        {
            while (FGetC() != '\n' && !source->atEnd) {}
        }
        else // Multiline comment?
        {
            while (!source->atEnd)
            {
                ch = FGetC();
                if (seq)
                {
                    if (ch == '#')
                        break;
                    seq = false;
                }

                if (ch == '<')
                    seq = true;
            }
        }
    }

    int ReadToken(void)
    {
        int                 ch;
        char*               out = token;

        // Has a token been unread?
        if (unreadToken[0])
        {
            strcpy(token, unreadToken);
            strcpy(unreadToken, "");
            return true;
        }

        ch = FGetC();
        if (source->atEnd)
            return false;

        // Skip whitespace and comments in the beginning.
        while ((ch == '#' || isspace(ch)))
        {
            if (ch == '#')
                SkipComment();
            ch = FGetC();
            if (source->atEnd)
                return false;
        }

        // Always store the first character.
        *out++ = ch;
        if (STOPCHAR(ch))
        {
            // Stop here.
            *out = 0;
            return true;
        }

        while (!STOPCHAR(ch) && !source->atEnd)
        {
            // Store the character in the buffer.
            ch = FGetC();
            *out++ = ch;
        }
        *(out - 1) = 0; // End token.

        // Put the last read character back in the stream.
        FUngetC(ch);
        return true;
    }

    void UnreadToken(const char* token)
    {
        // The next time ReadToken() is called, this is returned.
        strcpy(unreadToken, token);
    }

    /**
     * Current pos in the file is at the first ".
     * Does not expand escape sequences, only checks for \".
     *
     * @return  @c true if successful.
     */
    int ReadString(String &dest, bool inside = false, bool doubleq = false)
    {
        if (!inside)
        {
            ReadToken();
            if (!ISTOKEN("\""))
                return false;
        }

        bool esc = false, newl = false;

        // Start reading the characters.
        int ch = FGetC();
        while (esc || ch != '"') // The string-end-character.
        {
            if (source->atEnd)
                return false;

            // If a newline is found, skip all whitespace that follows.
            if (newl)
            {
                if (isspace(ch))
                {
                    ch = FGetC();
                    continue;
                }
                else
                {
                    // End the skip.
                    newl = false;
                }
            }

            // An escape character?
            if (!esc && ch == '\\')
            {
                esc = true;
            }
            else
            {
                // In case it's something other than \" or \\, just insert
                // the whole sequence as-is.
                if (esc && ch != '"' && ch != '\\')
                    dest += '\\';
                esc = false;
            }
            if (ch == '\n')
                newl = true;

            // Store the character in the buffer.
            if (!esc && !newl)
            {
                dest += char(ch);
                if (doubleq && ch == '"')
                    dest += '"';
            }

            // Read the next character, please.
            ch = FGetC();
        }

        return true;
    }

    int ReadString(char *dest, int maxLen)
    {
        DE_ASSERT(dest != nullptr);
        String buffer;
        if (!ReadString(buffer)) return false;
        strncpy(dest, buffer, maxLen);
        return true;
    }

    int ReadString(Variable &var, int)
    {
        String buffer;
        if (!ReadString(buffer)) return false;
        var.set(TextValue(buffer));
        return true;
    }

    /**
     * Read a string of (pretty much) any length.
     */
    int ReadAnyString(char **dest)
    {
        String buffer;

        if (!ReadString(buffer))
            return false;

        // Get rid of the old string.
        if (*dest) M_Free(*dest);

        // Make a copy.
        *dest = (char *) M_Malloc(buffer.size() + 1);
        strcpy(*dest, buffer);

        return true;
    }

    int ReadUri(res::Uri **dest, const char *defaultScheme)
    {
        String buffer;

        if (!ReadString(buffer))
            return false;

        // URIs are expected to use forward slashes.
        buffer = Path::normalizeString(buffer);

        if (!*dest)
            *dest = new res::Uri(buffer, RC_NULL);
        else
            (*dest)->setUri(buffer, RC_NULL);

        if (defaultScheme && defaultScheme[0] && (*dest)->scheme().isEmpty())
            (*dest)->setScheme(defaultScheme);

        return true;
    }

    int ReadUri(Variable &var, const char *defaultScheme)
    {
        res::Uri *uri = 0;
        if (!ReadUri(&uri, defaultScheme)) return false;
        var.set(TextValue(uri->compose()));
        delete uri;
        return true;
    }

    int ReadNByteVector(Variable &var, int count)
    {
        FINDBEGIN;
        for (int i = 0; i < count; ++i)
        {
            ReadToken();
            if (ISTOKEN("}"))
                return true;
            var.array().setElement(i, strtoul(token, 0, 0));
        }
        FINDEND;
        return true;
    }

    /**
     * @return              @c true, if successful.
     */
    int ReadByte(unsigned char* dest)
    {
        ReadToken();
        if (ISTOKEN(";"))
        {
            setError("Missing integer value");
            return false;
        }

        *dest = strtoul(token, 0, 0);
        return true;
    }

    /**
     * @return              @c true, if successful.
     */
    int ReadInt(int* dest, int unsign)
    {
        ReadToken();
        if (ISTOKEN(";"))
        {
            setError("Missing integer value");
            return false;
        }

        *dest = unsign? (int)strtoul(token, 0, 0) : strtol(token, 0, 0);
        return true;
    }

    int ReadInt(Variable *var, int unsign)
    {
        int value = 0;
        if (ReadInt(&value, unsign))
        {
            var->set(NumberValue(value));
            return true;
        }
        return false;
    }

    /**
     * @return              @c true, if successful.
     */
    int ReadFloat(float* dest)
    {
        ReadToken();
        if (ISTOKEN(";"))
        {
            setError("Missing float value");
            return false;
        }

        *dest = (float) strtod(token, 0);
        return true;
    }

    int ReadFloat(Variable *var)
    {
        float value = 0;
        if (ReadFloat(&value))
        {
            var->set(NumberValue(value));
            return true;
        }
        return false;
    }

    int ReadVector(Variable &var, int componentCount)
    {
        FINDBEGIN;
        for (int b = 0; b < componentCount; ++b)
        {
            float value = 0;
            if (!ReadFloat(&value)) return false;
            var.array().setElement(b, value);
        }
        ReadToken();
        return true;
    }

    int ReadFlags(int *dest, const char *prefix)
    {
        DE_ASSERT(dest);

        // By default, no flags are set.
        *dest = 0;

        ReadToken();
        if (ISTOKEN(";"))
        {
            setError("Missing flags value");
            return false;
        }
        if (ISTOKEN("0"))
        {
            // No flags defined.
            return true;
        }

        UnreadToken(token);
        String flag;
        if (ISTOKEN("\""))
        {
            // The old format.
            if (!ReadString(flag))
                return false;

            flag.strip();

            if (!flag.isEmpty())
            {
                *dest = ded->evalFlags(flag);
            }
            return true;
        }

        for (;;)
        {
            // Read the flag.
            ReadToken();
            if (prefix)
            {
                flag = String(prefix) + String(token);
            }
            else
            {
                flag = String(token);
            }

            flag.strip();

            if (!flag.isEmpty())
            {
                *dest |= ded->evalFlags(flag);
            }

            if (!ReadToken())
                break;

            if (!ISTOKEN("|")) // | is required for multiple flags.
            {
                UnreadToken(token);
                break;
            }
        }

        return true;
    }

    int ReadFlags(Variable *dest, const char *prefix, int elementIndex = -1)
    {
        int value = 0;
        if (ReadFlags(&value, prefix))
        {
            std::unique_ptr<NumberValue> flagsValue(new NumberValue(value, NumberValue::Hex));
            if (elementIndex < 0)
            {
                dest->set(flagsValue.release());
            }
            else
            {
                dest->array().setElement(NumberValue(elementIndex), flagsValue.release());
            }
            return true;
        }
        return false;
    }

    int ReadBlendmode(blendmode_t *dest)
    {
        LOG_AS("ReadBlendmode");

        String flag;
        blendmode_t bm;

        ReadToken();
        UnreadToken(token);
        if (ISTOKEN("\""))
        {
            // The old format.
            if (!ReadString(flag)) return false;

            bm = blendmode_t(ded->evalFlags(flag));
        }
        else
        {
            // Read the blendmode.
            ReadToken();

            flag = String("bm_") + String(token);

            bm = blendmode_t(ded->evalFlags(flag));
        }

        if (bm != BM_NORMAL)
        {
            *dest = bm;
        }
        else
        {
            LOG_RES_WARNING("Unknown BlendMode '%s' in \"%s\" on line #%i")
                << flag << (source ? source->fileName : "?") << (source ? source->lineNumber : 0);
        }

        return true;
    }

    int ReadBlendmode(Variable *var)
    {
        blendmode_t mode = BM_NORMAL;
        if (!ReadBlendmode(&mode)) return false;
        var->set(NumberValue(int(mode)));
        return true;
    }

    /**
     * @return @c true, if successful.
     */
    int ReadLabel(char* label)
    {
        strcpy(label, "");
        for (;;)
        {
            ReadToken();
            if (source->atEnd)
            {
                setError("Unexpected end of file");
                return false;
            }
            if (ISTOKEN("}")) // End block.
            {
                strcpy(label, token);
                return true;
            }
            if (ISTOKEN(";"))
            {
                if (source->version <= 5)
                {
                    setError("Label without value");
                    return false;
                }
                continue; // Semicolons are optional in v6.
            }
            if (ISTOKEN("=") || ISTOKEN("{"))
                break;

            if (label[0])
                strcat(label, " ");
            strcat(label, token);
        }

        return true;
    }

    /**
     * @param cond          Condition token. Can be a command line option
     *                      or a game mode.
     * @return              @c true if the condition passes.
     */
    dd_bool DED_CheckCondition(const char *cond, dd_bool expected)
    {
        dd_bool value = false;

        if (cond[0] == '-')
        {
            // A command line option.
            value = (CommandLine_Check(token) != 0);
        }
        else if (isalnum(cond[0]) && !DoomsdayApp::game().isNull())
        {
            // A game mode.
            value = !String(cond).compareWithoutCase(DoomsdayApp::game().id());
        }

        return value == expected;
    }

    int readData(const char *buffer, String sourceFile, bool sourceIsCustom)
    {
        const String &VAR_ID = defn::Definition::VAR_ID;

        char  dummy[128], label[128], tmp[256];
        int   dummyInt, idx, retVal = true;
        int   prevEpisodeDefIdx = -1; // For "Copy".
        int   prevMobjDefIdx = -1; // For "Copy".
        int   prevStateDefIdx = -1; // For "Copy"
        int   prevLightDefIdx = -1; // For "Copy".
        int   prevMaterialDefIdx = -1; // For "Copy".
        int   prevModelDefIdx = -1; // For "Copy".
        int   prevMapInfoDefIdx = -1; // For "Copy".
        int   prevMusicDefIdx = -1; // For "Copy".
        int   prevSkyDefIdx = -1; // For "Copy".
        int   prevDetailDefIdx = -1; // For "Copy".
        int   prevGenDefIdx = -1; // For "Copy".
        int   prevDecorDefIdx = -1; // For "Copy".
        int   prevRefDefIdx = -1; // For "Copy".
        int   prevLineTypeDefIdx = -1; // For "Copy".
        int   prevSectorTypeDefIdx = -1; // For "Copy".
        int   depth;
        char *rootStr = 0, *ptr;
        int   bCopyNext = 0;

        // Get the next entry from the source stack.
        DED_InitReader(buffer, sourceFile, sourceIsCustom);

        // For including other files -- we must know where we are.
        String sourceFileDir = sourceFile.fileNamePath();
        if (sourceFileDir.isEmpty())
        {
            sourceFileDir = NativePath::workPath();
        }

        while (ReadToken())
        {
            if (ISTOKEN("Copy") || ISTOKEN("*"))
            {
                bCopyNext = true;
                continue; // Read the next token.
            }

            if (ISTOKEN(";"))
            {
                // Unnecessary semicolon? Just skip it.
                continue;
            }

            if (ISTOKEN("SkipIf"))
            {
                dd_bool expected = true;

                ReadToken();
                if (ISTOKEN("Not"))
                {
                    expected = false;
                    ReadToken();
                }

                if (DED_CheckCondition(token, expected))
                {
                    // Ah, we're done. Get out of here.
                    goto ded_end_read;
                }
                CHECKSC;
            }

            if (ISTOKEN("Include"))
            {
                // A new include.
                READSTR(tmp);
                CHECKSC;

                DED_Include(tmp, sourceFileDir);
                strcpy(label, "");
            }

            if (ISTOKEN("IncludeIf")) // An optional include.
            {
                dd_bool expected = true;

                ReadToken();
                if (ISTOKEN("Not"))
                {
                    expected = false;
                    ReadToken();
                }

                if (DED_CheckCondition(token, expected))
                {
                    READSTR(tmp);
                    CHECKSC;

                    DED_Include(tmp, sourceFileDir);
                    strcpy(label, "");
                }
                else
                {
                    // Arg not found; just skip it.
                    READSTR(tmp);
                    CHECKSC;
                }
            }

            if (ISTOKEN("ModelPath"))
            {
                READSTR(label);
                CHECKSC;

                res::Uri newSearchPath = res::Uri::fromNativeDirPath(NativePath(label));
                FS1::Scheme& scheme = App_FileSystem().scheme(ResourceClass::classForId(RC_MODEL).defaultScheme());
                scheme.addSearchPath(reinterpret_cast<res::Uri const&>(newSearchPath), FS1::ExtraPaths);
            }

            if (ISTOKEN("Header"))
            {
                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    if (ISLABEL("Version"))
                    {
                        READINT(ded->version);
                        source->version = ded->version;
                    }
                    else RV_STR("Thing prefix", dummy)
                    RV_STR("State prefix", dummy)
                    RV_STR("Sprite prefix", dummy)
                    RV_STR("Sfx prefix", dummy)
                    RV_STR("Mus prefix", dummy)
                    RV_STR("Text prefix", dummy)
                    RV_STR("Model path", dummy)
                    RV_FLAGS("Common model flags", ded->modelFlags, "df_")
                    RV_FLT("Default model scale", ded->modelScale)
                    RV_FLT("Default model offset", ded->modelOffset)
                    RV_END
                    CHECKSC;
                }
            }

            if (ISTOKEN("Flag"))
            {
                ded_stringid_t id;
                int value = 0;
                char dummyStr[2];

                de::zap(id);

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", id)
                    RV_UINT("Value", value)
                    RV_STR("Info", dummyStr) // ignored
                    RV_END
                    CHECKSC;
                }

                if (strlen(id))
                {
                    ded->addFlag(id, value);
                    Record &flag = ded->flags.find(VAR_ID, id);
                    DE_ASSERT(flag.geti("value") == value); // Sanity check.
                    if (source->custom) flag.set("custom", true);
                }
            }

            if (ISTOKEN("Episode"))
            {
                bool bModify = false;
                Record dummyEpsd;
                Record *epsd = 0;

                ReadToken();
                if (!ISTOKEN("Mods"))
                {
                    // New episodes are appended to the end of the list.
                    idx = ded->addEpisode();
                    epsd = &ded->episodes[idx];
                }
                else if (!bCopyNext)
                {
                    ded_stringid_t otherEpisodeId;

                    READSTR(otherEpisodeId);
                    ReadToken();

                    idx = ded->getEpisodeNum(otherEpisodeId);
                    if (idx >= 0)
                    {
                        epsd = &ded->episodes[idx];
                        bModify = true;
                    }
                    else
                    {
                        LOG_RES_WARNING("Ignoring unknown Episode \"%s\" in %s on line #%i")
                                << otherEpisodeId << (source? source->fileName : "?")
                                << (source? source->lineNumber : 0);

                        // We'll read into a dummy definition.
                        defn::Episode(dummyEpsd).resetToDefaults();
                        epsd = &dummyEpsd;
                    }
                }
                else
                {
                    setError("Cannot both Copy(Previous) and Modify");
                    retVal = false;
                    goto ded_end_read;
                }
                DE_ASSERT(epsd != 0);

                if (prevEpisodeDefIdx >= 0 && bCopyNext)
                {
                    ded->episodes.copy(prevEpisodeDefIdx, *epsd);
                }
                if (source->custom) epsd->set("custom", true);

                defn::Episode mainDef(*epsd);
                int hub = 0;
                int notHubMap = 0;
                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    // ID cannot be changed when modifying
                    if (!bModify && ISLABEL("ID"))
                    {
                        READSTR((*epsd)[VAR_ID]);
                    }
                    else RV_URI("Start Map", (*epsd)["startMap"], "Maps")
                    RV_STR("Title", (*epsd)["title"])
                    RV_STR("Menu Help Info", (*epsd)["menuHelpInfo"])
                    RV_URI("Menu Image", (*epsd)["menuImage"], "Patches")
                    RV_STR("Menu Shortcut", (*epsd)["menuShortcut"])
                    if (ISLABEL("Hub"))
                    {
                        Record *hubRec;
                        // Add another hub.
                        if (hub >= mainDef.hubCount())
                        {
                            mainDef.addHub();
                        }
                        DE_ASSERT(hub < mainDef.hubCount());
                        hubRec = &mainDef.hub(hub);
                        if (source->custom) hubRec->set("custom", true);

                        int map = 0;
                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_STR("ID", (*hubRec)[VAR_ID])
                            if (ISLABEL("Map"))
                            {
                                // Add another map.
                                if (map >= int(hubRec->geta("map").size()))
                                {
                                    std::unique_ptr<Record> map(new Record);
                                    defn::MapGraphNode(*map).resetToDefaults();
                                    (*hubRec)["map"].array()
                                            .add(new RecordValue(map.release(), RecordValue::OwnsRecord));
                                }
                                DE_ASSERT(map < int(hubRec->geta("map").size()));
                                Record &mapRec = *hubRec->geta("map")[map].as<RecordValue>().record();
                                if (source->custom) mapRec.set("custom", true);

                                int exit = 0;
                                FINDBEGIN;
                                for (;;)
                                {
                                    READLABEL;
                                    RV_URI("ID", mapRec[VAR_ID], "Maps")
                                    RV_INT("Warp Number", mapRec["warpNumber"])
                                    if (ISLABEL("Exit"))
                                    {
                                        defn::MapGraphNode mgNodeDef(mapRec);

                                        // Add another exit.
                                        if (exit >= mgNodeDef.exitCount())
                                        {
                                            mgNodeDef.addExit();
                                        }
                                        DE_ASSERT(exit < mgNodeDef.exitCount());
                                        Record &exitRec = mgNodeDef.exit(exit);
                                        if (source->custom) exitRec.set("custom", true);

                                        FINDBEGIN;
                                        for (;;)
                                        {
                                            READLABEL;
                                            RV_STR("ID", exitRec[VAR_ID])
                                            RV_URI("Target Map", exitRec["targetMap"], "Maps")
                                            RV_END
                                            CHECKSC;
                                        }
                                        exit++;
                                    }
                                    else RV_END
                                    CHECKSC;
                                }
                                map++;
                            }
                            else RV_END
                            CHECKSC;
                        }
                        hub++;
                    }
                    else if (ISLABEL("Map"))
                    {
                        // Add another none-hub map.
                        if (notHubMap >= int(epsd->geta("map").size()))
                        {
                            std::unique_ptr<Record> map(new Record);
                            defn::MapGraphNode(*map).resetToDefaults();
                            (*epsd)["map"].array()
                                    .add(new RecordValue(map.release(), RecordValue::OwnsRecord));
                        }
                        DE_ASSERT(notHubMap < int(epsd->geta("map").size()));
                        Record &mapRec = *epsd->geta("map")[notHubMap].as<RecordValue>().record();
                        if (source->custom) mapRec.set("custom", true);

                        int exit = 0;
                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_URI("ID", mapRec[VAR_ID], "Maps")
                            RV_INT("Warp Number", mapRec["warpNumber"])
                            if (ISLABEL("Exit"))
                            {
                                defn::MapGraphNode mgNodeDef(mapRec);

                                // Add another exit.
                                if (exit >= mgNodeDef.exitCount())
                                {
                                    mgNodeDef.addExit();
                                }
                                DE_ASSERT(exit < mgNodeDef.exitCount());
                                Record &exitRec = mgNodeDef.exit(exit);
                                if (source->custom) exitRec.set("custom", true);

                                FINDBEGIN;
                                for (;;)
                                {
                                    READLABEL;
                                    RV_STR("ID", exitRec[VAR_ID])
                                    RV_URI("Target Map", exitRec["targetMap"], "Maps")
                                    RV_END
                                    CHECKSC;
                                }
                                exit++;
                            }
                            else RV_END
                            SKIPSC;
                        }
                        notHubMap++;
                    }
                    else RV_END
                    SKIPSC;
                }

                // If we did not read into a dummy update the previous index.
                if (idx > 0)
                {
                    prevEpisodeDefIdx = idx;
                }
            }

            if (ISTOKEN("Mobj") || ISTOKEN("Thing"))
            {
                dd_bool bModify = false;
                Record* mo;
                Record dummyMo;

                ReadToken();
                if (!ISTOKEN("Mods"))
                {
                    // A new mobj type.
                    idx = ded->addThing("");
                    mo = &ded->things[idx];
                }
                else if (!bCopyNext)
                {
                    String otherMobjId;
                    if (!ReadString(otherMobjId)) { FAILURE }
                    ReadToken();

                    idx = ded->getMobjNum(otherMobjId);
                    if (idx < 0)
                    {
                        LOG_RES_WARNING("Ignoring unknown Mobj \"%s\" in %s on line #%i")
                                << otherMobjId << (source? source->fileName : "?")
                                << (source? source->lineNumber : 0);

                        // We'll read into a dummy definition.
                        dummyMo.clear();
                        mo = &dummyMo;
                    }
                    else
                    {
                        mo = &ded->things[idx];
                        bModify = true;
                    }
                }
                else
                {
                    setError("Cannot both Copy(Previous) and Modify");
                    retVal = false;
                    goto ded_end_read;
                }

                if (prevMobjDefIdx >= 0 && bCopyNext)
                {
                    // Should we copy the previous definition?
                    ded->things.copy(prevMobjDefIdx, *mo);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    // ID cannot be changed when modifying
                    if (!bModify && ISLABEL("ID"))
                    {
                        READSTR((*mo)[VAR_ID]);
                    }
                    else RV_INT("DoomEd number", (*mo)["doomEdNum"])
                    RV_STR("Name", (*mo)["name"])

                    RV_STR_ELEM("Spawn state",   (*mo)["states"], SN_SPAWN)
                    RV_STR_ELEM("See state",     (*mo)["states"], SN_SEE)
                    RV_STR_ELEM("Pain state",    (*mo)["states"], SN_PAIN)
                    RV_STR_ELEM("Melee state",   (*mo)["states"], SN_MELEE)
                    RV_STR_ELEM("Missile state", (*mo)["states"], SN_MISSILE)
                    RV_STR_ELEM("Crash state",   (*mo)["states"], SN_CRASH)
                    RV_STR_ELEM("Death state",   (*mo)["states"], SN_DEATH)
                    RV_STR_ELEM("Xdeath state",  (*mo)["states"], SN_XDEATH)
                    RV_STR_ELEM("Raise state",   (*mo)["states"], SN_RAISE)

                    RV_STR_ELEM("See sound",    (*mo)["sounds"], SDN_SEE)
                    RV_STR_ELEM("Attack sound", (*mo)["sounds"], SDN_ATTACK)
                    RV_STR_ELEM("Pain sound",   (*mo)["sounds"], SDN_PAIN)
                    RV_STR_ELEM("Death sound",  (*mo)["sounds"], SDN_DEATH)
                    RV_STR_ELEM("Active sound", (*mo)["sounds"], SDN_ACTIVE)

                    RV_INT("Reaction time", (*mo)["reactionTime"])
                    RV_INT("Pain chance", (*mo)["painChance"])
                    RV_INT("Spawn health", (*mo)["spawnHealth"])
                    RV_FLT("Speed", (*mo)["speed"])
                    RV_FLT("Radius", (*mo)["radius"])
                    RV_FLT("Height", (*mo)["height"])
                    RV_INT("Mass", (*mo)["mass"])
                    RV_INT("Damage", (*mo)["damage"])
                    RV_FLAGS_ELEM("Flags", (*mo)["flags"], 0, "mf_")
                    RV_FLAGS_ELEM("Flags2", (*mo)["flags"], 1, "mf2_")
                    RV_FLAGS_ELEM("Flags3", (*mo)["flags"], 2, "mf3_")
                    RV_INT_ELEM("Misc1", (*mo)["misc"], 0)
                    RV_INT_ELEM("Misc2", (*mo)["misc"], 1)
                    RV_INT_ELEM("Misc3", (*mo)["misc"], 2)
                    RV_INT_ELEM("Misc4", (*mo)["misc"], 3)

                    RV_STR("On touch", (*mo)["onTouch"]) // script function (MF_SPECIAL)
                    RV_STR("On death", (*mo)["onDeath"]) // script function

                    RV_END
                    CHECKSC;
                }

                // If we did not read into a dummy update the previous index.
                if (idx > 0)
                {
                    prevMobjDefIdx = idx;
                }
            }

            if (ISTOKEN("State"))
            {
                dd_bool bModify = false;
                Record *st;
                Record dummyState;

                ReadToken();
                if (!ISTOKEN("Mods"))
                {
                    // A new state.
                    idx = ded->addState("");
                    st = &ded->states[idx];
                }
                else if (!bCopyNext)
                {
                    ded_stateid_t otherStateId;

                    READSTR(otherStateId);
                    ReadToken();

                    idx = ded->getStateNum(otherStateId);
                    if (idx < 0)
                    {
                        LOG_RES_WARNING("Ignoring unknown State \"%s\" in %s on line #%i")
                                << otherStateId << (source? source->fileName : "?")
                                << (source? source->lineNumber : 0);

                        // We'll read into a dummy definition.
                        dummyState.clear();
                        st = &dummyState;
                    }
                    else
                    {
                        st = &ded->states[idx];
                        bModify = true;
                    }
                }
                else
                {
                    setError("Cannot both Copy(Previous) and Modify");
                    retVal = false;
                    goto ded_end_read;
                }

                if (prevStateDefIdx >= 0 && bCopyNext)
                {
                    // Should we copy the previous definition?
                    ded->states.copy(prevStateDefIdx, *st);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    // ID cannot be changed when modifying
                    if (!bModify && ISLABEL("ID"))
                    {
                        READSTR((*st)[VAR_ID]);
                    }
                    else if (ISLABEL("Frame"))
                    {
    // Legacy state frame flags:
    #define FF_FULLBRIGHT               0x8000
    #define FF_FRAMEMASK                0x7fff

                        int frame = 0;
                        READINT(frame);
                        if (frame & FF_FULLBRIGHT)
                        {
                            frame &= FF_FRAMEMASK;
                            st->set("flags", st->geti("flags") | STF_FULLBRIGHT);
                        }
                        st->set("frame", frame);

    #undef FF_FRAMEMASK
    #undef FF_FULLBRIGHT
                    }
                    else RV_FLAGS("Flags", (*st)["flags"], "statef_")
                    RV_STR("Sprite",     (*st)["sprite"])
                    RV_INT("Tics",       (*st)["tics"])
                    RV_STR("Action",     (*st)["action"])
                    RV_STR("Next state", (*st)["nextState"])
                    RV_INT_ELEM("Misc1", (*st)["misc"], 0)
                    RV_INT_ELEM("Misc2", (*st)["misc"], 1)
                    RV_INT_ELEM("Misc3", (*st)["misc"], 2)
                    RV_STR("Execute",    (*st)["execute"])
                    RV_END
                    CHECKSC;
                }

                // If we did not read into a dummy update the previous index.
                if (idx > 0)
                {
                    prevStateDefIdx = idx;
                }
            }

            if (ISTOKEN("Sprite"))
            {
                // A new sprite.
                idx = DED_AddSprite(ded, "");
                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", ded->sprites[idx].id)
                    RV_END
                    CHECKSC;
                }
            }

            if (ISTOKEN("Light"))
            {
                ded_light_t* lig;

                // A new light.
                idx = DED_AddLight(ded, "");
                lig = &ded->lights[idx];

                // Should we copy the previous definition?
                if (prevLightDefIdx >= 0 && bCopyNext)
                {
                    ded->lights.copyTo(lig, prevLightDefIdx);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("State", lig->state)
                    RV_STR("Map", lig->uniqueMapID)
                    RV_FLT("X Offset", lig->offset[VX])
                    RV_FLT("Y Offset", lig->offset[VY])
                    RV_VEC("Origin", lig->offset, 3)
                    RV_FLT("Size", lig->size)
                    RV_FLT("Intensity", lig->size)
                    RV_FLT("Red", lig->color[0])
                    RV_FLT("Green", lig->color[1])
                    RV_FLT("Blue", lig->color[2])
                    RV_VEC("Color", lig->color, 3)
                    if (ISLABEL("Sector levels"))
                    {
                        int     b;
                        FINDBEGIN;
                        for (b = 0; b < 2; ++b)
                        {
                            READFLT(lig->lightLevel[b])
                            lig->lightLevel[b] /= 255.0f;
                            if (lig->lightLevel[b] < 0)
                                lig->lightLevel[b] = 0;
                            else if (lig->lightLevel[b] > 1)
                                lig->lightLevel[b] = 1;
                        }
                        ReadToken();
                    }
                    else
                    RV_FLAGS("Flags", lig->flags, "lgf_")
                    RV_URI("Top map", &lig->up, "LightMaps")
                    RV_URI("Bottom map", &lig->down, "LightMaps")
                    RV_URI("Side map", &lig->sides, "LightMaps")
                    RV_URI("Flare map", &lig->flare, "LightMaps")
                    RV_FLT("Halo radius", lig->haloRadius)
                    RV_END
                    CHECKSC;
                }
                prevLightDefIdx = idx;
            }

            if (ISTOKEN("material.h"))
            {
                Record dummyMat;
                Record *mat  = nullptr;
                bool bModify = false;

                ReadToken();
                if (!ISTOKEN("Mods"))
                {
                    // New materials are appended to the end of the list.
                    idx = ded->addMaterial();
                    mat = &ded->materials[idx];
                }
                else if (!bCopyNext)
                {
                    res::Uri *otherMat = nullptr;
                    READURI(&otherMat, nullptr);
                    ReadToken();

                    idx = ded->getMaterialNum(*otherMat);
                    if (idx >= 0)
                    {
                        mat     = &ded->materials[idx];
                        bModify = true;
                    }
                    else
                    {
                        LOG_RES_WARNING("Ignoring unknown Material \"%s\" in %s on line #%i")
                                << otherMat->asText()
                                << (source? source->fileName : "?")
                                << (source? source->lineNumber : 0);

                        // We'll read into a dummy definition.
                        defn::Material(dummyMat).resetToDefaults();
                        mat = &dummyMat;
                    }

                    delete otherMat;
                }
                else
                {
                    setError("Cannot both Copy(Previous) and Modify");
                    retVal = false;
                    goto ded_end_read;
                }
                DE_ASSERT(mat);

                // Should we copy the previous definition?
                if (prevMaterialDefIdx >= 0 && bCopyNext)
                {
                    ded->materials.copy(prevMaterialDefIdx, *mat);
                }

                defn::Material mainDef(*mat);
                int decor = 0;
                int layer = 0;
                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    // ID cannot be changed when modifying
                    if (!bModify && ISLABEL("ID"))
                    {
                        READURI((*mat)[VAR_ID], nullptr);
                    }
                    else
                    RV_FLAGS("Flags", (*mat)["flags"], "matf_")
                    RV_INT_ELEM("Width", (*mat)["dimensions"], 0)
                    RV_INT_ELEM("Height", (*mat)["dimensions"], 1)
                    if (ISLABEL("Layer"))
                    {
                        if (layer >= DED_MAX_MATERIAL_LAYERS)
                        {
                            setError("Too many Material.Layers");
                            retVal = false;
                            goto ded_end_read;
                        }

                        // Add another layer.
                        if (layer >= mainDef.layerCount()) mainDef.addLayer();
                        defn::MaterialLayer layerDef(mainDef.layer(layer));

                        int stage = 0;
                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            if (ISLABEL("Stage"))
                            {
                                // Need to allocate a new stage?
                                if (stage >= layerDef.stageCount())
                                {
                                    layerDef.addStage();

                                    if (mainDef.getb("autoGenerated") && stage > 0)
                                    {
                                        // When adding a new stage to an autogenerated material
                                        // initialize by copying values from the previous stage.
                                        layerDef.stage(stage).copyMembersFrom(layerDef.stage(stage - 1));
                                    }
                                }
                                Record &st = layerDef.stage(stage);

                                FINDBEGIN;
                                for (;;)
                                {
                                    READLABEL;
                                    RV_URI("Texture", st["texture"], 0) // Default to "any" scheme.
                                    RV_INT("Tics", st["tics"])
                                    RV_FLT("Rnd", st["variance"])
                                    RV_VEC_VAR("Offset", st["texOrigin"], 2)
                                    RV_FLT("Glow Rnd", st["glowStrengthVariance"])
                                    RV_FLT("Glow", st["glowStrength"])
                                    RV_END
                                    CHECKSC;
                                }
                                stage++;
                            }
                            else RV_END
                            CHECKSC;
                        }
                        layer++;
                    }
                    else if (ISLABEL("Light"))
                    {
                        if (decor >= DED_MAX_MATERIAL_DECORATIONS)
                        {
                            setError("Too many Material.Lights");
                            retVal = false;
                            goto ded_end_read;
                        }

                        // Add another decoration.
                        if (decor >= mainDef.decorationCount()) mainDef.addDecoration();
                        defn::MaterialDecoration decorDef(mainDef.decoration(decor));

                        int stage = 0;
                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_VEC_VAR("Pattern offset", decorDef.def()["patternOffset"], 2)
                            RV_VEC_VAR("Pattern skip", decorDef.def()["patternSkip"], 2)
                            if (ISLABEL("Stage"))
                            {
                                // Need to allocate a new stage?
                                if (stage >= decorDef.stageCount())
                                {
                                    decorDef.addStage();
                                }
                                Record &st = decorDef.stage(stage);

                                FINDBEGIN;
                                for (;;)
                                {
                                    READLABEL;
                                    RV_INT("Tics", st["tics"])
                                    RV_FLT("Rnd", st["variance"])
                                    RV_VEC_VAR("Offset", st["origin"], 2)
                                    RV_FLT("Distance", st["elevation"])
                                    RV_VEC_VAR("Color", st["color"], 3)
                                    RV_FLT("Radius", st["radius"])
                                    RV_FLT("Halo radius", st["haloRadius"])
                                    if (ISLABEL("Levels"))
                                    {
                                        FINDBEGIN;
                                        Vec2f levels;
                                        for (int b = 0; b < 2; ++b)
                                        {
                                            float val;
                                            READFLT(val)
                                            levels[b] = de::clamp(0.f, val / 255.0f, 1.f);
                                        }
                                        ReadToken();
                                        st["lightLevels"] = new ArrayValue(levels);
                                    }
                                    else
                                    RV_INT("Flare texture", st["haloTextureIndex"])
                                    RV_URI("Flare map", st["haloTexture"], "LightMaps")
                                    RV_URI("Top map", st["lightmapUp"], "LightMaps")
                                    RV_URI("Bottom map", st["lightmapDown"], "LightMaps")
                                    RV_URI("Side map", st["lightmapSide"], "LightMaps")
                                    RV_END
                                    CHECKSC;
                                }
                                stage++;
                            }
                            else RV_END
                            CHECKSC;
                        }
                        decor++;
                    }
                    else RV_END
                    CHECKSC;
                }

                // If we did not read into a dummy update the previous index.
                if (idx > 0)
                {
                    prevMaterialDefIdx = idx;
                }
            }

            if (ISTOKEN("Model"))
            {
                Record *prevModel = 0;
                int sub = 0;

                // New models are appended to the end of the list.
                idx = ded->addModel();
                Record &mdl = ded->models[idx];

                if (prevModelDefIdx >= 0)
                {
                    prevModel = &ded->models[prevModelDefIdx];

                    if (bCopyNext) ded->models.copy(prevModelDefIdx, mdl);
                }
                if (source->custom) mdl.set("custom", true);

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", mdl[VAR_ID])
                    RV_STR("State", mdl["state"])
                    RV_INT("Off", mdl["off"])
                    RV_STR("Sprite", mdl["sprite"])
                    RV_INT("Sprite frame", mdl["spriteFrame"])
                    RV_FLAGS("Group", mdl["group"], "mg_")
                    RV_INT("Selector", mdl["selector"])
                    RV_FLAGS("Flags", mdl["flags"], "df_")
                    RV_FLT("Inter", mdl["interMark"])
                    RV_INT("Skin tics", mdl["skinTics"])
                    RV_FLT("Resize", mdl["resize"])
                    if (ISLABEL("Scale"))
                    {
                        float scale; READFLT(scale);
                        mdl["scale"] = new ArrayValue(Vec3f(scale, scale, scale));
                    }
                    else
                    RV_VEC_VAR("Scale XYZ", mdl["scale"], 3)
                    RV_FLT_ELEM("Offset", mdl["offset"], 1)
                    RV_VEC_VAR("Offset XYZ", mdl["offset"], 3)
                    RV_VEC_VAR("Interpolate", mdl["interRange"], 2)
                    RV_FLT("Shadow radius", mdl["shadowRadius"])
                    if (ISLABEL("Md2") || ISLABEL("Sub"))
                    {
                        defn::Model mainDef(mdl);

                        // Add another submodel.
                        if (sub >= mainDef.subCount())
                        {
                            mainDef.addSub();
                        }
                        DE_ASSERT(sub < mainDef.subCount());

                        Record &subDef = mainDef.sub(sub);

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_URI("File", subDef["filename"], "Models")
                            RV_STR("Frame", subDef["frame"])
                            RV_INT("Frame range", subDef["frameRange"])
                            RV_BLENDMODE("Blending mode", subDef["blendMode"])
                            RV_INT("Skin", subDef["skin"])
                            RV_URI("Skin file", subDef["skinFilename"], "Models")
                            RV_INT("Skin range", subDef["skinRange"])
                            RV_VEC_VAR("Offset XYZ", subDef["offset"], 3)
                            RV_FLAGS("Flags", subDef["flags"], "df_")
                            RV_FLT("Transparent", subDef["alpha"])
                            RV_FLT("Parm", subDef["parm"])
                            RV_INT("Selskin mask", subDef["selSkinMask"])
                            RV_INT("Selskin shift", subDef["selSkinShift"])
                            RV_NBVEC("Selskins", subDef["selSkins"], 8)
                            RV_URI("Shiny skin", subDef["shinySkin"], "Models")
                            RV_FLT("Shiny", subDef["shiny"])
                            RV_VEC_VAR("Shiny color", subDef["shinyColor"], 3)
                            RV_FLT("Shiny reaction", subDef["shinyReact"])
                            RV_END
                            CHECKSC;
                        }
                        sub++;
                    }
                    else RV_END
                    CHECKSC;
                }

                if (prevModel)
                {
                    defn::Model(mdl).cleanupAfterParsing(*prevModel);
                }

                prevModelDefIdx = idx;
            }

            if (ISTOKEN("Sound"))
            {   // A new sound.
                ded_sound_t*        snd;

                idx = DED_AddSound(ded, "");
                snd = &ded->sounds[idx];

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", snd->id)
                    RV_STR("Lump", snd->lumpName)
                    RV_STR("Name", snd->name)
                    RV_STR("Link", snd->link)
                    RV_INT("Link pitch", snd->linkPitch)
                    RV_INT("Link volume", snd->linkVolume)
                    RV_INT("Priority", snd->priority)
                    RV_INT("Max channels", snd->channels)
                    RV_INT("Group", snd->group)
                    RV_FLAGS("Flags", snd->flags, "sf_")
                    RV_URI("Ext", &snd->ext, "Sfx")
                    RV_URI("File", &snd->ext, "Sfx")
                    RV_URI("File name", &snd->ext, "Sfx")
                    RV_END
                    CHECKSC;
                }
            }

            if (ISTOKEN("Music"))
            {
                bool bModify = false;
                Record dummyMusic;
                Record *music = 0;

                ReadToken();
                if (!ISTOKEN("Mods"))
                {
                    // New musics are appended to the end of the list.
                    idx = ded->addMusic();
                    music = &ded->musics[idx];
                }
                else if (!bCopyNext)
                {
                    ded_stringid_t otherMusicId;

                    READSTR(otherMusicId);
                    ReadToken();

                    idx = ded->getMusicNum(otherMusicId);
                    if (idx >= 0)
                    {
                        music = &ded->musics[idx];
                        bModify = true;
                    }
                    else
                    {
                        // Don't print a warning about the translated MAPINFO definitions
                        // (see issue #2083).
                        if (!source || source->fileName != "[TranslatedMapInfos]")
                        {
                            LOG_RES_WARNING("Ignoring unknown Music \"%s\" in %s on line #%i")
                            << otherMusicId << (source? source->fileName : "?")
                            << (source? source->lineNumber : 0);
                        }

                        // We'll read into a dummy definition.
                        defn::Music(dummyMusic).resetToDefaults();
                        music = &dummyMusic;
                    }
                }
                else
                {
                    setError("Cannot both Copy(Previous) and Modify");
                    retVal = false;
                    goto ded_end_read;
                }
                DE_ASSERT(music != 0);

                if (prevMusicDefIdx >= 0 && bCopyNext)
                {
                    ded->musics.copy(prevMusicDefIdx, *music);
                }
                if (source->custom) music->set("custom", true);

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    // ID cannot be changed when modifying
                    if (!bModify && ISLABEL("ID"))
                    {
                        READSTR((*music)[VAR_ID]);
                    }
                    else RV_STR("Name", (*music)["title"])
                    RV_STR("Lump", (*music)["lumpName"])
                    RV_URI("File name", (*music)["path"], "Music")
                    RV_URI("File", (*music)["path"], "Music")
                    RV_URI("Ext", (*music)["path"], "Music") // Both work.
                    RV_INT("CD track", (*music)["cdTrack"])
                    RV_END
                    CHECKSC;
                }

                // If we did not read into a dummy update the previous index.
                if (idx > 0)
                {
                    prevMusicDefIdx = idx;
                }
            }

            if (ISTOKEN("Sky"))
            {
                int model = 0;

                // New skies are appended to the end of the list.
                idx = ded->addSky();
                Record &sky = ded->skies[idx];

                if (prevSkyDefIdx >= 0 && bCopyNext)
                {
                    //Record *prevSky = &ded->skies[prevSkyDefIdx];
                    ded->skies.copy(prevSkyDefIdx, sky);
                }
                if (source->custom) sky.set("custom", true);

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", sky[VAR_ID])
                    RV_FLAGS("Flags", sky["flags"], "sif_")
                    RV_FLT("Height", sky["height"])
                    RV_FLT("Horizon offset", sky["horizonOffset"])
                    RV_VEC_VAR("Light color", sky["color"], 3)
                    if (ISLABEL("Layer 1") || ISLABEL("Layer 2"))
                    {
                        defn::Sky mainDef(sky);
                        Record &layerDef = mainDef.layer(atoi(label+6) - 1);
                        if (source->custom) layerDef.set("custom", true);

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_URI("material.h", layerDef["material"], 0)
                            RV_URI("Texture", layerDef["material"], "Textures" )
                            RV_FLAGS("Flags", layerDef["flags"], "slf_")
                            RV_FLT("Offset", layerDef["offset"])
                            RV_FLT("Offset speed", layerDef["offsetSpeed"])
                            RV_FLT("Color limit", layerDef["colorLimit"])
                            RV_END
                            CHECKSC;
                        }
                    }
                    else if (ISLABEL("Model"))
                    {
                        defn::Sky mainDef(sky);

                        if (model == 32/*MAX_SKY_MODELS*/)
                        {
                            // Too many!
                            setError("Too many Sky models");
                            retVal = false;
                            goto ded_end_read;
                        }

                        // Add another model.
                        if (model >= mainDef.modelCount())
                        {
                            mainDef.addModel();
                        }
                        DE_ASSERT(model < mainDef.modelCount());

                        Record &mdlDef = mainDef.model(model);
                        if (source->custom) mdlDef.set("custom", true);

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_STR("ID", mdlDef[VAR_ID])
                            RV_INT("Layer", mdlDef["layer"])
                            RV_FLT("Frame interval", mdlDef["frameInterval"])
                            RV_FLT("Yaw", mdlDef["yaw"])
                            RV_FLT("Yaw speed", mdlDef["yawSpeed"])
                            RV_VEC_VAR("Rotate", mdlDef["rotate"], 2)
                            RV_VEC_VAR("Offset factor", mdlDef["originOffset"], 3)
                            RV_VEC_VAR("Color", mdlDef["color"], 4)
                            RV_STR("Execute", mdlDef["execute"])
                            RV_END
                            CHECKSC;
                        }
                        model++;
                    }
                    else RV_END
                    CHECKSC;
                }

                prevSkyDefIdx = idx;
            }

            if (ISTOKEN("Map")) // Info
            {
                ReadToken();
                if (!ISTOKEN("Info"))
                {
                    setError("Unknown token 'Map" + String(token) + "'");
                    retVal = false;
                    goto ded_end_read;
                }

                bool bModify = false;
                Record dummyMi;
                Record *mi = nullptr;
                int model = 0;

                ReadToken();
                if (!ISTOKEN("Mods"))
                {
                    // New mapinfos are appended to the end of the list.
                    idx = ded->addMapInfo();
                    mi = &ded->mapInfos[idx];
                }
                else if (!bCopyNext)
                {
                    res::Uri *otherMap = nullptr;
                    READURI(&otherMap, "Maps");
                    ReadToken();

                    idx = ded->getMapInfoNum(*otherMap);
                    if (idx >= 0)
                    {
                        mi = &ded->mapInfos[idx];
                        bModify = true;
                    }
                    else
                    {
                        LOG_RES_WARNING("Ignoring unknown Map \"%s\" in %s on line #%i")
                                << otherMap->asText()
                                << (source? source->fileName : "?")
                                << (source? source->lineNumber : 0);
                    }
                    delete otherMap;

                    if (mi && ISTOKEN("if"))
                    {
                        bool negate = false;
                        bool testCustom = false;
                        for (;;)
                        {
                            ReadToken();
                            if (ISTOKEN("{"))
                            {
                                break;
                            }

                            if (!testCustom)
                            {
                                if (ISTOKEN("not"))
                                {
                                    negate = !negate;
                                }
                                else if (ISTOKEN("custom"))
                                {
                                    testCustom = true;
                                }
                                else RV_END
                            }
                            else RV_END
                        }

                        if (testCustom)
                        {
                            if (mi->getb("custom") != !negate)
                            {
                                mi = nullptr; // skip
                            }
                        }
                        else
                        {
                            setError("Expected condition expression to follow 'if'");
                            retVal = false;
                            goto ded_end_read;
                        }
                    }

                    if (!mi)
                    {
                        // We'll read into a dummy definition.
                        defn::MapInfo(dummyMi).resetToDefaults();
                        mi = &dummyMi;
                    }
                }
                else
                {
                    setError("Cannot both Copy(Previous) and Modify");
                    retVal = false;
                    goto ded_end_read;
                }
                DE_ASSERT(mi != 0);

                if (prevMapInfoDefIdx >= 0 && bCopyNext)
                {
                    // Record *prevMapInfo = &ded->mapInfos[prevMapInfoDefIdx];
                    ded->mapInfos.copy(prevMapInfoDefIdx, *mi);
                }
                if (source->custom) mi->set("custom", true);

                Record &sky = mi->subrecord("sky");
                if (source->custom) sky.set("custom", true);

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    // ID cannot be changed when modifying
                    if (!bModify && ISLABEL("ID"))
                    {
                        READURI((*mi)[VAR_ID], "Maps");
                    }
                    else RV_STR("Title", (*mi)["title"])
                    RV_STR("Name", (*mi)["title"]) // Alias
                    RV_URI("Title image", (*mi)["titleImage"], "Patches")
                    RV_STR("Author", (*mi)["author"])
                    RV_FLAGS("Flags", (*mi)["flags"], "mif_")
                    RV_STR("Music", (*mi)["music"])
                    RV_FLT("Par time", (*mi)["parTime"])
                    RV_FLT_ELEM("Fog color R", (*mi)["fogColor"], 0)
                    RV_FLT_ELEM("Fog color G", (*mi)["fogColor"], 1)
                    RV_FLT_ELEM("Fog color B", (*mi)["fogColor"], 2)
                    RV_FLT("Fog start", (*mi)["fogStart"])
                    RV_FLT("Fog end", (*mi)["fogEnd"])
                    RV_FLT("Fog density", (*mi)["fogDensity"])
                    RV_STR("Fade Table", (*mi)["fadeTable"])
                    RV_FLT("Ambient light", (*mi)["ambient"])
                    RV_FLT("Gravity", (*mi)["gravity"])
                    RV_STR("Execute", (*mi)["execute"])
                    RV_STR("Sky", (*mi)["skyId"])
                    RV_FLT("Sky height", sky["height"])
                    RV_FLT("Horizon offset", sky["horizonOffset"])
                    RV_STR("Intermission background", (*mi)["intermissionBg"])
                    RV_VEC_VAR("Sky light color", sky["color"], 3)
                    if (ISLABEL("Sky Layer 1") || ISLABEL("Sky Layer 2"))
                    {
                        defn::Sky skyDef(sky);
                        Record &layerDef = skyDef.layer(atoi(label+10) - 1);
                        if (source->custom) layerDef.set("custom", true);

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_URI("material.h", layerDef["material"], 0)
                            RV_URI("Texture", layerDef["material"], "Textures" )
                            RV_FLAGS("Flags", layerDef["flags"], "slf_")
                            RV_FLT("Offset", layerDef["offset"])
                            RV_FLT("Offset speed", layerDef["offsetSpeed"])
                            RV_FLT("Color limit", layerDef["colorLimit"])
                            RV_END
                            CHECKSC;
                        }
                    }
                    else if (ISLABEL("Sky Model"))
                    {
                        defn::Sky skyDef(sky);

                        if (model == 32/*MAX_SKY_MODELS*/)
                        {
                            // Too many!
                            setError("Too many Sky models");
                            retVal = false;
                            goto ded_end_read;
                        }

                        // Add another model.
                        if (model >= skyDef.modelCount())
                        {
                            skyDef.addModel();
                        }
                        DE_ASSERT(model < skyDef.modelCount());

                        Record &mdlDef = skyDef.model(model);
                        if (source->custom) mdlDef.set("custom", true);

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_STR("ID", mdlDef[VAR_ID])
                            RV_INT("Layer", mdlDef["layer"])
                            RV_FLT("Frame interval", mdlDef["frameInterval"])
                            RV_FLT("Yaw", mdlDef["yaw"])
                            RV_FLT("Yaw speed", mdlDef["yawSpeed"])
                            RV_VEC_VAR("Rotate", mdlDef["rotate"], 2)
                            RV_VEC_VAR("Offset factor", mdlDef["originOffset"], 3)
                            RV_VEC_VAR("Color", mdlDef["color"], 4)
                            RV_STR("Execute", mdlDef["execute"])
                            RV_END
                            CHECKSC;
                        }
                        model++;
                    }
                    else RV_END
                    SKIPSC;
                }

                // If we did not read into a dummy update the previous index.
                if (idx > 0)
                {
                    prevMapInfoDefIdx = idx;
                }
            }

            if (ISTOKEN("Text"))
            {
                // A new text.
                idx = DED_AddText(ded, "");
                ded_text_t *txt = &ded->text[idx];

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", txt->id)
                    if (ISLABEL("Text"))
                    {
                        String buffer;
                        if (ReadString(buffer))
                        {
                            buffer.replace("\\n", "\n"); // newline escapes are allowed
                            txt->text = (char *) M_Realloc(txt->text, buffer.size() + 1);
                            strcpy(txt->text, buffer);
                        }
                        else
                        {
                            setError("Syntax error in Text value");
                            retVal = false;
                            goto ded_end_read;
                        }
                    }
                    else RV_END
                    CHECKSC;
                }
            }

            if (ISTOKEN("Texture"))
            {
                ReadToken();
                if (ISTOKEN("Environment"))
                {   // A new environment.
                    ded_tenviron_t*     tenv;

                    idx = DED_AddTextureEnv(ded, "");
                    tenv = &ded->textureEnv[idx];

                    FINDBEGIN;
                    for (;;)
                    {
                        ded_uri_t *mn;

                        READLABEL;
                        RV_STR("ID", tenv->id)
                        if (ISLABEL("material.h") || ISLABEL("Texture") || ISLABEL("Flat"))
                        {
                            // A new material path.
                            ddstring_t schemeName; Str_Init(&schemeName);
                            Str_Set(&schemeName, ISLABEL("material.h")? "" : ISLABEL("Texture")? "Textures" : "Flats");
                            mn = tenv->materials.append();
                            FINDBEGIN;
                            for (;;)
                            {
                                READLABEL;
                                RV_URI("ID", &mn->uri, Str_Text(&schemeName))
                                RV_END
                                CHECKSC;
                            }
                            Str_Free(&schemeName);
                        }
                        else RV_END
                        CHECKSC;
                    }
                }
            }

            if (ISTOKEN("Composite"))
            {
                ReadToken();
                if (ISTOKEN("resource/bitmapfont.h"))
                {
                    ded_compositefont_t* cfont;

                    idx = DED_AddCompositeFont(ded, NULL);
                    cfont = &ded->compositeFonts[idx];

                    FINDBEGIN;
                    for (;;)
                    {
                        READLABEL;
                        if (ISLABEL("ID"))
                        {
                            READURI(&cfont->uri, "Game")
                            CHECKSC;
                        }
                        else if (M_IsStringValidInt(label))
                        {
                            ded_compositefont_mappedcharacter_t* mc = 0;
                            int ascii = atoi(label);
                            if (ascii < 0 || ascii > 255)
                            {
                                setError("Invalid ascii code");
                                retVal = false;
                                goto ded_end_read;
                            }

                            for (int i = 0; i < cfont->charMap.size(); ++i)
                            {
                                if (cfont->charMap[i].ch == (unsigned char) ascii)
                                    mc = &cfont->charMap[i];
                            }

                            if (mc == 0)
                            {
                                mc = cfont->charMap.append();
                                mc->ch = ascii;
                            }

                            FINDBEGIN;
                            for (;;)
                            {
                                READLABEL;
                                RV_URI("Texture", &mc->path, "Patches")
                                RV_END
                                CHECKSC;
                            }
                        }
                        else
                        RV_END
                    }
                }
            }

            if (ISTOKEN("Values")) // String Values
            {
                depth = 0;
                rootStr = (char*) M_Calloc(1); // A null string.

                FINDBEGIN;
                for (;;)
                {
                    // Get the next label but don't stop on }.
                    READLABEL_NOBREAK;
                    if (strchr(label, '|'))
                    {
                        setError("Value labels can not include '|' characters (ASCII 124)");
                        retVal = false;
                        goto ded_end_read;
                    }

                    if (ISTOKEN("="))
                    {
                        // Define a new string.
                        String buffer;

                        if (ReadString(buffer))
                        {
                            // Get a new value entry.
                            idx = DED_AddValue(ded, 0);
                            ded_value_t *val = &ded->values[idx];

//                            QByteArray bufferUtf8 = buffer.toUtf8();
                            val->text = (char *) M_Malloc(buffer.size() + 1);
                            strcpy(val->text, buffer);

                            // Compose the identifier.
                            val->id = (char *) M_Malloc(strlen(rootStr) + strlen(label) + 1);
                            strcpy(val->id, rootStr);
                            strcat(val->id, label);
                        }
                        else
                        {
                            setError("Syntax error in Value string");
                            retVal = false;
                            goto ded_end_read;
                        }
                    }
                    else if (ISTOKEN("{"))
                    {
                        // Begin a new group.
                        rootStr = (char*) M_Realloc(rootStr, strlen(rootStr) + strlen(label) + 2);
                        strcat(rootStr, label);
                        strcat(rootStr, "|");   // The separator.
                        // Increase group level.
                        depth++;
                        continue;
                    }
                    else if (ISTOKEN("}"))
                    {
                        size_t len;

                        // End group.
                        if (!depth)
                            break;   // End root depth.

                        // Decrease level and modify rootStr.
                        depth--;
                        len = strlen(rootStr);
                        rootStr[len-1] = 0; // Remove last |.
                        ptr = strrchr(rootStr, '|');
                        if (ptr)
                        {
                            ptr[1] = 0;
                            rootStr = (char*) M_Realloc(rootStr, strlen(rootStr) + 1);
                        }
                        else
                        {
                            // Back to level zero.
                            rootStr = (char*) M_Realloc(rootStr, 1);
                            *rootStr = 0;
                        }
                    }
                    else
                    {
                        // Only the above characters are allowed.
                        setError("Illegal token");
                        retVal = false;
                        goto ded_end_read;
                    }
                    CHECKSC;
                }
                M_Free(rootStr);
                rootStr = 0;
            }

            if (ISTOKEN("Detail")) // Detail Texture
            {
                idx = DED_AddDetail(ded, "");
                ded_detailtexture_t *dtl = &ded->details[idx];

                 // Should we copy the previous definition?
                if (prevDetailDefIdx >= 0 && bCopyNext)
                {
                    ded->details.copyTo(dtl, prevDetailDefIdx);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_FLAGS("Flags", dtl->flags, "dtf_")
                    if (ISLABEL("Texture"))
                    {
                        READURI(&dtl->material1, "Textures")
                    }
                    else if (ISLABEL("Wall")) // Alias
                    {
                        READURI(&dtl->material1, "Textures")
                    }
                    else if (ISLABEL("Flat"))
                    {
                        READURI(&dtl->material2, "Flats")
                    }
                    else if (ISLABEL("Lump"))
                    {
                        READURI(&dtl->stage.texture, "Lumps")
                    }
                    else if (ISLABEL("File"))
                    {
                        READURI(&dtl->stage.texture, 0)
                    }
                    else
                    RV_FLT("Scale", dtl->stage.scale)
                    RV_FLT("Strength", dtl->stage.strength)
                    RV_FLT("Distance", dtl->stage.maxDistance)
                    RV_END
                    CHECKSC;
                }
                prevDetailDefIdx = idx;
            }

            if (ISTOKEN("Reflection")) // Surface reflection
            {
                ded_reflection_t* ref = 0;

                idx = DED_AddReflection(ded);
                ref = &ded->reflections[idx];

                // Should we copy the previous definition?
                if (prevRefDefIdx >= 0 && bCopyNext)
                {
                    ded->reflections.copyTo(ref, prevRefDefIdx);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_FLAGS("Flags", ref->flags, "rff_")
                    RV_FLT("Shininess", ref->stage.shininess)
                    RV_VEC("Min color", ref->stage.minColor, 3)
                    RV_BLENDMODE("Blending mode", ref->stage.blendMode)
                    RV_URI("Shiny map", &ref->stage.texture, "LightMaps")
                    RV_URI("Mask map", &ref->stage.maskTexture, "LightMaps")
                    RV_FLT("Mask width", ref->stage.maskWidth)
                    RV_FLT("Mask height", ref->stage.maskHeight)
                    if (ISLABEL("material.h"))
                    {
                        READURI(&ref->material, 0)
                    }
                    else if (ISLABEL("Texture"))
                    {
                        READURI(&ref->material, "Textures")
                    }
                    else if (ISLABEL("Flat"))
                    {
                        READURI(&ref->material, "Flats")
                    }
                    else RV_END
                    CHECKSC;
                }
                prevRefDefIdx = idx;
            }

            if (ISTOKEN("generator.h")) // Particle Generator
            {
                ded_ptcgen_t* gen;
                int sub = 0;

                idx = DED_AddPtcGen(ded, "");
                gen = &ded->ptcGens[idx];

                // Should we copy the previous definition?
                if (prevGenDefIdx >= 0 && bCopyNext)
                {
                    ded->ptcGens.copyTo(gen, prevGenDefIdx);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("State", gen->state)
                    if (ISLABEL("material.h"))
                    {
                        READURI(&gen->material, 0)
                    }
                    else if (ISLABEL("Flat"))
                    {
                        READURI(&gen->material, "Flats")
                    }
                    else if (ISLABEL("Texture"))
                    {
                        READURI(&gen->material, "Textures")
                    }
                    else
                    RV_STR("Mobj", gen->type)
                    RV_STR("Alt mobj", gen->type2)
                    RV_STR("Damage mobj", gen->damage)
                    RV_URI("Map", &gen->map, "Maps")
                    RV_FLAGS("Flags", gen->flags, "gnf_")
                    RV_FLT("Speed", gen->speed)
                    RV_FLT("Speed Rnd", gen->speedVariance)
                    RV_VEC("Vector", gen->vector, 3)
                    RV_FLT("Vector Rnd", gen->vectorVariance)
                    RV_FLT("Init vector Rnd", gen->initVectorVariance)
                    RV_VEC("Center", gen->center, 3)
                    RV_INT("Submodel", gen->subModel)
                    RV_FLT("Spawn radius", gen->spawnRadius)
                    RV_FLT("Min spawn radius", gen->spawnRadiusMin)
                    RV_FLT("Distance", gen->maxDist)
                    RV_INT("Spawn age", gen->spawnAge)
                    RV_INT("Max age", gen->maxAge)
                    RV_INT("Particles", gen->particles)
                    RV_FLT("Spawn rate", gen->spawnRate)
                    RV_FLT("Spawn Rnd", gen->spawnRateVariance)
                    RV_INT("Presim", gen->preSim)
                    RV_INT("Alt start", gen->altStart)
                    RV_FLT("Alt Rnd", gen->altStartVariance)
                    RV_VEC("Force axis", gen->forceAxis, 3)
                    RV_FLT("Force radius", gen->forceRadius)
                    RV_FLT("Force", gen->force)
                    RV_VEC("Force origin", gen->forceOrigin, 3)
                    if (ISLABEL("Stage"))
                    {
                        ded_ptcstage_t *st = NULL;

                        if (sub >= gen->stages.size())
                        {
                            // Allocate new stage.
                            sub = DED_AddPtcGenStage(gen);
                        }

                        st = &gen->stages[sub];

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_FLAGS("Type", st->type, "pt_")
                            RV_INT("Tics", st->tics)
                            RV_FLT("Rnd", st->variance)
                            RV_VEC("Color", st->color, 4)
                            RV_FLT("Radius", st->radius)
                            RV_FLT("Radius rnd", st->radiusVariance)
                            RV_FLAGS("Flags", st->flags, "ptf_")
                            RV_FLT("Bounce", st->bounce)
                            RV_FLT("Gravity", st->gravity)
                            RV_FLT("Resistance", st->resistance)
                            RV_STR("Frame", st->frameName)
                            RV_STR("End frame", st->endFrameName)
                            RV_VEC("Spin", st->spin, 2)
                            RV_VEC("Spin resistance", st->spinResistance, 2)
                            RV_STR("Sound", st->sound.name)
                            RV_FLT("Volume", st->sound.volume)
                            RV_STR("Hit sound", st->hitSound.name)
                            RV_FLT("Hit volume", st->hitSound.volume)
                            RV_VEC("Force", st->vectorForce, 3)
                            RV_END
                            CHECKSC;
                        }
                        sub++;
                    }
                    else RV_END
                    CHECKSC;
                }
                prevGenDefIdx = idx;
            }

            if (ISTOKEN("Finale") || ISTOKEN("InFine"))
            {
                // New finales are appended to the end of the list.
                idx = ded->addFinale();
                Record &fin = ded->finales[idx];
                if (source->custom) fin.set("custom", true);

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", fin[VAR_ID])
                    RV_URI("Before", fin["before"], "Maps")
                    RV_URI("After", fin["after"], "Maps")
                    RV_INT("Game", dummyInt)
                    if (ISLABEL("Script"))
                    {
                        String buffer;
                        FINDBEGIN;
                        ReadToken();
                        while (!ISTOKEN("}") && !source->atEnd)
                        {
                            if (!buffer.isEmpty())
                                buffer += ' ';

                            buffer += String(token);
                            if (ISTOKEN("\""))
                            {
                                ReadString(buffer, true, true);
                                buffer += '"';
                            }
                            ReadToken();
                        }
                        fin.set("script", buffer);
                    }
                    else RV_END
                    CHECKSC;
                }
            }

            // An oldschool (light) decoration definition?
            if (ISTOKEN("render/decoration.h"))
            {
                idx = ded->addDecoration();
                Record &decor = ded->decorations[idx];

                // Should we copy the previous definition?
                if (prevDecorDefIdx >= 0 && bCopyNext)
                {
                    ded->decorations.copy(prevDecorDefIdx, decor);
                }

                defn::Decoration mainDef(decor);
                int light = 0;
                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_FLAGS("Flags", decor["flags"], "dcf_")
                    if (ISLABEL("material.h"))
                    {
                        READURI(decor["texture"], 0)
                    }
                    else if (ISLABEL("Texture"))
                    {
                        READURI(decor["texture"], "Textures")
                    }
                    else if (ISLABEL("Flat"))
                    {
                        READURI(decor["texture"], "Flats")
                    }
                    else if (ISLABEL("Light"))
                    {
                        if (light == DED_MAX_MATERIAL_DECORATIONS)
                        {
                            setError("Too many Decoration.Lights");
                            retVal = false;
                            goto ded_end_read;
                        }

                        // Add another light.
                        if (light >= mainDef.lightCount()) mainDef.addLight();
                        defn::MaterialDecoration lightDef(mainDef.light(light));

                        // One implicit stage.
                        Record &st = (lightDef.stageCount()? lightDef.stage(0) : lightDef.addStage());
                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_VEC_VAR("Offset", st["origin"], 2)
                            RV_FLT("Distance", st["elevation"])
                            RV_VEC_VAR("Color", st["color"], 3)
                            RV_FLT("Radius", st["radius"])
                            RV_FLT("Halo radius", st["haloRadius"])
                            RV_VEC_VAR("Pattern offset", lightDef.def()["patternOffset"], 2)
                            RV_VEC_VAR("Pattern skip", lightDef.def()["patternSkip"], 2)
                            if (ISLABEL("Levels"))
                            {
                                FINDBEGIN;
                                Vec2f levels;
                                for (int b = 0; b < 2; ++b)
                                {
                                    float val;
                                    READFLT(val)
                                    levels[b] = de::clamp(0.f, val / 255.0f, 1.f);
                                }
                                ReadToken();
                                st["lightLevels"] = new ArrayValue(levels);
                            }
                            else
                            RV_INT("Flare texture", st["haloTextureIndex"])
                            RV_URI("Flare map", st["haloTexture"], "LightMaps")
                            RV_URI("Top map", st["lightmapUp"], "LightMaps")
                            RV_URI("Bottom map", st["lightmapDown"], "LightMaps")
                            RV_URI("Side map", st["lightmapSide"], "LightMaps")
                            RV_END
                            CHECKSC;
                        }
                        light++;
                    }
                    else RV_END
                    CHECKSC;
                }
                prevDecorDefIdx = idx;
            }

            if (ISTOKEN("Group"))
            {
                idx = DED_AddGroup(ded);
                ded_group_t *grp = &ded->groups[idx];

                int sub = 0;
                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    if (ISLABEL("Texture") || ISLABEL("Flat"))
                    {
                        const bool haveTexture = ISLABEL("Texture");

                        // Need to allocate new stage?
                        if (sub >= grp->members.size())
                        {
                            sub = DED_AddGroupMember(grp);
                        }
                        ded_group_member_t *memb = &grp->members[sub];

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_URI("ID", &memb->material, (haveTexture? "Textures" : "Flats"))
                            if (ISLABEL("Tics"))
                            {
                                READINT(memb->tics);
                                if (memb->tics < 0)
                                {
                                    LOG_RES_WARNING("Invalid Group.%s.Tics: %i (< min: 0) in \"%s\" on line #%i"
                                                    "\nWill ignore this Group if used for Material animation")
                                            << (haveTexture ? "Texture" : "Flat")
                                            << memb->tics
                                            << (source ? source->fileName : "?") << (source ? source->lineNumber : 0);
                                }
                            }
                            else
                            RV_INT("Random", memb->randomTics)
                            RV_END
                            CHECKSC;
                        }
                        ++sub;
                    }
                    else RV_FLAGS("Flags", grp->flags, "tgf_")
                    RV_END
                    CHECKSC;
                }
            }

            /*if (ISTOKEN("XGClass"))     // XG Class
            {
                // A new XG Class definition
                idx = DED_AddXGClass(ded);
                xgc = ded->xgClasses + idx;
                sub = 0;

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_STR("ID", xgc->id)
                    RV_STR("Name", xgc->name)
                    if (ISLABEL("Property"))
                    {
                        ded_xgclass_property_t *xgcp = NULL;

                        if (sub >= xgc->properties_count.num)
                        {
                            // Allocate new property
                            sub = DED_AddXGClassProperty(xgc);
                        }

                        xgcp = &xgc->properties[sub];

                        FINDBEGIN;
                        for (;;)
                        {
                            READLABEL;
                            RV_FLAGS("ID", xgcp->id, "xgcp_")
                            RV_FLAGS("Flags", xgcp->flags, "xgcpf_")
                            RV_STR("Name", xgcp->name)
                            RV_STR("Flag Group", xgcp->flagprefix)
                            RV_END
                            CHECKSC;
                        }
                        sub++;
                    }
                    else RV_END
                    CHECKSC;
                }
            }*/

            if (ISTOKEN("Line")) // Line Type
            {
                ded_linetype_t* l;

                // A new line type.
                idx = DED_AddLineType(ded, 0);
                l = &ded->lineTypes[idx];

                // Should we copy the previous definition?
                if (prevLineTypeDefIdx >= 0 && bCopyNext)
                {
                    ded->lineTypes.copyTo(l, prevLineTypeDefIdx);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_INT("ID", l->id)
                    RV_STR("Comment", l->comment)
                    RV_FLAGS("Flags", l->flags[0], "ltf_")
                    RV_FLAGS("Flags2", l->flags[1], "ltf2_")
                    RV_FLAGS("Flags3", l->flags[2], "ltf3_")
                    RV_FLAGS("Class", l->lineClass, "ltc_")
                    RV_FLAGS("Type", l->actType, "lat_")
                    RV_INT("Count", l->actCount)
                    RV_FLT("Time", l->actTime)
                    RV_INT("Act tag", l->actTag)
                    RV_INT("Ap0", l->aparm[0])
                    RV_INT("Ap1", l->aparm[1])
                    RV_INT("Ap2", l->aparm[2])
                    RV_INT("Ap3", l->aparm[3])
                    RV_FLAGS("Ap4", l->aparm[4], "lref_")
                    RV_INT("Ap5", l->aparm[5])
                    RV_FLAGS("Ap6", l->aparm[6], "lref_")
                    RV_INT("Ap7", l->aparm[7])
                    RV_INT("Ap8", l->aparm[8])
                    RV_STR("Ap9", l->aparm9)
                    RV_INT("Health above", l->aparm[0])
                    RV_INT("Health below", l->aparm[1])
                    RV_INT("Power above", l->aparm[2])
                    RV_INT("Power below", l->aparm[3])
                    RV_FLAGS("Line act lref", l->aparm[4], "lref_")
                    RV_INT("Line act lrefd", l->aparm[5])
                    RV_FLAGS("Line inact lref", l->aparm[6], "lref_")
                    RV_INT("Line inact lrefd", l->aparm[7])
                    RV_INT("Color", l->aparm[8])
                    RV_STR("Thing type", l->aparm9)
                    RV_FLT("Ticker start time", l->tickerStart)
                    RV_FLT("Ticker end time", l->tickerEnd)
                    RV_INT("Ticker tics", l->tickerInterval)
                    RV_STR("Act sound", l->actSound)
                    RV_STR("Deact sound", l->deactSound)
                    RV_INT("Event chain", l->evChain)
                    RV_INT("Act chain", l->actChain)
                    RV_INT("Deact chain", l->deactChain)
                    RV_FLAGS("Wall section", l->wallSection, "lws_")
                    if (ISLABEL("Act material"))
                    {
                        READURI(&l->actMaterial, 0)
                    }
                    else if (ISLABEL("Act texture")) // Alias
                    {
                        READURI(&l->actMaterial, "Textures")
                    }
                    else if (ISLABEL("Deact material"))
                    {
                        READURI(&l->deactMaterial, 0)
                    }
                    else if (ISLABEL("Deact texture")) // Alias
                    {
                        READURI(&l->deactMaterial, "Textures")
                    }
                    else
                    RV_INT("Act type", l->actLineType)
                    RV_INT("Deact type", l->deactLineType)
                    RV_STR("Act message", l->actMsg)
                    RV_STR("Deact message", l->deactMsg)
                    RV_FLT("Texmove angle", l->materialMoveAngle)
                    RV_FLT("Materialmove angle", l->materialMoveAngle) // Alias
                    RV_FLT("Texmove speed", l->materialMoveSpeed)
                    RV_FLT("Materialmove speed", l->materialMoveSpeed) // Alias
                    RV_STR_INT("Ip0", l->iparmStr[0], l->iparm[0])
                    RV_STR_INT("Ip1", l->iparmStr[1], l->iparm[1])
                    RV_STR_INT("Ip2", l->iparmStr[2], l->iparm[2])
                    RV_STR_INT("Ip3", l->iparmStr[3], l->iparm[3])
                    RV_STR_INT("Ip4", l->iparmStr[4], l->iparm[4])
                    RV_STR_INT("Ip5", l->iparmStr[5], l->iparm[5])
                    RV_STR_INT("Ip6", l->iparmStr[6], l->iparm[6])
                    RV_STR_INT("Ip7", l->iparmStr[7], l->iparm[7])
                    RV_STR_INT("Ip8", l->iparmStr[8], l->iparm[8])
                    RV_STR_INT("Ip9", l->iparmStr[9], l->iparm[9])
                    RV_STR_INT("Ip10", l->iparmStr[10], l->iparm[10])
                    RV_STR_INT("Ip11", l->iparmStr[11], l->iparm[11])
                    RV_STR_INT("Ip12", l->iparmStr[12], l->iparm[12])
                    RV_STR_INT("Ip13", l->iparmStr[13], l->iparm[13])
                    RV_STR_INT("Ip14", l->iparmStr[14], l->iparm[14])
                    RV_STR_INT("Ip15", l->iparmStr[15], l->iparm[15])
                    RV_STR_INT("Ip16", l->iparmStr[16], l->iparm[16])
                    RV_STR_INT("Ip17", l->iparmStr[17], l->iparm[17])
                    RV_STR_INT("Ip18", l->iparmStr[18], l->iparm[18])
                    RV_STR_INT("Ip19", l->iparmStr[19], l->iparm[19])
                    RV_FLT("Fp0", l->fparm[0])
                    RV_FLT("Fp1", l->fparm[1])
                    RV_FLT("Fp2", l->fparm[2])
                    RV_FLT("Fp3", l->fparm[3])
                    RV_FLT("Fp4", l->fparm[4])
                    RV_FLT("Fp5", l->fparm[5])
                    RV_FLT("Fp6", l->fparm[6])
                    RV_FLT("Fp7", l->fparm[7])
                    RV_FLT("Fp8", l->fparm[8])
                    RV_FLT("Fp9", l->fparm[9])
                    RV_FLT("Fp10", l->fparm[10])
                    RV_FLT("Fp11", l->fparm[11])
                    RV_FLT("Fp12", l->fparm[12])
                    RV_FLT("Fp13", l->fparm[13])
                    RV_FLT("Fp14", l->fparm[14])
                    RV_FLT("Fp15", l->fparm[15])
                    RV_FLT("Fp16", l->fparm[16])
                    RV_FLT("Fp17", l->fparm[17])
                    RV_FLT("Fp18", l->fparm[18])
                    RV_FLT("Fp19", l->fparm[19])
                    RV_STR("Sp0", l->sparm[0])
                    RV_STR("Sp1", l->sparm[1])
                    RV_STR("Sp2", l->sparm[2])
                    RV_STR("Sp3", l->sparm[3])
                    RV_STR("Sp4", l->sparm[4])
                    if (l->lineClass)
                    {
                        // IpX Alt names can only be used if the class is defined first!
                        // they also support the DED v6 flags format.
                        int i;
                        for (i = 0; i < 20; ++i)
                        {
                            xgclassparm_t const& iParm = xgClassLinks[l->lineClass].iparm[i];

                            if (!iParm.name[0]) continue;
                            if (!ISLABEL(iParm.name)) continue;

                            if (iParm.flagPrefix[0])
                            {
                                READFLAGS(l->iparm[i], iParm.flagPrefix)
                            }
                            else
                            {
                                READSTRING(l->iparmStr[i], l->iparm[i])
                            }
                            break;
                        }
                        // Not a known label?
                        if (i == 20) RV_END

                    } else
                    RV_END
                    CHECKSC;
                }
                prevLineTypeDefIdx = idx;
            }

            if (ISTOKEN("Sector")) // Sector Type
            {
                ded_sectortype_t*   sec;

                // A new sector type.
                idx = DED_AddSectorType(ded, 0);
                sec = &ded->sectorTypes[idx];

                if (prevSectorTypeDefIdx >= 0 && bCopyNext)
                {
                    // Should we copy the previous definition?
                    ded->sectorTypes.copyTo(sec, prevSectorTypeDefIdx);
                }

                FINDBEGIN;
                for (;;)
                {
                    READLABEL;
                    RV_INT("ID", sec->id)
                    RV_STR("Comment", sec->comment)
                    RV_FLAGS("Flags", sec->flags, "stf_")
                    RV_INT("Act tag", sec->actTag)
                    RV_INT("Floor chain", sec->chain[0])
                    RV_INT("Ceiling chain", sec->chain[1])
                    RV_INT("Inside chain", sec->chain[2])
                    RV_INT("Ticker chain", sec->chain[3])
                    RV_FLAGS("Floor chain flags", sec->chainFlags[0], "scef_")
                    RV_FLAGS("Ceiling chain flags", sec->chainFlags[1], "scef_")
                    RV_FLAGS("Inside chain flags", sec->chainFlags[2], "scef_")
                    RV_FLAGS("Ticker chain flags", sec->chainFlags[3], "scef_")
                    RV_FLT("Floor chain start time", sec->start[0])
                    RV_FLT("Ceiling chain start time", sec->start[1])
                    RV_FLT("Inside chain start time", sec->start[2])
                    RV_FLT("Ticker chain start time", sec->start[3])
                    RV_FLT("Floor chain end time", sec->end[0])
                    RV_FLT("Ceiling chain end time", sec->end[1])
                    RV_FLT("Inside chain end time", sec->end[2])
                    RV_FLT("Ticker chain end time", sec->end[3])
                    RV_FLT("Floor chain min interval", sec->interval[0][0])
                    RV_FLT("Ceiling chain min interval", sec->interval[1][0])
                    RV_FLT("Inside chain min interval", sec->interval[2][0])
                    RV_FLT("Ticker chain min interval", sec->interval[3][0])
                    RV_FLT("Floor chain max interval", sec->interval[0][1])
                    RV_FLT("Ceiling chain max interval", sec->interval[1][1])
                    RV_FLT("Inside chain max interval", sec->interval[2][1])
                    RV_FLT("Ticker chain max interval", sec->interval[3][1])
                    RV_INT("Floor chain count", sec->count[0])
                    RV_INT("Ceiling chain count", sec->count[1])
                    RV_INT("Inside chain count", sec->count[2])
                    RV_INT("Ticker chain count", sec->count[3])
                    RV_STR("Ambient sound", sec->ambientSound)
                    RV_FLT("Ambient min interval", sec->soundInterval[0])
                    RV_FLT("Ambient max interval", sec->soundInterval[1])
                    RV_FLT("Floor texmove angle", sec->materialMoveAngle[0])
                    RV_FLT("Floor materialmove angle", sec->materialMoveAngle[0]) // Alias
                    RV_FLT("Ceiling texmove angle", sec->materialMoveAngle[1])
                    RV_FLT("Ceiling materialmove angle", sec->materialMoveAngle[1]) // Alias
                    RV_FLT("Floor texmove speed", sec->materialMoveSpeed[0])
                    RV_FLT("Floor materialmove speed", sec->materialMoveSpeed[0]) // Alias
                    RV_FLT("Ceiling texmove speed", sec->materialMoveSpeed[1])
                    RV_FLT("Ceiling materialmove speed", sec->materialMoveSpeed[1]) // Alias
                    RV_FLT("Wind angle", sec->windAngle)
                    RV_FLT("Wind speed", sec->windSpeed)
                    RV_FLT("Vertical wind", sec->verticalWind)
                    RV_FLT("Gravity", sec->gravity)
                    RV_FLT("Friction", sec->friction)
                    RV_STR("Light fn", sec->lightFunc)
                    RV_INT("Light fn min tics", sec->lightInterval[0])
                    RV_INT("Light fn max tics", sec->lightInterval[1])
                    RV_STR("Red fn", sec->colFunc[0])
                    RV_STR("Green fn", sec->colFunc[1])
                    RV_STR("Blue fn", sec->colFunc[2])
                    RV_INT("Red fn min tics", sec->colInterval[0][0])
                    RV_INT("Red fn max tics", sec->colInterval[0][1])
                    RV_INT("Green fn min tics", sec->colInterval[1][0])
                    RV_INT("Green fn max tics", sec->colInterval[1][1])
                    RV_INT("Blue fn min tics", sec->colInterval[2][0])
                    RV_INT("Blue fn max tics", sec->colInterval[2][1])
                    RV_STR("Floor fn", sec->floorFunc)
                    RV_FLT("Floor fn scale", sec->floorMul)
                    RV_FLT("Floor fn offset", sec->floorOff)
                    RV_INT("Floor fn min tics", sec->floorInterval[0])
                    RV_INT("Floor fn max tics", sec->floorInterval[1])
                    RV_STR("Ceiling fn", sec->ceilFunc)
                    RV_FLT("Ceiling fn scale", sec->ceilMul)
                    RV_FLT("Ceiling fn offset", sec->ceilOff)
                    RV_INT("Ceiling fn min tics", sec->ceilInterval[0])
                    RV_INT("Ceiling fn max tics", sec->ceilInterval[1])
                    RV_END
                    CHECKSC;
                }
                prevSectorTypeDefIdx = idx;
            }
            bCopyNext = false;
        }

    ded_end_read:
        M_Free(rootStr);

        // Free the source stack entry we were using.
        DED_CloseReader();

        return retVal;
    }

    void DED_Include(const char *fileName, String parentDirectory)
    {
        ddstring_t tmp;

        Str_InitStd(&tmp); Str_Set(&tmp, fileName);
        F_FixSlashes(&tmp, &tmp);
        F_ExpandBasePath(&tmp, &tmp);
        if (!F_IsAbsolute(&tmp))
        {
            Str_PrependChar(&tmp, '/');
            Str_Prepend(&tmp, parentDirectory);
        }

        Def_ReadProcessDED(ded, String(Str_Text(&tmp)));
        Str_Free(&tmp);

        // Reset state for continuing.
        strncpy(token, "", MAX_TOKEN_LEN);
    }
};

DEDParser::DEDParser(ded_t *ded) : d(new Impl(this))
{
    d->ded = ded;
}

int DEDParser::parse(const char *buffer, String sourceFile, bool sourceIsCustom)
{
    return d->readData(buffer, sourceFile, sourceIsCustom);
}
