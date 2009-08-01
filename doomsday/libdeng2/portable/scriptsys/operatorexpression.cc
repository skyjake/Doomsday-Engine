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

#include "de/OperatorExpression"
#include "de/Evaluator"
#include "de/Value"
#include "de/Numbervalue"
#include "de/Arrayvalue"
#include "de/RefValue"
#include "de/NoneValue"
#include "de/math.h"

using namespace de;

OperatorExpression::OperatorExpression(Operator op, Expression* operand)
    : op_(op), leftOperand_(0), rightOperand_(operand)
{
    if(op != PLUS && op != MINUS && op != NOT)
    {
        throw NonUnaryError("OperatorExpression::OperatorExpression",
            "Unary " + operatorToText(op) + " not defined");
    }
}

OperatorExpression::OperatorExpression(Operator op, Expression* leftOperand, Expression* rightOperand)
    : op_(op), leftOperand_(leftOperand), rightOperand_(rightOperand)
{
    if(op == NOT)
    {
        throw NonBinaryError("OperatorExpression::OperatorExpression",
            "Binary " + operatorToText(op) + " not defined");
    }
}

OperatorExpression::~OperatorExpression()
{
    delete leftOperand_;
    delete rightOperand_;
}

void OperatorExpression::push(Evaluator& evaluator, Record* names) const
{
    Expression::push(evaluator);
    
    if(op_ == MEMBER)
    {
        // The MEMBER operator works a bit differently. Just push the left side
        // now. We'll push the other side when we've found out what is the 
        // scope defined by the result of the left side (which must be a RefValue).
        leftOperand_->push(evaluator, names);
    }
    else
    {
        rightOperand_->push(evaluator);
        if(leftOperand_)
        {
            leftOperand_->push(evaluator, names);
        }
    }
}

Value* OperatorExpression::newBooleanValue(bool isTrue)
{
    return new NumberValue(isTrue? NumberValue::TRUE : NumberValue::FALSE);
}

