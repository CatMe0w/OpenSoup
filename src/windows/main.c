#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_3.h>
#include <shellscalingapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <windowsx.h>

#include "composition.h"
#include "scene_demo.h"

static const wchar_t* const window_class_name = L"OpenSoupScene";
static const wchar_t* const app_name = L"OpenSoup";

static HWND window;
static ID3D11Device* d3d_device;
static ID3D11DeviceContext* d3d_context;
static IDXGISwapChain1* swapchain;
static HANDLE swapchain_waitable;
static ID3D11RenderTargetView* render_view;
static int backbuffer_width;
static int backbuffer_height;
static float dpi_scale = 1.0f;
static bool running = true;

static void fail(const char* what, HRESULT hr) {
    char message[512];
    snprintf(message, sizeof message, "%s (hr=0x%08lx)", what,
             (unsigned long)hr);
    fprintf(stderr, "%s\n", message);
    MessageBoxA(NULL, message, "OpenSoup", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

static void require(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        fail(what, hr);
    }
}

static void require_win32(bool ok, const char* what) {
    if (!ok) {
        fail(what, HRESULT_FROM_WIN32(GetLastError()));
    }
}

#define release_com(object)                             \
    do {                                                \
        if (object) {                                   \
            IUnknown_Release((IUnknown*)(object));      \
            (object) = NULL;                            \
        }                                               \
    } while (0)

// the scene speaks logical pixels; backing pixels belong to the swapchain
static float logical_width(void) {
    return (float)backbuffer_width / dpi_scale;
}

static float logical_height(void) {
    return (float)backbuffer_height / dpi_scale;
}

static float to_logical(int physical) {
    return (float)physical / dpi_scale;
}

static void set_mouse_transparent(bool transparent) {
    const LONG style = GetWindowLong(window, GWL_EXSTYLE);
    const LONG next = transparent ? (style | WS_EX_TRANSPARENT)
                                  : (style & ~WS_EX_TRANSPARENT);
    if (next != style) {
        SetWindowLong(window, GWL_EXSTYLE, next);
    }
}

static float window_dpi_scale(HWND hwnd) {
    UINT dpi_x = USER_DEFAULT_SCREEN_DPI;
    UINT dpi_y = USER_DEFAULT_SCREEN_DPI;
    if (FAILED(GetDpiForMonitor(
            MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI,
            &dpi_x, &dpi_y))) {
        return 1.0f;
    }
    return (float)dpi_x / (float)USER_DEFAULT_SCREEN_DPI;
}

static RECT scene_bounds(void) {
    RECT work;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
        work.left = 0;
        work.top = 0;
        work.right = GetSystemMetrics(SM_CXSCREEN);
        work.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    return work;
}

static void apply_scene_bounds(void) {
    const RECT bounds = scene_bounds();
    SetWindowPos(window, NULL, bounds.left, bounds.top,
                 bounds.right - bounds.left, bounds.bottom - bounds.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

static void create_render_view(void) {
    ID3D11Texture2D* backbuffer = NULL;
    require(IDXGISwapChain1_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D,
                                      (void**)&backbuffer),
            "cannot reach the swapchain backbuffer");
    require(ID3D11Device_CreateRenderTargetView(
                d3d_device, (ID3D11Resource*)backbuffer, NULL, &render_view),
            "cannot create the swapchain render target view");
    ID3D11Texture2D_Release(backbuffer);
}

static void resize_swapchain(int width, int height) {
    if (!swapchain || width <= 0 || height <= 0
        || (width == backbuffer_width && height == backbuffer_height)) {
        return;
    }
    release_com(render_view);
    require(IDXGISwapChain1_ResizeBuffers(
                swapchain, 0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN,
                DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT),
            "cannot resize the swapchain");
    backbuffer_width = width;
    backbuffer_height = height;
    create_render_view();
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam) {
    switch (message) {
    case WM_LBUTTONDOWN:
        SetCapture(hwnd); // Win32 has no implicit drag capture
        scene_demo_grab_begin(to_logical(GET_X_LPARAM(lparam)),
                              to_logical(GET_Y_LPARAM(lparam)));
        return 0;
    case WM_MOUSEMOVE:
        scene_demo_grab_move(to_logical(GET_X_LPARAM(lparam)),
                             to_logical(GET_Y_LPARAM(lparam)));
        return 0;
    case WM_LBUTTONUP:
        scene_demo_grab_move(to_logical(GET_X_LPARAM(lparam)),
                             to_logical(GET_Y_LPARAM(lparam)));
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        scene_demo_grab_end();
        return 0;
    case WM_CAPTURECHANGED:
        scene_demo_grab_end();
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            running = false;
        }
        return 0;
    case WM_CLOSE:
    case WM_DESTROY:
        running = false;
        return 0;
    case WM_SIZE:
        resize_swapchain(LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_DPICHANGED:
        dpi_scale = (float)LOWORD(wparam) / (float)USER_DEFAULT_SCREEN_DPI;
        apply_scene_bounds();
        return 0;
    case WM_DISPLAYCHANGE:
        apply_scene_bounds();
        return 0;
    case WM_SETTINGCHANGE:
        if (wparam == SPI_SETWORKAREA) {
            apply_scene_bounds();
        }
        return 0;
    case WM_ERASEBKGND:
        return 1; // DirectComposition owns every pixel
    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static void create_window(void) {
    const HINSTANCE instance = GetModuleHandleW(NULL);
    const WNDCLASSEXW window_class = {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = window_proc,
        .hInstance = instance,
        .hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW),
        .lpszClassName = window_class_name,
    };
    require_win32(RegisterClassExW(&window_class) != 0,
                  "cannot register the window class");

    // WS_EX_LAYERED is what lets the toggled WS_EX_TRANSPARENT pass the mouse
    // through: pass-through is a layered-window behaviour, and on a plain window
    // WS_EX_TRANSPARENT only reorders painting.
    const RECT bounds = scene_bounds();
    window = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT
            | WS_EX_APPWINDOW,
        window_class_name, app_name, WS_POPUP,
        bounds.left, bounds.top, bounds.right - bounds.left,
        bounds.bottom - bounds.top, NULL, NULL, instance, NULL);
    require_win32(window != NULL, "cannot create the scene window");

    // a layered window stays invisible until its attributes are set
    require_win32(SetLayeredWindowAttributes(window, 0, 255, LWA_ALPHA),
                  "cannot set the layered window attributes");

    dpi_scale = window_dpi_scale(window);

    RECT client;
    GetClientRect(window, &client);
    backbuffer_width = client.right - client.left;
    backbuffer_height = client.bottom - client.top;
}

static void create_graphics(void) {
    const UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL feature_level = 0;
    require(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              device_flags, NULL, 0, D3D11_SDK_VERSION,
                              &d3d_device, &feature_level, &d3d_context),
            "cannot create the D3D11 device");

    IDXGIDevice* dxgi_device = NULL;
    require(ID3D11Device_QueryInterface(d3d_device, &IID_IDXGIDevice,
                                        (void**)&dxgi_device),
            "cannot reach the DXGI device");
    IDXGIAdapter* adapter = NULL;
    require(IDXGIDevice_GetAdapter(dxgi_device, &adapter),
            "cannot reach the DXGI adapter");
    IDXGIFactory2* factory = NULL;
    require(IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2,
                                   (void**)&factory),
            "cannot reach the DXGI factory");
    release_com(adapter);

    // premultiplied alpha is what lets the desktop show through
    const DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
        .Width = (UINT)backbuffer_width,
        .Height = (UINT)backbuffer_height,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        .AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT,
    };
    require(IDXGIFactory2_CreateSwapChainForComposition(
                factory, (IUnknown*)d3d_device, &swapchain_desc, NULL,
                &swapchain),
            "cannot create the composition swapchain");
    release_com(factory);

    IDXGISwapChain2* swapchain2 = NULL;
    require(IDXGISwapChain1_QueryInterface(swapchain, &IID_IDXGISwapChain2,
                                           (void**)&swapchain2),
            "cannot reach the waitable swapchain interface");
    IDXGISwapChain2_SetMaximumFrameLatency(swapchain2, 1);
    swapchain_waitable = IDXGISwapChain2_GetFrameLatencyWaitableObject(
        swapchain2);
    release_com(swapchain2);
    if (!swapchain_waitable) {
        fail("cannot reach the swapchain waitable object", E_FAIL);
    }

    create_render_view();

    require(composition_attach(window, dxgi_device, (IUnknown*)swapchain),
            "cannot show the swapchain through DirectComposition");
    release_com(dxgi_device);
}

