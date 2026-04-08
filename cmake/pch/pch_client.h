// Precompiled header for apps/client.
// Scope: STL, system, the_Foundation, libcore, libgui.
// The apps/api/ headers use DE_NO_API_MACROS_* guards and DE_USING_API()
// macros that make them unsuitable for PCH.
#pragma once

// C++ standard library
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// libcore public API
#include <de/app.h>
#include <de/c_wrapper.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/dscript.h>
#include <de/error.h>
#include <de/extension.h>
#include <de/filesystem.h>
#include <de/hash.h>
#include <de/keyevent.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/timer.h>
#include <de/legacy/vector1.h>
#include <de/list.h>
#include <de/log.h>
#include <de/logbuffer.h>
#include <de/loop.h>
#include <de/nativefile.h>
#include <de/nativepath.h>
#include <de/packageloader.h>
#include <de/regexp.h>
#include <de/set.h>
#include <de/textvalue.h>
#include <de/timer.h>

// libgui public API
#include <de/callbackaction.h>
#include <de/charsymbols.h>
#include <de/drawable.h>
#include <de/glinfo.h>
#include <de/glstate.h>
#include <de/labelwidget.h>
#include <de/popupmenuwidget.h>
#include <de/progresswidget.h>
#include <de/sequentiallayout.h>
#include <de/ui/subwidgetitem.h>
#include <de/variabletogglewidget.h>
