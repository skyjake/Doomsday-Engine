/*
 * The Doomsday Engine Project -- Supporting code
 *
 * Copyright (c) 2006-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 * Copyright (c) Max Horn <max@quendi.de>
 * Copyright (c) Darrell Walisser <dwaliss1@purdue.edu>
 *
 * License: LGPL
 *
 * This file defines the entry point for a Cocoa SDL app. Once the app is
 * initialized and running, it hands control to deng_Main().
 */

#include <stdint.h>
//typedef uint64_t io_user_reference_t; 

#include <dengmain.h>

#import <SDL.h>
#import <Cocoa/Cocoa.h>

@interface SDLMain : NSObject
@end

// Argument passed to the main() function.
static int mainArgc;
static char** mainArgv;

// Return value from deng_Main().
static int dengMainResult;

/**
 * An internal Apple class used to setup Apple menus.
 */
@interface NSAppleMenuController:NSObject {}
- (void)controlMenu:(NSMenu *)aMenu;
@end

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication
/**
 * Invoked from the Quit menu item.
 */
- (void)terminate:(id)sender
{
    /* Post a SDL_QUIT event */
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}
@end

/**
 * The main class of the application, the application's delegate.
 */
@implementation SDLMain

void setupAppleMenu(void)
{
    /* warning: this code is very odd */
    NSAppleMenuController *appleMenuController;
    NSMenu *appleMenu;
    NSMenuItem *appleMenuItem;

    appleMenuController = [[NSAppleMenuController alloc] init];
    appleMenu = [[NSMenu alloc] initWithTitle:@""];
    appleMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    
    [appleMenuItem setSubmenu:appleMenu];

    /* yes, we do need to add it and then remove it --
       if you don't add it, it doesn't get displayed
       if you don't remove it, you have an extra, titleless item in the menubar
       when you remove it, it appears to stick around
       very, very odd */
    [[NSApp mainMenu] addItem:appleMenuItem];
    [appleMenuController controlMenu:appleMenu];
    [[NSApp mainMenu] removeItem:appleMenuItem];
    [appleMenu release];
    [appleMenuItem release];
}

void setupWindowMenu(void)
{
    NSMenu      *windowMenu;
    NSMenuItem  *windowMenuItem;
    NSMenuItem  *menuItem;

    windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    
    // "Minimize" item.
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" 
        action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItem:menuItem];
    [menuItem release];
    
    // Put menu into the menubar.
    windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    [windowMenuItem setSubmenu:windowMenu];
    [[NSApp mainMenu] addItem:windowMenuItem];
    
    // Tell the application object that this is now the window menu.
    [NSApp setWindowsMenu:windowMenu];

    // Finally give up our references to the objects.
    [windowMenu release];
    [windowMenuItem release];
}

/**
 * Called when the internal OS X application event loop has just started running.
 * The entire run of Doomsday happens within this function.
 */
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
    dengMainResult = deng_Main(mainArgc, mainArgv);

    // Stop the event loop.
    exit(dengMainResult);
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *)sender
{
    return YES;
}

@end // SDLMain

#ifdef main
#   undef main
#endif

/**
 * The real entry point of the OS X executable.
 */
int main(int argc, char *argv[])
{
    NSAutoreleasePool   *pool = [[NSAutoreleasePool alloc] init];
    SDLMain             *sdlMain;

    mainArgc = argc;
    mainArgv = argv;

    // Ensure the application object is initialised.
    [SDLApplication sharedApplication];
    
    // Set up the menubar.
    [NSApp setMainMenu:[[NSMenu alloc] init]];
    setupAppleMenu();
    setupWindowMenu();
    
    // Create SDLMain and make it the app delegate.
    sdlMain = [[SDLMain alloc] init];
    [NSApp setDelegate:sdlMain];
    
    // Start the main event loop.
    [NSApp run];
        
    // Won't reach here.
    [sdlMain release];
    [pool release];

    return 0;
}
