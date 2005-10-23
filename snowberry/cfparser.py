# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not: http://www.opensource.org/

## @file cfparser.py Configuration File Parser
##
## This module implements a recursive descent parser that can be used
## for reading the various configuration files that Snowberry uses.
## The Parser offers the primary mechanism for parsing a file.
## Configuration files are composed of a number of hierarchically
## organized elements.

import string

class Element:
    """The base class for all Elements.  This class can't be used by
    itself.  Always use one of the subclasses."""
    
    def __init__(self, name):
        """Construct a new Element.  Element names are considered case
        independent."""
        self.name = name.lower()

    def isKey(self):
        """Returns true if this is a Key element."""
        return False

    def isList(self):
        """Returns true if this is a List element."""
        return False

    def isBlock(self):
        """Returns true if this is a Block element."""
        return False        
        
    def getName(self):
        """Returns the name of the element."""
        return self.name

    def setName(self, name):
        self.name = name

    def show(self, prefix=''):
        """Print the element's contents for debugging purposes."""
        print prefix + "Element(%s)" % self.getName()


class KeyElement(Element):
    "A KeyElement is an Element that contains a single value."
    
    def __init__(self, name, value):
        Element.__init__(self, name)
        self.value = value

    def isKey(self):
        "Returns true if this is a KeyElement."
        return True

    def getValue(self):
        "Return the value of the KeyElement."
        return self.value

    def getContents(self):
        """Similar behaviour as with a list element.  Returns the
        contents of the element as an array of strings.

        @return An array of strings containing only one element: the
        value of the element as a string.
        """
        return [self.getValue()]

    def show(self, prefix=''):
        "Print the element's contents for debugging purposes."
        print prefix + "KeyElement(%s, %s)" % (self.getName(), self.getValue())


class ListElement(Element):
    "A ListElement is an Element that contains a list of values."
    
    def __init__(self, name):
        """Constructor.
        
        @param name Name of the element.
        """
        Element.__init__(self, name)
        self.values = []
        
    def isList(self):
        "Returns true if this is a List element."
        return True

    def add(self, value):
        "Add a new value to the list."
        self.values.append(value)
        
    def getContents(self):
        """Returns the contents of the list element.
        
        @return An array of strings.
        """
        return self.values        

    def show(self, prefix=''):
        "Print the element's contents for debugging purposes."
        s = prefix + "ListElement(%s, [ " % self.getName()
        for v in self.values:
            s += "'%s' " % v
        s += "])"
        print s


class BlockElement(Element):
    "A BlockElement contains other Elements."
    
    def __init__(self, blockType, name):
        Element.__init__(self, name)
        self.type = blockType.lower()
        self.contents = []

    def isBlock(self):
        "Returns true if this is a Block element."
        return True   

    def add(self, childElement):
        "Add a new element to this block's contents."
        if not childElement:
            raise Exception("attempted to append an invalid child element to a block")

        self.contents.append(childElement)

    def show(self, prefix=''):
        "Print the element's contents for debugging purposes."
        print prefix + "BlockElement(type=%s, name=%s," % (self.getType(),
                                                           self.getName())
        subPrefix = prefix + 3 * ' '
        for c in self.contents:
            c.show(subPrefix)
        print prefix + ")"

    def getType(self):
        "Returns the type identifier of the block."
        return self.type

    def getContents(self):
        "Returns the contents array of this block."
        return self.contents

    def find(self, elementName):
        """Returns the element with the specified name.  If it doesn't
        exist, returns None."""

        # Element names are considered case independent.
        elementName.lower()
        
        for e in self.contents:
            if e.getName() == elementName:
                return e

        # The specified name was not in this block's contents.
        return None

    def findValue(self, elementName):
        """Returns the value of the KeyElement with the specified
        name.  If it doesn't exist, returns an empty string."""
        e = self.find(elementName)
        if not e:
            return ''
        else:
            return e.getValue()


class ParseFailed(Exception):
    """An exception that is raised when the Parser encounters a syntax
    error in the source file."""

    def __init__(self, reason=''):
        Exception.__init__(self, reason)


class OutOfElements(Exception):
    """This exception is thrown when there are no more
    elements to return from Parser.get()."""

    def __init__(self):
        Exception.__init__(self, "no more elements in source file")


