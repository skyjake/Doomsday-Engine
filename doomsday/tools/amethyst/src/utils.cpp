/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "utils.h"
#include "gem.h"
#include "defs.h"
#include "outputcontext.h"
#include "exception.h"
#include <QFile>
#include <QDateTime>
#include <QRegExp>
#include <QDebug>

String stringCase(const String& src, StringCasing casing)
{
    String dest;
    bool firstInWord = true;
    bool firstAlnum = true;
    const int len = src.size();
    
    // Move past spaces in the beginning.
    int i = 0;
    while(i < len && src[i].isSpace()) ++i;

    for(; i < len; i++)
    {
        // Choose the appropriate case.
        if((casing == CAPITALIZE_WORDS && firstInWord) ||
           (casing == CAPITALIZE_SENTENCE && firstAlnum) ||
           casing == UPPERCASE_ALL)
        {
            dest += src[i].toUpper();
        }
        else if(casing == LOWERCASE_ALL || firstInWord)
        {
            dest += src[i].toLower();
        }
        else
        {
            dest += src[i];
        }

        if(src[i].isLetterOrNumber())
        {
            firstInWord = false;
            firstAlnum = false;
        }
        else if(src[i].isSpace())
        {
            firstInWord = true;
        }
    }
    return dest;
}

String stringInterlace(const String& src, QChar c)
{
    String dest;
    int len = src.size();
    
    for(int i = 0; i < len; i++)
    {
        dest += src[i];
        if(i < len - 1) dest += c;
    }
    return dest;
}

bool cutBefore(String &str, const String& pat)
{
    int pos = str.indexOf(pat);
    if(pos < 0) return false;
    str.remove(0, pos);
    return true;
}

bool cutAfter(String &str, const String& pat)
{
    int pos = str.indexOf(pat);
    if(pos < 0) return false;
    pos += pat.size();
    str.remove(pos, str.size() - pos);
    return true;
}

/**
 * Zero corresponds 'A'.
 */
String alphaCounter(int num)
{
    String out;
    const int numLetters = 'z' - 'a' + 1;

    for(;;)
    {
        out = char('a' + num%numLetters) + out;
        if(num < numLetters) break;
        num /= numLetters;
        num--;
    }
    return out;
}

/**
 * The number must be greater than zero.
 */
String romanCounter(int num)
{
    String out;
    struct { int value; char symb; int unit; } symbols[] =
    {
        { 1000, 'm', 100 },
        { 500, 'd', 100 },
        { 100, 'c', 10 },
        { 50, 'l', 10 },
        { 10, 'x', 1 },
        { 5, 'v', 1 },
        { 1, 'i', 0 }
    };
    int i, numSymbols = sizeof(symbols)/sizeof(symbols[0]);

    while(num > 0)
    {
        for(i = 0; i < numSymbols; i++)
        {
            if(num >= symbols[i].value)
            {
                out += symbols[i].symb;
                num -= symbols[i].value;
                break;
            }
            if(symbols[i].unit > 0 
                && num >= symbols[i].value - symbols[i].unit)
            {
                for(int k = 0; k < numSymbols; k++)
                    if(symbols[i].unit == symbols[k].value)
                    {
                        out += symbols[k].symb;
                        break;
                    }
                out += symbols[i].symb;
                num -= symbols[i].value - symbols[i].unit;
                break;
            }
        }
    }
    return out;
}

/**
 * All numbers become roman numerals.
 */
String romanFilter(const String& src, bool upperCase)
{
    String dest;

    for(int i = 0; i < src.size(); )
    {
        if(!src[i].isDigit())
        {
            dest += src[i++];
            continue;
        }

        // Found a number. Read all the digits.
        int start = i;
        while(i < src.size() && src.at(i).isDigit()) ++i;
        int number = src.mid(start, i).toInt();
        dest += stringCase(romanCounter(number), upperCase? UPPERCASE_ALL : LOWERCASE_ALL);
    }
    return dest;
}

String htmlTagFilter(const String& input)
{
    String out = input;
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    return out;
}

