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

#ifndef LIBDENG2_PROCESS_H
#define LIBDENG2_PROCESS_H

#include "../Script"
#include "../Time"
#include "../Context"
#include "../Function"
#include "../String"

#include <utility>

namespace de
{
    class Variable;
    class ArrayValue;
    
    /**
     * Runs a script. The process maintains the execution environment, including things
     * like local variables and keeping track of which statement is being executed.
     * 
     * @ingroup script
     */
    class Process
    {
    public:
        /// A variable or function with the same name already exists when a new one is
        /// being created. @ingroup errors
        DEFINE_ERROR(AlreadyExistsError);
        
        /// The process is running while an operation is attempted that requires the 
        /// process to be stopped. @ingroup errors
        DEFINE_ERROR(NotStoppedError);
        
        /// Suspending or resuming fails. @ingroup errors
        DEFINE_ERROR(SuspendError);
        
        /// Execution is taking too long to complete. @ingroup errors
        DEFINE_ERROR(HangError);
        
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

    public:
        /**
         * Constructs a new process. The process is initialized to RUNNING state.
         *
         * @param script  Script to run.
         */
        Process(const Script& script);
        
        virtual ~Process();

        State state() const { return state_; }

        /**
         * Starts running the given script. Note that the process must be
         * in the FINISHED state for this to be a valid operation.
         *
         * @param script  Script to run.
         */
        void run(const Script& script);

        /**
         * Suspends or resumes execution of the script.
         */
        void suspend(bool suspended = true);
        
        /**
         * Stops the execution of the script. The process is set to the 
         * FINISHED state, which means it must be rerun with a new script.
         */
        void stop();

        /*
         * Execute the next command(s) in the script.
         *
         * @param timeBox  If defined, execution will continue at most for this
         *                 period of time. By default execution continues until
         *                 the script leaves RUNNING state.
         */
        void execute(const Time::Delta& timeBox = 0);

        /**
         * Finish the execution of the topmost context. Execution will 
         * continue from the second topmost context.
         * 
         * @param returnValue  Value to use as the return value from the 
         *                     context. Takes ownership of the value.
         */
        void finish(Value* returnValue = 0);

        /**
         * Changes the working path of the process. File system access within the 
         * process is relative to the working path.
         *
         * @param newWorkingPath  New working path for the process.
         */
        void setWorkingPath(const String& newWorkingPath);

        /**
         * Returns the current working path.
         */
        const String& workingPath() const;

        /**
         * Creates a new variable in the location specified in the path.
         *
         * @param names  Namespace where the variable is to be created.
         * @param path  Path of the variable. Also contains the name of the variable.
         * @param initialValue  Initial value of the variable. The 
         *      variable takes owernship of the Value.
         *
         * @return The new Variable.
         */
        //Variable* newVariable(Folder& names, const std::string& path, Value* initialValue);

        /**
         * Creates a new method in location specified in the path.
         *
         * @param path          Path of the method. Also specifies the name of the method.
         * @param arguments     Argument names for the method. Default values within
         *                      the map are owned by the new Method.
         * @param defaults      Default values for some or all of the arguments.
         * @param firstStatement  First statement of the method.
         * 
         * @return The new Method.
         */
        //Method* newMethod(const std::string& path, const Method::Arguments& arguments, 
        //    const Method::Defaults& defaults, const Statement* firstStatement);

        /// Return the current topmost execution context.
        Context& context();
        
        /// A method call.
        //void call(Method& method, Object& self, const ArrayValue* arguments = 0);
        
    private:
        State state_;

        // The execution environment.
        typedef std::vector<Context*> ContextStack;
        ContextStack stack_;
        
        /// This is the current working folder of the process. Relative paths
        /// given to workingFile() are located in relation to this
        /// folder. Initial value is the root folder.
        String workingPath_;
        
        /// The time left to sleep.  If this is larger than zero, the
        /// process will not continue until enough time has elapsed. 
        Time sleepUntil_;

        /// @c true, if the process has not executed a single statement yet.
        bool firstExecute_;
    };
}

#endif /* LIBDENG2_PROCESS_H */
