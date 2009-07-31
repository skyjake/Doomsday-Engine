/*
 * The Doomsday Engine Project -- Hawthorn
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

#ifndef DNAMEEXPRESSION_HH
#define DNAMEEXPRESSION_HH

#include "../derror.hh"
#include "dexpression.hh"

namespace de
{
/**
 * A NameExpression locates a named node in the namespaces, and creates 
 * an appropriate value to refer to that node.
 */
    class NameExpression : public Expression
    {
    public:
        /// This exception is thrown if the path does not specify an 
        /// existing node.
        DEFINE_ERROR(NotFoundError);
        
        enum Flags {
            BY_VALUE        = 0x0,  ///< Results in a value.
            BY_REFERENCE    = 0x1,  ///< Results in a reference.
            LOCAL_ONLY      = 0x2,  ///< Look for object in local namespace only.
            NEW_VARIABLE    = 0x4,  ///< If missing, create a new variable.
        };
        
    public:
        NameExpression(const std::string& path, int flags = BY_VALUE);
        
        Value* evaluate(Evaluator& evaluator) const;
        
    private:
        std::string path_;
        int flags_;
    };
}

#endif /* DNAMEEXPRESSION_HH */
