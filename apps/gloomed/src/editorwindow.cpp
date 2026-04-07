/** @file editorwindow.cpp
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "editorwindow.h"
#include "utils.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QToolBar>

using namespace de;
using namespace gloom;

DE_PIMPL(EditorWindow)
{
    Editor *editor;
    bool    stateBeingUpdated = false;

    Impl(Public *i) : Base(i) {}
};

EditorWindow::EditorWindow()
    : d(new Impl(this))
{
    d->editor = new Editor;
    setCentralWidget(d->editor);
    d->editor->updateWindowTitle();

    const QStringList allMaterials({"",
                                    "world.stone",
                                    "world.dirt",
                                    "world.grass",
                                    "world.test",
                                    "world.test2",
                                    "world.metal",
                                    "world.water"});

    // Line properties.
    {
        QToolBar *matr = new QToolBar(tr("Line Material"));
        addToolBar(Qt::BottomToolBarArea, matr);

        connect(d->editor, &Editor::modeChanged, matr, [matr] (int mode) {
            matr->setVisible(mode == Editor::EditLines);
        });

        matr->addWidget(new QLabel(tr("Line")));

        QComboBox *sideBox = new QComboBox;
        sideBox->addItem(tr("Front"), gloom::Line::Front);
        sideBox->addItem(tr("Back"),  gloom::Line::Back);
        matr->addWidget(sideBox);
        matr->setDisabled(true);

        QComboBox *lineMat[3] = { new QComboBox, new QComboBox, new QComboBox };
        for (auto *cb : lineMat)
        {
            cb->addItems(allMaterials);
            matr->addWidget(cb);
            connect(cb,
                    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    [this, sideBox, lineMat, cb](int)
            {
                if (!d->stateBeingUpdated)
                {
                    Map &               map  = d->editor->map();
                    const int           side = sideBox->currentData().toInt();
                    const Line::Section section =
                        (cb == lineMat[0] ? Line::Bottom : cb == lineMat[1] ? Line::Middle : Line::Top);

                    // Change line materials.
                    foreach (ID id, d->editor->selection())
                    {
                        if (map.isLine(id))
                        {
                            auto &line = map.line(id);
                            line.surfaces[side].material[section] = convert(cb->currentText());
                            d->editor->markAsChanged();
                        }
                    }
                }
            });
        }

        auto updateLineToolbar = [this, matr, sideBox, lineMat]()
        {
            d->stateBeingUpdated = true;

            const auto sel = d->editor->selection();
            matr->setDisabled(sel.isEmpty());

            if (!sel.isEmpty())
            {
                const Map &map = d->editor->map();
                for (ID id : sel)
                {
                    if (!map.isLine(id)) continue;

                    // Use the first selected Line for updating the widgets.
                    const Line &line = map.line(id);
                    int side = sideBox->currentData().toInt();
                    for (int i = 0; i < 3; ++i)
                    {
                        lineMat[i]->setCurrentIndex(
                            lineMat[i]->findText(convert(line.surfaces[side].material[i])));
                    }
                    break;
                }
            }

            d->stateBeingUpdated = false;
        };

        connect(d->editor, &Editor::lineSelectionChanged, updateLineToolbar);
        connect(sideBox,
                static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                updateLineToolbar);
    }

    // Plane materials.
    {
        QToolBar *matr = new QToolBar(tr("Plane Material"));
        addToolBar(Qt::BottomToolBarArea, matr);

        connect(d->editor, &Editor::modeChanged, matr, [matr] (int mode) {
            matr->setVisible(mode == Editor::EditPlanes);
        });

        matr->addWidget(new QLabel(tr("Plane")));
        matr->setDisabled(true);

        QComboBox *planeMat[2] = {new QComboBox, new QComboBox};
        for (auto *cb : planeMat)
        {
            cb->addItems(allMaterials);
            matr->addWidget(cb);
            connect(cb,
                    static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    [this, planeMat, cb](int)
            {
                if (!d->stateBeingUpdated)
                {
                    Map &map = d->editor->map();

                    // Change line materials.
                    foreach (ID id, d->editor->selection())
                    {
                        if (map.isPlane(id))
                        {
                            auto &plane = map.plane(id);
                            plane.material[cb == planeMat[0]? 0 : 1] = convert(cb->currentText());
                            d->editor->markAsChanged();
                        }
                    }
                }
            });
        }

        auto updatePlaneToolbar = [this, matr, planeMat]()
        {
            d->stateBeingUpdated = true;

            const auto sel = d->editor->selection();
            matr->setDisabled(sel.isEmpty());

            if (!sel.isEmpty())
            {
                const Map &map = d->editor->map();
                for (ID id : sel)
                {
                    if (!map.isPlane(id)) continue;

                    // Use the first selected Line for updating the widgets.
                    const Plane &plane = map.plane(id);
                    for (int i = 0; i < 2; ++i)
                    {
                        planeMat[i]->setCurrentIndex(
                            planeMat[i]->findText(convert(plane.material[i])));
                    }
                    break;
                }
            }

            d->stateBeingUpdated = false;
        };

        connect(d->editor, &Editor::planeSelectionChanged, updatePlaneToolbar);
    }

    QSettings st;
    if (st.contains("editorGeometry"))
    {
        restoreGeometry(st.value("editorGeometry").toByteArray());
    }
}

Editor &EditorWindow::editor()
{
    return *d->editor;
}

void EditorWindow::closeEvent(QCloseEvent *event)
{
    if (d->editor->maybeClose())
    {
        event->accept();
        QSettings().setValue("editorGeometry", saveGeometry());
        QMainWindow::closeEvent(event);
    }
    else
    {
        event->ignore();
    }
}
