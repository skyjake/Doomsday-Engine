/** @file hexlex.h  Lexical analyzer for Hexen definition/script syntax.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_HEXLEX_H
#define LIBCOMMON_HEXLEX_H

#include "common.h"

/**
 * Lexical analyzer for Hexen definition/script syntax.
 */
class HexLex
{
public:
    /**
     * Construct a new lexer and optionally prepare a script for parsing.
     *
     * @param script      If non-zero, prepare this script for parsing.
     * @param sourcePath  If non-zero, set this as the script source path.
     *
     * @see parse(), setSourcePath()
     */
    HexLex(const Str *script = 0, const Str *sourcePath = 0);
    ~HexLex();

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
     */
    void setSourcePath(const Str *sourcePath = 0);

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

    /**
     * Returns the line number at the current position in the script.
     */
    int lineNumber() const;

private:
    void checkOpen();
    bool atEnd();
    void syntaxError(const char *message);

    Str _sourcePath;    ///< Used to identify the source in error messages.

    const Str *_script; ///< The start of the script being parsed.
    int _readPos;       ///< Current read position.
    int _lineNumber;

    Str _token;
    bool _alreadyGot;
    bool _multiline;    ///< @c true= current token spans multiple lines.
};

#endif // LIBCOMMON_HEXLEX_H
