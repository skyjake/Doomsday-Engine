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

#include "de/OperatorExpression"
#include "de/Evaluator"
#include "de/Value"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/RefValue"
#include "de/RecordValue"
#include "de/NoneValue"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

namespace de {

OperatorExpression::OperatorExpression() : _op(NONE), _leftOperand(0), _rightOperand(0)
{}

OperatorExpression::OperatorExpression(Operator op, Expression *operand)
    : _op(op), _leftOperand(0), _rightOperand(operand)
{
    if(op != PLUS && op != MINUS && op != NOT)
    {
        throw NonUnaryError("OperatorExpression::OperatorExpression",
            "Unary " + operatorToText(op) + " not defined");
    }
}

OperatorExpression::OperatorExpression(Operator op, Expression *leftOperand, Expression *rightOperand)
    : _op(op), _leftOperand(leftOperand), _rightOperand(rightOperand)
{
    if(op == NOT)
    {
        throw NonBinaryError("OperatorExpression::OperatorExpression",
            "Binary " + operatorToText(op) + " not defined");
    }
}

OperatorExpression::~OperatorExpression()
{
    delete _leftOperand;
    delete _rightOperand;
}

void OperatorExpression::push(Evaluator &evaluator, Record *names) const
{
    Expression::push(evaluator);
    
    if(_op == MEMBER)
    {
        // The MEMBER operator works a bit differently. Just push the left side
        // now. We'll push the other side when we've found out what is the 
        // scope defined by the result of the left side (which must be a RecordValue).
        _leftOperand->push(evaluator, names);
    }
    else
    {
        _rightOperand->push(evaluator);
        if(_leftOperand)
        {
            _leftOperand->push(evaluator, names);
        }
    }
}

Value *OperatorExpression::newBooleanValue(bool isTrue)
{
    return new NumberValue(isTrue? NumberValue::True : NumberValue::False,
                           NumberValue::Boolean);
}

void OperatorExpression::verifyAssignable(Value *value)
{
    if(!dynamic_cast<RefValue *>(value))
    {
        throw NotAssignableError("OperatorExpression::verifyAssignable",
            "Cannot assign to: " + value->asText());
    }
}

Value *OperatorExpression::evaluate(Evaluator &evaluator) const
{
    // Get the operands.
    Value *rightValue = (_op == MEMBER? 0 : evaluator.popResult());
    Value *leftValue = (_leftOperand? evaluator.popResult() : 0);
    Value *result = (leftValue? leftValue : rightValue);

    try
    {
        switch(_op)
        {
        case PLUS:
            if(leftValue)
            {
                leftValue->sum(*rightValue);
            }
            else
            {
                // Unary plus is a no-op.
            }
            break;

        case PLUS_ASSIGN:
            verifyAssignable(leftValue);
            leftValue->sum(*rightValue);
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
            verifyAssignable(leftValue);
            leftValue->subtract(*rightValue);
            break;    

        case DIVIDE:
            leftValue->divide(*rightValue);
            break;

        case DIVIDE_ASSIGN:
            verifyAssignable(leftValue);
            leftValue->divide(*rightValue);
            break;    

        case MULTIPLY:
            leftValue->multiply(*rightValue);
            break;

        case MULTIPLY_ASSIGN:
            verifyAssignable(leftValue);
            leftValue->multiply(*rightValue);
            break;    

        case MODULO:
            leftValue->modulo(*rightValue);
            break;

        case MODULO_ASSIGN:
            verifyAssignable(leftValue);
            leftValue->modulo(*rightValue);
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

        case INDEX:
        {
            LOG_DEV_TRACE("INDEX: types %s [ %s ] byref:%b",
                          DENG2_TYPE_NAME(*leftValue) << DENG2_TYPE_NAME(*rightValue)
                          << flags().testFlag(ByReference));

            // As a special case, records can be indexed also by reference.
            RecordValue *recValue = dynamic_cast<RecordValue *>(leftValue);
            if(flags().testFlag(ByReference) && recValue)
            {
                result = new RefValue(&recValue->dereference()[rightValue->asText()]);
            }
            else
            {
                // Index by value.
                result = leftValue->duplicateElement(*rightValue);
            }
            break;
        }

        case SLICE:
            result = performSlice(leftValue, rightValue);
            break;

        case MEMBER: 
        {
            RecordValue const *recValue = dynamic_cast<RecordValue const *>(leftValue);
            if(!recValue)
            {
                throw ScopeError("OperatorExpression::evaluate",
                    "Left side of " + operatorToText(_op) + " must evaluate to a record");
            }
            
            // Now that we know what the scope is, push the rest of the expression
            // for evaluation (in this specific scope).
            _rightOperand->push(evaluator, recValue->record());
            
            // Cleanup.
            delete leftValue;
            DENG2_ASSERT(rightValue == NULL);

            // The MEMBER operator does not evaluate to any result. 
            // Whatever is on the right side will be the result.
            return NULL;
        }

        default:
            throw Error("OperatorExpression::evaluate", 
                "Operator " + operatorToText(_op) + " not implemented");
        }
    }
    catch(Error const &)
    {
        delete rightValue;
        delete leftValue;
        throw;
    }

    // Delete the unnecessary values.
    if(result != rightValue) delete rightValue;
    if(result != leftValue) delete leftValue;
    
    return result;
}

// Flags for serialization:
static duint8 const HAS_LEFT_OPERAND = 0x80;
static duint8 const OPERATOR_MASK    = 0x7f;

void OperatorExpression::operator >> (Writer &to) const
{
    to << SerialId(OPERATOR);

    Expression::operator >> (to);

    duint8 header = _op;
    if(_leftOperand)
    {
        header |= HAS_LEFT_OPERAND;
    }
    to << header << *_rightOperand;
    if(_leftOperand)
    {
        to << *_leftOperand;
    }
}

void OperatorExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != OPERATOR)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("OperatorExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    duint8 header;
    from >> header;
    _op = Operator(header & OPERATOR_MASK);

    delete _leftOperand;
    delete _rightOperand;
    _leftOperand = 0;
    _rightOperand = 0;
    
    _rightOperand = Expression::constructFrom(from);
    if(header & HAS_LEFT_OPERAND)
    {
        _leftOperand = Expression::constructFrom(from);
    }
}

