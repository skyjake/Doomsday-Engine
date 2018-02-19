/** @file editor.cpp
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

#include "editor.h"
#include "../src/gloomapp.h"
#include "gloom/geo/geomath.h"

#include <de/Matrix>

#include <QAction>
#include <QCloseEvent>
#include <QFile>
#include <QPainter>
#include <QSettings>

using namespace de;
using namespace gloom;

static const int DRAG_MIN_DIST = 2;
static const int UNDO_MAX = 50;

enum Direction {
    Horizontal = 0x1,
    Vertical   = 0x2,
    Both       = Horizontal | Vertical,
};
Q_DECLARE_FLAGS(Directions, Direction)
Q_DECLARE_OPERATORS_FOR_FLAGS(Directions)

DENG2_PIMPL(Editor)
{
    enum Mode {
        EditPoints,
        EditLines,
        EditSectors,
        EditEntities,
    };
    enum UserAction {
        None,
        TranslateView,
        SelectRegion,
        Move,
        Scale,
        Rotate,
        AddLines,
        AddSector,
    };

    Map map;
    QList<Map> undoStack;

    Mode       mode       = EditPoints;
    UserAction userAction = None;
    QPoint     actionPos;
    QPoint     pivotPos;
    QFont      metaFont;
    QRectF     selectRect;
    QSet<ID>   selection;
    ID         hoverPoint  = 0;
    ID         hoverLine   = 0;
    ID         hoverSector = 0;
    ID         hoverEntity = 0;

    float    viewScale = 10;
    Vector2f viewOrigin;
    Plane    viewPlane;
    Matrix4f viewTransform;
    Matrix4f inverseViewTransform;

    const QColor metaBg{255, 255, 255, 192};
    const QColor metaColor{0, 0, 0, 128};
    const QColor metaBg2{0, 0, 0, 128};
    const QColor metaColor2{255, 255, 255};

    Impl(Public *i) : Base(i)
    {
        // Load the last map.
        {
            QFile f(persistentMapPath());
            if (f.exists() && f.open(QFile::ReadOnly))
            {
                const Block data = f.readAll();
                map.deserialize(data);
            }
        }

        // Check for previous state.
        {
            QSettings st;
            viewScale  = st.value("viewScale", 10).toFloat();
            viewOrigin = geo::toVector2d(st.value("viewOrigin").value<QVector2D>());
        }
    }

    ~Impl()
    {
        // Save the editor state.
        {
            QSettings st;
            st.setValue("viewScale", viewScale);
            st.setValue("viewOrigin", geo::toQVector2D(viewOrigin));
        }

        // Save the map for later.
        {
            QFile f(persistentMapPath());
            if (f.open(QFile::WriteOnly))
            {
                const Block data = map.serialize();
                f.write(data.constData(), data.size());
            }
        }
    }

    String persistentMapPath() const
    {
        return GloomApp::app().userDir().filePath("persist.gloommap");
    }

    String modeText() const
    {
        const char *modeStr[4] = {
            "Points",
            "Lines",
            "Sectors",
            "Entities",
        };
        return modeStr[mode];
    }

    String actionText() const
    {
        switch (userAction)
        {
        case TranslateView: return "Translate view";
        case SelectRegion: return "Select";
        case Move: return "Move";
        case Scale: return "Scale";
        case Rotate: return "Rotate";
        case AddLines: return "Add lines";
        case AddSector: return "Add sector";
        case None: break;
        }
        return "";
    }

    String statusText() const
    {
        String selText;
        if (selection.size())
        {
            selText = String(":%1").arg(selection.size());
        }
        String text = String("%1 (%2%3) %4")
                .arg(modeText())
                .arg(mode == EditPoints   ? map.points()  .size()
                   : mode == EditLines    ? map.lines()   .size()
                   : mode == EditSectors  ? map.sectors() .size()
                   : mode == EditEntities ? map.entities().size()
                   : 0)
                .arg(selText)
                .arg(actionText());
        if (hoverPoint)
        {
            text += String(" [Point:%1]").arg(hoverPoint, 0, 16);
        }
        if (hoverLine)
        {
            text += String(" [Line:%1]").arg(hoverLine, 0, 16);
        }
        if (hoverEntity)
        {
            text += String(" [Entity:%1]").arg(hoverEntity, 0, 16);
        }
        return text;
    }

    void setMode(Mode newMode)
    {
        finishAction();
        mode = newMode;
        self().update();
    }

    bool isModifyingAction(UserAction action) const
    {
        switch (action)
        {
            default: return false;

            case Move:
            case Rotate:
            case Scale:
            case AddLines:
            case AddSector:
                return true;
        }
    }

    void beginAction(UserAction action)
    {
        finishAction();

        if (isModifyingAction(action))
        {
            pushUndo();
        }

        userAction = action;
        switch (action)
        {
        case Rotate:
        case Scale:
            pivotPos = actionPos = viewMousePos();
            self().setCursor(action == Rotate? Qt::SizeVerCursor : Qt::SizeFDiagCursor);
            break;

        default: break;
        }
    }

    bool finishAction()
    {
        switch (userAction)
        {
        case None:
            return false;

        case TranslateView:
            break;

        case SelectRegion:
            switch (mode)
            {
            case EditPoints:
                for (auto i = map.points().begin(); i != map.points().end(); ++i)
                {
                    const QPointF viewPos = worldToView(i.value());
                    if (selectRect.contains(viewPos))
                    {
                        selection.insert(i.key());
                    }
                }
                break;

            case EditLines:
            case EditSectors:
                for (auto i = map.lines().begin(); i != map.lines().end(); ++i)
                {
                    const auto &line = i.value();
                    QPointF viewPos[2] = {worldToView(map.point(line.points[0])),
                                          worldToView(map.point(line.points[1]))};
                    if (selectRect.contains(viewPos[0]) && selectRect.contains(viewPos[1]))
                    {
                        selection.insert(i.key());
                    }
                }
                break;
            }
            break;

        case Move:
        case Scale:
        case Rotate:
        case AddLines:
        case AddSector:
            break;
        }

        userAction = None;
        actionPos  = QPoint();
        selectRect = QRectF();

        self().setCursor(Qt::CrossCursor);
        self().update();
        return true;
    }

    QPointF worldToView(const Vector2d &pos, const Plane *plane = nullptr) const
    {
        if (!plane) plane = &viewPlane;
        const auto p = viewTransform * plane->projectPoint(pos);
        return QPointF(p.x, p.y);
    }

    Vector2d viewToWorld(const QPointF &pos) const
    {
        const auto p = inverseViewTransform * Vector3f(float(pos.x()), float(pos.y()));
        return Vector2d(p.x, p.z);
    }

    void updateView()
    {
        const QSize viewSize = self().rect().size();

        viewPlane     = Plane{{viewOrigin.x, 0, viewOrigin.y}, {0, 1, 0}};
        viewTransform = Matrix4f::translate(Vector3f(viewSize.width() / 2, viewSize.height() / 2)) *
                        Matrix4f::rotate(-90, Vector3f(1, 0, 0)) *
                        Matrix4f::scale(viewScale) * Matrix4f::translate(-viewPlane.point);
        inverseViewTransform = viewTransform.inverse();
    }

    QPoint viewMousePos() const { return self().mapFromGlobal(QCursor().pos()); }

    QLineF viewLine(const Line &line) const
    {
        const QPointF start = worldToView(map.point(line.points[0]));
        const QPointF end   = worldToView(map.point(line.points[1]));
        return QLineF(start, end);
    }

    Vector2d worldMousePos() const { return viewToWorld(viewMousePos()); }

    Vector2d worldActionPos() const { return viewToWorld(actionPos); }

    void pushUndo()
    {
        undoStack.append(map);
        if (undoStack.size() > UNDO_MAX)
        {
            undoStack.removeFirst();
        }
    }

    void popUndo()
    {
        if (!undoStack.isEmpty())
        {
            map = undoStack.takeLast();
            self().update();
        }
    }

    void userSelectAll()
    {
        selection.clear();
        switch (mode)
        {
        case EditPoints:
            for (auto id : map.points().keys())
            {
                selection.insert(id);
            }
            break;

        case EditLines:
            for (auto id : map.lines().keys())
            {
                selection.insert(id);
            }
            break;

        case EditSectors:
            for (auto id : map.sectors().keys())
            {
                selection.insert(id);
            }
            break;

        case EditEntities:
            for (auto id : map.entities().keys())
            {
                selection.insert(id);
            }
            break;
        }
        self().update();
    }

    void userSelectNone()
    {
        selection.clear();
        self().update();
    }

    void userAdd()
    {
        switch (mode)
        {
        case EditPoints:
            pushUndo();
            map.append(map.points(), worldMousePos());
            break;

        case EditLines:
            if (selection.size() == 1)
            {
                beginAction(AddLines);
            }
            break;

        case EditSectors:
                /*
            if (selection.size() >= 3)
            {
                pushUndo();
                Sector sector;
                for (ID id : selection)
                {
                    if (map.isLine(id)) sector.walls << id;
                }
                selection.clear();
                ID floor = map.append(map.planes(), Plane{{Vector3d()}, {Vector3f(0, 1, 0)}});
                ID ceil  = map.append(map.planes(), Plane{{Vector3d(0, 3, 0)}, {Vector3f(0, -1, 0)}});
                ID vol   = map.append(map.volumes(), Volume{{floor, ceil}});
                sector.volumes << vol;
                ID secId = map.append(map.sectors(), sector);
                if (!linkSectorLines(secId))
                {
                    popUndo();
                }
                selection.insert(secId);
            }*/
            break;

        case EditEntities:
            pushUndo();
            std::shared_ptr<Entity> ent(new Entity);
            Vector3d pos = worldMousePos();
            ent->setPosition(Vector3d(pos.x, 0, pos.y));
            ent->setId(map.append(map.entities(), ent));
            break;
        }
        self().update();
    }

    void userDelete()
    {
        switch (mode)
        {
        case EditPoints:
            if (selection.size())
            {
                pushUndo();
                for (auto id : selection)
                {
                    map.points().remove(id);
                }
            }
            break;

        case EditLines:
            if (hoverLine)
            {
                pushUndo();
                map.lines().remove(hoverLine);
                hoverLine = 0;
            }
            break;

        case EditSectors:
            if (hoverSector)
            {
                pushUndo();
                map.sectors().remove(hoverSector);
                hoverSector = 0;
            }
            break;

        case EditEntities:
            if (hoverEntity)
            {
                pushUndo();
                map.entities().remove(hoverEntity);
                hoverEntity = 0;
            }
            break;
        }
        selection.clear();
        map.removeInvalid();
        self().update();
    }

    void userClick(Qt::KeyboardModifiers modifiers)
    {
        if (userAction == AddLines)
        {
            ID prevPoint = 0;
            if (selection.size()) prevPoint = *selection.begin();

            selection.clear();
            selectClickedObject(modifiers);
            if (!selection.isEmpty())
            {
                // Connect to this point.
                Line newLine;
                newLine.points[0]  = prevPoint;
                newLine.points[1]  = *selection.begin();
                newLine.sectors[0] = newLine.sectors[1] = 0;
                if (newLine.points[0] != newLine.points[1])
                {
                    map.append(map.lines(), newLine);
                    self().update();
                    return;
                }
            }
        }

        if (userAction != None)
        {
            finishAction();
            return;
        }

        if (mode == EditSectors && !hoverSector && hoverLine)
        {
            if (modifiers & Qt::ShiftModifier)
            {
                selectOrUnselect(hoverLine);
                return;
            }

            // Select a group of lines that might form a new sector.
            const auto clickPos = worldMousePos();
            Edge startRef{hoverLine, map.geoLine(hoverLine).isFrontSide(clickPos)? Line::Front : Line::Back};

            if (map.line(hoverLine).sectors[startRef.side] == 0)
            {
                IDList      secPoints;
                IDList      secWalls;
                QList<Edge> secEdges;

                if (map.buildSector(startRef, secPoints, secWalls, secEdges))
                {
                    pushUndo();

                    const ID floor = map.append(map.planes(), Plane{{Vector3d()}, {Vector3f(0, 1, 0)}});
                    const ID ceil  = map.append(map.planes(), Plane{{Vector3d(0, 3, 0)}, {Vector3f(0, -1, 0)}});
                    const ID vol   = map.append(map.volumes(), Volume{{floor, ceil}});

                    Sector newSector{secPoints, secWalls, {vol}};
                    ID secId = map.append(map.sectors(), newSector);

                    for (Edge edge : secEdges)
                    {
                        map.line(edge.line).sectors[edge.side] = secId;
                    }
                    selection.clear();
                    selection.insert(secId);
                }
            }
            return;
        }

        // Select clicked object.
        if (!(modifiers & Qt::ShiftModifier))
        {
            selection.clear();
        }
        selectClickedObject(modifiers);
    }

    void userScale()
    {
        if (userAction != None)
        {
            finishAction();
        }
        else if (!selection.isEmpty())
        {
            beginAction(Scale);
        }
        self().update();
    }

    void userRotate()
    {
        if (userAction != None)
        {
            finishAction();
        }
        else if (!selection.isEmpty())
        {
            beginAction(Rotate);
        }
        self().update();
    }

    void drawGridLine(QPainter &ptr,
                      const Vector2d &worldPos,
                      const QColor &color,
                      Directions dirs = Both)
    {
        const QRect   winRect = self().rect();
        const QPointF origin  = worldToView(worldPos);

        ptr.setPen(color);

        if (dirs & Vertical)   ptr.drawLine(QLineF(origin.x(), 0, origin.x(), winRect.height()));
        if (dirs & Horizontal) ptr.drawLine(QLineF(0, origin.y(), winRect.width(), origin.y()));
    }

    void drawArrow(QPainter &ptr, QPointF a, QPointF b)
    {
        ptr.drawLine(a, b);

        QVector2D span(float(b.x() - a.x()), float(b.y() - a.y()));
        const float len = 5;
        if (span.length() > 5*len)
        {
            QVector2D dir = span.normalized();
            QVector2D normal{dir.y(), -dir.x()};
            QVector2D offsets[2] = {len*normal - 2*len*dir, -len*normal - 2*len*dir};
            QPointF mid = (a + 3*b) / 4;

            //ptr.drawLine(mid, mid + offsets[0].toPointF());
            ptr.drawLine(mid, mid + offsets[1].toPointF());
        }
    }

    void drawMetaLabel(QPainter &ptr, QPointF pos, QString text, bool lightStyle = true)
    {
        ptr.save();

        ptr.setFont(metaFont);
        ptr.setBrush(lightStyle? metaBg : metaBg2);
        ptr.setPen(Qt::NoPen);

        QFontMetrics metrics(metaFont);
        const QSize dims(metrics.width(text), metrics.height());
        const QPointF off(-dims.width()/2, dims.height()/2);
        const QPointF gap(-3, 3);

        ptr.drawRect(QRectF(pos - off - gap, pos + off + gap));
        ptr.setPen(lightStyle? metaColor : metaColor2);
        ptr.drawText(pos + off + QPointF(0, -metrics.descent()), text);

        ptr.restore();
    }

    double defaultClickDistance() const
    {
        return 20 / viewScale;
    }

    ID findPointAt(const Vector2d &pos, double maxDistance = -1) const
    {
        if (maxDistance < 0)
        {
            maxDistance = defaultClickDistance();
        }

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.points().begin(); i != map.points().end(); ++i)
        {
            double d = (i.value() - pos).length();
            if (d < dist)
            {
                id = i.key();
                dist = d;
            }
        }
        return id;
    }

    ID findLineAt(const Vector2d &pos, double maxDistance = -1) const
    {
        if (maxDistance < 0) maxDistance = defaultClickDistance();

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.lines().begin(); i != map.lines().end(); ++i)
        {
            const auto &line = i.value();
            const geo::Line<Vector2d> mapLine(map.point(line.points[0]), map.point(line.points[1]));
            double d = mapLine.distanceTo(pos);
            if (d < dist)
            {
                id = i.key();
                dist = d;
            }
        }
        return id;
    }

    ID findSectorAt(const Vector2d &pos) const
    {
        for (ID id : map.sectors().keys())
        {
            if (map.sectorPolygon(id).isPointInside(pos))
            {
                return id;
            }
        }
        return 0;
    }

    ID findEntityAt(const Vector2d &pos, double maxDistance = -1) const
    {
        if (maxDistance < 0) maxDistance = defaultClickDistance();

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.entities().begin(), end = map.entities().end(); i != end; ++i)
        {
            const auto &ent = i.value();
            double d = (ent->position().xz() - pos).length();
            if (d < dist)
            {
                id = i.key();
                dist = d;
            }
        }
        return id;
    }

    void selectOrUnselect(ID id)
    {
        if (!selection.contains(id))
        {
            selection.insert(id);
        }
        else
        {
            selection.remove(id);
        }
    }

    void selectClickedObject(Qt::KeyboardModifiers modifiers)
    {
        const auto pos = worldActionPos();
        switch (mode)
        {
        case EditPoints:
            if (auto id = findPointAt(pos))
            {
                selectOrUnselect(id);
            }
            break;

        case EditLines:
            if (modifiers & Qt::ShiftModifier)
            {
                if (hoverLine)
                {
                    selectOrUnselect(hoverLine);
                }
            }
            else
            {
                if (auto id = findPointAt(pos))
                {
                    selectOrUnselect(id);
                }
            }
            break;

        case EditSectors:
            if (hoverSector)
            {
                selectOrUnselect(hoverSector);
            }
            break;

        case EditEntities:
            if (hoverEntity)
            {
                selectOrUnselect(hoverEntity);
            }
            break;
        }
    }

    void build()
    {
        emit self().buildMapRequested();
    }
};

