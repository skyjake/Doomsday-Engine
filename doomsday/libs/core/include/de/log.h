/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_LOG_H
#define LIBCORE_LOG_H

#include "de/guard.h"
#include "de/iserializable.h"
#include "de/list.h"
#include "de/lockable.h"
#include "de/string.h"
#include "de/time.h"

#include <vector>
#include <string>
#include <cstdlib>

/// Macro for accessing the local log of the current thread.
#define LOG()               de::Log::threadLog()

/// Macro for accessing the local log of the current thread and entering
/// a new log section.
#define LOG_AS(sectionName) \
    de::Log::Section __logSection = de::Log::Section(sectionName);

/**
 * Macro for accessing the local log of the current thread and entering
 * a new log section with a de::String variable based name.
 *
 * @param str  Anything out of which a de::String can be constructed.
 *
 * @note Log::Section doesn't own the strings passed in; we have to
 * ensure that the string exists in memory as long as the section (scope)
 * is valid.
 */
#define LOG_AS_STRING(str) \
    de::Block __logSectionUtf8 { de::String(str).toUtf8() }; \
    LOG_AS(reinterpret_cast<const char *>(__logSectionUtf8.constData()));

/*
 * Macros that make a new log entry with a particular set of audience/level bits:
 */

// High-verbosity log entries. Pre-check if log level is enabled, otherwise message
// is ignored.
#define LOG_PRECHECK_LEVEL(level, str, args) \
        { if (de::LogBuffer::appBufferExists() && de::LogBuffer::get().isEnabled(level)) \
          { LOG_AT_LEVEL(level, str) << args; } }

// End-user/game audience
#define LOG_AT_LEVEL(level, str)        de::LogEntryStager(level, str)
#define LOG_XVERBOSE(str, args)         LOG_PRECHECK_LEVEL(de::LogEntry::Generic | de::LogEntry::XVerbose, str, args)
#define LOG_VERBOSE(str)                LOG_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_MSG(str)                    LOG_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_INFO(str)                   LOG_AT_LEVEL(de::LogEntry::Note,     str) // backwards comp
#define LOG_NOTE(str)                   LOG_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_WARNING(str)                LOG_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_ERROR(str)                  LOG_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_CRITICAL(str)               LOG_AT_LEVEL(de::LogEntry::Critical, str)

// Native code developer audience (general domain)
#define LOGDEV_AT_LEVEL(level, str)     LOG_AT_LEVEL(de::LogEntry::Dev | (level), str)
#define LOGDEV_XVERBOSE(str, args)      LOG_PRECHECK_LEVEL(de::LogEntry::Dev | de::LogEntry::Generic | de::LogEntry::XVerbose, str, args)
#define LOG_TRACE(str, args)            LOGDEV_XVERBOSE(str, args) // backwards comp
#define LOGDEV_VERBOSE(str)             LOGDEV_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_DEBUG(str)                  LOGDEV_VERBOSE(str) // backwards comp
#define LOGDEV_MSG(str)                 LOGDEV_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_NOTE(str)                LOGDEV_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_WARNING(str)             LOGDEV_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_ERROR(str)               LOGDEV_AT_LEVEL(de::LogEntry::Error,    str)

#define LOG_WIP(str)                    LOG_AT_LEVEL(de::LogEntry::Privileged | de::LogEntry::Note, str)

// Custom combination of audiences
#define LOG_XVERBOSE_TO(audflags, str, args) \
                                        LOG_PRECHECK_LEVEL((audflags) | de::LogEntry::XVerbose, str, args)