String hmtlCharacterFilter(const String& input)
{
    String out = input;
    out.replace("&", "&amp;");
    out.replace("\"", "&quot;");
    out.replace(QString::fromWCharArray(L"ä"), "&auml;");
    out.replace(QString::fromWCharArray(L"Ä"), "&Auml;");
    out.replace(QString::fromWCharArray(L"ö"), "&ouml;");
    out.replace(QString::fromWCharArray(L"Ö"), "&Ouml;");
    out.replace(QString::fromWCharArray(L"å"), "&aring;");
    out.replace(QString::fromWCharArray(L"Å"), "&Aring;");
    return out;
}

int findAnyMarker(String str, int start = 0, const char* except = 0)
{
    const char* markers[] = { "@<", "@>", "@|", "@[", "@]", 0 };
    int found = -1;

    for(const char** marker = markers; *marker; marker++)
    {
        if(except && QLatin1String(except) == QLatin1String(*marker)) continue;
        int pos = str.indexOf(*marker, start);
        if(pos >= 0)
        {
            if(found < 0 || pos < found)
            {
                // Closer to the beginning.
                found = pos;
            }
        }
    }
    return found;
}

String findMarkedSegment(String segmented, const String& marker)
{
    if(!segmented.contains(marker)) return ""; // Not present.

    int pos = segmented.indexOf(marker);
    int end = findAnyMarker(segmented, pos + marker.size());
    if(end < 0) end = segmented.size();
    return segmented.mid(pos + marker.size(), end - pos - marker.size());
}

String cutMarkedSegments(String segmented)
{
    forever
    {
        int pos = findAnyMarker(segmented, 0, "@|");
        if(pos < 0) break;

        int end = findAnyMarker(segmented, pos + 2);
        if(end < 0) end = segmented.size();
        segmented.remove(pos, end - pos);
    }
    return segmented;
}

