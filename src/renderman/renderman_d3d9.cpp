#include "renderman.h"

#include <d3d9.h>

#pragma warning(push)
#pragma warning(disable:4115)
#include <d3dcompiler.h>
#pragma warning(pop)

#if COMPILER_CL
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

#define DXERROR9(v,n,d) {v, TEXT(n), TEXT(d)},
#define DXERROR9LAST(v,n,d) {v, TEXT(n), TEXT(d)}
#include "dxerr_d3d9.c"  // @Todo: dxerr header file.

typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
} Immediate_Vertex;

#define MAX_IMMEDIATE_VERTICES 2048

typedef struct {
    IDirect3DTexture9 *pointer;
    u32 min_filter;
    u32 mag_filter;
    u32 wrap;
} RM_Texture9;

typedef struct {
    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d_device;
    IDirect3DVertexBuffer9      *immediate_vb;
    IDirect3DVertexDeclaration9 *d3d_vertex_layout;
    IDirect3DVertexShader9      *vertex_shader;
    IDirect3DPixelShader9       *pixel_shader;
    D3DPRESENT_PARAMETERS       d3d_params;

    RM_Texture9 *texture_pointers;
    u32 texture_pointer_allocated;
    u32 texture_pointer_count;

    u32 num_immediate_vertices;
    Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];

    float pixels_to_proj_matrix[4][4];
    
    bool d3d_device_lost;
    bool vertex_hw_processing_enabled;
    bool has_rgba_support;
} Renderman_State;

static Renderman_State rm_state;
static bool rm_initted;


// @Todo: One shader source.
char rm_vertex_shader_source[] = NB_STRINGIFY(
float4x4 wvp : register(c0);

struct VS_Output {
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

VS_Output main(float3 pos : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR) {
    VS_Output result;
    result.pos   = mul(float4(pos, 1.0f), wvp);
    result.uv    = uv;
    result.color = color;
    return result;
};
);


// @Todo: Account for half pixel offset.
// https://aras-p.info/blog/2016/04/08/solving-dx9-half-pixel-offset/

char rm_pixel_shader_source[] = NB_STRINGIFY(
sampler2D rm_sampler : register(s0);

struct VS_Output {
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

float4 main(VS_Output input) : COLOR {
    float4 texel = tex2D(rm_sampler, input.uv);
    return texel * input.color;
}
);


static void d3d_log_shader_error(ID3DBlob *shader_error) {
    if (shader_error) {
#if LANGUAGE_C
        char *shader_error_message = (char *)(shader_error->lpVtbl->GetBufferPointer(shader_error));
#else
        char *shader_error_message = (char *)(shader_error->GetBufferPointer());
#endif

        Log("%s", shader_error_message);
    }
}

static char *d3d_hresult_to_message(HRESULT hr) {
    char *result = null;

    WCHAR *message_text = null;

    DWORD message_length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER|
      FORMAT_MESSAGE_FROM_SYSTEM|
      FORMAT_MESSAGE_IGNORE_INSERTS,
      null,
      hr,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      // MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
      (LPWSTR)&message_text, 0,
      null
    );

    UNUSED(message_length);

    if (message_text) {
        result = b_w32_wide_to_utf8(message_text, 
                                          message_length, 
                                          nb_temporary_allocator);

        // Log("%s", result);

        LocalFree(message_text);
    } else {
        // Failed: fallback to dxerr messages.
        TCHAR *s = (TCHAR *)DXGetErrorString(hr);
        TCHAR *d = (TCHAR *)DXGetErrorDescription(hr);

        char *error_str  = b_w32_wide_to_utf8(s, 0, nb_temporary_allocator);
        char *error_desc = b_w32_wide_to_utf8(d, 0, nb_temporary_allocator);

        result = tprint("%s: %s", error_str, error_desc);
    }

    return result;
}

static bool 
d3d_check_format_support(IDirect3DDevice9 *device, D3DFORMAT format) {
    bool result = false;
    IDirect3D9 *d3d = null;
    D3DDEVICE_CREATION_PARAMETERS params = {};
    D3DDISPLAYMODE mode = {};

    if (IDirect3DDevice9_GetDirect3D(device, &d3d) != D3D_OK) {
        return result;
    }

    if (IDirect3DDevice9_GetCreationParameters(device, &params) != D3D_OK ||
        IDirect3DDevice9_GetDisplayMode(device, 0, &mode) != D3D_OK) {
        IDirect3D9_Release(d3d);
        return result;
    }

    result = IDirect3D9_CheckDeviceFormat(d3d, params.AdapterOrdinal, 
                                          params.DeviceType,
                                          mode.Format,
                                          D3DUSAGE_DYNAMIC|
                                          D3DUSAGE_QUERY_FILTER|
                                          D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                          D3DRTYPE_TEXTURE,
                                          format) == D3D_OK;

    return result;
}

char *
d3d_get_gpu_string(IDirect3DDevice9 *device) {
    char *result = null;
    IDirect3D9 *d3d = null;
    D3DDEVICE_CREATION_PARAMETERS params = {};
    D3DADAPTER_IDENTIFIER9 ident;

    IDirect3DDevice9_GetDirect3D(device, &d3d);
    if (d3d) {
        if (IDirect3DDevice9_GetCreationParameters(device, &params) == D3D_OK) {
            if (IDirect3D9_GetAdapterIdentifier(d3d, params.AdapterOrdinal, 0, &ident) == D3D_OK) {
                result = tprint("%s", ident.Description);
            }

            IDirect3D9_Release(d3d);
        }
    }

    return result;
}

static void d3d_immediate_mode_device_state_set(void) {
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_FILLMODE,  D3DFILL_SOLID);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZWRITEENABLE,    FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_BLENDOP,   D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SRCBLENDALPHA,  D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SCISSORTESTENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_FOGENABLE,         FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_RANGEFOGENABLE,    FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SPECULARENABLE,    FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_STENCILENABLE,     FALSE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_CLIPPING,          TRUE);
    IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_LIGHTING,          FALSE);
}

