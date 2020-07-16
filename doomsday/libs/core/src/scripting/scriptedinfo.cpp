/** @file scriptedinfo.cpp  Info document tree with script context.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/scriptedinfo.h"
#include "de/app.h"
#include "de/arrayvalue.h"
#include "de/folder.h"
#include "de/logbuffer.h"
#include "de/numbervalue.h"
#include "de/scripting/process.h"
#include "de/recordvalue.h"
#include "de/scripting/script.h"
#include "de/textvalue.h"

#include <algorithm>

namespace de {

const char *ScriptedInfo::SCRIPT         = "script";
const char *ScriptedInfo::BLOCK_GROUP    = "group";
const char *ScriptedInfo::VAR_SOURCE     = "__source__";
const char *ScriptedInfo::VAR_BLOCK_TYPE = "__type__";
const char *ScriptedInfo::VAR_INHERITED_SOURCES = "__inheritedSources__"; // array

static const char *BLOCK_NAMESPACE = "namespace";
static const char *BLOCK_SCRIPT    = ScriptedInfo::SCRIPT;
static const char *KEY_SCRIPT      = ScriptedInfo::SCRIPT;
static const char *KEY_INHERITS    = "inherits";
static const char *KEY_CONDITION   = "condition";
static const char *VAR_SCRIPT      = "__script%02d__";

DE_PIMPL(ScriptedInfo)
{
    typedef Info::Element::Value InfoValue;

    Info info;                     ///< Original full parsed contents.
    std::unique_ptr<Script> script; ///< Current script being executed.
    Process process;               ///< Execution context.
    String currentNamespace;

    Impl(Public *i, Record *globalNamespace)
        : Base(i)
        , process(globalNamespace)
    {
        // No limitation on duplicates for the special block types.
        info.setAllowDuplicateBlocksOfType({BLOCK_GROUP, BLOCK_NAMESPACE});

        // Blocks whose contents are parsed as scripts.
        info.setScriptBlocks({BLOCK_SCRIPT});

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

        LOG_SCR_XVERBOSE("Processed contents:\n", process.globals().asText());
    }

    void processElement(const Info::Element *element)
    {
        if (element->isBlock())
        {
            processBlock(element->as<Info::BlockElement>());
        }
        else if (element->isKey())
        {
            processKey(element->as<Info::KeyElement>());
        }
        else if (element->isList())
        {
            processList(element->as<Info::ListElement>());
        }
    }

    void executeWithContext(const Info::BlockElement *context)
    {
        Record &ns = process.globals();

        // The global "self" variable will point to the block where the script
        // is running (analogous to "self" in class member calling).
        bool needRemoveSelf = false;
        if (context)
        {
            String varName = variableName(*context);
            if (!varName.isEmpty())
            {
                if (!ns.has(varName))
                {
                    // If it doesn't exist yet, make sure it does.
                    ns.addSubrecord(varName);
                }
                ns.add("self") = new RecordValue(ns.subrecord(varName));
                needRemoveSelf = true;
            }
        }

        // Execute the current script.
        process.execute();

        if (needRemoveSelf)
        {
            delete &ns["self"];
        }
    }

    void inherit(const Info::BlockElement &block, const InfoValue &target)
    {
        if (block.name().isEmpty())
        {
            // Nameless blocks cannot be inherited into.
            return;
        }

        String varName = variableName(block);
        if (!varName.isEmpty())
        {
            Record &ns = process.globals();
            // Try a case-sensitive match in global namespace.
            String targetName = checkNamespaceForVariable(target);
            if (!ns.has(targetName))
            {
                // Assume it's an identifier rather than a regular variable.
                targetName = checkNamespaceForVariable(target.text.lower());
            }
            if (!ns.has(targetName))
            {
                // Try a regular variable within the same block.
                targetName = variableName(block.parent()? *block.parent() : block)
                                    .concatenateMember(target);
            }

            ns.add(varName.concatenateMember("__inherit__")) =
                    new TextValue(targetName);

            LOGDEV_SCR_XVERBOSE_DEBUGONLY("setting __inherit__ of %s %s (%p) to %s",
                                         block.blockType() << varName << &block << targetName);

            DE_ASSERT(!varName.isEmpty());
            DE_ASSERT(!targetName.isEmpty());

            // Copy all present members of the target record.
            const Record &src = ns[targetName].value<RecordValue>().dereference();
            Record &dest = ns.subrecord(varName);
            dest.copyMembersFrom(src, Record::IgnoreDoubleUnderscoreMembers);

            // Append the inherited source location.
            if (src.hasMember(VAR_SOURCE))
            {
                if (!dest.hasMember(VAR_INHERITED_SOURCES))
                {
                    dest.addArray(VAR_INHERITED_SOURCES);
                }
                dest[VAR_INHERITED_SOURCES].value<ArrayValue>()
                        .add(new TextValue(ScriptedInfo::sourcePathAndLine(src).first));
            }
        }
    }

    void inheritFromAncestors(const Info::BlockElement &block, const Info::BlockElement *from)
    {
        if (!from) return;

        // The highest ancestor goes first.
        if (from->parent())
        {
            inheritFromAncestors(block, from->parent());
        }

        // This only applies to groups.
        if (from->blockType() == BLOCK_GROUP)
        {
            if (Info::KeyElement *key = from->findAs<Info::KeyElement>(KEY_INHERITS))
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
    bool isUnqualifiedScriptBlock(const Info::BlockElement &block)
    {
        if (block.blockType() != BLOCK_SCRIPT) return false;
        for (const auto *child : block.contentsInOrder())
        {
            if (!child->isKey()) return false;
            const Info::KeyElement &key = child->as<Info::KeyElement>();
            if (key.name() != KEY_SCRIPT && key.name() != KEY_CONDITION)
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
    static String chooseScriptName(const Record &where)
    {
        int counter = 0;
        for (;;)
        {
            String name = Stringf(VAR_SCRIPT, counter);
            if (!where.has(name)) return name;
            counter++;
        }
    }

    void processBlock(const Info::BlockElement &block)
    {
        Record &ns = process.globals();

        if (Info::Element *condition = block.find(KEY_CONDITION))
        {
            // Any block will be ignored if its condition is false.
            std::unique_ptr<Value> result(evaluate(condition->values().first(), nullptr));
            if (!result || result->isFalse())
            {
                return;
            }
        }

        // Inherit from all nameless parent blocks.
        inheritFromAncestors(block, block.parent());

        // Direct inheritance.
        if (Info::KeyElement *key = block.findAs<Info::KeyElement>(KEY_INHERITS))
        {
            // Check for special attributes.
            if (key->flags().testFlag(Info::KeyElement::Attribute))
            {
                // Inherit contents of an existing Record.
                inherit(block, key->value());
            }
        }

        const bool isScriptBlock = (block.blockType() == BLOCK_SCRIPT);

        // Script blocks are executed now. This includes only the unqualified script
        // blocks that have only a single "script" key in them.
        if (isUnqualifiedScriptBlock(block))
        {
            DE_ASSERT(process.state() == Process::Stopped);

            script.reset(new Script(block.keyValue(KEY_SCRIPT)));
            script->setPath(info.sourcePath()); // where the source comes from
            process.run(*script);
            executeWithContext(block.parent());
        }
        else
        {
            String oldNamespace = currentNamespace;

            // Namespace blocks alter how variables get placed/looked up in the Record.
            if (block.blockType() == BLOCK_NAMESPACE)
            {
                if (!block.name().isEmpty())
                {
                    currentNamespace = currentNamespace.concatenateMember(block.name());
                }
                else
                {
                    // Reset to the global namespace.
                    currentNamespace = "";
                }
                LOG_SCR_XVERBOSE("%s: Namespace set to '%s'",
                                 block.sourceLocation() << currentNamespace);
            }
            else if (!block.name().isEmpty() || isScriptBlock)
            {
                String varName;

                // Determine the full variable name of the record of this block.
                if (isScriptBlock)
                {
                    // Qualified scripts get automatically generated names.
                    const String parentVarName = variableName(*block.parent());
                    varName = parentVarName.concatenateMember(chooseScriptName(ns[parentVarName]));
                }
                else
                {
                    // Use the parent block names to form the variable name.
                    varName = variableName(block);
                }

                // Create the block record if it doesn't exist.
                if (!ns.has(varName))
                {
                    ns.addSubrecord(varName);
                }
                Record &blockRecord = ns[varName];

                // Block type placed into a special variable.
                blockRecord.addText(VAR_BLOCK_TYPE, block.blockType());

                // Also store source location in a special variable.
                blockRecord.addNumber(VAR_SOURCE, block.sourceLineId())
                        .value<NumberValue>().setSemanticHints(NumberValue::Hex);

                if (!isScriptBlock)
                {
                    DE_NOTIFY_PUBLIC(NamedBlock, i)
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
                    for (const auto &i : block.contents())
                    {
                        auto *elem = i.second;
                        if (elem->isKey())
                        {
                            const auto &key = elem->as<Info::KeyElement>();
                            if (key.name() != KEY_CONDITION)
                            {
                                blockRecord.addText(key.name(), key.value());
                            }
                        }
                    }
                }
            }

            // Continue processing elements contained in the block (unless this is
            // script block).
            if (!isScriptBlock)
            {
                for (const auto *sub : block.contentsInOrder())
                {
                    // Handle special elements.
                    if (sub->name() == KEY_CONDITION || sub->name() == KEY_INHERITS)
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
    String variableName(const Info::Element &element)
    {
        String varName = element.name();
        for (Info::BlockElement *b = element.parent(); b != nullptr; b = b->parent())
        {
            if (b->blockType() == BLOCK_NAMESPACE) continue;

            if (!b->name().isEmpty())
            {
                if (varName.isEmpty())
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
        if (varName.isEmpty()) return "";

        if (!currentNamespace.isEmpty())
        {
            // First check if this exists in the current namespace.
            String nsVarName = currentNamespace.concatenateMember(varName);
            if (process.globals().has(nsVarName))
            {
                return nsVarName;
            }
        }

        // If it exists as-is, we'll take it.
        if (process.globals().has(varName))
        {
            return varName;
        }

        // We'll assume it will get created.
        if (!currentNamespace.isEmpty())
        {
            // If namespace defined, create the new variable in it.
            return currentNamespace.concatenateMember(varName);
        }
        return varName;
    }

    Value *evaluate(const String &source, const Info::BlockElement *context)
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
    Value *makeValue(const InfoValue &rawValue, const Info::BlockElement *context)
    {
        if (rawValue.flags.testFlag(InfoValue::Script))
        {
            return evaluate(rawValue.text, context);
        }
        return new TextValue(rawValue.text);
    }

    void processKey(const Info::KeyElement &key)
    {
        std::unique_ptr<Value> v(makeValue(key.value(), key.parent()));
        process.globals().add(variableName(key)) = v.release();
    }

    void processList(const Info::ListElement &list)
    {
        ArrayValue* av = new ArrayValue;
        for (const InfoValue &v : list.values())
        {
            *av << makeValue(v, list.parent());
        }
        process.globals().addArray(variableName(list), av);
    }

    static void findBlocks(const String &blockType, Paths &paths, const Record &rec, const String& prefix = {})
    {
        if (rec.hasMember(VAR_BLOCK_TYPE) &&
           !rec[VAR_BLOCK_TYPE].value().asText().compareWithoutCase(blockType))
        {
            // Block type matches.
            paths.insert(prefix);
        }
        for (const auto &i : rec.subrecords())
        {
            findBlocks(blockType, paths, *i.second, prefix.concatenateMember(i.first));
        }
    }

    DE_PIMPL_AUDIENCE(NamedBlock)
};

DE_AUDIENCE_METHOD(ScriptedInfo, NamedBlock)

ScriptedInfo::ScriptedInfo(Record *globalNamespace)
    : d(new Impl(this, globalNamespace))
{}

void ScriptedInfo::clear()
{
    d->clear();
}

void ScriptedInfo::parse(const String &source)
{
    d->clear();
    d->info.parse(source);
    d->processAll();
}

void ScriptedInfo::parse(const File &file)
{
    d->clear();
    d->info.parse(file);
    d->processAll();
}

Value *ScriptedInfo::evaluate(const String &source)
{
    return d->evaluate(source, nullptr);
}

Record &ScriptedInfo::objectNamespace()
{
    return d->process.globals();
}

const Record &ScriptedInfo::objectNamespace() const
{
    return d->process.globals();
}

ScriptedInfo::Paths ScriptedInfo::allBlocksOfType(const String &blockType) const
{
    return allBlocksOfType(blockType, d->process.globals());
}

String ScriptedInfo::absolutePathInContext(const Record &context, const String &relativePath) // static
{
    if (context.has(VAR_SOURCE))
    {
        const auto sourceLocation = Info::sourceLineTable().sourcePathAndLineNumber(
                    context.getui(VAR_SOURCE));

        String absPath = sourceLocation.first.fileNamePath() / relativePath;
        if (!App::rootFolder().has(absPath))
        {
            // As a fallback, look for possible inherited locations.
            if (context.has(VAR_INHERITED_SOURCES))
            {
                // Look in reverse so the latest inherited locations are checked first.
                const auto &elems = context.geta(VAR_INHERITED_SOURCES);
                for (long i = elems.size() - 1; i >= 0; --i)
                {
                    String inheritedPath = elems.at(i).asText().fileNamePath() / relativePath;
                    if (App::rootFolder().has(inheritedPath))
                    {
                        return inheritedPath;
                    }
                }
            }
        }
        return absPath;
    }

    // The relation is unknown.
    return relativePath;
}

bool ScriptedInfo::isTrue(const Value &value) // static
{
    if (const TextValue *textValue = maybeAs<TextValue>(value))
    {
        // Text values are interpreted a bit more loosely.
        const String value = textValue->asText();
        if (!value.compareWithoutCase("true") ||
            !value.compareWithoutCase("yes") ||
            !value.compareWithoutCase("on"))
        {
            return true;
        }
        return false;
    }
    return value.isTrue();
}

bool ScriptedInfo::isTrue(const RecordAccessor &rec, const String &name, bool defaultValue)
{
    if (rec.has(name))
    {
        return isTrue(rec.get(name));
    }
    return defaultValue;
}

String ScriptedInfo::blockType(const Record &block)
{
    return block.gets(VAR_BLOCK_TYPE, BLOCK_GROUP).lower();
}

bool ScriptedInfo::isFalse(const RecordAccessor &rec, const String &name, bool defaultValue)
{
    if (rec.has(name))
    {
        return isFalse(rec.get(name));
    }
    return defaultValue;
}

bool ScriptedInfo::isFalse(const String &token) // static
{
    // Text values are interpreted a bit more loosely.
    if (!token.compareWithoutCase("false") ||
        !token.compareWithoutCase("no")    ||
        !token.compareWithoutCase("off"))
    {
        return true;
    }
    return false;
}

bool ScriptedInfo::isFalse(const Value &value) // static
{
    if (const TextValue *textValue = maybeAs<TextValue>(value))
    {
        return isFalse(textValue->asText());
    }
    return !value.isTrue();
}

ScriptedInfo::Paths ScriptedInfo::allBlocksOfType(const String &blockType, const Record &root) // static
{
    Paths found;
    Impl::findBlocks(blockType, found, root);
    return found;
}

Record::Subrecords ScriptedInfo::subrecordsOfType(const String &blockType, const Record &record) // static
{
    return record.subrecords([&] (const Record &sub) {
        return sub.gets(VAR_BLOCK_TYPE, "") == blockType;
    });
}

StringList ScriptedInfo::sortRecordsBySource(const Record::Subrecords &subrecs)
{
    StringList keys =
        map<StringList>(subrecs, [](const std::pair<String, Record *> &v) { return v.first; });

    std::sort(keys.begin(), keys.end(), [&subrecs] (const String &a, const String &b) -> bool
    {
        const auto src1 = Info::sourceLineTable().sourcePathAndLineNumber(subrecs[a]->getui(VAR_SOURCE, 0));
        const auto src2 = Info::sourceLineTable().sourcePathAndLineNumber(subrecs[b]->getui(VAR_SOURCE, 0));
        if (!String(src1.first).compareWithoutCase(src2.first))
        {
            // Path is the same, compare line numbers.
            return src1.second < src2.second;
        }
        // Just compare paths.
        return String(src1.first).compareWithoutCase(src2.first) < 0;
    });

    return keys;
}

String ScriptedInfo::sourceLocation(const RecordAccessor &record)
{
    return Info::sourceLocation(record.getui(VAR_SOURCE, 0));
}

SourceLineTable::PathAndLine ScriptedInfo::sourcePathAndLine(const RecordAccessor &record)
{
    return Info::sourceLineTable().sourcePathAndLineNumber(record.getui(VAR_SOURCE, 0));
}

} // namespace de
