/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/counted.h"

namespace de {

#ifdef DE_DEBUG
std::atomic_int Counted::totalCount{0}; ///< Should return back to zero when program ends.
# ifdef DE_USE_COUNTED_TRACING
static QHash<void *, QByteArray> countedAllocs;
void Counted::printAllocs()
{
    qDebug() << "Counted objects:" << Counted::totalCount;
    for (auto i = countedAllocs.constBegin(); i != countedAllocs.constEnd(); ++i)
    {
        qDebug() << "-=- Object" << i.key() << "\n";
        qDebug() << i.value().constData() << "\n";
    }
}
# endif
#endif

Counted::Counted() : _refCount(1)
{
#ifdef DE_DEBUG
    totalCount++;
# ifdef DE_USE_COUNTED_TRACING
    QString trace;
    DE_BACKTRACE(32, trace);
    countedAllocs[this] = trace.toLatin1();
# endif
#endif
}

Counted::~Counted()
{
#ifdef DE_DEBUG
    totalCount--;
# ifdef DE_USE_COUNTED_TRACING
    countedAllocs.remove(this);
# endif
#endif

    DE_ASSERT(_refCount == 0);
}

void Counted::release() const
{
    //qDebug() << "Counted" << c << typeid(*c).name() << "ref dec'd to" << c->_refCount - 1;

    DE_ASSERT(_refCount > 0);
    if (!--_refCount)
    {
        delete this;
    }
}

void Counted::addRef(dint count) const
{
    DE_ASSERT(_refCount >= 0);
    _refCount += count;
}

} // namespace de
