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

#ifndef LIBDENG2_EVALUATOR_H
#define LIBDENG2_EVALUATOR_H

#include <vector>

namespace de
{
    class Context;
    class Process;
    class Expression;
    class Value;
    
    /**
     * Stack for evaluating expressions.
     */
    class Evaluator
    {
    public:
        Evaluator(Context& owner);
        ~Evaluator();

        Context& context() { return context_; }

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
        Context& context_;
        
        struct ScopedExpression {
            const Expression* expression;
            Record* names;
            ScopedExpression(const Expression* e = 0, Record* n = 0) : expression(e), names(n) {}
        };
        typedef std::vector<ScopedExpression> Expressions;
        typedef std::vector<Value*> Results;

        /// The expression that is currently being evaluated.
        const Expression* current_;
        
        /// Namespace for the current expression.
        Record* names_;
        
        Expressions stack_;
        Results results_;
    };
}

#endif /* LIBDENG2_EVALUATOR_H */
