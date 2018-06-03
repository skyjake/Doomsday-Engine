/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_STRING_H
#define LIBCORE_STRING_H

#include "../libcore.h"
#include "../Block"
#include "../Range"
#include "../List"
#include "../RegExp"

#include <c_plus/string.h>

namespace de {

/**
 * The String class extends the QString class with Block conversion and
 * other convenience methods.
 *
 * The default codec when converting strings from C strings is UTF-8 (e.g.,
 * constructor that takes a <code>const char *</code> as the only argument).
 *
 * @ingroup types
 */
class DE_PUBLIC String
{
public:
    /// Error related to String operations (note: shadows de::Error). @ingroup errors
    DE_ERROR(Error);

    /// Encoding conversion failed. @ingroup errors
    DE_SUB_ERROR(Error, ConversionError);

    /// An error was encountered in string pattern replacement. @ingroup errors
    DE_SUB_ERROR(Error, IllegalPatternError);

    /// Invalid record member name. @ingroup errors
    DE_SUB_ERROR(Error, InvalidMemberError);

    /**
     * Data argument for the pattern formatter.
     * @see String::patternFormat()
     */
    class IPatternArg
    {
    public:
        /// An incompatible type is requested in asText() or asNumber(). @ingroup errors
        DE_ERROR(TypeError);

    public:
        virtual ~IPatternArg() {}

        /// Returns the value of the argument as a text string.
        virtual String asText() const = 0;

        /// Returns the value of the argument as a number.
        virtual ddouble asNumber() const = 0;
    };

    typedef List<const IPatternArg *> PatternArgs;
    enum CaseSensitivity { CaseInsensitive, CaseSensitive };

public:
    using size_type = dsize;
    static size_type const npos;
    using Char2 = char[2];

public:
    String();
    String(const String &other);
    String(String &&moved);
    String(const Block &bytes);
    String(const iString *other);
    String(const std::string &text);
    String(const char *nullTerminatedCStr);
    String(const wchar_t *nullTerminatedWideStr);
    String(const char *cStr, int length);
    String(const char *cStr, dsize length);
//    String(const QChar *nullTerminatedStr);
//    String(const QChar *str, size_type length);
    String(dsize length, char ch);
    String(dsize length, Char ch);
    String(const char *start, const char *end);
    String(const std::string &str, dsize index, dsize length);

    template <typename Iterator>
    String(Iterator start, Iterator end)
    {
        for (Iterator i = start; i != end; ++i)
        {
            push_back(*i);
        }
    }

    ~String();

    inline String &operator=(const String &other)
    {
        set_String(&_str, &other._str);
        return *this;
    }

    inline String &operator=(String &&moved)
    {
        deinit_String(&_str);
        _str = moved._str;
        zap(moved._str);
        return *this;
    }

    inline explicit operator bool() const { return size() > 0; }

    inline dsize size() const { return size_String(&_str); }

    /// Determines whether the string is empty.
    inline bool empty() const { return size() == 0; }

    inline bool isEmpty() const { return empty(); }

    const char *c_str() const { return cstr_String(&_str); }

    /// Conversion to a character pointer.
    operator const char *() const { return c_str(); }

    operator std::string() const { return toStdString(); }

    std::string toStdString() const { return {constBegin_String(&_str), constEnd_String(&_str)}; }

    std::wstring toWideString() const;

    void clear();

    /// Returns the first character of the string.
    Char first() const;

    /// Returns the last character of the string.
    Char last() const;

    bool contains(const char *cStr) const;

