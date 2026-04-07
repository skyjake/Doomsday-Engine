/** @file p_saveio.cpp  Game save file IO.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "p_saveio.h"

#include <de/error.h>
#include <de/fixedbytearray.h>
#include <de/byterefarray.h>

// Used during write:
static de::Writer *writer;

// Used during read:
static de::Reader *reader;

static char sri8(reader_s *r)
{
    if(!r) return 0;
    int8_t val;
    DE_ASSERT(reader);
    *reader >> val;
    return char(val);
}

static short sri16(reader_s *r)
{
    if(!r) return 0;
    int16_t val;
    DE_ASSERT(reader);
    *reader >> val;
    return short(val);
}

static int sri32(reader_s *r)
{
    if(!r) return 0;
    DE_ASSERT(reader);
    int32_t val;
    *reader >> val;
    return int(val);
}

static float srf(reader_s *r)
{
    if(!r) return 0;
    DE_ASSERT(reader);
    DE_ASSERT(sizeof(float) == 4);
    int32_t val;
    *reader >> val;
    float rerVal = 0;
    std::memcpy(&rerVal, &val, 4);
    return rerVal;
}

static void srd(reader_s *r, char *data, int len)
{
    if(!r) return;
    DE_ASSERT(reader);
    if(data)
    {
        de::ByteRefArray ref(data, len);
        reader->readBytesFixedSize(ref);
    }
    else
    {
        reader->seek(len);
    }
}

reader_s *SV_NewReader()
{
    DE_ASSERT(reader != 0);
    return Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
}

de::Reader &SV_RawReader()
{
    if(reader != 0)
    {
        return *reader;
    }
    throw de::Error("SV_RawReader", "No map reader exists");
}

void SV_CloseFile()
{
    delete reader; reader = 0;
    delete writer; writer = 0;
}

bool SV_OpenFileForRead(const de::File &file)
{
    SV_CloseFile();
    reader = new de::Reader(file);
    return true;
}

bool SV_OpenFileForWrite(de::IByteArray &block)
{
    SV_CloseFile();
    writer = new de::Writer(block);
    return true;
}

static void swi8(writer_s *w, char val)
{
    if(!w) return;
    DE_ASSERT(writer);
    *writer << val;
}

static void swi16(Writer1 *w, short val)
{
    if(!w) return;
    DE_ASSERT(writer);
    *writer << val;
}

static void swi32(Writer1 *w, int val)
{
    if(!w) return;
    DE_ASSERT(writer);
    *writer << val;
}

static void swf(Writer1 *w, float val)
{
    if(!w) return;
    DE_ASSERT(writer);
    DE_ASSERT(sizeof(float) == 4);

    int32_t temp = 0;
    std::memcpy(&temp, &val, 4);
    *writer << val;
}

static void swd(Writer1 *w, const char *data, int len)
{
    if(!w) return;
    DE_ASSERT(writer);
    if(data)
    {
        writer->writeBytes(de::ByteRefArray(data, len));
    }
}

writer_s *SV_NewWriter()
{
    return Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
}

de::Writer &SV_RawWriter()
{
    if(writer != 0)
    {
        return *writer;
    }
    throw de::Error("SV_RawWriter", "No map writer exists");
}
