/** @file
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/m_decomp64.h"

static short mapA[629], mapB[629];
static short tableA[1258], tableB[1258];

static const byte* srcPos;

static void cycleTable(int a, int b)
{
    short               n;

    for (;;)
    {
        if (a == mapA[tableA[b]])
            n = mapB[tableA[b]];
        else
            n = mapA[tableA[b]];

        tableB[tableA[a]] = tableB[n] + tableB[a];

        if (tableA[a] == 1)
            break;

        a = b = tableA[a];
    }
}

static short rotateMap(int a, int b, int c)
{
    if (a == mapA[tableA[a]])
    {
        mapB[tableA[a]] = c;
    }
    else
    {
        mapA[tableA[a]] = c;
    }

    if (c == mapA[a])
    {
        mapA[a] = b;
        return mapB[a];
    }
    else
    {
        mapB[a] = b;
        return mapA[a];
    }
}

void M_Decompress64(byte* dst, const byte* src)
{
#define BUFF_SIZE       (21903)

    // ith = ((2 / 3) * (4 ^ n - 1)) * 8
    static const int    strides[] = {0, 16, 80, 336, 1360, 5456};

    int                 i, val, lastOut, curByte;
    byte                buff[BUFF_SIZE], curBit;
    byte*               dstPos;

    // Initialize LUTs, todo: precalculate where possible.
    for (i = 0; i < 1258; ++i)
    {
       tableB[i] = 1;
    }

    for (i = 0; i < 1258; ++i)
    {
        tableA[i] = ((i + 2) / 2) - 1;
    }

    mapB[0] = 0;
    for (i = 0; i < 628; ++i)
    {
        mapB[1 + i] = (i * 2) + 3;
    }

    for (i = 0; i < 629; ++i)
    {
        mapA[i] = i * 2;
    }

    srcPos = src;
    dstPos = dst;

    curByte = 0;
    curBit = 0;

    // Begin decompression.
    lastOut = 0;
    do
    {
        int                 index = 1;

        while (index < 629)
        {
            if (curBit == 0)
                curByte = *srcPos++;

            if (curByte & 0x80)
                index = mapB[index];
            else
                index = mapA[index];

            curBit = (curBit == 0? 7 : curBit - 1);
            curByte <<= 1;
        }

        tableB[index]++;

        if (tableA[index] != 1)
        {
            cycleTable(index, index);

            if (tableB[1] == 2000)
            {
                int                 i;

                for (i = 0; i < 1258; ++i)
                {
                    tableB[i] >>= 1;
                }
            }
        }

        {
        int                 c;

        c = index;
        while (tableA[c] != 1)
        {
            int                 b, a;

            a = tableA[c];

            if (a == mapA[tableA[a]])
                b = mapB[tableA[a]];
            else
                b = mapA[tableA[a]];

            if (tableB[b] < tableB[c])
            {
                int                 result;

                result = rotateMap(a, b, c);

                tableA[b] = a;
                tableA[c] = tableA[a];

                cycleTable(b, result);

                c = tableA[b];
            }
            else
            {
                c = tableA[c];
            }
        }
        }

        val = index - 629;

        if (val != 256)
        {
            if (val < 256)
            {
                byte                out = (val & 0xff);

                *dstPos++ = out;
                buff[lastOut++] = out;
            }
            else
            {
                int                 i, div, to, from, num, result;

                div = (val - 257) / 62;
                result = 0;

                {
                int                 shift = 1;

                for (i = 0; i < div * 2 + 4; ++i)
                {
                    if (curBit == 0)
                        curByte = *srcPos++;

                    if ((curByte) & 0x80)
                        result |= shift;

                    curBit = (curBit == 0? 7 : curBit - 1);
                    curByte <<= 1;

                    shift <<= 1;
                }
                }

                num = val - 254 - (div * 62);

                from = lastOut - result - strides[div] - num;
                if (from < 0)
                    from += BUFF_SIZE;

                to = lastOut;

                for (i = 0; i < num; ++i)
                {
                    byte                out = buff[from];

                    *dstPos++ = out;
                    buff[to] = out;

                    if (++from == BUFF_SIZE)
                        from = 0;

                    if (++to == BUFF_SIZE)
                        to = 0;
                }

                lastOut += num;
            }

            if (lastOut >= BUFF_SIZE)
                lastOut -= BUFF_SIZE;
        }
    } while (val != 256);

#undef BUFF_SIZE
}
