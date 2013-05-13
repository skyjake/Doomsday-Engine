/** @file scriptedinfo.h  Info document tree with script context.
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

#ifndef LIBDENG2_SCRIPTEDINFO_H
#define LIBDENG2_SCRIPTEDINFO_H

#include "../Info"
#include "../File"

#include <QSet>

namespace de {

/**
 * Info document tree with a script context and built-in support for handling
 * expressions and embedded scripts.
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
class DENG2_PUBLIC ScriptedInfo
{
public:
    typedef QSet<String> Paths;

public:
    ScriptedInfo();

    void clear();

    void parse(String const &source);

    void parse(File const &file);

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
    Value *evaluate(String const &source);

    Record &names();

    Record const &names() const;

    Variable const &operator [] (String const &path) const;

    /**
     * Finds all the blocks of a given type in the processed namespace.
     * The block type has been stored as a member called __type__ in each
     * subrecord that was produced from an Info block.
     *
     * @param blockType  Type of Info block to locate.
     *
     * @return Set of paths to all the located records of the correct type.
     * The records can be accessed via names().
     */
    Paths allBlocksOfType(String const &blockType) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_SCRIPTEDINFO_H
