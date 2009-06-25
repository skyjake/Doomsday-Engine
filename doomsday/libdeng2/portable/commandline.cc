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

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#ifdef UNIX
#   include <unistd.h>
#endif

#include "de/commandline.h"
#include "de/string.h"

#include <fstream>
#include <sstream>
#include <cctype>

#include <doomsday.h>

using namespace de;

CommandLine::CommandLine(int argc, char** v)
{
    // The pointers list is kept null terminated.
    pointers_.push_back(0);

    for(int i = 0; i < argc; ++i)
    {
        if(v[i][0] == '@')
        {
            // This is a response file or something else that requires parsing.
            parse(v[i]);
        }
        else
        {
            arguments_.push_back(v[i]);
            pointers_.insert(pointers_.end() - 1, arguments_[i].c_str());
        }
    }
}

CommandLine::CommandLine(const CommandLine& other)
    : arguments_(other.arguments_)
{
    // Use pointers to the already copied strings.
    for(Arguments::iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        pointers_.push_back(i->c_str());
    }
    pointers_.push_back(0);
}

void CommandLine::clear()
{
    arguments_.clear();
    pointers_.clear();
    pointers_.push_back(0);
}

void CommandLine::append(const std::string& arg)
{
    arguments_.push_back(arg);
    pointers_.insert(pointers_.end() - 1, arguments_.rbegin()->c_str());
}

void CommandLine::insert(duint pos, const std::string& arg)
{
    if(pos > arguments_.size())
    {
        throw OutOfRangeError("CommandLine::insert", "Index out of range");
    }
    arguments_.insert(arguments_.begin() + pos, arg);
    pointers_.insert(pointers_.begin() + pos, arguments_[pos].c_str());
}

void CommandLine::remove(duint pos)
{
    if(pos >= arguments_.size())
    {
        throw OutOfRangeError("CommandLine::remove", "Index out of range");
    }
    arguments_.erase(arguments_.begin() + pos);
    pointers_.erase(pointers_.begin() + pos);
}

dint CommandLine::check(const std::string& arg, dint numParams) const
{
    // Do a search for arg.
    Arguments::const_iterator i = arguments_.begin();
    for(; i != arguments_.end() && !matches(arg, *i); ++i);
    
    if(i == arguments_.end())
    {
        // Not found.
        return 0;
    }
    
    // It was found, check for the number of non-option parameters.
    Arguments::const_iterator k = i;
    while(numParams-- > 0)
    {
        if(++k == arguments_.end() || isOption(*k))
        {
            // Ran out of arguments, or encountered an option.
            return 0;
        }
    }
    
    return i - arguments_.begin();
}

dint CommandLine::has(const std::string& arg) const
{
    dint howMany = 0;
    
    for(Arguments::const_iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        if(matches(arg, *i))
        {
            howMany++;
        }
    }
    return howMany;
}

bool CommandLine::isOption(duint pos) const
{
    if(pos >= arguments_.size())
    {
        throw OutOfRangeError("CommandLine::isOption", "Index out of range");
    }
    assert(!arguments_[pos].empty());
    return isOption(arguments_[pos]);
}

bool CommandLine::isOption(const std::string& arg)
{
    return !(arg.empty() || arg[0] != '-');
}

const char* const* CommandLine::argv() const
{
    assert(*pointers_.rbegin() == 0);
    return &pointers_[0];
}

void CommandLine::parse(const std::string& cmdLine)
{
    std::string::const_iterator i = cmdLine.begin();

    // This is unset if we encounter a terminator token.
    bool isDone = false;

    // Are we currently inside quotes?
    bool quote = false;

    while(i != cmdLine.end() && !isDone)
    {
        // Skip initial whitespace.
        String::skipSpace(i, cmdLine.end());
        
        // Check for response files.
        bool isResponse = false;
        if(*i == '@')
        {
            isResponse = true;
            String::skipSpace(++i, cmdLine.end());
        }

        std::string word;

        while(i != cmdLine.end() && (quote || !std::isspace(*i)))
        {
            bool copyChar = true;
            if(!quote)
            {
                // We're not inside quotes.
                if(*i == '\"') // Quote begins.
                {
                    quote = true;
                    copyChar = false;
                }
            }
            else
            {
                // We're inside quotes.
                if(*i == '\"') // Quote ends.
                {
                    if(i + 1 != cmdLine.end() && *(i + 1) == '\"') // Doubles?
                    {
                        // Normal processing, but output only one quote.
                        i++;
                    }
                    else
                    {
                        quote = false;
                        copyChar = false;
                    }
                }
            }

            if(copyChar)
            {
                word.push_back(*i);
            }
            
            i++;
        }

        // Word has been extracted, examine it.
        if(isResponse) // Response file?
        {
            // This will quietly ignore missing response files.
            std::stringbuf response;
            std::ifstream(word.c_str()) >> &response;
            parse(response.str());
        }
        else if(word == "--") // End of arguments.
        {
            isDone = true;
        }
        else if(!word.empty()) // Make sure there *is* a word.
        {
            arguments_.push_back(word);
            pointers_.insert(pointers_.end() - 1, arguments_.rbegin()->c_str());
        }
    }
}

void CommandLine::alias(const std::string& full, const std::string& alias)
{
    aliases_[full].push_back(alias);
}

bool CommandLine::matches(const std::string& full, const std::string& fullOrAlias) const
{
    if(!String::compareWithoutCase(full, fullOrAlias))
    {
        // They are, in fact, the same.
        return true;
    }
    
    Aliases::const_iterator found = aliases_.find(full);
    if(found != aliases_.end())
    {
        for(Arguments::const_iterator i = found->second.begin(); i != found->second.end(); ++i)
        {
            if(!String::compareWithoutCase(*i, fullOrAlias))
            {
                // Found it among the aliases.
                return true;
            }
        }
    }
    
    return false;
}

void CommandLine::execute(char** envs) const
{
#ifdef UNIX
    // Fork and execute new file.
    pid_t result = fork();
    if(!result)
    {
        // Here we go in the child process.
        printf("Child loads %s...\n", pointers_[0]);
        execve(pointers_[0], const_cast<char* const*>(argv()), const_cast<char* const*>(envs));
    }
    else
    {
        if(result < 0)
        {
            perror("CommandLine::execute");
        }
    }
#endif    

#ifdef WIN32
    std::string quotedCmdLine;
    for(Arguments::const_iterator i = arguments_.begin() + 1; i != arguments_.end(); ++i)
    {
        quotedCmdLine += "\"" + *i + "\" ";
    }

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    ZeroMemory(&processInfo, sizeof(processInfo));

    if(!CreateProcess(pointers_[0], const_cast<char*>(quotedCmdLine.c_str()), 
        NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
    {
        // Failed to start.
        throw ExecuteError("CommandLine::execute", "Could not create process");
    }
#endif
}
