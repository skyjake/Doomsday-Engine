/**
 * @file dehreader.cpp
 * DeHackEd patch reader plugin for Doomsday Engine. @ingroup dehread
 *
 * @todo Presently there are a number of unsupported features, they should not
 *       not be ignored. (Most if not all features should be supported.)
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

static int stackDepth;
static const int maxIncludeDepth = MAX_OF(0, DEHREADER_INCLUDE_DEPTH_MAX);

/**
 * Not exposed outside this source file; use readDehPatch() instead.
 */
class DehReader
{
    const Block& patch;
    const char* patchPt;

    DehReaderFlags flags;

    int patchVersion;
    int doomVersion;

    /**
     * Each text line in the source patch is split into (at most) two parts
     * depending on the number of discreet tokens on that line.
     * The left side contains the first token only.
     * The right side is either empty or contains the rest of the tokens.
     */
    String lineLeft;
    String lineRight;

    // Token parse buffer.
    bool com_eof;
    char com_token[8192];

public:
    DehReader(const Block& _patch, DehReaderFlags _flags = 0)
        : patch(_patch), patchPt(patch.constData()), flags(_flags),
          // Initialize as unknown Patch & Doom version.
          patchVersion(-1), doomVersion(-1),
          lineLeft(""), lineRight(""),
          com_eof(false)
    {
        std::memset(com_token, 0, sizeof(com_token));

        stackDepth++;
    }

    ~DehReader()
    {
        stackDepth--;
    }

    void error(const QString& message)
    {
        //throw de::Error("JSONParser", de::String("Error at position %1 (%2^%3): %4")
        //                .arg(pos).arg(source.mid(pos - 4, 4)).arg(source.mid(pos, 4)).arg(message));
        throw de::Error("DehReader", de::String("Error %1").arg(message));
    }

    LogEntry& log(Log::LogLevel level, const String& format)
    {
        return LOG().enter(level, format);
    }

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

    bool atEnd()
    {
        return (!patchPt || *patchPt == '\0');
    }

    void skipToBreakLine()
    {
        while(!atEnd() && *patchPt != '\n')
        { patchPt++; }
    }

    String readLine()
    {
        if(!atEnd())
        {
            const char* start = patchPt;
            skipToBreakLine();
            if(!atEnd())
            {
                String line = String::fromAscii(start, (patchPt - start));
                if(*patchPt == '\n')
                    *patchPt++;
                return line;
            }
        }
        return String();
    }

    int readAndSplitLine()
    {
        // Loop until we get a line with more than just whitespace or a whole-line comment.
        String line;
        do
        {
            line = readLine();
            // EOF?
            if(line.isNull()) return 0; // STOP PARSE

        } while(line.isEmpty() || line.at(0) == '#' || line.trimmed().isEmpty());

        /**
         * We are looking at a non-comment, non-zero-length line.
         */

        // Determine the split (or 'pivot') position.
        int assign = line.indexOf('=');

        QStringList parts = splitMax(line, assign != -1? '=' : ' ', 2);
        lineLeft = String(parts.at(0)).rightStrip();
        if(parts.size() == 2)
            lineRight = String(parts.at(1)).leftStrip();
        else
            lineRight.clear();

        // Basic grammar checking.
        if(assign != -1)
        {
            // Nothing before '=' ?
            if(lineLeft.isEmpty()) return 0; // STOP PARSE

            // Nothing after '=' ?
            if(lineRight.isEmpty()) return 0; // STOP PARSE

            return 1;
        }

        return 2;
    }

    int skipToNextSection(void)
    {
        int cont;
        while((cont = readAndSplitLine()) == 1) {}
        return cont;
    }

