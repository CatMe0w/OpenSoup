// OpenSoup MVP: transparent always-on-top overlay with per-pixel
// click-through, rendering via sokol_gfx (Metal). No sokol_app: window
// management is our own, since click-through overlays are the whole point.
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "scene.h"
#include "demo.h"
#include "rubyhost.h"
#include "toydefs.h"

static NSWindow* window;
static MTKView* view;
static id<MTLDevice> mtl_device;
static id view_delegate; // MTKView.delegate is weak, keep it alive here
static const char* assets_root = "private/extracted"; // temporary, remove once we have a proper asset pipeline
static CFTimeInterval last_frame_time;

// window points (bottom-left origin) -> device pixels (top-left origin),
// the scene's coordinate space
static void to_px(NSPoint p, float* x, float* y) {
    const NSRect b = view.bounds;
    const float scale = (float)window.backingScaleFactor;
    *x = (float)p.x * scale;
    *y = (float)(b.size.height - p.y) * scale;
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
- (void)mouseDown:(NSEvent*)event {
    float x, y;
    to_px([event locationInWindow], &x, &y);
    const int sprite = scene_pick(x, y);
    if (sprite >= 0) {
        scene_raise(sprite);
        captured_sprite = sprite;
        rbh_mouse_down(sprite, x, y, 1);
    }
}
- (void)mouseDragged:(NSEvent*)event {
    if (captured_sprite >= 0) {
        float x, y;
        to_px([event locationInWindow], &x, &y);
        rbh_mouse_move(captured_sprite, x, y, 1, true);
    }
}
- (void)mouseUp:(NSEvent*)event {
    if (captured_sprite >= 0) {
        float x, y;
        to_px([event locationInWindow], &x, &y);
        rbh_mouse_up(captured_sprite, x, y, 1);
        captured_sprite = -1;
    }
}
- (void)keyDown:(NSEvent*)event {
    if (event.keyCode == 53) { // Esc
        [NSApp terminate:nil];
    }
}
@end

@interface OverlayViewDelegate : NSObject<MTKViewDelegate>
@end
@implementation OverlayViewDelegate
- (void)mtkView:(nonnull MTKView*)v drawableSizeWillChange:(CGSize)size {
    (void)v;
    rbh_screen_size(size.width, size.height);
}
- (void)drawInMTKView:(nonnull MTKView*)v {
    @autoreleasepool {
        // per-pixel click-through: poll the global cursor, hit-test the
        // scene, toggle ignoresMouseEvents. Never toggle mid-drag.
        if (captured_sprite < 0) {
            const NSPoint p = [NSEvent mouseLocation];
            const NSRect f = window.frame;
            float x, y;
            to_px(NSMakePoint(p.x - f.origin.x, p.y - f.origin.y), &x, &y);
            window.ignoresMouseEvents = !scene_hit_test(x, y);
        }
        const CFTimeInterval now = CACurrentMediaTime();
        const double dt_ms = last_frame_time > 0 ? (now - last_frame_time) * 1000.0 : 16.7;
        last_frame_time = now;
        rbh_frame(dt_ms); // Ruby heartbeat: run_steps + dispatch_timers

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
        scene_frame(&swapchain, dt_ms);
    }
}
@end

@interface OverlayAppDelegate : NSObject<NSApplicationDelegate>
@end
@implementation OverlayAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)note {
    (void)note;
    const NSRect screen_rect = [NSScreen mainScreen].frame;

    window = [[OverlayWindow alloc]
        initWithContentRect:screen_rect
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.level = NSStatusWindowLevel;
    window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
                              | NSWindowCollectionBehaviorStationary
                              | NSWindowCollectionBehaviorFullScreenAuxiliary;
    window.opaque = NO;
    window.backgroundColor = NSColor.clearColor;
    window.hasShadow = NO;
    window.ignoresMouseEvents = YES;

    mtl_device = MTLCreateSystemDefaultDevice();
    view = [[OverlayView alloc] initWithFrame:screen_rect device:mtl_device];
    view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    view.depthStencilPixelFormat = MTLPixelFormatInvalid;
    view.sampleCount = 1;
    view.preferredFramesPerSecond = 60;
    view.layer.opaque = NO;
    view_delegate = [[OverlayViewDelegate alloc] init];
    view.delegate = view_delegate;
    window.contentView = view;

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

    // world extents must be known before toys spawn
    rbh_screen_size(view.drawableSize.width, view.drawableSize.height);

    const int n = demo_load(assets_root);
    NSLog(@"OpenSoup MVP up: %d sprites from %s; drag them, empty space clicks through, Esc quits", n, assets_root);

    [window orderFrontRegardless];
}
- (void)applicationWillTerminate:(NSNotification*)note {
    (void)note;
    scene_shutdown();
}
@end

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IOLBF, 0); // keep demo printfs visible when piped
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--assets") == 0) {
            assets_root = argv[i + 1];
        }
    }
    // Ruby boot from main: 1.8's conservative GC records the stack base at
    // ruby_init, so init must sit at least as shallow as any later Ruby call.
    //
    // Framework-only for now; the native demo still owns the scene.
    {
        char p[1024];
        snprintf(p, sizeof p, "%s/toydefs.json", assets_root);
        if (!toydefs_load(p)) {
            fprintf(stderr, "toydefs.json missing at %s\n", p);
        }
        snprintf(p, sizeof p, "%s/souptoys_core_toy", assets_root);
        if (rbh_boot(p)) {
            NSLog(@"Ruby framework booted");
        } else {
            NSLog(@"Ruby framework boot failed, continuing native-only");
        }
    }
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        OverlayAppDelegate* delegate = [[OverlayAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp run];
    }
    return 0;
}
