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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QStringList>

#include <de/memory.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/filesys/lumpindex.h>
#include <doomsday/defs/thing.h>
#include <doomsday/defs/state.h>
#include <doomsday/defs/ded.h>
#include <doomsday/game.h>
#include <de/App>
#include <de/ArrayValue>
#include <de/Block>
#include <de/Error>
#include <de/Log>
#include <de/String>

#include "importdeh.h"
#include "dehreader_util.h"
#include "info.h"

using namespace de;

static int stackDepth;
static const int maxIncludeDepth = de::max(0, DEHREADER_INCLUDE_DEPTH_MAX);

/// Mask containing only those reader flags which should be passed from the current
/// parser to any child parsers for file include statements.
static DehReaderFlag const DehReaderFlagsIncludeMask = IgnoreEOF;

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
    DENG2_ERROR(SyntaxError);

    /// The parser encountered an unknown section in the source file. @ingroup errors
    DENG2_ERROR(UnknownSection);

    /// The parser reached the end of the source file. @ingroup errors
    DENG2_ERROR(EndOfFile);

public:
    Block const &patch;
    bool patchIsCustom = true;

    int pos = 0;
    int currentLineNumber = 0;

    DehReaderFlags flags = 0;

    int patchVersion = -1;  ///< @c -1= Unknown.
    int doomVersion  = -1;  ///< @c -1= Unknown.

    String line;            ///< Current line.

public:
    DehReader(Block const &patch, bool patchIsCustom = true, DehReaderFlags flags = 0)
        : patch(patch), patchIsCustom(patchIsCustom), flags(flags)
    {
        stackDepth++;
    }

    ~DehReader()
    {
        stackDepth--;
    }

#if 0
    LogEntry &log(Log::LogLevel level, String const &format)
    {
        return LOG().enter(level, format);
    }
