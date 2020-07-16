/** @file dehreader.cpp  DeHackEd patch reader plugin for Doomsday Engine.
 * @ingroup dehread
 *
 * @todo Presently there are a number of unsupported features which should not
 *       be ignored. (Most if not all features should be supported.)
 *
 * @author Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dehreader.h"

#include <de/legacy/memory.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/filesys/lumpindex.h>
#include <doomsday/defs/thing.h>
#include <doomsday/defs/state.h>
#include <doomsday/defs/ded.h>
#include <doomsday/game.h>
#include <de/app.h>
#include <de/arrayvalue.h>
#include <de/block.h>
#include <de/error.h>
#include <de/log.h>
#include <de/string.h>
#include <de/regexp.h>

#include <fstream>

#include "importdeh.h"
#include "dehreader_util.h"
#include "info.h"

using namespace de;
using namespace res;

static int       stackDepth;
static const int maxIncludeDepth = de::max(0, DEHREADER_INCLUDE_DEPTH_MAX);

/// Mask containing only those reader flags which should be passed from the current
/// parser to any child parsers for file include statements.
static const DehReaderFlag DehReaderFlagsIncludeMask = IgnoreEOF;

// Helper for managing a dummy definition allocated on the stack.
template <typename T>
struct Dummy : public T {
    Dummy() { zap(*this); }
    ~Dummy() { T::release(); }
    void clear() { T::release(); zap(*this); }
};

/**
 * Not exposed outside this source file; use readDehPatch() instead.
 */
class DehReader
{
    /// The parser encountered a syntax error in the source file. @ingroup errors
    DE_ERROR(SyntaxError);

    /// The parser encountered an unknown section in the source file. @ingroup errors
    DE_ERROR(UnknownSection);

    /// The parser reached the end of the source file. @ingroup errors
    DE_ERROR(EndOfFile);

public:
    String       patch;
    mb_iterator  pos;
    int          currentLineNumber = 0;
    CString      line; ///< Current line.

    bool           patchIsCustom = true;
    DehReaderFlags flags         = 0;
    int            patchVersion  = -1; ///< @c -1= Unknown.
    int            doomVersion   = -1; ///< @c -1= Unknown.

public:
    DehReader(Block patch, bool patchIsCustom = true, DehReaderFlags flags = 0)
        : patchIsCustom(patchIsCustom)
        , flags(flags)
    {
        if (flags & IgnoreEOF)
        {
            patch.removeAll('\0');
        }
        this->patch = patch;
        pos = this->patch.begin();
        stackDepth++;
    }

    ~DehReader()
    {
        stackDepth--;
    }

    /**
     * Doom version numbers in the patch use the orignal game versions,
     * "16" => Doom v1.6, "19" => Doom v1.9, etc...
     */
    static inline bool normalizeDoomVersion(int &ver)
    {
        switch(ver)
        {
        case 16: ver = 0; return true;
        case 17: ver = 2; return true;
        case 19: ver = 3; return true;
        case 20: ver = 1; return true;
        case 21: ver = 4; return true;
        default: return false; // What is this??
        }
    }

    bool atEnd()
    {
        return pos == patch.end(); //size_t(pos) >= patch.size();
    }

//    bool atEnd()
//    {
//        if (atRealEnd()) return true;
//        if (!(flags & IgnoreEOF) && *pos == 0) return true;
//        return false;
//    }

    void advance()
    {
        if (atEnd()) return;

        // Handle special characters in the input.
        Char ch = currentChar();
        switch (ch)
        {
        case '\0':
            if (pos != patch.end())
            {
                LOG_WARNING("Unexpected EOF encountered on line #%i") << currentLineNumber;
            }
            break;
        case '\n':
            currentLineNumber++;
            break;
        default: break;
        }

        ++pos;
    }

    Char currentChar()
    {
        if (atEnd()) return {};
        return *pos;
    }

    void skipToEOL()
    {
        while (!atEnd() && currentChar() != '\n') { advance(); }
    }

    void readLine()
    {
        if (atEnd())
        {
            throw EndOfFile(stringf("EOF on line #%i", currentLineNumber));
        }

        mb_iterator start = pos;
        skipToEOL();

            //auto endOfLine = pos - start;
            // Ignore any trailing carriage return.
            //if (endOfLine > 0 && patch.at(start + endOfLine - 1) == '\r') endOfLine -= 1;

        line = CString(start, pos).strip(); // = patch.mid(start, endOfLine);

            // When tolerating mid stream EOF characters, we must first
            // strip them before attempting any encoding conversion.
//            if (flags & IgnoreEOF)
//            {
//                rawLine.replace("\0", "");
//            }

            // Perform encoding conversion for this line and move on.
//            line = String::fromLatin1(rawLine);
        if (currentChar() == '\n')
        {
            advance();
        }
    }

    /**
     * Keep reading lines until we find one that is something other than
     * whitespace or a whole-line comment.
     */
    CString skipToNextLine()
    {
        for (;;)
        {
            readLine();
            if (!line.isEmpty() && line.first() != '#') break;
        }
        return line;
    }

    bool lineInCurrentSection()
    {
        return line.indexOf('=') != CString::npos;
    }

    void skipToNextSection()
    {
        do {
            skipToNextLine();
        } while (lineInCurrentSection());
    }

    void logPatchInfo()
    {
        // Log reader settings and patch version information.
        LOG_RES_MSG("Patch version: %i, Doom version: %i\nNoText: %b")
            << patchVersion << doomVersion << bool(flags & NoText);

        if (patchVersion != 6)
        {
            LOG_WARNING("Patch version %i unknown, unexpected results may occur") << patchVersion;
        }
    }

