/**\file size.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Daniel Swanson <danij@dengine.net>
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

struct size2i_s {
    /// Use a "raw" size natively so that we can return immutable references
    /// to this directly (no dynamic allocation).
    Size2Rawi raw;
};

struct size2f_s {
    /// Use a "raw" size natively so that we can return immutable references
    /// to this directly (no dynamic allocation).
    Size2Rawf raw;
};

Size2i* Size2i_New(void)
{
    Size2i* s = calloc(1, sizeof *s);
    return s;
}

Size2i* Size2i_NewWithDimensions(int width, int height)
{
    Size2i* s = Size2i_New();
    Size2i_SetWidthHeight(s, width, height);
    return s;
}

Size2i* Size2i_NewFromRaw(const Size2Rawi* rawSize)
{
    assert(rawSize);
    return Size2i_NewWithDimensions(rawSize->width, rawSize->height);
}

void Size2i_Delete(Size2i* s)
{
    assert(s);
    free(s);
}

boolean Size2i_IsNull(const Size2i* s)
{
    assert(s);
    return s->raw.width == 0 && s->raw.height == 0;
}

const Size2Rawi* Size2i_ToRaw(const Size2i* s)
{
    assert(s);
    return &s->raw;
}

int Size2i_Width(const Size2i* s)
{
    assert(s);
    return s->raw.width;
}

int Size2i_Height(const Size2i* s)
{
    assert(s);
    return s->raw.height;
}

void Size2i_SetWidth(Size2i* s, int width)
{
    assert(s);
    s->raw.width = width;
}

void Size2i_SetHeight(Size2i* s, int height)
{
    assert(s);
    s->raw.height = height;
}

const int* Size2i_WidthHeight(const Size2i* s)
{
    assert(s);
    return s->raw.widthHeight;
}

void Size2i_SetWidthHeight(Size2i* s, int width, int height)
{
    assert(s);
    s->raw.width = width;
    s->raw.height = height;
}

void Size2i_Sum(Size2i* s, const Size2i* other)
{
    assert(s && other);
    s->raw.width  += Size2i_Width(other);
    s->raw.height += Size2i_Height(other);
}

boolean Size2i_Equality(const Size2i* s, const Size2i* other)
{
    assert(s && other);
    return s == other || (s->raw.width  == Size2i_Width(other) &&
                          s->raw.height == Size2i_Height(other));
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

const Size2Rawf* Size2f_ToRaw(const Size2f* s)
{
    assert(s);
    return &s->raw;
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
