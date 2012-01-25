# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2005
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

## @file expressions.py Expressions
## 
## Routines for evaluating expressions.

import string
import os
import confdb as st
import paths


def evaluate(expression, contextAddon):
    """
    @param expression    Expression to evaluate as a string.

    @param contextAddon  Addon in the context of which the evaluation is made.
    
    @return  The evaluated version with all functions and variables
    expanded.
    """
    result = ''
    pos = 0

    while pos < len(expression):
        ch = expression[pos]

        # Is this the beginning of a new expression?
        if ch == '$':
            # Try to scan it.
            subExpression = scan(expression[pos:])

            if len(subExpression):
                result += execute(subExpression[1:], contextAddon)
                pos += len(subExpression)
                continue

        # Append the character to the result.
        result += ch
        pos += 1

    #print 'Evaluated result: ' + result
    return result


def scan(src):
    """Scans a subexpression that begins with $."""

    pos = 1

    # Use angle brackets < and > because they can't be used on the
    # command line.

    # $FUNCTION
    # $FUNCTION<asdas>

    while pos < len(src):
        ch = src[pos]
        if ch not in string.uppercase + '_':
            # This is no longer a part of the function name.
            if ch == '<':
                # Begin an argument block. Find the end.
                endPos = findMatchingBracket(src, pos)
                if endPos < 0:
                    # This is invalid!
                    return ''
                return src[:endPos + 1]
            break
                    
        pos += 1

    return src[:pos]


def findMatchingBracket(src, startPos):
    level = 0
    pos = startPos
    while pos < len(src):
        if src[pos] == '<':
            level += 1
        elif src[pos] == '>':
            level -= 1
        if level == 0:
            return pos
        pos += 1
    # Not good.
    return -1


def getCommand(src):
    """Extract the command from the beginning of an expression.

    @param src  Expression.  Must start at <code>$</code>.

    @return  Command as a string.
    """
    command = ''
    pos = 0
    while pos < len(src):
        if src[pos] in string.uppercase + '_':
            command += src[pos]
        else:
            break
        pos += 1
    
    return command
    

def execute(src, contextAddon):
    """Execute a command.

    @param src  Command to execute.  Must start at <code>$</code>.

    @return  The command's evaluated result.
    """

    #print "executing: " + src

    command = getCommand(src)
    if len(src) > len(command):
        argSrc = src[len(command) + 1 : -1]
    else:
        argSrc = ''

    #print 'command: ' + command + ';'
    #print 'argSrc: ' + argSrc + ';'

    if command == 'BUNDLE':
        if contextAddon:
            return contextAddon.getContentPath()
        else:
            return ''
            
    if command == 'SBROOT':
        import __main__
        arg = evaluate(argSrc, contextAddon)
        return paths.quote(os.path.join(os.path.dirname(
            os.path.abspath(__main__.__file__)), arg))

    if command == 'DENGBASE':
        arg = evaluate(argSrc, contextAddon)
        if st.isDefined('doomsday-base'):
            return paths.quote(os.path.abspath(os.path.join(
                st.getSystemString('doomsday-base'), arg)))
        else:
            return '}' + arg
        
    if command == 'PATH':
        arg = os.path.normpath(evaluate(argSrc, contextAddon))
        return paths.quote(arg)

    if command == 'VALUE':
        arg = evaluate(argSrc, contextAddon)
        import sb.profdb
        activeProfile = sb.profdb.getActive()

        # Try local value identifier first.
        if contextAddon:
            localized = contextAddon.makeLocalId(arg)
            v = activeProfile.getValue(localized)
            if v:
                return v.getValue()

        v = activeProfile.getValue(arg)
        if v:
            return v.getValue()

        return ''

    if command == 'QUOTE':
        return paths.quote( evaluate(argSrc, contextAddon) )

    return ''
