// Precompiled header for libs/doomsday.
// Scope: STL, system, the_Foundation, libcore only.
// libdoomsday defines the doomsday/* API itself, so those headers are excluded.
#pragma once

// C++ standard library
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <utility>

// libcore public API
#include <de/app.h>
#include <de/c_wrapper.h>
#include <de/error.h>
#include <de/keymap.h>
#include <de/legacy/memory.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/vector1.h>
#include <de/list.h>
#include <de/log.h>
#include <de/logbuffer.h>
#include <de/nativepath.h>
#include <de/packageloader.h>
#include <de/reader.h>
#include <de/record.h>
#include <de/recordvalue.h>
#include <de/string.h>
#include <de/textvalue.h>
