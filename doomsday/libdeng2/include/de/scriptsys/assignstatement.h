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
 
#ifndef DASSIGNSTATEMENT_HH
#define DASSIGNSTATEMENT_HH

#include "deng.hh"
#include "../derror.hh"
#include "dstatement.hh"
#include "darrayexpression.hh"

#include <string>
#include <vector>

namespace de
{
    class NameExpression;
    
/**
 * Assignments are perhaps the most fundamental statements.
 * They assign a value to a variable, and also create new variables
 * if one with the specified identifier does not yet exist.
 */
    class AssignStatement : public Statement
    {
    public:
        /// This exception is thrown if trying to assign into something other
        /// than a variable reference (VariableValue).
        DEFINE_ERROR(IllegalTargetError);
        
        typedef std::vector<Expression*> Indices;
        
    public:
        /**
         * Constructor. Statement takes ownership of the expressions 
         * @c target and @c value.
         *
         * @param target  Expression that resolves to a Variable reference (VariableValue).
         * @param indices Expressions that determine element indices into existing
         *                element-based values. Empty, if there is no indices for
         *                the assignment. 
         * @param value   Expression that determines the value of the variable.
         */
        AssignStatement(Expression* target, const Indices& index, Expression* value);
        
        ~AssignStatement();
        
        void execute(Context& context) const;
        
    private:
        ArrayExpression args_;
        dint indexCount_;
    };
}

#endif /* DASSIGNSTATEMENT_HH */