#define LOG_VERBOSE_TO(audflags, str)   LOG_AT_LEVEL((audflags) | de::LogEntry::Verbose,  str)
#define LOG_MSG_TO(audflags, str)       LOG_AT_LEVEL((audflags) | de::LogEntry::Message,  str)
#define LOG_NOTE_TO(audflags, str)      LOG_AT_LEVEL((audflags) | de::LogEntry::Note,     str)
#define LOG_WARNING_TO(audflags, str)   LOG_AT_LEVEL((audflags) | de::LogEntry::Warning,  str)
#define LOG_ERROR_TO(audflags, str)     LOG_AT_LEVEL((audflags) | de::LogEntry::Error,    str)
#define LOG_CRITICAL_TO(audflags, str)  LOG_AT_LEVEL((audflags) | de::LogEntry::Critical, str)

// Resource domain
#define LOG_RES_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Resource | (level), str)
#define LOG_RES_XVERBOSE(str, args)     LOG_PRECHECK_LEVEL(de::LogEntry::Resource | de::LogEntry::XVerbose, str, args)
#define LOG_RES_VERBOSE(str)            LOG_RES_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_RES_MSG(str)                LOG_RES_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_RES_NOTE(str)               LOG_RES_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_RES_WARNING(str)            LOG_RES_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_RES_ERROR(str)              LOG_RES_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_RES_CRITICAL(str)           LOG_RES_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_RES_AT_LEVEL(level, str) LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::Resource | (level), str)
#define LOGDEV_RES_XVERBOSE(str, args)  LOG_PRECHECK_LEVEL(de::LogEntry::Dev | de::LogEntry::Resource | de::LogEntry::XVerbose, str, args)
#define LOGDEV_RES_VERBOSE(str)         LOGDEV_RES_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_RES_MSG(str)             LOGDEV_RES_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_RES_NOTE(str)            LOGDEV_RES_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_RES_WARNING(str)         LOGDEV_RES_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_RES_ERROR(str)           LOGDEV_RES_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_RES_CRITICAL(str)        LOGDEV_RES_AT_LEVEL(de::LogEntry::Critical, str)

// Map domain
#define LOG_MAP_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Map | (level), str)
#define LOG_MAP_XVERBOSE(str, args)     LOG_PRECHECK_LEVEL(de::LogEntry::Map | de::LogEntry::XVerbose, str, args)
#define LOG_MAP_VERBOSE(str)            LOG_MAP_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_MAP_MSG(str)                LOG_MAP_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_MAP_NOTE(str)               LOG_MAP_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_MAP_WARNING(str)            LOG_MAP_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_MAP_ERROR(str)              LOG_MAP_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_MAP_CRITICAL(str)           LOG_MAP_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_MAP_AT_LEVEL(level, str) LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::Map | (level), str)
#define LOGDEV_MAP_XVERBOSE(str, args)  LOG_PRECHECK_LEVEL(de::LogEntry::Map | de::LogEntry::Dev | de::LogEntry::XVerbose, str, args)
#define LOGDEV_MAP_VERBOSE(str)         LOGDEV_MAP_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_MAP_MSG(str)             LOGDEV_MAP_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_MAP_NOTE(str)            LOGDEV_MAP_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_MAP_WARNING(str)         LOGDEV_MAP_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_MAP_ERROR(str)           LOGDEV_MAP_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_MAP_CRITICAL(str)        LOGDEV_MAP_AT_LEVEL(de::LogEntry::Critical, str)

// Script domain
#define LOG_SCR_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Script | (level), str)
#define LOG_SCR_XVERBOSE(str, args)     LOG_PRECHECK_LEVEL(de::LogEntry::Script | de::LogEntry::XVerbose, str, args)
#define LOG_SCR_VERBOSE(str)            LOG_SCR_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_SCR_MSG(str)                LOG_SCR_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_SCR_NOTE(str)               LOG_SCR_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_SCR_WARNING(str)            LOG_SCR_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_SCR_ERROR(str)              LOG_SCR_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_SCR_CRITICAL(str)           LOG_SCR_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_SCR_AT_LEVEL(level, str) LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::Script | (level), str)
#define LOGDEV_SCR_XVERBOSE(str, args)  LOG_PRECHECK_LEVEL(de::LogEntry::Dev | de::LogEntry::Script | de::LogEntry::XVerbose, str, args)
#define LOGDEV_SCR_VERBOSE(str)         LOGDEV_SCR_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_SCR_MSG(str)             LOGDEV_SCR_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_SCR_NOTE(str)            LOGDEV_SCR_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_SCR_WARNING(str)         LOGDEV_SCR_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_SCR_ERROR(str)           LOGDEV_SCR_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_SCR_CRITICAL(str)        LOGDEV_SCR_AT_LEVEL(de::LogEntry::Critical, str)