    void parse()
    {
        LOG_AS_STRING(stackDepth == 1? "DehReader" :
                      QString("[%1]").arg(stackDepth - 1));

        // Attempt to parse the DeHackEd patch signature and version numbers.
        int cont = 0;
        if(!parsePatchSignature(cont))
        {
            LOG_WARNING("Patch is missing a signature, assuming BEX.");

            doomVersion  = 19;
            patchVersion = 6;
            patchPt = patch; // Rewind reader.

            cont = skipToNextSection();
        }

        /// @todo fixme - dead code!
        if(doomVersion == -1 || patchVersion == -1)
        {
            error("This is not a DeHackEd patch!");
        }

        // Log patch version information.
        LOG_INFO("Patch version: %i Doom version: %i") << patchVersion << doomVersion;
        if(patchVersion != 6)
        {
            LOG_WARNING("Unknown patch version. Unexpected results may occur.") << patchVersion;
        }

        // Is this for a known Doom version?
        if(!normalizeDoomVersion(doomVersion))
        {
            LOG_WARNING("Unknown Doom version, assuming v1.9.");
            doomVersion = 3;
        }

        // Patches are subdivided into sections.
        do
        {
            if(cont == 2) // A new section begins.
            {
                cont = parseSection();
            }
            else if(cont == 1)
            {
                LOG_WARNING("Token '%s' encountered out of context, cannot continue.") << lineLeft;
                cont = 0;
            }
        } while(cont);
    }

    bool parsePatchSignature(int& cont)
    {
        if(qstrncmp(patch, "Patch File for DeHackEd v", 25)) return false;

        skipToBreakLine();

        while((cont = readAndSplitLine()) == 1)
        {
            if(!lineLeft.compareWithoutCase("Doom version"))
            {
                doomVersion = lineRight.toIntLeft();
            }
            else if(!lineLeft.compareWithoutCase("Patch format"))
            {
                patchVersion = lineRight.toIntLeft();
            }
        }
        return true;
    }

    int parseInclude(int /*elementIdx*/)
    {
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
            QString arg = lineRight;
            DehReaderFlags includeFlags = 0;

            if(arg.startsWith("notext ", Qt::CaseInsensitive))
            {
                includeFlags |= NoText;
                arg.remove(0, 7);
            }

            if(!arg.isEmpty())
            {
                const String filePath = String::fromNativePath(arg);
                QFile file(filePath);
                if(!file.open(QFile::ReadOnly | QFile::Text))
                {
                    LOG_AS("parseInclude");
                    LOG_WARNING("Failed opening \"%s\" for read, aborting...") << QDir::toNativeSeparators(filePath);
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
                    catch(const de::Error& er)
                    {
                        LOG().enter(de::Log::WARNING, er.asText());
                    }
                }
            }
            else
            {
                LOG_AS("parseInclude");
                LOG_WARNING("Include directive missing filename, ignoring.");
            }
        }

