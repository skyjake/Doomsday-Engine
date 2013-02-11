/** @file qtrootwidget.h  Root widget that works with a Qt canvas.
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

#ifndef QTROOTWIDGET_H
#define QTROOTWIDGET_H

#include <QWidget>
#include <de/shell/TextRootWidget>

class QtTextCanvas;

/**
 * Root widget that works with a Qt canvas.
 *
 * QtRootWidget owns a TextRootWidget; any received input events will be
 * passed to the de::Widgets in the tree.
 */
class QtRootWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QtRootWidget(QWidget *parent = 0);
    ~QtRootWidget();

    de::shell::TextRootWidget &rootWidget();

    QtTextCanvas &canvas();

    /**
     * Sets the font to use on the canvas. The size of the font determines the
     * number of character size.
     *
     * @param font  Font.
     */
    void setFont(QFont const &font);

    void setOverlaidMessage(QString const &msg = "");

    void keyPressEvent(QKeyEvent *ev);
    void resizeEvent(QResizeEvent *ev);
    void paintEvent(QPaintEvent *ev);

protected slots:
    void blink();
    void cursorBlink();

private:
    struct Instance;
    Instance *d;
};

#endif // QTROOTWIDGET_H
