/** @file cursesapp.cpp Application based on curses for input and output.
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

#include <QTimer>
#include <QProcess>
#include <QTextStream>
#include <QDebug>
#include "cursesapp.h"
#include "cursestextcanvas.h"
#include "keyevent.h"
#include "textrootwidget.h"
#include "textwidget.h"
#include <curses.h>
#include <stdio.h>
#include <de/Clock>
#include <de/Animation>
#include <de/Rule>
#include <de/Vector>

static void windowResized(int)
{
    ungetch(KEY_RESIZE);
}

/**
 * Determines the actual current size of the terminal.
 *
 * @return Terminal columns and rows.
 */
static de::Vector2i actualTerminalSize()
{
    QByteArray result;
    FILE *p = popen("stty size", "r");
    forever
    {
        int c = fgetc(p);
        if(c == EOF) break;
        result.append(c);
    }
    pclose(p);

    QTextStream is(result);
    de::Vector2i size;
    is >> size.y >> size.x;
    return size;
}

struct CursesApp::Instance
{
    CursesApp &self;
    de::Clock clock;

    // Curses state:
    WINDOW *rootWin;
    de::Vector2i rootSize;
    QTimer *refreshTimer;
    int unicodeContinuation;

    TextRootWidget *rootWidget;

    Instance(CursesApp &a) : self(a), unicodeContinuation(0), rootWidget(0)
    {
        de::Animation::setClock(&clock);
        de::Clock::setAppClock(&clock);

        initCurses();

        // Create the canvas.
        rootWidget = new TextRootWidget(new CursesTextCanvas(rootSize, rootWin));
        rootWidget->draw();
    }

    ~Instance()
    {
        delete rootWidget;

        shutdownCurses();

        de::Clock::setAppClock(0);
        de::Animation::setClock(0);
    }

    void initCurses()
    {
        // Initialize curses.
        if(!(rootWin = initscr()))
        {
            qFatal("Failed initializing curses.");
            return;
        }
        initCursesState();

        // Listen for window resizing.
        signal(SIGWINCH, windowResized);

        refreshTimer = new QTimer(&self);
        QObject::connect(refreshTimer, SIGNAL(timeout()), &self, SLOT(refresh()));
        refreshTimer->start(1000 / 30);
    }

    void initCursesState()
    {
        // The current size of the screen.
        getmaxyx(stdscr, rootSize.y, rootSize.x);

        scrollok(rootWin, FALSE);
        wclear(rootWin);

        // Initialize input.
        cbreak();
        noecho();
        nonl();
        raw(); // Ctrl-C shouldn't cause a signal
        nodelay(rootWin, TRUE);
        keypad(rootWin, TRUE);
    }

    void shutdownCurses()
    {
        delete refreshTimer;
        refreshTimer = 0;

        delwin(rootWin);
        rootWin = 0;

        endwin();
        refresh();
    }

    void refresh()
    {
        if(!rootWin) return;

        // Update time.
        clock.setTime(de::Time());

        // Poll for input.
        int key;
        while((key = wgetch(rootWin)) != ERR)
        {
            if(key == KEY_RESIZE)
            {
                // Terminal has been resized.
                de::Vector2i size = actualTerminalSize();

                // Curses needs to resize its buffers.
                wclear(rootWin);
                resize_term(size.y, size.x);

                // The root widget will update the UI.
                rootWidget->setViewSize(size);

                // We must redraw all characters since wclear was called.
                rootWidget->rootCanvas().markDirty();
            }
            else if((key & KEY_CODE_YES) || key < 0x20 || key == 0x7f)
            {
                int code = 0;
                KeyEvent::Modifiers mods;

                // Control keys.
                switch(key)
                {
                case 0x1b: // Escape
                    self.quit(); // development only
                    return;

                case KEY_ENTER:
                case 0xd: // Enter
                    code = Qt::Key_Enter;
                    break;

                case 0x7f: // Delete
                    code = Qt::Key_Backspace;
                    break;

                case KEY_DC:
                case 0x4: // Ctrl-D
                    code = Qt::Key_Delete;
                    break;

                case KEY_BACKSPACE:
                    code = Qt::Key_Backspace;
                    break;

                case KEY_BTAB: // back-tab
                    code = Qt::Key_Backtab;
                    break;

                case KEY_LEFT:
                    code = Qt::Key_Left;
                    break;

                case KEY_RIGHT:
                    code = Qt::Key_Right;
                    break;

                case KEY_UP:
                    code = Qt::Key_Up;
                    break;

                case KEY_DOWN:
                    code = Qt::Key_Down;
                    break;

                case KEY_HOME:
                case 0x1: // Ctrl-A
                    code = Qt::Key_Home;
                    break;

                case KEY_END:
                case 0x5: // Ctrl-E
                    code = Qt::Key_End;
                    break;

                case 0xb: // Ctrl-K
                    code = Qt::Key_K;
                    mods = KeyEvent::Control;
                    break;

                default:
                    if(key & KEY_CODE_YES)
                        qDebug() << "CURSES" << QString("0%1").arg(key, 0, 8).toAscii().constData();
                    else
                        // This key code is ignored.
                        qDebug() << QString("%1").arg(key, 0, 16);
                    break;
                }

                if(code)
                {
                    KeyEvent ev(code, mods);
                    rootWidget->processEvent(&ev);
                }
            }
            else
            {
                // Convert the key code(s) into a string.
                de::String keyStr;

                if(unicodeContinuation)
                {
                    char utf8[3] = { char(unicodeContinuation), char(key), 0 };
                    keyStr = de::String(utf8);
                    //qDebug() << QString("%1 %2, %3").arg(unicodeCont, 0, 16).arg(key, 0, 16)
                    //            .arg(keyStr[0].unicode(), 0, 16);
                    unicodeContinuation = 0;
                }
                else
                {
                    if((key >= 0x80 && key <= 0xbf) || (key >= 0xc2 && key <= 0xf4))
                    {
                        unicodeContinuation = key;
                        continue;
                    }
                    else
                    {
                        keyStr.append(QChar(key));
                    }
                }

                KeyEvent ev(keyStr);
                rootWidget->processEvent(&ev);
            }
        }

        // Automatically redraw the UI if the values of layout rules have changed.
        if(de::Rule::invalidRulesExist() || rootWidget->drawWasRequested())
        {
            rootWidget->draw();
        }

        if(rootWidget->focus())
        {
            de::Vector2i p = rootWidget->focus()->cursorPosition();
            wmove(rootWin, p.y, p.x);
            wrefresh(rootWin);
        }
    }

    void update()
    {
        rootWidget->draw();
    }
};

CursesApp::CursesApp(int &argc, char **argv)
    : QCoreApplication(argc, argv), d(new Instance(*this))
{
}

CursesApp::~CursesApp()
{
    delete d;
}

TextRootWidget &CursesApp::rootWidget()
{
    return *d->rootWidget;
}

void CursesApp::refresh()
{
    d->refresh();
}
