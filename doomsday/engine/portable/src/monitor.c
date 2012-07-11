/**
 * @file monitor.c
 * Implementation of network traffic monitoring. @ingroup network
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_base.h"
#include "de_console.h"
#include "de_platform.h"

#if _DEBUG

static uint monitor[256];
static uint monitoredBytes;
static uint monitoredPackets;
static size_t monitorMaxSize;

static void Monitor_Start(int maxPacketSize)
{
    monitorMaxSize = maxPacketSize;
    monitoredBytes = monitoredPackets = 0;
    memset(&monitor, 0, sizeof(monitor));
}

static void Monitor_Stop(void)
{
    monitorMaxSize = 0;
}

void Monitor_Add(const uint8_t* bytes, size_t size)
{
    if(size <= monitorMaxSize)
    {
        uint i;
        monitoredPackets++;
        monitoredBytes += size;
        for(i = 0; i < size; ++i) monitor[bytes[i]]++;
    }
}

static void Monitor_Print(void)
{
    int i, k;

    if(!monitoredBytes)
    {
        Con_Message("Nothing has been sent yet.\n");
        return;
    }
    Con_Message("%u bytes sent (%i packets).\n", monitoredBytes, monitoredPackets);

    for(i = 0, k = 0; i < 256; ++i)
    {
        if(!k) Con_Message("    ");

        Con_Message("%10.10lf", (double)(monitor[i]) / (double)monitoredBytes);

        // Break lines.
        if(++k == 4)
        {
            k = 0;
            Con_Message(",\n");
        }
        else
        {
            Con_Message(", ");
        }
    }
    if(k) Con_Message("\n");
}

D_CMD(NetFreqs)
{
    if(argc == 1) // No args?
    {
        Con_Printf("Usage:\n  %s start (maxsize)\n  %s stop\n  %s print/show\n", argv[0], argv[0], argv[0]);
        return true;
    }
    if(argc == 3 && !strcmp(argv[1], "start"))
    {
        Monitor_Start(strtoul(argv[2], 0, 10));
        return true;
    }
    if(argc == 2 && !strcmp(argv[1], "stop"))
    {
        Monitor_Stop();
        return true;
    }
    if(argc == 2 && (!strcmp(argv[1], "print") || !strcmp(argv[1], "show")))
    {
        Monitor_Print();
        return true;
    }
    return false;
}

#endif // _DEBUG