// Audio domain
#define LOG_AUDIO_AT_LEVEL(level, str)  LOG_AT_LEVEL(de::LogEntry::Audio | (level), str)
#define LOG_AUDIO_XVERBOSE(str, args)   LOG_PRECHECK_LEVEL(de::LogEntry::Audio | de::LogEntry::XVerbose, str, args)
#define LOG_AUDIO_VERBOSE(str)          LOG_AUDIO_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_AUDIO_MSG(str)              LOG_AUDIO_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_AUDIO_NOTE(str)             LOG_AUDIO_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_AUDIO_WARNING(str)          LOG_AUDIO_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_AUDIO_ERROR(str)            LOG_AUDIO_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_AUDIO_CRITICAL(str)         LOG_AUDIO_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_AUDIO_AT_LEVEL(level, str) \
                                        LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::Audio | (level), str)
#define LOGDEV_AUDIO_XVERBOSE(str, args) \
                                        LOG_PRECHECK_LEVEL(de::LogEntry::Audio | de::LogEntry::Dev | de::LogEntry::XVerbose, str, args)
#define LOGDEV_AUDIO_VERBOSE(str)       LOGDEV_AUDIO_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_AUDIO_MSG(str)           LOGDEV_AUDIO_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_AUDIO_NOTE(str)          LOGDEV_AUDIO_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_AUDIO_WARNING(str)       LOGDEV_AUDIO_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_AUDIO_ERROR(str)         LOGDEV_AUDIO_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_AUDIO_CRITICAL(str)      LOGDEV_AUDIO_AT_LEVEL(de::LogEntry::Critical, str)

// Graphics domain
#define LOG_GL_AT_LEVEL(level, str)     LOG_AT_LEVEL(de::LogEntry::GL | (level), str)
#define LOG_GL_XVERBOSE(str, args)      LOG_PRECHECK_LEVEL(de::LogEntry::GL | de::LogEntry::XVerbose, str, args)
#define LOG_GL_VERBOSE(str)             LOG_GL_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_GL_MSG(str)                 LOG_GL_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_GL_NOTE(str)                LOG_GL_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_GL_WARNING(str)             LOG_GL_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_GL_ERROR(str)               LOG_GL_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_GL_CRITICAL(str)            LOG_GL_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_GL_AT_LEVEL(level, str)  LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::GL | (level), str)
#define LOGDEV_GL_XVERBOSE(str, args)   LOG_PRECHECK_LEVEL(de::LogEntry::Dev | de::LogEntry::GL | de::LogEntry::XVerbose, str, args)
#define LOGDEV_GL_VERBOSE(str)          LOGDEV_GL_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_GL_MSG(str)              LOGDEV_GL_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_GL_NOTE(str)             LOGDEV_GL_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_GL_WARNING(str)          LOGDEV_GL_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_GL_ERROR(str)            LOGDEV_GL_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_GL_CRITICAL(str)         LOGDEV_GL_AT_LEVEL(de::LogEntry::Critical, str)

