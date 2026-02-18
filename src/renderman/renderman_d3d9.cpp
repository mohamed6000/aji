#include "renderman.h"

#include <d3d9.h>

#if COMPILER_CL
#pragma comment(lib, "d3d9.lib")
#endif

static bool rm_initted;

static IDirect3DDevice9 *d3d_device;
static D3DPRESENT_PARAMETERS d3d_params;

NB_EXTERN bool rm_init(u32 window_id) {
    if (rm_initted) return true;

    IDirect3D9 *d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d9) {
        Log(NB_LOG_ERROR, "D3D9", "Failed to Direct3DCreate9.");
        return false;
    }

    HRESULT hr;
    UINT adapter = D3DADAPTER_DEFAULT;
    D3DFORMAT format = D3DFMT_X8R8G8B8;

    // Check the device capabilities.
    D3DCAPS9 caps = {0};
    hr = IDirect3D9_GetDeviceCaps(d3d9, adapter, D3DDEVTYPE_HAL, &caps);
    if (FAILED(hr)) {
        Log(NB_LOG_ERROR, "D3D9", "Failed to IDirect3D9_GetDeviceCaps.");
        return false;
    }
    
    bool vertex_hardware_processing_enabled = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

    HWND hwnd = (HWND)b_get_window_handle(window_id);

    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    d3d_params.Windowed   = TRUE;
    d3d_params.SwapEffect = D3DSWAPEFFECT_DISCARD;//D3DSWAPEFFECT_COPY;
    d3d_params.BackBufferFormat = format;
    d3d_params.hDeviceWindow = hwnd;
    d3d_params.BackBufferWidth  = client_rect.right  - client_rect.left;
    d3d_params.BackBufferHeight = client_rect.bottom - client_rect.top;
    d3d_params.EnableAutoDepthStencil = TRUE;
    d3d_params.AutoDepthStencilFormat = D3DFMT_D24S8;


    // Check device format.
/*
    hr = IDirect3D9_CheckDeviceFormat(d3d9,
                                      adapter,
                                      D3DDEVTYPE_HAL,
                                      ...);
*/

    // Enumerate adapters.
    UINT adapter_mode_count = IDirect3D9_GetAdapterModeCount(
        d3d9,
        adapter,
        format
    );

    if (!adapter_mode_count) {
        Log(NB_LOG_ERROR, "D3D9", "No adapter modes were found for the specified D3D format.");
        return false;
    }

    Log(NB_LOG_NONE, "D3D9", "Found %u adapter modes.", 
        adapter_mode_count);

    D3DDISPLAYMODE *display_modes = (D3DDISPLAYMODE *)nb_new_array(D3DDISPLAYMODE, adapter_mode_count, NB_GET_ALLOCATOR());
    if (!display_modes) return false;

    for (UINT index = 0; index < adapter_mode_count; ++index) {
        hr = IDirect3D9_EnumAdapterModes(d3d9,
                                         adapter,
                                         format,
                                         index,
                                         display_modes + index);
        if (hr == D3D_OK) {
            D3DDISPLAYMODE *mode = display_modes + index;
            Log(NB_LOG_NONE, "D3D9", "Found valid device mode:");
            Log(NB_LOG_NONE, null, "    Device %u: %ux%u %uHz", 
                index, mode->Width, mode->Height, mode->RefreshRate);
        } else if (hr == D3DERR_INVALIDCALL) {
            Log(NB_LOG_ERROR, "D3D9", "INVALIDCALL: The adapter equals or exceeds the number of display adapters in the system.");
        } else if (hr == D3DERR_NOTAVAILABLE) {
            Log(NB_LOG_ERROR, "D3D9", "NOTAVAILABLE: Either surface format is not supported or hardware acceleration is not available for the specified formats.");
        }
    }

    // hr = IDirect3D9_CheckDeviceType(...);

    hr = IDirect3D9_CreateDevice(d3d9, 
                                 adapter,
                                 D3DDEVTYPE_HAL,
                                 hwnd,
                                 vertex_hardware_processing_enabled ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                 &d3d_params,
                                 &d3d_device);
    if (FAILED(hr) && vertex_hardware_processing_enabled) {
        hr = IDirect3D9_CreateDevice(d3d9, 
                                     adapter,
                                     D3DDEVTYPE_HAL,
                                     hwnd,
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &d3d_params,
                                     &d3d_device);
    }

    if (FAILED(hr)) {
        Log(NB_LOG_ERROR, "D3D9", "Failed to IDirect3D9_CreateDevice.");
        return false;
    }

    return true;
}

NB_EXTERN void rm_finish(void) {
    IDirect3DDevice9_Release(d3d_device);
}

NB_EXTERN void rm_swap_buffers(u32 window_id) {
    UNUSED(window_id);
    IDirect3DDevice9_EndScene(d3d_device);
    IDirect3DDevice9_Present(d3d_device, 0, 0, 0, 0);
}

NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a) {
    IDirect3DDevice9_Clear(d3d_device, 0, null, 
                           D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                           D3DCOLOR_COLORVALUE(r,g,b,a),
                           1.0f, 0);

    // Test device lost state.
    {
        HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(d3d_device);
        if (FAILED(hr)) {
            if (hr == D3DERR_DEVICELOST) {
                // Do nothing.
            }

            if (hr == D3DERR_DEVICENOTRESET) {
                // @Todo: Free resources.
                IDirect3DDevice9_Reset(d3d_device, &d3d_params);
                // @Todo: Init resources.
            }
        }
    }

    if (FAILED(IDirect3DDevice9_BeginScene(d3d_device))) {
        Log(NB_LOG_ERROR, "D3D9", "Failed to IDirect3DDevice9_BeginScene.");
    }
}
