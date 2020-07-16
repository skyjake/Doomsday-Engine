/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/c_wrapper.h"

#include "de/address.h"
#include "de/app.h"
#include "de/block.h"
#include "de/byteorder.h"
#include "de/byterefarray.h"
#include "de/commandline.h"
#include "de/error.h"
#include "de/info.h"
#include "de/logbuffer.h"
#include "de/loop.h"
#include "de/string.h"
#include "de/unixinfo.h"

#include <cstring>
#include <stdarg.h>

#define DE_COMMANDLINE()     DE_APP->commandLine()

static bool checkLogEntryMetadata(unsigned int &metadata)
{
    // Automatically apply the generic domain if not specified.
    if (!(metadata & de::LogEntry::DomainMask))
    {
        metadata |= de::LogEntry::Generic;
    }

    // Validate the level.
    de::LogEntry::Level logLevel = de::LogEntry::Level(metadata & de::LogEntry::LevelMask);
    if (logLevel < de::LogEntry::XVerbose || logLevel > de::LogEntry::Critical)
    {
        metadata &= ~de::LogEntry::LevelMask;
        metadata |= de::LogEntry::Message;
    }

    // If this level is not enabled, just ignore.
    return de::LogBuffer::get().isEnabled(metadata);
}

static void logFragmentPrinter(duint32 metadata, const char *fragment)
{
    static std::string currentLogLine;

    currentLogLine += fragment;

    std::string::size_type pos;
    while ((pos = currentLogLine.find('\n')) != std::string::npos)
    {
        LOG().enter(metadata, currentLogLine.substr(0, pos).c_str());
        currentLogLine.erase(0, pos + 1);
    }
}

void App_Log(unsigned int metadata, const char *format, ...)
{
    if (!checkLogEntryMetadata(metadata)) return;

    char buffer[0x2000];
    va_list args;
    va_start(args, format);
    size_t nc = vsprintf(buffer, format, args); /// @todo unsafe
    va_end(args);
    DE_ASSERT(nc < sizeof(buffer) - 2);
    if (!nc) return;

    LOG().enter(metadata, buffer);

    // Make sure there's a newline in the end.
    /*if (buffer[nc - 1] != '\n')
    {
        buffer[nc++] = '\n';
        buffer[nc] = 0;
    }
    logFragmentPrinter(metadata, buffer);*/
}

void App_Timer(unsigned int milliseconds, void (*callback)(void))
{
    de::Loop::timer(de::TimeSpan::fromMilliSeconds(milliseconds), callback);
}

void App_FatalError(const char *msgFormat, ...)
{
    char buffer[4096];
    de::zap(buffer);

    va_list args;
    va_start(args, msgFormat);
    std::vsnprintf(buffer, sizeof(buffer) - 1, msgFormat, args);
    va_end(args);

    DE_APP->handleUncaughtException(buffer);

    // Let's make sure this is the end.
    exit(-1);
}

void CommandLine_Alias(const char *longname, const char *shortname)
{
    DE_COMMANDLINE().alias(longname, shortname);
}

int CommandLine_Count(void)
{
    return DE_COMMANDLINE().sizei();
}

const char *CommandLine_At(int i)
{
    DE_ASSERT(i >= 0);
    DE_ASSERT(i < DE_COMMANDLINE().sizei());
    return *(DE_COMMANDLINE().argv() + i);
}

const char *CommandLine_PathAt(int i)
{
    DE_COMMANDLINE().makeAbsolutePath(i);
    return CommandLine_At(i);
}

static int argLastMatch = 0; // used only in ArgCheck/ArgNext (not thread-safe)

const char *CommandLine_Next(void)
{
    if (!argLastMatch || argLastMatch >= CommandLine_Count() - 1)
    {
        // No more arguments following the last match.
        return nullptr;
    }
    return CommandLine_At(++argLastMatch);
}

const char *CommandLine_NextAsPath(void)
{
    if (!argLastMatch || argLastMatch >= CommandLine_Count() - 1)
    {
        // No more arguments following the last match.
        return nullptr;
    }
    DE_COMMANDLINE().makeAbsolutePath(argLastMatch + 1);
    return CommandLine_Next();
}

int CommandLine_Check(const char *check)
{
    return argLastMatch = DE_COMMANDLINE().check(check);
}

int CommandLine_CheckWith(const char *check, int num)
{
    return argLastMatch = DE_COMMANDLINE().check(check, num);
}

int CommandLine_Exists(const char *check)
{
    return DE_COMMANDLINE().has(check);
}

int CommandLine_IsOption(int i)
{
    return DE_COMMANDLINE().isOption(i);
}

int CommandLine_IsMatchingAlias(const char *original, const char *originalOrAlias)
{
    return DE_COMMANDLINE().matches(original, originalOrAlias);
}

void LogBuffer_Flush(void)
{
    if (de::LogBuffer::appBufferExists())
    {
        de::LogBuffer::get().flush();
    }
}

void LogBuffer_Clear(void)
{
    de::LogBuffer::get().clear();
}

void LogBuffer_EnableStandardOutput(int enable)
{
    de::LogBuffer::get().enableStandardOutput(enable != 0);
}

void LogBuffer_Printf(unsigned int metadata, const char *format, ...)
{
    if (!checkLogEntryMetadata(metadata)) return;

    char buffer[0x2000];
    va_list args;
    va_start(args, format);
    size_t nc = vsprintf(buffer, format, args); /// @todo unsafe
    va_end(args);
    DE_ASSERT(nc < sizeof(buffer) - 1);
    DE_UNUSED(nc);

    logFragmentPrinter(metadata, buffer);
}