// Input domain
#define LOG_INPUT_AT_LEVEL(level, str)  LOG_AT_LEVEL(de::LogEntry::Input | (level), str)
#define LOG_INPUT_XVERBOSE(str, args)   LOG_PRECHECK_LEVEL(de::LogEntry::Input | de::LogEntry::XVerbose, str, args)
#define LOG_INPUT_VERBOSE(str)          LOG_INPUT_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_INPUT_MSG(str)              LOG_INPUT_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_INPUT_NOTE(str)             LOG_INPUT_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_INPUT_WARNING(str)          LOG_INPUT_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_INPUT_ERROR(str)            LOG_INPUT_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_INPUT_CRITICAL(str)         LOG_INPUT_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_INPUT_AT_LEVEL(level, str) \
                                        LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::Input | (level), str)
#define LOGDEV_INPUT_XVERBOSE(str, args) \
                                        LOG_PRECHECK_LEVEL(de::LogEntry::Dev | de::LogEntry::Input | de::LogEntry::XVerbose, str, args)
#define LOGDEV_INPUT_VERBOSE(str)       LOGDEV_INPUT_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_INPUT_MSG(str)           LOGDEV_INPUT_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_INPUT_NOTE(str)          LOGDEV_INPUT_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_INPUT_WARNING(str)       LOGDEV_INPUT_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_INPUT_ERROR(str)         LOGDEV_INPUT_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_INPUT_CRITICAL(str)      LOGDEV_INPUT_AT_LEVEL(de::LogEntry::Critical, str)

// Network domain
#define LOG_NET_AT_LEVEL(level, str)    LOG_AT_LEVEL(de::LogEntry::Network | (level), str)
#define LOG_NET_XVERBOSE(str, args)     LOG_PRECHECK_LEVEL(de::LogEntry::Network | de::LogEntry::XVerbose, str, args)
#define LOG_NET_VERBOSE(str)            LOG_NET_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOG_NET_MSG(str)                LOG_NET_AT_LEVEL(de::LogEntry::Message,  str)
#define LOG_NET_NOTE(str)               LOG_NET_AT_LEVEL(de::LogEntry::Note,     str)
#define LOG_NET_WARNING(str)            LOG_NET_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOG_NET_ERROR(str)              LOG_NET_AT_LEVEL(de::LogEntry::Error,    str)
#define LOG_NET_CRITICAL(str)           LOG_NET_AT_LEVEL(de::LogEntry::Critical, str)
#define LOGDEV_NET_AT_LEVEL(level, str) LOG_AT_LEVEL(de::LogEntry::Dev | de::LogEntry::Network | (level), str)
#define LOGDEV_NET_XVERBOSE(str, args)  LOG_PRECHECK_LEVEL(de::LogEntry::Dev | de::LogEntry::Network | de::LogEntry::XVerbose, str, args)
#define LOGDEV_NET_VERBOSE(str)         LOGDEV_NET_AT_LEVEL(de::LogEntry::Verbose,  str)
#define LOGDEV_NET_MSG(str)             LOGDEV_NET_AT_LEVEL(de::LogEntry::Message,  str)
#define LOGDEV_NET_NOTE(str)            LOGDEV_NET_AT_LEVEL(de::LogEntry::Note,     str)
#define LOGDEV_NET_WARNING(str)         LOGDEV_NET_AT_LEVEL(de::LogEntry::Warning,  str)
#define LOGDEV_NET_ERROR(str)           LOGDEV_NET_AT_LEVEL(de::LogEntry::Error,    str)
#define LOGDEV_NET_CRITICAL(str)        LOGDEV_NET_AT_LEVEL(de::LogEntry::Critical, str)

#ifdef DE_DEBUG
/**
 * Makes a developer-only extra verbose level log entry. Only enabled in debug builds; use this
 * for internal messages that might have a significant processing overhead. (Note that parameters
 * differ compared to the normal LOG_* macros.)
 */
