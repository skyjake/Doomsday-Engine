/** @file hexlex.h  Lexical analyzer for Hexen definition/script syntax.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef IMPORTIDTECH1_HEXLEX_H
#define IMPORTIDTECH1_HEXLEX_H

#include "importidtech1.h"

namespace idtech1 {

/**
 * Lexical analyzer for Hexen definition/script syntax.
 *
 * @ingroup idtech1converter
 */
class HexLex
{
public:
    /// Base error for syntax errors at the level of lexical analysis (e.g.,
    /// a non-terminated string constant). @ingroup errors
    DE_ERROR(SyntaxError);

public:
    /**
     * Construct a new lexer and optionally prepare a script for parsing.
     *
     * @param script      If non-zero, prepare this script for parsing.
     * @param sourcePath  Path of the source of @a script, used when logging/reporting errors.
     *
     * @see parse(), setSourcePath()
     */
    explicit HexLex(const Str *script = nullptr, const de::String &sourcePath = "");

    /**
     * Prepare a new script for parsing. It is assumed that the @a script data
     * remains available until parsing is completed (or the script is replaced).
     *
     * @param script  The script source to be parsed.
     */
    void parse(const Str *script);

    /**
     * Change the source path used to identify the script in log messages.
     *
     * @param sourcePath  New source path to apply. A copy is made.
     *
     * @see sourcePath()
     */
    void setSourcePath(const de::String &sourcePath);

    /**
     * Returns the currently configured source path.
     *
     * @see setSourcePath()
     */
    const de::String &sourcePath() const;

    /**
     * Returns the line number at the current position in the script.
     */
    int lineNumber() const;

    /**
     * Attempt to read the next token from the script. @c true is returned if a
     * token was parsed (or the previously parsed token was @em unread); otherwise
     * @c false is returned (e.g., the end of the script was reached).
     */
    bool readToken();

    /**
     * Mark the last read token as @em unread, so that it will be re-read as the
     * next read token.
     */
    void unreadToken();

    /**
     * Returns a copy of the last read token.
     */
    const Str *token();

    double readNumber();
    const Str *readString();
    res::Uri readUri(const de::String &defaultScheme = "");

private:
    DE_PRIVATE(d)
};

} // namespace idtech1

#endif // IMPORTIDTECH1_HEXLEX_H