Editor::Editor()
    : d(new Impl(this))
{
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    QSettings st;
    if (st.contains("editorGeometry"))
    {
        restoreGeometry(st.value("editorGeometry").toByteArray());
    }

    auto addKeyAction = [this] (QString shortcut, std::function<void()> func)
    {
        QAction *add = new QAction;
        add->setShortcut(QKeySequence(shortcut));
        connect(add, &QAction::triggered, func);
        addAction(add);
    };

    d->metaFont = font();
    d->metaFont.setPointSizeF(d->metaFont.pointSizeF() * .75);

    // Actions.
    addKeyAction("Ctrl+1",          [this]() { d->setMode(Impl::EditPoints); });
    addKeyAction("Ctrl+2",          [this]() { d->setMode(Impl::EditLines); });
    addKeyAction("Ctrl+3",          [this]() { d->setMode(Impl::EditSectors); });
    addKeyAction("Ctrl+4",          [this]() { d->setMode(Impl::EditEntities); });
    addKeyAction("Ctrl+A",          [this]() { d->userSelectAll(); });
    addKeyAction("Ctrl+Shift+A",    [this]() { d->userSelectNone(); });
    addKeyAction("Ctrl+D",          [this]() { d->userAdd(); });
    addKeyAction("Ctrl+Backspace",  [this]() { d->userDelete(); });
    addKeyAction("R",               [this]() { d->userRotate(); });
    addKeyAction("S",               [this]() { d->userScale(); });
    addKeyAction("Ctrl+Z",          [this]() { d->popUndo(); });
    addKeyAction("Return",          [this]() { d->build(); });
}