static bool 
d3d_immediate_mode_init(void) {
    HRESULT hr;

    // Immediate non-FVF vertex buffer.
    hr = IDirect3DDevice9_CreateVertexBuffer(rm_state.d3d_device, 
                                             size_of(rm_state.immediate_vertices),
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             0, // No more FVF D3DFVF_XYZ,
                                             D3DPOOL_DEFAULT,
                                             &rm_state.immediate_vb, null);
    if (FAILED(hr)) {
        Log("Failed to create the immediate vertex buffer.");
        Log("%s", d3d_hresult_to_message(hr));
        return false;
    }

    // Vertex declaration.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/mapping-fvf-codes-to-a-directx-9-declaration
    D3DVERTEXELEMENT9 vertex_elements[] = {
        {/*stream=*/0, /*offset=*/0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, /*usage_index=*/0},
        {/*stream=*/0, /*offset=*/12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, /*usage_index=*/0},
        {/*stream=*/0, /*offset=*/20, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, /*usage_index=*/0},

        D3DDECL_END()
    };

    hr = IDirect3DDevice9_CreateVertexDeclaration(rm_state.d3d_device,
                                                  vertex_elements,
                                                  &rm_state.d3d_vertex_layout);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreateVertexDeclaration.");
        Log("%s", d3d_hresult_to_message(hr));
        return false;
    }

    // Vertex shader.
    ID3DBlob *compiled_shader = null;
    ID3DBlob *shader_error    = null;
    hr = D3DCompile(rm_vertex_shader_source, 
                    size_of(rm_vertex_shader_source),
                    /*pSourceName=*/"basic_vs.hlsl", 
                    /*pDefines=*/null,
                    /*pInclude=*/null,
                    "main",
                    "vs_3_0",
                    0,
                    0,
                    &compiled_shader,
                    &shader_error);
    if (FAILED(hr)) {
        Log("Failed to compile the vertex shader.");
        Log("%s", d3d_hresult_to_message(hr));

        d3d_log_shader_error(shader_error);

        return false;
    }

    // @Todo: Release compiled shader.

#if LANGUAGE_C
    DWORD *data = (DWORD *)(compiled_shader->lpVtbl->GetBufferPointer(compiled_shader));//ID3D10Blob_GetBufferPointer(compiled_shader);
#else
    DWORD *data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreateVertexShader(rm_state.d3d_device,
                                             data,
                                             &rm_state.vertex_shader);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreateVertexShader.");
        Log("%s", d3d_hresult_to_message(hr));
        return false;
    }

    // Pixel shader.
    compiled_shader = null;
    shader_error    = null;
    hr = D3DCompile(rm_pixel_shader_source, 
                    size_of(rm_pixel_shader_source),
                    /*pSourceName=*/"basic_ps.hlsl", 
                    /*pDefines=*/null,
                    /*pInclude=*/null,
                    "main",
                    "ps_3_0",
                    0,
                    0,
                    &compiled_shader,
                    &shader_error);
    if (FAILED(hr)) {
        Log("Failed to compile the pixel shader.");
        Log("%s", d3d_hresult_to_message(hr));

        d3d_log_shader_error(shader_error);

        return false;
    }

#if LANGUAGE_C
    data = (DWORD *)(compiled_shader->lpVtbl->GetBufferPointer(compiled_shader));
#else
    data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreatePixelShader(rm_state.d3d_device,
                                            data,
                                            &rm_state.pixel_shader);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreatePixelShader.");
        Log("%s", d3d_hresult_to_message(hr));
        return false;
    }

    // @Todo: Release compiled shader.


    d3d_immediate_mode_device_state_set();

    return true;
}

static void d3d_immediate_mode_release(void) {
    if (rm_state.immediate_vb) {
        IDirect3DVertexBuffer9_Release(rm_state.immediate_vb);
        rm_state.immediate_vb = null;
    }

    if (rm_state.d3d_vertex_layout) {
        IDirect3DVertexDeclaration9_Release(rm_state.d3d_vertex_layout);
        rm_state.d3d_vertex_layout = null;
    }

    if (rm_state.vertex_shader) {
        IDirect3DVertexShader9_Release(rm_state.vertex_shader);
        rm_state.vertex_shader = null;
    }

    if (rm_state.pixel_shader) {
        IDirect3DPixelShader9_Release(rm_state.pixel_shader);
        rm_state.pixel_shader = null;
    }
}

