/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_STRING_H
#define LIBDENG2_STRING_H

#include <QString>

#include "../libdeng2.h"
#include "../Block"

namespace de {

/**
 * The String class extends the QString class with Block conversion and
 * other convenience methods.
 *
 * @ingroup types
 */
class DENG2_PUBLIC String : public QString
{
public:
    /// Error related to String operations (note: shadows de::Error). @ingroup errors
    DENG2_ERROR(Error);

    /// Encoding conversion failed. @ingroup errors
    DENG2_SUB_ERROR(Error, ConversionError);

    /// An error was encountered in string pattern replacement. @ingroup errors
    DENG2_SUB_ERROR(Error, IllegalPatternError);

    /// Invalid record member name. @ingroup errors
    DENG2_SUB_ERROR(Error, InvalidMemberError);

    /**
     * Data argument for the pattern formatter.
     * @see String::patternFormat()
     */
    class IPatternArg
    {
    public:
        /// An incompatible type is requested in asText() or asNumber(). @ingroup errors
        DENG2_ERROR(TypeError);

    public:
        virtual ~IPatternArg() {}

        /// Returns the value of the argument as a text string.
        virtual String asText() const = 0;

        /// Returns the value of the argument as a number.
        virtual ddouble asNumber() const = 0;
    };

    typedef dint size_type;

public:
    static size_type const npos;

public:
    String();
    String(String const &other);
    String(QString const &text);
    String(char const *nullTerminatedCStr);
    String(char const *cStr, size_type length);
    String(QChar const *nullTerminatedStr);
    String(QChar const *str, size_type length);
    String(size_type length, QChar ch);
    String(QString const &str, size_type index, size_type length);
    String(const_iterator start, const_iterator end);
    String(iterator start, iterator end);

    /// Conversion to a character pointer.
    operator QChar const *() const {
        return data();
    }

    /// Determines whether the string is empty.
    bool empty() const { return size() == 0; }

    /// Returns the first character of the string.
    QChar first() const;

    /// Returns the last character of the string.
    QChar last() const;

    bool beginsWith(QString const &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return startsWith(s, cs);
    }
    bool beginsWith(QChar const &c, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return startsWith(c, cs);
    }

    String substr(int position, int n = -1) const {
        return mid(position, n);
    }

    /**
     * Does a path concatenation on this string and the argument. Note that if
     * @a path is an absolute path (starts with @a dirChar), the result of the
     * concatenation is just @a path.
     *
     * @param path     Path to concatenate.
     * @param dirChar  Directory/folder separator character.
     */
    String concatenatePath(String const &path, QChar dirChar = '/') const;

    /**
     * Does a path concatenation on this string and the argument. Note that if
     * @a path is an absolute path (starts with '/'), the result of the
     * concatenation is just @a path.
     *
     * @param path  Path to concatenate.
     */
    String operator / (String const &path) const;

    /**
     * Does a record member concatenation on a variable name. Record members
     * use '.' as the separator character.
     *
     * @param member  Identifier to append to this string.
     */
    String concatenateMember(String const &member) const;

    /// Removes whitespace from the beginning and end of the string.
    /// @return Copy of the string without whitespace.
    String strip() const;

    /// Removes whitespace from the beginning of the string.
    /// @return Copy of the string without whitespace.
    String leftStrip() const;

    /// Removes whitespace from the end of the string.
    /// @return Copy of the string without whitespace.
    String rightStrip() const;

    /// Returns a lower-case version of the string.
    String lower() const;

    /// Returns an upper-case version of the string.
    String upper() const;

    /// Extracts the base name from the string (includes extension).
    String fileName() const;

    /// Extracts the base name from the string (does not include extension).
    String fileNameWithoutExtension() const;

    /**
     * Extracts the file name extension from a path. A valid extension
     * is the part of a file name after a period where the file name
     * itself is at least one character long. For instance:
     * with "a.ext" the extension is ".ext", but with ".ext" there is
     * no extension.
     *
     * @return The extension, including the period in the beginning.
     * An empty string is returned if the string contains no period.
     */
    String fileNameExtension() const;

