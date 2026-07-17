// OpenSoup: transparent desktop scene with per-pixel
// click-through, rendering via sokol_gfx (Metal). No sokol_app: window
// management is our own, since click-through overlays are the whole point.
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "app_paths.h"
#include "scene.h"
#include "audio.h"
#include "rubyhost.h"
#include "toydefs.h"
#include "toybox.h"
#include "sprite_hook.h"

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
@end

@implementation AppMenuActions
- (void)openOpenSoupFolder:(id)sender {
    (void)sender;
    open_opensoup_folder();
}
@end

static AppMenuActions* app_menu_actions;

static void prepare_modal_ui(void) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
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

static void show_missing_asset_alert(app_assets_state state) {
    NSString* name = nil;
    if (state == APP_ASSETS_TOYDEFS_MISSING) {
        name = @"toydefs.json";
    } else if (state == APP_ASSETS_CORE_MISSING) {
        name = @"souptoys_core_toy";
    } else {
        return;
    }

    NSString* root = [NSString stringWithUTF8String:assets_root];
    NSString* missing = [root stringByAppendingPathComponent:name];
    show_quit_alert(@"Required game asset not found",
        [NSString stringWithFormat:
            @"OpenSoup could not find the required asset at:\n\n%@", missing]);
}

static void show_assets_directory_creation_alert(void) {
    NSString* path = [NSString stringWithUTF8String:assets_root];
    show_quit_alert(@"Assets folder could not be created",
        [NSString stringWithFormat:
            @"OpenSoup could not create its assets folder at:\n\n%@", path]);
}

static void show_installer_picker(void) {
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

    // The selected installer is intentionally not consumed yet.
    [panel runModal];
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
    [app_menu addItem:[NSMenuItem separatorItem]];
    [app_menu addItemWithTitle:[@"Quit " stringByAppendingString:app_name]
                        action:@selector(terminate:)
                 keyEquivalent:@"q"];
    app_menu_item.submenu = app_menu;
    NSApp.mainMenu = main_menu;
}

// Window coordinates -> OpenSoup view coordinates. OverlayView is flipped,
// so convertPoint supplies logical pixels/points with a top-left origin and
// y-down. Backing pixels never enter input, Ruby, Toybox, or hit-testing.
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
// mouse events route through Ruby: pick the sprite here (alpha test is
// scene-side), then Sprite#internal_mouse_* bubbles the event and the
// framework's default grab (limb.rb) drives engine.input_grab/move/release.
// captured = the mouse-downed sprite, our stand-in for Win32 mouse capture.
static int captured_sprite = -1;
static float down_pos[2];
- (void)mouseDown:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    if (toybox_mouse_down(x, y)) {
        captured_sprite = -1;
        return;
    }
    const int sprite = scene_pick(x, y);
    if (sprite >= 0) {
        scene_raise(sprite);
        captured_sprite = sprite;
        down_pos[0] = x;
        down_pos[1] = y;
        rbh_mouse_down(sprite, x, y, 1);
    }
}
- (void)mouseDragged:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    if (toybox_capturing()) {
        toybox_mouse_dragged(x, y);
    } else if (captured_sprite >= 0) {
        rbh_mouse_move(captured_sprite, x, y, 1, true);
    }
}
- (void)mouseUp:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    if (toybox_capturing()) {
        toybox_mouse_up(x, y);
    } else if (captured_sprite >= 0) {
        const bool over_toybox = toybox_hit_test(x, y);
        rbh_mouse_up(captured_sprite, x, y, 1);
        const bool recycled = over_toybox
                           && rbh_recycle_sprite(captured_sprite);
        // barely-moved release = click (Win32 sends it on button release)
        if (!over_toybox && !recycled
            && fabsf(x - down_pos[0]) < 4
            && fabsf(y - down_pos[1]) < 4) {
            rbh_mouse_click(captured_sprite, x, y, 1);
        }
        captured_sprite = -1;
    }
}
- (void)scrollWheel:(NSEvent*)event {
    float x, y;
    to_view_point([event locationInWindow], &x, &y);
    if (toybox_hit_test(x, y)) {
        const bool precise = event.hasPreciseScrollingDeltas;
        // AppKit reports precise scrolling deltas in points, which are already
        // OpenSoup's logical view units. Non-precise deltas remain detents.
        toybox_scroll((float)event.scrollingDeltaY, precise);
    }
}
@end