static IDirect3DDevice9 *d3d_device_create(UINT adapter, HWND hwnd) {
    HRESULT hr;
    IDirect3DDevice9 *device = null;

    hr = IDirect3D9_CreateDevice(rm_state.d3d9, 
                                 adapter,
                                 D3DDEVTYPE_HAL,
                                 hwnd,
                                 rm_state.vertex_hw_processing_enabled ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                 &rm_state.d3d_params,
                                 &device);
    if (FAILED(hr)) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to create d3d9 device.\nTrying to create a device with depth stencil format: D3DFMT_D16.");
        rm_state.d3d_params.AutoDepthStencilFormat = D3DFMT_D16;
        hr = IDirect3D9_CreateDevice(rm_state.d3d9, 
                                     adapter,
                                     D3DDEVTYPE_HAL,
                                     hwnd,
                                     rm_state.vertex_hw_processing_enabled ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &rm_state.d3d_params,
                                     &device);
        if (FAILED(hr)) {
            nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to create d3d9 device.\nTrying to create a device with software vertex processing.");
            hr = IDirect3D9_CreateDevice(rm_state.d3d9, 
                                         adapter,
                                         D3DDEVTYPE_HAL,
                                         hwnd,
                                         D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                         &rm_state.d3d_params,
                                         &device);
        }
    }

    if (FAILED(hr) || !device) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to IDirect3D9_CreateDevice.");
        nb_log_print(NB_LOG_ERROR, "D3D9", "%s", d3d_hresult_to_message(hr));
        return null;
    }

    return device;
}


NB_EXTERN bool rm_init(u32 window_id) {
    if (rm_initted) return true;

    const char *old_ident = nb_logger_push_ident("D3D9");
    u32 old_mode = nb_logger_push_mode(NB_LOG_ERROR);

    rm_state.d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!rm_state.d3d9) {
        Log("Failed to Direct3DCreate9.");
        return false;
    }

    HRESULT hr;
    UINT adapter = D3DADAPTER_DEFAULT;
    D3DFORMAT tested_formats[] = {D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8};
    D3DFORMAT format = tested_formats[2];

    // Check the device capabilities.
    D3DCAPS9 caps = {0};
    hr = IDirect3D9_GetDeviceCaps(rm_state.d3d9, adapter, D3DDEVTYPE_HAL, &caps);
    if (FAILED(hr)) {
        Log("Failed to IDirect3D9_GetDeviceCaps.");
        Log("%s", d3d_hresult_to_message(hr));
        return false;
    }
    
    rm_state.vertex_hw_processing_enabled = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

    HWND hwnd = (HWND)b_get_window_handle(window_id);

    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    rm_state.d3d_params.Windowed   = TRUE;
    rm_state.d3d_params.SwapEffect = D3DSWAPEFFECT_DISCARD;//D3DSWAPEFFECT_COPY;
    rm_state.d3d_params.BackBufferFormat = format;
    rm_state.d3d_params.BackBufferCount  = 1;
    rm_state.d3d_params.hDeviceWindow    = hwnd;
    rm_state.d3d_params.BackBufferWidth  = client_rect.right  - client_rect.left;
    rm_state.d3d_params.BackBufferHeight = client_rect.bottom - client_rect.top;
    rm_state.d3d_params.MultiSampleType  = D3DMULTISAMPLE_NONE;
    rm_state.d3d_params.MultiSampleQuality = 0;
    rm_state.d3d_params.Flags = 0;
    rm_state.d3d_params.EnableAutoDepthStencil = TRUE;
    rm_state.d3d_params.AutoDepthStencilFormat = D3DFMT_D24S8;
    rm_state.d3d_params.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    rm_state.d3d_params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;  // vsync on.
    // rm_state.d3d_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;  // vsync off.


    // Check device format.
/*
    hr = IDirect3D9_CheckDeviceFormat(d3d9,
                                      adapter,
                                      D3DDEVTYPE_HAL,
                                      ...);
*/

#if 0
    // Enumerate adapters.
    UINT adapter_mode_count = IDirect3D9_GetAdapterModeCount(
        d3d9,
        adapter,
        format
    );

    if (!adapter_mode_count) {
        Log("No adapter modes were found for the specified D3D format.");
        return false;
    }

    nb_log_print(NB_LOG_NONE, "D3D9", "Found %u adapter modes.", adapter_mode_count);

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
            nb_log_print(NB_LOG_NONE, "D3D9", "Found valid device mode:");

            nb_log_print(NB_LOG_NONE, null,
                         "    Device %u: %ux%u %uHz", index, mode->Width, mode->Height, mode->RefreshRate);
        } else if (hr == D3DERR_INVALIDCALL) {
            nb_log_print(NB_LOG_ERROR, "D3D9", 
                         "INVALIDCALL: The adapter equals or exceeds the number of display adapters in the system.");
        } else if (hr == D3DERR_NOTAVAILABLE) {
            nb_log_print(NB_LOG_ERROR, "D3D9", 
                         "NOTAVAILABLE: Either surface format is not supported or hardware acceleration is not available for the specified formats.");
        }
    }
