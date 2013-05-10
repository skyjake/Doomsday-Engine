/** @file scriptedinfo.cpp  Info document tree with script context.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/ScriptedInfo"
#include "de/Script"
#include "de/Process"
#include "de/ArrayValue"
#include "de/RecordValue"

namespace de {

DENG2_PIMPL(ScriptedInfo)
{
    Info info;                     ///< Original full parsed contents.
    QScopedPointer<Script> script; ///< Current script being executed.
    Process process;               ///< Execution context.
    String sourcePath;

    Instance(Public *i) : Base(i)
    {}

    void clear()
    {
        info.clear();
        process.clear();
        script.reset();
    }

    /**
     * Iterates through the parsed Info contents and processes each element.
     * Script blocks are parsed as Doomsday Script and executed in the local
     * process. Key/value elements become variables (and values) in the
     * process's global namespace.
     */
    void processAll()
    {
        processBlock(info.root());

        LOG_DEBUG("Processed contents:\n") << process.globals().asText();
    }

    void processElement(Info::Element const *element)
    {
        if(element->isBlock())
        {
            processBlock(*static_cast<Info::BlockElement const *>(element));
        }
        else if(element->isKey())
        {
            processKey(*static_cast<Info::KeyElement const *>(element));
        }
        else if(element->isList())
        {
            processList(*static_cast<Info::ListElement const *>(element));
        }
    }

    void processBlock(Info::BlockElement const &block)
    {
        if(Info::Element *condition = block.find("condition"))
        {
            // Any block will be ignored if its condition is false.
            QScopedPointer<Value> result(evaluate(condition->values().first()));
            if(result.isNull() || result->isFalse())
            {
                return;
            }
        }

        if(block.blockType() == "script")
        {
            DENG2_ASSERT(block.find("script") != 0);
            DENG2_ASSERT(process.state() == Process::Stopped);

            script.reset(new Script(block.find("script")->values().first()));
            script->setPath(sourcePath); // where the source comes from
            process.run(*script);
            process.execute();
        }
        else
        {
            // Block type placed into a special variable (only with named blocks, though).
            if(!block.name().isEmpty())
            {
                String varName = variableName(block).concatenatePath("__type__", '.');
                process.globals().add(varName) = new TextValue(block.blockType());
            }

            foreach(Info::Element const *sub, block.contentsInOrder())
            {
                if(sub->name() == "condition")
                {
                    // Skip special elements.
                    continue;
                }
                if(sub->name() == ":" && !block.name().isEmpty())
                {
                    // Inheritance.
                    String target = sub->values().first();
                    process.globals().add(variableName(block).concatenatePath("__inherit__", '.')) =
                            new TextValue(target);

                    // Copy all present members of the target record.
                    process.globals().subrecord(variableName(block))
                            .copyMembersFrom(process.globals()[target].value<RecordValue>().dereference());
                    continue;
                }
                processElement(sub);
            }
        }
    }

    String variableName(Info::Element const &element)
    {
        String varName = element.name();
        for(Info::BlockElement *b = element.parent(); b != 0; b = b->parent())
        {
            if(!b->name().isEmpty())
            {
                varName = b->name().concatenatePath(varName, '.');
            }
        }
        return varName;
    }

    static bool isEvaluatable(String const &value)
    {
        return value.startsWith('$');
    }

    Value *evaluate(String const &source)
    {
        script.reset(new Script(source));
        process.run(*script);
        process.execute();
        return process.context().evaluator().result().duplicate();
    }

    Value *makeValue(String const &rawValue)
    {
        if(isEvaluatable(rawValue))
        {
            return evaluate(rawValue.substr(1));
        }
        return new TextValue(rawValue);
    }

    void processKey(Info::KeyElement const &key)
    {
        QScopedPointer<Value> v(makeValue(key.value()));
        process.globals().add(variableName(key)) = v.take();
    }

    void processList(Info::ListElement const &list)
    {
        ArrayValue* av = new ArrayValue;
        foreach(QString v, list.values())
        {
            *av << makeValue(v);
        }
        process.globals().addArray(variableName(list), av);
    }
};

ScriptedInfo::ScriptedInfo() : d(new Instance(this))
{}

void ScriptedInfo::clear()
{
    d->clear();
}

void ScriptedInfo::parse(String const &source)
{
    d->clear();
    d->info.parse(source);
    d->processAll();
}

void ScriptedInfo::parse(File const &file)
{
    d->sourcePath = file.path();
    parse(String::fromUtf8(Block(file)));
}

Value *ScriptedInfo::evaluate(String const &source)
{
    return d->evaluate(source);
}

} // namespace de