Map &Editor::map()
{
    return d->map;
}

void Editor::closeEvent(QCloseEvent *event)
{
    QSettings().setValue("editorGeometry", saveGeometry());
    QWidget::closeEvent(event);
}

void Editor::paintEvent(QPaintEvent *)
{
    d->updateView();

    QPainter ptr(this);
    ptr.setRenderHint(QPainter::Antialiasing);

    QRect        winRect = rect();
    QFontMetrics fontMetrics(font());
    QFontMetrics metaMetrics(d->metaFont);

    const Points &  mapPoints  = d->map.points();
    const Planes &  mapPlanes  = d->map.planes();
    const Lines &   mapLines   = d->map.lines();
    const Sectors & mapSectors = d->map.sectors();
    const Volumes & mapVolumes = d->map.volumes();
    const Entities &mapEnts    = d->map.entities();

    const int lineHgt = fontMetrics.height();
    const int gap     = 6;

    const QColor panelBg = (d->mode == Impl::EditPoints?   QColor(0, 0, 0, 128)
                          : d->mode == Impl::EditLines?    QColor(0, 20, 90, 160)
                          : d->mode == Impl::EditEntities? QColor(140, 10, 0, 160)
                          : QColor(255, 160, 0, 192));
    const QColor selectColor(64, 92, 255);
    const QColor selectColorAlpha(selectColor.red(), selectColor.green(), selectColor.blue(), 150);
    const QColor gridMajor{180, 180, 180, 255};
    const QColor gridMinor{220, 220, 220, 255};
    const QColor textColor = (d->mode == Impl::EditSectors? QColor(0, 0, 0) : QColor(255, 255, 255));
    const QColor pointColor(170, 0, 0, 255);
    const QColor lineColor(64, 64, 64);
    const QColor sectorColor(128, 92, 0, 96);

    // Grid.
    {
        d->drawGridLine(ptr, d->worldMousePos(), gridMinor);
        d->drawGridLine(ptr, Vector2d(), gridMajor);
    }

    // Sectors.
    {
        for (auto i = mapSectors.begin(), end = mapSectors.end(); i != end; ++i)
        {
            const ID    secId  = i.key();
            const auto &sector = i.value();

            const auto geoPoly = d->map.sectorPolygon(secId);

            QPolygonF poly;
            for (const auto &pp : geoPoly.points)
            {
                poly.append(d->worldToView(pp.pos));
            }
            if (d->selection.contains(secId))
            {
                ptr.setPen(QPen(selectColor, 4));
            }
            else
            {
                ptr.setPen(Qt::NoPen);
            }
            ptr.setBrush(d->hoverSector == secId? panelBg : sectorColor);
            ptr.drawPolygon(poly);
            if (d->selection.contains(secId))
            {
                d->drawMetaLabel(ptr, poly.boundingRect().center(), String::format("%X", secId));
            }
        }
    }

    // Points.
    if (!mapPoints.isEmpty())
    {
        ptr.setPen(d->metaColor);
        ptr.setFont(d->metaFont);

        QVector<QPointF> points;
        QVector<QRectF>  selected;
        QVector<ID>      selectedIds;

        for (auto i = mapPoints.begin(); i != mapPoints.end(); ++i)
        {
            const ID      id  = i.key();
            const QPointF pos = d->worldToView(i.value());

            points << pos;

            // Indicate selected points.
            if (d->selection.contains(id))
            {
                selected << QRectF(pos - QPointF(gap, gap), QSizeF(2*gap, 2*gap));
                selectedIds << id;
            }
        }
        ptr.setFont(font());

        ptr.setPen(QPen(pointColor, 4));
        ptr.drawPoints(&points[0], points.size());

        if (selected.size())
        {
            ptr.setPen(QPen(selectColorAlpha));
            ptr.setBrush(Qt::NoBrush);
            ptr.drawRects(&selected[0], selected.size());

            // Show ID numbers.
            for (int i = 0; i < selected.size(); ++i)
            {
                d->drawMetaLabel(ptr, selected[i].center() - QPointF(0, 2*gap), String::format("%X", selectedIds[i]));
            }
        }
    }

    // Lines.
    if (!mapLines.isEmpty())
    {
        ptr.setPen(lineColor);

        QVector<QLineF> lines;
        QVector<QLineF> selected;
        QVector<ID>     selectedIds;

        for (auto i = mapLines.begin(); i != mapLines.end(); ++i)
        {
            auto vline = d->viewLine(i.value());
            lines << vline;

            if (d->selection.contains(i.key()))
            {
                selected << vline;
                selectedIds << i.key();
            }
        }
        ptr.drawLines(&lines[0], lines.size());

        if ((d->mode == Impl::EditLines || d->mode == Impl::EditSectors) && d->hoverLine)
        {
            const QLineF vl = d->viewLine(mapLines[d->hoverLine]);
            ptr.setPen(QPen(lineColor, 2));
            d->drawArrow(ptr, vl.p1(), vl.p2());
        }

        if (selected.size())
        {
            ptr.setPen(QPen(selectColor, 3));
            ptr.drawLines(&selected[0], selected.size());

            for (int i = 0; i < selected.size(); ++i)
            {
                const auto &line = mapLines[selectedIds[i]];
                const auto normal = selected[i].normalVector();
                QPointF delta{normal.dx(), normal.dy()};

                d->drawMetaLabel(ptr, selected[i].center(), String::format("%X", selectedIds[i]));

                if (normal.length() > 80)
                {
                    delta /= normal.length();

                    //if (line.sectors[0])
                        d->drawMetaLabel(ptr,
                                         selected[i].center() + delta * -20,
                                         String::format("%X", line.sectors[0]), false);
                    if (line.sectors[1])
                        d->drawMetaLabel(ptr,
                                         selected[i].center() + delta * 20,
                                         String::format("%X", line.sectors[1]), false);
                }
            }
        }
    }

    // Entities.
    {
        ptr.setPen(Qt::black);

        for (auto i = mapEnts.begin(), end = mapEnts.end(); i != end; ++i)
        {
            const auto &  ent = i.value();
            const QPointF pos = d->worldToView(ent->position().xz());

            float radius = 0.5f * d->viewScale;
            ptr.setBrush(d->selection.contains(i.key())? selectColor : QColor(Qt::white));
            ptr.drawEllipse(pos, radius, radius);
        }
    }

    // Status bar.
    {
        const int statusHgt = lineHgt + 2*gap;
        const QRect rect{0, winRect.height() - statusHgt, winRect.width(), statusHgt};
        const QRect content = rect.adjusted(gap, gap, -gap, -gap);

        ptr.setBrush(QBrush(panelBg));
        ptr.setPen(Qt::NoPen);
        ptr.drawRect(rect);

        ptr.setBrush(Qt::NoBrush);
        ptr.setPen(textColor);
        const int y = content.center().y() + fontMetrics.ascent()/2;
        ptr.drawText(content.left(), y, d->statusText());

        const auto mouse = d->worldMousePos();
        const String viewText = String("[%1 %2] (%3 %4) z:%5")
                .arg(mouse.x,         0, 'f', 1)
                .arg(mouse.y,         0, 'f', 1)
                .arg(d->viewOrigin.x, 0, 'f', 1)
                .arg(d->viewOrigin.y, 0, 'f', 1)
                .arg(d->viewScale,    0, 'f', 2);
        ptr.drawText(content.right() - fontMetrics.width(viewText), y, viewText);
    }

    // Current selection.
    if (d->userAction == Impl::SelectRegion)
    {
        ptr.setPen(selectColor);
        ptr.setBrush(Qt::NoBrush);
        ptr.drawRect(d->selectRect);
    }

    // Line connection indicator.
    if (d->userAction == Impl::AddLines)
    {
        const QColor invalidColor{200, 0, 0};
        const QColor validColor{0, 200, 0};

        if (d->selection.size())
        {
            const auto startPos = d->worldToView(d->map.point(*d->selection.begin()));
            const auto endPos   = d->viewMousePos();

            ptr.setPen(QPen(d->hoverPoint? validColor : invalidColor, 2));
            d->drawArrow(ptr, startPos, endPos);
        }
    }
}