#endif

    // hr = IDirect3D9_CheckDeviceType(...);

    D3DADAPTER_IDENTIFIER9 identifier;
    if (IDirect3D9_GetAdapterIdentifier(rm_state.d3d9, 0, 0, &identifier) == D3D_OK) {
        nb_log_print(NB_LOG_NONE, "D3D9", "Driver: %s", identifier.Driver);
        nb_log_print(NB_LOG_NONE, "D3D9", "Description: %s", identifier.Description);
        nb_log_print(NB_LOG_NONE, "D3D9", "Device Name: %s", identifier.DeviceName);
        nb_log_print(NB_LOG_NONE, "D3D9", "Driver Version: %d.%d.%d.%d",
                     HIWORD(identifier.DriverVersion.HighPart),
                     LOWORD(identifier.DriverVersion.HighPart),
                     HIWORD(identifier.DriverVersion.LowPart),
                     LOWORD(identifier.DriverVersion.LowPart));
    }

    rm_state.d3d_device = d3d_device_create(D3DADAPTER_DEFAULT, hwnd);
    if (!rm_state.d3d_device) return false;

    rm_state.has_rgba_support = d3d_check_format_support(rm_state.d3d_device,
                                                         D3DFMT_A8B8G8R8);

    if (!d3d_immediate_mode_init()) {
        return false;
    }

    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width  = rm_state.d3d_params.BackBufferWidth;
    vp.Height = rm_state.d3d_params.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    IDirect3DDevice9_SetViewport(rm_state.d3d_device, &vp);


    nb_logger_push_mode(old_mode);
    nb_logger_push_ident(old_ident);

    rm_initted = true;

    return true;
}

NB_EXTERN void rm_finish(void) {
    d3d_immediate_mode_release();

    if (rm_state.d3d_device) {
        IDirect3DDevice9_Release(rm_state.d3d_device);
        rm_state.d3d_device = null;
    }

    if (rm_state.d3d9) {
        IDirect3D9_Release(rm_state.d3d9);
        rm_state.d3d9 = null;
    }
}

#if 0
static D3DMATRIX *
d3d_matrix_multiply(D3DMATRIX *m_out, 
                    D3DMATRIX *m1,
                    D3DMATRIX *m2) {
    nb_memory_zero(m_out, size_of(D3DMATRIX));

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            // m_out->m[i][j] = 0;
            for (int k = 0; k < 4; ++k) {
                m_out->m[i][j] += m1->m[i][k] * m2->m[k][j];
            }
        }
    }

    return m_out;
}

static D3DMATRIX *
d3d_matrix_transpose(D3DMATRIX *m_out, D3DMATRIX *m_in) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m_out->m[i][j] = m_in->m[j][i];
        }
    }

    return m_out;
}
#endif

static float *
rm_matrix_transpose(float *m_out, float *m_in) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m_out[4*i+j] = m_in[4*j+i];
        }
    }

    return m_out;
}

NB_EXTERN void rm_swap_buffers(u32 window_id) {
    UNUSED(window_id);
    
    HRESULT hr;
    
    hr = IDirect3DDevice9_Present(rm_state.d3d_device, 0, 0, 0, 0);
    if (hr == D3DERR_DEVICELOST) {
        rm_state.d3d_device_lost = true;
    }
}

static void d3d_reset_device(void) {
    HRESULT hr;

    // Release immediate mode default resources.
    if (rm_state.immediate_vb) {
        IDirect3DVertexBuffer9_Release(rm_state.immediate_vb);
        rm_state.immediate_vb = null;
    }

    hr = IDirect3DDevice9_Reset(rm_state.d3d_device, &rm_state.d3d_params);
    assert(hr != D3DERR_INVALIDCALL);

    // Init immediate mode default resources.

    // Immediate non-FVF vertex buffer.
    hr = IDirect3DDevice9_CreateVertexBuffer(rm_state.d3d_device, 
                                             size_of(rm_state.immediate_vertices),
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             0, // No more FVF D3DFVF_XYZ,
                                             D3DPOOL_DEFAULT,
                                             &rm_state.immediate_vb, null);
    if (FAILED(hr)) {
        Log("Failed to create the immediate vertex buffer.");
        Log("%s", d3d_hresult_to_message(hr));
        return;
    }


    d3d_immediate_mode_device_state_set();
}

NB_EXTERN void rm_backbuffer_resize(s32 width, s32 height) {
    //
    // @Note: We only need to recreate the D3DPOOL_DEFAULT resources.
    // This is good for texture memory (D3DPOOL_MANAGED), but we still
    // need to re-bind the sampler states.
    //

    rm_state.d3d_params.BackBufferWidth  = width;
    rm_state.d3d_params.BackBufferHeight = height;

    // @Note: d3d_reset_device uses rm_state.d3d_params
    d3d_reset_device();
}

NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a) {
    IDirect3DDevice9_Clear(rm_state.d3d_device, 0, null, 
                           D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                           D3DCOLOR_COLORVALUE(r,g,b,a),
                           1.0f, 0);
}



