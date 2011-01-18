/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2010, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "testapp.h"
#include <de/LogBuffer>
#include <QDebug>

using namespace de;

TestApp::TestApp(int argc, char** argv)
    : de::ConsoleApp(argc, argv, "/config/testapp.de", "testapp", de::Log::DEBUG)
{
    logBuffer().enableStandardOutput();
    LOG_MESSAGE("TestApp constructed.");
}

TestApp::~TestApp()
{
    LOG_MESSAGE("TestApp destroyed.");
}

void TestApp::iterate(const de::Time::Delta& elapsed)
{
    LOG_AS("TestApp::iterate");
    LOG_VERBOSE("%f sec elapsed.") << elapsed;
}
