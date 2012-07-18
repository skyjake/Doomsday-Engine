/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Operator"
#include "de/String"

using namespace de;

String de::operatorToText(Operator op)
{
    switch(op)
    {
    case NOT:
        return "NOT";
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
    default:
        return "UNKNOWN";
    }        
}

bool de::leftOperandByReference(Operator op)
{
    switch(op)
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
