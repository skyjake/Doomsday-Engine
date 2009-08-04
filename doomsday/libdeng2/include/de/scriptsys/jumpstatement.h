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
 
#ifndef LIBDENG2_JUMPSTATEMENT_H
#define LIBDENG2_JUMPSTATEMENT_H

#include "../Statement"

namespace de
{
    class Expression;
    
    /**
     * Jumps within the current context, i.e., local jumps.
     *
     * @ingroup script
     */
    class JumpStatement : public Statement
    {
    public:
        /// Type of jump.
        enum Type {
            CONTINUE,
            BREAK,
            RETURN
        };
        
    public:
        JumpStatement();
        
        JumpStatement(Type type, Expression* countArgument = 0);
        
        ~JumpStatement();
        
        void execute(Context& context) const;
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);         
        
    private:        
        Type type_;
        Expression* arg_;
    };
}

#endif /* LIBDENG2_JUMPSTATEMENT_H */
