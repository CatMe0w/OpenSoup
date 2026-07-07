// OpenSoup MVP: transparent always-on-top overlay with per-shape
// click-through, rendering via sokol_gfx (Metal). No sokol_app: window
// management is our own, since click-through overlays are the whole point.
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "scene.h"

static NSWindow* window;
static MTKView* view;
static id<MTLDevice> mtl_device;
static id view_delegate; // MTKView.delegate is weak, keep it alive here

// mouse position in window coords -> NDC (both y-up, no flip needed)
static void to_ndc(NSPoint p, float* nx, float* ny) {
    const NSRect b = view.bounds;
    *nx = 2.0f * (float)(p.x / b.size.width) - 1.0f;
    *ny = 2.0f * (float)(p.y / b.size.height) - 1.0f;
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
- (void)mouseDown:(NSEvent*)event {
    float nx, ny;
    to_ndc([event locationInWindow], &nx, &ny);
    if (scene_grab_begin(nx, ny)) {
        NSLog(@"grab begin (%.3f, %.3f)", nx, ny);
    }
}
- (void)mouseDragged:(NSEvent*)event {
    float nx, ny;
    to_ndc([event locationInWindow], &nx, &ny);
    scene_grab_move(nx, ny);
}
- (void)mouseUp:(NSEvent*)event {
    (void)event;
    if (scene_grabbing()) {
        scene_grab_end();
        NSLog(@"grab end");
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
    (void)v; (void)size;
}
- (void)drawInMTKView:(nonnull MTKView*)v {
    @autoreleasepool {
        // per-pixel(-shape) click-through: poll the global cursor, hit-test
        // the scene, and toggle ignoresMouseEvents. Never toggle mid-grab.
        if (!scene_grabbing()) {
            const NSPoint p = [NSEvent mouseLocation];
            const NSRect f = window.frame;
            float nx, ny;
            to_ndc(NSMakePoint(p.x - f.origin.x, p.y - f.origin.y), &nx, &ny);
            window.ignoresMouseEvents = !scene_hit_test(nx, ny);
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
        scene_frame(&swapchain);
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

    [window orderFrontRegardless];
    NSLog(@"OpenSoup MVP up: drag the triangle, empty space clicks through, Esc quits");
}
- (void)applicationWillTerminate:(NSNotification*)note {
    (void)note;
    scene_shutdown();
}
@end

int main(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        OverlayAppDelegate* delegate = [[OverlayAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
        [NSApp run];
    }
    return 0;
}