    /// Extracts the path of the string.
    String fileNamePath(QChar dirChar = '/') const;

    /**
     * Compare two strings (case sensitive).
     *
     * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
     *         Negative, if @a a < @a b.
     */
    dint compareWithCase(String const &str) const;

    /**
     * Compare two strings (case insensitive).
     *
     * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
     *         Negative, if @a a < @a b.
     */
    dint compareWithoutCase(String const &str) const;

    /**
     * Compare two strings (case insensitive), but only up to @a n characters.
     * @param str  Other string.
     * @param n    Maximum length to compare up to.
     * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
     *         Negative, if @a a < @a b.
     */
    dint compareWithoutCase(String const &str, int n) const;

    /**
     * Compares two strings (case sensitive) to see how many characters they
     * have in common starting from the left.
     *
     * @param str  String.
     *
     * @return  Number of characters the two strings have in common from the left.
     */
    int commonPrefixLength(String const &str) const;

    /**
     * Converts the string to UTF-8 and returns it as a byte block.
     */
    Block toUtf8() const;

    /**
     * Converts the string to Latin1 and returns it as a byte block.
     */
    Block toLatin1() const;

    /**
     * Flags for controlling how string-to-integer conversion works.
     * See de::String::toInt().
     */
    enum IntConversionFlag {
        AllowOnlyWhitespace = 0x0,
        AllowSuffix = 0x1
    };
    Q_DECLARE_FLAGS(IntConversionFlags, IntConversionFlag)

    /**
     * Converts the string to an integer. The default behavior is identical to
     * QString::toInt(). With the @a flags parameter the conversion behavior can
     * be modified.
     *
     * Whitespace at the beginning of the string is always ignored.
     *
     * With the @c AllowSuffix conversion flag, the conversion emulates the
     * behavior of the standard C library's @c atoi(): the number is converted
     * successfully even when it is followed by characters that do not parse
     * into a valid number (a non-digit and non-sign character).
     *
     * @param ok     @c true is returned via this pointer if the conversion was
     *               successful.
     * @param base   Base for the number.
     * @param flags  Conversion flags, see de::String::IntConversionFlag.
     *
     * @return Integer parsed from the string (@c *ok set to true). @c 0 if
     * the conversion fails (@c *ok set to @c false).
     */
    dint toInt(bool *ok = 0, int base = 10, IntConversionFlags flags = AllowOnlyWhitespace) const;

public:
    /**
     * Builds a String out of an array of bytes that contains a UTF-8 string.
     */
    static String fromUtf8(IByteArray const &byteArray);

    /**
     * Builds a String out of an array of bytes that contains a Latin1 string.
     */
    static String fromLatin1(IByteArray const &byteArray);

    static dint compareWithCase(QChar const *a, QChar const *b, dsize count);

    /**
     * Advances the iterator until a nonspace character is encountered.
     *
     * @param i  Iterator to advance.
     * @param end  End of the string. Will not advance past this.
     */
    static void skipSpace(String::const_iterator &i, String::const_iterator const &end);

    /**
     * Formats data according to formatting instructions. Outputs a
     * string containing the formatted data.
     *
     * @param formatIter  Will be moved past the formatting instructions.
     *                    After the call, will remain past the end of
     *                    the formatting characters.
     * @param formatEnd   Iterator to the end of the format string.
     * @param arg         Argument to format.
     *
     * @return  Formatted argument as a string.
     */
    static String patternFormat(String::const_iterator &formatIter,
                                String::const_iterator const &formatEnd,
                                IPatternArg const &arg);

    static void advanceFormat(String::const_iterator &i,
                              String::const_iterator const &end);
};

/**
 * Support routine for determining the length of a null-terminated QChar*
 * string. Later versions have a QString constructor that needs no length
 * when constructing a string from QChar*.
 */
size_t qchar_strlen(QChar const *str);

} // namespace de

#endif // LIBDENG2_STRING_H