NB_EXTERN void rm_begin_rendering_2d(float render_target_width, 
                                     float render_target_height) {
    // float zn = -1.0f, zf = 1.0f;

    rm_state.pixels_to_proj_matrix[0][0] = 2.0f / render_target_width;
    rm_state.pixels_to_proj_matrix[0][1] = 0;
    rm_state.pixels_to_proj_matrix[0][2] = 0;
    rm_state.pixels_to_proj_matrix[0][3] = 0;

    rm_state.pixels_to_proj_matrix[1][0] = 0;
    rm_state.pixels_to_proj_matrix[1][1] = 2.0f / render_target_height;
    rm_state.pixels_to_proj_matrix[1][2] = 0;
    rm_state.pixels_to_proj_matrix[1][3] = 0;

    rm_state.pixels_to_proj_matrix[2][0] = 0;
    rm_state.pixels_to_proj_matrix[2][1] = 0;
    // rm_state.pixels_to_proj_matrix[2][2] = 1/(zn-zf);
    rm_state.pixels_to_proj_matrix[2][2] = 1;
    rm_state.pixels_to_proj_matrix[2][3] = 0;

    rm_state.pixels_to_proj_matrix[3][0] = -1;
    rm_state.pixels_to_proj_matrix[3][1] = -1;
    // rm_state.pixels_to_proj_matrix[3][2] = zn/(zn-zf);
    rm_state.pixels_to_proj_matrix[3][2] = 0;
    rm_state.pixels_to_proj_matrix[3][3] = 1;
}

NB_EXTERN void rm_immediate_frame_end(void) {
    HRESULT hr;

    // Test device lost state.
    if (rm_state.d3d_device_lost) {
        hr = IDirect3DDevice9_TestCooperativeLevel(rm_state.d3d_device);
        if (hr == D3DERR_DEVICELOST) {
            // Do nothing.
            nb_log_print(NB_LOG_NONE, "D3D9", "Device Lost.");
            bender_sleep_ms(10);
            return;
        }

        if (hr == D3DERR_DEVICENOTRESET) {
            nb_log_print(NB_LOG_NONE, "D3D9", "Device Not Reset.");
            d3d_reset_device();
        }

        rm_state.d3d_device_lost = false;
    }

    if (IDirect3DDevice9_BeginScene(rm_state.d3d_device) >= 0) {
        if (rm_state.num_immediate_vertices) {
            // IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_CULLMODE, D3DCULL_CW);
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZENABLE, FALSE);
            // IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ALPHABLENDENABLE, FALSE);
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SCISSORTESTENABLE, FALSE);

            // @Todo: Renderer state changes.


            {
                RM_Texture9 *t9 = rm_state.texture_pointers;

                IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_MINFILTER, t9->min_filter);
                IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_MAGFILTER, t9->mag_filter);
                IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_ADDRESSU, t9->wrap);
                IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_ADDRESSV, t9->wrap);

                IDirect3DDevice9_SetTexture(rm_state.d3d_device, /*slot=*/0, (IDirect3DBaseTexture9 *)(void *)(t9->pointer));
            }


            void *locked_vb = null;
            if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(rm_state.immediate_vb, 
                0, 
                rm_state.num_immediate_vertices*size_of(Immediate_Vertex), 
                &locked_vb, D3DLOCK_DISCARD))) {
                memcpy(locked_vb, 
                       rm_state.immediate_vertices, 
                       rm_state.num_immediate_vertices*size_of(Immediate_Vertex));

                IDirect3DVertexBuffer9_Unlock(rm_state.immediate_vb);
            }

            IDirect3DDevice9_SetStreamSource(rm_state.d3d_device,
                                             0,
                                             rm_state.immediate_vb,
                                             0,
                                             size_of(Immediate_Vertex));

            IDirect3DDevice9_SetVertexDeclaration(rm_state.d3d_device, 
                                                  rm_state.d3d_vertex_layout);

            float transposed[16];
            rm_matrix_transpose(transposed, &rm_state.pixels_to_proj_matrix[0][0]);
            IDirect3DDevice9_SetVertexShaderConstantF(rm_state.d3d_device, 
                                                      0, 
                                                      transposed, 4);

            IDirect3DDevice9_SetVertexShader(rm_state.d3d_device, rm_state.vertex_shader);
            IDirect3DDevice9_SetPixelShader(rm_state.d3d_device, rm_state.pixel_shader);

            UINT num_primitives = rm_state.num_immediate_vertices / 3;
            IDirect3DDevice9_DrawPrimitive(rm_state.d3d_device,
                                           D3DPT_TRIANGLELIST, 
                                           0, num_primitives);

            rm_state.num_immediate_vertices = 0;
        }

        IDirect3DDevice9_EndScene(rm_state.d3d_device);
    } else {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to IDirect3DDevice9_BeginScene.");
        Log("%s", d3d_hresult_to_message(GetLastError()));
    }
}

NB_EXTERN void rm_viewport_set(float x0, float y0, float x1, float y1) {
    D3DVIEWPORT9 vp;
    vp.X = (DWORD)x0;
    vp.Y = (DWORD)y0;
    vp.Width  = (DWORD)(x1-x0);
    vp.Height = (DWORD)(y1-y0);
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    IDirect3DDevice9_SetViewport(rm_state.d3d_device, &vp);
}



NB_INLINE void rm_put_vertex(Immediate_Vertex *dest, 
                             float x, float y, float z,
                             float u, float v,
                             float r, float g, float b, float a) {
    dest->x = x;
    dest->y = y;
    dest->z = z;

    dest->u = u;
    dest->v = v;

    dest->r = r;
    dest->g = g;
    dest->b = b;
    dest->a = a;
}

