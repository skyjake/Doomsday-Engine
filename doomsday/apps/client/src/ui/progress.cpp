/** @file progress.cpp  Simple wrapper for the Busy progress bar.
 * @ingroup console
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "ui/progress.h"
#include "ui/clientwindow.h"
#include "ui/widgets/busywidget.h"
#include <de/progresswidget.h>

using namespace de;

// Time for the progress to reach the new target.
static constexpr TimeSpan PROGRESS_DELTA_TIME = 500_ms;

static ProgressWidget &progress()
{
    return ClientWindow::main().busy().progress();
}

void Con_InitProgress2(int maxProgress, float start, float end)
{
    progress().setRange(Rangei(0, maxProgress), Rangef(start, end));
    progress().setProgress(0, 0.0);
}

void Con_InitProgress(int maxProgress)
{
    Con_InitProgress2(maxProgress, 0.f, 1.f);
}

dd_bool Con_IsProgressAnimationCompleted(void)
{
    return progress().isHidden() || !progress().isAnimating();
}

void Con_SetProgress(int progressValue)
{
     progress().setProgress(progressValue, progressValue < progress().range().end?
                                PROGRESS_DELTA_TIME : TimeSpan(PROGRESS_DELTA_TIME / 2));
}
