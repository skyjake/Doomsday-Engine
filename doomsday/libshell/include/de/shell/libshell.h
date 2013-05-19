/** @file libshell.h  Common definitions for libshell.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBSHELL_MAIN_H
#define LIBSHELL_MAIN_H

#include <de/String>

/*
 * The LIBSHELL_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libshell.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __LIBSHELL__
// This is defined when compiling the library.
#    define LIBSHELL_PUBLIC __declspec(dllexport)
#  else
#    define LIBSHELL_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define LIBSHELL_PUBLIC
#endif

namespace de {
namespace shell {

struct LIBSHELL_PUBLIC Range
{
    int start;
    int end;

    Range(int a = 0, int b = 0) : start(a), end(b) {}
    inline int size() const { return end - start; }
    inline bool contains(int i) const { return i >= start && i < end; }
    inline bool operator == (Range const &other) const {
        return start == other.start && end == other.end;
    }
};

/// Word wrapping.
struct LIBSHELL_PUBLIC WrappedLine
{
    Range range;
    bool isFinal;

    WrappedLine(Range const &range_, bool final = false)
        : range(range_), isFinal(final) {}
};

class LIBSHELL_PUBLIC ILineWrapping
{
public:
    virtual ~ILineWrapping() {}

    virtual bool isEmpty() const = 0;
    virtual void clear() = 0;
    virtual void wrapTextToWidth(String const &text, int maxWidth) = 0;
    virtual WrappedLine line(int index) const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

} // namespace shell
} // namespace de

#endif // LIBSHELL_MAIN_H
