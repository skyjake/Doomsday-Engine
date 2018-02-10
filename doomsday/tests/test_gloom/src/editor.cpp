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

#include <de/Matrix>

#include <QAction>
#include <QCloseEvent>
#include <QPainter>
#include <QSettings>

using namespace de;
using namespace gloom;

static const int DRAG_MIN_DIST = 2;

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
    };

    Map map;

    Mode       mode       = EditPoints;
    UserAction userAction = None;
    QPoint     actionPos;
    QPoint     pivotPos;
    QFont      metaFont;
    QRectF     selectRect;
    QSet<ID>   selection;

    float    viewScale = 10;
    Vector2f viewOrigin;
    Matrix4f viewTransform;
    Matrix4f inverseViewTransform;

    Impl(Public *i) : Base(i)
    {
        map.append(map.points(), Point(-1, -1));
        map.append(map.points(), Point( 1, -1));
        map.append(map.points(), Point( 1,  1));
        map.append(map.points(), Point(-1,  1));

        map.append(map.lines(), Line{{0, 1}, {1, 0}});
        map.append(map.lines(), Line{{1, 2}, {1, 0}});
        map.append(map.lines(), Line{{2, 3}, {1, 0}});
        map.append(map.lines(), Line{{3, 0}, {1, 0}});

/*        map.append(map.volumes(), )

        map.append(map.sectors(), Sector(IDList({5, 6, 7, 8}), IDList({9})));*/
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

    void beginAction(UserAction action)
    {
        if (userAction != None)
        {
            finishAction();
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

    void finishAction()
    {
        switch (userAction)
        {
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
        case None:
            break;
        }

        userAction = None;
        actionPos  = QPoint();
        selectRect = QRectF();
        self().setCursor(Qt::CrossCursor);
    }

    QPoint viewMousePos() const
    {
        return self().mapFromGlobal(QCursor().pos());
    }

    Vector2d worldMousePos() const
    {
        return viewToWorld(viewMousePos());
    }

    Vector2d worldActionPos() const
    {
        return viewToWorld(actionPos);
    }

    void userAdd()
    {
        switch (mode)
        {
        case EditPoints: map.append(map.points(), worldMousePos()); break;
        }
        self().update();
    }

    void userDelete()
    {
        switch (mode)
        {
        case EditPoints:
            for (auto id : selection)
            {
                map.points().remove(id);
            }
            selection.clear();
            break;
        }
        self().update();
    }

    void userClick(Qt::KeyboardModifiers modifiers)
    {
        if (userAction != None)
        {
            finishAction();
            self().update();
            return;
        }

        switch (mode)
        {
        case EditPoints:
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

    ID findPointAt(const Vector2d &pos, double maxDistance) const
    {
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

    void selectClickedObject()
    {
        const auto pos = worldActionPos();
        switch (mode)
        {
        case EditPoints:
            if (auto id = findPointAt(pos, 20 / viewScale))
            {
                selection.insert(id);
            }
            break;
        }
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

    // Actions.
    {
        QAction *add = new QAction;
        add->setShortcut(QKeySequence("Ctrl+D"));
        connect(add, &QAction::triggered, [this]() { d->userAdd(); });
        addAction(add);

        QAction *del = new QAction;
        del->setShortcut(QKeySequence("Ctrl+Backspace"));
        connect(del, &QAction::triggered, [this]() { d->userDelete(); });
        addAction(del);

        QAction *rotate = new QAction;
        rotate->setShortcut(QKeySequence("R"));
        connect(rotate, &QAction::triggered, [this]() { d->userRotate(); });
        addAction(rotate);

        QAction *scale = new QAction;
        scale->setShortcut(QKeySequence("S"));
        connect(scale, &QAction::triggered, [this]() { d->userScale(); });
        addAction(scale);
    }

    d->metaFont = font();
    d->metaFont.setPointSizeF(d->metaFont.pointSizeF() * .75);
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

    const QColor panelBg(0, 0, 0, 128);
    const QColor metaBg(255, 255, 255, 64);
    const QColor selectColor(0, 0, 255, 128);
    const QColor gridMajor{180, 180, 180, 255};
    const QColor gridMinor{220, 220, 220, 255};
    const QColor textColor(255, 255, 255, 255);
    const QColor metaColor(0, 0, 0, 92);
    const QColor pointColor(128, 0, 0, 255);

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
                const String label = String::number(id);
                ptr.drawText(pos + QPointF(-metaMetrics.width(label)/2, -gap), String::number(id));
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
}

void Editor::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    d->actionPos = event->pos();
}

void Editor::mouseMoveEvent(QMouseEvent *event)
{
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
        QPoint delta = event->pos() - d->actionPos;
        d->actionPos = event->pos();
        for (auto id : d->selection)
        {
            Vector2d worldDelta = Vector2d(delta.x(), delta.y()) / d->viewScale;
            d->map.points()[id] += worldDelta;
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

    if (d->userAction != Impl::None)
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