    bool beginsWith(const String &s, CaseSensitivity cs = CaseSensitive) const
    {
        return startsWithSc_String(&_str, s, cs == CaseSensitive? &iCaseSensitive : &iCaseInsensitive);
    }
    //    bool beginsWith(Qconst String &s, CaseSensitivity cs = CaseSensitive) const {
    //        return startsWith(s, cs == CaseSensitive? Qt::CaseSensitive : Qt::CaseInsensitive);
    //    }
    bool beginsWith(char ch, CaseSensitivity cs = CaseSensitive) const
    {
        return beginsWith(Char2{ch, 0}, cs);
    }
    //    bool beginsWith(QLatin1String ls, CaseSensitivity cs = CaseSensitive) const {
    //        return startsWith(ls, cs == CaseSensitive? Qt::CaseSensitive : Qt::CaseInsensitive);
    //    }
    bool beginsWith(const char *cstr, CaseSensitivity cs = CaseSensitive) const
    {
        return startsWithSc_String(&_str, cstr, cs == CaseSensitive? &iCaseSensitive : &iCaseInsensitive);
    }

//    bool endsWith(Qconst String &s, CaseSensitivity cs = CaseSensitive) const {
//        return QString::endsWith(s, cs == CaseSensitive? Qt::CaseSensitive : Qt::CaseInsensitive);
//    }
    bool endsWith(char ch, CaseSensitivity cs = CaseSensitive) const
    {
        return endsWith(Char2{ch, 0}, cs);
        //        return QString::endsWith(c, cs == CaseSensitive? Qt::CaseSensitive : Qt::CaseInsensitive);
    }
//    bool endsWith(QLatin1String ls, CaseSensitivity cs = CaseSensitive) const {
//        return QString::endsWith(ls, cs == CaseSensitive? Qt::CaseSensitive : Qt::CaseInsensitive);
//    }
    bool endsWith(const char *cstr, CaseSensitivity cs = CaseSensitive) const
    {
        return endsWithSc_String(
            &_str, cstr, cs == CaseSensitive ? &iCaseSensitive : &iCaseInsensitive);
    }

    inline String mid(int position, int n = -1) const { return substr(position, n); }
    String        substr(size_type pos, size_type n = iInvalidPos) const;
    String        substr(int position, int n = -1) const;
    String        substr(const Rangei &range) const { return mid(range.start, range.size()); }

    String        operator+(const char *) const;
    inline String operator+(const String &other) const { return *this + (const char *) other; }

    String &operator+=(char ch);
    String &operator+=(Char ch);
    String &operator+=(const char *);
    String &operator+=(const String &);

    /**
     * Does a path concatenation on this string and the argument. Note that if
     * @a path is an absolute path (starts with @a dirChar), the result of the
     * concatenation is just @a path.
     *
     * @param path     Path to concatenate.
     * @param dirChar  Directory/folder separator character.
     */
    String concatenatePath(const String &path, Char dirChar = '/') const;

    String concatenateRelativePath(const String &path, Char dirChar = '/') const;

    /**
     * Does a path concatenation on this string and the argument. Note that if
     * @a path is an absolute path (starts with '/'), the result of the
     * concatenation is just @a path.
     *
     * @param path  Path to concatenate.
     */
    String operator/(const String &path) const;

    /**
     * Applies pattern formatting using the string as a format string.
     *
     * @param args  List of arguments.
     *
     * @return String with placeholders replaced using the arguments.
     * @see patternFormat()
     */
    String operator%(const PatternArgs &args) const;

    /**
     * Does a record member concatenation on a variable name. Record members
     * use '.' as the separator character.
     *
     * @param member  Identifier to append to this string.
     */
    String concatenateMember(const String &member) const;

    /// Removes whitespace from the beginning and end of the string.
    /// @return Copy of the string without whitespace.
    String strip() const;

    /// Removes whitespace from the beginning of the string.
    /// @return Copy of the string without whitespace.
    String leftStrip() const;

    /// Removes whitespace from the end of the string.
    /// @return Copy of the string without whitespace.
    String rightStrip() const;

    /// Replaces all sequences of whitespace with single space characters.
    String normalizeWhitespace() const;

    /// Returns a copy of the string with matches removed.
    String removed(const RegExp &expr) const;

    /// Returns a lower-case version of the string.
    String lower() const;

    /// Returns an upper-case version of the string.
    String upper() const;

    /// Returns a version of the string where the first character is in uppercase.
    String upperFirstChar() const;

    /// Extracts the base name from the string (includes extension).
    String fileName(Char dirChar = '/') const;

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
    String fileNamePath(Char dirChar = '/') const;

    /// Extracts everything but the extension from string.
    String fileNameAndPathWithoutExtension(char dirChar = '/') const;

