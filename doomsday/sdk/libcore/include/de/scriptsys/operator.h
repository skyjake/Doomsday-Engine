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
 
#ifndef LIBDENG2_OPERATOR_H
#define LIBDENG2_OPERATOR_H

namespace de {

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
    OR,
    RESULT_TRUE, ///< Pop a result, check if it is True.
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
    BITWISE_NOT,
};

String operatorToText(Operator op);

bool leftOperandByReference(Operator op);

bool isUnary(Operator op);

bool isBinary(Operator op);

} // namespace de

#endif /* LIBDENG2_OPERATOR_H */
