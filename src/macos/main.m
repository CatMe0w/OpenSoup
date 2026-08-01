// macOS host: transparent overlay with per-pixel click-through (Metal).
// Policy lives in opensoup.c; this file translates AppKit events.
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "app_assets.h"
#include "host_dialog.h"
#include "opensoup.h"
#include "scene.h"

static NSWindow* window;
static MTKView* view;
static id<MTLDevice> mtl_device;
static id view_delegate; // MTKView.delegate is weak, keep it alive here
static const char* assets_root;
static CFTimeInterval last_frame_time;
static NSString* const app_name = @"OpenSoup";

static NSString* opensoup_folder_path(void) {
    NSString* assets = [NSString stringWithUTF8String:assets_root];
    return [assets stringByDeletingLastPathComponent];
}

static void open_opensoup_folder(void) {
    [[NSWorkspace sharedWorkspace]
        openURL:[NSURL fileURLWithPath:opensoup_folder_path()
                         isDirectory:YES]];
}

@interface AppMenuActions : NSObject
- (void)openOpenSoupFolder:(id)sender;
- (void)saveDiagnosticDump:(id)sender;
@end

@implementation AppMenuActions
- (void)openOpenSoupFolder:(id)sender {
    (void)sender;
    open_opensoup_folder();
}
- (void)saveDiagnosticDump:(id)sender {
    (void)sender;
    opensoup_diagnostics_request();
}
@end

static AppMenuActions* app_menu_actions;

static void prepare_modal_ui(void) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}

static void show_quit_alert(NSString* message, NSString* information) {
    prepare_modal_ui();
    NSAlert* alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = message;
    alert.informativeText = information;
    [alert addButtonWithTitle:@"Open Folder and Quit"];
    [alert addButtonWithTitle:@"Quit"];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        open_opensoup_folder();
    }
}

static void show_missing_asset_alert(void) {
    NSString* root = [NSString stringWithUTF8String:assets_root];
    show_quit_alert(@"Game assets not found",
        [NSString stringWithFormat:
            @"OpenSoup could not find game assets at:\n\n%@", root]);
}

static bool show_installer_picker(void) {
    prepare_modal_ui();
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.title = @"Select the original Souptoys installer";
    panel.message = @"Select the original Souptoys installer (.exe) to set up OpenSoup's game assets.";
    panel.prompt = @"Select Installer";
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = @[@"exe"];
#pragma clang diagnostic pop

    if ([panel runModal] != NSModalResponseOK) {
        return false;
    }

    const char* path = panel.URL.fileSystemRepresentation;
    char error[1024] = {0};
    if (app_assets_install_from_installer(
            path, assets_root, error, sizeof error)) {
        return true;
    }
    show_quit_alert(@"Game asset installation failed",
                    [NSString stringWithUTF8String:error]);
    return false;
}

static bool run_open_panel(const char* title, const char* directory,
                           const char* kind, const char* extension,
                           char* out, size_t out_size, void* user) {
    (void)user;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title = [NSString stringWithUTF8String:title];
        panel.message = [NSString stringWithFormat:@"%s files (*.%s)",
                                                   kind, extension];
        panel.prompt = @"Open";
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        if (directory) {
            panel.directoryURL = [NSURL fileURLWithPath:
                [NSString stringWithUTF8String:directory] isDirectory:YES];
        }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        panel.allowedFileTypes = @[[NSString stringWithUTF8String:extension]];
#pragma clang diagnostic pop

        if ([panel runModal] != NSModalResponseOK) {
            return false;
        }
        const char* path = panel.URL.fileSystemRepresentation;
        if (!path || strlen(path) >= out_size) {
            return false;
        }
        snprintf(out, out_size, "%s", path);
        return true;
    }
}

static NSRect visible_scene_frame(void) {
    NSScreen* screen = window.screen ?: NSScreen.mainScreen;
    return screen.visibleFrame;
}