#  define LOG_TRACE_DEBUGONLY(form, args)               LOG_TRACE(form, args)
#  define LOGDEV_MAP_XVERBOSE_DEBUGONLY(form, args)     LOGDEV_MAP_XVERBOSE(form, args)
#  define LOGDEV_RES_XVERBOSE_DEBUGONLY(form, args)     LOGDEV_RES_XVERBOSE(form, args)
#  define LOGDEV_SCR_XVERBOSE_DEBUGONLY(form, args)     LOGDEV_SCR_XVERBOSE(form, args)
#  define LOGDEV_NET_XVERBOSE_DEBUGONLY(form, args)     LOGDEV_NET_XVERBOSE(form, args)
#else
#  define LOG_TRACE_DEBUGONLY(form, args)
#  define LOGDEV_MAP_XVERBOSE_DEBUGONLY(form, args)
#  define LOGDEV_RES_XVERBOSE_DEBUGONLY(form, args)
#  define LOGDEV_SCR_XVERBOSE_DEBUGONLY(form, args)
#  define LOGDEV_NET_XVERBOSE_DEBUGONLY(form, args)
#endif

#ifdef WIN32
#   undef ERROR
#endif

namespace de {

class LogBuffer;


/**
 * An entry to be stored in the log entry buffer. Log entries are created with
 * Log::enter().
 *
 * Log entry arguments must be created before the entry itself is created. The
 * LogEntryStager class is designed to help with this. Once an entry is
 * inserted to the log buffer, no modifications may be done to it any more
 * because another thread may need it immediately for flushing.
 *
 * @ingroup core
 */
class DE_PUBLIC LogEntry : public Lockable, public ISerializable
{
public:
    /**
     * Entry domain (bits) and target audience. If not set, the entry is generic and
     * intended for the end-user/player.
     */
    enum Context
    {
        // Domains:
        FirstDomainBit  = 16,
        GenericBit      = FirstDomainBit,
        ResourceBit,
        MapBit,
        ScriptBit,
        GLBit,
        AudioBit,
        InputBit,
        NetworkBit,
        LastDomainBit   = NetworkBit,

        Generic  = (1 << GenericBit),   /**< Global domain (bit automatically set if no
                                             other domains). */
        Resource = (1 << ResourceBit),  /**< Resource or resource pack domain (files,
                                             etc.). "Resource" is here meant in a wider
                                             sense of all the external data that Doomsday
                                             utilizes. */
        Map      = (1 << MapBit),       /**< Map domain: information pertaining to the map
                                             and its elements, playsim, etc. */
        Script   = (1 << ScriptBit),    ///< Script domain
        GL       = (1 << GLBit),        ///< Graphics/renderer domain (shaders, etc.)
        Audio    = (1 << AudioBit),     ///< Audio domain
        Input    = (1 << InputBit),     ///< Input domain: events, devices, etc.
        Network  = (1 << NetworkBit),   ///< Network domain: connections, packets, etc.

        // User groups:
        Dev         = 0x08000000,       /**< Native code developer (i.e., the programmer);
                                             can be combined with other flags to mark the
                                             entry for devs. If bit is not set, the entry
                                             is for the end-user. */
        Privileged  = 0x04000000,       /**< Work in progress. Entries with this flag are
                                             shown regardless of log filtering, in a
                                             separate overlay. Use this for whatever you
                                             are currently working on (so there is no need
                                             to rely on qDebug). */
        Interactive = 0x02000000,       /**< Output from a command entered manually by the
                                             user. Typically these should never be
                                             filtered. */

        AllDomains  = 0x00ff0000,
        DomainMask  = AllDomains,
        ContextMask = 0x0fff0000
    };

    static String contextToText(Flags context)
    {
        const String suffix = (context & Dev? "Dev" : "");
        switch (context & DomainMask)
        {
        case Resource: return "Resource" + suffix;
        case Map:      return "Map" + suffix;
        case Script:   return "Script" + suffix;
        case GL:       return "GL" + suffix;
        case Audio:    return "Audio" + suffix;
        case Input:    return "Input" + suffix;
        case Network:  return "Network" + suffix;
        default:       return suffix;
        }
    }

