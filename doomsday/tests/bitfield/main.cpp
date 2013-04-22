/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/BitField>
#include <QTextStream>
#include <QDebug>

using namespace de;

int main(int, char **)
{
    try
    {
        BitField pack;       
        pack.addElement(1, 1);
        pack.set(1, duint(1));
        qDebug() << pack.asText().toLatin1().constData();

        pack.clear();

        pack.addElement(1, 1);
        pack.addElement(2, 1);
        pack.set(2, true);
        qDebug() << pack.asText().toLatin1().constData();
        pack.set(1, true);
        qDebug() << pack.asText().toLatin1().constData();

        pack.addElement(3, 3);
        pack.set(1, false);
        qDebug() << pack.asText().toLatin1().constData();
        pack.set(3, 6u);
        qDebug() << pack.asText().toLatin1().constData();

        pack.addElement(10, 8);
        pack.set(10, 149u);
        qDebug() << pack.asText().toLatin1().constData();

        qDebug() << "Field 1:" << pack[1];
        qDebug() << "Field 2:" << pack[2];
        qDebug() << "Field 3:" << pack[3];
        qDebug() << "Field 10:" << pack[10];

        DENG2_ASSERT(pack[10] == 149);

        BitField pack2 = pack;
        qDebug() << "Copied:" << pack2.asText().toLatin1().constData();

        qDebug() << "Equal:" << (pack2 == pack? "true" : "false");
        qDebug() << "Delta:" << pack.delta(pack2);

        pack2.set(3, 3u);
        qDebug() << "Modified:" << pack2.asText().toLatin1().constData();
        qDebug() << "Equal:" << (pack2 == pack? "true" : "false");
        qDebug() << "Delta:" << pack.delta(pack2);

        pack2.set(3, 6u);
        pack2.set(10, 128u);
        qDebug() << "Modified:" << pack2.asText().toLatin1().constData();
        qDebug() << "Field 10:" << pack2[10];
        qDebug() << "Equal:" << (pack2 == pack? "true" : "false");
        qDebug() << "Delta:" << pack.delta(pack2);

        pack2.set(1, true);
        qDebug() << "Modified:" << pack2.asText().toLatin1().constData();
        qDebug() << "Equal:" << (pack2 == pack? "true" : "false");
        qDebug() << "Delta:" << pack.delta(pack2);
        qDebug() << "Delta (reverse):" << pack2.delta(pack);
    }
    catch(Error const &err)
    {
        qWarning() << err.asText() << "\n";
    }

    qDebug() << "Exiting main()...\n";
    return 0;        
}
