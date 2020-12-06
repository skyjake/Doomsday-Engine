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

#include "de/Operator"
#include "de/String"

namespace de {

String operatorToText(Operator op)
{
    switch (op)
    {
    case NOT:
        return "NOT";
    case AND:
        return "AND";
    case OR:
        return "OR";
    case IN:
        return "IN";
    case EQUAL:
        return "EQUAL";
    case NOT_EQUAL:
        return "NOT_EQUAL";
    case LESS:
        return "LESS";
    case GREATER:
        return "GREATER";
    case LEQUAL:
        return "LEQUAL";
    case GEQUAL:
        return "GEQUAL";
    case PLUS:
        return "PLUS";
    case MINUS:
        return "MINUS";
    case DIVIDE:
        return "DIVIDE";
    case MULTIPLY:
        return "MULTIPLY";
    case MODULO:
        return "MODULO";
    case PLUS_ASSIGN:
        return "PLUS_ASSIGN";
    case MINUS_ASSIGN:
        return "MINUS_ASSIGN";
    case DIVIDE_ASSIGN:
        return "DIVIDE_ASSIGN";
    case MULTIPLY_ASSIGN:
        return "MULTIPLY_ASSIGN";
    case MODULO_ASSIGN:
        return "MODULO_ASSIGN";
    case DOT:
        return "DOT";
    case MEMBER:
        return "MEMBER";
    case CALL:
        return "CALL";
    case ARRAY:
        return "ARRAY";
    case DICTIONARY:
        return "DICTIONARY";
    case INDEX:
        return "INDEX";
    case SLICE:
        return "SLICE";
    case RESULT_TRUE:
        return "RESULT_TRUE";
    case BITWISE_AND:
        return "BITWISE_AND";
    case BITWISE_OR:
        return "BITWISE_OR";
    case BITWISE_XOR:
        return "BITWISE_XOR";
    case BITWISE_NOT:
        return "BITWISE_NOT";
    default:
        return "UNKNOWN";
    }        
}

bool leftOperandByReference(Operator op)
{
    switch (op)
    {
    case PLUS_ASSIGN:
    case MINUS_ASSIGN:
    case DIVIDE_ASSIGN:
    case MULTIPLY_ASSIGN:
    case MODULO_ASSIGN:
        return true;
        
    default:
        return false;
    }
}

bool isUnary(Operator op)
{
    return (op == PLUS || op == MINUS || op == NOT || op == BITWISE_NOT || op == RESULT_TRUE);
}

bool isBinary(Operator op)
{
    return (op != NOT && op != BITWISE_NOT);
}

} // namespace de