Value* OperatorExpression::evaluate(Evaluator& evaluator) const
{
#if 0
    // Get the operands.
    Value* rightValue = (op_ == MEMBER? 0 : evaluator.popResult());
    Value* leftValue = (leftOperand_? evaluator.popResult() : 0);
    Value* result = (leftValue? leftValue : rightValue);

    switch(op_)
    {
    case PLUS:
        std::cerr << "lval:" << leftValue->asText() << ", rval:" << rightValue->asText() << "\n";
        if(leftValue)
        {
            leftValue->sum(*rightValue);
        }
        else
        {
            // Unary plus is defined as a no-op.
        }
        break;
        
    case PLUS_ASSIGN:
        std::cerr << "lval:" << leftValue->asText() << ", rval:" << rightValue->asText() << "\n";
        leftValue->dereference().sum(*rightValue);
        break;    
        
    case MINUS:
        if(leftValue)
        {
            leftValue->subtract(*rightValue);
        }
        else
        {
            // Negation.
            rightValue->negate();
        }
        break;
        
    case MINUS_ASSIGN:
        leftValue->dereference().subtract(*rightValue);
        break;    
        
    case DIVIDE:
        leftValue->divide(*rightValue);
        break;

    case DIVIDE_ASSIGN:
        leftValue->dereference().divide(*rightValue);
        break;    
        
    case MULTIPLY:
        leftValue->multiply(*rightValue);
        break;

    case MULTIPLY_ASSIGN:
        leftValue->dereference().multiply(*rightValue);
        break;    
        
    case MODULO:
        leftValue->modulo(*rightValue);
        break;

    case MODULO_ASSIGN:
        leftValue->dereference().modulo(*rightValue);
        break;    
        
    case NOT:
        result = newBooleanValue(rightValue->isFalse());
        break;
        
    case EQUAL:
        result = newBooleanValue(!leftValue->compare(*rightValue));
        break;

    case NOT_EQUAL:
        result = newBooleanValue(leftValue->compare(*rightValue) != 0);
        break;

    case LESS:
        result = newBooleanValue(leftValue->compare(*rightValue) < 0);
        break;

    case GREATER:
        result = newBooleanValue(leftValue->compare(*rightValue) > 0);
        break;

    case LEQUAL:
        result = newBooleanValue(leftValue->compare(*rightValue) <= 0);
        break;

    case GEQUAL:
        result = newBooleanValue(leftValue->compare(*rightValue) >= 0);
        break;
        
    case IN:
        result = newBooleanValue(rightValue->contains(*leftValue));
        break;
        
    case CALL:
        leftValue->call(evaluator.process(), *rightValue);
        // Result comes from whatever is being called.
        result = 0;
        break;
        
    case MEMBER:
        {
            const ObjectValue* objectValue = 
                dynamic_cast<const ObjectValue*>(leftValue);
            if(!objectValue)
            {
                throw NonObjectScopeError("OperatorExpression::evaluate",
                    "Left side of " + operatorToText(op_) + " must evaluate to an ObjectValue");
            }
            // Now that we know what is the scope, push the rest of the expression.
            // The evaluator will hold onto an object reference.
            rightOperand_->push(evaluator, objectValue->object());
            // The MEMBER operator does not evaluate to any result. 
            // Whatever is on the right side will be the result.
            delete objectValue;
            assert(rightValue == NULL);
            return NULL;
        }
        
    case INDEX:
        std::cerr << "lval:" << leftValue->asText() << ", rval:" << rightValue->asText() << "\n";
        result = leftValue->element(*rightValue)->duplicate();
        break;
        
    case SLICE:
        {
            assert(rightValue->size() >= 2);
            
            const ArrayValue* args = dynamic_cast<ArrayValue*>(rightValue);
            assert(args != NULL); // Parser makes sure.

            auto_ptr<ArrayValue> slice(new ArrayValue());
            
            // Determine the stepping of the slice.
            dint step = 1;
            if(args->size() >= 3)
            {
                step = args->elements()[2]->asNumber();
                if(!step)
                {
                    throw SliceError("OperatorExpression::evaluate",
                        operatorToText(op_) + " cannot use zero as step");
                }
            }

            dint leftSize = leftValue->size();
            dint begin = 0;
            dint end = leftSize;
            bool unspecifiedStart = false;
            bool unspecifiedEnd = false;
            
            // Check the start index of the slice.
            const Value* startValue = args->elements()[0];
            if(dynamic_cast<const NumberValue*>(startValue))
            {
                begin = startValue->asNumber();
            }
            else if(dynamic_cast<const ObjectValue*>(startValue))
            {
                if(startValue->compare(ObjectValue()))
                {
                    throw SliceError("OperatorExpression::evaluate",
                        operatorToText(op_) + " requires a number value for the slice start "
                        "(got an object)");
                }
                unspecifiedStart = true;
            }
            else
            {
                throw SliceError("OperatorExpression::evaluate",
                    operatorToText(op_) + " requires a number value for the slice start");
            }

            // Check the end index of the slice.
            const Value* endValue = args->elements()[1];
            if(dynamic_cast<const NumberValue*>(endValue))
            {
                end = endValue->asNumber();
            }
            else if(dynamic_cast<const ObjectValue*>(endValue))
            {
                if(endValue->compare(ObjectValue()))
                {
                    throw SliceError("OperatorExpression::evaluate",
                        operatorToText(op_) + " requires a number value for the slice end "
                        "(got an object)");
                }
                unspecifiedEnd = true;
            }
            else
            {
                throw SliceError("OperatorExpression::evaluate",
                    operatorToText(op_) + " requires a number value for the slice end");
            }

            // Convert them to positive indices.
            if(begin < 0)
            {
                begin += leftSize;
            }
            if(end < 0)
            {
                end += leftSize;
            }
            if(end > begin && step < 0 || begin > end && step > 0)
            {
                // The step goes to the wrong direction.
                begin = end = 0;
            }

            // Full reverse range?
            if(unspecifiedStart && unspecifiedEnd && step < 0)
            {
                begin = leftSize - 1;
                end = -1;
            }
            
            begin = clamp(0, begin, leftSize - 1);
            end = clamp(-1, end, leftSize);
                        
            for(dint i = begin; (end >= begin && i < end) || (begin > end && i > end); i += step)
            {
                slice->add(leftValue->element(NumberValue(i))->duplicate());
            }        
        
            result = slice.release();
            break;
        }
        
    default:
        throw std::logic_error("Operator " + operatorToText(op_) + " not implemented");
    }

    // Delete the unnecessary values.
    if(result != rightValue) delete rightValue;
    if(result != leftValue) delete leftValue;
    
    return result;
#endif
    return new NoneValue();
}
