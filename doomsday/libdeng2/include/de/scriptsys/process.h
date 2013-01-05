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

#ifndef LIBDENG2_PROCESS_H
#define LIBDENG2_PROCESS_H

#include "../Script"
#include "../Time"
#include "../Context"
#include "../Function"
#include "../String"
#include "../Variable"

#include <list>

namespace de
{
    class ArrayValue;
    
    /**
     * Executes a script. The process maintains the execution environment, including things
     * like local variables and keeping track of which statement is being executed.
     * 
     * @ingroup script
     */
    class DENG2_PUBLIC Process
    {
    public:
        /// The process is running while an operation is attempted that requires the 
        /// process to be stopped. @ingroup errors
        DENG2_ERROR(NotStoppedError);
        
        /// Suspending or resuming fails. @ingroup errors
        DENG2_ERROR(SuspendError);
        
        /// Execution is taking too long to complete. @ingroup errors
        DENG2_ERROR(HangError);
        
        /// A process is always in one of these states.
        enum State {
            RUNNING,    /**< The process is running normally. */
            SUSPENDED,  /**< The process has been suspended and will
                         *   not continue running until restored.  A
                         *   process cannot restore itself from a
                         *   suspended state. */
            STOPPED     /**< The process has reached the end of the
                         *   script or has been terminated. */
        };
        
        typedef std::list<Record *> Namespaces;

    public:
        /**
         * Constructs a new process. The process is initialized to STOPPED state.
         *
         * @param externalGlobalNamespace  Namespace to use as the global namespace
         *                                 of the process. If not specified, a private
         *                                 global namespace is created for the process.
         *                                 The process does not get ownership of the
         *                                 external global namespace.
         */
        Process(Record *externalGlobalNamespace = 0);
        
        /**
         * Constructs a new process. The process is initialized to RUNNING state.
         *
         * @param script  Script to run. No reference to the script is retained
         *                apart from pointing to its statements. The script instance
         *                must remain in existence while the process is running,
         *                as it is the owner of the statements.
         */
        Process(Script const &script);
        
        virtual ~Process();

        State state() const { return _state; }

        /// Determines the current depth of the call stack.
        duint depth() const;

        /**
         * Starts running the given script. Note that the process must be
         * in the FINISHED state for this to be a valid operation.
         *
         * @param script  Script to run. No reference to the script is retained
          *               apart from pointing to its statements. The script instance
          *               must remain in existence while the process is running,
          *               as it is the owner of the statements.
         */
        void run(Script const &script);

        /**
         * Suspends or resumes execution of the script.
         */
        void suspend(bool suspended = true);
        
        /**
         * Stops the execution of the script. The process is set to the 
         * FINISHED state, which means it must be rerun with a new script.
         */
        void stop();

        /**
         * Execute the next command(s) in the script. Execution continues until
         * the script leaves RUNNING state.
         */
        void execute();

        /**
         * Finish the execution of the topmost context. Execution will 
         * continue from the second topmost context.
         * 
         * @param returnValue  Value to use as the return value from the 
         *                     context. Takes ownership of the value.
         */
        void finish(Value *returnValue = 0);

        /**
         * Changes the working path of the process. File system access within the 
         * process is relative to the working path.
         *
         * @param newWorkingPath  New working path for the process.
         */
        void setWorkingPath(String const &newWorkingPath);

        /**
         * Returns the current working path.
         */
        String const &workingPath() const;

        /**
         * Return an execution context. By default returns the topmost context.
         *
         * @param downDepth  How many levels to go down. There are depth() levels
         *                   in the context stack.
         *
         * @return  Context at @a downDepth.
         */
        Context &context(duint downDepth = 0);
                
        /**
         * Performs a function call. A new context is created on the context 
         * stack and the function's statements are executed on the new stack.
         * After the call is finished, the result value is pushed to the
         * calling context's evaluator.
         *
         * @param function   Function to call.
         * @param arguments  Arguments for the function. The array's first element
         *                   must be a DictionaryValue containing values for the 
         *                   named arguments of the call. The rest of the array
         *                   are the unnamed arguments.
         */
        void call(Function const &function, ArrayValue const &arguments);
        
        /**
         * Collects the namespaces currently visible. This includes the process's
         * own stack and the global namespaces.
         *
         * @param spaces  The namespaces are collected here. The order is important:
         *                earlier namespaces shadow the subsequent ones.
         */
        void namespaces(Namespaces &spaces);

        /**
         * Returns the global namespace of the process. This is always the 
         * bottommost context in the stack.
         */
        Record &globals();
        
    protected:
        /// Pops contexts off the stack until depth @a downToLevel is reached.
        void clearStack(duint downToLevel = 0);
        
        /// Pops the topmost context off the stack and returns it. Ownership given
        /// to caller.
        Context *popContext();
        
        /// Fast forward to a suitable catch statement for @a err.
        /// @return  @c true, if suitable catch statement found.
        bool jumpIntoCatch(Error const &err);
                
    private:
        State _state;
        
        // The execution environment.
        typedef std::vector<Context *> ContextStack;
        ContextStack _stack;
        
        /// This is the current working folder of the process. Relative paths
        /// given to workingFile() are located in relation to this
        /// folder. Initial value is the root folder.
        String _workingPath;
        
        /// Time when execution was started at depth 1.
        Time _startedAt;
    };
}

#endif /* LIBDENG2_PROCESS_H */
