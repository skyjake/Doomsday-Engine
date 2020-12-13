/** @file string.h  Multibyte string.
 *
 * Copyright © 2004-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/libcore.h"
#include "de/block.h"
#include "de/list.h"

#include <the_Foundation/string.h>
#include <ostream>

/**
 * Defines a static String for use as a global.
 *
 * Non-POD globals (such as a de::String) may not be initialized in the expected order (or at all)
 * in libraries, so this should be used instead. Note that it defines a static function instead of
 * a plain static variable.
 */
#define DE_STATIC_STRING(Name, ...) \
    static const de::String &Name() { static const de::String s{__VA_ARGS__}; return s; }

namespace de {

template <typename> struct Range;
class RegExp;
class CString;
class String;
class Path;

enum CaseSensitivity { CaseInsensitive, CaseSensitive };

struct DE_PUBLIC Sensitivity
{
    CaseSensitivity cs;

    inline Sensitivity(CaseSensitivity cs = CaseSensitive) : cs(cs) {}
    inline operator CaseSensitivity() const { return cs; }
    inline operator const iStringComparison *() const {
        return cs == CaseSensitive ? &iCaseSensitive : &iCaseInsensitive;
    }
};

enum PositionType { ByteOffset, CharacterOffset };

template <PositionType PosType>
struct StronglyTypedPosition
{
    dsize index;

    using Pos = StronglyTypedPosition<PosType>;

    static constexpr PositionType type = PosType;
    static constexpr dsize        npos = std::numeric_limits<dsize>::max();

    explicit constexpr StronglyTypedPosition(dsize i = npos)
        : index(i)
    {}

    explicit operator bool() const { return index != npos; }

    inline bool operator==(dsize i) const   { return index == i; }
    inline bool operator!=(dsize i) const   { return index != i; }
    inline bool operator<(dsize i) const    { if (index == npos || i == npos) return false; return index < i; }
    inline bool operator>(dsize i) const    { if (index == npos || i == npos) return false; return index > i; }
    inline bool operator<=(dsize i) const   { if (index == npos || i == npos) return false; return index <= i; }
    inline bool operator>=(dsize i) const   { if (index == npos || i == npos) return false; return index >= i; }

    inline bool operator==(Pos i) const { return index == i.index; }
    inline bool operator!=(Pos i) const { return index != i.index; }
    inline bool operator<(Pos i) const  { if (index == npos || i.index == npos) return false; return index < i.index; }
    inline bool operator>(Pos i) const  { if (index == npos || i.index == npos) return false; return index > i.index; }
    inline bool operator<=(Pos i) const { if (index == npos || i.index == npos) return false; return index <= i.index; }
    inline bool operator>=(Pos i) const { if (index == npos || i.index == npos) return false; return index >= i.index; }

    Pos  operator-(long sub) const { return Pos{index != npos ? index - sub : npos}; }
    Pos  operator+(long sub) const { return Pos{index != npos ? index + sub : npos}; }
    Pos &operator+=(long sub)      { if (index != npos) { index += sub; } return *this; }
    Pos &operator-=(long sub)      { if (index != npos) { index -= sub; } return *this; }

    Pos &operator+=(const Pos &p) { if (index != npos && p.index != npos) { index += p.index; } return *this; }

    Pos  operator+(const Pos &p) const { return Pos{index != npos ? index + p.index : npos}; }
    Pos  operator-(Pos p) const        { return Pos(index != npos ? index - p.index : npos); }
    Pos  operator++(int) { Pos p = *this; if (index != npos) index++; return p; }
    Pos  operator--(int) { Pos p = *this; if (index != npos) index--; return p; }
    Pos &operator++() { if (index != npos) index++; return *this; }
    Pos &operator--() { if (index != npos) index--; return *this; }
};

using BytePos = StronglyTypedPosition<ByteOffset>;

/**
 * Character index. A single character may be composed of multiple bytes.
 */
using CharPos = StronglyTypedPosition<CharacterOffset>;

/**
 * Multibyte character iterator.
 */
struct DE_PUBLIC mb_iterator {
    const char *      cur;
    const char *      start;
    mutable mbstate_t mb{};