NB_EXTERN void 
rm_immediate_quad(float x0, float y0, float x1, float y1,
                  float r, float g, float b, float a) {
    Immediate_Vertex *dest = rm_state.immediate_vertices + rm_state.num_immediate_vertices;

    rm_put_vertex(dest,   x0, y0, 0,  0, 0,  r, g, b, a);
    rm_put_vertex(dest+1, x1, y0, 0,  1, 0,  r, g, b, a);
    rm_put_vertex(dest+2, x1, y1, 0,  1, 1,  r, g, b, a);

    rm_put_vertex(dest+3, x0, y0, 0,  0, 0,  r, g, b, a);
    rm_put_vertex(dest+4, x1, y1, 0,  1, 1,  r, g, b, a);
    rm_put_vertex(dest+5, x0, y1, 0,  0, 1,  r, g, b, a);

    rm_state.num_immediate_vertices += 6;
}



NB_INLINE D3DFORMAT 
d3d9_format_from_renderman(Renderman_Format format) {
    D3DFORMAT result;

    switch (format) {
        // case RM_FORMAT_R8:      result = D3DFMT_A8; break;
        case RM_FORMAT_R8:      result = D3DFMT_L8; break;
        case RM_FORMAT_RG8:     result = D3DFMT_A8L8; break;
        case RM_FORMAT_RGB8:    result = D3DFMT_R8G8B8; break;
        case RM_FORMAT_RGBA8:   result = D3DFMT_A8R8G8B8; break;
        case RM_FORMAT_R16:     result = D3DFMT_R16F; break;
        case RM_FORMAT_RG16:    result = D3DFMT_G16R16F; break;
        // case RM_FORMAT_RGB16:   result = D3DFMT_A16B16G16R16F; break; // @Cleanup: Expands to RGBA16.
        case RM_FORMAT_RGBA16:  result = D3DFMT_A16B16G16R16F; break;
        case RM_FORMAT_R32:     result = D3DFMT_R32F; break;
        case RM_FORMAT_RG32:    result = D3DFMT_G32R32F; break;
        // case RM_FORMAT_RGB32:   result = D3DFMT_A32B32G32R32F; break; // @Cleanup: Expands to RGBA32.
        case RM_FORMAT_RGBA32:  result = D3DFMT_A32B32G32R32F; break;
        case RM_FORMAT_DEPTH16: result = D3DFMT_D16; break;
        case RM_FORMAT_DEPTH32: result = D3DFMT_D32; break;
        case RM_FORMAT_DEPTH24_STENCIL8: result = D3DFMT_D24S8; break;

        default: result = D3DFMT_UNKNOWN; break;
    }

    return result;
}

