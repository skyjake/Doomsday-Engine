/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_COMMANDLINE_H
#define LIBDENG2_COMMANDLINE_H

#include <string>
#include <vector>
#include <map>

#include <de/deng.h>
#include <de/Error>

namespace de
{
    /**
     * Stores and provides access to the command line arguments passed
     * to an application at launch.
     */
    class PUBLIC_API CommandLine
    {
    public:
        DEFINE_ERROR(OutOfRangeError);
        DEFINE_ERROR(ExecuteError);
        
    public:
        /**
         * Constructs a CommandLine out of the provided strings. It is assumed
         * that @c argc and @c args are the ones passed from the system to the main() 
         * function. The strings that begin with a @@ character are parsed, the
         * rest are used without modification.
         *
         * @param argc  Number of argument strings.
         * @param args  The argument strings.
         */
        CommandLine(int argc, char** args);

        CommandLine(const CommandLine& other);        

        /**
         * Returns the number of arguments. This includes the program name, which
         * is the first argument in the list.
         */
        dint count() const { return arguments_.size(); }

        void clear();

        /**
         * Appends a new argument to the list of arguments.
         *
         * @param arg  Argument to append.
         */
        void append(const std::string& arg);

        /**
         * Inserts a new argument to the list of arguments at index @c pos.
         * 
         * @param pos  Index at which the new argument will be at.
         */
        void insert(duint pos, const std::string& arg);

        /**
         * Removes an argument by index.
         *
         * @param pos  Index of argument to remove.
         */
        void remove(duint pos);

        /**
         * Checks whether @c arg is in the arguments. Since the first argument is
         * the program name, it is not included in the search.
         *
         * @param arg  Argument to look for. Don't use aliases here.
         * @param count  Number of non-option arguments that follow the located argument.
         *               @see isOption()
         *
         * @return  Index of the argument, if found. Otherwise zero.
         */
        dint check(const std::string& arg, dint count = 0) const;

        /**
         * Determines whether @c arg exists in the list of arguments.
         *
         * @param arg  Argument to look for. Don't use aliases here.
         *
         * @return  Number of times @c arg is found in the arguments.
         */
        dint has(const std::string& arg) const;

        /**
         * Determines whether an argument is an option, i.e., it begins with a hyphen.
         */
        bool isOption(duint pos) const;

        /**
         * Determines whether an argument is an option, i.e., it begins with a hyphen.
         */
        static bool isOption(const std::string& arg);

        const std::string& at(duint pos) const { return arguments_.at(pos); }

        /**
         * Returns a list of pointers to the arguments. The list contains
         * count() strings and is NULL-terminated.
         */
        const char* const* argv() const;
        
        /**
         * Breaks down a single string containing the arguments.
         *
         * Examples of behavior:
         * - -cmd "echo ""this is a command""" => [-cmd] [echo "this is a command"]
         * - Hello" My"Friend => [Hello MyFriend]
         * - @@test.rsp [reads contents of test.rsp]
         * - @@\"Program Files"\test.rsp [reads contents of "\Program Files\test.rsp"]
         *
         * @param cmdLine  String containing the arguments.
         */ 
        void parse(const std::string& cmdLine);
        
        /**
         * Defines a new alias for a full argument.
         *
         * @param full  The full argument, e.g., "--help"
         * @param alias  Alias for the full argument, e.g., "-h"
         */
        void alias(const std::string& full, const std::string& alias);

        bool matches(const std::string& full, const std::string& fullOrAlias) const;
        
        /**
         * Spawns a new process using the command line. The first argument
         * specifies the file name of the executable. Returns immediately
         * after the process has been started.
         *
         * @param envs  Environment variables passed to the new process.
         */
        void execute(char** envs) const;
        
    private:
        typedef std::vector<std::string> Arguments;
        Arguments arguments_;
    
        typedef std::vector<const char*> ArgumentPointers;
        ArgumentPointers pointers_;
        
        typedef std::map<std::string, Arguments> Aliases;
        Aliases aliases_;
    };
}

#endif /* LIBDENG2_COMMANDLINE_H */