    mb_iterator() : cur{nullptr}, start{nullptr} {}
    mb_iterator(const char *p) : cur{p}, start{p} {}
    mb_iterator(const char *p, const char *start) : cur{p}, start{start} {}
    mb_iterator(const String &str);
    mb_iterator(const mb_iterator &other)
        : cur(other.cur)
        , start(other.start)
    {}

    mb_iterator &operator=(const mb_iterator &other)
    {
        cur = other.cur;
        start = other.start;
        mb = other.mb;
        return *this;
    }

    operator const char *() const { return cur; }
    Char operator*() const;
    mb_iterator &operator++();
    mb_iterator operator++(int);
    mb_iterator &operator--();
    mb_iterator operator--(int);
    mb_iterator operator+(int offset) const;
    mb_iterator &operator+=(int offset);
    mb_iterator operator-(int offset) const;
    mb_iterator &operator-=(int offset);
    bool operator==(const char *ptr) const { return cur == ptr; }
    bool operator!=(const char *ptr) const { return cur != ptr; }
    bool operator==(const mb_iterator &other) const { return cur == other.cur; }
    bool operator!=(const mb_iterator &other) const { return !(*this == other); }
    explicit operator bool() const { return cur != nullptr; }

    BytePos pos() const { return BytePos(cur - start); }
    BytePos pos(const char *reference) const { return BytePos(cur - reference); }
    BytePos pos(const String &reference) const;

    Char decode(const char **end = nullptr) const;
};

/**
 * Multibyte string.
 *
 * Supports byte access via IByteArray. The default character encoding is UTF-8.
 *
 * @ingroup types
 */
class DE_PUBLIC String : public IByteArray
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
        virtual ~IPatternArg() = default;

        /// Returns the value of the argument as a text string.
        virtual String asText() const = 0;

        /// Returns the value of the argument as a number.
        virtual ddouble asNumber() const = 0;
    };

    using PatternArgs = List<const IPatternArg *>;

    /**
     * Comparator for case-insensitive container keys.
     */
    struct DE_PUBLIC InsensitiveLessThan
    {
        inline bool operator()(const String &a, const String &b) const
        {
            return a.compareWithoutCase(b) < 0;
        }
    };

public:
    using size_type = dsize;
    using ByteRange = Range<BytePos>;
    using CharRange = Range<CharPos>;

    static constexpr dsize npos = std::numeric_limits<dsize>::max();