    int indexOf(char ch) const { size_t pos = indexOf_String(&_str, ch); return pos == iInvalidPos ? -1 : pos; }
    int indexOf(Char ch) const { size_t pos = indexOf_String(&_str, ch); return pos == iInvalidPos ? -1 : pos; }
    int indexOf(const char *cstr) const { size_t pos = indexOfCStr_String(&_str, cstr); return pos == iInvalidPos ? -1 : pos; }
    int lastIndexOf(char ch) const { size_t pos = lastIndexOf_String(&_str, ch); return pos == iInvalidPos ? -1 : pos; }
    int lastIndexOf(Char ch) const { size_t pos = lastIndexOf_String(&_str, ch); return pos == iInvalidPos ? -1 : pos; }
    int lastIndexOf(const char *cstr) const { size_t pos = lastIndexOfCStr_String(&_str, cstr); return pos == iInvalidPos ? -1 : pos;}

    bool containsWord(const String &word) const;

    inline bool operator==(const char *cstr) const { return compare(cstr) == 0; }
    inline bool operator!=(const char *cstr) const { return compare(cstr) != 0; }
    inline bool operator< (const char *cstr) const { return compare(cstr) <  0; }
    inline bool operator> (const char *cstr) const { return compare(cstr) >  0; }
    inline bool operator<=(const char *cstr) const { return compare(cstr) <= 0; }
    inline bool operator>=(const char *cstr) const { return compare(cstr) >= 0; }

    inline int compare(const char *cstr) const { return cmp_String(&_str, cstr); }
    inline int compare(const String &s) const { return cmpString_String(&_str, &s); }

    /**
     * Compare two strings (case sensitive).
     *
     * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
     *         Negative, if @a a < @a b.
     */
    dint compareWithCase(const String &str) const;

    /**
     * Compare two strings (case insensitive).
     *
     * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
     *         Negative, if @a a < @a b.
     */
    dint compareWithoutCase(const String &str) const;

    /**
     * Compare two strings (case insensitive), but only up to @a n characters.
     * @param str  Other string.
     * @param n    Maximum length to compare up to.
     * @return 0, if @a a and @a b are identical. Positive, if @a a > @a b.
     *         Negative, if @a a < @a b.
     */
    dint compareWithoutCase(const String &str, int n) const;

    /**
     * Compares two strings to see how many characters they have in common
     * starting from the left.
     *
     * @param str          String.
     * @param sensitivity  Case sensitivity.
     *
     * @return  Number of characters the two strings have in common from the left.
     */
    int commonPrefixLength(const String &str, CaseSensitivity sensitivity = CaseSensitive) const;

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
//    Q_DECLARE_FLAGS(IntConversionFlags, IntConversionFlag)

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
    dint toInt(bool *ok = nullptr, int base = 10, duint flags = AllowOnlyWhitespace) const;

    /**
     * Converts the string to a 32-bit unsigned integer. The behavior is the same as
     * QString::toUInt(), with the exception that the default is to autodetect the
     * base of the number.
     *
     * @param ok     @c true is returned via this pointer if the conversion was
     *               successful.
     * @param base   Base for the number.
     *
     * @return 32-bit unsigned integer parsed from the string (@c *ok set to true).
     * @c 0 if the conversion fails (@c *ok set to @c false).
     */
    duint32 toUInt32(bool *ok = nullptr, int base = 0) const;

    /**
     * Adds a prefix to each line in the text.
     *
     * @param prefix  Prefix text.
     *
     * @return Prefixed text.
     */
    String addLinePrefix(const String &prefix) const;

    /**
     * Prefixes double quotes and backslashes with a backslash.
     */
    String escaped() const;

    String truncateWithEllipsis(dsize maxLength) const;

    Block toPercentEncoding() const;

public:
    // Iterators:

    struct const_iterator
    {
        iStringConstIterator iter;

        operator Char() const { return iter.value; }
        Char operator*() const { return iter.value; }
        void operator++() { next_StringConstIterator(&iter); }
        void operator++(int) { next_StringConstIterator(&iter); }
        bool operator==(const const_iterator &other) const {
            return iter.str == other.str && iter.pos == other.pos;
        }
        bool operator!=(const const_iterator &other) const { return !(*this == other); }
    };

    struct const_reverse_iterator
    {
        iStringReverseConstIterator iter;

        operator Char() const { return iter.value; }
        Char operator*() const { return iter.value; }
        void operator++() { next_StringReverseConstIterator(&iter); }
        void operator++(int) { next_StringReverseConstIterator(&iter); }
        bool operator==(const const_reverse_iterator &other) const {
            return iter.str == other.str && iter.pos == other.pos;
        }
        bool operator!=(const const_reverse_iterator &other) const { return !(*this == other); }
    };

