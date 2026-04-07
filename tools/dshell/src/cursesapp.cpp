/** @file cursesapp.cpp Application based on curses for input and output.
 *
 * @authors Copyright © 2013-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "cursesapp.h"
#include "cursestextcanvas.h"
#include <de/clock.h>
#include <de/timer.h>
#include <de/error.h>
#include <de/animation.h>
#include <de/rule.h>
#include <de/vector.h>
#include <de/logbuffer.h>
#include <de/term/textrootwidget.h>
#include <de/term/widget.h>
#include <de/term/keyevent.h>
#include <curses.h>
#include <stdio.h>
#include <sstream>
#include <signal.h>

using namespace de;
using namespace de::term;

static void windowResized(int)
{
    ungetch(KEY_RESIZE);
}

static String runSystemCommand(const char *cmd)
{
    String result;
    FILE *p = popen(cmd, "r");
    for (;;)
    {
        int c = fgetc(p);
        if (c == EOF) break;
        result.append(char(c));
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
static Vec2ui actualTerminalSize(const Vec2ui &oldSize)
{
    Vec2ui size = oldSize;
    const auto result = runSystemCommand("stty size");
    std::istringstream is(result.toStdString());
    is >> size.y >> size.x;
    return size;
}

DE_PIMPL(CursesApp)
{
    LogBuffer logBuffer;
    Clock     clock;

    // Curses state:
    WINDOW *rootWin;
    Vec2ui  rootSize;
    int     unicodeContinuation = 0;

    TextRootWidget *rootWidget = nullptr;

    Impl(Public &i) : Base(i)
    {
        logBuffer.enableStandardOutput(false);
        logBuffer.setAutoFlushInterval(0.1);

        LogBuffer::setAppBuffer(logBuffer);
        Animation::setClock(&clock);
        Clock::setAppClock(&clock);

        initCurses();

        // Create the canvas.
        rootWidget = new TextRootWidget(new CursesTextCanvas(rootSize, rootWin));
        rootWidget->draw();
    }

    ~Impl()
    {
        delete rootWidget;

        shutdownCurses();

        Clock::setAppClock(nullptr);
        Animation::setClock(nullptr);
    }

    void initCurses()
    {
        // Initialize curses.
        if (!(rootWin = initscr()))
        {
            warning("Failed initializing curses!");
            exit(-1);
        }
        initCursesState();

        // Listen for window resizing.
        signal(SIGWINCH, windowResized);
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
        rootWin = nullptr;

        endwin();
        refresh();
    }

//    void requestRefresh()
//    {
//        Loop::timer(1.0 / 30.0, [this]() { refresh(); });
//    }

    void handleResize()
    {
        // Terminal has been resized.
        Vec2ui size = actualTerminalSize(rootSize);

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
        if (!rootWin) return;

        // Update time.
        clock.setTime(Time());

        // Poll for input.
        int key;
        while ((key = wgetch(rootWin)) != ERR)
        {
            if (key == KEY_RESIZE)
            {
                handleResize();
            }
            else if ((key & KEY_CODE_YES) || key < 0x20 || key == 0x7f)
            {
                Key code = Key::None;
                KeyEvent::Modifiers mods;

                // Control keys.
                switch (key)
                {
                case KEY_ENTER:
                case 0xd: // Enter
                    code = Key::Enter;
                    break;

                case 0x7f: // Delete
                    code = Key::Backspace;
                    break;

                case 0x3: // Ctrl-C
                    code = Key::Break; //Qt::Key_C;
                    //mods = KeyEvent::Control;
                    break;

                case KEY_DC:
                case 0x4: // Ctrl-D
                    code = Key::Delete;
                    break;

                case KEY_BACKSPACE:
                    code = Key::Backspace; //Qt::Key_Backspace;
                    break;

                case 0x9:
                    code = Key::Tab; //Qt::Key_Tab;
                    break;

                case KEY_BTAB: // back-tab
                    code = Key::Backtab;
                    break;

                case KEY_LEFT:
                    code = Key::Left;
                    break;

                case KEY_RIGHT:
                    code = Key::Right;
                    break;

                case KEY_UP:
                    code = Key::Up;
                    break;

                case KEY_DOWN:
                    code = Key::Down;
                    break;

                case KEY_HOME:
                case 0x1: // Ctrl-A
                    code = Key::Home;
                    break;

                case KEY_END:
                case 0x5: // Ctrl-E
                    code = Key::End;
                    break;

                case KEY_NPAGE:
                case 0x16: // Ctrl-V
                    code = Key::PageDown;
                    break;

                case KEY_PPAGE:
                case 0x19: // Ctrl-Y
                    code = Key::PageUp;
                    break;

                case 0xb:
                    code = Key::Kill;
                    //mods = KeyEvent::Control;
                    break;

                case KEY_F(1):
                    code = Key::F1;
                    break;

                case KEY_F(2):
                    code = Key::F2;
                    break;

                case KEY_F(3):
                    code = Key::F3;
                    break;

                case KEY_F(4):
                    code = Key::F4;
                    break;

                case KEY_F(5):
                    code = Key::F5;
                    break;

                case KEY_F(6):
                    code = Key::F6;
                    break;

                case KEY_F(7):
                    code = Key::F7;
                    break;

                case KEY_F(8):
                    code = Key::F8;
                    break;

                case KEY_F(9):
                    code = Key::F9;
                    break;

                case KEY_F(10):
                    code = Key::F10;
                    break;

                case KEY_F(11):
                    code = Key::F11;
                    break;

                case KEY_F(12):
                    code = Key::F12;
                    break;

                case 0x18:
                    code = Key::Cancel; //Qt::Key_X;
                    //mods = KeyEvent::Control;
                    break;

                case 0x1a:
                    code = Key::Substitute; //Qt::Key_Z;
                    //mods = KeyEvent::Control;
                    break;

                case 0x1b:
                    code = Key::Escape;
                    break;

                default:
#if defined (DE_DEBUG)
                    if (key & KEY_CODE_YES)
                        debug("CURSES 0x%x", key);
                    else
                        // This key code is ignored.
                        debug("0x%x", key);
#endif
                    break;
                }

                if (code != Key::None)
                {
                    rootWidget->processEvent(KeyEvent(code, mods));
                }
            }
            else
            {
                // Convert the key code(s) into a string.
                String keyStr;

                if (unicodeContinuation)
                {
                    char utf8[3] = { char(unicodeContinuation), char(key), 0 };
                    keyStr = String(utf8);
                    //qDebug() << QString("%1 %2, %3").arg(unicodeCont, 0, 16).arg(key, 0, 16)
                    //            .arg(keyStr[0].unicode(), 0, 16);
                    unicodeContinuation = 0;
                }
                else
                {
                    if ((key >= 0x80 && key <= 0xbf) || (key >= 0xc2 && key <= 0xf4))
                    {
                        unicodeContinuation = key;
                        continue;
                    }
                    else
                    {
                        keyStr.append(Char(uint32_t(key)));
                    }
                }

                rootWidget->processEvent(KeyEvent(keyStr));
            }
        }

        rootWidget->update();

        // Automatically redraw the UI if the values of layout rules have changed.
        if (Rule::invalidRulesExist() || rootWidget->drawWasRequested())
        {
            rootWidget->draw();
        }

        if (rootWidget->focus())
        {
            Vec2i p = rootWidget->focus()->cursorPosition();
            wmove(rootWin, p.y, p.x);
            wrefresh(rootWin);
        }
    }
};

CursesApp::CursesApp(int &argc, char **argv)
    : TextApp(makeList(argc, argv))
    , d(new Impl(*this))
{}

TextRootWidget &CursesApp::rootWidget()
{
    return *d->rootWidget;
}

int CursesApp::exec()
{
    loop().setRate(30.0);
    loop().audienceForIteration() += [this]() { refresh(); };
    return TextApp::exec();
}

void CursesApp::refresh()
{
    d->refresh();
}

void CursesApp::quit()
{
    TextApp::quit(0);
}
