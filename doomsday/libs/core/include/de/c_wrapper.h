/** @file c_wrapper.h  C wrapper for various libcore classes.
 * @ingroup core
 *
 * Defines a C wrapper API for (some of the) libcore classes. Legacy code
 * can use this wrapper API to access libcore functionality. Note that the
 * identifiers in this file are @em not in the @c de namespace.
 *
 * @note The basic de data types (e.g., dint32) are not available for the C
 * API; instead, only the standard C data types should be used.
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_C_WRAPPER_H
#define LIBCORE_C_WRAPPER_H

#include "libcore.h"

#if defined(__cplusplus) && !defined(DE_C_API_ONLY)
using de::dint16;
using de::dint32;
using de::dint64;
using de::duint16;
using de::duint32;
using de::duint64;
using de::dfloat;
using de::ddouble;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * App
 */
DE_PUBLIC void App_Log(unsigned int metadata, const char *format, ...);
DE_PUBLIC void App_Timer(unsigned int milliseconds, void (*callback)(void));
DE_PUBLIC DE_NORETURN void App_FatalError(const char *msgFormat, ...);

/*
 * CommandLine
 */
DE_PUBLIC void CommandLine_Alias(const char *longname, const char *shortname);
DE_PUBLIC int CommandLine_Count(void);
DE_PUBLIC const char *CommandLine_At(int i);
DE_PUBLIC const char *CommandLine_PathAt(int i);
DE_PUBLIC const char *CommandLine_Next(void);
DE_PUBLIC const char *CommandLine_NextAsPath(void);
DE_PUBLIC int CommandLine_Check(const char *check);
DE_PUBLIC int CommandLine_CheckWith(const char *check, int num);
DE_PUBLIC int CommandLine_Exists(const char *check);
DE_PUBLIC int CommandLine_IsOption(int i);
DE_PUBLIC int CommandLine_IsMatchingAlias(const char *original, const char *originalOrAlias);

/*
 * LogBuffer
 */
#define DE2_ESC(StringLiteral) "\x1b" #StringLiteral

