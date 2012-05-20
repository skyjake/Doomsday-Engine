/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/CommandLine"
#include "de/String"

#include <QFile>
#include <QProcess>
#include <QDebug>

#include <fstream>
#include <sstream>
#include <cctype>

using namespace de;

CommandLine::CommandLine(int argc, char** v)
{
    for(int i = 0; i < argc; ++i)
    {
        if(v[i][0] == '@')
        {
            // This is a response file or something else that requires parsing.
            parse(v[i]);
        }
        else
        {
            _arguments.push_back(v[i]);
            _pointers.push_back(_arguments[i].c_str());
        }
    }

    // The pointers list is kept null terminated.
    _pointers.push_back(0);
}

CommandLine::CommandLine(const CommandLine& other)
    : _arguments(other._arguments)
{
    // Use pointers to the already copied strings.
    DENG2_FOR_EACH(i, _arguments, Arguments::iterator)
    {
        _pointers.push_back(i->c_str());
    }
    _pointers.push_back(0);
}

dint CommandLine::count() const
{
    return _arguments.size();
}

void CommandLine::clear()
{
    _arguments.clear();
    _pointers.clear();
    _pointers.push_back(0);
}

void CommandLine::append(const String& arg)
{
    _arguments.push_back(arg.toStdString());
    _pointers.insert(_pointers.end() - 1, _arguments.rbegin()->c_str());
}

void CommandLine::insert(duint pos, const String& arg)
{
    if(pos > _arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::insert", "Index out of range");
    }
    _arguments.insert(_arguments.begin() + pos, arg.toStdString());
    _pointers.insert(_pointers.begin() + pos, _arguments[pos].c_str());
}

void CommandLine::remove(duint pos)
{
    if(pos >= _arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::remove", "Index out of range");
    }
    _arguments.erase(_arguments.begin() + pos);
    _pointers.erase(_pointers.begin() + pos);
}

dint CommandLine::check(const String& arg, dint numParams) const
{
    // Do a search for arg.
    Arguments::const_iterator i = _arguments.begin();
    for(; i != _arguments.end() && !matches(arg, String::fromStdString(*i)); ++i) {}
    
    if(i == _arguments.end())
    {
        // Not found.
        return 0;
    }

    // It was found, check for the number of non-option parameters.
    Arguments::const_iterator k = i;
    while(numParams-- > 0)
    {
        if(++k == _arguments.end() || isOption(String::fromStdString(*k)))
        {
            // Ran out of arguments, or encountered an option.
            return 0;
        }
    }
    
    return i - _arguments.begin();
}

bool CommandLine::getParameter(const String& arg, String& param) const
{
    dint pos = check(arg, 1);
    if(pos > 0)
    {
        param = at(pos + 1);
        return true;
    }
    return false;
}

dint CommandLine::has(const String& arg) const
{
    dint howMany = 0;
    
    DENG2_FOR_EACH(i, _arguments, Arguments::const_iterator)
    {
        if(matches(arg, String::fromStdString(*i)))
        {
            howMany++;
        }
    }
    return howMany;
}

bool CommandLine::isOption(duint pos) const
{
    if(pos >= _arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::isOption", "Index out of range");
    }
    DENG2_ASSERT(!_arguments[pos].empty());
    return isOption(String::fromStdString(_arguments[pos]));
}

bool CommandLine::isOption(const String& arg)
{
    return !(arg.empty() || arg[0] != '-');
}

const String CommandLine::at(duint pos) const
{
    return String::fromStdString(_arguments.at(pos));
}

const char* const* CommandLine::argv() const
{
    DENG2_ASSERT(*_pointers.rbegin() == 0);
    return &_pointers[0];
}

void CommandLine::parse(const String& cmdLine)
{
    String::const_iterator i = cmdLine.begin();

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

        String word;

        while(i != cmdLine.end() && (quote || !(*i).isSpace()))
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
            QFile response(word);
            if(response.open(QFile::ReadOnly | QFile::Text))
            {
                parse(QString::fromUtf8(response.readAll().constData()));
            }
        }
        else if(word == "--") // End of arguments.
        {
            isDone = true;
        }
        else if(!word.empty()) // Make sure there *is* a word.
        {
            _arguments.push_back(word.toStdString());
            _pointers.push_back(_arguments.rbegin()->c_str());
        }
    }
}

void CommandLine::alias(const String& full, const String& alias)
{
    _aliases[full.toStdString()].push_back(alias.toStdString());
}

bool CommandLine::matches(const String& full, const String& fullOrAlias) const
{
    if(!full.compareWithoutCase(fullOrAlias))
    {
        // They are, in fact, the same.
        return true;
    }
    
    Aliases::const_iterator found = _aliases.find(full.toStdString());
    if(found != _aliases.end())
    {
        DENG2_FOR_EACH(i, found->second, Arguments::const_iterator)
        {
            String s = String::fromStdString(*i);
            if(!s.compareWithoutCase(fullOrAlias))
            {
                // Found it among the aliases.
                return true;
            }
        }
    }    
    return false;
}

void CommandLine::execute(char** /*envs*/) const
{
    qDebug("CommandLine: should call QProcess to execute!\n");

    /*
#ifdef Q_OS_UNIX
    // Fork and execute new file.
    pid_t result = fork();
    if(!result)
    {
        // Here we go in the child process.
        printf("Child loads %s...\n", _pointers[0]);
        execve(_pointers[0], const_cast<char* const*>(argv()), const_cast<char* const*>(envs));
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
    String quotedCmdLine;
    FOR_EACH(i, _arguments, Arguments::const_iterator)
    {
        quotedCmdLine += "\"" + *i + "\" ";
    }

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    ZeroMemory(&processInfo, sizeof(processInfo));

    if(!CreateProcess(_pointers[0], const_cast<char*>(quotedCmdLine.c_str()), 
        NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
    {
        /// @throw ExecuteError The system call to start a new process failed.
        throw ExecuteError("CommandLine::execute", "Could not create process");
    }
#endif
    */
}
