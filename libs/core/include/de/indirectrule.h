/** @file indirectrule.h  Indirect rule.
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

#ifndef LIBCORE_INDIRECTRULE_H
#define LIBCORE_INDIRECTRULE_H

#include "de/rule.h"

namespace de {

/**
 * Rule that gets its value indirectly from another rule. The value of an
 * indirect rule cannot be set directly.
 *
 * Indirect rules are useful when others need to depend one a rule that may
 * change dynamically. Anyone relying on the indirect rule will be duly
 * notified of changes in the source of the indirect rule, without having to
 * change anything in the existing rule relationships.
 *
 * @ingroup widgets
 */
class DE_PUBLIC IndirectRule : public Rule
{
public:
    IndirectRule();
    ~IndirectRule() override;

    /**
     * Sets the source rule whose value this indirect rule will reflect.
     *
     * @param rule  Source rule.
     */
    void setSource(const Rule &rule);

    void unsetSource();

    bool        hasSource() const;
    const Rule &source() const;

    void   update() override;
    String description() const override;

private:
    const Rule *_source;
};

} // namespace de

#endif // LIBCORE_INDIRECTRULE_H
