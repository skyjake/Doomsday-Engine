/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifdef MACOS_10_4
#include <stdint.h>
typedef uint64_t      io_user_reference_t; 
#endif

#import "../include/StartupWindowController.h"

@implementation StartupWindowController

- (void)awakeFromNib
{
    /*
    [window setAutodisplay:YES];

    // This startup window is used by the engine.
    extern StartupWindowController* gStartupWindowController;
    gStartupWindowController = self;

    // Print something.
    NSTextStorage* storage = [startupText textStorage];

    NSAttributedString* str = [[NSAttributedString alloc]
        initWithString:@"Doomsday Engine launching...\n"];

    [storage appendAttributedString:str];   
     */
}

- (void)print:(const char *)message
{
    NSTextStorage* storage = [startupText textStorage];

    NSString* messageString = [[NSString alloc] initWithCString:message
        /*encoding:NSASCIIStringEncoding*/];

    NSAttributedString* str = [[[NSAttributedString alloc]
        initWithString:messageString] autorelease];
        
    // Where is the scroller?
    float pos = [[[startupText enclosingScrollView] verticalScroller] floatValue];
    
    [storage appendAttributedString:str];   
    
    if(pos > .9)
    {
        // Stay at the bottom.
        [startupText scrollRangeToVisible: 
            NSMakeRange([[startupText string] length], 0)];
    }
    
    // A bit crude but does the job. (The event loop is blocked by SDL...)

    // TODO: Make a separate thread and invoke display from there.
    // Only actually does display when something has been printed.
    // This'll make startup faster. The thread can be stopped when startup
    // is over.
    
    [startupText display];
}

@end
