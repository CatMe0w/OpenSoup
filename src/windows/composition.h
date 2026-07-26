#pragma once

#include <windows.h>

#include <dxgi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Shows content (a composition swapchain) in a window, and owns the
// DirectComposition visual tree until composition_release().
HRESULT composition_attach(HWND window, IDXGIDevice* dxgi_device,
                           IUnknown* content);
void composition_release(void);

#ifdef __cplusplus
}
#endif
