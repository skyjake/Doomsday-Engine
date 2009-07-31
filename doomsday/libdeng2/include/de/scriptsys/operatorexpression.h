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

#ifndef DOPERATOREXPRESSION_HH
#define DOPERATOREXPRESSION_HH

#include "../derror.hh"
#include "doperator.hh"
#include "dexpression.hh"

namespace de
{
    class Evaluator;
    class Value;

/**
 * Expression that evaluates the results of unary and binary operators. 
 * This includes, for example, arithmetic operators, text concatenation,
 * and logical expressions.
 */    
    class OperatorExpression : public Expression
    {
    public:
        /// This exception is thrown if an unary operation is attempted even
        /// though the selected operation cannot be unary.
        DEFINE_ERROR(NonUnaryError);
        
        /// This exception is thrown if a binary operation is attempted even
        /// though the selected operation cannot be binary.
        DEFINE_ERROR(NonBinaryError);
        
        /// This exception is thrown if the MEMBER operator receives a 
        /// non-Object scope on the left side.
        DEFINE_ERROR(NonObjectScopeError);
        
        /// This exception is thrown if the SLICE operator has invalid arguments.
        DEFINE_ERROR(SliceError);
        
    public:
        /**
         * Constructor for unary operations (+, -).
         * @param op Operation to perform.
         * @param operand Expression that provides the right-hand operand.
         */
        OperatorExpression(Operator op, Expression* operand);

        /**
         * Constructor for binary operations.
         * @param op Operation to perform.
         * @param leftOperand Expression that provides the left-hand operand.
         * @param rightOperand Expression that provides the right-hand operand.
         */
        OperatorExpression(Operator op, Expression* leftOperand, Expression* rightOperand);

        ~OperatorExpression();

		void push(Evaluator& evaluator, Object* names = 0) const;

		Value* evaluate(Evaluator& evaluator) const;
        
    private:
        /// Used to create return values of boolean operations.
        static Value* newBooleanValue(bool isTrue);
                
    private:
        Operator op_;
        Expression* leftOperand_;
        Expression* rightOperand_;
    };
}

#endif /* DOPERATOREXPRESSION_HH */