static NSInteger preferred_fps_for_current_screen(void) {
    NSScreen* screen = window.screen ?: NSScreen.mainScreen;
    if (@available(macOS 12.0, *)) {
        const NSInteger fps = screen.maximumFramesPerSecond;
        if (fps > 0) {
            return fps;
        }
    }
    return 60;
}

static void update_preferred_fps(void) {
    if (!view) {
        return;
    }
    const NSInteger fps = preferred_fps_for_current_screen();
    if (view.preferredFramesPerSecond != fps) {
        view.preferredFramesPerSecond = fps;
        NSLog(@"Display frame-rate preference: %ld FPS", (long)fps);
    }
}

static void setup_app_menu(void) {
    NSMenu* main_menu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem* app_menu_item = [[NSMenuItem alloc]
        initWithTitle:app_name action:nil keyEquivalent:@""];
    [main_menu addItem:app_menu_item];

    NSMenu* app_menu = [[NSMenu alloc] initWithTitle:app_name];
    app_menu_actions = [[AppMenuActions alloc] init];
    NSMenuItem* open_folder = [app_menu
        addItemWithTitle:@"Open OpenSoup Folder"
                   action:@selector(openOpenSoupFolder:)
            keyEquivalent:@""];
    open_folder.target = app_menu_actions;
    NSMenuItem* dump = [app_menu
        addItemWithTitle:@"Save Diagnostic Dump"
                   action:@selector(saveDiagnosticDump:)
            keyEquivalent:@"d"];
    dump.target = app_menu_actions;
    [app_menu addItem:[NSMenuItem separatorItem]];
    [app_menu addItemWithTitle:[@"Quit " stringByAppendingString:app_name]
                        action:@selector(terminate:)
                 keyEquivalent:@"q"];
    app_menu_item.submenu = app_menu;
    NSApp.mainMenu = main_menu;
}

// Window -> logical px (flipped view: top-left origin, y-down).
static void to_view_point(NSPoint window_point, float* x, float* y) {
    const NSPoint p = [view convertPoint:window_point fromView:nil];
    *x = (float)p.x;
    *y = (float)p.y;
}

@interface OverlayWindow : NSWindow
@end
@implementation OverlayWindow
- (BOOL)canBecomeKeyWindow {
    return YES;
}
@end

@interface OverlayView : MTKView
@end
@implementation OverlayView
- (BOOL)isFlipped {
    return YES;
}
- (BOOL)isOpaque {
    return NO;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    (void)event;
    return YES;
}
- (void)mouseDown:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    opensoup_mouse_down(x, y);
}
- (void)mouseDragged:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    opensoup_mouse_drag(x, y);
}
- (void)mouseUp:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    opensoup_mouse_up(x, y);
}
- (void)keyDown:(NSEvent*)event {
    if (event.keyCode == 101) { // F9
        opensoup_diagnostics_request();
        return;
    }
    [super keyDown:event];
}
- (void)scrollWheel:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    // Precise deltas are already in logical points; non-precise = detents.
    opensoup_scroll(x, y, (float)event.scrollingDeltaY,
                    event.hasPreciseScrollingDeltas);
}
@end

@interface OverlayViewDelegate : NSObject<MTKViewDelegate>
@end
@implementation OverlayViewDelegate
- (void)mtkView:(nonnull MTKView*)v drawableSizeWillChange:(CGSize)size {
    (void)size;
    const NSSize logical_size = v.bounds.size;
    opensoup_resize(logical_size.width, logical_size.height);
}
- (void)drawInMTKView:(nonnull MTKView*)v {
    @autoreleasepool {
        const NSPoint p = [window
            convertPointFromScreen:[NSEvent mouseLocation]];
        float cx, cy;
        to_view_point(p, &cx, &cy);
        const CFTimeInterval now = CACurrentMediaTime();
        const double dt_ms = last_frame_time > 0
                           ? (now - last_frame_time) * 1000.0 : 0.0;
        last_frame_time = now;

        const opensoup_frame_result r = opensoup_frame(dt_ms, cx, cy, true);
        window.ignoresMouseEvents = !r.wants_mouse;
        if (r.quit) {
            [NSApp terminate:nil];
            return;
        }

        const sg_swapchain swapchain = {
            .width = (int)v.drawableSize.width,
            .height = (int)v.drawableSize.height,
            .sample_count = 1,
            .color_format = SG_PIXELFORMAT_BGRA8,
            .depth_format = SG_PIXELFORMAT_NONE,
            .metal = {
                .current_drawable = (__bridge const void*)v.currentDrawable,
            },
        };
        const NSSize logical_size = v.bounds.size;
        scene_frame(&swapchain, (float)logical_size.width,
                    (float)logical_size.height, dt_ms);
    }
}
@end

