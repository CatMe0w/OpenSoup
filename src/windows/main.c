#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <commdlg.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <windowsx.h>

#include "app_assets.h"
#include "composition.h"
#include "host_dialog.h"
#include "opensoup.h"
#include "scene.h"

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
static bool started; // the app is up, so resizes are worth reporting
static bool dragging;
static POINT last_mouse; // physical client px, for a synthesized release
static LARGE_INTEGER qpc_frequency;
static LARGE_INTEGER last_frame_counter;
static char opensoup_folder[MAX_PATH]; // native separators, for the shell

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

static void remember_opensoup_folder(const char* assets_root) {
    snprintf(opensoup_folder, sizeof opensoup_folder, "%s", assets_root);
    char* last = strrchr(opensoup_folder, '/');
    if (last) {
        *last = 0;
    }
    for (char* p = opensoup_folder; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
}

static void open_opensoup_folder(void) {
    ShellExecuteA(NULL, "open", opensoup_folder, NULL, NULL, SW_SHOWNORMAL);
}

static void show_quit_alert(const char* message, const char* information) {
    char body[1536];
    snprintf(body, sizeof body, "%s\n\n%s\n\nOpen the OpenSoup folder?",
             message, information);
    fprintf(stderr, "%s: %s\n", message, information);
    if (MessageBoxA(NULL, body, "OpenSoup",
                    MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND) == IDYES) {
        open_opensoup_folder();
    }
}

static bool show_installer_picker(const char* assets_root) {
    char path[MAX_PATH] = {0};
    OPENFILENAMEA picker = {
        .lStructSize = sizeof(OPENFILENAMEA),
        .lpstrFilter = "Souptoys installer (*.exe)\0*.exe\0"
                       "All files (*.*)\0*.*\0",
        .lpstrFile = path,
        .nMaxFile = sizeof path,
        .lpstrTitle = "Select the original Souptoys installer (.exe) to set "
                      "up OpenSoup's game assets",
        // OFN_NOCHANGEDIR: keep the process's working directory
        .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
    };
    if (!GetOpenFileNameA(&picker)) {
        return false;
    }

    char error[1024] = {0};
    if (app_assets_install_from_installer(path, assets_root, error,
                                          sizeof error)) {
        return true;
    }
    show_quit_alert("Game asset installation failed", error);
    return false;
}

// For the Toybox's open button
static bool run_open_dialog(const char* title, const char* directory,
                            const char* kind, const char* extension,
                            char* out, size_t out_size, void* user) {
    (void)user;
    // A GetOpenFileName filter is a run of NUL-terminated pairs closed by a
    // second NUL, so it cannot be built with plain string formatting.
    char filter[128];
    const int label = snprintf(filter, sizeof filter, "%s Files (*.%s)",
                               kind, extension);
    if (label < 0 || (size_t)label + 1 >= sizeof filter) {
        return false;
    }
    const int pattern = snprintf(filter + label + 1,
                                 sizeof filter - (size_t)label - 1,
                                 "*.%s", extension);
    if (pattern < 0 || (size_t)(label + 1 + pattern) + 2 > sizeof filter) {
        return false;
    }
    filter[label + 1 + pattern + 1] = 0;

    char native[MAX_PATH] = {0};
    char start[MAX_PATH];
    if (directory) {
        snprintf(start, sizeof start, "%s", directory);
        for (char* p = start; *p; p++) {
            if (*p == '/') {
                *p = '\\';
            }
        }
    }
    OPENFILENAMEA picker = {
        .lStructSize = sizeof(OPENFILENAMEA),
        .hwndOwner = window,
        .lpstrFilter = filter,
        .lpstrFile = native,
        .nMaxFile = sizeof native,
        .lpstrInitialDir = directory ? start : NULL,
        .lpstrTitle = title,
        .lpstrDefExt = extension,
        .nFilterIndex = 1,
        // OFN_NOCHANGEDIR: keep the process's working directory
        .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR,
    };
    if (!GetOpenFileNameA(&picker) || strlen(native) >= out_size) {
        return false;
    }
    // Everything downstream splits paths on '/' alone.
    snprintf(out, out_size, "%s", native);
    for (char* p = out; *p; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    return true;
}

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

// A DPI change moves the logical size even when the physical one holds still,
// so the view size is reported from here rather than from WM_SIZE alone.
static void report_view_size(void) {
    if (started) {
        opensoup_resize(logical_width(), logical_height());
    }
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
        dragging = true;
        last_mouse.x = GET_X_LPARAM(lparam);
        last_mouse.y = GET_Y_LPARAM(lparam);
        opensoup_mouse_down(to_logical(last_mouse.x),
                            to_logical(last_mouse.y));
        return 0;
    case WM_MOUSEMOVE:
        last_mouse.x = GET_X_LPARAM(lparam);
        last_mouse.y = GET_Y_LPARAM(lparam);
        // hover is driven by the per-frame cursor poll, so only a real drag
        // reaches the app from here
        if (dragging) {
            opensoup_mouse_drag(to_logical(last_mouse.x),
                                to_logical(last_mouse.y));
        }
        return 0;
    case WM_LBUTTONUP:
        last_mouse.x = GET_X_LPARAM(lparam);
        last_mouse.y = GET_Y_LPARAM(lparam);
        dragging = false; // ReleaseCapture re-enters as WM_CAPTURECHANGED
        opensoup_mouse_up(to_logical(last_mouse.x), to_logical(last_mouse.y));
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        return 0;
    case WM_CAPTURECHANGED:
        // Capture taken mid-drag; synthesize a release or the grab sticks.
        if (dragging) {
            dragging = false;
            opensoup_mouse_up(to_logical(last_mouse.x),
                              to_logical(last_mouse.y));
        }
        return 0;
    case WM_MOUSEWHEEL: {
        // unlike the button messages, this one carries SCREEN coordinates
        POINT p = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        if (ScreenToClient(hwnd, &p)) {
            opensoup_scroll(to_logical(p.x), to_logical(p.y),
                            (float)GET_WHEEL_DELTA_WPARAM(wparam)
                                / (float)WHEEL_DELTA,
                            false);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            running = false;
        } else if (wparam == VK_F9) {
            opensoup_diagnostics_request();
        }
        return 0;
    case WM_CLOSE:
    case WM_DESTROY:
        running = false;
        return 0;
    case WM_SIZE:
        resize_swapchain(LOWORD(lparam), HIWORD(lparam));
        report_view_size();
        return 0;
    case WM_DPICHANGED:
        dpi_scale = (float)LOWORD(wparam) / (float)USER_DEFAULT_SCREEN_DPI;
        apply_scene_bounds();
        report_view_size();
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

    // WS_EX_LAYERED is required for WS_EX_TRANSPARENT to pass mouse events.
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

static double elapsed_ms(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const LONGLONG previous = last_frame_counter.QuadPart;
    last_frame_counter = now;
    if (previous == 0) {
        return 0.0;
    }
    return (double)(now.QuadPart - previous) * 1000.0
         / (double)qpc_frequency.QuadPart;
}

static void render_frame(void) {
    if (!render_view || backbuffer_width <= 0 || backbuffer_height <= 0) {
        return;
    }
    const double dt_ms = elapsed_ms();

    POINT cursor;
    const bool cursor_valid = GetCursorPos(&cursor)
                           && ScreenToClient(window, &cursor);
    const opensoup_frame_result r = opensoup_frame(
        dt_ms, cursor_valid ? to_logical(cursor.x) : 0.0f,
        cursor_valid ? to_logical(cursor.y) : 0.0f, cursor_valid);
    set_mouse_transparent(!r.wants_mouse);
    if (r.quit) {
        running = false;
        return;
    }

    const sg_swapchain sokol_swapchain = {
        .width = backbuffer_width,
        .height = backbuffer_height,
        .sample_count = 1,
        .color_format = SG_PIXELFORMAT_BGRA8,
        .depth_format = SG_PIXELFORMAT_NONE,
        .d3d11 = { .render_view = render_view },
    };
    scene_frame(&sokol_swapchain, logical_width(), logical_height(), dt_ms);

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
    QueryPerformanceFrequency(&qpc_frequency);

    const char* assets_root = app_assets_path();
    if (!assets_root) {
        fail("cannot resolve the assets path", E_FAIL);
    }
    remember_opensoup_folder(assets_root);

    app_assets_state assets = app_assets_get_state(assets_root);
    if (assets == APP_ASSETS_MISSING) {
        if (!show_installer_picker(assets_root)) {
            return 1;
        }
        assets = app_assets_get_state(assets_root);
    }
    if (assets != APP_ASSETS_READY) {
        char message[512];
        snprintf(message, sizeof message,
                 "OpenSoup could not find game assets at:\n\n%s", assets_root);
        show_quit_alert("Game assets not found", message);
        return 1;
    }
    // see opensoup_boot for the Ruby 1.8 stack-base rule
    if (!opensoup_boot(assets_root)) {
        show_quit_alert("Ruby framework failed to start",
                        "OpenSoup could not load the Souptoys Ruby framework. "
                        "Check the Ruby scripts in the OpenSoup folder.");
        return 1;
    }

    create_window();
    create_graphics();
    host_dialog_set_open_file(run_open_dialog, NULL);

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
    scene_setup(&environment);

    // World and assets use logical pixels; backbuffer is physical.
    opensoup_start(logical_width(), logical_height());
    started = true;

    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);

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

    opensoup_shutdown();
    destroy_graphics();
    if (window) {
        DestroyWindow(window);
    }
    return 0;
}
