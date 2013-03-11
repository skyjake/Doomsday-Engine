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

#include "source.h"
#include "defs.h"
#include "exception.h"
#include <QDebug>

Source::Source() : Linkable(true)
{
    _lineNumber = 0;
    _mustDeleteStream = false;
    _is = 0;
    _peekedChar = 0;
}

Source::Source(QTextStream& inputStream) : Linkable()
{
    _lineNumber = 1;
    _mustDeleteStream = false;
    _is = &inputStream;

    // Get the first char.
    nextChar();
}

Source::Source(String fileName) : Linkable()
{
    _fileName = fileName;
    _lineNumber = 1;
    _is = 0;

    if(!QFile::exists(fileName))
    {
        // Default extension?
        if(QFile::exists(fileName + ".ame"))
        {
            _fileName = fileName + ".ame";
        }
    }

    // Open the file.
    _file.setFileName(_fileName);
    if(!_file.open(QFile::ReadOnly | QFile::Text))
    {
        // Couldn't open it.
        qWarning() << (fileName + ": " + _file.errorString()).toLatin1().data();
        return;
    }

    _is = new QTextStream(&_file);
    _mustDeleteStream = true;

    // Get the first char.
    nextChar();
}

Source::~Source()
{
    if(_mustDeleteStream) delete _is;
}

void Source::nextChar()
{
    QString chars = _is->read(1);
    if(chars.isEmpty())
    {
        // We're at the end.
        _peekedChar = EOF;
    }
    else
    {
        _peekedChar = chars[0];
    }
}

bool Source::isOpen()
{
    return _is != 0;
}

QChar Source::peek()
{
    return _peekedChar;
}

void Source::ignore()
{
    if(!_is) return;
    nextChar();
}

QChar Source::get()
{
    QChar c = peek();
    ignore();
    return c;
}

void Source::skipToMatching()
{
    int level = 0;
    QChar c;

    while((c = get()) != EOF)
    {
        if(c == '@')    // Escape sequence.
        {
            ignore();   // Skip whatever follows.
            continue;
        }
        if(c == '{') level++;
        if(c == '}') if(!level--) break;
    }
}

/**
  * A line with nothing but whitespace amounts to an empty token (a blank).
  * Increments the line number when moving forwards in the stream.
  */
bool Source::getTokenOrBlank(String &token)
{
    bool gotNewline = false, gotBlank = false;

    // Is there a pushed-back token?
    if(!_pushedTokens.isEmpty())
    {
        token = _pushedTokens.takeLast();
        return true;        
    }
    token.clear();
    
    // Extract the next token, or blank.
    // First eat whitespace & comments, and keep an eye out for a blank.
    QChar c;
    while((c = peek()) != EOF)
    {
        if(c == '$') // Begin a comment?
        {
            ignore();
            if((c = peek()) == '*') // Delimited comment?
            {
                ignore();
                // Eat all characters, but keep a look-out for *$.
                while((c = peek()) != EOF)
                {
                    // Don't loose track of line numbers.
                    if(c == '\n') _lineNumber++;
                    ignore();
                    if(c == '*' && peek() == '$')
                    {
                        // The comment ends...
                        ignore();
                        break;
                    }                   
                }
            }
            else // One-liner.
            {
                while((c = peek()) != EOF && c != '\n') ignore();
                if(c == EOF) break;
                // Eat the newline as well.
                _lineNumber++;
                ignore();
            }
            continue;
        }
        if(!c.isSpace()) break; // No more whitespace.
        if(c == '\n')
        {
            _lineNumber++;
            if(!gotNewline)
                gotNewline = true;
            else // There already was one newline!
                gotBlank = true;
        }
        ignore();
    }
    if(gotBlank)
    {
        // We have ourselves a blank here. The source read cursor is left
        // at the first non-white character.
        return true;        
    }

    // Now extract until whitespace encountered. This becomes the token.
    while((c = peek()) != EOF)
    {
        if(c.isSpace()) break; // The token ends.
        if(!token.isEmpty() && IS_BREAK(c)) break;
        // All non-white characters end up in the token.
        token += c;
        ignore();
        if(c == '@')
        {
            // Must check the next character.
            QChar n = peek();
            if(!IS_BREAK(n)) break; // Stop here.
            // This is an escape sequence.
            token += n;
            ignore();
            break;
        }
        else if(IS_BREAK(c)) break;
    }

    return !token.isEmpty();
}

/**
 * Returns true if a token was successfully extracted from the input
 * stream, otherwise false.
 */
bool Source::getToken(String &token)
{
    while(getTokenOrBlank(token))
    {
        if(!token.isEmpty())
            return true;
    }
    // No tokens left, or just blank!
    return false;
}

/**
 * Causes a fatal error if a token is not available.
 */
void Source::mustGetToken(String &token)
{
    if(!getToken(token))
        throw Exception("Unexpected end of file.", _fileName, _lineNumber);
}

/**
 * Pushing an empty token does nothing.
 */
void Source::pushToken(const String& token)
{
    _pushedTokens.append(token);
}
