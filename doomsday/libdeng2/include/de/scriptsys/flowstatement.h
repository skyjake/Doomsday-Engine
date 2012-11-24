/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#ifndef LIBDENG2_FLOWSTATEMENT_H
#define LIBDENG2_FLOWSTATEMENT_H

#include "../Statement"

namespace de
{
    class Expression;
    
    /**
     * Controls the script's flow of execution.
     *
     * @ingroup script
     */
    class FlowStatement : public Statement
    {
    public:
        /// Type of control flow operation.
        enum Type {
            PASS,
            CONTINUE,
            BREAK,
            RETURN,
            THROW
        };
        
    public:
        FlowStatement();
        
        FlowStatement(Type type, Expression *countArgument = 0);
        
        ~FlowStatement();
        
        void execute(Context &context) const;
        
        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);         
        
    private:        
        Type _type;
        Expression *_arg;
    };
}

#endif /* LIBDENG2_FLOWSTATEMENT_H */