class Parser:
    """The Parser class is used for reading the contents of
    configuration, profile, and metadata files."""

    ## Characters that cause a token to end.
    TOKEN_BREAKING_CHARS = '#:=()<>,"' + string.whitespace
    
    def __init__(self, content):
        """Initialize the parser for reading a block of source content.

        @param content The configuration data that is to be parsed.
        """
        # The source data.  Add an extra newline so the character
        # reader won't get confused.
        self.content = content + "\n"
        # Index of the next character from the source.
        self.cursor = 0
        
        self.nextChar()
        self.tokenStartOffset = 0

        # When nextToken() is called and the current token is empty,
        # it is deduced that the source file has ended. We must
        # therefore set a dummy token that will be discarded
        # immediately.
        self.currentToken = ' '

        self.nextToken()

    def peekChar(self):
        "Returns the next character from the source file."
        return self.currentChar

    def nextChar(self):
        "Move to the next character in the source file."
        if self.cursor >= len(self.content):
            # No more characters to read.
            raise EOFError

        self.currentChar = self.content[self.cursor]
        self.cursor += 1

    def readLine(self):
        """Read a line of text from the content and return it."""
        
        line = ""

        self.nextChar()
        while self.currentChar != '\n':
            line += self.currentChar
            self.nextChar()

        return line        

    def readToEOL(self):
        """Read until a newline is encountered.  Returns the contents
        of the line."""
        self.cursor = self.tokenStartOffset
        line = self.readLine()
        try:
            self.nextChar()
        except EOFError:
            # If the file ends right after the line, we'll get the EOF
            # exception.  We can safely ignore it for now.
            pass
        return line
        
    def peekToken(self):
        return self.currentToken

    def nextToken(self):
        "Returns the next meaningful token from the source file."

        # Already drawn a blank?
        if self.currentToken == "":
            raise EOFError("out of tokens")

        self.currentToken = ""

        try:
            # Skip over any whitespace.
            while self.peekChar() in string.whitespace + '#':
                # Comments are considered whitespace.
                if self.peekChar() == '#':
                    self.readLine()
                self.nextChar()

            # Store the offset where the token begins.
            self.tokenStartOffset = self.cursor

            # The first nonwhite is accepted.
            self.currentToken += self.peekChar()
            self.nextChar()

            # Token breakers are tokens all by themselves.
            if self.currentToken[0] in Parser.TOKEN_BREAKING_CHARS:
                return self.currentToken
            
            while self.peekChar() not in Parser.TOKEN_BREAKING_CHARS:
                self.currentToken += self.peekChar()
                self.nextChar()

        except EOFError:
            # If the source file ends, return an empty token.
            pass

        return self.currentToken

    def get(self):
        """This is the method that the user calls to retrieve the next
        element from the source file.  If there are no more elements
        to return, a OutOfElements exception is raised."""
        e = self.parseElement()
        if not e: raise OutOfElements
        return e

    def parseElement(self):
        """Returns the next element from the source file.

        @return An instance of one of the Element classes.
        """
        
        try:
            key = self.peekToken()

            # The next token decides what kind of element we have here.
            next = self.nextToken()
            
        except EOFError:
            # The file ended.
            return None

        if next == ':' or next == '=':
            return self.parseKeyElement(key)
        elif next == '<':
            return self.parseListElement(key)
        else:
            # It's a block element.
            return self.parseBlockElement(key)

    def parseString(self):
        """Parse a string literal.  Returns the string sans the
        quotation marks in the beginning and the end."""

        if self.peekToken() != '"':
            raise ParseFailed('expected string to begin with \'"\', but \'%s\' found instead' % self.peekToken())

        # The collected characters.
        chars = []

        while self.peekChar() != '"':
            if self.peekChar() == "'":
                # Double single quotes form a double quote ('' => ").
                self.nextChar()
                if self.peekChar() == "'":
                    chars.append('"')
                else:
                    chars.append("'" + self.peekChar())
            else:
                # Other characters are appended as-is, even newlines.
                chars.append(self.peekChar())
            self.nextChar()

        # Move the parser to the next token.
        self.nextChar()
        self.nextToken()

        # Compile a string out of all the characters.
        return string.join(chars, '')

    def parseValue(self):
        """Parse a value from the source file.  The current token
        should be on the first token of the value.  Values come in
        different flavours:
        - single token
        - string literal (can be split)"""

        # Check if it is the beginning of a string literal.
        if self.peekToken() == '"':
            value = ""
            try:
                # The value will be composed of any number of
                # sub-strings.
                while True:
                    value += self.parseString()
            except:
                # No more strings to append.
                return value

        # Then it must be a single token.
        value = self.peekToken()
        self.nextToken()
        return value

    def parseKeyElement(self, name):
        """Parse a key element.
        @param name Name of the parsed key element."""

        # A colon means that that the rest of the line is the value of
        # the key element.
        if self.peekToken() == ':':
            value = self.readToEOL().strip()
            self.nextToken()
        else:
            #
            # Key =
            #   "This is a long string "
            #   "that spans multiple lines."
            #
            self.nextToken()
            value = self.parseValue()

        return KeyElement(name, value)
               
    def parseBlockElement(self, blockType):
        """Parse a block element, identified by the given name.

        @param blockType Identifier of the block.
        """
        
        block = BlockElement(blockType, self.peekToken())

        # How about some attributes?
        # Syntax: {token value} '('
        
        try:
            self.nextToken()
            while self.peekToken() != '(':
                keyName = self.peekToken()
                self.nextToken()
                value = self.parseValue()

                # This becomes a key element inside the block.
                block.add(KeyElement(keyName, value))

            # Move past the opening parentheses.
            self.nextToken()

            while self.peekToken() != ')':
                element = self.parseElement()
                if element == None:
                    raise ParseFailed("block element was never closed, end of file encountered before ')' was found")
                block.add(element)

        except EOFError:
            raise ParseFailed("end of file encountered unexpectedly while parsing a block element")

        # Move past the closing parentheses.
        self.nextToken()

        return block

    def parseListElement(self, name):
        "Parse a list element, identified by the given name."
        element = ListElement(name)

        if self.peekToken() != '<':
            raise ParseFailed("list must begin with a '<', but '%s' found instead" % self.peekToken())

        # List syntax:
        # list ::= list-identifier '<' [value {',' value}] '>'
        # list-identifier ::= token

        # Move past the opening angle bracket.
        self.nextToken()

        while True:
            element.add(self.parseValue())

            # List elements are separated explicitly.
            separator = self.peekToken()
            self.nextToken()

            # The closing bracket?
            if separator == '>':
                break

            # There should be a comma here.
            if separator != ',':
                raise ParseFailed("list values must be separated with a comma, but '%s' found instead" % separator)

        return element


class FileParser (Parser):
    """A parser for reading configuration data directly from a file."""

    def __init__(self, fileName):
        """Construct a FileParser.

        @param fileName Path of the source file.
        """
        self.contentFile = file(fileName)
        Parser.__init__(self, self.contentFile.read())
