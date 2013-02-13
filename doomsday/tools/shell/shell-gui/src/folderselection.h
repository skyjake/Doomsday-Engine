/** @file folderselection.h  Widget for selecting a folder.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef FOLDERSELECTION_H
#define FOLDERSELECTION_H

#include <de/libdeng2.h>
#include <de/NativePath>
#include <QWidget>

/**
 * Widget for selecting a folder.
 */
class FolderSelection : public QWidget
{
    Q_OBJECT

public:
    explicit FolderSelection(QString const &prompt, QWidget *parent = 0);
    explicit FolderSelection(QString const &prompt, QString const &extraLabel, QWidget *parent = 0);
    virtual ~FolderSelection();

    void setPath(de::NativePath const &path);

    de::NativePath path() const;
    
signals:
    void selected();
    
public slots:
    void selectFolder();
    
private:
    struct Instance;
    Instance *d;
};

#endif // FOLDERSELECTION_H
