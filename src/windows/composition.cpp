// The one C++ file in OpenSoup: MinGW's <dcomp.h> is a C++-only header, so the
// DirectComposition calls live here behind a C boundary
#include "composition.h"

#include <dcomp.h>

namespace {

IDCompositionDevice* device;
IDCompositionTarget* target;
IDCompositionVisual* visual;

void release_all() {
    if (visual) {
        visual->Release();
        visual = nullptr;
    }
    if (target) {
        target->Release();
        target = nullptr;
    }
    if (device) {
        device->Release();
        device = nullptr;
    }
}

} // namespace

extern "C" HRESULT composition_attach(HWND window, IDXGIDevice* dxgi_device,
                                      IUnknown* content) {
    HRESULT hr = DCompositionCreateDevice(
        dxgi_device, __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(&device));
    if (SUCCEEDED(hr)) {
        hr = device->CreateTargetForHwnd(window, TRUE, &target);
    }
    if (SUCCEEDED(hr)) {
        hr = device->CreateVisual(&visual);
    }
    if (SUCCEEDED(hr)) {
        hr = visual->SetContent(content);
    }
    if (SUCCEEDED(hr)) {
        hr = target->SetRoot(visual);
    }
    if (SUCCEEDED(hr)) {
        hr = device->Commit();
    }
    if (FAILED(hr)) {
        release_all();
    }
    return hr;
}

extern "C" void composition_release(void) {
    release_all();
}
