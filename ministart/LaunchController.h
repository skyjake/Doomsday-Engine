/* LaunchController */

#import <Cocoa/Cocoa.h>

@interface LaunchController : NSObject
{
    IBOutlet NSPopUpButton *gameList;
    IBOutlet NSTextField *iwadField;
    IBOutlet NSTextField *optionsField;
    IBOutlet NSProgressIndicator *indicator;
    IBOutlet id launcher;
}

- (void)awakeFromNib;
- (void)updateFields;

/* Delegates */
- (void)applicationWillTerminate:(NSNotification *)aNotification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication*)theApplication;

/* Actions */
- (IBAction)browseWads:(id)sender;
- (IBAction)fieldChanged:(id)sender;
- (IBAction)gameChanged:(id)sender;
- (IBAction)launch:(id)sender;
@end