        return readAndSplitLine();
    }

    /// @note Some sections have their own grammar quirks!
    int parseSection()
    {
        // Parse the section header/identifier.
        const String& ident  = lineLeft;
        const int elementIdx = lineRight.toIntLeft();

        // These appear in .deh and .bex files:
        if(!ident.compareWithoutCase("Thing"))
        {
            return parseThing(elementIdx);
        }
        else if(!ident.compareWithoutCase("Frame"))
        {
            return parseFrame(elementIdx);
        }
        else if(!ident.compareWithoutCase("Pointer"))
        {
            return parsePointer(elementIdx);
        }
        else if(!ident.compareWithoutCase("Sprite"))
        {
            return parseSprite(elementIdx);
        }
        else if(!ident.compareWithoutCase("Ammo"))
        {
            return parseAmmo(elementIdx);
        }
        else if(!ident.compareWithoutCase("Misc"))
        {
            return parseMisc(elementIdx);
        }
        else if(!ident.compareWithoutCase("Weapon"))
        {
            return parseWeapon(elementIdx);
        }
        else if(!ident.compareWithoutCase("Sound"))
        {
            // Not yet supported.
            return parseSound(elementIdx);
        }
        else if(!ident.compareWithoutCase("Text"))
        {
            return parseText(elementIdx);
        }
        else if(!ident.compareWithoutCase("Cheat"))
        {
            // No intention of support.
            return parseCheat(elementIdx);
        }
        else if(!ident.compareWithoutCase("include")) // .bex
        {
            return parseInclude(elementIdx);
        }
        else if(!ident.compareWithoutCase("[STRINGS]")) // .bex
        {
            // Not yet supported.
            return parseStringsBex(elementIdx);
        }
        else if(!ident.compareWithoutCase("[PARS]")) // .bex
        {
            return parseParsBex(elementIdx);
        }
        else if(!ident.compareWithoutCase("[CODEPTR]")) // .bex
        {
            return parsePointerBex(elementIdx);
        }
        else if(!ident.compareWithoutCase("[HELPER]")) // .bex
        {
            // (Helper Dogs from MBF) Not yet supported.
            return parseHelperBex(elementIdx);
        }
        else if(!ident.compareWithoutCase("[SPRITES]")) // .bex
        {
            // Not yet supported.
            return parseSpritesBex(elementIdx);
        }
        else if(!ident.compareWithoutCase("[SOUNDS]")) // .bex
        {
            // Not yet supported.
            return parseSoundsBex(elementIdx);
        }
        else if(!ident.compareWithoutCase("[MUSIC]")) // .bex
        {
            // Not yet supported.
            return parseMusicBex(elementIdx);
        }
        else
        {
            // An unknown section - skip it.
            LOG_WARNING("Expected section name but encountered \"%s\", skipping...") << ident;
            return skipToNextSection();
        }
    }

    String readTextBlob(int size)
    {
        if(!size) return String(); // Return an empty string.

        String string;
        do
        {
            // Ignore carriage returns.
            if(*patchPt != '\r')
                string += *patchPt; ///@todo fixme implicit fromAscii conversion.
            else
                size++;

            patchPt++;
        } while(--size);

        return string.trimmed();
    }

    bool parseMobjTypeFlag(const QString& token, const FlagMapping** flag)
    {
        return findMobjTypeFlagMappingByDehLabel(token, flag) >= 0;
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
        DENG2_FOR_EACH(i, tokens, QStringList::const_iterator)
        {
            const String& token = *i;
            bool tokenIsNumber;

            int flagsValue = token.toIntLeft(&tokenIsNumber);
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

            LOG_WARNING("Unknown flag mnemonic '%s', ignoring.") << token;
        }

        return changedGroups;
    }

    int parseThing(int elementIndex)
    {
        LOG_AS("parseThing");

        const int mobjType = elementIndex - 1; // Patch indices are 1-based.
        bool ignore = false, hadHeight = false, checkHeight = false;

        ded_mobj_t* mobj = ded->mobjs + mobjType;
        if(mobjType < 0 || mobjType >= ded->count.mobjs.num)
        {
            LOG_WARNING("Thing #%i out of range, ignoring. (Create more Thing defs!)") << (mobjType + 1);
            ignore = true;
        }

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(lineLeft.endsWith(" frame", Qt::CaseInsensitive))
            {
                const StateMapping* mapping;
                const String dehStateName = lineLeft.left(lineLeft.size() - 6);
                if(findStateMappingByDehLabel(dehStateName, &mapping) < 0)
                {
                    if(!ignore) LOG_WARNING("Unknown frame '%s'', ignoring.") << dehStateName;
                }
                else
                {
                    const int value = lineRight.toIntLeft();
                    if(!ignore)
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
            else if(lineLeft.endsWith(" sound", Qt::CaseInsensitive))
            {
                const SoundMapping* mapping;
                const String dehSoundName = lineLeft.left(lineLeft.size() - 6);
                if(findSoundMappingByDehLabel(dehSoundName, &mapping) < 0)
                {
                    if(!ignore) LOG_WARNING("Unknown sound '%s'', ignoring.") << dehSoundName;
                }
                else
                {
                    const int value = lineRight.toIntLeft();
                    if(!ignore)
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
                            error(String("Unexpected soundname id %i.").arg(mapping->id));
                        }

                        const ded_sound_t& sound = ded->sounds[soundsIdx];
                        qstrncpy((char*)soundAdr, sound.id, DED_STRINGID_LEN + 1);

                        LOG_DEBUG("Type #%i \"%s\" sound:%s => \"%s\" (#%i)")
                            << mobjType << mobj->id << mapping->name
                            << (char*)soundAdr << soundsIdx;
                    }
                }
            }
            else if(!lineLeft.compareWithoutCase("Bits"))
            {
                int flags[NUM_MOBJ_FLAGS] = { 0, 0, 0 };
                int changedFlagGroups = parseMobjTypeFlags(lineRight, flags);
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
            else if(!lineLeft.compareWithoutCase("ID #"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->doomEdNum = value;
                    LOG_DEBUG("Type #%i \"%s\" doomEdNum => %i") << mobjType << mobj->id << mobj->doomEdNum;
                }
            }
            else if(!lineLeft.compareWithoutCase("Height"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->height = value / float(0x10000);
                    hadHeight = true;
                    LOG_DEBUG("Type #%i \"%s\" height => %f") << mobjType << mobj->id << mobj->height;
                }
            }
            else if(!lineLeft.compareWithoutCase("Hit points"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->spawnHealth = value;
                    LOG_DEBUG("Type #%i \"%s\" spawnHealth => %i") << mobjType << mobj->id << mobj->spawnHealth;
                }
            }
            else if(!lineLeft.compareWithoutCase("Mass"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->mass = value;
                    LOG_DEBUG("Type #%i \"%s\" mass => %i") << mobjType << mobj->id << mobj->mass;
                }
            }
            else if(!lineLeft.compareWithoutCase("Missile damage"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->damage = value;
                    LOG_DEBUG("Type #%i \"%s\" damage => %i") << mobjType << mobj->id << mobj->damage;
                }
            }
            else if(!lineLeft.compareWithoutCase("Pain chance"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->painChance = value;
                    LOG_DEBUG("Type #%i \"%s\" painChance => %i") << mobjType << mobj->id << mobj->painChance;
                }
            }
            else if(!lineLeft.compareWithoutCase("Reaction time"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->reactionTime = value;
                    LOG_DEBUG("Type #%i \"%s\" reactionTime => %i") << mobjType << mobj->id << mobj->reactionTime;
                }
            }
            else if(!lineLeft.compareWithoutCase("Speed"))
            {
                /// @todo Is this right??
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->speed = (abs(value) < 256 ? float(value) : FIX2FLT(value));
                    LOG_DEBUG("Type #%i \"%s\" speed => %f") << mobjType << mobj->id << mobj->speed;
                }
            }
            else if(!lineLeft.compareWithoutCase("Width"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    mobj->radius = value / float(0x10000);
                    LOG_DEBUG("Type #%i \"%s\" radius => %f") << mobjType << mobj->id << mobj->radius;
                }
            }
            else
            {
                LOG_WARNING("Unknown key \"%s\" encountered in #%i, ignoring.") << lineLeft << mobjType;
            }
        }

        /// @todo Does this still make sense given DED can change the values?
        if(checkHeight && !hadHeight)
        {
            mobj->height = originalHeightForMobjType(mobjType);
        }

        return cont;
    }

    int parseFrame(int stateNum)
    {
        LOG_AS("parseFrame");

        bool ignore = false;

        ded_state_t* state = ded->states + stateNum;
        if(stateNum < 0 || stateNum >= ded->count.states.num)
        {
            LOG_WARNING("Frame #%i out of range, ignoring. (Create more State defs!)") << stateNum;
            ignore = true;
        }

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(!lineLeft.compareWithoutCase("Duration"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
                {
                    state->tics = value;
                    LOG_DEBUG("State #%i \"%s\" tics => %i") << stateNum << state->id << state->tics;
                }
            }
            else if(!lineLeft.compareWithoutCase("Next frame"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
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
            else if(!lineLeft.compareWithoutCase("Sprite number"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore)
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
            else if(!lineLeft.compareWithoutCase("Sprite subnumber"))
            {
                const int value = lineRight.toIntLeft();
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
            else if(lineLeft.startsWith("Unknown ", Qt::CaseInsensitive))
            {
                const int miscIdx = lineLeft.substr(8).toIntLeft();
                const int value   = lineRight.toIntLeft();
                if(!ignore)
                if(miscIdx < 0 || miscIdx >= NUM_STATE_MISC)
                {
                    LOG_WARNING("Unknown unknown-value '%s', ignoring.") << lineLeft.mid(8);
                }
                else
                {
                    state->misc[miscIdx] = value;
                    LOG_DEBUG("State #%i \"%s\" misc:%i => %i") << stateNum << state->id << miscIdx << value;
                }
            }
            else
            {
                LOG_WARNING("Unknown key \"%s\" encountered in #%i, ignoring.") << lineLeft << stateNum;
            }
        }

        return cont;
    }

    int parseSprite(int sprNum)
    {
        LOG_AS("parseSprite");

        bool ignore = false;

        ded_sprid_t* sprite = ded->sprites + sprNum;
        if(sprNum < 0 || sprNum >= NUMSPRITES)
        {
            LOG_WARNING("Sprite #%d out of range, ignoring. (Create more Sprite defs!)") << sprNum;
            ignore = true;
        }

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(!lineLeft.compareWithoutCase("Offset"))
            {
                const int value = lineRight.toIntLeft();
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
                LOG_WARNING("Unknown key \"%s\" encountered in #%i, ignoring.") << lineLeft << sprNum;
            }
        }

        return cont;
    }

    int parseSound(int /*soundNum*/)
    {
        LOG_AS("parseSound");
        LOG_WARNING("[Sound] patches are not supported.");

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {}

        return cont;
    }

    int parseAmmo(int ammoNum)
    {
        LOG_AS("parseAmmo");

        const char* ammostr[4] = { "Clip", "Shell", "Cell", "Misl" };
        bool ignore = false;

        const char* theAmmo = ammostr[ammoNum];
        if(ammoNum < 0 || ammoNum >= 4)
        {
            LOG_WARNING("Ammo #%d out of range, ignoring.") << ammoNum;
            ignore = true;
        }

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(!lineLeft.compareWithoutCase("Max ammo"))
            {
                const int value = lineRight.toIntLeft();
                if(!ignore) createValueDef(String("Player|Max ammo|%1").arg(theAmmo), QString::number(value));
            }
            else if(!lineLeft.compareWithoutCase("Per ammo"))
            {
                int per = lineRight.toIntLeft();
                if(!ignore) createValueDef(String("Player|Clip ammo|%1").arg(theAmmo), QString::number(per));
            }
            else
            {
                LOG_WARNING("Unknown key \"%s\" encountered in #%i, ignoring.") << lineLeft << ammoNum;
            }
        }

        return cont;
    }

    int parseWeapon(int weapNum)
    {
        LOG_AS("parseWeapon");

        if(weapNum < 0)
        {
            LOG_WARNING("Weapon #%i out of range, ignoring.") << weapNum;
            return skipToNextSection();
        }

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(lineLeft.endsWith(" frame", Qt::CaseInsensitive))
            {
                const String dehStateName = lineLeft.left(lineLeft.size() - 6);
                const int value = lineRight.toIntLeft();
                const WeaponStateMapping* weapon;

                if(findWeaponStateMappingByDehLabel(dehStateName, &weapon) < 0)
                {
                    LOG_WARNING("Unknown frame '%s', ignoring.") << dehStateName;
                }
                else
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
            else if(!lineLeft.compareWithoutCase("Ammo type"))
            {
                const String ammotypes[] = { "clip", "shell", "cell", "misl", "-", "noammo" };
                const int value = lineRight.toIntLeft();
                if(value < 0 || value >= 6)
                {
                    LOG_WARNING("Unknown ammotype #%i, ignoring.") << value;
                }
                else
                {
                    createValueDef(String("Weapon Info|%1|Type").arg(weapNum), ammotypes[value]);
                }
            }
            else if(!lineLeft.compareWithoutCase("Ammo per shot"))
            {
                const int value = lineRight.toIntLeft();
                createValueDef(String("Weapon Info|%1|Per shot").arg(weapNum), QString::number(value));
            }
            else
            {
                LOG_WARNING("Unknown key \"%s\" encountered in #%i, ignoring.") << lineLeft << weapNum;
            }
        }
        return cont;
    }

    int parsePointer(int ptrNum)
    {
        LOG_AS("parsePointer");

        bool ignore = false;
        const int stateIdx = stateIndexForActionOffset(ptrNum);
        ded_state_t* state = ded->states + stateIdx;

        if(stateIdx < 0 || stateIdx >= ded->count.states.num)
        {
            LOG_WARNING("Pointer #%i out of range, ignoring.") << ptrNum;
            ignore = true;
        }

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(!lineLeft.compareWithoutCase("Codep Frame"))
            {
                const int actionIdx = lineRight.toIntLeft();
                if(!ignore)
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
            else
            {
                LOG_WARNING("Unknown key \"%s\" encountered in #%i, ignoring.") << lineLeft << ptrNum;
            }
        }
        return cont;
    }

    int parseCheat(int /*elementIdx*/)
    {
        LOG_AS("parseCheat");
        LOG_WARNING("[Cheat] patches are not supported.");
        return skipToNextSection();
    }

    int parseMisc(int /*elementIdx*/)
    {
        LOG_AS("parseMisc");
        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            const ValueMapping* mapping;
            if(findValueMappingForDehLabel(lineLeft, &mapping) >= 0)
            {
                int value = lineRight.toIntLeft();
                createValueDef(mapping->valuePath, QString::number(value));
            }
            else
            {
                LOG_WARNING("Unknown value \"%s = %s\", ignoring.") << lineLeft << lineRight;
            }
        }
        return cont;
    }

    int parseParsBex(int /*elementIdx*/)
    {
        LOG_AS("parseParsBex");

        int cont;
        while((cont = readAndSplitLine()))
        {
            // Argh! .bex doesn't follow the same rules as .deh
            if(cont == 1)
            {
                LOG_WARNING("Unknown key \"%s\", ignoring") << lineLeft;
                continue;
            }

            if(lineLeft.startsWith("par", Qt::CaseInsensitive))
            {
                if(lineRight.isEmpty())
                {
                    LOG_WARNING("Need data after par, ignoring.");
                    continue;
                }

                /**
                 * @attention Team TNT's original DEH parser would read the first one
                 * or two tokens then applied atoi() on the remainder of the line to
                 * obtain the last argument (i.e., par time).
                 *
                 * Here we emulate this behavior by splitting the line into at most
                 * three arguments and then apply atoi()-like readNumberAtStart() on
                 * the last.
                 */
                const int maxTokens = 3;
                QStringList args = splitMax(String(lineRight).leftStrip(), ' ', maxTokens);
                if(args.size() < 2)
                {
                    LOG_WARNING("Unknown format string \"%s\", ignoring.") << lineRight;
                    continue;
                }

                // Parse values from the arguments.
                int arg = 0;
                int episode   = (args.size() > 2? args.at(arg++).toInt() : 0);
                int map       = args.at(arg++).toInt();
                float parTime = float(String(args.at(arg++)).toIntLeft());

                // Apply.
                Uri* uri  = composeMapUri(episode, map);
                ddstring_t* path = Uri_ToString(uri);

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
                Str_Delete(path);
                Uri_Delete(uri);
            }
        }
        return cont;
    }

    int parseHelperBex(int /*elementIdx*/)
    {
        LOG_AS("parseHelperBex");
        LOG_WARNING("[HELPER] patches are not supported.");
        return skipToNextSection();
    }

    int parseSpritesBex(int /*elementIdx*/)
    {
        LOG_AS("parseSpritesBex");
        LOG_WARNING("[SPRITES] patches are not supported.");
        return skipToNextSection();
    }

    int parseSoundsBex(int /*elementIdx*/)
    {
        LOG_AS("parseSoundsBex");
        LOG_WARNING("[SOUNDS] patches are not supported.");
        return skipToNextSection();
    }

    int parseMusicBex(int /*elementIdx*/)
    {
        LOG_AS("parseMusicBex");
        LOG_WARNING("[MUSIC] patches are not supported.");
        return skipToNextSection();
    }

    int parsePointerBex(int /*elementIdx*/)
    {
        LOG_AS("parsePointerBex");

        int cont;
        while((cont = readAndSplitLine()) == 1)
        {
            if(lineLeft.startsWith("Frame ", Qt::CaseInsensitive))
            {
                const int stateNum = lineLeft.substr(6).toIntLeft();
                if(stateNum < 0 || stateNum >= ded->count.states.num)
                {
                    LOG_WARNING("Frame #%d out of range, ignoring. (Create more State defs!)") << stateNum;
                }
                else
                {
                    ded_state_t& state = ded->states[stateNum];

                    // Skip the assignment operator.
                    Block temp = lineRight.toAscii();
                    parseToken(temp.constData());

                    // Compose the action name.
                    String action = String::fromAscii(com_token);
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
                            LOG_WARNING("Unknown action '%s', ignoring.") << action.mid(2);
                        }
                    }
                }
            }
        }

        return cont;
    }

    int parseText(int oldSize)
    {
        LOG_AS("parseText");

        Block temp = lineRight.toAscii();
        const char* ch = parseToken(temp.constData()); // Skip old size, since we already have it
        if(!parseToken(ch))
        {
            LOG_WARNING("Missing new text size, ignoring.");
            return 2;
        }
        int newSize = String(com_token).toIntLeft();

        String oldStr = readTextBlob(oldSize);
        String newStr = readTextBlob(newSize);

        // Skip anything else on the line.
        skipToBreakLine();

        if(!(flags & NoText)) // Disabled?
        {
            // Try each type of "text" replacement in turn...
            bool found = false;
            if(patchSpriteNames(oldStr, newStr))    found = true;
            if(patchMusicLumpNames(oldStr, newStr)) found = true;
            if(patchText(oldStr, newStr))           found = true;

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

        // Fetch next identifier for main loop.
        return skipToNextSection();
    }

    int parseStringsBex(int /*elementIdx*/)
    {
        LOG_AS("parseStringsBex");
        LOG_WARNING("[Strings] patches not supported.");
        return skipToNextSection();
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
            idx = DED_AddValue(ded, pathUtf8.constData());
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

    bool patchMusicLumpNames(const String& origName, const String& newName)
    {
        // Only music lump names in the original name map can be patched.
        /// @todo Why the restriction?
        if(findMusicLumpNameInMap(origName) < 0) return false;

        Block origNamePrefUtf8 = String("D_%s").arg(origName).toUtf8();
        Block newNamePrefUtf8  = String("D_%s").arg(newName ).toUtf8();

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

    /**
     * Parse a token out of a string.
     * @note Derived from ZDoom's cmdlib.cpp COM_Parse()
     */
    const char* parseToken(const char* data)
    {
        com_token[0] = 0;
        if(!data) return NULL;

        // Skip whitespace.
        int c, len = 0;
      skipwhite:
        while((c = *data) <= ' ')
        {
            if(c == 0)
            {
                com_eof = true;
                return NULL; // end of file;
            }
            data++;
        }

        // Skip // comments.
        if(c == '/' && data[1] == '/')
        {
            while(*data && *data != '\n') { data++; }
            goto skipwhite;
        }

        // Handle quoted strings specially.
        if(c == '\"')
        {
            data++;

            while((c = *data++) != '\"')
            {
                com_token[len] = c;
                len++;
            }

            com_token[len] = 0;
            return data;
        }

        // Parse single characters.
        if(c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || /*[RH] */ c == '=')
        {
            com_token[len] = c;
            len++;
            com_token[len] = 0;
            return data + 1;
        }

        // Parse a regular word.
        do
        {
            com_token[len] = c;
            data++;
            len++;
            c = *data;

            if(c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == '=')
                break;
        } while(c > 32);

        com_token[len] = 0;
        return data;
    }
};

void readDehPatch(const Block& patch, DehReaderFlags flags)
{
    try
    {
        DehReader(patch, flags).parse();
    }
    catch(const de::Error& er)
    {
        LOG().enter(de::Log::WARNING, er.asText());
    }
}
