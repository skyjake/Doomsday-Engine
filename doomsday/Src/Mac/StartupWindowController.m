#import "StartupWindowController.h"

@implementation StartupWindowController

- (void)awakeFromNib
{
    [window setAutodisplay:YES];

    // This startup window is used by the engine.
    extern StartupWindowController* gStartupWindowController;
    gStartupWindowController = self;

    // Print something.
    NSTextStorage* storage = [startupText textStorage];

    NSAttributedString* str = [[NSAttributedString alloc]
        initWithString:@"Doomsday Engine launching...\n"];

    [storage appendAttributedString:str];   
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
