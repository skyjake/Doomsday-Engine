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

#ifndef LIBDENG2_BUILTINEXPRESSION_H
#define LIBDENG2_BUILTINEXPRESSION_H

#include "../deng.h"
#include "../Expression"

namespace de
{
    /**
     * Evaluates a built-in function on the argument(s).
     *
     * @ingroup script
     */
    class BuiltInExpression : public Expression
    {
    public:
        /// A wrong number of arguments is given to one of the built-in methods. @ingroup errors
        DEFINE_ERROR(WrongArgumentsError);
        
        /// Type of the built-in expression.
        enum Type {
            LENGTH, ///< Evaluate the length of an value (by calling size()).
            DICTIONARY_KEYS,
            DICTIONARY_VALUES
        };
        
    public:
        BuiltInExpression(Type type, Expression* argument);
        
        ~BuiltInExpression();
        
        void push(Evaluator& evaluator, Record* names = 0) const;

        Value* evaluate(Evaluator& evaluator) const;
        
    private:  
        Type type_;
        Expression* arg_;
    };
}

#endif /* LIBDENG2_BUILTINEXPRESSION_H */