@interface OverlayViewDelegate : NSObject<MTKViewDelegate>
@end
@implementation OverlayViewDelegate
- (void)mtkView:(nonnull MTKView*)v drawableSizeWillChange:(CGSize)size {
    (void)size;
    const NSSize logical_size = v.bounds.size;
    rbh_screen_size(logical_size.width, logical_size.height);
    toybox_resize(logical_size.width, logical_size.height);
}
- (void)drawInMTKView:(nonnull MTKView*)v {
    @autoreleasepool {
        // per-pixel click-through: poll the global cursor, hit-test the
        // scene, toggle ignoresMouseEvents. Never toggle mid-drag.
        if (captured_sprite < 0 && !toybox_capturing()) {
            const NSPoint p = [window
                convertPointFromScreen:[NSEvent mouseLocation]];
            float x, y;
            to_view_point(p, &x, &y);
            toybox_pointer_move(x, y);
            window.ignoresMouseEvents =
                !(toybox_hit_test(x, y) || scene_hit_test(x, y));
        } else {
            window.ignoresMouseEvents = NO;
        }
        const CFTimeInterval now = CACurrentMediaTime();
        const double dt_ms = last_frame_time > 0
                           ? (now - last_frame_time) * 1000.0 : 0.0;
        last_frame_time = now;
        rbh_frame(dt_ms); // Ruby heartbeat: run_steps + dispatch_timers
        toybox_frame(dt_ms);
        if (toybox_quit_requested()) {
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

    // World extents and original 1x assets use logical pixels. drawableSize
    // is backing pixels and only belongs to the Metal swapchain.
    const NSSize logical_size = view.bounds.size;
    rbh_screen_size(logical_size.width, logical_size.height);

    sprite_hook_install(assets_root);
    const bool toybox_ok = toybox_init(assets_root, logical_size.width,
                                       logical_size.height);
    NSLog(@"OpenSoup up: Toybox %s (%d icons) from %s",
          toybox_ok ? "ready" : "unavailable", toybox_catalog_count(),
          assets_root);

    [window makeKeyAndOrderFront:nil];
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
    toybox_shutdown();
    scene_shutdown();
    rbh_shutdown();
    audio_shutdown();
}
@end

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0); // keep diagnostics visible when piped
    assets_root = app_assets_root();
    if (!assets_root) {
        fprintf(stderr, "cannot resolve the assets path\n");
        return 1;
    }
    const app_assets_state state = app_assets_get_state();
    if (state == APP_ASSETS_DIRECTORY_MISSING) {
        @autoreleasepool {
            // XXX: create directory after picker returns a valid installer
            if (app_assets_create_directory()) {
                show_installer_picker();
            } else {
                show_assets_directory_creation_alert();
            }
        }
        return 1; // TODO
    }
    if (state != APP_ASSETS_READY) {
        @autoreleasepool {
            show_missing_asset_alert(state);
        }
        return 1;
    }
    // Ruby boot from main: 1.8's conservative GC records the stack base at
    // ruby_init, so init must sit at least as shallow as any later Ruby call.
    {
        if (!audio_init(false)) {
            NSLog(@"Audio output unavailable, continuing silent");
        }
        char p[1024];
        snprintf(p, sizeof p, "%s/toydefs.json", assets_root);
        if (!toydefs_load(p)) {
            fprintf(stderr, "toydefs.json missing at %s\n", p);
        }
        snprintf(p, sizeof p, "%s/souptoys_core_toy", assets_root);
        if (!rbh_boot(p)) {
            @autoreleasepool {
                show_quit_alert(@"Ruby framework failed to start",
                    @"OpenSoup could not load the Souptoys Ruby framework. "
                     "Check the Ruby scripts in the Assets folder.");
            }
            return 1;
        }
        NSLog(@"Ruby framework booted");
    }
    @autoreleasepool {
        NSProcessInfo.processInfo.processName = app_name;
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        setup_app_menu();
        OverlayAppDelegate* delegate = [[OverlayAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp run];
    }
    return 0;
}
