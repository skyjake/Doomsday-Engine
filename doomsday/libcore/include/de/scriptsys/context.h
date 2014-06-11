/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBDENG2_CONTEXT_H
#define LIBDENG2_CONTEXT_H

#include "../Evaluator"
#include "../Record"

namespace de {

class Statement;
class Process;

/**
 * Entry in the process's call stack.
 *
 * @ingroup script
 */
class DENG2_PUBLIC Context
{
public:
    /// Attempting a jump when there is no suitable target (continue or break). @ingroup errors
    DENG2_ERROR(JumpError);

    /// There is no instance scope defined for the context. @ingroup errors
    DENG2_ERROR(UndefinedScopeError);

    enum Type {
        BaseProcess,
        GlobalNamespace,
        FunctionCall
    };

public:
    /**
     * Constructor.
     *
     * @param type     Type of the execution context.
     * @param owner    Process that owns the context.
     * @param globals  Optionally a global namespace. Lookups will stop here.
     */
    Context(Type type, Process *owner, Record *globals = 0);

    /// Determines the type of the execution context.
    Type type();

    /// Returns the process that owns this context.
    Process &process();

    /// Returns the namespace of the context.
    Record &names();

    /// Returns the expression evaluator of the context.
    Evaluator &evaluator();

    /**
     * Start the execution of a series of statements.
     *
     * @param statement     The first statement to execute.
     * @param flow          The statement to continue program flow when no more
     *                      statements follow the current one.
     * @param jumpContinue  The statement to jump to when a "continue"
     *                      statement is encountered.
     * @param jumpBreak     The statement to jump to when a "break" statement
     *                      is encountered.
     */
    void start(Statement const *statement,
               Statement const *flow = NULL,
               Statement const *jumpContinue = NULL,
               Statement const *jumpBreak = NULL);

    /**
     * Clears the evaluator and control flow. Does not empty the namespace.
     * This needs to be called if the process is aborted.
     */
    void reset();

    /// Returns the currently executed statement.
    /// @return Statement, or @c NULL if no control flow information exists.
    Statement const *current();

    /**
     * Execute the current statement.
     *
     * @return @c false if there are no more statements to execute.
     */
    bool execute();

    /**
     * Proceed to the next statement as dictated by the control flow.
     */
    void proceed();

    /**
     * Jump to the topmost continue target in the control flow stack.
     * If there are no continue targets available, an error will be
     * thrown.
     */
    void jumpContinue();

    /**
     * Jump to the topmost break target in the control flow stack.
     * If there are no break targets available, an error will be
     * thrown.
     *
     * @param count  Number of times to break out. Allows breaking
     *               out of nested loops.
     */
    void jumpBreak(duint count = 1);

    /// Returns the current iteration value of the context.
    Value *iterationValue();

    /**
     * Sets the iteration value of the context.
     *
     * @param value  Value to be iterated within the context.
     */
    void setIterationValue(Value *value);

    /**
     * Sets the instance scope of the context. This is equivalent to the value
     * of the "self" variable, however only used for native function calls.
     *
     * @param scope  Value that specifies the instance whose scope the context
     *               is being evaluated in. Ownership taken.
     */
    void setInstanceScope(Value *scope);

    /**
     * Returns the current instance scope. A scope must exist if this is called.
     */
    Value &instanceScope() const;

    /**
     * Returns the throwaway variable. This can be used for dumping
     * values that are not needed. For instance, the weak assignment operator
     * will use this when the identifier already exists.
     */
    Variable &throwaway();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG2_CONTEXT_H */