static void destroy_graphics(void) {
    composition_release();
    release_com(render_view);
    if (swapchain_waitable) {
        CloseHandle(swapchain_waitable);
        swapchain_waitable = NULL;
    }
    release_com(swapchain);
    release_com(d3d_context);
    release_com(d3d_device);
}

static void render_frame(void) {
    if (!render_view || backbuffer_width <= 0 || backbuffer_height <= 0) {
        return;
    }

    // per-shape click-through, never toggled mid-grab
    if (!scene_demo_grabbing()) {
        POINT cursor;
        if (GetCursorPos(&cursor) && ScreenToClient(window, &cursor)) {
            set_mouse_transparent(!scene_demo_hit_test(to_logical(cursor.x),
                                                       to_logical(cursor.y)));
        }
    }

    const sg_swapchain sokol_swapchain = {
        .width = backbuffer_width,
        .height = backbuffer_height,
        .sample_count = 1,
        .color_format = SG_PIXELFORMAT_BGRA8,
        .depth_format = SG_PIXELFORMAT_NONE,
        .d3d11 = { .render_view = render_view },
    };
    scene_demo_frame(&sokol_swapchain, logical_width(), logical_height());

    require(IDXGISwapChain1_Present(swapchain, 1, 0),
            "cannot present the swapchain");
}

static void pump_messages(void) {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            running = false;
            return;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); // msvcrt has no line buffering

    create_window();
    create_graphics();

    const sg_environment environment = {
        .defaults = {
            .sample_count = 1,
            .color_format = SG_PIXELFORMAT_BGRA8,
            .depth_format = SG_PIXELFORMAT_NONE,
        },
        .d3d11 = {
            .device = d3d_device,
            .device_context = d3d_context,
        },
    };
    scene_demo_setup(&environment, logical_width(), logical_height());

    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    puts("OpenSoup Windows host up: drag the triangle, empty space clicks "
         "through, Esc quits");

    while (running) {
        const DWORD wait = MsgWaitForMultipleObjectsEx(
            1, &swapchain_waitable, INFINITE, QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);
        pump_messages();
        if (!running) {
            break;
        }
        if (wait == WAIT_OBJECT_0) {
            render_frame();
        }
    }

    scene_demo_shutdown();
    destroy_graphics();
    if (window) {
        DestroyWindow(window);
    }
    return 0;
}