    void parse()
    {
        LOG_AS_STRING(stackDepth == 1? "DehReader" : Stringf("[%i]", stackDepth - 1));

        skipToNextLine();

        // Attempt to parse the DeHackEd patch signature and version numbers.
        if (line.beginsWith("Patch File for DeHackEd v", CaseInsensitive))
        {
            skipToNextLine();
            parsePatchSignature();
        }
        else
        {
            LOG_WARNING("Patch is missing a signature, assuming BEX");
            doomVersion  = 19;
            patchVersion = 6;
        }

        logPatchInfo();

        // Is this for a known Doom version?
        if (!normalizeDoomVersion(doomVersion))
        {
            LOG_WARNING("Doom version undefined, assuming v1.9");
            doomVersion = 3;
        }

        // Patches are subdivided into sections.
        try
        {
            for (;;)
            {
                try
                {
                    /// @note Some sections have their own grammar quirks!
                    if (line.beginsWith("include", CaseInsensitive)) // BEX
                    {
                        parseInclude(line.substr(7).leftStrip());
                        skipToNextSection();
                    }
                    else if (line.beginsWith("Thing", CaseInsensitive))
                    {
                        Record *mobj;
                        Record dummyMobj;

                        const String arg  = line.substr(5).leftStrip();
                        const int mobjNum = parseMobjNum(arg);
                        if(mobjNum >= 0)
                        {
                            mobj = &ded->things[mobjNum];
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Thing '%s' out of range\n(Create more Thing defs)") << arg;
                            dummyMobj.clear();
                            mobj = &dummyMobj;
                        }

                        skipToNextLine();
                        parseThing(*mobj, mobj == &dummyMobj);
                    }
                    else if (line.beginsWith("Frame", CaseInsensitive))
                    {
                        Record *state;
                        Record dummyState;

                        const String arg   = line.substr(5).leftStrip();
                        const int stateNum = parseStateNum(arg);
                        if(stateNum >= 0)
                        {
                            state = &ded->states[stateNum];
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Frame '%s' out of range\n(Create more State defs)") << arg;
                            dummyState.clear();
                            state = &dummyState;
                        }

                        skipToNextLine();
                        parseFrame(*state, state == &dummyState);
                    }
                    else if (line.beginsWith("Pointer", CaseInsensitive))
                    {
                        Record *state;
                        Record dummyState;

                        const String arg   = line.substr(7).leftStrip();
                        const int stateNum = parseStateNumFromActionOffset(arg);
                        if(stateNum >= 0)
                        {
                            state = &ded->states[stateNum];
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Pointer '%s' out of range\n(Create more State defs)") << arg;
                            dummyState.clear();
                            state = &dummyState;
                        }

                        skipToNextLine();
                        parsePointer(*state, state == &dummyState);
                    }
                    else if (line.beginsWith("Sprite", CaseInsensitive))
                    {
                        ded_sprid_t *sprite;
                        Dummy<ded_sprid_t> dummySprite;

                        const String arg    = line.substr(6).leftStrip();
                        const int spriteNum = parseSpriteNum(arg);
                        if(spriteNum >= 0)
                        {
                            sprite = &ded->sprites[spriteNum];
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Sprite '%s' out of range\n(Create more Sprite defs)") << arg;
                            dummySprite.clear();
                            sprite = &dummySprite;
                        }

                        skipToNextLine();
                        parseSprite(sprite, sprite == &dummySprite);
                    }
                    else if (line.beginsWith("Ammo", CaseInsensitive))
                    {
                        const String arg          = line.substr(4).leftStrip();
                        int ammoNum               = 0;
                        const bool isKnownAmmoNum = parseAmmoNum(arg, &ammoNum);
                        const bool ignore         = !isKnownAmmoNum;

                        if(!isKnownAmmoNum)
                        {
                            LOG_WARNING("DeHackEd Ammo '%s' out of range") << arg;
                        }

                        skipToNextLine();
                        parseAmmo(ammoNum, ignore);
                    }
                    else if (line.beginsWith("Weapon", CaseInsensitive))
                    {
                        const String arg            = line.substr(6).leftStrip();
                        int weaponNum               = 0;
                        const bool isKnownWeaponNum = parseWeaponNum(arg, &weaponNum);
                        const bool ignore           = !isKnownWeaponNum;

                        if(!isKnownWeaponNum)
                        {
                            LOG_WARNING("DeHackEd Weapon '%s' out of range") << arg;
                        }

                        skipToNextLine();
                        parseWeapon(weaponNum, ignore);
                    }
                    else if (line.beginsWith("Sound", CaseInsensitive))
                    {
                        ded_sound_t *sound;
                        Dummy<ded_sound_t> dummySound;

                        const String arg   = line.substr(5).leftStrip();
                        const int soundNum = parseSoundNum(arg);
                        if(soundNum >= 0)
                        {
                            sound = &ded->sounds[soundNum];
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Sound '%s' out of range\n(Create more Sound defs)") << arg;
                            dummySound.clear();
                            sound = &dummySound;
                        }

                        skipToNextLine();
                        parseSound(sound, sound == &dummySound);
                    }
                    else if (line.beginsWith("Text", CaseInsensitive))
                    {
                        String args = line.substr(4).leftStrip();
                        auto firstArgEnd = args.indexOf(' ');
                        if (!firstArgEnd)
                        {
                            throw SyntaxError(
                                stringf("Expected old text size on line #%i", currentLineNumber));
                        }

                        bool isNumber;
                        const int oldSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(
                                stringf("Expected old text size but encountered \"%s\" on line #%i",
                                        args.substr(firstArgEnd).c_str(),
                                        currentLineNumber));
                        }

                        args.remove(BytePos(0), firstArgEnd + 1);

                        const int newSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(
                                stringf("Expected new text size but encountered \"%s\" on line #%i",
                                        args.c_str(),
                                        currentLineNumber));
                        }

                        parseText(oldSize, newSize);
                    }
                    else if (line.beginsWith("Misc", CaseInsensitive))
                    {
                        skipToNextLine();
                        parseMisc();
                    }
                    else if (line.beginsWith("Cheat", CaseInsensitive))
                    {
                        if (!(!patchIsCustom && DoomsdayApp::game().id() == "hacx"))
                        {
                            LOG_WARNING("DeHackEd [Cheat] patches are not supported");
                        }
                        skipToNextSection();
                    }
                    else if (line.beginsWith("[CODEPTR]", CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseCodePointers();
                    }
                    else if (line.beginsWith("[PARS]", CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parsePars();
                    }
                    else if (line.beginsWith("[STRINGS]", CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseStrings();
                    }
                    else if (line.beginsWith("[HELPER]", CaseInsensitive)) // Eternity
                    {
                        // Not yet supported (Helper Dogs from MBF).
                        //skipToNextLine();
                        parseHelper();
                        skipToNextSection();
                    }
                    else if (line.beginsWith("[SPRITES]", CaseInsensitive)) // Eternity
                    {
                        // Not yet supported.
                        //skipToNextLine();
                        parseSprites();
                        skipToNextSection();
                    }
                    else if (line.beginsWith("[SOUNDS]", CaseInsensitive)) // Eternity
                    {
                        skipToNextLine();
                        parseSounds();
                    }
                    else if (line.beginsWith("[MUSIC]", CaseInsensitive)) // Eternity
                    {
                        skipToNextLine();
                        parseMusic();
                    }
                    else
                    {
                        // An unknown section.
                        throw UnknownSection(
                            stringf("Expected section name but encountered \"%s\" on line #%i",
                                    line.toString().c_str(),
                                    currentLineNumber));
                    }
                }
                catch(const UnknownSection &er)
                {
                    LOG_WARNING("%s. Skipping section...") << er.asText();
                    skipToNextSection();
                }
            }
        }
        catch(const EndOfFile & /*er*/)
        {} // Ignore.
    }

    void parseAssignmentStatement(const String &line, String &var, String &expr)
    {
        // Determine the split (or 'pivot') position.
        auto assign = line.indexOf('=');
        if (!assign)
        {
            throw SyntaxError(
                "parseAssignmentStatement",
                stringf("Expected assignment statement but encountered \"%s\" on line #%i",
                        line.c_str(),
                        currentLineNumber));
        }

        var  = line.substr(BytePos(0), assign).rightStrip();
        expr = line.substr(assign + 1).leftStrip();

        // Basic grammar checking.
        // Nothing before '=' ?
        if (var.isEmpty())
        {
            throw SyntaxError(
                "parseAssignmentStatement",
                stringf("Expected keyword before '=' on line #%i", currentLineNumber));
        }

        // Nothing after '=' ?
        if (expr.isEmpty())
        {
            throw SyntaxError(
                "parseAssignmentStatement",
                stringf("Expected expression after '=' on line #%i", currentLineNumber));
        }
    }

    bool parseAmmoNum(const String &str, int *ammoNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(ammoNum) *ammoNum = result;
        return (result >= 0 && result < 4);
    }

    int parseMobjNum(const String &str)
    {
        const int num = str.toInt(0, 0, String::AllowSuffix) - 1; // Patch indices are 1-based.
        if(num < 0 || num >= ded->things.size()) return -1;
        return num;
    }

    int parseSoundNum(const String &str)
    {
        const int num = str.toInt(0, 0, String::AllowSuffix);
        if(num < 0 || num >= ded->sounds.size()) return -1;
        return num;
    }

    int parseSpriteNum(const String &str)
    {
        const int num = str.toInt(0, 0, String::AllowSuffix);
        if(num < 0 || num >= NUMSPRITES) return -1;
        return num;
    }

    int parseStateNum(const String &str)
    {
        const int num = str.toInt(0, 0, String::AllowSuffix);
        if(num < 0 || num >= ded->states.size()) return -1;
        return num;
    }

    int parseStateNumFromActionOffset(const String &str)
    {
        const int num = stateIndexForActionOffset(str.toInt(0, 0, String::AllowSuffix));
        if(num < 0 || num >= ded->states.size()) return -1;
        return num;
    }

    bool parseWeaponNum(const String &str, int *weaponNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(weaponNum) *weaponNum = result;
        return (result >= 0);
    }

    bool parseMobjTypeState(const String &token, const StateMapping **state)
    {
        return findStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMobjTypeFlag(const String &token, const FlagMapping **flag)
    {
        return findMobjTypeFlagMappingByDehLabel(token, flag) >= 0;
    }

    bool parseMobjTypeSound(const String &token, const SoundMapping **sound)
    {
        return findSoundMappingByDehLabel(token, sound) >= 0;
    }

    bool parseWeaponState(const String &token, const WeaponStateMapping **state)
    {
        return findWeaponStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMiscValue(const String &token, const ValueMapping **value)
    {
        return findValueMappingForDehLabel(token, value) >= 0;
    }

    void parsePatchSignature()
    {
        for (; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if (!var.compareWithoutCase("Doom version"))
            {
                doomVersion = expr.toInt(0, 10, String::AllowSuffix);
            }
            else if (!var.compareWithoutCase("Patch format"))
            {
                patchVersion = expr.toInt(0, 10, String::AllowSuffix);
            }
            else if (!var.compareWithoutCase("Engine config") || !var.compareWithoutCase("IWAD"))
            {
                // Ignore these WhackEd2 specific values.
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i")
                    << var << currentLineNumber;
            }
        }
    }

    void parseInclude(String arg)
    {
        if (flags & NoInclude)
        {
            LOG_AS("parseInclude");
            LOG_DEBUG("Skipping disabled Include directive");
            return;
        }

        if (stackDepth > maxIncludeDepth)
        {
            LOG_AS("parseInclude");
            if (!maxIncludeDepth)
            {
                LOG_WARNING("Sorry, nested includes are not supported. Directive ignored");
            }
            else
            {
                const char *includes = (maxIncludeDepth == 1 ? "include" : "includes");
                LOG_WARNING("Sorry, there can be at most %i nested %s. Directive ignored")
                    << maxIncludeDepth << includes;
            }
        }
        else
        {
            DehReaderFlags includeFlags = flags & DehReaderFlagsIncludeMask;

            if (arg.beginsWith("notext ", CaseInsensitive))
            {
                includeFlags |= NoText;
                arg.remove(BytePos(0), 7);
            }

            if (!arg.isEmpty())
            {
                const NativePath filePath{arg};
                if (std::ifstream file{filePath})
                {
                    try
                    {
                        LOG_RES_VERBOSE("Including \"%s\"...") << filePath.pretty();
                        DehReader(Block::readAll(file), true /*is-custom*/, includeFlags).parse();
                    }
                    catch (const Error &er)
                    {
                        LOG_WARNING(er.asText() + ".");
                    }
                }
                else
                {
                    LOG_AS("parseInclude");
                    LOG_RES_WARNING("Failed opening \"%s\" for read, aborting...") << filePath;
                }
            }
            else
            {
                LOG_AS("parseInclude");
                LOG_RES_WARNING("Include directive missing filename");
            }
        }
    }

    String readTextBlob(int size)
    {
        if (!size) return String(); // Return an empty string.

        String string;
        do
        {
            // Ignore carriage returns.
            Char c = currentChar();
            if (c != '\r')
                string += c;
            else
                size++;

            advance();
        } while(--size);

        return string.strip();
    }

    /**
     * @todo fixme - missing translations!!!
     *
     * @param arg           Flag argument string to be parsed.
     * @param flagGroups    Flag groups to be updated.
     *
     * @return (& 0x1)= flag group #1 changed, (& 0x2)= flag group #2 changed, etc..
     */
    int parseMobjTypeFlags(const String &arg, int flagGroups[NUM_MOBJ_FLAGS])
    {
        DE_ASSERT(flagGroups);

        if(arg.isEmpty()) return 0; // Erm? No change...
        int changedGroups = 0;

        // Split the argument into discreet tokens and process each individually.
        /// @todo Re-implement with a left-to-right algorithm.
        for (const String &token : arg.split(RegExp("[,+| ]|\t|\f|\r")))
        {
            bool tokenIsNumber;

            int flagsValue = token.toInt(&tokenIsNumber, 10, String::AllowSuffix);
            if(tokenIsNumber)
            {
                // Force the top 4 bits to 0 so that the user is forced to use
                // the mnemonics to change them.
                /// @todo fixme - What about the other groups???
                flagGroups[0] |= (flagsValue & 0x0fffffff);

                changedGroups |= 0x1;
                continue;
            }

            // Flags can also be specified by name (a BEX extension).
            const FlagMapping *flag;
            if(parseMobjTypeFlag(token, &flag))
            {
                /// @todo fixme - Get the proper bit values from the ded def db.
                int value = 0;
                if(flag->bit & 0xff00)
                    value |= 1 << (flag->bit >> 8);
                value |= 1 << (flag->bit & 0xff);

                // Apply the new value.
                DE_ASSERT(flag->group >= 0 && flag->group < NUM_MOBJ_FLAGS);
                flagGroups[flag->group] |= value;

                changedGroups |= 1 << flag->group;
                continue;
            }

            LOG_WARNING("DeHackEd Unknown flag mnemonic '%s'") << token;
        }

        return changedGroups;
    }

    void parseThing(defn::Thing mobj, bool ignore = false)
    {
        LOG_AS("parseThing");

        const int thingNum = mobj.geti(defn::Definition::VAR_ORDER);
        bool hadHeight     = false, checkHeight = false;

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", CaseInsensitive))
            {
                const StateMapping *mapping;
                const String dehStateName = var.left(var.sizeb() - 6);
                if(!parseMobjTypeState(dehStateName, &mapping))
                {
                    if(!ignore)
                    {
                        LOG_WARNING("DeHackEd Frame '%s' unknown") << dehStateName;
                    }
                }
                else
                {
                    const int value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->states.size())
                        {
                            LOG_WARNING("DeHackEd Frame #%i out of range") << value;
                        }
                        else
                        {
                            const int stateIdx = value;
                            const Record &state = ded->states[stateIdx];

                            DE_ASSERT(mapping->id >= 0 && mapping->id < STATENAMES_COUNT);
                            /*qstrncpy(mobj->states[mapping->id], state.gets("id").toLatin1(),
                                     DED_STRINGID_LEN + 1);*/
                            mobj.def()["states"].array().setElement(mapping->id, state.gets("id"));

                            LOG_DEBUG("Type #%i \"%s\" state:%s => \"%s\" (#%i)")
                                    << thingNum << mobj.gets("id") << mapping->name
                                    << mobj.geta("states")[mapping->id].asText()
                                    << stateIdx;
                        }
                    }
                }
            }
            else if(var.endsWith(" sound", CaseInsensitive))
            {
                const SoundMapping *mapping;
                const String dehSoundName = var.left(var.sizeb() - 6);
                if(!parseMobjTypeSound(dehSoundName, &mapping))
                {
                    if(!ignore)
                    {
                        LOG_WARNING("DeHackEd Sound '%s' unknown") << dehSoundName;
                    }
                }
                else
                {
                    const int value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->sounds.size())
                        {
                            LOG_WARNING("DeHackEd Sound #%i out of range") << value;
                        }
                        else
                        {
                            if(mapping->id < SOUNDNAMES_FIRST || mapping->id >= SOUNDNAMES_COUNT)
                            {
                                throw Error("DehReader",
                                            stringf("Thing Sound %i unknown", mapping->id));
                            }

                            const int soundsIdx = value;
                            const ded_sound_t &sound = ded->sounds[soundsIdx];
                            mobj.setSound(mapping->id, sound.id);

                            LOG_DEBUG("Type #%i \"%s\" sound:%s => \"%s\" (#%i)")
                                    << thingNum << mobj.gets("id") << mapping->name
                                    << mobj.sound(mapping->id) << soundsIdx;
                        }
                    }
                }
            }
            else if(!var.compareWithoutCase("Bits"))
            {
                int flags[NUM_MOBJ_FLAGS] = { 0, 0, 0 };
                int changedFlagGroups = parseMobjTypeFlags(expr, flags);
                if(!ignore)
                {
                    // Apply the new flags.
                    for(uint k = 0; k < NUM_MOBJ_FLAGS; ++k)
                    {
                        if(!(changedFlagGroups & (1 << k))) continue;

                        mobj.setFlags(k, flags[k]);
                        LOG_DEBUG("Type #%i \"%s\" flags:%i => %X (%i)")
                                << thingNum << mobj.gets("id") << k
                                << mobj.flags(k) << mobj.flags(k);
                    }

                    // Any special translation necessary?
                    if(changedFlagGroups & 0x1)
                    {
                        if(mobj.flags(0) & 0x100 /*mf_spawnceiling*/)
                            checkHeight = true;

                        // Bit flags are no longer used to specify translucency.
                        // This is just a temporary hack.
                        /*if(info->flags[0] & 0x60000000)
                            info->translucency = (info->flags[0] & 0x60000000) >> 15; */
                    }
                }
            }
            else if(!var.compareWithoutCase("Bits2")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("DeHackEd Thing.Bits2 is not supported");
            }
            else if(!var.compareWithoutCase("Bits3")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("DeHackEd Thing.Bits3 is not supported");
            }
            else if(!var.compareWithoutCase("Blood color")) // Eternity
            {
                /**
                 * Red (normal)        0
                 * Grey                1
                 * Green               2
                 * Blue                3
                 * Yellow              4
                 * Black               5
                 * Purple              6
                 * White               7
                 * Orange              8
                 */
                /// @todo Support this extension.
                LOG_WARNING("DeHackEd Thing.Blood color is not supported");
            }
            else if(!var.compareWithoutCase("ID #"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("doomEdNum", value);
                    LOG_DEBUG("Type #%i \"%s\" doomEdNum => %i") << thingNum << mobj.gets("id") << mobj.geti("doomEdNum");
                }
            }
            else if(!var.compareWithoutCase("Height"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("height", value / float(0x10000));
                    hadHeight = true;
                    LOG_DEBUG("Type #%i \"%s\" height => %f") << thingNum << mobj.gets("id") << mobj.getf("height");
                }
            }
            else if(!var.compareWithoutCase("Hit points"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("spawnHealth", value);
                    LOG_DEBUG("Type #%i \"%s\" spawnHealth => %i") << thingNum << mobj.gets("id") << mobj.geti("spawnHealth");
                }
            }
            else if(!var.compareWithoutCase("Mass"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("mass", value);
                    LOG_DEBUG("Type #%i \"%s\" mass => %i") << thingNum << mobj.gets("id") << mobj.geti("mass");
                }
            }
            else if(!var.compareWithoutCase("Missile damage"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("damage", value);
                    LOG_DEBUG("Type #%i \"%s\" damage => %i") << thingNum << mobj.gets("id") << mobj.geti("damage");
                }
            }
            else if(!var.compareWithoutCase("Pain chance"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("painChance", value);
                    LOG_DEBUG("Type #%i \"%s\" painChance => %i") << thingNum << mobj.gets("id") << mobj.geti("painChance");
                }
            }
            else if(!var.compareWithoutCase("Reaction time"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("reactionTime", value);
                    LOG_DEBUG("Type #%i \"%s\" reactionTime => %i") << thingNum << mobj.gets("id") << mobj.geti("reactionTime");
                }
            }
            else if(!var.compareWithoutCase("Speed"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    /// @todo Is this right??
                    mobj.def().set("speed", (abs(value) < 256 ? float(value) : FIX2FLT(value)));
                    LOG_DEBUG("Type #%i \"%s\" speed => %f") << thingNum << mobj.gets("id") << mobj.getf("speed");
                }
            }
            else if(!var.compareWithoutCase("Translucency")) // Eternity
            {
                //const int value = expr.toInt(0, 10, String::AllowSuffix);
                //const float opacity = de::clamp(0, value, 65536) / 65536.f;
                /// @todo Support this extension.
                LOG_WARNING("DeHackEd Thing.Translucency is not supported");
            }
            else if(!var.compareWithoutCase("Width"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("radius", value / float(0x10000));
                    LOG_DEBUG("Type #%i \"%s\" radius => %f") << thingNum << mobj.gets("id") << mobj.getf("radius");
                }
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i")
                        << var << currentLineNumber;
            }
        }

        /// @todo Does this still make sense given DED can change the values?
        if(checkHeight && !hadHeight)
        {
            mobj.def().set("height", originalHeightForMobjType(thingNum));
        }
    }

    void parseFrame(defn::State state, bool ignore = false)
    {
        LOG_AS("parseFrame");
        const int stateNum = state.geti(defn::Definition::VAR_ORDER);

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Duration"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    state.def().set("tics", value);
                    LOG_DEBUG("State #%i \"%s\" tics => %i")
                        << stateNum << state.gets("id") << state.geti("tics");
                }
            }
            else if(!var.compareWithoutCase("Next frame"))
            {
                const int value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= ded->states.size())
                    {
                        LOG_WARNING("DeHackEd Frame #%i out of range") << value;
                    }
                    else
                    {
                        const int nextStateIdx = value;
                        state.def().set("nextState", ded->states[nextStateIdx].gets("id"));
                        LOG_DEBUG("State #%i \"%s\" nextState => \"%s\" (#%i)")
                                << stateNum << state.gets("id") << state.gets("nextState")
                                << nextStateIdx;
                    }
                }
            }
            else if(!var.compareWithoutCase("Particle event")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("DeHackEd Frame.Particle event is not supported");
            }
            else if(!var.compareWithoutCase("Sprite number"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value > ded->sprites.size())
                    {
                        LOG_WARNING("DeHackEd Sprite #%i out of range") << value;
                    }
                    else
                    {
                        const int spriteIdx = value;
                        const ded_sprid_t &sprite = ded->sprites[spriteIdx];
                        state.def().set("sprite", sprite.id);
                        LOG_DEBUG("State #%i \"%s\" sprite => \"%s\" (#%i)")
                                << stateNum << state.gets("id") << state.gets("sprite")
                                << spriteIdx;
                    }
                }
            }
            else if(!var.compareWithoutCase("Sprite subnumber"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    const int FF_FULLBRIGHT = 0x8000;

                    // Translate the old fullbright bit.
                    int stateFlags = state.geti("flags");
                    if(value & FF_FULLBRIGHT) stateFlags |=  STF_FULLBRIGHT;
                    else                      stateFlags &= ~STF_FULLBRIGHT;
                    state.def().set("flags", stateFlags);
                    state.def().set("frame", value & ~FF_FULLBRIGHT); // frame, not flags

                    LOG_DEBUG("State #%i \"%s\" frame => %i")
                        << stateNum << state.gets("id") << state.geti("frame");
                }
            }
            else if(var.beginsWith("Unknown ", CaseInsensitive))
            {
                const int miscIdx = var.substr(BytePos(8)).toInt(0, 10, String::AllowSuffix);
                const int value   = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(miscIdx < 0 || miscIdx >= NUM_STATE_MISC)
                    {
                        LOG_WARNING("DeHackEd Unknown-value '%s' unknown")
                                << var.substr(BytePos(8));
                    }
                    else
                    {
                        state.setMisc(miscIdx, value);
                        LOG_DEBUG("State #%i \"%s\" misc:%i => %i")
                                << stateNum << state.gets("id") << miscIdx << value;
                    }
                }
            }
            else if(var.beginsWith("Args", CaseInsensitive)) // Eternity
            {
                LOG_WARNING("DeHackEd Frame.%s is not supported") << var;
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i")
                        << var << currentLineNumber;
            }
        }
    }

    void parseSprite(ded_sprid_t *sprite, bool ignore = false)
    {
        const int sprNum = ded->sprites.indexOf(sprite);
        LOG_AS("parseSprite");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Offset"))
            {
                const int value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    // Calculate offset from beginning of sprite names.
                    int offset = -1;
                    if(value > 0)
                    {
                        // From DeHackEd source.
                        DE_ASSERT(doomVersion >= 0 && doomVersion < 5);
                        static int const spriteNameTableOffset[] = { 129044, 129044, 129044, 129284, 129380 };
                        offset = (value - spriteNameTableOffset[doomVersion] - 22044) / 8;
                    }

                    if(offset < 0 || offset >= ded->sprites.size())
                    {
                        LOG_WARNING("DeHackEd Sprite offset #%i out of range") << value;
                    }
                    else
                    {
                        const ded_sprid_t &origSprite = origSpriteNames[offset];
                        strncpy(sprite->id, origSprite.id, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("Sprite #%i id => \"%s\" (#%i)") << sprNum << sprite->id << offset;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i.")
                        << var << currentLineNumber;
            }
        }
    }

    void parseSound(ded_sound_t *sound, bool ignore = false)
    {
        LOG_AS("parseSound");
        const int soundNum = ded->sounds.indexOf(sound);

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Offset")) // sound->id
            {
                LOG_WARNING("DeHackEd Sound.Offset is not supported");
            }
            else if(!var.compareWithoutCase("Zero/One"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->group = value;
                    LOG_DEBUG("Sound #%i \"%s\" group => %i")
                            << soundNum << sound->id << sound->group;
                }
            }
            else if(!var.compareWithoutCase("Value"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->priority = value;
                    LOG_DEBUG("Sound #%i \"%s\" priority => %i")
                            << soundNum << sound->id << sound->priority;
                }
            }
            else if(!var.compareWithoutCase("Zero 1")) // sound->link
            {
                LOG_WARNING("DeHackEd Sound.Zero 1 is not supported");
            }
            else if(!var.compareWithoutCase("Zero 2"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkPitch = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkPitch => %i")
                            << soundNum << sound->id << sound->linkPitch;
                }
            }
            else if(!var.compareWithoutCase("Zero 3"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkVolume = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkVolume => %i")
                            << soundNum << sound->id << sound->linkVolume;
                }
            }
            else if(!var.compareWithoutCase("Zero 4")) // ??
            {
                LOG_WARNING("DeHackEd Sound.Zero 4 is not supported");
            }
            else if(!var.compareWithoutCase("Neg. One 1")) // ??
            {
                LOG_WARNING("DeHackEd Sound.Neg. One 1 is not supported");
            }
            else if(!var.compareWithoutCase("Neg. One 2"))
            {
                const int lumpNum = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    const LumpIndex &lumpIndex = *reinterpret_cast<const LumpIndex *>(F_LumpIndex());
                    const int numLumps = lumpIndex.size();
                    if(lumpNum < 0 || lumpNum >= numLumps)
                    {
                        LOG_WARNING("DeHackEd Neg. One 2 #%i out of range") << lumpNum;
                    }
                    else
                    {
                        strncpy(sound->lumpName, lumpIndex[lumpNum].name(), DED_STRINGID_LEN + 1);
                        LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                                << soundNum << sound->id << sound->lumpName;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i")
                        << var << currentLineNumber;
            }
        }
    }

    void parseAmmo(int const ammoNum, bool ignore = false)
    {
        static const char *ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
        const char *theAmmo = ammostr[ammoNum];
        LOG_AS("parseAmmo");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Max ammo"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(Stringf("Player|Max ammo|%s", theAmmo), String::asText(value));
            }
            else if(!var.compareWithoutCase("Per ammo"))
            {
                int per = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(Stringf("Player|Clip ammo|%s", theAmmo), String::asText(per));
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i")
                        << var << currentLineNumber;
            }
        }
    }

    void parseWeapon(int const weapNum, bool ignore = false)
    {
        LOG_AS("parseWeapon");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", CaseInsensitive))
            {
                const String dehStateName = var.left(var.sizeb() - 6);
                const int value           = expr.toInt(0, 0, String::AllowSuffix);

                const WeaponStateMapping *weapon;
                if(!parseWeaponState(dehStateName, &weapon))
                {
                    if(!ignore)
                    {
                        LOG_WARNING("DeHackEd Frame '%s' unknown") << dehStateName;
                    }
                }
                else
                {
                    if(!ignore)
                    {
                        if(value < 0 || value > ded->states.size())
                        {
                            LOG_WARNING("DeHackEd Frame #%i out of range") << value;
                        }
                        else
                        {
                            DE_ASSERT(weapon->id >= 0 && weapon->id < ded->states.size());

                            const Record &state = ded->states[value];
                            createValueDef(
                                Stringf("Weapon Info|%i|%s", weapNum, weapon->name.c_str()),
                                state.gets("id"));
                        }
                    }
                }
            }
            else if (!var.compareWithoutCase("Ammo type"))
            {
                static const char *ammotypes[] = { "clip", "shell", "cell", "misl", "-", "noammo" };
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= 6)
                    {
                        LOG_WARNING("DeHackEd Ammo Type %i unknown") << value;
                    }
                    else
                    {
                        createValueDef(
                            Stringf("Weapon Info|%i|Type", weapNum), ammotypes[value]);
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo per shot")) // Eternity
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if (!ignore)
                    createValueDef(Stringf("Weapon Info|%i|Per shot", weapNum),
                                   String::asText(value));
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i")
                        << var << currentLineNumber;
            }
        }
    }

    void parsePointer(defn::State state, bool ignore)
    {
        LOG_AS("parsePointer");
        const int stateNum = state.geti(defn::Definition::VAR_ORDER);

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Codep Frame"))
            {
                const int actionIdx = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    if(actionIdx < 0 || actionIdx >= NUMSTATES)
                    {
                        LOG_WARNING("DeHackEd Codep frame #%i out of range") << actionIdx;
                    }
                    else
                    {
                        state.def().set("action", origActionNames[actionIdx]);
                        LOG_DEBUG("State #%i \"%s\" action => \"%s\"")
                                << stateNum << state.gets("id") << state.gets("action");
                    }
                }
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i")
                        << var << currentLineNumber;
            }
        }
    }

    void parseMisc()
    {
        LOG_AS("parseMisc");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            const ValueMapping *mapping;
            if(parseMiscValue(var, &mapping))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                createValueDef(mapping->valuePath, String::asText(value));
            }
            else
            {
                LOG_WARNING("Misc-value \"%s\" unknown") << var;
            }
        }
    }

    void parsePars() // BEX
    {
        LOG_AS("parsePars");
        // BEX doesn't follow the same rules as .deh
        for(; !line.isEmpty(); readLine())
        {
            // Skip comment lines.
            if (line.first() == '#') continue;

            try
            {
                if (line.beginsWith("par", CaseInsensitive))
                {
                    const String argStr = line.substr(3).leftStrip();
                    if (argStr.isEmpty())
                    {
                        throw SyntaxError(
                            "parseParsBex",
                            stringf("Expected format expression on line #%i", currentLineNumber));
                    }

                    /**
                     * @attention Team TNT's original DEH parser would read the first one
                     * or two tokens then apply atoi() on the remainder of the line to
                     * obtain the last argument (i.e., par time).
                     *
                     * Here we emulate this behavior by splitting the line into at most
                     * three arguments and then apply atoi()-like de::String::toIntLeft()
                     * on the last.
                     */
                    const int maxArgs = 3;
                    StringList args = splitMax(argStr, ' ', maxArgs);

                    // If the third argument is a comment remove it.
                    if(args.size() == 3)
                    {
                        if(String(args.at(2)).beginsWith('#'))
                            args.removeAt(2);
                    }

                    if(args.size() < 2)
                    {
                        throw SyntaxError("parseParsBex",
                                          stringf("Invalid format string \"%s\" on line #%i",
                                                  argStr.c_str(),
                                                  currentLineNumber));
                    }

                    // Parse values from the arguments.
                    int arg = 0;
                    int episode   = (args.size() > 2? args.at(arg++).toInt(0, 10) : 0);
                    int map       = args.at(arg++).toInt(0, 10);
                    float parTime = float(String(args.at(arg++)).toInt(0, 10, String::AllowSuffix));

                    // Apply.
                    const res::Uri uri = composeMapUri(episode, map);
                    int idx = ded->getMapInfoNum(uri);
                    if(idx >= 0)
                    {
                        ded->mapInfos[idx].set("parTime", parTime);
                        LOG_DEBUG("MapInfo #%i \"%s\" parTime => %d")
                                << idx << uri << parTime;
                    }
                    else
                    {
                        LOG_WARNING("Failed locating MapInfo for \"%s\" (episode:%i, map:%i)")
                                << uri << episode << map;
                    }
                }
            }
            catch(const SyntaxError &er)
            {
                LOG_WARNING("%s") << er.asText();
            }
        }

        if (line.isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseHelper() // Eternity
    {
        LOG_AS("parseHelper");
        LOG_WARNING("DeHackEd [HELPER] patches are not supported");
    }

    void parseSprites() // Eternity
    {
        LOG_AS("parseSprites");
        LOG_WARNING("DeHackEd [SPRITES] patches are not supported");
    }

    void parseSounds() // Eternity
    {
        LOG_AS("parseSounds");
        // BEX doesn't follow the same rules as .deh
        for (; !line.isEmpty(); readLine())
        {
            // Skip comment lines.
            if (line.first() == '#') continue;

            try
            {
                String var, expr;
                parseAssignmentStatement(line, var, expr);
                if (!patchSoundLumpNames(var, expr))
                {
                    LOG_WARNING("Failed to locate sound \"%s\" for patching") << var;
                }
            }
            catch (const SyntaxError &er)
            {
                LOG_WARNING("%s") << er.asText();
            }
        }

        if (line.isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseMusic() // Eternity
    {
        LOG_AS("parseMusic");
        // BEX doesn't follow the same rules as .deh
        for (; !line.isEmpty(); readLine())
        {
            // Skip comment lines.
            if (line.first() == '#') continue;

            try
            {
                String var, expr;
                parseAssignmentStatement(line, var, expr);
                if (!patchMusicLumpNames(var, expr))
                {
                    LOG_WARNING("Failed to locate music \"%s\" for patching") << var;
                }
            }
            catch (const SyntaxError &er)
            {
                LOG_WARNING("%s") << er.asText();
            }
        }

        if (line.isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseCodePointers() // BEX
    {
        LOG_AS("parseCodePointers");
        // BEX doesn't follow the same rules as .deh
        for(; !line.isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.first() == '#') continue;

            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.beginsWith("Frame ", CaseInsensitive))
            {
                const int stateNum = var.substr(BytePos(6)).toInt(0, 0, String::AllowSuffix);
                if(stateNum < 0 || stateNum >= ded->states.size())
                {
                    LOG_WARNING("DeHackEd Frame #%d out of range\n(Create more State defs!)")
                            << stateNum;
                }
                else
                {
                    Record &state = ded->states[stateNum];

                    // Compose the action name.
                    String action = expr.rightStrip();
                    if (!action.beginsWith("A_", CaseInsensitive)) action.prepend("A_");
                    action.truncate(BytePos(32));

                    // Is this a known action?
                    if(!action.compareWithoutCase("A_NULL"))
                    {
                        state.set("action", "NULL");
                        LOG_DEBUG("State #%i \"%s\" action => \"NULL\"")
                                << stateNum << state.gets("id");
                    }
                    else
                    {
                        if(Def_Get(DD_DEF_ACTION, action, nullptr))
                        {
                            state.set("action", action);
                            LOG_DEBUG("State #%i \"%s\" action => \"%s\"")
                                    << stateNum << state.gets("id") << state.gets("action");
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Action '%s' unknown")
                                    << action.substr(BytePos(2));
                        }
                    }
                }
            }
        }

        if(line.isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseText(int const oldSize, int const newSize)
    {
        LOG_AS("parseText");

        const String oldStr = readTextBlob(oldSize);
        const String newStr = readTextBlob(newSize);

        if(!(flags & NoText)) // Disabled?
        {
            // Try each type of "text" replacement in turn...
            bool found = false;
            if(patchFinaleBackgroundNames(oldStr, newStr))  found = true;
            if(patchMusicLumpNames(oldStr, newStr))         found = true;
            if(patchSpriteNames(oldStr, newStr))            found = true;
            if(patchSoundLumpNames(oldStr, newStr))         found = true;
            if(patchText(oldStr, newStr))                   found = true;

            // Give up?
            if(!found)
            {
                LOG_WARNING("Failed to determine source for:\nText %i %i\n%s")
                        << oldSize << newSize << oldStr;
            }
        }
        else
        {
            LOG_DEBUG("Skipping disabled Text patch");
        }

        skipToNextLine();
    }

    static void replaceTextValue(const String &id, String newValue)
    {
        if(id.isEmpty()) return;

        int textIdx = ded->getTextNum(id);
        if(textIdx < 0) return;

        // We must escape new lines.
        newValue.replace("\n", "\\n");

        // Replace this text.
        ded->text[textIdx].setText(newValue);
        LOG_DEBUG("Text #%i \"%s\" is now:\n%s")
                << textIdx << id << newValue;
    }

    void parseStrings() // BEX
    {
        LOG_AS("parseStrings");

        bool multiline = false;
        String textId;
        String newValue;

        // BEX doesn't follow the same rules as .deh
        for (;; readLine())
        {
            if (!multiline)
            {
                if (line.isEmpty()) break;

                // Skip comment lines.
                if (line.first() == '#') continue;

                // Determine the split (or 'pivot') position.
                dsize assign = line.indexOf('=');
                if (assign == CString::npos)
                {
                    throw SyntaxError(
                        "parseStrings",
                        stringf("Expected assignment statement but encountered \"%s\" on line #%i",
                                line.toString().c_str(),
                                currentLineNumber));
                }

                textId = line.substr(0, assign).rightStrip();

                // Nothing before '=' ?
                if (textId.isEmpty())
                {
                    throw SyntaxError(
                        "parseStrings",
                        stringf("Expected keyword before '=' on line #%i", currentLineNumber));
                }

                newValue = line.substr(assign + 1).leftStrip();
            }
            else
            {
                newValue += line.leftStrip();
            }

            // Concatenate another multi-line replacement?
            if (newValue.endsWith('\\'))
            {
                newValue.truncate(newValue.sizeb() - 1);
                multiline = true;
                continue;
            }

            replaceTextValue(textId, newValue);
            multiline = false;
        }

        if (line.isEmpty())
        {
            skipToNextSection();
        }
    }

    void createValueDef(const String &path, const String &value)
    {
        // An existing value?
        ded_value_t *def;
        int idx = valueDefForPath(path, &def);
        if(idx < 0)
        {
            // Not found - create a new Value.
            def = ded->values.append();
            def->id   = M_StrDup(path);
            def->text = 0;

            idx = ded->values.indexOf(def);
        }

        def->text = static_cast<char *>(M_Realloc(def->text, value.size() + 1));
        strcpy(def->text, value);

        LOG_DEBUG("Value #%i \"%s\" => \"%s\"") << idx << path << def->text;
    }

    bool patchSpriteNames(String const &origName, String const &newName)
    {
        // Is this potentially a sprite name?
        if (origName.length() != 4 || newName.length() != 4)
        {
            return false;
        }

        // Look for the corresponding sprite definition and change the sprite name.
        auto *defs = DED_Definitions();
        for (int i = 0; i < defs->sprites.size(); ++i)
        {
            if (String(defs->sprites[i].id).compareWithoutCase(origName) == 0)
            {
                strcpy(defs->sprites[i].id, newName);
                LOG_DEBUG("Sprite #%d \"%s\" => \"%s\"")
                    << i << origName << newName;

                // Update all states that refer to this sprite.
                for (int s = 0; s < defs->states.size(); ++s)
                {
                    auto &state = defs->states[s];
                    if (state.gets("sprite") == origName)
                    {
                        state.set("sprite", newName);
                    }
                }
                return true;
            }
        }
        return false;

#if 0
        if(spriteIdx >= ded->count.sprites.num)
        {
            LOG_WARNING("Sprite name #%i out of range.") << spriteIdx;
            return false;
        }

        ded_sprid_t &def = ded->sprites[spriteIdx];
        Block newNameUtf8 = newName.toUtf8();
        qstrncpy(def.id, newNameUtf8.constData(), DED_SPRITEID_LEN + 1);

        LOG_DEBUG("Sprite #%i id => \"%s\"") << spriteIdx << def.id;
        return true;
#endif
    }

    bool patchFinaleBackgroundNames(const String &origName, const String &newName)
    {
        const FinaleBackgroundMapping *mapping;
        if(findFinaleBackgroundMappingForText(origName, &mapping) < 0) return false;
        createValueDef(mapping->mnemonic, newName);
        return true;
    }

    bool patchMusicLumpNames(const String &origName, const String &newName)
    {
        // Only music lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findMusicLumpNameInMap(origName) < 0) return false;

        String origNamePref = "D_" + origName;
        String newNamePref  = "D_" + newName;

        // Update ALL songs using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->musics.size(); ++i)
        {
            defn::Definition music{ded->musics[i]};
            if (music.id().endsWith("_dd_xlt"))
            {
                // This is a Music definition generated by MapInfoTranslator based on
                // a custom MAPINFO lump. We'll skip it because the music lump set in the
                // MAPINFO should be used instead.
                continue;
            }
            if (music.gets("lumpName").compareWithoutCase(origNamePref)) continue;

            music.def().set("lumpName", newNamePref);
            numPatched++;

            LOG_DEBUG("Music #%i \"%s\" lumpName => \"%s\"")
                << i << music.id() << music.gets("lumpName");
        }
        return (numPatched > 0);
    }

    bool patchSoundLumpNames(const String &origName, const String &newName)
    {
        // Only sound lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findSoundLumpNameInMap(origName) < 0) return false;

        String origNamePrefUtf8 = "DS" + origName;
        String newNamePrefUtf8  = "DS" + newName;

        // Update ALL sounds using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->sounds.size(); ++i)
        {
            ded_sound_t &sound = ded->sounds[i];
            if(iCmpStrCase(sound.lumpName, origNamePrefUtf8)) continue;

            strncpy(sound.lumpName, newNamePrefUtf8, 9);
            numPatched++;

            LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                    << i << sound.id << sound.lumpName;
        }
        return (numPatched > 0);
    }

    bool patchText(const String &origStr, const String &newStr)
    {
        const TextMapping *textMapping;

        // Which text are we replacing?
        if (textMappingForBlob(origStr, &textMapping) < 0) return false;

        // Is replacement disallowed/not-supported?
        if (textMapping->name.isEmpty()) return true; // Pretend success.

        int textIdx = ded->getTextNum(textMapping->name);
        if (textIdx < 0) return false;

        // We must escape new lines.
        String newStrCopy = newStr;
        newStrCopy.replace("\n", "\\n");

        // Replace this text.
        ded->text[textIdx].setText(newStrCopy);

        LOG_DEBUG("Text #%i \"%s\" is now:\n%s") << textIdx << textMapping->name << newStrCopy;
        return true;
    }
};

void readDehPatch(const Block &patch, bool patchIsCustom, DehReaderFlags flags)
{
    try
    {
        DehReader(patch, patchIsCustom, flags).parse();
    }
    catch(const Error &er)
    {
        LOG_WARNING(er.asText() + ".");
    }
}
