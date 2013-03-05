/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Info"
#include <QFile>

using namespace de;

const static QString WHITESPACE = " \t\r\n";
const static QString WHITESPACE_OR_COMMENT = " \t\r\n#";
const static QString TOKEN_BREAKING_CHARS = "#:=(){}<>,\"" + WHITESPACE;

struct Info::Instance
{
    DENG2_ERROR(OutOfElements);
    DENG2_ERROR(EndOfFile);

    Instance() : currentLine(0), cursor(0), tokenStartOffset(0), rootBlock("", "")
    {}

    /**
     * Initialize the parser for reading a block of source content.
     * @param source  Text to be parsed.
     */
    void init(String const &source)
    {
        rootBlock.clear();

        // The source data. Add an extra newline so the character reader won't
        // get confused.
        content = source + "\n";
        currentLine = 1;

        nextChar();
        tokenStartOffset = 0;

        // When nextToken() is called and the current token is empty,
        // it is deduced that the source file has ended. We must
        // therefore set a dummy token that will be discarded
        // immediately.
        currentToken = " ";
        nextToken();
    }

    /**
     * Returns the next character from the source file.
     */
    QChar peekChar()
    {
        return currentChar;
    }

    /**
     * Move to the next character in the source file.
     */
    void nextChar()
    {
        if(cursor >= content.size())
        {
            // No more characters to read.
            throw EndOfFile(QString("EOF on line %1").arg(currentLine));
        }
        if(currentChar == '\n') currentLine++;
        currentChar = content[cursor];
        cursor++;
    }

    /**
     * Read a line of text from the content and return it.
     */
    String readLine()
    {
        String line;
        nextChar();
        while(currentChar != '\n')
        {
            line += currentChar;
            nextChar();
        }
        return line;
    }

    /**
     * Read until a newline is encountered. Returns the contents of the line.
     */
    String readToEOL()
    {
        cursor = tokenStartOffset;
        String line = readLine();
        try
        {
            nextChar();
        }
        catch(EndOfFile const &)
        {
            // If the file ends right after the line, we'll get the EOF
            // exception.  We can safely ignore it for now.
        }
        return line;
    }

    String peekToken()
    {
        return currentToken;
    }

    /**
     * Returns the next meaningful token from the source file.
     */
    String nextToken()
    {
        // Already drawn a blank?
        if(currentToken.isEmpty()) throw EndOfFile("out of tokens");

        currentToken = "";

        try
        {
            // Skip over any whitespace.
            while(WHITESPACE_OR_COMMENT.contains(peekChar()))
            {
                // Comments are considered whitespace.
                if(peekChar() == '#') readLine();
                nextChar();
            }

            // Store the offset where the token begins.
            tokenStartOffset = cursor;

            // The first nonwhite is accepted.
            currentToken += peekChar();
            nextChar();

            // Token breakers are tokens all by themselves.
            if(TOKEN_BREAKING_CHARS.contains(currentToken[0]))
                return currentToken;

            while(!TOKEN_BREAKING_CHARS.contains(peekChar()))
            {
                currentToken += peekChar();
                nextChar();
            }
        }
        catch(EndOfFile const &)
        {}

        return currentToken;
    }

    /**
     * This is the method that the user calls to retrieve the next element from
     * the source file. If there are no more elements to return, a
     * OutOfElements exception is thrown.
     *
     * @return Parsed element. Caller gets owernship.
     */
    Element *get()
    {
        Element *e = parseElement();
        if(!e) throw OutOfElements("");
        return e;
    }

    /**
     * Returns the next element from the source file.
     * @return An instance of one of the Element classes, or @c NULL if there are none.
     */
    Element *parseElement()
    {
        String key;
        String next;
        try
        {
            key = peekToken();

            // The next token decides what kind of element we have here.
            next = nextToken();
        }
        catch(EndOfFile const &)
        {
            // The file ended.
            return 0;
        }

        if(next == ":" || next == "=")
        {
            return parseKeyElement(key);
        }
        else if(next == "<")
        {
            return parseListElement(key);
        }
        else
        {
            // It must be a block element.
            return parseBlockElement(key);
        }
    }

    /**
     * Parse a string literal. Returns the string sans the quotation marks in
     * the beginning and the end.
     */
    String parseString()
    {
        if(peekToken() != "\"")
        {
            throw SyntaxError("Info::parseString",
                              QString("Expected string to begin with '\"', but '%1' found instead (on line %2).")
                              .arg(peekToken()).arg(currentLine));
        }

        // The collected characters.
        String chars;

        while(peekChar() != '"')
        {
            if(peekChar() == '\'')
            {
                // Double single quotes form a double quote ('' => ").
                nextChar();
                if(peekChar() == '\'')
                    chars.append("\"");
                else
                {
                    chars.append("'");
                    chars.append(peekChar());
                }
            }
            else
            {
                // Other characters are appended as-is, even newlines.
                chars.append(peekChar());
            }
            nextChar();
        }

        // Move the parser to the next token.
        nextChar();
        nextToken();
        return chars;
    }

    /**
     * Parse a value from the source file.  The current token
     * should be on the first token of the value.  Values come in
     * different flavours:
     * - single token
     * - string literal (can be split)
     */
    String parseValue()
    {
        String value;

        // Check if it is the beginning of a string literal.
        if(peekToken() == "\"")
        {
            try
            {
               // The value will be composed of any number of sub-strings.
               forever { value += parseString(); }
            }
            catch(de::Error const &)
            {
                // No more strings to append.
                return value;
            }
        }

        // Then it must be a single token.
        value = peekToken();
        nextToken();
        return value;
    }

