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
#include "../gloom/geomath.h"

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
    };
    enum UserAction {
        None,
        TranslateView,
        SelectRegion,
        Move,
        Scale,
        Rotate,
        AddLines,
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
    ID         hoverPoint = 0;
    ID         hoverLine = 0;

    float    viewScale = 10;
    Vector2f viewOrigin;
    Matrix4f viewTransform;
    Matrix4f inverseViewTransform;

    Impl(Public *i) : Base(i)
    {
        /*map.append(map.points(), Point(-1, -1));
        map.append(map.points(), Point( 1, -1));
        map.append(map.points(), Point( 1,  1));
        map.append(map.points(), Point(-1,  1));

        map.append(map.lines(), Line{{0, 1}, {1, 0}});
        map.append(map.lines(), Line{{1, 2}, {1, 0}});
        map.append(map.lines(), Line{{2, 3}, {1, 0}});
        map.append(map.lines(), Line{{3, 0}, {1, 0}});*/

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
        const char *modeStr[3] = {
            "Points",
            "Lines",
            "Sectors",
        };
        return modeStr[mode];
    }

    String actionText() const
    {
        switch (userAction)
        {
        case TranslateView: return "translate view";
        case SelectRegion: return "select";
        case Move: return "move";
        case Scale: return "scale";
        case Rotate: return "rotate";
        case AddLines: return "add lines";
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
                .arg(mode == EditPoints  ? map.points() .size()
                   : mode == EditLines   ? map.lines()  .size()
                   : mode == EditSectors ? map.sectors().size() : 0)
                .arg(selText)
                .arg(actionText());
        if (hoverPoint)
        {
            text += String(" [P:%1]").arg(hoverPoint, 0, 16).toUpper();
        }
        if (hoverLine)
        {
            text += String(" [L:%1]").arg(hoverLine, 0, 16).toUpper();
        }
        return text;
    }

    QPointF worldToView(const Vector2d &pos) const
    {
        const auto p = viewTransform * Vector3f(float(pos.x), float(pos.y));
        return QPointF(p.x, p.y);
    }

    Vector2d viewToWorld(const QPointF &pos) const
    {
        const auto p = inverseViewTransform * Vector3f(float(pos.x()), float(pos.y()));
        return Vector2d(p.x, p.y);
    }

    void updateView()
    {
        const QSize viewSize = self().rect().size();

        viewTransform = Matrix4f::translate(Vector3f(viewSize.width() / 2, viewSize.height() / 2)) *
                        Matrix4f::scale(viewScale) * Matrix4f::translate(-viewOrigin);
        inverseViewTransform = viewTransform.inverse();
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
            for (auto i = map.points().begin(); i != map.points().end(); ++i)
            {
                const QPointF viewPos = worldToView(i.value());
                if (selectRect.contains(viewPos))
                {
                    selection.insert(i.key());
                }
            }
            break;

        case Move:
        case Scale:
        case Rotate:
        case AddLines:
            break;
        }

        userAction = None;
        actionPos  = QPoint();
        selectRect = QRectF();

        self().setCursor(Qt::CrossCursor);
        self().update();
        return true;
    }

    QPoint viewMousePos() const { return self().mapFromGlobal(QCursor().pos()); }

    Vector2d worldMousePos() const { return viewToWorld(viewMousePos()); }

    Vector2d worldActionPos() const { return viewToWorld(actionPos); }

    void userAdd()
    {
        switch (mode)
        {
        case EditPoints: map.append(map.points(), worldMousePos()); break;

        case EditLines:
            if (selection.size() == 1)
            {
                beginAction(AddLines);
            }
            break;
        }
        self().update();
    }

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
                map.removeInvalid();
                selection.clear();
            }
            break;

        case EditLines:
            if (hoverLine)
            {
                pushUndo();
                map.lines().remove(hoverLine);
                hoverLine = 0;
                selection.remove(hoverLine);
            }
            break;
        }
        self().update();
    }

    void userClick(Qt::KeyboardModifiers modifiers)
    {
        if (userAction == AddLines)
        {
            ID prevPoint = 0;
            if (selection.size()) prevPoint = *selection.begin();

            selection.clear();
            selectClickedObject();
            if (!selection.isEmpty())
            {
                // Connect to this point.
                Line newLine;
                newLine.points[0] = prevPoint;
                newLine.points[1] = *selection.begin();
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

        switch (mode)
        {
        case EditPoints:
        case EditLines:
            if (!(modifiers & Qt::ShiftModifier))
            {
                selection.clear();
            }
            selectClickedObject();
            break;
        }
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
            QPointF mid = (a + b) / 2;

            ptr.drawLine(mid, mid + offsets[0].toPointF());
            ptr.drawLine(mid, mid + offsets[1].toPointF());
        }
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
        if (maxDistance < 0)
        {
            maxDistance = defaultClickDistance();
        }

        ID id = 0;
        double dist = maxDistance;
        for (auto i = map.lines().begin(); i != map.lines().end(); ++i)
        {
            const auto &line = i.value();
            const geo::Line<Vector2d> mapLine(map.points()[line.points[0]], map.points()[line.points[1]]);
            double d = mapLine.distanceTo(pos);
            if (d < dist)
            {
                id = i.key();
                dist = d;
            }
        }
        return id;
    }

    void selectClickedObject()
    {
        const auto pos = worldActionPos();
        switch (mode)
        {
        case EditPoints:
        case EditLines:
            if (auto id = findPointAt(pos))
            {
                selection.insert(id);
            }
            break;
        }
    }

    QLineF viewLine(const Line &line) const
    {
        const QPointF start = worldToView(map.points()[line.points[0]]);
        const QPointF end   = worldToView(map.points()[line.points[1]]);
        return QLineF(start, end);
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
    addKeyAction("Ctrl+1", [this]() { d->setMode(Impl::EditPoints); });
    addKeyAction("Ctrl+2", [this]() { d->setMode(Impl::EditLines); });
    addKeyAction("Ctrl+D", [this]() { d->userAdd(); });
    addKeyAction("Ctrl+Backspace", [this]() { d->userDelete(); });
    addKeyAction("R", [this]() { d->userRotate(); });
    addKeyAction("S", [this]() { d->userScale(); });
    addKeyAction("Ctrl+Z", [this]() { d->popUndo(); });
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

    const int lineHgt = fontMetrics.height();
    const int gap = 6;

    const QColor panelBg = (d->mode == Impl::EditPoints? QColor(0, 0, 0, 128)
                          : d->mode == Impl::EditLines? QColor(0, 20, 48, 150)
                          : QColor(64, 32, 0, 128));
    const QColor metaBg(255, 255, 255, 64);
    const QColor selectColor(0, 0, 255, 128);
    const QColor gridMajor{180, 180, 180, 255};
    const QColor gridMinor{220, 220, 220, 255};
    const QColor textColor(255, 255, 255, 255);
    const QColor metaColor(0, 0, 0, 92);
    const QColor pointColor(170, 0, 0, 255);
    const QColor lineColor(64, 64, 64);

    // Grid.
    {
        d->drawGridLine(ptr, d->worldMousePos(), gridMinor);
        d->drawGridLine(ptr, Vector2d(), gridMajor);
    }

    // Points.
    {
        ptr.setPen(metaColor);
        ptr.setFont(d->metaFont);

        QVector<QPointF> points;
        QVector<QRectF> selected;

        for (auto i = d->map.points().begin(); i != d->map.points().end(); ++i)
        {
            const ID      id  = i.key();
            const QPointF pos = d->worldToView(i.value());

            points << pos;

            // Show ID numbers.
            if (d->mode == Impl::EditPoints)
            {
                const String label = String::number(id, 16).toUpper();
                ptr.drawText(pos + QPointF(-metaMetrics.width(label)/2, -gap), label);
            }

            // Indicate selected points.
            if (d->selection.contains(id))
            {
                selected << QRectF(pos - QPointF(gap, gap), QSizeF(2*gap, 2*gap));
            }
        }
        ptr.setFont(font());

        ptr.setPen(QPen(pointColor, 4));
        ptr.drawPoints(&points[0], points.size());

        if (selected.size())
        {
            ptr.setPen(QPen(selectColor));
            ptr.setBrush(Qt::NoBrush);
            ptr.drawRects(&selected[0], selected.size());
        }
    }

    // Lines.
    {
        ptr.setPen(lineColor);

        QVector<QLineF> lines;
        for (auto i = d->map.lines().begin(); i != d->map.lines().end(); ++i)
        {
            lines << d->viewLine(i.value());
        }
        ptr.drawLines(&lines[0], lines.size());

        if (d->mode == Impl::EditLines && d->hoverLine)
        {
            const QLineF vl = d->viewLine(d->map.lines()[d->hoverLine]);
            ptr.setPen(QPen(lineColor, 2));
            d->drawArrow(ptr, vl.p1(), vl.p2());
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
            const auto startPos = d->worldToView(d->map.points()[*d->selection.begin()]);
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
                    d->selectClickedObject();
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
        if (d->mode == Impl::EditPoints)
        {
            QPoint delta = event->pos() - d->actionPos;
            d->actionPos = event->pos();
            for (auto id : d->selection)
            {
                Vector2d worldDelta = Vector2d(delta.x(), delta.y()) / d->viewScale;
                d->map.points()[id] += worldDelta;
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
            const Vector3d p    = d->map.points()[id];
            d->map.points()[id] = xf * p;
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
