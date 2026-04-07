/**
 * @file size.c
 * 2D sizes.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/legacy/size.h"
#include "de/legacy/memory.h"

struct size2_s {
    /// Use a "raw" Size natively so that we can simply copy when called upon.
    Size2Raw raw;
};

struct size2f_s {
    /// Use a "raw" Size natively so that we can simply copy when called upon.
    Size2Rawf raw;
};

Size2 *Size2_New(void)
{
    Size2 *s = M_Calloc(sizeof *s);
    return s;
}

Size2 *Size2_NewWithDimensions(int width, int height)
{
    Size2 *s = Size2_New();
    Size2_SetWidthHeight(s, width, height);
    return s;
}

Size2 *Size2_NewFromRaw(Size2Raw const *rawSize)
{
    DE_ASSERT(rawSize);
    return Size2_NewWithDimensions(rawSize->width, rawSize->height);
}

void Size2_Delete(Size2 *s)
{
    DE_ASSERT(s);
    M_Free(s);
}

dd_bool Size2_IsNull(Size2 const *s)
{
    DE_ASSERT(s);
    return s->raw.width == 0 && s->raw.height == 0;
}

Size2Raw *Size2_Raw(Size2 const *s, Size2Raw *rawSize)
{
    DE_ASSERT(s);
    if (!rawSize) return NULL;
    memcpy(rawSize, &s->raw, sizeof(*rawSize));
    return rawSize;
}

int Size2_Width(Size2 const *s)
{
    DE_ASSERT(s);
    return s->raw.width;
}

int Size2_Height(Size2 const *s)
{
    DE_ASSERT(s);
    return s->raw.height;
}

void Size2_SetWidth(Size2 *s, int width)
{
    DE_ASSERT(s);
    s->raw.width = width;
}

void Size2_SetHeight(Size2 *s, int height)
{
    DE_ASSERT(s);
    s->raw.height = height;
}

int const *Size2_WidthHeight(Size2 const *s)
{
    DE_ASSERT(s);
    return s->raw.widthHeight;
}

void Size2_SetWidthHeight(Size2 *s, int width, int height)
{
    DE_ASSERT(s);
    s->raw.width = width;
    s->raw.height = height;
}

void Size2_Sum(Size2 *s, Size2 const *other)
{
    DE_ASSERT(s && other);
    s->raw.width  += Size2_Width(other);
    s->raw.height += Size2_Height(other);
}

dd_bool Size2_Equality(Size2 const *s, Size2 const *other)
{
    DE_ASSERT(s && other);
    return s == other || (s->raw.width  == Size2_Width(other) &&
                          s->raw.height == Size2_Height(other));
}

Size2f *Size2f_New(void)
{
    Size2f *s = M_Calloc(sizeof *s);
    return s;
}

Size2f *Size2f_NewWithDimensions(double width, double height)
{
    Size2f *s = Size2f_New();
    Size2f_SetWidthHeight(s, width, height);
    return s;
}

Size2f *Size2f_NewFromRaw(Size2Rawf const *rawSize)
{
    DE_ASSERT(rawSize);
    return Size2f_NewWithDimensions(rawSize->width, rawSize->height);
}

void Size2f_Delete(Size2f *s)
{
    DE_ASSERT(s);
    M_Free(s);
}

dd_bool Size2f_IsNull(Size2f const *s)
{
    DE_ASSERT(s);
    return s->raw.width == 0 && s->raw.height == 0;
}

Size2Rawf *Size2f_Raw(Size2f const *s, Size2Rawf *rawSize)
{
    DE_ASSERT(s);
    if (!rawSize) return NULL;
    memcpy(rawSize, &s->raw, sizeof(*rawSize));
    return rawSize;
}

double Size2f_Width(Size2f const *s)
{
    DE_ASSERT(s);
    return s->raw.width;
}

double Size2f_Height(Size2f const *s)
{
    DE_ASSERT(s);
    return s->raw.height;
}

void Size2f_SetWidth(Size2f *s, double width)
{
    DE_ASSERT(s);
    s->raw.width = width;
}

void Size2f_SetHeight(Size2f *s, double height)
{
    DE_ASSERT(s);
    s->raw.height = height;
}

double const *Size2f_WidthHeight(Size2f const *s)
{
    DE_ASSERT(s);
    return s->raw.widthHeight;
}

void Size2f_SetWidthHeight(Size2f *s, double width, double height)
{
    DE_ASSERT(s);
    s->raw.width  = width;
    s->raw.height = height;
}

void Size2f_Sum(Size2f *s, Size2f const *other)
{
    DE_ASSERT(s && other);
    s->raw.width  += Size2f_Width(other);
    s->raw.height += Size2f_Height(other);
}

dd_bool Size2f_Equality(Size2f const *s, Size2f const *other)
{
    DE_ASSERT(s && other);
    return s == other || (s->raw.width  == Size2f_Width(other) &&
                          s->raw.height == Size2f_Height(other));
}
