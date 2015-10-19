/** @file scriptedinfo.cpp  Info document tree with script context.
 *
 * @authors Copyright © 2013-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <algorithm>

namespace de {

String const ScriptedInfo::SCRIPT         = "script";
String const ScriptedInfo::BLOCK_GROUP    = "group";
String const ScriptedInfo::VAR_SOURCE     = "__source__";
String const ScriptedInfo::VAR_BLOCK_TYPE = "__type__";

static String const BLOCK_NAMESPACE = "namespace";
static String const BLOCK_SCRIPT    = ScriptedInfo::SCRIPT;
static String const KEY_SCRIPT      = ScriptedInfo::SCRIPT;
static String const KEY_INHERITS    = "inherits";
static String const KEY_CONDITION   = "condition";
static String const VAR_SCRIPT      = "__script%1__";

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

        // Blocks whose contents are parsed as scripts.
        info.setScriptBlocks(QStringList() << BLOCK_SCRIPT);

        // Single-token blocks are implicitly treated as "group" blocks.
        info.setImplicitBlockType(BLOCK_GROUP);
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
            // Try a case-sensitive match in global namespace.
            String targetName = checkNamespaceForVariable(target);
            if(!ns.has(targetName))
            {
                // Assume it's an identifier rather than a regular variable.
                targetName = checkNamespaceForVariable(target.text.toLower());
            }
            if(!ns.has(targetName))
            {
                // Try a regular variable within the same block.
                targetName = variableName(block.parent()? *block.parent() : block)
                                    .concatenateMember(target);
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
            if(Info::KeyElement *key = from->findAs<Info::KeyElement>(KEY_INHERITS))
            {
                inherit(block, key->value());
            }
        }
    }

    /**
     * Determines whether a script block is unqualified, meaning it is an anonymous
     * script block that has no other keys than the script itself and possibly a
     * "condition" key.
     *
     * @param block  Block.
     *
     * @return @c true, if an unqualified script block (that will be executed immediately
     * during processing of the ScriptedInfo document), or otherwise @c false, in which
     * case the script is assumed to be executed at a later point in time.
     */
    bool isUnqualifiedScriptBlock(Info::BlockElement const &block)
    {
        if(block.blockType() != BLOCK_SCRIPT) return false;
        for(auto const *child : block.contentsInOrder())
        {
            if(!child->isKey()) return false;
            Info::KeyElement const &key = child->as<Info::KeyElement>();
            if(key.name() != KEY_SCRIPT && key.name() != KEY_CONDITION)
            {
                return false;
            }
        }
        return block.contains(KEY_SCRIPT);
    }

    /**
     * Chooses a new automatically generated (anonymous) script block name that is
     * unique within @a where.
     *
     * @param where  Record where the script will be stored.
     *
     * @return Variable name.
     */
    static String chooseScriptName(Record const &where)
    {
        int counter = 0;
        forever
        {
            String name = VAR_SCRIPT.arg(counter, 2 /*width*/, 10 /*base*/, QLatin1Char('0'));
            if(!where.has(name)) return name;
            counter++;
        }
    }

    void processBlock(Info::BlockElement const &block)
    {
        Record &ns = process.globals();

        if(Info::Element *condition = block.find(KEY_CONDITION))
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
        if(Info::KeyElement *key = block.findAs<Info::KeyElement>(KEY_INHERITS))
        {
            // Check for special attributes.
            if(key->flags().testFlag(Info::KeyElement::Attribute))
            {
                // Inherit contents of an existing Record.
                inherit(block, key->value());
            }
        }

        bool const isScriptBlock = (block.blockType() == BLOCK_SCRIPT);

        // Script blocks are executed now. This includes only the unqualified script
        // blocks that have only a single "script" key in them.
        if(isUnqualifiedScriptBlock(block))
        {
            DENG2_ASSERT(process.state() == Process::Stopped);

            script.reset(new Script(block.keyValue(KEY_SCRIPT)));
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
            else if(!block.name().isEmpty() || isScriptBlock)
            {
                String varName;

                // Determine the full variable name of the record of this block.
                if(isScriptBlock)
                {
                    // Qualified scripts get automatically generated names.
                    String const parentVarName = variableName(*block.parent());
                    varName = parentVarName.concatenateMember(chooseScriptName(ns[parentVarName]));
                }
                else
                {
                    // Use the parent block names to form the variable name.
                    varName = variableName(block);
                }

                // Create the block record if it doesn't exist.
                if(!ns.has(varName))
                {
                    ns.addRecord(varName);
                }
                Record &blockRecord = ns[varName];

                // Block type placed into a special variable.
                blockRecord.addText(VAR_BLOCK_TYPE, block.blockType());

                // Also store source location in a special variable.
                blockRecord.addText(VAR_SOURCE, block.sourceLocation());

                if(!isScriptBlock)
                {
                    DENG2_FOR_PUBLIC_AUDIENCE2(NamedBlock, i)
                    {
                        i->parsedNamedBlock(varName, blockRecord);
                    }
                }
                else
                {
                    // Add the extra attributes of script blocks.
                    // These are not processed as regular subelements because the
                    // path of the script record is not directly determined by the parent
                    // blocks (see above; chooseScriptName()).
                    for(auto const *elem : block.contents().values())
                    {
                        if(elem->isKey())
                        {
                            auto const &key = elem->as<Info::KeyElement>();
                            if(key.name() != KEY_CONDITION)
                            {
                                blockRecord.addText(key.name(), key.value());
                            }
                        }
                    }
                }
            }

            // Continue processing elements contained in the block (unless this is
            // script block).
            if(!isScriptBlock)
            {
                foreach(Info::Element const *sub, block.contentsInOrder())
                {
                    // Handle special elements.
                    if(sub->name() == KEY_CONDITION || sub->name() == KEY_INHERITS)
                    {
                        // Already handled.
                        continue;
                    }
                    processElement(sub);
                }
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

    DENG2_PIMPL_AUDIENCE(NamedBlock)
};

DENG2_AUDIENCE_METHOD(ScriptedInfo, NamedBlock)

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
    if(context.has(VAR_SOURCE))
    {
        String src = context[VAR_SOURCE].value<TextValue>();
        // Exclude the possible line number following a colon.
        int pos = src.lastIndexOf(':');
        if(pos < 0) return src / relativePath;
        src.truncate(pos);
        return src.fileNamePath() / relativePath;
    }
    return relativePath;
}

bool ScriptedInfo::isTrue(Value const &value) // static
{
    if(TextValue const *textValue = value.maybeAs<TextValue>())
    {
        // Text values are interpreted a bit more loosely.
        String const value = textValue->asText();
        if(!value.compareWithoutCase("true") ||
           !value.compareWithoutCase("yes") ||
           !value.compareWithoutCase("on"))
        {
            return true;
        }
        return false;
    }
    return value.isTrue();
}

bool ScriptedInfo::isTrue(RecordAccessor const &rec, String const &name, bool defaultValue)
{
    if(rec.has(name))
    {
        return isTrue(rec.get(name));
    }
    return defaultValue;
}

String ScriptedInfo::blockType(Record const &block)
{
    return block.gets(VAR_BLOCK_TYPE, BLOCK_GROUP).toLower();
}

bool ScriptedInfo::isFalse(RecordAccessor const &rec, String const &name, bool defaultValue)
{
    if(rec.has(name))
    {
        return isFalse(rec.get(name));
    }
    return defaultValue;
}

bool ScriptedInfo::isFalse(Value const &value) // static
{
    if(TextValue const *textValue = value.maybeAs<TextValue>())
    {
        // Text values are interpreted a bit more loosely.
        String const value = textValue->asText();
        if(!value.compareWithoutCase("false") ||
           !value.compareWithoutCase("no") ||
           !value.compareWithoutCase("off"))
        {
            return true;
        }
        return false;
    }
    return !value.isTrue();
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

StringList ScriptedInfo::sortRecordsBySource(Record::Subrecords const &subrecs)
{
    StringList keys = subrecs.keys();

    std::sort(keys.begin(), keys.end(),
              [&subrecs] (String const &a, String const &b) -> bool {
        String src1 = subrecs[a]->gets(VAR_SOURCE, ":0");
        String src2 = subrecs[b]->gets(VAR_SOURCE, ":0");
        DENG2_ASSERT(src1.contains(':'));
        DENG2_ASSERT(src2.contains(':'));
        QStringList parts1 = src1.split(':');
        QStringList parts2 = src2.split(':');
        if(!String(parts1.at(0)).compareWithoutCase(parts2.at(0)))
        {
            // Path is the same, compare line numbers.
            return parts1.at(1).toInt() < parts2.at(1).toInt();
        }
        // Just compare paths.
        return String(parts1.at(0)).compareWithoutCase(parts2.at(0)) < 0;
    });

    return keys;
}

} // namespace de
