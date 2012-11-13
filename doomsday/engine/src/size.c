/**\file size.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <de_base.h>
#include <de_console.h>

#include "size.h"

struct size2_s {
    /// Use a "raw" Size natively so that we can simply copy when called upon.
    Size2Raw raw;
};

struct size2f_s {
    /// Use a "raw" Size natively so that we can simply copy when called upon.
    Size2Rawf raw;
};

Size2* Size2_New(void)
{
    Size2* s = calloc(1, sizeof *s);
    return s;
}

Size2* Size2_NewWithDimensions(int width, int height)
{
    Size2* s = Size2_New();
    Size2_SetWidthHeight(s, width, height);
    return s;
}

Size2* Size2_NewFromRaw(const Size2Raw* rawSize)
{
    assert(rawSize);
    return Size2_NewWithDimensions(rawSize->width, rawSize->height);
}

void Size2_Delete(Size2* s)
{
    assert(s);
    free(s);
}

boolean Size2_IsNull(const Size2* s)
{
    assert(s);
    return s->raw.width == 0 && s->raw.height == 0;
}

Size2Raw* Size2_Raw(const Size2* s, Size2Raw* rawSize)
{
    assert(s);
    if(!rawSize) return NULL;
    memcpy(rawSize, &s->raw, sizeof(*rawSize));
    return rawSize;
}

int Size2_Width(const Size2* s)
{
    assert(s);
    return s->raw.width;
}

int Size2_Height(const Size2* s)
{
    assert(s);
    return s->raw.height;
}

void Size2_SetWidth(Size2* s, int width)
{
    assert(s);
    s->raw.width = width;
}

void Size2_SetHeight(Size2* s, int height)
{
    assert(s);
    s->raw.height = height;
}

const int* Size2_WidthHeight(const Size2* s)
{
    assert(s);
    return s->raw.widthHeight;
}

void Size2_SetWidthHeight(Size2* s, int width, int height)
{
    assert(s);
    s->raw.width = width;
    s->raw.height = height;
}

void Size2_Sum(Size2* s, const Size2* other)
{
    assert(s && other);
    s->raw.width  += Size2_Width(other);
    s->raw.height += Size2_Height(other);
}

boolean Size2_Equality(const Size2* s, const Size2* other)
{
    assert(s && other);
    return s == other || (s->raw.width  == Size2_Width(other) &&
                          s->raw.height == Size2_Height(other));
}

Size2f* Size2f_New(void)
{
    Size2f* s = calloc(1, sizeof *s);
    return s;
}

Size2f* Size2f_NewWithDimensions(double width, double height)
{
    Size2f* s = Size2f_New();
    Size2f_SetWidthHeight(s, width, height);
    return s;
}

Size2f* Size2f_NewFromRaw(const Size2Rawf* rawSize)
{
    assert(rawSize);
    return Size2f_NewWithDimensions(rawSize->width, rawSize->height);
}

void Size2f_Delete(Size2f* s)
{
    assert(s);
    free(s);
}

boolean Size2f_IsNull(const Size2f* s)
{
    assert(s);
    return s->raw.width == 0 && s->raw.height == 0;
}

Size2Rawf* Size2f_Raw(const Size2f* s, Size2Rawf* rawSize)
{
    assert(s);
    if(!rawSize) return NULL;
    memcpy(rawSize, &s->raw, sizeof(*rawSize));
    return rawSize;
}

double Size2f_Width(const Size2f* s)
{
    assert(s);
    return s->raw.width;
}

double Size2f_Height(const Size2f* s)
{
    assert(s);
    return s->raw.height;
}

void Size2f_SetWidth(Size2f* s, double width)
{
    assert(s);
    s->raw.width = width;
}

void Size2f_SetHeight(Size2f* s, double height)
{
    assert(s);
    s->raw.height = height;
}

const double* Size2f_WidthHeight(const Size2f* s)
{
    assert(s);
    return s->raw.widthHeight;
}

void Size2f_SetWidthHeight(Size2f* s, double width, double height)
{
    assert(s);
    s->raw.width  = width;
    s->raw.height = height;
}

void Size2f_Sum(Size2f* s, const Size2f* other)
{
    assert(s && other);
    s->raw.width  += Size2f_Width(other);
    s->raw.height += Size2f_Height(other);
}

boolean Size2f_Equality(const Size2f* s, const Size2f* other)
{
    assert(s && other);
    return s == other || (s->raw.width  == Size2f_Width(other) &&
                          s->raw.height == Size2f_Height(other));
}