String applyFilter(String input, const String& filter, FilterApplyMode mode, Gem *gem)
{
    String output, fmt = filter;
    bool escape = false; 
    typedef QPair<QString, QString> Replacement;
    QList<Replacement> replacements;
    static int xVar = 0;

    enum SuperMode {
        NONE,
        REPEAT_CHAR,
        UNDERLINE_REPEAT_CHAR,
        INTERLACE,
        LINE_PREFIX,
        REPLACE_STRING,
        EVALUATE
    };
    SuperMode superEscape = NONE;

    // Filter out the parts of the format string that do not belong
    // in this mode.
    switch(mode)
    {
    case ApplyPre:
        fmt = findMarkedSegment(fmt, "@<");
        break;

    case ApplyPost:
        fmt = findMarkedSegment(fmt, "@>");
        break;

    case ApplyAnchorPrepend:
        fmt = findMarkedSegment(fmt, "@]");
        break;

    case ApplyAnchorAppend:
        fmt = findMarkedSegment(fmt, "@[");
        break;

    default:
        fmt = cutMarkedSegments(fmt);
        break;
    }

    while(!fmt.isEmpty())
    {
        QChar c = fmt[0];
        fmt.remove(0, 1);
        if(!escape)
        {
            if(c == '@') 
                escape = true;
            else
                output += c;
            continue;
        }
        // Escape characters do most of the work.
        if(superEscape)
        {
            // Super sequences have three chars.
            switch(superEscape)
            {
            case REPEAT_CHAR:
                output += char(OutputContext::CtrlFill);
                output += c;
                break;

            case UNDERLINE_REPEAT_CHAR:
                output += char(OutputContext::CtrlUnderfill);
                output += c;
                break;

            case INTERLACE:
                output += stringInterlace(input, c);
                break;

            case LINE_PREFIX:
            {
                // The prefix is enclosed in parentheses.
                output += char(OutputContext::CtrlLinePrefixBegin);
                if(c != '(')
                {
                    throw Exception("Line prefix must be enclosed in parentheses.", "applyFilter");
                }
                bool esc = false;
                while(!fmt.isEmpty())
                {
                    c = fmt[0];
                    fmt.remove(0, 1);
                    if(!esc)
                    {
                        if(c == ')') // End?
                            break;
                        if(c == '@') // Escape?
                            esc = true;
                        else
                            output += c;
                    }
                    else
                    {
                        if(c == '_') c = ' '; // Space.
                        if(c == 't') c = '\t'; // Breaking space.
                        if(c == 'n') c = OutputContext::CtrlLineBreak;
                        if(c == 'N') c = OutputContext::CtrlParagraphBreak;
                        output += c;
                        esc = false;
                    }
                }
                if(c != ')')
                {
                    throw Exception("Line prefix must be enclosed in parentheses.", "applyFilter");
                }
                output += char(OutputContext::CtrlLinePrefixEnd);
                break;
            }

            case REPLACE_STRING:
            {
                if(c != '(')
                {
                    throw Exception("String replacement must be enclosed in parentheses.", "applyFilter");
                }
                QString key;
                QString value;
                QString* current = &key;
                bool esc = false;
                while(!fmt.isEmpty())
                {
                    c = fmt[0];
                    fmt.remove(0, 1);
                    if(!esc)
                    {
                        if(c == '|') // Switch to value?
                        {
                            if(current == &value)
                            {
                                throw Exception("String replacement cannot have more than one output value.",
                                                "applyFilter");
                            }
                            current = &value;
                        }
                        else if(c == ')') // End?
                            break;
                        else if(c == '@') // Escape?
                            esc = true;
                        else
                            *current += c;
                    }
                    else
                    {
                        if(c == '_') c = ' '; // Space.
                        if(c == 't') c = '\t'; // Breaking space.
                        if(c == 'n') c = OutputContext::CtrlLineBreak;
                        if(c == 'N') c = OutputContext::CtrlParagraphBreak;
                        *current += c;
                        esc = false;
                    }
                }
                if(c != ')')
                {
                    throw Exception("String replacement must be enclosed in parentheses.", "applyFilter");
                }
                replacements << Replacement(key, value);
                break;
            }

            case EVALUATE: // Integer evaluation.
            {
                // Examples:
                // @v( 10+20 ) => 30
                // @v( @w * @c ) => <width>*<counter>
                // @v( 1+3*5 ) => 20 (simple left-to-right; no precedence rules)
                if(c != '(')
                {
                    throw Exception("Expression must be enclosed in parentheses.", "applyFilter");
                }
                bool esc = false;
                enum Operator {
                    NONE,
                    PLUS,
                    MINUS,
                    MULTIPLY,
                    DIVIDE,
                    MODULO,
                    ASSIGNMENT
                };
                QString curNumber;
                Operator curOp = NONE;
                int result = 0;
                bool quiet = false; // Quiet will not output anything.
                while(!fmt.isEmpty())
                {
                    c = fmt[0];
                    fmt.remove(0, 1);
                    if(!esc)
                    {
                        if(c == '@') // Escape?
                            esc = true;
                        else
                        {
                            Operator nextOp = NONE;
                            if(c.isDigit())
                            {
                                curNumber += c;
                            }
                            else if(c == 'x' && curNumber.isEmpty())
                            {
                                curNumber = QString::number(xVar);
                            }
                            else if(c == '+')
                            {
                                nextOp = PLUS;
                            }
                            else if(c == '-')
                            {
                                nextOp = MINUS;
                            }
                            else if(c == '*')
                            {
                                nextOp = MULTIPLY;
                            }
                            else if(c == '/')
                            {
                                nextOp = DIVIDE;
                            }
                            else if(c == '%')
                            {
                                nextOp = DIVIDE;
                            }
                            else if(c == '=')
                            {
                                nextOp = ASSIGNMENT;
                            }
                            else if(c != ')' && !c.isSpace())
                            {
                                throw Exception("Unrecognized character in expression.", "applyFilter");
                            }

                            if(c == ')' || nextOp != NONE)
                            {
                                if(curOp == NONE)
                                {
                                    // The first operand.
                                    result = curNumber.toInt();
                                    curNumber.clear();
                                }
                                else
                                {
                                    int num = curNumber.toInt();
                                    curNumber.clear();
                                    // Apply operator.
                                    switch(curOp)
                                    {
                                    case PLUS:          result += num; break;
                                    case MINUS:         result -= num; break;
                                    case MULTIPLY:      result *= num; break;
                                    case DIVIDE:        result /= num; break;
                                    case MODULO:        result %= num; break;
                                    case ASSIGNMENT:    xVar = result; break;
                                    case NONE: break;
                                    }
                                }
                                curOp = nextOp;
                            }
                            if(c == ')') break; // The end.
                        }
                    }
                    else
                    {
                        if(c == 'w') curNumber = QString::number(gem->width());
                        if(c == 'c') curNumber = QString::number(gem->order() + 1);
                        if(c == '.') quiet = true;
                        esc = false;
                    }
                }
                if(c != ')')
                {
                    throw Exception("Expression must be enclosed in parentheses.", "applyFilter");
                }
                if(!quiet)
                {
                    output += QString::number(result);
                }
                break;
            }

            default:
                break;
            }
            superEscape = NONE;
            escape = false;
            continue;
        }
        // Normal 2-char escape.
        switch(c.toAscii())
        {
        case '@':
        case '{':
        case '}':
            output += c;
            break;

        case '.':
            // Nothing.
            break;

        case '=':       // The unmodified text of the gem.
            output += input;
            break;

        case '_':       // Non-breaking space.
            output += ' ';
            break;

        case '\\':      // Pipe. Output becomes input.
            input = output;
            output = "";
            break;

        case '&':       // Anchor.
            output += OutputContext::CtrlAnchor;
            break;

        case 'a':       // Alphabetic counter.      
        case 'A':
            output += stringCase(alphaCounter(gem->order()), c == 'A'? UPPERCASE_ALL : LOWERCASE_ALL);
            break;

        case 'b':       // Break control.
        case 'B':
            output += char(c == 'B'? OutputContext::CtrlRawBreaks : OutputContext::CtrlCleanBreaks);
            break;

        case 'c':       // Counter.
            output += QString::number(gem->order() + 1);
            break;
        
        case 'd':       // Flush left.
            output += char(OutputContext::CtrlAlign);
            output += 'L';
            break;

        case 'D':       // Format as a timestamp.
            output += dateString(input);
            break;

        case 'e':       // Flush center.
            output += char(OutputContext::CtrlAlign);
            output += 'C';
            break;

        case 'f':       // Flush right.
            output += char(OutputContext::CtrlAlign);
            output += 'R';
            break;

        case 'g':       // Full HTML filter.
            output += htmlTagFilter(hmtlCharacterFilter(input));
            break;

        case 'h':       // Entity refs for special characters (&amp;).
            output += hmtlCharacterFilter(input);
            break;

        case 'H':       // <, > HTML filter.
            output += htmlTagFilter(input);
            break;

        case 'i':       // Interlace (with spaces).
            output += stringInterlace(input);
            break;

        case 'I':       // Super interlace.
            superEscape = INTERLACE;
            break;

        case 'l':       // Lowercase.
            output += stringCase(input, LOWERCASE_ALL);
            break;

        case 'L':       // Lowercase, except for the first letter, which is uppercased.
            output += stringCase(input, CAPITALIZE_SENTENCE);
            break;

        case 'n':       // Break current line.
            output += OutputContext::CtrlLineBreak;
            break;

        case 'N':       // Force one empty line.
            output += OutputContext::CtrlParagraphBreak;
            break;

        case 'p':       // Line prefix.
            superEscape = LINE_PREFIX;
            break;

        case 'r':       // Repeater.
            superEscape = REPEAT_CHAR;
            break;

        case 'R':       // Underline repeater.
            superEscape = UNDERLINE_REPEAT_CHAR;
            break;

        case 's':       // String replacement mapping.
            superEscape = REPLACE_STRING;
            break;

        case 't':
            output += OutputContext::CtrlBreakingSpace;
            break;

        case 'T':
            output += OutputContext::CtrlTab;
            break;

        case 'u':       // Uppercase.
            output += stringCase(input, UPPERCASE_ALL);
            break;

        case 'U':       // First-letter case.
            output += stringCase(input, CAPITALIZE_WORDS);
            break;

        case 'v':       // Evaluate numeric expression.
            superEscape = EVALUATE;
            break;

        case 'w':       // Gem width as a number.
            output += QString::number(gem->width());
            break;

        case 'x':       // Roman counter.
        case 'X':
            output += stringCase(romanCounter(gem->order() + 1), c == 'X'? UPPERCASE_ALL : LOWERCASE_ALL);
            break;

        case 'y':       // Roman filter.
        case 'Y':
            output += romanFilter(input, c == 'Y');
            break;

        default:
            // Strange escape sequences are just skipped.
            break;
        }
        if(!superEscape) escape = false;
    }

    // Finally apply the defined replacements.
    foreach(const Replacement& r, replacements)
    {
        output.replace(r.first, r.second);
    }

    return output;          
}

