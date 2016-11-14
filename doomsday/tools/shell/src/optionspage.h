/** @file optionspage.h  Page for game options.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef OPTIONSPAGE_H
#define OPTIONSPAGE_H

#include <QWidget>
#include <de/Record>

class OptionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit OptionsPage(QWidget *parent = 0);

    void updateWithGameState(de::Record const &gameState);

signals:
    void commandsSubmitted(QStringList commands);

public slots:

private:
    DENG2_PRIVATE(d)
};

#endif // OPTIONSPAGE_H