public:
    String();
    String(const String &other);
    String(String &&moved);
    String(const Block &bytes);
    String(const iBlock *bytes);
    String(const iString *other);
    String(const std::string &text);
    String(const std::wstring &text);
    String(const char *nullTerminatedCStr);
    String(const char *cStr, int length);
    String(const char *cStr, dsize length);
    String(dsize length, char ch);
    String(dsize length, Char ch);
    String(const char *start, const char *end);
    String(const Range<const char *> &range);
    String(const CString &cstr);
    String(const std::string &str, dsize index, dsize length);

    template <typename Iterator>
    inline String(Iterator start, Iterator end)
    {
        init_String(&_str);
        for (Iterator i = start; i != end; ++i)
        {
            push_back(*i);
        }
    }

    ~String() override;

    inline String &operator=(const String &other)
    {
        set_String(&_str, &other._str);
        return *this;
    }

    inline String &operator=(String &&moved)
    {
        set_String(&_str, &moved._str);
        return *this;
    }

    inline BytePos sizeb() const { return BytePos{size_String(&_str)}; }
    inline dint    sizei() const { return dint(size_String(&_str)); }
    inline dsize   sizeu() const { return size_String(&_str); }
    inline CharPos sizec() const { return CharPos(length()); }

    /// Calculates the length of the string in characters.
    inline dsize length() const { return length_String(&_str); }
    inline dint lengthi() const { return dint(length_String(&_str)); }

    void resize(size_t newSize);

    /// Determines whether the string is empty.
    inline bool empty() const { return sizeu() == 0; }
    inline bool isEmpty() const { return empty(); }
    inline explicit operator bool() const { return !empty(); }

    inline char *writablePointer(BytePos offset = BytePos(0))
    {
        return static_cast<char *>(data_Block(&_str.chars)) + offset.index; // detaches
    }

    inline const char *c_str() const { return cstr_String(&_str); }
    inline const char *data() const { return c_str(); }
    inline operator iRangecc() const { return iRangecc{data(), data() + size()}; }

    inline operator const char *() const { return c_str(); }
    inline operator const iString *() const { return &_str; }
    inline iString *i_str() { return &_str; }
    inline operator std::string() const { return toStdString(); }

    inline std::string toStdString() const
    {
        return {constBegin_String(&_str), constEnd_String(&_str)};
    }
    std::wstring toWideString() const;
    CString toCString() const;

    void clear();

    /// Returns the first character of the string.
    Char first() const;

    /// Returns the last character of the string.
    Char last() const;

    bool contains(char c) const;
    bool contains(Char c) const;
    bool contains(const char *cStr, Sensitivity cs = CaseSensitive) const;
    inline bool contains(const String &str, Sensitivity cs = CaseSensitive) const
    {
        return contains(str.c_str(), cs);
    }
    int count(char ch) const;

    bool beginsWith(const String &s, Sensitivity cs = CaseSensitive) const
    {
        return startsWithSc_String(&_str, s, cs);
    }
    bool beginsWith(char ch, Sensitivity cs = CaseSensitive) const
    {
        const char c[2] = {ch, 0};
        return beginsWith(c, cs);
    }
    bool beginsWith(const char *cstr, Sensitivity cs = CaseSensitive) const
    {
        return startsWithSc_String(&_str, cstr, cs);
    }
    bool beginsWith(Char ch, Sensitivity cs = CaseSensitive) const;
    bool endsWith(char ch, Sensitivity cs = CaseSensitive) const
    {
        const char c[2] = {ch, 0};
        return endsWith(c, cs);
    }
    bool endsWith(const char *cstr, Sensitivity cs = CaseSensitive) const
    {
        return endsWithSc_String(&_str, cstr, cs);
    }
    inline bool endsWith(const String &str, Sensitivity cs = CaseSensitive) const
    {
        return endsWith(str.c_str(), cs);
    }

    inline char operator[](BytePos pos) const { return c_str()[pos.index]; }
    char at(BytePos pos) const { DE_ASSERT(pos < sizeu()); return c_str()[pos.index]; }

    Char at(CharPos pos) const; // Slow! Don't use this.

    inline String mid(CharPos pos, dsize charCount = npos) const { return substr(pos, charCount); }
    String        substr(CharPos pos, dsize charCount = npos) const;
    String        substr(const Range<CharPos> &charRange) const;
    String        substr(BytePos pos, dsize charCount = npos) const;
    inline String substr(BytePos pos, BytePos charCount) const { return substr(pos, charCount.index); }
    String        substr(const Range<BytePos> &byteRange) const;
    String        left(BytePos count) const { return substr(BytePos{0}, count.index); }
    String        left(CharPos count) const { return substr(CharPos{0}, count.index); }
    String        right(BytePos count) const { return substr(sizeb() - count); }
    String        right(CharPos count) const;
    String        remove(BytePos count) const { return substr(count); }
    String        remove(CharPos count) const { return substr(count); }
    void          remove(BytePos start, dsize byteCount);
    inline void   remove(BytePos start, BytePos count) { remove(start, count.index); }
    void          remove(BytePos start, CharPos charCount);
    void          truncate(BytePos pos);
    inline void   truncate(CharPos pos) { *this = substr(CharPos(0), pos.index); }
    List<String>  split(const char *separator) const;
    List<String>  split(Char ch) const;
    List<String>  split(const String &separator) const { return split(separator.c_str()); }
    List<String>  split(const RegExp &regExp) const;
    List<CString> splitRef(const char *separator) const;
    List<CString> splitRef(Char ch) const;

    String        operator+(char *) const;
    String        operator+(const char *) const;
    String        operator+(const CString &) const;
    String        operator+(const std::string &s) const;
    String operator+(const String &other) const;

    String &operator+=(char ch);
    String &operator+=(Char ch);
    String &operator+=(const char *);
    String &operator+=(const CString &);
    String &operator+=(const String &);

    String &append(char ch) { return *this += ch; }
    String &append(Char ch) { return *this += ch; }
    String &append(const char *s) { return *this += s; }
    String &append(const String &s) { return *this += s; }

    String &prepend(Char ch);
    String &prepend(const String &other);

    inline void push_front(Char ch) { prepend(ch); }
    inline void push_back(Char ch) { append(ch); }

    void insert(BytePos pos, const char *cStr);
    void insert(BytePos pos, const String &str);
    String &replace(Char before, Char after);
    String &replace(const CString &before, const CString &after);
    String &replace(const RegExp &before, const CString &after);

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

    String operator/(const CString &path) const;

    String operator/(const char *path) const;

    String operator/(const Path &path) const;

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
    CString fileName(Char dirChar = '/') const;

    /// Extracts the base name from the string (does not include extension).
    CString fileNameWithoutExtension() const;

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
    CString fileNameExtension() const;

    /// Extracts the path of the string.
    CString fileNamePath(Char dirChar = '/') const;

    /// Extracts everything but the extension from string.
    String fileNameAndPathWithoutExtension(Char dirChar = '/') const;

    BytePos indexOf(char ch) const { return BytePos{indexOf_String(&_str, ch)}; }
    BytePos indexOf(Char ch) const { return BytePos{indexOf_String(&_str, ch.unicode())}; }
    BytePos indexOf(const char *cstr) const { return BytePos{indexOfCStr_String(&_str, cstr)}; }
    BytePos indexOf(const char *cstr, BytePos from) const { return BytePos{indexOfCStrFrom_String(&_str, cstr, from.index)}; }
    BytePos indexOf(const char *cstr, Sensitivity s) const { return BytePos(indexOfCStrFromSc_String(&_str, cstr, 0, s)); }
    inline BytePos indexOf(const String &str) const { return indexOf(str.c_str()); }
    BytePos lastIndexOf(char ch) const { return BytePos{lastIndexOf_String(&_str, ch)}; }
    BytePos lastIndexOf(Char ch) const { return BytePos{lastIndexOf_String(&_str, ch.unicode())}; }
    BytePos lastIndexOf(const char *cstr) const { return BytePos{lastIndexOfCStr_String(&_str, cstr)}; }

    bool containsWord(const String &word) const;

    inline bool operator==(const char *cstr) const { return compare(cstr) == 0; }
    inline bool operator!=(const char *cstr) const { return compare(cstr) != 0; }
    inline bool operator< (const char *cstr) const { return compare(cstr) <  0; }
    inline bool operator> (const char *cstr) const { return compare(cstr) >  0; }
    inline bool operator<=(const char *cstr) const { return compare(cstr) <= 0; }
    inline bool operator>=(const char *cstr) const { return compare(cstr) >= 0; }

    bool        operator==(const CString &cstr) const;
    inline bool operator!=(const CString &cstr) const { return !(*this == cstr); }

    inline bool operator==(const String &str) const { return compare(str) == 0; }
    inline bool operator!=(const String &str) const { return !(*this == str); }

    inline int compare(const char *cstr, Sensitivity cs = CaseSensitive) const
    {
        return cmpSc_String(&_str, cstr, cs);
    }
    int        compare(const CString &str, Sensitivity cs = CaseSensitive) const;
    inline int compare(const String &s, Sensitivity cs = CaseSensitive) const
    {
        return cmpStringSc_String(&_str, &s._str, cs);
    }

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
    CharPos commonPrefixLength(const String &str, Sensitivity sensitivity = CaseSensitive) const;

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
    enum IntConversionFlag { AllowOnlyWhitespace = 0x0, AllowSuffix = 0x1 };

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

    long    toLong(bool *ok = nullptr, int base = 0) const;
    dfloat  toFloat() const;
    ddouble toDouble() const;

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

    Block toUtf16() const;

    // Implements IByteArray.
    Size size() const override { return size_String(&_str); }
    void get(Offset at, Byte *values, Size count) const override;
    void set(Offset at, const Byte *values, Size count) override;