    static Context textToContext(String text)
    {
        duint32 val = 0;
        if (text.endsWith("Dev"))
        {
            val |= Dev;
            text = text.remove(text.sizeb() - 3);
        }
        for (int i = FirstDomainBit; i <= LastDomainBit; ++i)
        {
            if (!contextToText(LogEntry::Context(1 << i)).compareWithoutCase(text))
                return LogEntry::Context((1 << i) | val);
        }
        throw de::Error("Log::textToContext", "'" + text + "' is not a valid log entry context");
    }

    /// Importance level of the log entry.
    enum Level
    {
        /**
         * Verbose messages should be used for logging additional/supplementary
         * information. All verbose messages can be safely ignored.
         */
        XVerbose = 1,
        Verbose = 2,

        /**
         * The base level: normal log entries.
         */
        Message = 3,

        /**
         * Important messages that are intended for situations that are particularly
         * noteworthy. They will not cause an alert to be raised, but the information is
         * deemed particularly valuable.
         */
        Note = 4,

        /**
         * Warning messages are reserved for error situations that were automatically
         * recovered from. A warning might be logged for example when the expected
         * resource could not be found, and a fallback resource was used instead.
         * Warnings will cause an alert to be raised so that the target audience is aware
         * of the problem.
         */
        Warning = 5,

        /**
         * Error messages are intended for errors that could not be recovered from: the
         * attempted operation had to be cancelled entirely. Will cause an alert to be
         * raised so that the target audience is aware of the problem.
         */
        Error = 6,

        /**
         * Critical messages are intended for fatal errors that force the game to be
         * unloaded or the entire engine to be shut down.
         */
        Critical = 7,

        LowestLogLevel  = XVerbose,
        HighestLogLevel = Critical,
        LevelMask       = 0x7
    };

    static String levelToText(duint32 level)
    {
        switch (level & LevelMask)
        {
        case XVerbose: return "XVerbose";
        case Verbose:  return "Verbose";
        case Message:  return "Message";
        case Note:     return "Note";
        case Warning:  return "Warning";
        case Error:    return "Error";
        case Critical: return "Critical";
        default:       return "";
        }
    }

    static Level textToLevel(const String &text)
    {
        for (int i = XVerbose; i <= HighestLogLevel; ++i)
        {
            if (!levelToText(Level(i)).compareWithoutCase(text))
                return Level(i);
        }
        throw de::Error("Log::textToLevel", "'" + text + "' is not a valid log level");
    }

    /**
     * Argument for a log entry. The arguments of an entry are usually created
     * automatically by LogEntryStager.
     *
     * @ingroup core
     */
    class DE_PUBLIC Arg : public String::IPatternArg, public ISerializable
    {
    public:
        /// The wrong type is used in accessing the value. @ingroup errors
        DE_ERROR(TypeError);

        enum Type {
            IntegerArgument,
            FloatingPointArgument,
            StringArgument
        };

        /**
         * Base class for classes that support adding to the arguments. Any
         * class that is derived from this class may be used as an argument for
         * log entries. In practice, all arguments are converted to either
         * numbers (64-bit integer or double) or text strings.
         */
        class Base {
        public:
            /// Attempted conversion from unsupported type.
            DE_ERROR(TypeError);

        public:
            virtual ~Base() {}

            virtual Type logEntryArgType() const = 0;
            virtual dint64 asInt64() const {
                throw TypeError("LogEntry::Arg::Base", "dint64 not supported");
            }
            virtual ddouble asDouble() const {
                throw TypeError("LogEntry::Arg::Base", "ddouble not supported");
            }
            virtual String asText() const {
                throw TypeError("LogEntry::Arg::Base", "String not supported");
            }
        };

    public:
        Arg();
        virtual ~Arg();

        void clear();

