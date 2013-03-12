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
#include <curses.h>
#include <stdio.h>
#include <signal.h>
#include <de/Clock>
#include <de/Error>
#include <de/Animation>
#include <de/Rule>
#include <de/Vector>
#include <de/LogBuffer>
#include <de/shell/TextRootWidget>
#include <de/shell/TextWidget>
#include <de/shell/KeyEvent>

using namespace de::shell;

static void windowResized(int)
{
    ungetch(KEY_RESIZE);
}

static QByteArray runSystemCommand(char const *cmd)
{
    QByteArray result;
    FILE *p = popen(cmd, "r");
    forever
    {
        int c = fgetc(p);
        if(c == EOF) break;
        result.append(c);
    }
    pclose(p);
    return result;
}

/**
 * Determines the actual current size of the terminal.
 *
 * @param oldSize  Old size of the terminal, used as the default value
 *                 in case we can't determine the current size.
 *
 * @return Terminal columns and rows.
 */
static de::Vector2i actualTerminalSize(de::Vector2i const &oldSize)
{
    de::Vector2i size = oldSize;
    QByteArray result = runSystemCommand("stty size");
    QTextStream is(result);
    is >> size.y >> size.x;
    return size;
}

DENG2_PIMPL(CursesApp)
{
    de::LogBuffer logBuffer;
    de::Clock clock;

    // Curses state:
    WINDOW *rootWin;
    de::Vector2i rootSize;
    int unicodeContinuation;

    TextRootWidget *rootWidget;

    Instance(Public &i) : Base(i), unicodeContinuation(0), rootWidget(0)
    {
        logBuffer.enableStandardOutput(false);

        de::LogBuffer::setAppBuffer(logBuffer);
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

        requestRefresh();
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
        delwin(rootWin);
        rootWin = 0;

        endwin();
        refresh();
    }

    void requestRefresh()
    {
        QTimer::singleShot(1000 / 30, &self, SLOT(refresh()));
    }

    void handleResize()
    {
        // Terminal has been resized.
        de::Vector2i size = actualTerminalSize(rootSize);

        // Curses needs to resize its buffers.
        werase(rootWin);
        resize_term(size.y, size.x);

        // The root widget will update the UI.
        rootWidget->setViewSize(size);
        rootSize = size;

        // We must redraw all characters since wclear was called.
        rootWidget->rootCanvas().markDirty();
    }

    void refresh()
    {
        if(!rootWin) return;

        // Schedule the next refresh.
        requestRefresh();

        // Update time.
        clock.setTime(de::Time());

        // Poll for input.
        int key;
        while((key = wgetch(rootWin)) != ERR)
        {
            if(key == KEY_RESIZE)
            {
                handleResize();
            }
            else if((key & KEY_CODE_YES) || key < 0x20 || key == 0x7f)
            {
                int code = 0;
                KeyEvent::Modifiers mods;

                // Control keys.
                switch(key)
                {
                case KEY_ENTER:
                case 0xd: // Enter
                    code = Qt::Key_Enter;
                    break;

                case 0x7f: // Delete
                    code = Qt::Key_Backspace;
                    break;

                case 0x3: // Ctrl-C
                    code = Qt::Key_C;
                    mods = KeyEvent::Control;
                    break;

                case KEY_DC:
                case 0x4: // Ctrl-D
                    code = Qt::Key_Delete;
                    break;

                case KEY_BACKSPACE:
                    code = Qt::Key_Backspace;
                    break;

                case 0x9:
                    code = Qt::Key_Tab;
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

                case KEY_NPAGE:
                case 0x16: // Ctrl-V
                    code = Qt::Key_PageDown;
                    break;

                case KEY_PPAGE:
                case 0x19: // Ctrl-Y
                    code = Qt::Key_PageUp;
                    break;

                case 0xb:
                    code = Qt::Key_K;
                    mods = KeyEvent::Control;
                    break;

                case KEY_F(1):
                    code = Qt::Key_F1;
                    break;

                case KEY_F(2):
                    code = Qt::Key_F2;
                    break;

                case KEY_F(3):
                    code = Qt::Key_F3;
                    break;

                case KEY_F(4):
                    code = Qt::Key_F4;
                    break;

                case KEY_F(5):
                    code = Qt::Key_F5;
                    break;

                case KEY_F(6):
                    code = Qt::Key_F6;
                    break;

                case KEY_F(7):
                    code = Qt::Key_F7;
                    break;

                case KEY_F(8):
                    code = Qt::Key_F8;
                    break;

                case KEY_F(9):
                    code = Qt::Key_F9;
                    break;

                case KEY_F(10):
                    code = Qt::Key_F10;
                    break;

                case KEY_F(11):
                    code = Qt::Key_F11;
                    break;

                case KEY_F(12):
                    code = Qt::Key_F12;
                    break;

                case 0x18:
                    code = Qt::Key_X;
                    mods = KeyEvent::Control;
                    break;

                case 0x1a:
                    code = Qt::Key_Z;
                    mods = KeyEvent::Control;
                    break;

                case 0x1b:
                    code = Qt::Key_Escape;
                    break;

                default:
#ifdef _DEBUG
                    if(key & KEY_CODE_YES)
                        qDebug() << "CURSES" << QString("0%1").arg(key, 0, 8).toLatin1().constData();
                    else
                        // This key code is ignored.
                        qDebug() << QString("%1").arg(key, 0, 16);
#endif
                    break;
                }

                if(code)
                {
                    rootWidget->processEvent(KeyEvent(code, mods));
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

                rootWidget->processEvent(KeyEvent(keyStr));
            }
        }

        rootWidget->update();

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
};

CursesApp::CursesApp(int &argc, char **argv)
    : QCoreApplication(argc, argv), d(new Instance(*this))
{}

bool CursesApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QCoreApplication::notify(receiver, event);
    }
    catch(de::Error const &er)
    {
        qDebug() << "Caught exception:" << er.asText().toLatin1().constData();
    }
    return false;
}

TextRootWidget &CursesApp::rootWidget()
{
    return *d->rootWidget;
}

void CursesApp::refresh()
{
    d->refresh();
}