NB_EXTERN u32 
rm_texture_create(Renderman_Format format, u32 x, u32 y, u32 z, 
                  bool filter, bool wrap, void *data) {
    u32 result, bytes_per_pixel;
    HRESULT hr;
    D3DFORMAT d3d_format;
    IDirect3DTexture9 *texture;
    D3DLOCKED_RECT locked_rect;

    if (x == 0) x = 1;
    if (y == 0) y = 1;
    if (z == 0) z = 1;

    result = (u32)-1;
    texture = null;
    d3d_format = D3DFMT_UNKNOWN;
    bytes_per_pixel = 0;

    switch (format) {
        case RM_FORMAT_R8:
            d3d_format = D3DFMT_L8;
            bytes_per_pixel = 1;
        break;
        
        case RM_FORMAT_RG8:
            d3d_format = D3DFMT_A8L8;
            bytes_per_pixel = 2;
        break;
        
        case RM_FORMAT_RGBA8:
            d3d_format = D3DFMT_A8R8G8B8;
            bytes_per_pixel = 4;
        break;
        
        case RM_FORMAT_R16:
            d3d_format = D3DFMT_R16F;
            bytes_per_pixel = 2;
        break;
        
        case RM_FORMAT_RG16:
            d3d_format = D3DFMT_G16R16F;
            bytes_per_pixel = 4; 
        break;
        
        case RM_FORMAT_RGBA16:
            d3d_format = D3DFMT_A16B16G16R16F;
            bytes_per_pixel = 8;
        break;
        
        case RM_FORMAT_R32:
            d3d_format = D3DFMT_R32F;
            bytes_per_pixel = 4;
        break;
        
        case RM_FORMAT_RG32:
            d3d_format = D3DFMT_G32R32F;
            bytes_per_pixel = 8;
        break;
        
        case RM_FORMAT_RGB8:
        case RM_FORMAT_RGB16:
        case RM_FORMAT_RGB32:
            assert(!"This is rarely used in D3D 9.");
        break;
        
        case RM_FORMAT_RGBA32:
            d3d_format = D3DFMT_A32B32G32R32F;
            bytes_per_pixel = 16;
        break;
        
        case RM_FORMAT_DEPTH16:
            d3d_format = D3DFMT_D16;
            bytes_per_pixel = 2;
        break;
        
        case RM_FORMAT_DEPTH32:
            d3d_format = D3DFMT_D32;
            bytes_per_pixel = 4;
        break;
        
        case RM_FORMAT_DEPTH24_STENCIL8: 
            d3d_format = D3DFMT_D24S8;
            bytes_per_pixel = 4;
        break;
    }

    // @Todo: Convert RGBA to BGRA, because RGBA is not well supported 
    // by D3D9 devices.
    
    if (z == 1) {  // 2D Texture.
        // @Note: Textures created with D3DPOOL_DEFAULT are not lockable.
        hr = IDirect3DDevice9_CreateTexture(rm_state.d3d_device,
                                            x, y,
                                            1,
                                            0,
                                            d3d_format,
                                            D3DPOOL_MANAGED,
                                            &texture,
                                            null);
        if (hr != D3D_OK) {
            nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to create the texture.");
            nb_log_print(NB_LOG_ERROR, "D3D9", "%s", d3d_hresult_to_message(hr));
            return result;
        }
    } else if (z == RM_CREATE_CUBE_MAP) {

    }

    assert(texture != null);

    if (data) {
        if (IDirect3DTexture9_LockRect(texture, 0, &locked_rect, NULL, 0) == D3D_OK) {
            u32 texture_pitch = x * bytes_per_pixel;

            for (u32 row = 0; row < y; ++row) {
                memcpy((u8 *)locked_rect.pBits + row * locked_rect.Pitch,
                       (u8 *)data + row * texture_pitch,
                       texture_pitch);
            }

            IDirect3DTexture9_UnlockRect(texture, 0);
        }
    } else {
        if (IDirect3DTexture9_LockRect(texture, 0, &locked_rect, NULL, 0) == D3D_OK) {
            u8 *buffer = (u8 *)locked_rect.pBits;
            u8 *dest;
            float *f_dest;

            for (u32 j = 0; j < y; ++j) {
                for (u32 i = 0; i < x; ++i) {
                    switch (format) {
                        case RM_FORMAT_R8:
                            dest = buffer + j * locked_rect.Pitch + i * bytes_per_pixel;
                            if ((i+j) % 2) {
                                *dest = 0;
                            } else {
                                *dest = 0xFF;
                            }
                        break;

                        case RM_FORMAT_RG8:
                            dest = buffer + j * locked_rect.Pitch + i * bytes_per_pixel;
                            if ((i+j) % 2) {
                                dest[0] = 0;
                                dest[1] = 0xFF;
                            } else {
                                dest[0] = 0xFF;
                                dest[1] = 0xFF;
                            }
                        break;

                        case RM_FORMAT_RGBA8:
                            dest = buffer + j * locked_rect.Pitch + i * bytes_per_pixel;
                            if ((i+j) % 2) {
                                dest[0] = 0;
                                dest[1] = 0;
                                dest[2] = 0;
                                dest[3] = 0xFF;
                            } else {
                                dest[0] = 0xFF;
                                dest[1] = 0xFF;
                                dest[2] = 0xFF;
                                dest[3] = 0xFF;
                            }
                        break;
                        
                        case RM_FORMAT_R16:
                            dest = buffer + j * locked_rect.Pitch + i * bytes_per_pixel;
                            // During sampling D3D9 expands this to 4 component vector (r, 0, 0, 1).
                            if ((i+j) % 2) {
                                dest[0] = 0;
                                dest[1] = 0;
                            } else {
                                dest[0] = 0;
                                dest[1] = 0x3C;
                            }
                        break;
                        
                        case RM_FORMAT_RG16:
                            dest = buffer + j * locked_rect.Pitch + i * bytes_per_pixel;
                            if ((i+j) % 2) {
                                dest[0] = 0;
                                dest[1] = 0;
                                dest[2] = 0;
                                dest[3] = 0;
                            } else {
                                dest[0] = 0;
                                dest[1] = 0x3C;
                                dest[2] = 0;
                                dest[3] = 0x3C;
                            }
                        break;
                        
                        case RM_FORMAT_RGB8:
                        case RM_FORMAT_RGB16:
                        case RM_FORMAT_RGB32:
                            assert(!"This is rarely used in D3D 9.");
                        break;

                        case RM_FORMAT_RGBA16:
                            dest = buffer + j * locked_rect.Pitch + i * bytes_per_pixel;
                            if ((i+j) % 2) {
                                dest[0] = 0;
                                dest[1] = 0;
                                dest[2] = 0;
                                dest[3] = 0;
                                dest[4] = 0;
                                dest[5] = 0;
                                dest[6] = 0;
                                dest[7] = 0x3C;
                            } else {
                                dest[0] = 0;
                                dest[1] = 0x3C;
                                dest[2] = 0;
                                dest[3] = 0x3C;
                                dest[4] = 0;
                                dest[5] = 0x3C;
                                dest[6] = 0;
                                dest[7] = 0x3C;
                            }
                        break;
                        
                        
                        case RM_FORMAT_R32:
                            f_dest = (float *)(buffer + j * locked_rect.Pitch + i * bytes_per_pixel);
                            if ((i+j) % 2) {
                                *f_dest = 0;
                            } else {
                                *f_dest = 1.0f;
                            }
                        break;
                        
                        case RM_FORMAT_RG32:
                            f_dest = (float *)(buffer + j * locked_rect.Pitch + i * bytes_per_pixel);
                            if ((i+j) % 2) {
                                f_dest[0] = 1.0f;
                                f_dest[1] = 0;
                            } else {
                                f_dest[0] = 1.0f;
                                f_dest[1] = 1.0f;
                            }
                        break;
                        
                        case RM_FORMAT_RGBA32:
                            f_dest = (float *)(buffer + j * locked_rect.Pitch + i * bytes_per_pixel);
                            if ((i+j) % 2) {
                                f_dest[0] = 0;
                                f_dest[1] = 0;
                                f_dest[2] = 0;
                                f_dest[3] = 1.0f;
                            } else {
                                f_dest[0] = 1.0f;
                                f_dest[1] = 1.0f;
                                f_dest[2] = 1.0f;
                                f_dest[3] = 1.0f;
                            }
                        break;
                    }
                }
            }

            IDirect3DTexture9_UnlockRect(texture, 0);
        }
    }

    if (!rm_state.texture_pointers || !rm_state.texture_pointer_allocated) {
        rm_state.texture_pointer_allocated = 8;
        rm_state.texture_pointer_count = 1;
        rm_state.texture_pointers = nb_new_array(RM_Texture9, rm_state.texture_pointer_allocated);
    }

    if (rm_state.texture_pointer_count != 0) {
        for (u32 index = 0; index < rm_state.texture_pointer_count; ++index) {
            IDirect3DTexture9 *ptr = rm_state.texture_pointers[index].pointer;
            if (ptr == null) {
                result = index;
                break;
            }
        }
    }

    if (rm_state.texture_pointer_count >= rm_state.texture_pointer_allocated) {
        u32 old_size = rm_state.texture_pointer_allocated * size_of(RM_Texture9);
        rm_state.texture_pointer_allocated *= 2;
        rm_state.texture_pointers = (RM_Texture9 *)nb_realloc(rm_state.texture_pointers,
                                       rm_state.texture_pointer_allocated*size_of(RM_Texture9),
                                       old_size);
    }

    if (result == -1) {
        result = rm_state.texture_pointer_count;
        rm_state.texture_pointer_count += 1;
    }

    assert(result != -1);
    RM_Texture9 *t9 = rm_state.texture_pointers + result;
    t9->pointer = texture;

    if (filter) {
        t9->min_filter = D3DTEXF_LINEAR;
        t9->mag_filter = D3DTEXF_LINEAR;
    } else {
        t9->min_filter = D3DTEXF_POINT;
        t9->mag_filter = D3DTEXF_POINT;
    }

    if (wrap) {
        t9->wrap = D3DTADDRESS_WRAP;
    } else {
        t9->wrap = D3DTADDRESS_CLAMP;
    }

    return result;
}

