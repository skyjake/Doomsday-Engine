/* Launcher */

#import <Cocoa/Cocoa.h>

typedef struct gamedesc_s {
    NSString *name;
    NSString *iwad;
    NSString *options;
} gamedesc_t;

#define NUM_GAMES 11

@interface Launcher : NSObject
{
    gamedesc_t config[NUM_GAMES];
    int previousGame;
}
- (void)restore;
- (void)readConfig;
- (void)writeConfig;
- (NSArray*)games;
- (void)setPreviousGame:(int)index;
- (int)previousGame;
- (NSString*)iwadOfIndex:(int)index;
- (NSString*)optionsOfIndex:(int)index;
- (void)setIwadOfIndex:(int)index to:(NSString*)iwad;
- (void)setOptionsOfIndex:(int)index to:(NSString*)options;
- (NSString*)appDirectory;
- (NSString*)configFileName;
- (void)startDoomsday:(int)game 
    iwad:(NSString*)iwadFileName options:(NSString*)options;
@end
