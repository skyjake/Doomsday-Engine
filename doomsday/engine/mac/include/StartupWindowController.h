/* StartupWindowController */

#import <Cocoa/Cocoa.h>

@interface StartupWindowController : NSObject
{
    IBOutlet NSTextView *startupText;
    IBOutlet NSWindow *window;
}

- (void)awakeFromNib;
- (void)print:(const char *)message;

@end