// Log levels (see de::Log for description).
typedef enum logentry_metadata_e
{
    DE2_LOG_XVERBOSE    = 1,
    DE2_LOG_VERBOSE     = 2,
    DE2_LOG_MESSAGE     = 3,
    DE2_LOG_NOTE        = 4,
    DE2_LOG_WARNING     = 5,
    DE2_LOG_ERROR       = 6,
    DE2_LOG_CRITICAL    = 7,

    DE2_LOG_GENERIC     = 0x10000,
    DE2_LOG_RES         = 0x20000,
    DE2_LOG_MAP         = 0x40000,      ///< Map developer
    DE2_LOG_SCR         = 0x80000,      ///< Script developer
    DE2_LOG_GL          = 0x100000,     ///< GL domain (shaders, etc.)
    DE2_LOG_AUDIO       = 0x200000,     ///< Audio domain
    DE2_LOG_INPUT       = 0x400000,     ///< Input domain
    DE2_LOG_NET         = 0x800000,     ///< Network domain
    DE2_LOG_DEV         = 0x8000000,    ///< Native code developer (i.e., the programmer)

    DE2_DEV_XVERBOSE = DE2_LOG_DEV | DE2_LOG_XVERBOSE,
    DE2_DEV_VERBOSE  = DE2_LOG_DEV | DE2_LOG_VERBOSE,
    DE2_DEV_MSG      = DE2_LOG_DEV | DE2_LOG_MESSAGE,
    DE2_DEV_NOTE     = DE2_LOG_DEV | DE2_LOG_NOTE,
    DE2_DEV_WARNING  = DE2_LOG_DEV | DE2_LOG_WARNING,
    DE2_DEV_ERROR    = DE2_LOG_DEV | DE2_LOG_ERROR,
    DE2_DEV_CRITICAL = DE2_LOG_DEV | DE2_LOG_CRITICAL,

    // RES
    DE2_RES_XVERBOSE = DE2_LOG_RES | DE2_LOG_XVERBOSE,
    DE2_RES_VERBOSE  = DE2_LOG_RES | DE2_LOG_VERBOSE,
    DE2_RES_MSG      = DE2_LOG_RES | DE2_LOG_MESSAGE,
    DE2_RES_NOTE     = DE2_LOG_RES | DE2_LOG_NOTE,
    DE2_RES_WARNING  = DE2_LOG_RES | DE2_LOG_WARNING,
    DE2_RES_ERROR    = DE2_LOG_RES | DE2_LOG_ERROR,
    DE2_RES_CRITICAL = DE2_LOG_RES | DE2_LOG_CRITICAL,

    DE2_DEV_RES_XVERBOSE = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_XVERBOSE,
    DE2_DEV_RES_VERBOSE  = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_VERBOSE,
    DE2_DEV_RES_MSG      = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_MESSAGE,
    DE2_DEV_RES_NOTE     = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_NOTE,
    DE2_DEV_RES_WARNING  = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_WARNING,
    DE2_DEV_RES_ERROR    = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_ERROR,
    DE2_DEV_RES_CRITICAL = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_CRITICAL,

    // MAP
    DE2_MAP_XVERBOSE = DE2_LOG_MAP | DE2_LOG_XVERBOSE,
    DE2_MAP_VERBOSE  = DE2_LOG_MAP | DE2_LOG_VERBOSE,
    DE2_MAP_MSG      = DE2_LOG_MAP | DE2_LOG_MESSAGE,
    DE2_MAP_NOTE     = DE2_LOG_MAP | DE2_LOG_NOTE,
    DE2_MAP_WARNING  = DE2_LOG_MAP | DE2_LOG_WARNING,
    DE2_MAP_ERROR    = DE2_LOG_MAP | DE2_LOG_ERROR,
    DE2_MAP_CRITICAL = DE2_LOG_MAP | DE2_LOG_CRITICAL,

    DE2_DEV_MAP_XVERBOSE = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_XVERBOSE,
    DE2_DEV_MAP_VERBOSE  = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_VERBOSE,
    DE2_DEV_MAP_MSG      = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_MESSAGE,
    DE2_DEV_MAP_NOTE     = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_NOTE,
    DE2_DEV_MAP_WARNING  = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_WARNING,
    DE2_DEV_MAP_ERROR    = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_ERROR,
    DE2_DEV_MAP_CRITICAL = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_CRITICAL,

    // SCR
    DE2_SCR_XVERBOSE = DE2_LOG_SCR | DE2_LOG_XVERBOSE,
    DE2_SCR_VERBOSE  = DE2_LOG_SCR | DE2_LOG_VERBOSE,
    DE2_SCR_MSG      = DE2_LOG_SCR | DE2_LOG_MESSAGE,
    DE2_SCR_NOTE     = DE2_LOG_SCR | DE2_LOG_NOTE,
    DE2_SCR_WARNING  = DE2_LOG_SCR | DE2_LOG_WARNING,
    DE2_SCR_ERROR    = DE2_LOG_SCR | DE2_LOG_ERROR,
    DE2_SCR_CRITICAL = DE2_LOG_SCR | DE2_LOG_CRITICAL,

    DE2_DEV_SCR_XVERBOSE = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_XVERBOSE,
    DE2_DEV_SCR_VERBOSE  = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_VERBOSE,
    DE2_DEV_SCR_MSG      = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_MESSAGE,
    DE2_DEV_SCR_NOTE     = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_NOTE,
    DE2_DEV_SCR_WARNING  = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_WARNING,
    DE2_DEV_SCR_ERROR    = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_ERROR,
    DE2_DEV_SCR_CRITICAL = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_CRITICAL,

    // GL
    DE2_GL_XVERBOSE = DE2_LOG_GL | DE2_LOG_XVERBOSE,
    DE2_GL_VERBOSE  = DE2_LOG_GL | DE2_LOG_VERBOSE,
    DE2_GL_MSG      = DE2_LOG_GL | DE2_LOG_MESSAGE,
    DE2_GL_NOTE     = DE2_LOG_GL | DE2_LOG_NOTE,
    DE2_GL_WARNING  = DE2_LOG_GL | DE2_LOG_WARNING,
    DE2_GL_ERROR    = DE2_LOG_GL | DE2_LOG_ERROR,
    DE2_GL_CRITICAL = DE2_LOG_GL | DE2_LOG_CRITICAL,

    DE2_DEV_GL_XVERBOSE = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_XVERBOSE,
    DE2_DEV_GL_VERBOSE  = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_VERBOSE,
    DE2_DEV_GL_MSG      = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_MESSAGE,
    DE2_DEV_GL_NOTE     = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_NOTE,
    DE2_DEV_GL_WARNING  = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_WARNING,
    DE2_DEV_GL_ERROR    = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_ERROR,
    DE2_DEV_GL_CRITICAL = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_CRITICAL,

    // AUDIO
    DE2_AUDIO_XVERBOSE = DE2_LOG_AUDIO | DE2_LOG_XVERBOSE,
    DE2_AUDIO_VERBOSE  = DE2_LOG_AUDIO | DE2_LOG_VERBOSE,
    DE2_AUDIO_MSG      = DE2_LOG_AUDIO | DE2_LOG_MESSAGE,
    DE2_AUDIO_NOTE     = DE2_LOG_AUDIO | DE2_LOG_NOTE,
    DE2_AUDIO_WARNING  = DE2_LOG_AUDIO | DE2_LOG_WARNING,
    DE2_AUDIO_ERROR    = DE2_LOG_AUDIO | DE2_LOG_ERROR,
    DE2_AUDIO_CRITICAL = DE2_LOG_AUDIO | DE2_LOG_CRITICAL,

    DE2_DEV_AUDIO_XVERBOSE = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_XVERBOSE,
    DE2_DEV_AUDIO_VERBOSE  = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_VERBOSE,
    DE2_DEV_AUDIO_MSG      = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_MESSAGE,
    DE2_DEV_AUDIO_NOTE     = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_NOTE,
    DE2_DEV_AUDIO_WARNING  = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_WARNING,
    DE2_DEV_AUDIO_ERROR    = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_ERROR,
    DE2_DEV_AUDIO_CRITICAL = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_CRITICAL,

    // INPUT
    DE2_INPUT_XVERBOSE = DE2_LOG_INPUT | DE2_LOG_XVERBOSE,
    DE2_INPUT_VERBOSE  = DE2_LOG_INPUT | DE2_LOG_VERBOSE,
    DE2_INPUT_MSG      = DE2_LOG_INPUT | DE2_LOG_MESSAGE,
    DE2_INPUT_NOTE     = DE2_LOG_INPUT | DE2_LOG_NOTE,
    DE2_INPUT_WARNING  = DE2_LOG_INPUT | DE2_LOG_WARNING,
    DE2_INPUT_ERROR    = DE2_LOG_INPUT | DE2_LOG_ERROR,
    DE2_INPUT_CRITICAL = DE2_LOG_INPUT | DE2_LOG_CRITICAL,

    DE2_DEV_INPUT_XVERBOSE = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_XVERBOSE,
    DE2_DEV_INPUT_VERBOSE  = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_VERBOSE,
    DE2_DEV_INPUT_MSG      = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_MESSAGE,
    DE2_DEV_INPUT_NOTE     = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_NOTE,
    DE2_DEV_INPUT_WARNING  = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_WARNING,
    DE2_DEV_INPUT_ERROR    = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_ERROR,
    DE2_DEV_INPUT_CRITICAL = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_CRITICAL,

    // NET
    DE2_NET_XVERBOSE = DE2_LOG_NET | DE2_LOG_XVERBOSE,
    DE2_NET_VERBOSE  = DE2_LOG_NET | DE2_LOG_VERBOSE,
    DE2_NET_MSG      = DE2_LOG_NET | DE2_LOG_MESSAGE,
    DE2_NET_NOTE     = DE2_LOG_NET | DE2_LOG_NOTE,
    DE2_NET_WARNING  = DE2_LOG_NET | DE2_LOG_WARNING,
    DE2_NET_ERROR    = DE2_LOG_NET | DE2_LOG_ERROR,
    DE2_NET_CRITICAL = DE2_LOG_NET | DE2_LOG_CRITICAL,

    DE2_DEV_NET_XVERBOSE = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_XVERBOSE,
    DE2_DEV_NET_VERBOSE  = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_VERBOSE,
    DE2_DEV_NET_MSG      = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_MESSAGE,
    DE2_DEV_NET_NOTE     = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_NOTE,
    DE2_DEV_NET_WARNING  = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_WARNING,
    DE2_DEV_NET_ERROR    = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_ERROR,
    DE2_DEV_NET_CRITICAL = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_CRITICAL
}
logentry_metadata_t;

