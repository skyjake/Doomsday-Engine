// Precompiled header for the Doom game library (jDoom).
// Scope: STL/C standard library only. Game libs use apps/api/ headers with
// DE_USING_API() macros that are incompatible with PCH.
#pragma once
#ifdef __cplusplus
#  include <cctype>
#  include <cerrno>
#  include <cmath>
#  include <cstdio>
#  include <cstdlib>
#  include <cstring>
#else
#  include <ctype.h>
#  include <errno.h>
#  include <math.h>
#  include <stdio.h>
#  include <stdlib.h>
#  include <string.h>
#endif
