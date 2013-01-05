/**
 * @file dehreader.cpp
 * DeHackEd patch reader plugin for Doomsday Engine. @ingroup dehread
 *
 * @todo Presently there are a number of unsupported features, they should not
 *       be ignored. (Most if not all features should be supported.)
 *
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "dehread.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QStringList>

#include <de/App>
#include <de/Block>
#include <de/Error>
#include <de/Log>
#include <de/String>
#include <de/memory.h>

#include "dehreader.h"
#include "dehreader_util.h"
#include "info.h"

using namespace de;

#if 0
/**
 * Logging is routed through DehReader's special case handler.
 */
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_VERBOSE
#undef LOG_MSG
#undef LOG_INFO
#undef LOG_WARNING
#undef LOG_ERROR
#undef LOG_CRITICAL
#define LOG_TRACE(str)      DehReader::log(de::Log::TRACE, str)
#define LOG_DEBUG(str)      DehReader::log(de::Log::DEBUG, str)
#define LOG_VERBOSE(str)    DehReader::log(de::Log::VERBOSE, str)
#define LOG_MSG(str)        DehReader::log(str)
#define LOG_INFO(str)       DehReader::log(de::Log::INFO, str)
#define LOG_WARNING(str)    DehReader::log(de::Log::WARNING, str)
#define LOG_ERROR(str)      DehReader::log(de::Log::ERROR, str)
#define LOG_CRITICAL(str)   DehReader::log(de::Log::CRITICAL, str)
#endif

static int stackDepth;
static const int maxIncludeDepth = MAX_OF(0, DEHREADER_INCLUDE_DEPTH_MAX);

/// Mask containing only those reader flags which should be passed from the current
/// parser to any child parsers for file include statements.
static const DehReaderFlag DehReaderFlagsIncludeMask = IgnoreEOF;

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

    const Block& patch;
    int pos;
    int currentLineNumber;

    DehReaderFlags flags;

    int patchVersion;
    int doomVersion;

    String line; ///< Current line.

public:
    DehReader(const Block& _patch, DehReaderFlags _flags = 0)
        : patch(_patch), pos(0), currentLineNumber(0),
          flags(_flags),
          // Initialize as unknown Patch & Doom version.
          patchVersion(-1), doomVersion(-1),
          line("")
    {
        stackDepth++;
    }

    ~DehReader()
    {
        stackDepth--;
    }

#if 0
    LogEntry& log(Log::LogLevel level, const String& format)
    {
        return LOG().enter(level, format);
    }