void Editor::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    d->actionPos = event->pos();
}

void Editor::mouseMoveEvent(QMouseEvent *event)
{
    // Check what the mouse is hovering on.
    {
        const auto pos = d->viewToWorld(event->pos());
        d->hoverPoint = d->findPointAt(pos);
        d->hoverLine  = d->findLineAt(pos);
        d->hoverSector = (d->mode == Impl::EditSectors? d->findSectorAt(pos) : 0);
        d->hoverEntity = d->findEntityAt(pos);
    }

    // Begin a drag action.
    if (event->buttons() && d->userAction == Impl::None &&
        (event->pos() - d->actionPos).manhattanLength() >= DRAG_MIN_DIST)
    {
        if (event->buttons() & Qt::LeftButton)
        {
            if (event->modifiers() & Qt::ShiftModifier)
            {
                d->beginAction(Impl::SelectRegion);
                update();
            }
            else
            {
                if (d->selection.size() <= 1)
                {
                    d->selection.clear();
                    d->selectClickedObject(event->modifiers());
                }
                if (!d->selection.isEmpty())
                {
                    d->beginAction(Impl::Move);
                    update();
                }
            }
        }
        if ((event->modifiers() & Qt::ShiftModifier) && (event->buttons() & Qt::RightButton))
        {
            // Translate the view.
            d->beginAction(Impl::TranslateView);
            update();
        }
    }

    switch (d->userAction)
    {
    case Impl::TranslateView: {
        QPoint delta = event->pos() - d->actionPos;
        d->actionPos = event->pos();
        d->viewOrigin -= Vector2f(delta.x(), delta.y()) / d->viewScale;
        d->updateView();
        break;
    }
    case Impl::SelectRegion:
        d->selectRect = QRect(d->actionPos, event->pos());
        break;

    case Impl::Move: {
        if (d->mode == Impl::EditPoints || d->mode == Impl::EditEntities)
        {
            QPoint delta = event->pos() - d->actionPos;
            d->actionPos = event->pos();
            const Vector2d worldDelta = Vector2d(delta.x(), delta.y()) / d->viewScale;
            for (auto id : d->selection)
            {
                if (d->mode == Impl::EditPoints && d->map.points().contains(id))
                {
                    d->map.point(id) += worldDelta;
                }
                else if (d->mode == Impl::EditEntities && d->map.entities().contains(id))
                {
                    auto &ent = d->map.entity(id);
                    ent.setPosition(ent.position() + Vector3d(worldDelta.x, 0, worldDelta.y));
                }
            }
        }
        break;
    }
    case Impl::Rotate:
    case Impl::Scale:
    {
        QPoint delta = event->pos() - d->actionPos;
        d->actionPos = event->pos();

        Matrix4f xf;

        if (d->userAction == Impl::Rotate)
        {
            float      angle = delta.y() / 2.f;
            const auto pivot = d->viewToWorld(d->pivotPos);
            xf               = Matrix4f::rotateAround(
                Vector3f(float(pivot.x), float(pivot.y)), angle, Vector3f(0, 0, 1));
        }
        else
        {
            const Vector3d pivot = d->viewToWorld(d->pivotPos);
            Vector3f scaler(1 + delta.x()/100.f, 1 + delta.y()/100.f);
            if (!(event->modifiers() & Qt::AltModifier)) scaler.y = scaler.x;
            xf = Matrix4f::translate(pivot) * Matrix4f::scale(scaler) * Matrix4f::translate(-pivot);
        }

        for (auto id : d->selection)
        {
            if (d->map.points().contains(id))
            {
                d->map.point(id) = xf * Vector3d(d->map.point(id));
            }
        }
        break;
    }
    default:
        break;
    }

    update();
}

void Editor::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();

    if (d->userAction != Impl::None && d->userAction != Impl::AddLines)
    {
        d->finishAction();
        update();
    }
    else
    {
        if ((event->pos() - d->actionPos).manhattanLength() < DRAG_MIN_DIST)
        {
            d->userClick(event->modifiers());
            update();
        }
    }
}

void Editor::mouseDoubleClickEvent(QMouseEvent *)
{
}

void Editor::wheelEvent(QWheelEvent *event)
{
    const QPoint delta = event->pixelDelta();
    if (event->modifiers() & Qt::ShiftModifier)
    {
        d->viewScale *= de::clamp(.1f, 1.f - delta.y()/1000.f, 10.f);
    }
    else
    {
        d->viewOrigin -= Vector2f(delta.x(), delta.y()) / d->viewScale;
    }
    d->updateView();
    update();
}
