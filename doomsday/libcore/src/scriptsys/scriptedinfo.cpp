/** @file scriptedinfo.cpp  Info document tree with script context.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ScriptedInfo"
#include "de/Script"
#include "de/Process"
#include "de/ArrayValue"
#include "de/RecordValue"
#include "de/App"

namespace de {

static String const BLOCK_GROUP     = "group";
static String const BLOCK_NAMESPACE = "namespace";
static String const KEY_INHERIT     = "inherits";
static String const VAR_BLOCK_TYPE  = "__type__";
static String const VAR_SOURCE      = "__source__";

DENG2_PIMPL(ScriptedInfo)
{
    typedef Info::Element::Value InfoValue;

    Info info;                     ///< Original full parsed contents.
    QScopedPointer<Script> script; ///< Current script being executed.
    Process process;               ///< Execution context.
    String currentNamespace;

    Instance(Public *i, Record *globalNamespace)
        : Base(i)
        , process(globalNamespace)
    {
        // No limitation on duplicates for the special block types.
        info.setAllowDuplicateBlocksOfType(
                    QStringList() << BLOCK_GROUP << BLOCK_NAMESPACE);
    }

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

        LOG_SCR_XVERBOSE("Processed contents:\n") << process.globals().asText();
    }

    void processElement(Info::Element const *element)
    {
        if(element->isBlock())
        {
            processBlock(element->as<Info::BlockElement>());
        }
        else if(element->isKey())
        {
            processKey(element->as<Info::KeyElement>());
        }
        else if(element->isList())
        {
            processList(element->as<Info::ListElement>());
        }
    }

    void executeWithContext(Info::BlockElement const *context)
    {
        Record &ns = process.globals();

        // The global "self" variable will point to the block where the script
        // is running (analogous to "self" in class member calling).
        bool needRemoveSelf = false;
        if(context)
        {
            String varName = variableName(*context);
            if(!varName.isEmpty())
            {
                if(!ns.has(varName))
                {
                    // If it doesn't exist yet, make sure it does.
                    ns.addRecord(varName);
                }
                ns.add("self") = new RecordValue(ns.subrecord(varName));
                needRemoveSelf = true;
            }
        }

        // Execute the current script.
        process.execute();

        if(needRemoveSelf)
        {
            delete &ns["self"];
        }
    }

    void inherit(Info::BlockElement const &block, InfoValue const &target)
    {
        if(block.name().isEmpty())
        {
            // Nameless blocks cannot be inherited into.
            return;
        }

        String varName = variableName(block);
        if(!varName.isEmpty())
        {
            Record &ns = process.globals();            
            String targetName = checkNamespaceForVariable(target);
            if(!ns.has(targetName))
            {
                // Assume it's an identifier rather than a regular variable.
                targetName = checkNamespaceForVariable(target.text.toLower());
            }

            ns.add(varName.concatenateMember("__inherit__")) =
                    new TextValue(targetName);

            LOGDEV_SCR_XVERBOSE_DEBUGONLY("setting __inherit__ of %s %s (%p) to %s",
                                         block.blockType() << varName << &block << targetName);

            DENG2_ASSERT(!varName.isEmpty());
            DENG2_ASSERT(!targetName.isEmpty());

            // Copy all present members of the target record.
            ns.subrecord(varName)
                    .copyMembersFrom(ns[targetName].value<RecordValue>().dereference(),
                                     Record::IgnoreDoubleUnderscoreMembers);
        }
    }

    void inheritFromAncestors(Info::BlockElement const &block, Info::BlockElement const *from)
    {
        if(!from) return;

        // The highest ancestor goes first.
        if(from->parent())
        {
            inheritFromAncestors(block, from->parent());
        }

        // This only applies to groups.
        if(from->blockType() == BLOCK_GROUP)
        {
            if(Info::KeyElement *key = from->findAs<Info::KeyElement>(KEY_INHERIT))
            {
                inherit(block, key->value());
            }
        }
    }

    void processBlock(Info::BlockElement const &block)
    {
        Record &ns = process.globals();

        if(Info::Element *condition = block.find("condition"))
        {
            // Any block will be ignored if its condition is false.
            QScopedPointer<Value> result(evaluate(condition->values().first(), 0));
            if(result.isNull() || result->isFalse())
            {
                return;
            }
        }

        // Inherit from all nameless parent blocks.
        inheritFromAncestors(block, block.parent());

        // Direct inheritance.
        if(Info::KeyElement *key = block.findAs<Info::KeyElement>(KEY_INHERIT))
        {
            // Check for special attributes.
            if(key->flags().testFlag(Info::KeyElement::Attribute))
            {
                // Inherit contents of an existing Record.
                inherit(block, key->value());
            }
        }

        // Script blocks are executed now.
        if(block.blockType() == "script")
        {
            DENG2_ASSERT(block.find("script") != 0);
            DENG2_ASSERT(process.state() == Process::Stopped);

            script.reset(new Script(block.find("script")->values().first()));
            script->setPath(info.sourcePath()); // where the source comes from
            process.run(*script);
            executeWithContext(block.parent());
        }
        else
        {
            String oldNamespace = currentNamespace;

            // Namespace blocks alter how variables get placed/looked up in the Record.
            if(block.blockType() == BLOCK_NAMESPACE)
            {
                if(!block.name().isEmpty())
                {
                    currentNamespace = currentNamespace.concatenateMember(block.name());
                }
                else
                {
                    // Reset to the global namespace.
                    currentNamespace = "";
                }
                LOG_SCR_XVERBOSE("Namespace set to '%s' on line %i") << currentNamespace << block.lineNumber();
            }
            else if(!block.name().isEmpty())
            {
                // Block type placed into a special variable (only with named blocks, though).
                ns.add(variableName(block).concatenateMember(VAR_BLOCK_TYPE)) =
                        new TextValue(block.blockType());

                // Also store source location in a special variable.
                ns.add(variableName(block).concatenateMember(VAR_SOURCE)) =
                        new TextValue(block.sourceLocation());
            }

            foreach(Info::Element const *sub, block.contentsInOrder())
            {
                // Handle special elements.
                if(sub->name() == "condition" || sub->name() == "inherits")
                {
                    // Already handled.
                    continue;
                }
                processElement(sub);
            }

            // Continue with the old namespace after the block.
            currentNamespace = oldNamespace;
        }
    }

    /**
     * Determines the name of the variable representing an element. All named
     * containing parent blocks contribute to the variable name.
     *
     * @param element  Element whose variable name to determine.
     *
     * @return Variable name in the form <tt>ancestor.parent.elementname</tt>.
     */
    String variableName(Info::Element const &element)
    {
        String varName = element.name();
        for(Info::BlockElement *b = element.parent(); b != 0; b = b->parent())
        {
            if(b->blockType() == BLOCK_NAMESPACE) continue;

            if(!b->name().isEmpty())
            {
                if(varName.isEmpty())
                    varName = b->name();
                else
                    varName = b->name().concatenateMember(varName);
            }
        }        
        return checkNamespaceForVariable(varName);
    }

    /**
     * Look up a variable name taking into consideration the current namespace.
     * Unlike simple grouping, namespaces also allow accessing the global
     * namespace if nothing shadows the identifier in the namespace. However,
     * all new variables get created using the current namespace.
     *
     * @param varName  Provided variable name without a namespace.
     *
     * @return Variable to use after namespace has been applied.
     */
    String checkNamespaceForVariable(String varName)
    {
        if(varName.isEmpty()) return "";

        if(!currentNamespace.isEmpty())
        {
            // First check if this exists in the current namespace.
            String nsVarName = currentNamespace.concatenateMember(varName);
            if(process.globals().has(nsVarName))
            {
                return nsVarName;
            }
        }

        // If it exists as-is, we'll take it.
        if(process.globals().has(varName))
        {
            return varName;
        }

        // We'll assume it will get created.
        if(!currentNamespace.isEmpty())
        {
            // If namespace defined, create the new variable in it.
            return currentNamespace.concatenateMember(varName);
        }
        return varName;
    }

    Value *evaluate(String const &source, Info::BlockElement const *context)
    {
        script.reset(new Script(source));
        script->setPath(info.sourcePath()); // where the source comes from
        process.run(*script);
        executeWithContext(context);
        return process.context().evaluator().result().duplicate();
    }

    /**
     * Constructs a Value from the value of an element. If the element value has been
     * marked with the semantic hint for scripting, it will be evaluated as a script. A
     * global "self" variable will be pointed to the Record representing the @a context
     * block.
     *
     * @param rawValue  Value of an element.
     * @param context   Containing block element.
     *
     * @return Value instance. Caller gets ownership.
     */
    Value *makeValue(InfoValue const &rawValue, Info::BlockElement const *context)
    {
        if(rawValue.flags.testFlag(InfoValue::Script))
        {
            return evaluate(rawValue.text, context);
        }
        return new TextValue(rawValue.text);
    }

    void processKey(Info::KeyElement const &key)
    {
        QScopedPointer<Value> v(makeValue(key.value(), key.parent()));
        process.globals().add(variableName(key)) = v.take();
    }

    void processList(Info::ListElement const &list)
    {
        ArrayValue* av = new ArrayValue;
        foreach(InfoValue const &v, list.values())
        {
            *av << makeValue(v, list.parent());
        }
        process.globals().addArray(variableName(list), av);
    }

    static void findBlocks(String const &blockType, Paths &paths, Record const &rec, String prefix = "")
    {
        if(rec.hasMember(VAR_BLOCK_TYPE) &&
           !rec[VAR_BLOCK_TYPE].value().asText().compareWithoutCase(blockType))
        {
            // Block type matches.
            paths.insert(prefix);
        }

        Record::Subrecords const subs = rec.subrecords();
        DENG2_FOR_EACH_CONST(Record::Subrecords, i, subs)
        {
            findBlocks(blockType, paths, *i.value(), prefix.concatenateMember(i.key()));
        }
    }
};