    /**
     * Parse a key element.
     * @param name Name of the parsed key element.
     */
    KeyElement *parseKeyElement(String const &name)
    {
        String value;

        // A colon means that that the rest of the line is the value of
        // the key element.
        if(peekToken() == ":")
        {
            value = readToEOL().trimmed();
            nextToken();
        }
        else
        {
            /**
             * Key =
             *   "This is a long string "
             *   "that spans multiple lines."
             */
            nextToken();
            value = parseValue();
        }
        return new KeyElement(name, value);
    }

    /**
     * Parse a list element, identified by the given name.
     */
    ListElement *parseListElement(String const &name)
    {
        if(peekToken() != "<")
        {
            throw SyntaxError("Info::parseListElement",
                              QString("List must begin with a '<', but '%1' found instead (on line %2).")
                              .arg(peekToken()).arg(currentLine));
        }

        QScopedPointer<ListElement> element(new ListElement(name));

        /// List syntax:
        /// list ::= list-identifier '<' [value {',' value}] '>'
        /// list-identifier ::= token

        // Move past the opening angle bracket.
        nextToken();

        forever
        {
            element->add(parseValue());

            // List elements are separated explicitly.
            String separator = peekToken();
            nextToken();

            // The closing bracket?
            if(separator == ">") break;

            // There should be a comma here.
            if(separator != ",")
            {
                throw SyntaxError("Info::parseListElement",
                                  QString("List values must be separated with a comma, but '%1' found instead (on line %2).")
                                  .arg(separator).arg(currentLine));
            }
        }
        return element.take();
    }

    /**
     * Parse a block element, identified by the given name.
     * @param blockType Identifier of the block.
     */
    BlockElement *parseBlockElement(String const &blockType)
    {
        QScopedPointer<BlockElement> block(new BlockElement(blockType, peekToken()));
        int startLine = currentLine;

        // How about some attributes?
        // Syntax: {token value} '('|'{'

        try
        {
            String endToken;

            nextToken();
            while(peekToken() != "(" && peekToken() != "{")
            {
                String keyName = peekToken();
                nextToken();
                String value = parseValue();

                // This becomes a key element inside the block.
                block->add(new KeyElement(keyName, value));
            }

            endToken = (peekToken() == "("? ")" : "}");

            // Move past the opening parentheses.
            nextToken();

            while(peekToken() != endToken)
            {
                Element *element = parseElement();
                if(!element)
                {
                    throw SyntaxError("Info::parseBlockElement",
                                      QString("Block element was never closed, end of file encountered before '%1' was found (on line %2).")
                                      .arg(endToken).arg(currentLine));
                }
                block->add(element);
            }
        }
        catch(EndOfFile const &)
        {
            throw SyntaxError("Info::parseBlockElement",
                              QString("End of file encountered unexpectedly while parsing a block element (block started on line %1).")
                              .arg(startLine));
        }

        // Move past the closing parentheses.
        nextToken();

        return block.take();
    }

    void parse(String const &source)
    {
        init(source);
        forever
        {
            Element *e = parseElement();
            if(!e) break;
            rootBlock.add(e);
        }
    }

    String content;
    int currentLine;
    int cursor; ///< Index of the next character from the source.
    QChar currentChar;
    int tokenStartOffset;
    String currentToken;
    BlockElement rootBlock;
};

Info::BlockElement::~BlockElement()
{
    clear();
}

void Info::BlockElement::clear()
{
    for(Contents::iterator i = _contents.begin(); i != _contents.end(); ++i)
        delete i.value();

    _contents.clear();
    _contentsInOrder.clear();
}

Info::Element *Info::BlockElement::findByPath(String const &path) const
{
    String name;
    String remainder;
    int pos = path.indexOf(':');
    if(pos >= 0)
    {
        name = path.left(pos);
        remainder = path.mid(pos + 1);
    }
    else
    {
        name = path;
    }
    name = name.trimmed();

    // Does this element exist?
    Element *e = find(name);
    if(!e) return 0;

    if(e->isBlock())
    {
        // Descend into sub-blocks.
        return static_cast<BlockElement *>(e)->findByPath(remainder);
    }
    return e;
}

Info::Info() : d(new Instance)
{}

Info::Info(String const &source)
{
    QScopedPointer<Instance> inst(new Instance); // parsing may throw exception
    inst->parse(source);
    d.reset(inst.take());
}

void Info::parse(String const &infoSource)
{
    d->parse(infoSource);
}

void Info::parseNativeFile(String const &nativePath)
{
    QFile file(nativePath);
    if(file.open(QFile::ReadOnly | QFile::Text))
    {
        parse(file.readAll().constData());
    }
}

Info::BlockElement const &Info::root() const
{
    return d->rootBlock;
}

Info::Element const *Info::findByPath(String const &path) const
{
    if(path.isEmpty()) return &d->rootBlock;
    return d->rootBlock.findByPath(path);
}

bool Info::findValueForKey(String const &key, String &value) const
{
    Element const *element = findByPath(key);
    if(element && element->isKey())
    {
        value = static_cast<KeyElement const *>(element)->value();
        return true;
    }
    return false;
}