        void setValue(int32_t i);
        void setValue(uint32_t i);
        void setValue(int64_t i);
        void setValue(uint64_t i);
        void setValue(ddouble d);
#if defined(MACOSX)
        inline void setValue(size_t i) { setValue(uint64_t(i)); }
#endif
        void setValue(const void *p);
        void setValue(const char *s);
        void setValue(const std::string &s);
        void setValue(const String &s);
        void setValue(const Time &t);
        void setValue(const Base &arg);
        void setValue(const std::array<char, 4> &typecode);

        template <typename ValueType>
        Arg &set(const ValueType &s) { setValue(s); return *this; }

        Arg &operator = (const Arg &other);

        inline Type type() const { return _type; }
        inline dint64 intValue() const {
            DE_ASSERT(_type == IntegerArgument);
            return _data.intValue;
        }
        inline ddouble floatValue() const {
            DE_ASSERT(_type == FloatingPointArgument);
            return _data.floatValue;
        }
        inline String stringValue() const {
            DE_ASSERT(_type == StringArgument);
            return *_data.stringValue;
        }

        // Implements String::IPatternArg.
        ddouble asNumber() const;
        String asText() const;

        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);

    public:
        static Arg *newFromPool();
        static void returnToPool(Arg *arg);

        template <typename ValueType>
        static inline Arg *newFromPool(const ValueType &v) {
            return &(newFromPool()->set(v));
        }

    private:
        Type _type;
        union Data {
            dint64  intValue;
            ddouble floatValue;
            String *stringValue;
        } _data;
    };

public:
    enum Flag
    {
        /// In simple mode, only print the actual message contents,
        /// without metadata.
        Simple = 0x1,

        /// Use escape sequences to format the entry with text styles
        /// (for graphical output).
        Styled = 0x2,

        /// Omit the section from the entry text.
        OmitSection = 0x4,

        /// Indicate that the section is the same as on the previous line.
        SectionSameAsBefore = 0x8,

        /// Parts of the section can be abbreviated because they are clear
        /// from the context (e.g., previous line).
        AbbreviateSection = 0x10,

        /// Entry is not from a local source. Could be used to mark entries
        /// originating from a remote LogBuffer (over the network).
        Remote = 0x20,

        /// Entry level is not included in the output.
        OmitLevel = 0x40,

        /// Entry domain is not included in the output.
        OmitDomain = 0x80
    };

    typedef List<Arg *> Args;

public:
    /**
     * Constructs a disabled log entry.
     */
    LogEntry();

    LogEntry(duint32       metadata,
             const String &section,
             int           sectionDepth,
             const String &format,
             const Args &  args);

    /**
     * Copy constructor.
     *
     * @param other       Log entry.
     * @param extraFlags  Additional flags to apply to the new entry.
     */
    LogEntry(const LogEntry &other, Flags extraFlags = 0);

    ~LogEntry();

    Flags flags() const;

    /// Returns the timestamp of the entry.
    Time when() const { return _when; }

    inline duint32 metadata() const { return _metadata; }

    inline duint32 context() const { return _metadata & ContextMask; }

    inline Level level() const { return Level(_metadata & LevelMask); }

    /// Returns a reference to the entry's section part. Reference is valid
    /// for the lifetime of the entry.
    const String &section() const { return _section; }

    /// Returns the number of sub-sections in the entry's section part.
    int sectionDepth() const { return _sectionDepth; }

    const String &format() const { return _format; }

    /**
     * Converts the log entry to a string.
     *
     * @param flags           Flags that control how the text is composed.
     * @param shortenSection  Number of characters to cut from the beginning of the section.
     *                        With AbbreviateSection this limits which portion of the
     *                        section is subject to abbreviation.
     *
     * @return Composed textual representation of the entry.
     */
    String asText(const Flags &flags = 0, dsize shortenSection = 0) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    void advanceFormat(String::const_iterator &i) const;

private:
    Time    _when;
    duint32 _metadata;
    String  _section;
    int     _sectionDepth;
    String  _format;
    Flags   _defaultFlags;
    bool    _disabled;
    Args    _args;
};