#define DE2_LOG_DEBUG   (DE2_LOG_DEV | DE2_LOG_VERBOSE)
#define DE2_LOG_TRACE   (DE2_LOG_DEV | DE2_LOG_XVERBOSE)

DE_PUBLIC void LogBuffer_EnableStandardOutput(int enable);
DE_PUBLIC void LogBuffer_Flush(void);
DE_PUBLIC void LogBuffer_Clear(void);
DE_PUBLIC void LogBuffer_Printf(unsigned int metadata, const char *format, ...); // note: manual newlines

/*
 * Info
 */
DE_OPAQUE(de_Info)

DE_PUBLIC de_Info *Info_NewFromString(const char *utf8text);
DE_PUBLIC de_Info *Info_NewFromFile(const char *nativePath);
DE_PUBLIC void Info_Delete(de_Info *info);
DE_PUBLIC int Info_FindValue(de_Info *info, const char *path, char *buffer, size_t bufSize);

/*
 * UnixInfo
 */
DE_PUBLIC char *UnixInfo_GetConfigValue(const char *configFile, const char *key); // caller must free() returned

/*
 * ByteOrder
 */
DE_PUBLIC dint16  LittleEndianByteOrder_ToForeignInt16(dint16 value);
DE_PUBLIC dint32  LittleEndianByteOrder_ToForeignInt32(dint32 value);
DE_PUBLIC dint64  LittleEndianByteOrder_ToForeignInt64(dint64 value);
DE_PUBLIC duint16 LittleEndianByteOrder_ToForeignUInt16(duint16 value);
DE_PUBLIC duint32 LittleEndianByteOrder_ToForeignUInt32(duint32 value);
DE_PUBLIC duint64 LittleEndianByteOrder_ToForeignUInt64(duint64 value);
DE_PUBLIC dfloat  LittleEndianByteOrder_ToForeignFloat(dfloat value);
DE_PUBLIC ddouble LittleEndianByteOrder_ToForeignDouble(ddouble value);
DE_PUBLIC dint16  LittleEndianByteOrder_ToNativeInt16(dint16 value);
DE_PUBLIC dint32  LittleEndianByteOrder_ToNativeInt32(dint32 value);
DE_PUBLIC dint64  LittleEndianByteOrder_ToNativeInt64(dint64 value);
DE_PUBLIC duint16 LittleEndianByteOrder_ToNativeUInt16(duint16 value);
DE_PUBLIC duint32 LittleEndianByteOrder_ToNativeUInt32(duint32 value);
DE_PUBLIC duint64 LittleEndianByteOrder_ToNativeUInt64(duint64 value);
DE_PUBLIC dfloat  LittleEndianByteOrder_ToNativeFloat(dfloat value);
DE_PUBLIC ddouble LittleEndianByteOrder_ToNativeDouble(ddouble value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCORE_C_WRAPPER_H