public:
    // Iterators.
    using const_iterator = mb_iterator;
    struct DE_PUBLIC const_reverse_iterator
    {
        mb_iterator iter;

        const_reverse_iterator(const mb_iterator &mbi) : iter(mbi) {}
        const_reverse_iterator(const Range<const char *> &range);
        const_reverse_iterator(const CString &cstr);
        const_reverse_iterator(const String &str);

        operator const char *() const { return iter; }

        Char operator *() const { return *iter; }

        inline const_reverse_iterator &operator++()
        {
            --iter;
            return *this;
        }
        inline const_reverse_iterator &operator--()
        {
            ++iter;
            return *this;
        }
        inline const_reverse_iterator operator++(int)
        {
            const_reverse_iterator i = *this;
            --iter;
            return i;
        }
        inline const_reverse_iterator operator--(int)
        {
            const_reverse_iterator i = *this;
            ++iter;
            return i;
        }
        inline const_reverse_iterator &operator+=(int off)
        {
            if (off > 0)
            {
                while (off-- > 0) ++*this;
            }
            else
            {
                while (off++ < 0) --*this;
            }
            return *this;
        }
        inline const_reverse_iterator &operator-=(int off)
        {
            (*this) += -off;
            return *this;
        }
        inline bool operator==(const char *ptr) const { return iter == ptr; }
        inline bool operator!=(const char *ptr) const { return iter != ptr; }
        inline bool operator==(const mb_iterator &other) const { return iter == other; }
        inline bool operator!=(const mb_iterator &other) const { return !(*this == other); }

        BytePos pos() const { return iter.pos(); }
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
    static String take(iString *str);
    static String take(iBlock *data);

    /**
     * Builds a String out of an array of bytes that contains a UTF-8 string.
     */
    static String fromUtf8(const IByteArray &byteArray);

    static String fromUtf8(const Block &block);

    static String fromUtf8(const char *nullTerminatedCStr);

    static String fromUtf16(const Block &utf16);

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

    static String asText(dint8 value)   { return format("%d", value); }
    static String asText(dint16 value)  { return format("%d", value); }
    static String asText(dint32 value)  { return format("%d", value); }
    static String asText(dint64 value)  { return format("%lld", value); }
    static String asText(duint8 value)  { return format("%u", value); }
    static String asText(duint16 value) { return format("%u", value); }
    static String asText(duint32 value) { return format("%u", value); }
    static String asText(duint64 value) { return format("%llu", value); }
    static String asText(dfloat value)  { return format("%f", double(value)); }
    static String asText(dfloat value, int precision);
    static String asText(ddouble value) { return format("%f", value); }
    static String asText(char value)    { return format("%c", value); }
    static String asText(Char value)
    {
        iMultibyteChar mb;
        init_MultibyteChar(&mb, value.unicode());
        return asText(mb.bytes);
    }
    static String asText(const char *value) { return format("%s", value); }

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

    static String join(const List<String> &stringList, const char *sep = "");

    static bool contains(const List<String> &stringList,
                         const char *        str,
                         Sensitivity         s = CaseSensitive);

private:
    iString _str;
};