#endif

    /**
     * Doom version numbers in the patch use the orignal game versions,
     * "16" => Doom v1.6, "19" => Doom v1.9, etc...
     */
    static inline bool normalizeDoomVersion(int& ver)
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
        char ch = currentChar().toAscii();
        switch(ch)
        {
        case '\0':
            if(size_t(pos) != patch.size() - 1)
            {
                LOG_WARNING("Unexpected EOF encountered on line #%i, ignoring.") << currentLineNumber;
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
        return QChar::fromAscii(patch.at(pos));
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
            line = String::fromAscii(rawLine);
            if(currentChar() == '\n') advance();
            return;
        }
        throw EndOfFile(String("EOF on line #%1").arg(currentLineNumber));
    }

    /**
     * Keep reading lines until we find one that is something other than
     * whitespace or a whole-line comment.
     */
    const String& skipToNextLine()
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
        do  skipToNextLine();
        while(lineInCurrentSection());
    }

    void logPatchInfo()
    {
        // Log reader settings and patch version information.
        LOG_INFO("Patch version: %i Doom version: %i\nNoText: %b")
            << patchVersion << doomVersion << bool(flags & NoText);

        if(patchVersion != 6)
        {
            LOG_WARNING("Unknown patch version. Unexpected results may occur.") << patchVersion;
        }
    }

    void parse()
    {
        LOG_AS_STRING(stackDepth == 1? "DehReader" : QString("[%1]").arg(stackDepth - 1));

        skipToNextLine();

        // Attempt to parse the DeHackEd patch signature and version numbers.
        if(line.beginsWith("Patch File for DeHackEd v", Qt::CaseInsensitive))
        {
            skipToNextLine();
            parsePatchSignature();
        }
        else
        {
            LOG_WARNING("Patch is missing a signature, assuming BEX.");
            doomVersion  = 19;
            patchVersion = 6;
        }

        logPatchInfo();

        // Is this for a known Doom version?
        if(!normalizeDoomVersion(doomVersion))
        {
            LOG_WARNING("Unknown Doom version, assuming v1.9.");
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
                    if(line.beginsWith("include", Qt::CaseInsensitive)) // .bex
                    {
                        parseInclude(line.substr(7).leftStrip());
                        skipToNextSection();
                    }
                    else if(line.beginsWith("Thing", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(5).leftStrip();
                        int mobjType = 0;
                        const bool isKnownMobjType = parseMobjType(arg, &mobjType);
                        const bool ignore = !isKnownMobjType;

                        if(!isKnownMobjType)
                        {
                            LOG_WARNING("Thing '%s' out of range, ignoring. (Create more Thing defs.)") << arg;
                        }

                        skipToNextLine();
                        parseThing(ded->mobjs + mobjType, ignore);
                    }
                    else if(line.beginsWith("Frame", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(5).leftStrip();
                        int stateNum = 0;
                        const bool isKnownStateNum = parseStateNum(arg, &stateNum);
                        const bool ignore = !isKnownStateNum;

                        if(!isKnownStateNum)
                        {
                            LOG_WARNING("Frame '%s' out of range, ignoring. (Create more State defs.)") << arg;
                        }

                        skipToNextLine();
                        parseFrame(ded->states + stateNum, ignore);
                    }
                    else if(line.beginsWith("Pointer", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(7).leftStrip();
                        int stateNum = 0;
                        const bool isKnownStateNum = parseStateNumFromActionOffset(arg, &stateNum);
                        const bool ignore = !isKnownStateNum;

                        if(!isKnownStateNum)
                        {
                            LOG_WARNING("Pointer '%s' out of range, ignoring. (Create more State defs.)") << arg;
                        }

                        skipToNextLine();
                        parsePointer(ded->states + stateNum, ignore);
                    }
                    else if(line.beginsWith("Sprite", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(6).leftStrip();
                        int spriteNum = 0;
                        const bool isKnownSpriteNum = parseSpriteNum(arg, &spriteNum);
                        const bool ignore = !isKnownSpriteNum;

                        if(!isKnownSpriteNum)
                        {
                            LOG_WARNING("Sprite '%s' out of range, ignoring. (Create more Sprite defs.)") << arg;
                        }

                        skipToNextLine();
                        parseSprite(ded->sprites + spriteNum, ignore);
                    }
                    else if(line.beginsWith("Ammo", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(4).leftStrip();
                        int ammoNum = 0;
                        const bool isKnownAmmoNum = parseAmmoNum(arg, &ammoNum);
                        const bool ignore = !isKnownAmmoNum;

                        if(!isKnownAmmoNum)
                        {
                            LOG_WARNING("Ammo '%s' out of range, ignoring.") << arg;
                        }

                        skipToNextLine();
                        parseAmmo(ammoNum, ignore);
                    }
                    else if(line.beginsWith("Weapon", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(6).leftStrip();
                        int weaponNum = 0;
                        const bool isKnownWeaponNum = parseWeaponNum(arg, &weaponNum);
                        const bool ignore = !isKnownWeaponNum;

                        if(!isKnownWeaponNum)
                        {
                            LOG_WARNING("Weapon '%s' out of range, ignoring.") << arg;
                        }

                        skipToNextLine();
                        parseWeapon(weaponNum, ignore);
                    }
                    else if(line.beginsWith("Sound", Qt::CaseInsensitive))
                    {
                        const String arg = line.substr(5).leftStrip();
                        int soundNum = 0;
                        const bool isKnownSoundNum = parseSoundNum(arg, &soundNum);
                        const bool ignore = !isKnownSoundNum;

                        if(!isKnownSoundNum)
                        {
                            LOG_WARNING("Sound '%s' out of range, ignoring. (Create more Sound defs.)") << arg;
                        }

                        skipToNextLine();
                        parseSound(ded->sounds + soundNum, ignore);
                    }
                    else if(line.beginsWith("Text", Qt::CaseInsensitive))
                    {
                        String args = line.substr(4).leftStrip();
                        int firstArgEnd = args.indexOf(' ');
                        if(firstArgEnd < 0)
                        {
                            throw SyntaxError(String("Expected old text size on line #%1")
                                                  .arg(currentLineNumber));
                        }
                        bool isNumber;
                        const int oldSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(String("Expected old text size but encountered \"%1\" on line #%2")
                                                  .arg(args.substr(firstArgEnd)).arg(currentLineNumber));
                        }

                        args.remove(0, firstArgEnd + 1);

                        const int newSize = args.toInt(&isNumber, 10, String::AllowSuffix);
                        if(!isNumber)
                        {
                            throw SyntaxError(String("Expected new text size but encountered \"%1\" on line #%2")
                                                  .arg(args).arg(currentLineNumber));
                        }

                        parseText(oldSize, newSize);
                    }
                    else if(line.beginsWith("Misc", Qt::CaseInsensitive))
                    {
                        skipToNextLine();
                        parseMisc();
                    }
                    else if(line.beginsWith("Cheat", Qt::CaseInsensitive))
                    {
                        LOG_WARNING("[Cheat] patches are not supported.");
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[CODEPTR]", Qt::CaseInsensitive)) // .bex
                    {
                        skipToNextLine();
                        parsePointerBex();
                    }
                    else if(line.beginsWith("[PARS]", Qt::CaseInsensitive)) // .bex
                    {
                        skipToNextLine();
                        parseParsBex();
                    }
                    else if(line.beginsWith("[STRINGS]", Qt::CaseInsensitive)) // .bex
                    {
                        // Not yet supported.
                        //skipToNextLine();
                        parseStringsBex();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[HELPER]", Qt::CaseInsensitive)) // .bex
                    {
                        // Not yet supported (Helper Dogs from MBF).
                        //skipToNextLine();
                        parseHelperBex();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[SPRITES]", Qt::CaseInsensitive)) // .bex
                    {
                        // Not yet supported.
                        //skipToNextLine();
                        parseSpritesBex();
                        skipToNextSection();
                    }
                    else if(line.beginsWith("[SOUNDS]", Qt::CaseInsensitive)) // .bex
                    {
                        skipToNextLine();
                        parseSoundsBex();
                    }
                    else if(line.beginsWith("[MUSIC]", Qt::CaseInsensitive)) // .bex
                    {
                        skipToNextLine();
                        parseMusicBex();
                    }
                    else
                    {
                        // An unknown section.
                        throw UnknownSection(String("Expected section name but encountered \"%1\" on line #%2")
                                                 .arg(line).arg(currentLineNumber));
                    }
                }
                catch(const UnknownSection& er)
                {
                    LOG_WARNING("%s. Skipping section...") << er.asText();
                    skipToNextSection();
                }
            }
        }
        catch(const EndOfFile& /*er*/)
        {} // Ignore.
    }

    void parseAssignmentStatement(const String& line, String& var, String& expr)
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

    bool parseAmmoNum(const String& str, int* ammoNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(ammoNum) *ammoNum = result;
        return (result >= 0 && result < 4);
    }

    bool parseMobjType(const String& str, int* mobjType)
    {
        int result = str.toInt(0, 0, String::AllowSuffix) - 1; // Patch indices are 1-based.
        if(mobjType) *mobjType = result;
        return (result >= 0 && result < ded->count.mobjs.num);
    }

    bool parseSoundNum(const String& str, int* soundNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(soundNum) *soundNum = result;
        return (result >= 0 && result < ded->count.sounds.num);
    }

    bool parseSpriteNum(const String& str, int* spriteNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(spriteNum) *spriteNum = result;
        return (result >= 0 && result < NUMSPRITES);
    }

    bool parseStateNum(const String& str, int* stateNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(stateNum) *stateNum = result;
        return (result >= 0 && result < ded->count.states.num);
    }

    bool parseStateNumFromActionOffset(const String& str, int* stateNum)
    {
        int result = stateIndexForActionOffset(str.toInt(0, 0, String::AllowSuffix));
        if(stateNum) *stateNum = result;
        return (result >= 0 && result < ded->count.states.num);
    }

    bool parseWeaponNum(const String& str, int* weaponNum)
    {
        int result = str.toInt(0, 0, String::AllowSuffix);
        if(weaponNum) *weaponNum = result;
        return (weaponNum >= 0);
    }

    bool parseMobjTypeState(const QString& token, const StateMapping** state)
    {
        return findStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMobjTypeFlag(const QString& token, const FlagMapping** flag)
    {
        return findMobjTypeFlagMappingByDehLabel(token, flag) >= 0;
    }

    bool parseMobjTypeSound(const QString& token, const SoundMapping** sound)
    {
        return findSoundMappingByDehLabel(token, sound) >= 0;
    }

    bool parseWeaponState(const QString& token, const WeaponStateMapping** state)
    {
        return findWeaponStateMappingByDehLabel(token, state) >= 0;
    }

    bool parseMiscValue(const QString& token, const ValueMapping** value)
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
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parseInclude(QString arg)
    {
        if(flags & NoInclude)
        {
            LOG_AS("parseInclude");
            LOG_DEBUG("Skipping disabled Include directive.");
            return;
        }

        if(stackDepth > maxIncludeDepth)
        {
            LOG_AS("parseInclude");
            if(!maxIncludeDepth)
            {
                LOG_WARNING("Sorry, nested includes are not supported. Directive ignored.");
            }
            else
            {
                const char* includes = (maxIncludeDepth == 1? "include" : "includes");
                LOG_WARNING("Sorry, there can be at most %i nested %s. Directive ignored.")
                    << maxIncludeDepth << includes;
            }
        }
        else
        {
            DehReaderFlags includeFlags = flags & DehReaderFlagsIncludeMask;

            if(arg.startsWith("notext ", Qt::CaseInsensitive))
            {
                includeFlags |= NoText;
                arg.remove(0, 7);
            }

            if(!arg.isEmpty())
            {
                const NativePath filePath(arg);
                QFile file(filePath);
                if(!file.open(QFile::ReadOnly | QFile::Text))
                {
                    LOG_AS("parseInclude");
                    LOG_WARNING("Failed opening \"%s\" for read, aborting...") << filePath;
                }
                else
                {
                    /// @todo Do not use a local buffer.
                    Block deh = file.readAll();
                    deh.append(QChar(0));
                    file.close();

                    LOG_INFO("Including \"%s\"...") << F_PrettyPath(filePath.toUtf8().constData());

                    try
                    {
                        DehReader(deh, includeFlags).parse();
                    }
                    catch(const Error& er)
                    {
                        LOG_WARNING(er.asText() + ".");
                    }
                }
            }
            else
            {
                LOG_AS("parseInclude");
                LOG_WARNING("Include directive missing filename, ignoring.");
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
    int parseMobjTypeFlags(const QString& arg, int flagGroups[NUM_MOBJ_FLAGS])
    {
        DENG2_ASSERT(flagGroups);

        if(arg.isEmpty()) return 0; // Erm? No change...
        int changedGroups = 0;

        // Split the argument into discreet tokens and process each individually.
        /// @todo Re-implement with a left-to-right algorithm.
        QStringList tokens = arg.split(QRegExp("[,+| ]|\t|\f|\r"), QString::SkipEmptyParts);
        DENG2_FOR_EACH_CONST(QStringList, i, tokens)
        {
            const String& token = *i;
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

            // Flags can also be specified by name (a .bex extension).
            const FlagMapping* flag;
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

            LOG_WARNING("Unknown flag mnemonic '%s' on line #%i, ignoring.") << token << currentLineNumber;
        }

        return changedGroups;
    }

    void parseThing(ded_mobj_t* mobj, bool ignore = false)
    {
        const int mobjType = (mobj - ded->mobjs);
        bool hadHeight = false, checkHeight = false;

        LOG_AS("parseThing");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", Qt::CaseInsensitive))
            {
                const StateMapping* mapping;
                const String dehStateName = var.left(var.size() - 6);
                if(!parseMobjTypeState(dehStateName, &mapping))
                {
                    if(!ignore) LOG_WARNING("Unknown frame '%s' on line #%i, ignoring.") << dehStateName << currentLineNumber;
                }
                else
                {
                    const int value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->count.states.num)
                        {
                            LOG_WARNING("Frame #%i out of range, ignoring.") << value;
                        }
                        else
                        {
                            const int stateIdx = value;
                            const ded_state_t& state = ded->states[stateIdx];

                            DENG2_ASSERT(mapping->id >= 0 && mapping->id < STATENAMES_COUNT);
                            qstrncpy(mobj->states[mapping->id], state.id, DED_STRINGID_LEN + 1);

                            LOG_DEBUG("Type #%i \"%s\" state:%s => \"%s\" (#%i)")
                                << mobjType << mobj->id << mapping->name
                                << mobj->states[mapping->id] << stateIdx;
                        }
                    }
                }
            }
            else if(var.endsWith(" sound", Qt::CaseInsensitive))
            {
                const SoundMapping* mapping;
                const String dehSoundName = var.left(var.size() - 6);
                if(!parseMobjTypeSound(dehSoundName, &mapping))
                {
                    if(!ignore) LOG_WARNING("Unknown sound '%s' on line #%i, ignoring.") << dehSoundName << currentLineNumber;
                }
                else
                {
                    const int value = expr.toInt(0, 0, String::AllowSuffix);
                    if(!ignore)
                    {
                        if(value < 0 || value >= ded->count.sounds.num)
                        {
                            LOG_WARNING("Sound #%i out of range, ignoring.") << value;
                        }
                        else
                        {
                            const int soundsIdx = value;
                            void* soundAdr;
                            switch(mapping->id)
                            {
                            case SDN_PAIN:      soundAdr = &mobj->painSound;   break;
                            case SDN_DEATH:     soundAdr = &mobj->deathSound;  break;
                            case SDN_ACTIVE:    soundAdr = &mobj->activeSound; break;
                            case SDN_ATTACK:    soundAdr = &mobj->attackSound; break;
                            case SDN_SEE:       soundAdr = &mobj->seeSound;    break;
                            default:
                                throw Error("DehReader", String("Unknown soundname id %i").arg(mapping->id));
                            }

                            const ded_sound_t& sound = ded->sounds[soundsIdx];
                            qstrncpy((char*)soundAdr, sound.id, DED_STRINGID_LEN + 1);

                            LOG_DEBUG("Type #%i \"%s\" sound:%s => \"%s\" (#%i)")
                                << mobjType << mobj->id << mapping->name
                                << (char*)soundAdr << soundsIdx;
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

                        mobj->flags[k] = flags[k];
                        LOG_DEBUG("Type #%i \"%s\" flags:%i => %X (%i)")
                            << mobjType << mobj->id << k
                            << mobj->flags[k] << mobj->flags[k];
                    }

                    // Any special translation necessary?
                    if(changedFlagGroups & 0x1)
                    {
                        if(mobj->flags[0] & 0x100/*mf_spawnceiling*/)
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
                LOG_WARNING("Thing - \"Bits2\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Bits3")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("Thing - \"Bits3\" patches are not supported.");
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
                LOG_WARNING("Thing - \"Blood color\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("ID #"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->doomEdNum = value;
                    LOG_DEBUG("Type #%i \"%s\" doomEdNum => %i") << mobjType << mobj->id << mobj->doomEdNum;
                }
            }
            else if(!var.compareWithoutCase("Height"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->height = value / float(0x10000);
                    hadHeight = true;
                    LOG_DEBUG("Type #%i \"%s\" height => %f") << mobjType << mobj->id << mobj->height;
                }
            }
            else if(!var.compareWithoutCase("Hit points"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->spawnHealth = value;
                    LOG_DEBUG("Type #%i \"%s\" spawnHealth => %i") << mobjType << mobj->id << mobj->spawnHealth;
                }
            }
            else if(!var.compareWithoutCase("Mass"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->mass = value;
                    LOG_DEBUG("Type #%i \"%s\" mass => %i") << mobjType << mobj->id << mobj->mass;
                }
            }
            else if(!var.compareWithoutCase("Missile damage"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->damage = value;
                    LOG_DEBUG("Type #%i \"%s\" damage => %i") << mobjType << mobj->id << mobj->damage;
                }
            }
            else if(!var.compareWithoutCase("Pain chance"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->painChance = value;
                    LOG_DEBUG("Type #%i \"%s\" painChance => %i") << mobjType << mobj->id << mobj->painChance;
                }
            }
            else if(!var.compareWithoutCase("Reaction time"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->reactionTime = value;
                    LOG_DEBUG("Type #%i \"%s\" reactionTime => %i") << mobjType << mobj->id << mobj->reactionTime;
                }
            }
            else if(!var.compareWithoutCase("Speed"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    /// @todo Is this right??
                    mobj->speed = (abs(value) < 256 ? float(value) : FIX2FLT(value));
                    LOG_DEBUG("Type #%i \"%s\" speed => %f") << mobjType << mobj->id << mobj->speed;
                }
            }
            else if(!var.compareWithoutCase("Translucency")) // Eternity
            {
                //const int value = expr.toInt(0, 10, String::AllowSuffix);
                //const float opacity = MINMAX_OF(0, value, 65536) / 65536.f;
                /// @todo Support this extension.
                LOG_WARNING("Thing - \"Translucency\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Width"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    mobj->radius = value / float(0x10000);
                    LOG_DEBUG("Type #%i \"%s\" radius => %f") << mobjType << mobj->id << mobj->radius;
                }
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }

        /// @todo Does this still make sense given DED can change the values?
        if(checkHeight && !hadHeight)
        {
            mobj->height = originalHeightForMobjType(mobjType);
        }
    }

    void parseFrame(ded_state_t* state, bool ignore = false)
    {
        const int stateNum = (state - ded->states);
        LOG_AS("parseFrame");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Duration"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    state->tics = value;
                    LOG_DEBUG("State #%i \"%s\" tics => %i") << stateNum << state->id << state->tics;
                }
            }
            else if(!var.compareWithoutCase("Next frame"))
            {
                const int value = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= ded->count.states.num)
                    {
                        LOG_WARNING("Frame #%i out of range, ignoring.") << value;
                    }
                    else
                    {
                        const int nextStateIdx = value;
                        qstrncpy(state->nextState, ded->states[nextStateIdx].id, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("State #%i \"%s\" nextState => \"%s\" (#%i)")
                            << stateNum << state->id << state->nextState << nextStateIdx;
                    }
                }
            }
            else if(!var.compareWithoutCase("Particle event")) // Eternity
            {
                /// @todo Support this extension.
                LOG_WARNING("Frame - \"Particle event\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Sprite number"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value > ded->count.sprites.num)
                    {
                        LOG_WARNING("Sprite #%i out of range, ignoring.") << value;
                    }
                    else
                    {
                        const int spriteIdx = value;
                        const ded_sprid_t& sprite = ded->sprites[spriteIdx];
                        qstrncpy(state->sprite.id, sprite.id, DED_SPRITEID_LEN + 1);
                        LOG_DEBUG("State #%i \"%s\" sprite => \"%s\" (#%i)")
                            << stateNum << state->id << state->sprite.id << spriteIdx;
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
                    if(value & FF_FULLBRIGHT)   state->flags |=  STF_FULLBRIGHT;
                    else                        state->flags &= ~STF_FULLBRIGHT;
                    state->frame = value & ~FF_FULLBRIGHT;

                    LOG_DEBUG("State #%i \"%s\" frame => %i") << stateNum << state->id << state->frame;
                }
            }
            else if(var.startsWith("Unknown ", Qt::CaseInsensitive))
            {
                const int miscIdx = var.substr(8).toInt(0, 10, String::AllowSuffix);
                const int value   = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(miscIdx < 0 || miscIdx >= NUM_STATE_MISC)
                    {
                        LOG_WARNING("Unknown unknown-value '%s' on line #%i, ignoring.") << var.mid(8) << currentLineNumber;
                    }
                    else
                    {
                        state->misc[miscIdx] = value;
                        LOG_DEBUG("State #%i \"%s\" misc:%i => %i") << stateNum << state->id << miscIdx << value;
                    }
                }
            }
            else if(var.beginsWith("Args", Qt::CaseInsensitive)) // Eternity
            {
                LOG_WARNING("Frame - \"%s\" patches are not supported.") << var;
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parseSprite(ded_sprid_t* sprite, bool ignore = false)
    {
        const int sprNum = sprite - ded->sprites;
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
                        DENG2_ASSERT(doomVersion >= 0 && doomVersion < 5);
                        static const int spriteNameTableOffset[] = { 129044, 129044, 129044, 129284, 129380 };
                        offset = (value - spriteNameTableOffset[doomVersion] - 22044) / 8;
                    }

                    if(offset < 0 || offset >= ded->count.sprites.num)
                    {
                        LOG_WARNING("Sprite offset #%i out of range, ignoring.") << value;
                    }
                    else
                    {
                        const ded_sprid_t& origSprite = origSpriteNames[offset];
                        qstrncpy(sprite->id, origSprite.id, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("Sprite #%i id => \"%s\" (#%i)") << sprNum << sprite->id << offset;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unexpected symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parseSound(ded_sound_t* sound, bool ignore = false)
    {
        const int soundIdx = sound - ded->sounds;
        LOG_AS("parseSound");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Offset")) // sound->id
            {
                LOG_WARNING("Sound - \"Offset\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Zero/One"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->group = value;
                    LOG_DEBUG("Sound #%i \"%s\" group => %i") << soundIdx << sound->id << sound->group;
                }
            }
            else if(!var.compareWithoutCase("Value"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->priority = value;
                    LOG_DEBUG("Sound #%i \"%s\" priority => %i") << soundIdx << sound->id << sound->priority;
                }
            }
            else if(!var.compareWithoutCase("Zero 1")) // sound->link
            {
                LOG_WARNING("Sound - \"Zero 1\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Zero 2"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkPitch = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkPitch => %i") << soundIdx << sound->id << sound->linkPitch;
                }
            }
            else if(!var.compareWithoutCase("Zero 3"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    sound->linkVolume = value;
                    LOG_DEBUG("Sound #%i \"%s\" linkVolume => %i") << soundIdx << sound->id << sound->linkVolume;
                }
            }
            else if(!var.compareWithoutCase("Zero 4")) // ??
            {
                LOG_WARNING("Sound - \"Zero 4\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Neg. One 1")) // ??
            {
                LOG_WARNING("Sound - \"Neg. One 1\" patches are not supported.");
            }
            else if(!var.compareWithoutCase("Neg. One 2"))
            {
                const int lumpNum = expr.toInt(0, 0, String::AllowSuffix);
                if(!ignore)
                {
                    const int numLumps = *reinterpret_cast<int*>(DD_GetVariable(DD_NUMLUMPS));
                    if(lumpNum < 0 || lumpNum >= numLumps)
                    {
                        LOG_WARNING("Neg. One 2 #%i out of range, ignoring.") << lumpNum;
                    }
                    else
                    {
                        qstrncpy(sound->lumpName, Str_Text(W_LumpName(lumpNum)), DED_STRINGID_LEN + 1);
                        LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                            << soundIdx << sound->id << sound->lumpName;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parseAmmo(const int ammoNum, bool ignore = false)
    {
        static const char* ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
        const char* theAmmo = ammostr[ammoNum];
        LOG_AS("parseAmmo");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(!var.compareWithoutCase("Max ammo"))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Player|Max ammo|%1").arg(theAmmo), QString::number(value));
            }
            else if(!var.compareWithoutCase("Per ammo"))
            {
                int per = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Player|Clip ammo|%1").arg(theAmmo), QString::number(per));
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parseWeapon(const int weapNum, bool ignore = false)
    {
        LOG_AS("parseWeapon");
        for(; lineInCurrentSection(); skipToNextLine())
        {
            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.endsWith(" frame", Qt::CaseInsensitive))
            {
                const String dehStateName = var.left(var.size() - 6);
                const int value = expr.toInt(0, 0, String::AllowSuffix);
                const WeaponStateMapping* weapon;
                if(!parseWeaponState(dehStateName, &weapon))
                {
                    if(!ignore) LOG_WARNING("Unknown frame '%s' on line #%i, ignoring.") << dehStateName << currentLineNumber;
                }
                else
                {
                    if(!ignore)
                    {
                        if(value < 0 || value > ded->count.states.num)
                        {
                            LOG_WARNING("Frame #%i out of range, ignoring.") << value;
                        }
                        else
                        {
                            DENG2_ASSERT(weapon->id >= 0 && weapon->id < ded->count.states.num);

                            const ded_state_t& state = ded->states[value];
                            createValueDef(String("Weapon Info|%1|%2").arg(weapNum).arg(weapon->name),
                                           QString::fromUtf8(state.id));
                        }
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo type"))
            {
                const String ammotypes[] = { "clip", "shell", "cell", "misl", "-", "noammo" };
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore)
                {
                    if(value < 0 || value >= 6)
                    {
                        LOG_WARNING("Unknown ammotype %i on line #%i, ignoring.") << value << currentLineNumber;
                    }
                    else
                    {
                        createValueDef(String("Weapon Info|%1|Type").arg(weapNum), ammotypes[value]);
                    }
                }
            }
            else if(!var.compareWithoutCase("Ammo per shot")) // Eternity
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                if(!ignore) createValueDef(String("Weapon Info|%1|Per shot").arg(weapNum), QString::number(value));
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parsePointer(ded_state_t* state, bool ignore)
    {
        const int stateIdx = state - ded->states;
        LOG_AS("parsePointer");
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
                        LOG_WARNING("Codep frame #%i out of range, ignoring.") << actionIdx;
                    }
                    else
                    {
                        const ded_funcid_t& newAction = origActionNames[actionIdx];
                        qstrncpy(state->action, newAction, DED_STRINGID_LEN + 1);
                        LOG_DEBUG("State #%i \"%s\" action => \"%s\"")
                            << stateIdx << state->id << state->action;
                    }
                }
            }
            else
            {
                LOG_WARNING("Unknown symbol \"%s\" encountered on line #%i, ignoring.") << var << currentLineNumber;
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

            const ValueMapping* mapping;
            if(parseMiscValue(var, &mapping))
            {
                const int value = expr.toInt(0, 10, String::AllowSuffix);
                createValueDef(mapping->valuePath, QString::number(value));
            }
            else
            {
                LOG_WARNING("Unknown value \"%s\" on line #%i, ignoring.") << var << currentLineNumber;
            }
        }
    }

    void parseParsBex()
    {
        LOG_AS("parseParsBex");
        // .bex doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            try
            {
                if(line.beginsWith("par", Qt::CaseInsensitive))
                {
                    const String argStr = line.substr(3).leftStrip();
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
                    const int maxArgs = 3;
                    QStringList args = splitMax(argStr, ' ', maxArgs);
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
                    uri_s* uri    = composeMapUri(episode, map);
                    AutoStr* path = Uri_ToString(uri);

                    ded_mapinfo_t* def;
                    int idx = mapInfoDefForUri(*uri, &def);
                    if(idx >= 0)
                    {
                        def->parTime = parTime;
                        LOG_DEBUG("MapInfo #%i \"%s\" parTime => %d") << idx << Str_Text(path) << def->parTime;
                    }
                    else
                    {
                        LOG_WARNING("Failed locating MapInfo for \"%s\" (episode:%i, map:%i), ignoring.")
                            << Str_Text(path) << episode << map;
                    }
                    Uri_Delete(uri);
                }
            }
            catch(const SyntaxError& er)
            {
                LOG_WARNING("%s, ignoring.") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseHelperBex()
    {
        LOG_AS("parseHelperBex");
        LOG_WARNING("[HELPER] patches are not supported.");
    }

    void parseSpritesBex()
    {
        LOG_AS("parseSpritesBex");
        LOG_WARNING("[SPRITES] patches are not supported.");
    }

    void parseSoundsBex()
    {
        LOG_AS("parseSoundsBex");
        // .bex doesn't follow the same rules as .deh
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
                    LOG_WARNING("Failed to locate sound \"%s\" for patching.") << var;
                }
            }
            catch(const SyntaxError& er)
            {
                LOG_WARNING("%s, ignoring.") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parseMusicBex()
    {
        LOG_AS("parseMusicBex");
        // .bex doesn't follow the same rules as .deh
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
                    LOG_WARNING("Failed to locate music \"%s\" for patching.") << var;
                }
            }
            catch(const SyntaxError& er)
            {
                LOG_WARNING("%s, ignoring.") << er.asText();
            }
        }

        if(line.trimmed().isEmpty())
        {
            skipToNextSection();
        }
    }

    void parsePointerBex()
    {
        LOG_AS("parsePointerBex");
        // .bex doesn't follow the same rules as .deh
        for(; !line.trimmed().isEmpty(); readLine())
        {
            // Skip comment lines.
            if(line.at(0) == '#') continue;

            String var, expr;
            parseAssignmentStatement(line, var, expr);

            if(var.startsWith("Frame ", Qt::CaseInsensitive))
            {
                const int stateNum = var.substr(6).toInt(0, 0, String::AllowSuffix);
                if(stateNum < 0 || stateNum >= ded->count.states.num)
                {
                    LOG_WARNING("Frame #%d out of range, ignoring. (Create more State defs!)") << stateNum;
                }
                else
                {
                    ded_state_t& state = ded->states[stateNum];

                    // Compose the action name.
                    String action = expr.rightStrip();
                    if(!action.startsWith("A_", Qt::CaseInsensitive))
                        action.prepend("A_");
                    action.truncate(32);

                    // Is this a known action?
                    if(!action.compareWithoutCase("A_NULL"))
                    {
                        qstrncpy(state.action, "NULL", DED_STRINGID_LEN+1);
                        LOG_DEBUG("State #%i \"%s\" action => \"NULL\"") << stateNum << state.id;
                    }
                    else
                    {
                        Block actionUtf8 = action.toUtf8();
                        if(Def_Get(DD_DEF_ACTION, actionUtf8.constData(), 0) >= 0)
                        {
                            qstrncpy(state.action, actionUtf8.constData(), DED_STRINGID_LEN + 1);
                            LOG_DEBUG("State #%i \"%s\" action => \"%s\"") << stateNum << state.id << state.action;
                        }
                        else
                        {
                            LOG_WARNING("Unknown action '%s' on line #%i, ignoring.") << action.mid(2) << currentLineNumber;
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

    void parseText(const int oldSize, const int newSize)
    {
        LOG_AS("parseText");

        String oldStr = readTextBlob(oldSize);
        String newStr = readTextBlob(newSize);

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
            LOG_DEBUG("Skipping disabled Text patch.");
        }

        skipToNextLine();
    }

    void parseStringsBex()
    {
        LOG_AS("parseStringsBex");
        LOG_WARNING("[Strings] patches not supported.");
    }

    void createValueDef(const QString& path, const QString& value)
    {
        // An existing value?
        ded_value_t* def;
        int idx = valueDefForPath(path, &def);
        if(idx < 0)
        {
            // Not found - create a new Value.
            Block pathUtf8 = path.toUtf8();
            idx = _api_Def.DED_AddValue(ded, pathUtf8.constData());
            def = &ded->values[idx];
            def->text = 0;
        }

        // Must allocate memory using DD_Realloc.
        def->text = static_cast<char*>(DD_Realloc(def->text, value.length() + 1));
        Block valueUtf8 = value.toUtf8();
        qstrcpy(def->text, valueUtf8.constData());

        LOG_DEBUG("Value #%i \"%s\" => \"%s\"") << idx << path << def->text;
    }

    bool patchSpriteNames(const String& origName, const String& newName)
    {
        // Is this a sprite name?
        if(origName.length() != 4) return false;

        // Only sprites names in the original name map can be patched.
        /// @todo Why the restriction?
        int spriteIdx = findSpriteNameInMap(origName);
        if(spriteIdx < 0) return false;

        /// @todo Presently disabled because the engine can't handle remapping.
        DENG2_UNUSED(newName);
        LOG_WARNING("Sprite name table remapping is not supported.");
        return true; // Pretend success.

#if 0
        if(spriteIdx >= ded->count.sprites.num)
        {
            LOG_WARNING("Sprite name #%i out of range, ignoring.") << spriteIdx;
            return false;
        }

        ded_sprid_t& def = ded->sprites[spriteIdx];
        Block newNameUtf8 = newName.toUtf8();
        qstrncpy(def.id, newNameUtf8.constData(), DED_SPRITEID_LEN + 1);

        LOG_DEBUG("Sprite #%i id => \"%s\"") << spriteIdx << def.id;
        return true;
#endif
    }

    bool patchFinaleBackgroundNames(const String& origName, const String& newName)
    {
        const FinaleBackgroundMapping* mapping;
        if(findFinaleBackgroundMappingForText(origName, &mapping) < 0) return false;
        createValueDef(mapping->mnemonic, newName);
        return true;
    }

    bool patchMusicLumpNames(const String& origName, const String& newName)
    {
        // Only music lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findMusicLumpNameInMap(origName) < 0) return false;

        Block origNamePrefUtf8 = String("D_%1").arg(origName).toUtf8();
        Block newNamePrefUtf8  = String("D_%1").arg(newName ).toUtf8();

        // Update ALL songs using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->count.music.num; ++i)
        {
            ded_music_t& music = ded->music[i];
            if(qstricmp(music.lumpName, origNamePrefUtf8.constData())) continue;

            qstrncpy(music.lumpName, newNamePrefUtf8.constData(), 9);
            numPatched++;

            LOG_DEBUG("Music #%i \"%s\" lumpName => \"%s\"")
                << i << music.id << music.lumpName;
        }
        return (numPatched > 0);
    }

    bool patchSoundLumpNames(const String& origName, const String& newName)
    {
        // Only sound lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findSoundLumpNameInMap(origName) < 0) return false;

        Block origNamePrefUtf8 = String("DS%1").arg(origName).toUtf8();
        Block newNamePrefUtf8  = String("DS%1").arg(newName ).toUtf8();

        // Update ALL sounds using this lump name.
        int numPatched = 0;
        for(int i = 0; i < ded->count.sounds.num; ++i)
        {
            ded_sound_t& sound = ded->sounds[i];
            if(qstricmp(sound.lumpName, origNamePrefUtf8.constData())) continue;

            qstrncpy(sound.lumpName, newNamePrefUtf8.constData(), 9);
            numPatched++;

            LOG_DEBUG("Sound #%i \"%s\" lumpName => \"%s\"")
                << i << sound.id << sound.lumpName;
        }
        return (numPatched > 0);
    }

    bool patchText(const String& origStr, const String& newStr)
    {
        const TextMapping* textMapping;

        // Which text are we replacing?
        if(textMappingForBlob(origStr, &textMapping) < 0) return false;

        // Is replacement disallowed/not-supported?
        if(textMapping->name.isEmpty()) return true; // Pretend success.

        Block textNameUtf8 = textMapping->name.toUtf8();
        int textIdx = Def_Get(DD_DEF_TEXT, textNameUtf8.constData(), NULL);
        if(textIdx < 0) return false;

        // We must escape new lines.
        String newStrCopy = newStr;
        Block newStrUtf8 = newStrCopy.replace("\n", "\\n").toUtf8();

        // Replace this text.
        Def_Set(DD_DEF_TEXT, textIdx, 0, newStrUtf8.constData());

        LOG_DEBUG("Text #%i \"%s\" is now:\n%s")
            << textIdx << textMapping->name << newStrUtf8.constData();
        return true;
    }
};

void readDehPatch(const Block& patch, DehReaderFlags flags)
{
    try
    {
        DehReader(patch, flags).parse();
    }
    catch(const Error& er)
    {
        LOG_WARNING(er.asText() + ".");
    }
}