de_Info *Info_NewFromString(const char *utf8text)
{
    try
    {
        return reinterpret_cast<de_Info *>(new de::Info(de::String::fromUtf8(utf8text)));
    }
    catch (const de::Error &er)
    {
        LOG_WARNING(er.asText());
        return nullptr;
    }
}

de_Info *Info_NewFromFile(const char *nativePath)
{
    try
    {
        std::unique_ptr<de::Info> info(new de::Info);
        info->parseNativeFile(nativePath);
        return reinterpret_cast<de_Info *>(info.release());
    }
    catch (const de::Error &er)
    {
        LOG_WARNING(er.asText());
        return nullptr;
    }
}

void Info_Delete(de_Info *info)
{
    if (info)
    {
        DE_SELF(Info, info);
        delete self;
    }
}

int Info_FindValue(de_Info *info, const char *path, char *buffer, size_t bufSize)
{
    if (!info) return false;

    DE_SELF(Info, info);
    const de::Info::Element *element = self->findByPath(path);
    if (!element || !element->isKey()) return false;
    de::String value = static_cast<const de::Info::KeyElement *>(element)->value();
    if (buffer)
    {
        strncpy(buffer, value, bufSize);
        return true;
    }
    else
    {
        // Just return the size of the value.
        return value.sizei();
    }
}

char *UnixInfo_GetConfigValue(const char *configFile, const char *key)
{
    de::UnixInfo &info = de::App::unixInfo();

    // "paths" is the only config file currently being used.
    if (!iCmpStr(configFile, "paths"))
    {
        de::NativePath foundValue;
        if (info.path(key, foundValue))
        {
            return iDupStr(foundValue);
        }
    }
    else if (!iCmpStr(configFile, "defaults"))
    {
        de::String foundValue;
        if (info.defaults(key, foundValue))
        {
            return iDupStr(foundValue);
        }
    }
    return nullptr;
}

dint16 LittleEndianByteOrder_ToForeignInt16(dint16 value)
{
    DE_ASSERT(sizeof(dint16) == sizeof(de::dint16));
    return de::littleEndianByteOrder.toNetwork(de::dint16(value));
}

dint32 LittleEndianByteOrder_ToForeignInt32(dint32 value)
{
    DE_ASSERT(sizeof(dint32) == sizeof(de::dint32));
    return de::littleEndianByteOrder.toNetwork(de::dint32(value));
}

dint64 LittleEndianByteOrder_ToForeignInt64(dint64 value)
{
    DE_ASSERT(sizeof(dint64) == sizeof(de::dint64));
    return de::littleEndianByteOrder.toNetwork(de::dint64(value));
}

duint16 LittleEndianByteOrder_ToForeignUInt16(duint16 value)
{
    DE_ASSERT(sizeof(duint16) == sizeof(de::duint16));
    return de::littleEndianByteOrder.toNetwork(de::duint16(value));
}

duint32 LittleEndianByteOrder_ToForeignUInt32(duint32 value)
{
    DE_ASSERT(sizeof(duint32) == sizeof(de::duint32));
    return de::littleEndianByteOrder.toNetwork(de::duint32(value));
}

duint64 LittleEndianByteOrder_ToForeignUInt64(duint64 value)
{
    DE_ASSERT(sizeof(duint64) == sizeof(de::duint64));
    return de::littleEndianByteOrder.toNetwork(de::duint64(value));
}

dfloat LittleEndianByteOrder_ToForeignFloat(dfloat value)
{
    DE_ASSERT(sizeof(dfloat) == sizeof(float));
    return de::littleEndianByteOrder.toNetwork(float(value));
}

ddouble LittleEndianByteOrder_ToForeignDouble(ddouble value)
{
    DE_ASSERT(sizeof(ddouble) == sizeof(double));
    return de::littleEndianByteOrder.toNetwork(double(value));
}

dint16 LittleEndianByteOrder_ToNativeInt16(dint16 value)
{
    DE_ASSERT(sizeof(dint16) == sizeof(de::dint16));
    return de::littleEndianByteOrder.toHost(de::dint16(value));
}

dint32 LittleEndianByteOrder_ToNativeInt32(dint32 value)
{
    DE_ASSERT(sizeof(dint32) == sizeof(de::dint32));
    return de::littleEndianByteOrder.toHost(de::dint32(value));
}

dint64 LittleEndianByteOrder_ToNativeInt64(dint64 value)
{
    DE_ASSERT(sizeof(dint64) == sizeof(de::dint64));
    return de::littleEndianByteOrder.toHost(de::dint64(value));
}

duint16 LittleEndianByteOrder_ToNativeUInt16(duint16 value)
{
    DE_ASSERT(sizeof(duint16) == sizeof(de::duint16));
    return de::littleEndianByteOrder.toHost(de::duint16(value));
}

duint32 LittleEndianByteOrder_ToNativeUInt32(duint32 value)
{
    DE_ASSERT(sizeof(duint32) == sizeof(de::duint32));
    return de::littleEndianByteOrder.toHost(de::duint32(value));
}

duint64 LittleEndianByteOrder_ToNativeUInt64(duint64 value)
{
    DE_ASSERT(sizeof(duint64) == sizeof(de::duint64));
    return de::littleEndianByteOrder.toHost(de::duint64(value));
}

dfloat LittleEndianByteOrder_ToNativeFloat(dfloat value)
{
    DE_ASSERT(sizeof(dfloat) == sizeof(float));
    return de::littleEndianByteOrder.toHost(float(value));
}

ddouble LittleEndianByteOrder_ToNativeDouble(ddouble value)
{
    DE_ASSERT(sizeof(ddouble) == sizeof(double));
    return de::littleEndianByteOrder.toHost(double(value));
}