ScriptedInfo::ScriptedInfo(Record *globalNamespace)
    : d(new Instance(this, globalNamespace))
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
    d->clear();
    d->info.parse(file);
    d->processAll();
}

Value *ScriptedInfo::evaluate(String const &source)
{
    return d->evaluate(source, 0);
}

Record &ScriptedInfo::names()
{
    return d->process.globals();
}

Record const &ScriptedInfo::names() const
{
    return d->process.globals();
}

Variable const &ScriptedInfo::operator [] (String const &name) const
{
    return names()[name];
}

ScriptedInfo::Paths ScriptedInfo::allBlocksOfType(String const &blockType) const
{
    return allBlocksOfType(blockType, d->process.globals());
}

String ScriptedInfo::absolutePathInContext(Record const &context, String const &relativePath) // static
{
    if(context.has("__source__"))
    {
        String src = context["__source__"].value<TextValue>();
        int pos = src.lastIndexOf(':');
        if(pos < 0) return src / relativePath;
        src.truncate(pos);
        return src.fileNamePath() / relativePath;
    }
    return relativePath;
}

ScriptedInfo::Paths ScriptedInfo::allBlocksOfType(String const &blockType, Record const &root) // static
{
    Paths found;
    Instance::findBlocks(blockType, found, root);
    return found;
}

Record::Subrecords ScriptedInfo::subrecordsOfType(String const &blockType, Record const &record) // static
{
    return record.subrecords([&] (Record const &sub) {
        return sub.gets(VAR_BLOCK_TYPE, "") == blockType;
    });
}

} // namespace de
