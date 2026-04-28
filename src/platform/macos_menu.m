#import <Cocoa/Cocoa.h>
#include "platform/macos_menu.h"
#include <stdlib.h>
#include <string.h>

static int g_installed = 0;
static gvizPlatformOpenOBJCallback g_cb = NULL;
static void *g_cb_userdata = NULL;
static char *g_pendingPath = NULL;

@interface GVizMenuTarget : NSObject
- (void)openOBJ:(id)sender;
@end

@implementation GVizMenuTarget
- (void)openOBJ:(id)sender {
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setAllowedFileTypes:@[@"obj", @"OBJ"]];
    [panel setTitle:@"Open OBJ Mesh"];

    if ([panel runModal] != NSModalResponseOK) return;
    NSURL *url = [[panel URLs] firstObject];
    if (!url) return;
    const char *posix = [[url path] fileSystemRepresentation];
    if (!posix) return;

    /* Replace the most recently queued path. */
    if (g_pendingPath) { free(g_pendingPath); g_pendingPath = NULL; }
    size_t n = strlen(posix);
    g_pendingPath = (char *)malloc(n + 1);
    if (g_pendingPath) memcpy(g_pendingPath, posix, n + 1);

    if (g_cb && g_pendingPath) g_cb(g_pendingPath, g_cb_userdata);
}
@end

static GVizMenuTarget *g_target = nil;

int gvizPlatformMenuInit(void) {
    if (g_installed) return 0;
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        if (!app) return -1;

        g_target = [[GVizMenuTarget alloc] init];

        NSMenu *mainMenu = [[NSMenu alloc] init];

        /* App menu (first item is special — its title is shown as the app name). */
        NSMenuItem *appItem = [[NSMenuItem alloc] init];
        [mainMenu addItem:appItem];
        NSMenu *appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"Quit Grapher"
                           action:@selector(terminate:)
                    keyEquivalent:@"q"];
        [appItem setSubmenu:appMenu];

        /* File menu */
        NSMenuItem *fileItem = [[NSMenuItem alloc] init];
        [mainMenu addItem:fileItem];
        NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
        NSMenuItem *openItem = [[NSMenuItem alloc] initWithTitle:@"Open .obj…"
                                                          action:@selector(openOBJ:)
                                                   keyEquivalent:@"o"];
        [openItem setTarget:g_target];
        [fileMenu addItem:openItem];
        [fileItem setSubmenu:fileMenu];

        [app setMainMenu:mainMenu];
    }
    g_installed = 1;
    return 0;
}

void gvizPlatformMenuOnOpenOBJ(gvizPlatformOpenOBJCallback cb, void *userdata) {
    g_cb = cb;
    g_cb_userdata = userdata;
}

char *gvizPlatformMenuPollPendingOBJPath(void) {
    char *p = g_pendingPath;
    g_pendingPath = NULL;
    return p;
}

void gvizPlatformMenuFreePath(char *path) {
    if (path) free(path);
}