/**
 * Provides means for adding log entries into the log entry buffer (LogBuffer).
 * Each thread has its own Log instance. A thread's Log keeps track of the
 * thread-local section stack.
 *
 * Note that there is only one LogBuffer where all the entries are collected.
 *
 * @ingroup core
 */
class DE_PUBLIC Log
{
public:
    class DE_PUBLIC Section
    {
    public:
        /**
         * The Section does not take a copy of @c name, so whatever
         * it's pointing to must exist while the Section exists.
         *
         * @param name  Name of the log section.
         */
        Section(const char *name);
        ~Section();

        Log &log() const { return _log; }

    private:
        Log &_log;
        const char *_name;
    };

public:
    Log();
    virtual ~Log();

    /**
     * Sets the metadata that applies to the current entry being staged. This can be
     * checked by print methods interested in adapting their content to the context
     * of the entry.
     *
     * @param metadata  Entry metadata flags.
     */
    void setCurrentEntryMetadata(duint32 metadata);

    /**
     * Returns the metadata for the entry currently being staged.
     *
     * @return Metadata flags, or 0 if no entry is being staged.
     */
    duint32 currentEntryMetadata() const;

    /**
     * Determines if an entry is currently being staged using LogEntryStager.
     */
    bool isStaging() const;

    /**
     * Begins a new section in the log. Sections can be nested.
     *
     * @param name  Name of the section. No copy of this string is made,
     *              so it must exist while the section is in use.
     */
    void beginSection(const char *name);

    /**
     * Ends the topmost section in the log.
     *
     * @param name  Name of the topmost section.
     */
    void endSection(const char *name);

    /**
     * Begins an interactive section. All entries added while interactive get
     * flagged as such. You must call endInteractive() to end the section.
     * Interactive sections can be nested.
     */
    void beginInteractive();

    /**
     * Ends an interactive section. The number of endInteractive() calls must
     * match the number of beginInteractive() calls.
     */
    void endInteractive();

    bool isInteractive() const;

    /**
     * Creates a new log entry with the default (Message) level, targeted to the end-user.
     *
     * @param format     Format template of the entry.
     * @param arguments  List of arguments. The entry is given ownership of
     *                   each Arg instance.
     */
    LogEntry &enter(const String &format, LogEntry::Args arguments = LogEntry::Args());

    /**
     * Creates a new log entry with the specified log entry level.
     *
     * @param metadata   Level of the entry and context bits.
     * @param format     Format template of the entry.
     * @param arguments  List of arguments. The entry is given ownership of
     *                   each Arg instance.
     */
    LogEntry &enter(duint32 metadata, const String &format, LogEntry::Args arguments = LogEntry::Args());

public:
    /**
     * Returns the logger of the current thread.
     */
    static Log &threadLog();

    /*
     * Deletes the current thread's log. Threads should call this before
     * they quit.
     */
//    static void disposeThreadLog();

private:
    DE_PRIVATE(d)
};

/**
 * Stages a log entry for insertion into LogBuffer. Instances of LogEntryStager
 * are built on the stack.
 *
 * You should use the @c LOG_* macros instead of using LogEntryStager directly.
 */
class DE_PUBLIC LogEntryStager
{
public:
    LogEntryStager(duint32 metadata, const String &format);

    /// Appends a new argument to the entry.
    template <typename ValueType>
    inline LogEntryStager &operator << (const ValueType &v) {
        // Args are created only if the level is enabled.
        if (!_disabled) {
            _args << LogEntry::Arg::newFromPool(v);
        }
        return *this;
    }

    ~LogEntryStager();

private:
    bool           _disabled;
    duint32        _metadata;
    String         _format;
    LogEntry::Args _args;
};

} // namespace de

#endif /* LIBCORE_LOG_H */