ArgType interpretArgType(const String& types, int index)
{
    if(types.isEmpty()) return ArgShard;
    if(index < 0) index = 0;
    if(index >= int(types.size())) index = types.size() - 1;
        
    QChar t = types[index];
    if(t == 't') return ArgToken;
    if(t == 'b') return ArgBlock;
    return ArgShard;
}

static struct { const char *condition; int flag; } gemFlags[] =
{
    { "em", GSF_EMPHASIZE },
    { "def", GSF_DEFINITION },
    { "code", GSF_CODE },
    { "kbd", GSF_KEYBOARD },
    { "samp", GSF_SAMPLE },
    { "var", GSF_VARIABLE },
    { "file", GSF_FILE },
    { "opt", GSF_OPTION },
    { "cmd", GSF_COMMAND },
    { "cite", GSF_CITE },
    { "acro", GSF_ACRONYM },
    { "url", GSF_URL },
    { "email", GSF_EMAIL },
    { "strong", GSF_STRONG },
    { "enum", GSF_ENUMERATE },
    { "header", GSF_HEADER },
    { "linebreak", GSF_BREAK_LINE },
    { "single", GSF_SINGLE },
    { "double", GSF_DOUBLE },
    { "thick", GSF_THICK },
    { "thin", GSF_THIN },
    { "roman", GSF_ROMAN },
    { "large", GSF_LARGE },
    { "small", GSF_SMALL },
    { "huge", GSF_HUGE },
    { "tiny", GSF_TINY },
    { "note", GSF_NOTE },
    { "warning", GSF_WARNING },
    { "important", GSF_IMPORTANT },
    { "pre", GSF_PREFORMATTED },
    { "caption", GSF_CAPTION },
    { "tag", GSF_TAG },
    { 0, 0 }
};

