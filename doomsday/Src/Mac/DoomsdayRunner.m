#import "SDL.h"
#import "DoomsdayRunner.h"

@implementation DoomsdayRunner

+ (void) threadEntryPoint:(id)anObject
{
    extern int gArgc;
    extern char **gArgv;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Hand off to main application code.
    int status = SDL_main(gArgc, gArgv);

    /* We're done, thank you for playing */
    exit(status);
    
    [pool release];
}

@end