@interface OverlayAppDelegate : NSObject<NSApplicationDelegate, NSWindowDelegate>
@end
@implementation OverlayAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)note {
    (void)note;
    const NSRect screen_rect = visible_scene_frame();

    window = [[OverlayWindow alloc]
        initWithContentRect:screen_rect
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.level = NSNormalWindowLevel;
    window.opaque = NO;
    window.backgroundColor = NSColor.clearColor;
    window.hasShadow = NO;
    window.ignoresMouseEvents = YES;
    window.title = app_name;
    window.delegate = self;

    mtl_device = MTLCreateSystemDefaultDevice();
    const NSRect view_rect = NSMakeRect(0, 0, screen_rect.size.width,
                                       screen_rect.size.height);
    view = [[OverlayView alloc] initWithFrame:view_rect device:mtl_device];
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    view.depthStencilPixelFormat = MTLPixelFormatInvalid;
    view.sampleCount = 1;
    view.layer.opaque = NO;
    view_delegate = [[OverlayViewDelegate alloc] init];
    view.delegate = view_delegate;
    window.contentView = view;
    update_preferred_fps();

    const sg_environment env = {
        .defaults = {
            .sample_count = 1,
            .color_format = SG_PIXELFORMAT_BGRA8,
            .depth_format = SG_PIXELFORMAT_NONE,
        },
        .metal = {
            .device = (__bridge const void*)mtl_device,
        },
    };
    scene_setup(&env);

    // World and assets use logical pixels; drawableSize is backing pixels.
    const NSSize logical_size = view.bounds.size;
    opensoup_start(logical_size.width, logical_size.height);

    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}
- (void)applicationDidChangeScreenParameters:(NSNotification*)note {
    (void)note;
    [window setFrame:visible_scene_frame() display:YES];
    update_preferred_fps();
}
- (void)windowDidChangeScreen:(NSNotification*)note {
    (void)note;
    update_preferred_fps();
}
- (void)applicationWillTerminate:(NSNotification*)note {
    (void)note;
    opensoup_shutdown();
}
@end

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0); // keep diagnostics visible when piped
    assets_root = app_assets_path();
    if (!assets_root) {
        fprintf(stderr, "cannot resolve the assets path\n");
        return 1;
    }
    app_assets_state state = app_assets_get_state(assets_root);
    if (state == APP_ASSETS_MISSING) {
        @autoreleasepool {
            if (!show_installer_picker()) {
                return 1;
            }
        }
        state = app_assets_get_state(assets_root);
    }
    if (state != APP_ASSETS_READY) {
        @autoreleasepool {
            show_missing_asset_alert();
        }
        return 1;
    }
    // see opensoup_boot for the Ruby 1.8 stack-base rule
    if (!opensoup_boot(assets_root)) {
        @autoreleasepool {
            show_quit_alert(@"Ruby framework failed to start",
                @"OpenSoup could not load the Souptoys Ruby framework. "
                 "Check the Ruby scripts in the Assets folder.");
        }
        return 1;
    }
    @autoreleasepool {
        NSProcessInfo.processInfo.processName = app_name;
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        setup_app_menu();
        host_dialog_set_open_file(run_open_panel, NULL);
        OverlayAppDelegate* delegate = [[OverlayAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp run];
    }
    return 0;
}
