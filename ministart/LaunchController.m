#import "LaunchController.h"
#import "Launcher.h"
#include <string.h>

@implementation LaunchController

- (void)awakeFromNib
{
    [NSApp setDelegate:self];

    /* It would probably be possible to construct the menu manually 
       from scratch, but this appears easier. */
    [gameList removeAllItems];
    [launcher restore];

    NSArray *games = [launcher games];
    NSEnumerator *enumerator = [games objectEnumerator];
    NSString *name;

    /* Add the names of all the games to the popup menu. */
    while(name = [enumerator nextObject])
        [gameList addItemWithTitle:name];

    [gameList selectItemAtIndex:[launcher previousGame]];

    [self updateFields];
}

- (void)updateFields
{
    /* Update the text fields based on the currently selected game. */
    int sel = [gameList indexOfSelectedItem];
    
    [iwadField setStringValue:[launcher iwadOfIndex:sel]];
    [optionsField setStringValue:[launcher optionsOfIndex:sel]];
}

- (IBAction)browseWads:(id)sender
{
    int result;
    NSArray *fileTypes = [NSArray arrayWithObject:@"wad"];
    NSOpenPanel *panel = [NSOpenPanel openPanel];

    [panel setAllowsMultipleSelection:NO];
    [panel setTitle:[@"Choose IWAD for " 
        stringByAppendingString:[[gameList selectedItem] title]]];
    [panel setPrompt:@"Choose"];
    result = [panel runModalForDirectory:NSHomeDirectory()
        file:nil types:fileTypes];
    if(result == NSOKButton) 
    {
        [iwadField setStringValue:[[panel filenames] objectAtIndex:0]];
        [self fieldChanged:iwadField];
    }
}

- (IBAction)gameChanged:(id)sender
{
    [launcher setPreviousGame:[gameList indexOfSelectedItem]];
    [self updateFields];
}

- (IBAction)fieldChanged:(id)sender
{
    NSString *newContents = [sender stringValue];
    int sel = [gameList indexOfSelectedItem];
    
    if(sender == iwadField)
        [launcher setIwadOfIndex:sel to:newContents];
    else
        [launcher setOptionsOfIndex:sel to:newContents];
}

- (IBAction)launch:(id)sender
{
    /* Make sure the fields get saved. */
    [self fieldChanged:iwadField];
    [self fieldChanged:optionsField];
    [launcher writeConfig];

    /* Get the values the user has selected and make sure they appear OK. */
    [indicator startAnimation:sender];
    [launcher startDoomsday:[gameList indexOfSelectedItem]
        iwad:[iwadField stringValue] options:[optionsField stringValue]];
    [indicator stopAnimation:sender];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    [launcher writeConfig];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication*)theApplication
{
    return YES;
}

@end