namespace internal {
    struct SliceTarget {
        SliceTarget(Value *v) : value(v) {}
        virtual ~SliceTarget() {
            delete value;
        }
        Value *take() {
            Value *v = value;
            value = 0;
            return v;
        }
        virtual void append(Value const &src, dint index) = 0;
        Value *value;
    };
    struct ArraySliceTarget : public SliceTarget {
        ArraySliceTarget() : SliceTarget(new ArrayValue) {}
        ArrayValue &array() { return *static_cast<ArrayValue *>(value); }
        void append(Value const &src, dint index) {
            array().add(src.duplicateElement(NumberValue(index)));
        }
    };
    struct TextSliceTarget : public SliceTarget {
        TextSliceTarget() : SliceTarget(new TextValue) {}
        TextValue &text() { return *static_cast<TextValue *>(value); }
        void append(Value const &src, dint index) {
            text().sum(TextValue(String(1, src.asText().at(index))));
        }
    };
}

Value *OperatorExpression::performSlice(Value *leftValue, Value *rightValue) const
{
    using internal::SliceTarget;
    using internal::TextSliceTarget;
    using internal::ArraySliceTarget;

    DENG2_ASSERT(rightValue->size() >= 2);

    ArrayValue const *args = dynamic_cast<ArrayValue *>(rightValue);
    DENG2_ASSERT(args != NULL); // Parser makes sure.

    // The resulting slice of leftValue's elements.
    std::auto_ptr<SliceTarget> slice;
    if(dynamic_cast<TextValue *>(leftValue))
    {
        slice.reset(new TextSliceTarget);
    }
    else
    {
        slice.reset(new ArraySliceTarget);
    }

    // Determine the stepping of the slice.
    dint step = 1;
    if(args->size() >= 3)
    {
        step = dint(args->elements()[2]->asNumber());
        if(!step)
        {
            throw SliceError("OperatorExpression::evaluate",
                operatorToText(_op) + " cannot use zero as step");
        }
    }

    dint leftSize = leftValue->size();
    dint begin = 0;
    dint end = leftSize;
    bool unspecifiedStart = false;
    bool unspecifiedEnd = false;

    // Check the start index of the slice.
    Value const *startValue = args->elements()[0];
    if(dynamic_cast<NoneValue const *>(startValue))
    {
        unspecifiedStart = true;
    }
    else
    {
        begin = dint(startValue->asNumber());
    }

    // Check the end index of the slice.
    Value const *endValue = args->elements()[1];
    if(dynamic_cast<NoneValue const *>(endValue))
    {
        unspecifiedEnd = true;
    }
    else
    {
        end = dint(endValue->asNumber());
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
    if((end > begin && step < 0) || (begin > end && step > 0))
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
        slice->append(*leftValue, i);
    }

    return slice->take();
}

} // namespace de