#endif

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

    bool atRealEnd()
    {
        return size_t(pos) >= patch.size();
    }

    bool atEnd()
    {
        if(atRealEnd()) return true;
        if(!(flags & IgnoreEOF) && patch.at(pos) == '\0') return true;
        return false;
    }

    void advance()
    {
        if(atEnd()) return;

        // Handle special characters in the input.
        char ch = currentChar().toLatin1();
        switch(ch)
        {
        case '\0':
            if(size_t(pos) != patch.size() - 1)
            {
                LOG_WARNING("Unexpected EOF encountered on line #%i") << currentLineNumber;
            }
            break;
        case '\n':
            currentLineNumber++;
            break;
        default: break;
        }

        pos++;
    }

    QChar currentChar()
    {
        if(atEnd()) return 0;
        return QChar::fromLatin1(patch.at(pos));
    }

    void skipToEOL()
    {
        while(!atEnd() && currentChar() != '\n') advance();
    }

    void readLine()
    {
        int start = pos;
        skipToEOL();
        if(!atEnd())
        {
            int endOfLine = pos - start;
            // Ignore any trailing carriage return.
            if(endOfLine > 0 && patch.at(start + endOfLine - 1) == '\r') endOfLine -= 1;

            QByteArray rawLine = patch.mid(start, endOfLine);

            // When tolerating mid stream EOF characters, we must first
            // strip them before attempting any encoding conversion.
            if(flags & IgnoreEOF)
            {
                rawLine.replace('\0', "");
            }

            // Perform encoding conversion for this line and move on.
            line = QString::fromLatin1(rawLine);
            if(currentChar() == '\n') advance();
            return;
        }
        throw EndOfFile(String("EOF on line #%1").arg(currentLineNumber));
    }

    /**
     * Keep reading lines until we find one that is something other than
     * whitespace or a whole-line comment.
     */
    String const &skipToNextLine()
    {
        forever
        {
            readLine();
            if(!line.trimmed().isEmpty() && line.at(0) != '#') break;
        }
        return line;
    }

    bool lineInCurrentSection()
    {
        return line.indexOf('=') != -1;
    }

    void skipToNextSection()
    {
        do skipToNextLine();
        while(lineInCurrentSection());
    }

    void logPatchInfo()
    {
        // Log reader settings and patch version information.
        LOG_RES_MSG("Patch version: %i, Doom version: %i\nNoText: %b")
            << patchVersion << doomVersion << bool(flags & NoText);

        if(patchVersion != 6)
        {
            LOG_WARNING("Patch version %i unknown, unexpected results may occur") << patchVersion;
        }
    }

    void parse()
    {
        LOG_AS_STRING(stackDepth == 1? "DehReader" : String("[%1]").arg(stackDepth - 1));

        skipToNextLine();

        // Attempt to parse the DeHackEd patch signature and version numbers.
        if(line.beginsWith("Patch File for DeHackEd v", String::CaseInsensitive))
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
        if(!normalizeDoomVersion(doomVersion))
        {
            LOG_WARNING("Doom version undefined, assuming v1.9");
            doomVersion = 3;
        }

        // Patches are subdivided into sections.
        try
        {
            forever
            {
                try
                {
                    /// @note Some sections have their own grammar quirks!
                    if(line.beginsWith("include", String::CaseInsensitive)) // BEX
                    {
                        parseInclude(line.substr(7).leftStrip());
                        skipToNextSection();
                    }
                    else if(line.beginsWith("Thing", String::CaseInsensitive))
                    {
                        Record *mobj;
                        Record dummyMobj;

                        String const arg  = line.substr(5).leftStrip();
                        int const mobjNum = parseMobjNum(arg);
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
                    else if(line.beginsWith("Frame", String::CaseInsensitive))
                    {
                        Record *state;
                        Record dummyState;

                        String const arg   = line.substr(5).leftStrip();
                        int const stateNum = parseStateNum(arg);
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
                    else if(line.beginsWith("Pointer", String::CaseInsensitive))
                    {
                        Record *state;
                        Record dummyState;

                        String const arg   = line.substr(7).leftStrip();
                        int const stateNum = parseStateNumFromActionOffset(arg);
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
                    else if(line.beginsWith("Sprite", String::CaseInsensitive))
                    {
                        ded_sprid_t *sprite;
                        Dummy<ded_sprid_t> dummySprite;

                        String const arg    = line.substr(6).leftStrip();
                        int const spriteNum = parseSpriteNum(arg);
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
                    else if(line.beginsWith("Ammo", String::CaseInsensitive))
                    {
                        String const arg          = line.substr(4).leftStrip();
                        int ammoNum               = 0;
                        bool const isKnownAmmoNum = parseAmmoNum(arg, &ammoNum);
                        bool const ignore         = !isKnownAmmoNum;

                        if(!isKnownAmmoNum)
                        {
                            LOG_WARNING("DeHackEd Ammo '%s' out of range") << arg;
                        }

                        skipToNextLine();
                        parseAmmo(ammoNum, ignore);
                    }
                    else if(line.beginsWith("Weapon", String::CaseInsensitive))
                    {
                        String const arg            = line.substr(6).leftStrip();
                        int weaponNum               = 0;
                        bool const isKnownWeaponNum = parseWeaponNum(arg, &weaponNum);
                        bool const ignore           = !isKnownWeaponNum;

                        if(!isKnownWeaponNum)
                        {
                            LOG_WARNING("DeHackEd Weapon '%s' out of range") << arg;
                        }

                        skipToNextLine();
                        parseWeapon(weaponNum, ignore);
                    }
                    else if(line.beginsWith("Sound", String::CaseInsensitive))
                    {
                        ded_sound_t *sound;
                        Dummy<ded_sound_t> dummySound;

                        String const arg   = line.substr(5).leftStrip();
                        int const soundNum = parseSoundNum(arg);
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
                    else if(line.beginsWith("Text", String::CaseInsensitive))
                    {
                        String args = line.substr(4).leftStrip();
                        int firstArgEnd = args.indexOf(' ');
                        if(firstArgEnd < 0)
                        {
                            throw SyntaxError(String("Expected old text size on line #%1")
                                                .arg(currentLineNumber));
                        }

                        bool isNumber;
                        int const oldSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(String("Expected old text size but encountered \"%1\" on line #%2")
                                                .arg(args.substr(firstArgEnd)).arg(currentLineNumber));
                        }

                        args.remove(0, firstArgEnd + 1);

                        int const newSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(String("Expected new text size but encountered \"%1\" on line #%2")
                                                .arg(args).arg(currentLineNumber));
                        }

                        parseText(oldSize, newSize);
                    }
                    else if(line.beginsWith("Misc", String::CaseInsensitive))
                    {
                        skipToNextLine();
                        parseMisc();
                    }
                    else if(line.beginsWith("Cheat", String::CaseInsensitive))
                    {
                        if(!(!patchIsCustom && DoomsdayApp::game().id() == "hacx"))
                        {
                            LOG_WARNING("DeHackEd [Cheat] patches are not supported");
                        }
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[CODEPTR]", String::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseCodePointers();
                    }
                    else if(line.beginsWith("[PARS]", String::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parsePars();
                    }
                    else if(line.beginsWith("[STRINGS]", String::CaseInsensitive)) // BEX
                    {
                        skipToNextLine();
                        parseStrings();
                    }
                    else if(line.beginsWith("[HELPER]", String::CaseInsensitive)) // Eternity
                    {
                        // Not yet supported (Helper Dogs from MBF).
                        //skipToNextLine();
                        parseHelper();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[SPRITES]", String::CaseInsensitive)) // Eternity
                    {
                        // Not yet supported.
                        //skipToNextLine();
                        parseSprites();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[SOUNDS]", String::CaseInsensitive)) // Eternity
                    {
                        skipToNextLine();
                        parseSounds();
                    }
                    else if(line.beginsWith("[MUSIC]", String::CaseInsensitive)) // Eternity
                    {
                        skipToNextLine();
                        parseMusic();
                    }
                    else
                    {
                        // An unknown section.
                        throw UnknownSection(String("Expected section name but encountered \"%1\" on line #%2")
                                                 .arg(line).arg(currentLineNumber));
                    }
                }
                catch(UnknownSection const &er)
                {
                    LOG_WARNING("%s. Skipping section...") << er.asText();
                    skipToNextSection();
                }
            }
        }
        catch(EndOfFile const & /*er*/)
        {} // Ignore.
    }

    void parseAssignmentStatement(String const &line, String &var, String &expr)
    {
        // Determine the split (or 'pivot') position.
        int assign = line.indexOf('=');
        if(assign < 0)
        {
            throw SyntaxError("parseAssignmentStatement",
                              String("Expected assignment statement but encountered \"%1\" on line #%2")
                                .arg(line).arg(currentLineNumber));
        }

        var  = line.substr(0, assign).rightStrip();
        expr = line.substr(assign + 1).leftStrip();

        // Basic grammar checking.
        // Nothing before '=' ?
        if(var.isEmpty())
        {
            throw SyntaxError("parseAssignmentStatement",
                              String("Expected keyword before '=' on line #%1").arg(currentLineNumber));
        }

        // Nothing after '=' ?
        if(expr.isEmpty())
        {
            throw SyntaxError("parseAssignmentStatement",
                              String("Expected expression after '=' on line #%1").arg(currentLineNumber));
        }
    }

    bool parseAmmoNum(String const &str, int *ammoNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(ammoNum) *ammoNum = result;
        return (result >= 0 && result < 4);
    }

    int parseMobjNum(String const &str)
    {
        int const num = str.toInt(0, 0, String::AllowSuffix) - 1; // Patch indices are 1-based.
        if(num < 0 || num >= ded->things.size()) return -1;
        return num;
    }

    int parseSoundNum(String const &str)
    {
        int const num = str.toInt(0, 0, String::AllowSuffix);
        if(num < 0 || num >= ded->sounds.size()) return -1;
        return num;
    }

    int parseSpriteNum(String const &str)
    {
        int const num = str.toInt(0, 0, String::AllowSuffix);
        if(num < 0 || num >= NUMSPRITES) return -1;
        return num;
    }

    int parseStateNum(String const &str)
    {
        int const num = str.toInt(0, 0, String::AllowSuffix);
        if(num < 0 || num >= ded->states.size()) return -1;
        return num;
    }

    int parseStateNumFromActionOffset(String const &str)
    {
        int const num = stateIndexForActionOffset(str.toInt(0, 0, String::AllowSuffix));
        if(num < 0 || num >= ded->states.size()) return -1;
        return num;
    }

    bool parseWeaponNum(String const &str, int *weaponNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(weaponNum) *weaponNum = result;
        return (result >= 0);
    }

    bool parseMobjTypeState(QString const &token, StateMapping const **state)
    {
        return findStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMobjTypeFlag(QString const &token, FlagMapping const **flag)
    {
        return findMobjTypeFlagMappingByDehLabel(token, flag) >= 0;
    }

    bool parseMobjTypeSound(QString const &token, SoundMapping const **sound)
    {
        return findSoundMappingByDehLabel(token, sound) >= 0;
    }

    bool parseWeaponState(QString const &token, WeaponStateMapping const **state)
    {
        return findWeaponStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMiscValue(QString const &token, ValueMapping const **value)
    {
        return findValueMappingForDehLabel(token, value) >= 0;
    }

    void parsePatchSignature()
    {
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Doom version"))
            {
                doomVersion = expr.toInt(0, 10, String::AllowSuffix);
            }
            else if(!var.compareWithoutCase("Patch format"))
            {
                patchVersion = expr.toInt(0, 10, String::AllowSuffix);
            }
            else if(!var.compareWithoutCase("Engine config") ||
                    !var.compareWithoutCase("IWAD"))
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
        if(flags & NoInclude)
        {
            LOG_AS("parseInclude");
            LOG_DEBUG("Skipping disabled Include directive");
            return;
        }

        if(stackDepth > maxIncludeDepth)
        {
            LOG_AS("parseInclude");
            if(!maxIncludeDepth)
            {
                LOG_WARNING("Sorry, nested includes are not supported. Directive ignored");
            }
            else
            {
                char const *includes = (maxIncludeDepth == 1? "include" : "includes");
                LOG_WARNING("Sorry, there can be at most %i nested %s. Directive ignored")
                        << maxIncludeDepth << includes;
            }
        }
        else
        {
            DehReaderFlags includeFlags = flags & DehReaderFlagsIncludeMask;

            if(arg.beginsWith("notext ", String::CaseInsensitive))
            {
                includeFlags |= NoText;
                arg.remove(0, 7);
            }

            if(!arg.isEmpty())
            {
                NativePath const filePath(arg);
                QFile file(filePath);
                if(!file.open(QFile::ReadOnly | QFile::Text))
                {
                    LOG_AS("parseInclude");
                    LOG_RES_WARNING("Failed opening \"%s\" for read, aborting...") << filePath;
                }
                else
                {
                    /// @todo Do not use a local buffer.
                    Block deh = file.readAll();
                    file.close();

                    LOG_RES_VERBOSE("Including \"%s\"...") << filePath.pretty();

                    try
                    {
                        DehReader(deh, true/*is-custom*/, includeFlags).parse();
                    }
                    catch(Error const &er)
                    {
                        LOG_WARNING(er.asText() + ".");
                    }
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
        if(!size) return String(); // Return an empty string.

        String string;
        do
        {
            // Ignore carriage returns.
            QChar c = currentChar();
            if(c != '\r')
                string += c;
            else
                size++;

            advance();
        } while(--size);

        return string.trimmed();
    }

    /**
     * @todo fixme - missing translations!!!
     *
     * @param arg           Flag argument string to be parsed.
     * @param flagGroups    Flag groups to be updated.
     *
     * @return (& 0x1)= flag group #1 changed, (& 0x2)= flag group #2 changed, etc..
     */
    int parseMobjTypeFlags(QString const &arg, int flagGroups[NUM_MOBJ_FLAGS])
    {
        DENG2_ASSERT(flagGroups);

        if(arg.isEmpty()) return 0; // Erm? No change...
        int changedGroups = 0;

        // Split the argument into discreet tokens and process each individually.
        /// @todo Re-implement with a left-to-right algorithm.
        QStringList tokens = arg.split(QRegExp("[,+| ]|\t|\f|\r"), QString::SkipEmptyParts);
        DENG2_FOR_EACH_CONST(QStringList, i, tokens)
        {
            String const &token = *i;
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
            FlagMapping const *flag;
            if(parseMobjTypeFlag(token, &flag))
            {
                /// @todo fixme - Get the proper bit values from the ded def db.
                int value = 0;
                if(flag->bit & 0xff00)
                    value |= 1 << (flag->bit >> 8);
                value |= 1 << (flag->bit & 0xff);

                // Apply the new value.
                DENG2_ASSERT(flag->group >= 0 && flag->group < NUM_MOBJ_FLAGS);
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

        int const thingNum = mobj.geti(defn::Definition::VAR_ORDER);
        bool hadHeight     = false, checkHeight = false;

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", String::CaseInsensitive))
            {
                StateMapping const *mapping;
                String const dehStateName = var.left(var.size() - 6);
                if(!parseMobjTypeState(dehStateName, &mapping))
                {
                    if(!ignore)
                    {
                        LOG_WARNING("DeHackEd Frame '%s' unknown") << dehStateName;
                    }
                }
                else
                {
                    int const value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->states.size())
                        {
                            LOG_WARNING("DeHackEd Frame #%i out of range") << value;
                        }
                        else
                        {
                            int const stateIdx = value;
                            Record const &state = ded->states[stateIdx];

                            DENG2_ASSERT(mapping->id >= 0 && mapping->id < STATENAMES_COUNT);
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
            else if(var.endsWith(" sound", String::CaseInsensitive))
            {
                SoundMapping const *mapping;
                String const dehSoundName = var.left(var.size() - 6);
                if(!parseMobjTypeSound(dehSoundName, &mapping))
                {
                    if(!ignore)
                    {
                        LOG_WARNING("DeHackEd Sound '%s' unknown") << dehSoundName;
                    }
                }
                else
                {
                    int const value = expr.toInt(0, 0, String::AllowSuffix);
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
                                throw Error("DehReader", String("Thing Sound %i unknown").arg(mapping->id));
                            }

                            int const soundsIdx = value;
                            ded_sound_t const &sound = ded->sounds[soundsIdx];
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
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("doomEdNum", value);
                    LOG_DEBUG("Type #%i \"%s\" doomEdNum => %i") << thingNum << mobj.gets("id") << mobj.geti("doomEdNum");
                }
            }
            else if(!var.compareWithoutCase("Height"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("height", value / float(0x10000));
                    hadHeight = true;
                    LOG_DEBUG("Type #%i \"%s\" height => %f") << thingNum << mobj.gets("id") << mobj.getf("height");
                }
            }
            else if(!var.compareWithoutCase("Hit points"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("spawnHealth", value);
                    LOG_DEBUG("Type #%i \"%s\" spawnHealth => %i") << thingNum << mobj.gets("id") << mobj.geti("spawnHealth");
                }
            }
            else if(!var.compareWithoutCase("Mass"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("mass", value);
                    LOG_DEBUG("Type #%i \"%s\" mass => %i") << thingNum << mobj.gets("id") << mobj.geti("mass");
                }
            }
            else if(!var.compareWithoutCase("Missile damage"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("damage", value);
                    LOG_DEBUG("Type #%i \"%s\" damage => %i") << thingNum << mobj.gets("id") << mobj.geti("damage");
                }
            }
            else if(!var.compareWithoutCase("Pain chance"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("painChance", value);
                    LOG_DEBUG("Type #%i \"%s\" painChance => %i") << thingNum << mobj.gets("id") << mobj.geti("painChance");
                }
            }
            else if(!var.compareWithoutCase("Reaction time"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj.def().set("reactionTime", value);
                    LOG_DEBUG("Type #%i \"%s\" reactionTime => %i") << thingNum << mobj.gets("id") << mobj.geti("reactionTime");
                }
            }
            else if(!var.compareWithoutCase("Speed"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    /// @todo Is this right??
                    mobj.def().set("speed", (abs(value) < 256 ? float(value) : FIX2FLT(value)));
                    LOG_DEBUG("Type #%i \"%s\" speed => %f") << thingNum << mobj.gets("id") << mobj.getf("speed");
                }
            }
            else if(!var.compareWithoutCase("Translucency")) // Eternity
            {
                //int const value = expr.toInt(0, 10, String::AllowSuffix);
                //float const opacity = de::clamp(0, value, 65536) / 65536.f;
                /// @todo Support this extension.
                LOG_WARNING("DeHackEd Thing.Translucency is not supported");
            }
            else if(!var.compareWithoutCase("Width"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
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
        int const stateNum = state.geti(defn::Definition::VAR_ORDER);

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Duration"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    state.def().set("tics", value);
                    LOG_DEBUG("State #%i \"%s\" tics => %i")
                        << stateNum << state.gets("id") << state.geti("tics");
                }
            }
            else if(!var.compareWithoutCase("Next frame"))
            {
                int const value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= ded->states.size())
                    {
                        LOG_WARNING("DeHackEd Frame #%i out of range") << value;
                    }
                    else
                    {
                        int const nextStateIdx = value;
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
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value > ded->sprites.size())
                    {
                        LOG_WARNING("DeHackEd Sprite #%i out of range") << value;
                    }
                    else
                    {
                        int const spriteIdx = value;
                        ded_sprid_t const &sprite = ded->sprites[spriteIdx];
                        state.def().set("sprite", sprite.id);
                        LOG_DEBUG("State #%i \"%s\" sprite => \"%s\" (#%i)")
                                << stateNum << state.gets("id") << state.gets("sprite")
                                << spriteIdx;
                    }
                }
            }
            else if(!var.compareWithoutCase("Sprite subnumber"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    int const FF_FULLBRIGHT = 0x8000;

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
            else if(var.beginsWith("Unknown ", String::CaseInsensitive))
            {
                int const miscIdx = var.substr(8).toInt(0, 10, String::AllowSuffix);
                int const value   = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(miscIdx < 0 || miscIdx >= NUM_STATE_MISC)
                    {
                        LOG_WARNING("DeHackEd Unknown-value '%s' unknown")
                                << var.mid(8);
                    }
                    else
                    {
                        state.setMisc(miscIdx, value);
                        LOG_DEBUG("State #%i \"%s\" misc:%i => %i")
                                << stateNum << state.gets("id") << miscIdx << value;
                    }
                }
            }
            else if(var.beginsWith("Args", String::CaseInsensitive)) // Eternity
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
        int const sprNum = ded->sprites.indexOf(sprite);
        LOG_AS("parseSprite");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Offset"))
            {
                int const value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    // Calculate offset from beginning of sprite names.
                    int offset = -1;
                    if(value > 0)
                    {
                        // From DeHackEd source.
                        DENG2_ASSERT(doomVersion >= 0 && doomVersion < 5);
                        static int const spriteNameTableOffset[] = { 129044, 129044, 129044, 129284, 129380 };
                        offset = (value - spriteNameTableOffset[doomVersion] - 22044) / 8;
                    }

                    if(offset < 0 || offset >= ded->sprites.size())
                    {
                        LOG_WARNING("DeHackEd Sprite offset #%i out of range") << value;
                    }
                    else
                    {
                        ded_sprid_t const &origSprite = origSpriteNames[offset];
                        qstrncpy(sprite->id, origSprite.id, DED_STRINGID_LEN + 1);
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
        int const soundNum = ded->sounds.indexOf(sound);

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
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->group = value;
                    LOG_DEBUG("Sound #%i \"%s\" group => %i")
                            << soundNum << sound->id << sound->group;
                }
            }
            else if(!var.compareWithoutCase("Value"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
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
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkPitch = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkPitch => %i")
                            << soundNum << sound->id << sound->linkPitch;
                }
            }
            else if(!var.compareWithoutCase("Zero 3"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
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
                int const lumpNum = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    LumpIndex const &lumpIndex = *reinterpret_cast<LumpIndex const *>(F_LumpIndex());
                    int const numLumps = lumpIndex.size();
                    if(lumpNum < 0 || lumpNum >= numLumps)
                    {
                        LOG_WARNING("DeHackEd Neg. One 2 #%i out of range") << lumpNum;
                    }
                    else
                    {
                        qstrncpy(sound->lumpName, lumpIndex[lumpNum].name().toUtf8().constData(), DED_STRINGID_LEN + 1);
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
        static char const *ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
        char const *theAmmo = ammostr[ammoNum];
        LOG_AS("parseAmmo");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Max ammo"))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Player|Max ammo|%1").arg(theAmmo), QString::number(value));
            }
            else if(!var.compareWithoutCase("Per ammo"))
            {
                int per = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Player|Clip ammo|%1").arg(theAmmo), QString::number(per));
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

            if(var.endsWith(" frame", String::CaseInsensitive))
            {
                String const dehStateName = var.left(var.size() - 6);
                int const value           = expr.toInt(0, 0, String::AllowSuffix);

                WeaponStateMapping const *weapon;
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
                            DENG2_ASSERT(weapon->id >= 0 && weapon->id < ded->states.size());

                            Record const &state = ded->states[value];
                            createValueDef(String("Weapon Info|%1|%2").arg(weapNum).arg(weapon->name),
                                           state.gets("id"));
                        }
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo type"))
            {
                String const ammotypes[] = { "clip", "shell", "cell", "misl", "-", "noammo" };
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= 6)
                    {
                        LOG_WARNING("DeHackEd Ammo Type %i unknown") << value;
                    }
                    else
                    {
                        createValueDef(String("Weapon Info|%1|Type").arg(weapNum), ammotypes[value]);
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo per shot")) // Eternity
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Weapon Info|%1|Per shot").arg(weapNum), QString::number(value));
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
        int const stateNum = state.geti(defn::Definition::VAR_ORDER);

        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Codep Frame"))
            {
                int const actionIdx = expr.toInt(0, 0, String::AllowSuffix);
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

            ValueMapping const *mapping;
            if(parseMiscValue(var, &mapping))
            {
                int const value = expr.toInt(0, 10, String::AllowSuffix);
                createValueDef(mapping->valuePath, QString::number(value));
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
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                if(line.beginsWith("par", String::CaseInsensitive))
                {
                    String const argStr = line.substr(3).leftStrip();
                    if(argStr.isEmpty())
                    {
                        throw SyntaxError("parseParsBex", String("Expected format expression on line #%1")
                                                              .arg(currentLineNumber));
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
                    int const maxArgs = 3;
                    QStringList args = splitMax(argStr, ' ', maxArgs);

                    // If the third argument is a comment remove it.
                    if(args.size() == 3)
                    {
                        if(String(args.at(2)).beginsWith('#'))
                            args.removeAt(2);
                    }

                    if(args.size() < 2)
                    {
                        throw SyntaxError("parseParsBex", String("Invalid format string \"%1\" on line #%2")
                                                              .arg(argStr).arg(currentLineNumber));
                    }

                    // Parse values from the arguments.
                    int arg = 0;
                    int episode   = (args.size() > 2? args.at(arg++).toInt(0, 10) : 0);
                    int map       = args.at(arg++).toInt(0, 10);
                    float parTime = float(String(args.at(arg++)).toInt(0, 10, String::AllowSuffix));

                    // Apply.
                    de::Uri const uri = composeMapUri(episode, map);
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
            catch(SyntaxError const &er)
            {
                LOG_WARNING("%s") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
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
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                String var, expr;
                parseAssignmentStatement(line, var, expr);
                if(!patchSoundLumpNames(var, expr))
                {
                    LOG_WARNING("Failed to locate sound \"%s\" for patching") << var;
                }
            }
            catch(SyntaxError const &er)
            {
                LOG_WARNING("%s") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseMusic() // Eternity
    {
        LOG_AS("parseMusic");
        // BEX doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                String var, expr;
                parseAssignmentStatement(line, var, expr);
                if(!patchMusicLumpNames(var, expr))
                {
                    LOG_WARNING("Failed to locate music \"%s\" for patching") << var;
                }
            }
            catch(SyntaxError const &er)
            {
                LOG_WARNING("%s") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseCodePointers() // BEX
    {
        LOG_AS("parseCodePointers");
        // BEX doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.beginsWith("Frame ", String::CaseInsensitive))
            {
                int const stateNum = var.substr(6).toInt(0, 0, String::AllowSuffix);
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
                    if(!action.beginsWith("A_", String::CaseInsensitive))
                        action.prepend("A_");
                    action.truncate(32);

                    // Is this a known action?
                    if(!action.compareWithoutCase("A_NULL"))
                    {
                        state.set("action", "NULL");
                        LOG_DEBUG("State #%i \"%s\" action => \"NULL\"")
                                << stateNum << state.gets("id");
                    }
                    else
                    {
                        if(Def_Get(DD_DEF_ACTION, action.toUtf8().constData(), nullptr))
                        {
                            state.set("action", action);
                            LOG_DEBUG("State #%i \"%s\" action => \"%s\"")
                                    << stateNum << state.gets("id") << state.gets("action");
                        }
                        else
                        {
                            LOG_WARNING("DeHackEd Action '%s' unknown")
                                    << action.mid(2);
                        }
                    }
                }
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseText(int const oldSize, int const newSize)
    {
        LOG_AS("parseText");

        String const oldStr = readTextBlob(oldSize);
        String const newStr = readTextBlob(newSize);

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

    static void replaceTextValue(String const &id, String newValue)
    {
        if(id.isEmpty()) return;

        int textIdx = ded->getTextNum(id.toUtf8().constData());
        if(textIdx < 0) return;

        // We must escape new lines.
        newValue.replace("\n", "\\n");

        // Replace this text.
        ded->text[textIdx].setText(newValue.toUtf8().constData());
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
        for(;; readLine())
        {
            if(!multiline)
            {
                if(line.trimmed().isEmpty()) break;

                // Skip comment lines.
                if(line.at(0) == '#') continue;

                // Determine the split (or 'pivot') position.
                int assign = line.indexOf('=');
                if(assign < 0)
                {
                    throw SyntaxError("parseStrings", String("Expected assignment statement but encountered \"%1\" on line #%2")
                                                        .arg(line).arg(currentLineNumber));
                }

                textId = line.substr(0, assign).rightStrip();

                // Nothing before '=' ?
                if(textId.isEmpty())
                {
                    throw SyntaxError("parseStrings", String("Expected keyword before '=' on line #%1")
                                                        .arg(currentLineNumber));
                }

                newValue = line.substr(assign + 1).leftStrip();
            }
            else
            {
                newValue += line.leftStrip();
            }

            // Concatenate another multi-line replacement?
            if(newValue.endsWith('\\'))
            {
                newValue.truncate(newValue.length() - 1);
                multiline = true;
                continue;
            }

            replaceTextValue(textId, newValue);
            multiline = false;
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void createValueDef(String const &path, String const &value)
    {
        // An existing value?
        ded_value_t *def;
        int idx = valueDefForPath(path, &def);
        if(idx < 0)
        {
            // Not found - create a new Value.
            def = ded->values.append();
            def->id   = M_StrDup(path.toUtf8());
            def->text = 0;

            idx = ded->values.indexOf(def);
        }

        def->text = static_cast<char *>(M_Realloc(def->text, value.length() + 1));
        qstrcpy(def->text, value.toUtf8());

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
                strcpy(defs->sprites[i].id, newName.toLatin1());
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

//        /// @todo Presently disabled because the engine can't handle remapping.
//        DENG2_UNUSED(newName);
//        LOG_WARNING("DeHackEd sprite name table remapping is not supported");
//        // Pretend success.

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

    bool patchFinaleBackgroundNames(String const &origName, String const &newName)
    {
        FinaleBackgroundMapping const *mapping;
        if(findFinaleBackgroundMappingForText(origName, &mapping) < 0) return false;
        createValueDef(mapping->mnemonic, newName);
        return true;
    }

    bool patchMusicLumpNames(String const &origName, String const &newName)
    {
        // Only music lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findMusicLumpNameInMap(origName) < 0) return false;

        String origNamePref = String("D_%1").arg(origName);
        String newNamePref = String("D_%1").arg(newName);

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

    bool patchSoundLumpNames(String const &origName, String const &newName)
    {
        // Only sound lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findSoundLumpNameInMap(origName) < 0) return false;

        Block origNamePrefUtf8 = String("DS%1").arg(origName).toUtf8();
        Block newNamePrefUtf8  = String("DS%1").arg(newName ).toUtf8();

        // Update ALL sounds using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->sounds.size(); ++i)
        {
            ded_sound_t &sound = ded->sounds[i];
            if(qstricmp(sound.lumpName, origNamePrefUtf8.constData())) continue;

            qstrncpy(sound.lumpName, newNamePrefUtf8.constData(), 9);
            numPatched++;

            LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                    << i << sound.id << sound.lumpName;
        }
        return (numPatched > 0);
    }

    bool patchText(String const &origStr, String const &newStr)
    {
        TextMapping const *textMapping;

        // Which text are we replacing?
        if(textMappingForBlob(origStr, &textMapping) < 0) return false;

        // Is replacement disallowed/not-supported?
        if(textMapping->name.isEmpty()) return true; // Pretend success.

        int textIdx = ded->getTextNum(textMapping->name.toUtf8().constData());
        if(textIdx < 0) return false;

        // We must escape new lines.
        String newStrCopy = newStr;
        Block newStrUtf8 = newStrCopy.replace("\n", "\\n").toUtf8();

        // Replace this text.
        ded->text[textIdx].setText(newStrUtf8.constData());

        LOG_DEBUG("Text #%i \"%s\" is now:\n%s")
                << textIdx << textMapping->name << newStrUtf8.constData();
        return true;
    }
};

void readDehPatch(Block const &patch, bool patchIsCustom, DehReaderFlags flags)
{
    try
    {
        DehReader(patch, patchIsCustom, flags).parse();
    }
    catch(Error const &er)
    {
        LOG_WARNING(er.asText() + ".");
    }
}