using StringList = List<String>;

inline StringList makeList(int count, const char * const *strings)
{
    StringList list;
    for (int i = 0; i < count; ++i)
    {
        list << String(strings[i]);
    }
    return list;
}

inline bool operator>=(int a, const BytePos &b)
{
    return a >= int(b.index);
}

inline bool operator<=(int a, const BytePos &b)
{
    return a <= int(b.index);
}

inline String operator+(Char ch, const String &s)
{
    return String(1, ch) + s;
}

inline String operator+(const char *left, const String &right)
{
    return String(left) + right;
}

inline const char *operator+(const char *cStr, const BytePos &offset)
{
    return cStr + offset.index;
}

inline String operator/(const char *cStr, const String &str)
{
    return String(cStr) / str;
}

inline std::ostream &operator<<(std::ostream &os, const String &str)
{
    os.write(str, str.sizei());
    return os;
}

template <typename... Args>
inline String Stringf(const char *format, Args... args)
{
    return String::format(format, args...);
}

inline bool operator!(const String &str)
{
    return str.empty();
}

} // namespace de

namespace std
{
    template<>
    struct hash<de::String> {
        std::size_t operator()(const de::String &key) const {
            return hash<std::string>()(key.toStdString());
        }
    };
}

#endif // LIBCORE_STRING_H