int styleForName(const String& name)
{
    for(int i = 0; gemFlags[i].condition; i++)
        if(name == gemFlags[i].condition) 
            return gemFlags[i].flag;
    return 0;
}

String nameForStyle(int flag)
{
    for(int i = 0; gemFlags[i].flag; i++)
        if(flag == gemFlags[i].flag)
            return gemFlags[i].condition;
    return String();
}

String trimLeft(const String& str)
{
    int pos = 0;
    while(pos < str.size() && str[pos].isSpace()) pos++;
    return str.mid(pos);
}

String trimRight(const String& str)
{
    int pos = str.size() - 1;
    while(pos >= 0 && str[pos].isSpace()) pos--;
    return str.left(pos + 1);
}

String trimRightSpaceOnly(const String& str)
{
    int pos = str.size() - 1;
    while(pos >= 0 && str[pos] == ' ') pos--;
    return str.left(pos + 1);
}

String trim(const String& str)
{
    return str.trimmed();
}

String replace(const String& str, QChar ch, QChar withCh)
{
    String out = str;
    out.replace(ch, withCh);
    return out;
}

String dateString(String format)
{
    return QDateTime::currentDateTime().toString(format);
}

int visualSize(const String& str)
{
    int len = 0;
    for(int i = 0; i < str.size(); i++)
    {
        switch(str[i].toAscii())
        {
        case OutputContext::CtrlAlign:
        case OutputContext::CtrlFill:
        case OutputContext::CtrlUnderfill:
            i++;
            break;

        case '\r':
        case '\n':
            return len;

        default:
            len++;
        }
    }
    return len;
}

bool fileFound(const String& fileName)
{
    if(!QFile::exists(fileName) && !QFile::exists(fileName + ".ame"))
        return false;

    return true;
}
