/** @file scriptedinfo.h  Info document tree with script context.
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

#ifndef LIBCORE_SCRIPTEDINFO_H
#define LIBCORE_SCRIPTEDINFO_H

#include "de/info.h"
#include "de/file.h"
#include "de/scripting/iobject.h"
#include "de/set.h"

namespace de {

/**
 * Info document tree with a script context and built-in support for handling
 * expressions and embedded scripts. @ingroup script
 *
 * Analogous to an XML document with embedded JavaScript: Info acts as the
 * generic, declarative, structured document and Doomsday Script is the
 * procedural programming language.
 *
 * An instance of ScriptedInfo contains an Info document. It has its own
 * private script execution context, in which expressions can be evaluated and
 * scripts run. After a ScriptedInfo has been parsed, all the embedded scripts
 * are run and the Info elements become variables and values in the local
 * namespace (ScriptedInfo::names()).
 *
 * @par Special elements
 *
 * Each block of a ScriptedInfo document has a couple of special elements
 * that alter how the block is processed:
 *
 * - The "condition" element that may be present in any block determines
 *   whether the block is processed or skipped. The value of the "condition"
 *   element is evaluated as a script, and if it evaluates to False, the
 *   entire block is ignored.
 *
 * - The contents of any previously processed block (or any record available in
 *   the namespace) can be copied with the special inheritance element
 *   (named "inherits"):
 * <pre>type1 firstblock { key = value }
 * type2 exampleblock inherits firstblock {}</pre>
 *
 *   Here @c firstblock would be treated as a variable name in the document's
 *   local namespace, referring to the block above, which has already been
 *   added to the local namespace (elements are processed sequentially from
 *   beginning to end). The resulting Record is:
 * <pre>exampleblock. __inherit__: firstblock
 *                  __type__: type2
 *                       key: value
 *   firstblock. __type__: type1
 *                    key: value</pre>
 *
 * @par Grouping
 *
 * The block type "group" is reserved for generic grouping of contained
 * elements. The normal usage of groups is analogous to structs in C.
 *
 * If the group is named, it will contribute its name to the path
 * of the produced variable (same as with any named block):
 * <pre>group test {
 *     type1 block { key = value }
 * }</pre>
 *
 * In this example, the variable representing @c key would be
 * <tt>test.block.key</tt> in the ScriptedInfo instance's namespace.
 *
 * @par Namespaces
 *
 * The block type "namespace" is reserved for specifying a namespace prefix
 * that determines where variables are created and looked up when processing an
 * Info document. The namespace prefix can be any variable path (e.g.,
 * <tt>test.block</tt>).
 *
 * Even though the current namespace has precedence when looking up existing
 * variables (say, for inheriting members from another record), if an
 * identifier does not exist in the current namespace but is present in the
 * global namespace, the global namespace gets still used.
 * <pre>namespace ns {
 *     type1 block { key = value }
 *     type2 another inherits block {}
 * }</pre>
 *
 * In this example, the produced records are <tt>ns.block</tt> and
 * <tt>ns.another</tt> that inherits <tt>ns.block</tt>. Compare to the case
 * when not using a namespace:
 * <pre>type1 ns.block { key = value }
 * type2 ns.another inherits ns.block {}
 * </pre>
 * Or when using a group:
 * <pre>group ns {
 *     type1 block { key = value }
 *     type2 another inherits ns.block {}
 * }</pre>
 *
 * @par Group inheritance
 *
 * When the "inherits" element is used in a group, it will affect all the
 * blocks in the group instead of inheriting anything into the group itself.
 * <pre>thing A { key = value }
 * group {
 *     inherits = A
 *     thing B {}
 *     thing C {}
 * }</pre>
 * Here B and C would both inherit from A.
 */
class DE_PUBLIC ScriptedInfo : public IObject
{
public:
    typedef Set<String> Paths;

    DE_AUDIENCE(NamedBlock, void parsedNamedBlock(const String &name, Record &block))

public:
    /**
     * Creates a new ScriptedInfo parser.
     *
     * @param globalNamespace  Optionally an existing namespace where the parsed
     *                         content will be stored in. If not provided, ScriptedInfo
     *                         will create a new one (returned by names()).
     */
    ScriptedInfo(Record *globalNamespace = 0);

    void clear();

    void parse(const String &source);

    void parse(const File &file);

    /**
     * Evaluates one or more statements and returns the result. All processing
     * is done in the document's local script context. Changes to the context's
     * global namespace are allowed and will remain in effect after the
     * evaluation finishes.
     *
     * @param source  Statements or expression to evaluate.
     *
     * @return  Result value. Caller gets ownership.
     */
    Value *evaluate(const String &source);

    /**
     * Finds all the blocks of a given type in the processed namespace.
     * The block type has been stored as a member called __type__ in each
     * subrecord that was produced from an Info block.
     *
     * @param blockType  Type of Info block to locate.
     *
     * @return Set of paths to all the located records of the correct type.
     * The records can be accessed via objectNamespace().
     */
    Paths allBlocksOfType(const String &blockType) const;

    // Implements IObject.
    Record &objectNamespace();
    const Record &objectNamespace() const;

public:
    /**
     * Checks if the context has a "__source__", and resolves @a relativePath in
     * relation to it.
     *
     * @param context       Namespace.
     * @param relativePath  Relative path.
     *
     * @return Absolute path resolved using the context.
     */
    static String absolutePathInContext(const Record &context, const String &relativePath);

    /**
     * Determines if a value should be considered False. Use this when
     * interpreting contents of Info documents where boolean values are
     * expected.
     *
     * This is different from <pre>!de::Value::isTrue()</pre> in that it allows
     * for more relaxed interpretation of the value.
     *
     * @param value  Value to check.
     *
     * @return @c true, if the value should be considered False. Otherwise, @c false.
     */
    static bool isFalse(const Value &value);

    static bool isFalse(const RecordAccessor &rec, const String &name,
                        bool defaultValue = true /* assume false if missing */);

    static bool isFalse(const String &token);

    static bool isTrue(const Value &value);

    static bool isTrue(const RecordAccessor &rec, const String &name,
                       bool defaultValue = false /* assume false if missing */);

    static String blockType(const Record &block);

    static Paths allBlocksOfType(const String &blockType, const Record &root);

    /**
     * Finds all the subrecords with a given __type__. Only the subrecords
     * immediately under @a record are checked.
     *
     * @param blockType  Type of Info block to locate.
     * @param record     Namespace to look in.
     *
     * @return Subrecords whose __type__ matches @a blockType.
     */
    static Record::Subrecords subrecordsOfType(const String &blockType, const Record &record);

    /**
     * Gives a set of subrecords, sorts them by source path and line number
     * (ascending).
     *
     * @param subrecs  Subrecords to sort.
     *
     * @return  Names of the subrecords in the sorted order.
     */
    static StringList sortRecordsBySource(const Record::Subrecords &subrecs);

    static String sourceLocation(const RecordAccessor &record);

    static SourceLineTable::PathAndLine sourcePathAndLine(const RecordAccessor &record);

public:
    static const char *SCRIPT;
    static const char *BLOCK_GROUP;

    /// Name of a special variable where the source location of a record is stored.
    static const char *VAR_SOURCE;

    /// Name of a special variable where the block type is stored.
    static const char *VAR_BLOCK_TYPE;

    static const char *VAR_INHERITED_SOURCES;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_SCRIPTEDINFO_H
