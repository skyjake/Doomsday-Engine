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
 
#ifndef LIBDENG2_OPERATOR_H
#define LIBDENG2_OPERATOR_H

namespace de
{
    class String;
    
    /**
     * Operators. 
     * 
     * @note  These are serialized as is, so don't change the existing values.
     *
     * @ingroup script
     */
    enum Operator {
        NONE = 0,
        NOT,
        IN,
        EQUAL,
        NOT_EQUAL,
        LESS,
        GREATER,
        LEQUAL,
        GEQUAL,
        PLUS,
        MINUS,
        MULTIPLY,
        DIVIDE,
        MODULO,
        PLUS_ASSIGN,
        MINUS_ASSIGN,
        MULTIPLY_ASSIGN,
        DIVIDE_ASSIGN,
        MODULO_ASSIGN,
        DOT,
        MEMBER,
        CALL,
        ARRAY,
        DICTIONARY,
        INDEX,
        SLICE,
        PARENTHESIS,
        AND,
        OR
    };
    
    String operatorToText(Operator op);
    
    bool leftOperandByReference(Operator op);
}

#endif /* LIBDENG2_OPERATOR_H */
