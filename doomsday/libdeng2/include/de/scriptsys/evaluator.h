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

#ifndef LIBDENG2_EVALUATOR_H
#define LIBDENG2_EVALUATOR_H

#include "../libdeng2.h"
#include "../NoneValue"

#include <vector>
#include <list>

namespace de
{
    class Context;
    class Process;
    class Expression;
    class Value;
    class Record;
    
    /**
     * Stack for evaluating expressions.
     *
     * @ingroup script
     */
    class DENG2_PUBLIC Evaluator
    {
    public:
        /// Result is of wrong type. @ingroup errors
        DENG2_ERROR(ResultTypeError);
        
        typedef std::list<Record*> Namespaces;
    
    public:
        Evaluator(Context& owner);
        ~Evaluator();

        Context& context() { return _context; }

        /**
         * Returns the process that owns this evaluator.
         */
        Process& process();

        /**
         * Resets the evaluator so it's ready for another expression.
         * Called when the statement changes in the context.
         */
        void reset();
        
        /**
         * Fully evaluate the given expression. The result value will remain 
         * in the results stack.
         *
         * @return  Result of the evaluation.
         */
        Value& evaluate(const Expression* expression);

        template <typename Type>
        Type& evaluateTo(const Expression* expr) {
            Type* r = dynamic_cast<Type*>(&evaluate(expr));
            if(!r) {
                throw ResultTypeError("Evaluator::result<Type>", "Unexpected result type");
            }
            return *r;
        }

        /**
         * Determines the namespace for the currently evaluated expression.
         * Those expressions whose operation depends on the current
         * namespace scope, should use this to look identifiers from.
         * This changes as expressions are popped off the stack.
         * 
         * @return  Namespace scope of the current evaluation. If @c NULL,
         *          expressions should assume that all namespaces are available.
         */
        Record* names() const;

        /**
         * Collect the namespaces currently visible.
         *
         * @param spaces  List of namespaces. The order is important: the earlier
         *                namespaces shadow the subsequent ones.
         */
        void namespaces(Namespaces& spaces);

        /**
         * Insert the given expression to the top of the expression stack.
         *
         * @param expression  Expression to push on the stack.
         * @param names       Namespace scope for this expression only.
         */
        void push(const Expression* expression, Record* names = 0);

        /**
         * Push a value onto the result stack.
         *
         * @param value  Value to push on the result stack. The evaluator
         *               gets ownership of the value.
         */
        void pushResult(Value* value);
        
        /**
         * Pop a value off of the result stack. 
         *
         * @return  Value resulting from expression evaluation. Caller
         *          gets ownership of the returned object.
         */
        Value* popResult();
        
        /**
         * Pop a value off of the result stack, making sure it has a specific type.
         *
         * @return  Value resulting from expression evaluation. Caller
         *          gets ownership of the returned object.
         */
        template <typename Type>
        Type* popResultAs() {
            if(!dynamic_cast<Type*>(&result())) {
                throw ResultTypeError("Evaluator::result<Type>", 
                    "Result type is not compatible with Type");
            }
            return dynamic_cast<Type*>(popResult());
        }
        
        /**
         * Determines whether a final result has been evaluated.
         */
        bool hasResult() const;

        /**
         * Determines the result of the evaluation without relinquishing
         * ownership of the value instances.
         *
         * @return  The final result of the evaluation. 
         */
        Value& result();
                
    private:
        void clearNames();
        void clearResults();
        void clearStack();
        
        /// The context that owns this evaluator.
        Context& _context;
        
        struct ScopedExpression {
            const Expression* expression;
            Record* names;
            ScopedExpression(const Expression* e = 0, Record* n = 0) : expression(e), names(n) {}
        };
        typedef std::vector<ScopedExpression> Expressions;
        typedef std::vector<Value*> Results;

        /// The expression that is currently being evaluated.
        const Expression* _current;
        
        /// Namespace for the current expression.
        Record* _names;
        
        Expressions _stack;
        Results _results;
        
        /// Returned when there is no result to give.
        NoneValue _noResult;
    };
}

#endif /* LIBDENG2_EVALUATOR_H */