    const_iterator begin() const;
    const_iterator cbegin() const { return begin(); }
    const_reverse_iterator rbegin() const;
    const_reverse_iterator crbegin() const { return rbegin(); }

    const_iterator end() const;
    const_iterator cend() const { return end(); }
    const_reverse_iterator rend() const;
    const_reverse_iterator crend() const { return rend(); }

public:
    /**
     * Builds a String out of an array of bytes that contains a UTF-8 string.
     */
    static String fromUtf8(const IByteArray &byteArray);

//    static String fromUtf8(const QByteArray &byteArray);

    static String fromUtf8(const Block &block);

    static String fromUtf8(const char *nullTerminatedCStr);

    /**
     * Builds a String out of an array of bytes that contains a Latin1 string.
     */
    static String fromLatin1(const IByteArray &byteArray);

    /**
     * Builds a String out of an array of bytes using the IBM PC character set
     * (DOS code page 437).
     *
     * @param byteArray  Characters to convert.
     *
     * @return Converted string.
     */
    static String fromCP437(const IByteArray &byteArray);

    static String fromPercentEncoding(const Block &percentEncoded);

//    static dint compareWithCase(const QChar *a, const QChar *b, dsize count);

    /**
     * Checks if two strings are the same (case sensitive), up to @a count characters.
     * If the strings are longer than @a count, the remainder will be ignored in the
     * check.
     *
     * No deep copying occurs in this method.
     *
     * @param a      Null-terminated string.
     * @param b      Null-terminated string.
     * @param count  Maximum number of characters to check.
     *
     * @return @c true, if the strings are equal (excluding the part that is longer
     * than @a count).
     */
//    static bool equals(const QChar *a, const QChar *b, dsize count);

    /**
     * Advances the iterator until a nonspace character is encountered.
     *
     * @param i  Iterator to advance.
     * @param end  End of the string. Will not advance past this.
     */
    static void skipSpace(const_iterator &i, const const_iterator &end);

    /**
     * Formats a string using standard printf() formatting.
     *
     * @param format  Format string.
     *
     * @return Formatted output.
     */
    static String format(const char *format, ...);

    static String number(dint8 value)       { return String::format("%d", value); }
    static String number(dint16 value)      { return String::format("%d", value); }
    static String number(dint32 value)      { return String::format("%d", value); }
    static String number(dint64 value)      { return String::format("%lld", value); }
    static String number(duint8 value)      { return String::format("%u", value); }
    static String number(duint16 value)     { return String::format("%u", value); }
    static String number(duint32 value)     { return String::format("%u", value); }
    static String number(duint64 value)     { return String::format("%llu", value); }
    static String number(dfloat value)      { return String::format("%f", value); }
    static String number(ddouble int value) { return String::format("%f", value); }

    /**
     * Formats data according to formatting instructions. Outputs a
     * string containing the formatted data.
     *
     * Note that the behavior is different than the standard C printf() formatting.
     * For example, "%b" will format the number argument as boolean "True" or "False".
     * Use String::format() for normal printf() formatting.
     *
     * @param formatIter  Will be moved past the formatting instructions.
     *                    After the call, will remain past the end of
     *                    the formatting characters.
     * @param formatEnd   Iterator to the end of the format string.
     * @param arg         Argument to format.
     *
     * @return  Formatted argument as a string.
     */
    static String patternFormat(const_iterator &formatIter,
                                const const_iterator &formatEnd,
                                const IPatternArg &arg);

    static void advanceFormat(const_iterator &i,
                              const const_iterator &end);

    static String join(const List<String> &stringList, const String &sep = {});

private:
    iString _str;
};

inline String operator+(Char ch, const String &s) {
    return String(1, ch) + s;
}

inline String operator+(const char *cstr, const String &s) {
    return String(cstr) + s;
}

using StringList = List<String>;

/**
 * Support routine for determining the length of a null-terminated QChar*
 * string. Later versions have a QString constructor that needs no length
 * when constructing a string from QChar*.
 */
//size_t qchar_strlen(const QChar *str);

//inline StringList toStringList(const QStringList &qstr) {
//    return compose<StringList>(qstr.begin(), qstr.end());
//}

inline String operator / (const char *cstr, const String &str) {
    return String(cstr) / str;
}

//inline String operator / (Qconst String &qs, const String &str) {
//    return String(qs) / str;
//}

} // namespace de

#endif // LIBCORE_STRING_H
