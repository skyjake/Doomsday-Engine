/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

    virtual ~Context();

    /// Determines the type of the execution context.
    Type type() { return _type; }

    /// Returns the process that owns this context.
    Process &process() { return *_owner; }

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
     * Returns the throwaway variable. This can be used for dumping
     * values that are not needed. For instance, the weak assignment operator
     * will use this when the identifier already exists.
     */
    Variable &throwaway() { return _throwaway; }

private:
    /**
     * Information about the control flow is stored within a stack of
     * ControlFlow instances.
     */
    class ControlFlow {
    public:
        /**
         * Constructor.
         *
         * @param current  Current statement being executed.
         * @param f        Statement where normal flow continues.
         * @param c        Statement where to jump on "continue".
         *                 @c NULL if continuing is not allowed.
         * @param b        Statement where to jump to and flow from on "break".
         *                 @c NULL if breaking is not allowed.
         */
        ControlFlow(Statement const *current,
                    Statement const *f = 0,
                    Statement const *c = 0,
                    Statement const *b = 0)
            : flow(f), jumpContinue(c), jumpBreak(b), iteration(0), _current(current) {}

        /// Returns the currently executed statement.
        Statement const *current() const { return _current; }

        /// Sets the currently executed statement. When the statement
        /// changes, the phase is reset back to zero.
        void setCurrent(Statement const *s) { _current = s; }

    public:
        Statement const *flow;
        Statement const *jumpContinue;
        Statement const *jumpBreak;
        Value *iteration;

    private:
        Statement const *_current;
    };

private:
    /// Returns the topmost control flow information.
    ControlFlow &flow() { return _controlFlow.back(); }

    /// Pops the topmost control flow instance off of the stack. The
    /// iteration value is deleted, if it has been defined.
    void popFlow();

    /// Sets the currently executed statement.
    void setCurrent(Statement const *statement);

private:
    /// Type of the execution context.
    Type _type;

    /// The process that owns this context.
    Process *_owner;

    /// Control flow stack.
    typedef std::vector<ControlFlow> FlowStack;
    FlowStack _controlFlow;

    /// Expression evaluator.
    Evaluator _evaluator;

    /// Determines whether the namespace is owned by the context.
    bool _ownsNamespace;

    /// The local namespace of this context.
    Record *_names;

    Variable _throwaway;
};

} // namespace de

#endif /* LIBDENG2_CONTEXT_H */
