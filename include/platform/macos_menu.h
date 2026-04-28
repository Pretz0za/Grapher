#ifndef _GVIZ_PLATFORM_MACOS_MENU_H_
#define _GVIZ_PLATFORM_MACOS_MENU_H_

/*
 * macOS Cocoa application menu bridge.
 *
 * Call gvizPlatformMenuInit() once after the GL window has been created
 * (raylib's InitWindow). It installs an NSMenu with a File menu that
 * contains an "Open .obj…" entry mapped to Cmd+O.
 *
 * When the user picks a file, the registered callback is fired from the
 * main (AppKit) thread. Because raylib's main loop runs on the same
 * thread, the callback should NOT mutate scene state directly — push
 * the path onto a queue and drain it from the loop. See
 * gvizPlatformMenuPollPendingOBJPath.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gvizPlatformOpenOBJCallback)(const char *path, void *userdata);

/* Install the application menu. Safe to call multiple times (no-op after
 * first). Returns 0 on success, -1 on failure. */
int gvizPlatformMenuInit(void);

/* Register the callback fired when the user picks a .obj file. Replaces
 * any previously registered callback. */
void gvizPlatformMenuOnOpenOBJ(gvizPlatformOpenOBJCallback cb, void *userdata);

/*
 * Convenience pull-style API for callers that don't want to register a
 * callback. Returns the most recently picked .obj path (heap-allocated,
 * caller frees with gvizPlatformMenuFreePath) and clears the queued slot.
 * Returns NULL if no path is pending.
 */
char *gvizPlatformMenuPollPendingOBJPath(void);
void  gvizPlatformMenuFreePath(char *path);

#ifdef __cplusplus
}
#endif

#endif
