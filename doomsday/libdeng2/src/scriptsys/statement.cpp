/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Statement"
#include "de/AssignStatement"
#include "de/CatchStatement"
#include "de/DeleteStatement"
#include "de/ExpressionStatement"
#include "de/ForStatement"
#include "de/FunctionStatement"
#include "de/IfStatement"
#include "de/FlowStatement"
#include "de/PrintStatement"
#include "de/TryStatement"
#include "de/WhileStatement"
#include "de/Reader"

using namespace de;

Statement *Statement::constructFrom(Reader &reader)
{
    SerialId id;
    reader.mark();
    reader >> id;
    reader.rewind();
    
    std::auto_ptr<Statement> result;
    switch(id)
    {
    case ASSIGN:
        result.reset(new AssignStatement);
        break;
        
    case CATCH:
        result.reset(new CatchStatement);
        break;

    case DELETE:
        result.reset(new DeleteStatement);
        break;

    case EXPRESSION:
        result.reset(new ExpressionStatement);
        break;
        
    case FLOW:
        result.reset(new FlowStatement);
        break;
        
    case FOR:
        result.reset(new ForStatement);
        break;
        
    case FUNCTION:
        result.reset(new FunctionStatement);
        break;
        
    case IF:
        result.reset(new IfStatement);
        break;
        
    case PRINT:
        result.reset(new PrintStatement);
        break;
        
    case TRY:
        result.reset(new TryStatement);
        break;    
        
    case WHILE:
        result.reset(new WhileStatement);
        break;
                
    default:
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized statement was invalid.
        throw DeserializationError("Statement::constructFrom", "Invalid statement identifier");
    }

    // Deserialize it.
    reader >> *result.get();
    return result.release();    
}