NB_EXTERN void rm_texture_free(u32 texture_id) {
    IDirect3DTexture9 *texture;

    assert(texture_id != -1);
    texture = rm_state.texture_pointers[texture_id].pointer;
    rm_state.texture_pointers[texture_id].pointer = null;

    IDirect3DTexture9_Release(texture);
}

static void d3d_copy_texture_region(void *dest, void *src, u32 dest_pitch, u32 src_pitch, u32 width, u32 height) {
    UNUSED(width);
    u32 row;

    for (row = 0; row < height; ++row) {
        memcpy((u8 *)dest + row * dest_pitch,
               (u8 *)src  + row * src_pitch,
               src_pitch);
    }
}

NB_EXTERN void 
rm_texture_update(u32 texture_id, Renderman_Format format, 
                  u32 x_offset, u32 y_offset, u32 z_offset, 
                  u32 x, u32 y, u32 z, void *data) {
    UNUSED(z);
    UNUSED(z_offset);

    IDirect3DTexture9 *texture;
    D3DLOCKED_RECT locked_rect;
    RECT updated_rect;
    u32 bytes_per_pixel;

    assert(texture_id != -1);
    texture = rm_state.texture_pointers[texture_id].pointer;

    bytes_per_pixel = 0;

    switch (format) {
        case RM_FORMAT_R8:
            bytes_per_pixel = 1;
        break;
        
        case RM_FORMAT_RG8:
        case RM_FORMAT_R16:
        case RM_FORMAT_DEPTH16:
            bytes_per_pixel = 2;
        break;
        
        case RM_FORMAT_RGBA8:
        case RM_FORMAT_RG16:
        case RM_FORMAT_R32:
        case RM_FORMAT_DEPTH32:
        case RM_FORMAT_DEPTH24_STENCIL8: 
            bytes_per_pixel = 4;
        break;
        
        case RM_FORMAT_RGBA16:
        case RM_FORMAT_RG32:
            bytes_per_pixel = 8;
        break;
        
        case RM_FORMAT_RGB8:
        case RM_FORMAT_RGB16:
        case RM_FORMAT_RGB32:
            assert(!"This is rarely used in D3D 9.");
        break;
        
        case RM_FORMAT_RGBA32:
            bytes_per_pixel = 16;
        break;
    }

    updated_rect.left   = x_offset;
    updated_rect.right  = x_offset + x;
    updated_rect.top    = y_offset;
    updated_rect.bottom = y_offset + y;

    if (IDirect3DTexture9_LockRect(texture, 0, &locked_rect, &updated_rect, 0) == D3D_OK) {
        d3d_copy_texture_region(locked_rect.pBits, data, locked_rect.Pitch, x * bytes_per_pixel, x, y);

        IDirect3DTexture9_UnlockRect(texture, 0);
    }
}
