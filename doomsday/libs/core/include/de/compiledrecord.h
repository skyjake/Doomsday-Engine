/** @file compiledrecord.h  Record that has been compiled to a native struct.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_COMPILEDRECORD_H
#define LIBCORE_COMPILEDRECORD_H

#include "de/record.h"

namespace de {

/**
 * Specialized Record that can be compiled into an efficient native struct.
 *
 * Using this is preferable to regular Record if the data is being accessed frequently
 * but modified seldom. This is typically the case when Records are being created
 * by native code for specific purposes (as opposed to Records created within scripts).
 */
class DE_PUBLIC CompiledRecord : public Record
{
public:
    virtual ~CompiledRecord();

    inline bool isCompiled() const { return _compiled; }
    virtual void compile() const = 0;

protected:
    void setCompiled(bool compiled) const { _compiled = compiled; }

private:
    mutable bool _compiled = false;
};

template <typename NativeStruct>
class CompiledRecordT : public CompiledRecord
{
public:
    void compile() const override
    {
        _compiled = NativeStruct(*this);
        setCompiled(true);
    }

    void resetCompiled()
    {
        _compiled = NativeStruct();
        setCompiled(false);
    }

    inline const NativeStruct &compiled() const
    {
        if (!isCompiled()) compile();
        return _compiled;
    }

protected:
    mutable NativeStruct _compiled;
};

} // namespace de

#endif // LIBCORE_COMPILEDRECORD_H
