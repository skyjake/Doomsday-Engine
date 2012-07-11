#import "Launcher.h"

@implementation Launcher

- (void)restore
{
    const char *raw[NUM_GAMES] = {
        "Doom (Shareware)",
        "Doom",
        "Ultimate Doom",
        "Doom 2",
        "Final Doom: Plutonia",
        "Final Doom: TNT",
        "Heretic (Shareware)",
        "Heretic",
        "Hexen 4-Level Demo",
        "Hexen",
        "Hexen: Deathkings"
    };
    int i;
    
    /* The game that was last played. */
    previousGame = 0;
    
    for(i = 0; i < NUM_GAMES; ++i)
    {
        config[i].name = [[NSString alloc] initWithCString:raw[i]];
        config[i].iwad = [[NSString alloc] init];
        config[i].options = [[NSString alloc] init];
    }        
    
    /* Try reading the settings file. */
    [self readConfig];
}

- (NSString*)configFileName
{
    return [NSHomeDirectory() 
        stringByAppendingPathComponent:@".doomsday/MiniStart.conf"];
}

- (void)readConfig
{
    if(![[NSFileManager defaultManager] fileExistsAtPath:
        [self configFileName]]) return;
    
    @try 
    {
        /* Unarchive the config settings array. */
        NSMutableArray *array = [NSUnarchiver unarchiveObjectWithFile:
            [self configFileName]];
        NSEnumerator *enumerator = [array objectEnumerator];
        
        NSNumber *prev = [enumerator nextObject];
        previousGame = [prev intValue];
        
        NSString *str;
        int i;
        
        for(i = 0; i < NUM_GAMES; ++i)
        {
            config[i].name = [[NSString alloc] 
            initWithString:[enumerator nextObject]];
            
            config[i].iwad = [[NSString alloc]
            initWithString:[enumerator nextObject]];
            
            config[i].options = [[NSString alloc] 
            initWithString:[enumerator nextObject]];
        }
    }
    @catch(NSException *exception)
    {
        /* Not really interested... */
    }
}

- (void)writeConfig
{   
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:NUM_GAMES*3];
    int i;
    
    /* Begin the array with the last played game. */
    [array addObject:[NSNumber numberWithInt:previousGame]];
    
    /* Put the configuration in an array. */
    for(i = 0; i < NUM_GAMES; ++i)
    {
        [array addObject:config[i].name];
        [array addObject:config[i].iwad];
        [array addObject:config[i].options];
    }        
    
    /* Archive the array to the config file. */
    [NSArchiver archiveRootObject:array toFile:[self configFileName]];
}

- (NSArray*)games
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:NUM_GAMES];
    int i;
    
    for(i = 0; i < NUM_GAMES; ++i)
        [array addObject:config[i].name];

    return array;
}

- (void)setPreviousGame:(int)index
{
    previousGame = index;
}

- (int)previousGame
{
    return previousGame;
}

- (NSString*)iwadOfIndex:(int)index
{
    return config[index].iwad;
}

- (NSString*)optionsOfIndex:(int)index
{
    return config[index].options;
}

- (void)setIwadOfIndex:(int)index to:(NSString*)iwad
{
    config[index].iwad = [[NSString alloc] initWithString:iwad];
}

- (void)setOptionsOfIndex:(int)index to:(NSString*)options
{
    config[index].options = [[NSString alloc] initWithString:options];
}

- (NSString*)appDirectory
{
    return [[NSBundle mainBundle] bundlePath];
}

- (void)startDoomsday:(int)game 
    iwad:(NSString*)iwadFileName options:(NSString*)options
{
    NSString *gameBundle = @"jdoom.bundle";
    
    if(game >= 6 && game < 8)
    {
        gameBundle = @"jheretic.bundle";
    }
    else if(game >= 8)
    {   
        gameBundle = @"jhexen.bundle";
    }

    /* Launch Doomsday with the provided arguments. */
    NSString *home = NSHomeDirectory();
    NSString *bundle = [self appDirectory];

    NSTask *aTask = [[NSTask alloc] init];
    NSMutableArray *args = [NSMutableArray array];

    /* Set arguments. */
    [args addObject:@"-game"];
    [args addObject:gameBundle];
    [args addObject:@"-basedir"];
    [args addObject:[bundle stringByAppendingPathComponent:
        @"Contents/Doomsday.app/Contents/Resources"]];
    [args addObject:@"-userdir"];
    [args addObject:[home stringByAppendingPathComponent:@".doomsday"]];
    [args addObject:@"-file"];
    [args addObject:iwadFileName];
    [args addObject:options];

    [aTask setCurrentDirectoryPath:[bundle stringByAppendingPathComponent:
        @"Contents/Doomsday.app/Contents"]];
    [aTask setLaunchPath:[bundle stringByAppendingPathComponent:
        @"Contents/Doomsday.app/Contents/MacOS/Doomsday"]];
    [aTask setArguments:args];

    /* Start the game. */
    [aTask launch];
    [aTask waitUntilExit];
}

@end
