#include "renderman.h"

// Enable extra D3D debugging in debug builds if using the debug DirectX runtime.  
// This makes D3D objects work well in the debugger watch window, but slows down 
// performance slightly.
#if defined(NB_DEBUG) || defined(DEBUG) | defined(_DEBUG)
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

#include <d3d9.h>

#pragma warning(push)
#pragma warning(disable:4115)
#include <d3dcompiler.h>
#pragma warning(pop)

#if COMPILER_CL
// #pragma comment(lib, "d3d9.lib")
// #pragma comment(lib, "d3dcompiler.lib")
#endif

typedef IDirect3D9 * WINAPI Direct3DCreate9PROC(UINT SDKVersion);

#include "dxerr.h"

typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
} Immediate_Vertex;

#define MAX_IMMEDIATE_VERTICES 2048

typedef enum {
    RM_GRAPHICS_CARD_UNKNOWN = 0,
    RM_GRAPHICS_CARD_ATI,
    RM_GRAPHICS_CARD_NVIDIA,
    RM_GRAPHICS_CARD_INTEL,
} RM_Graphics_Card_Type;

typedef enum {
    RM_SC_NONE = 0,
    
    RM_SC_DEPTH_TEST = 0x1,
    RM_SC_CULL_MODE  = 0x2,
    RM_SC_FILL_MODE  = 0x4,
    RM_SC_BLEND_MODE = 0x8,
    RM_SC_ALPHA_TO_COVERAGE = 0x10,
    RM_SC_COLOR_MASK        = 0x20,
    RM_SC_DEPTH_STENCIL     = 0x40,

    RM_SC_ALL_RENDER_STATES = RM_SC_DEPTH_TEST|RM_SC_CULL_MODE|RM_SC_FILL_MODE|
                              RM_SC_BLEND_MODE|RM_SC_ALPHA_TO_COVERAGE|
                              RM_SC_COLOR_MASK|RM_SC_DEPTH_STENCIL,
} RM_State_Change;

typedef struct {
    IDirect3DTexture9 *pointer;
    u32 min_filter;
    u32 mag_filter;
    u32 wrap;
} RM_Texture9;

typedef struct {
    u32 stencil_func;
    u32 reference;
    u32 read_mask;
    u32 write_mask;
    u32 stencil_fail_op;
    u32 depth_fail_op;
    u32 success_op;
} RMStencil_State;

typedef struct {
    u32 depth_func;
    u32 cull_mode;
    u32 fill_mode;
    
    u32 blend_op;
    u32 blend_src;
    u32 blend_dest;

    RMStencil_State stencil[2]; // Two states: front & back.
    
    bool color_mask[4];
    bool depth_write;
    bool depth_test;
    bool alpha_to_coverage;
    bool stencil_enabled;
    bool blend_enabled;
} RMShader_State;

struct RMShader {
    char name[32];
    IDirect3DVertexShader9 *vs;
    IDirect3DPixelShader9  *ps;

    RMShader_State state;
};

typedef struct {
    HMODULE d3d_module;
    HMODULE d3d_compiler_module;
    Direct3DCreate9PROC *direct3d_create9;
    pD3DCompile          d3d_compile;

    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d_device;
    IDirect3DVertexBuffer9      *immediate_vb;
    IDirect3DVertexDeclaration9 *d3d_vertex_layout;
    D3DPRESENT_PARAMETERS       d3d_params;
    D3DCAPS9                    d3d_caps;

    u32 state_flags;

    RMShader *current_shader;
    RMShader *argb_texture_shader;

    RM_Texture9 *texture_pointers;
    u32 texture_pointer_allocated;
    u32 texture_pointer_count;
    u32 bound_texture_ids[16];

    u32 num_immediate_vertices;
    Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];

    float pixels_to_proj_matrix[4][4];

    RM_Graphics_Card_Type graphics_card_type;
    u32 available_texture_memory;
    
    bool d3d_device_lost;
    bool vertex_hw_processing_enabled;
    bool has_rgba_support;
    bool has_alpha_to_coverage_support;
    bool has_twosided_stencil_support;
} Renderman_State;

static Renderman_State rm_state;
static bool rm_initted;


char rm_vertex_shader_source[] = NB_STRINGIFY(
float4x4 wvp : register(c0);\n
\n
struct VS_Output {\n
    float4 pos   : POSITION;\n
    float2 uv    : TEXCOORD0;\n
    float4 color : COLOR;\n
};\n
\n
VS_Output main(float3 pos : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR) {\n
    VS_Output result;\n
    result.pos   = mul(float4(pos, 1.0f), wvp);\n
    result.uv    = uv;\n
    result.color = color;\n
    return result;\n
}\n
);


// @Todo: Account for half pixel offset.
// https://aras-p.info/blog/2016/04/08/solving-dx9-half-pixel-offset/

char rm_pixel_shader_source[] = NB_STRINGIFY(
sampler2D rm_sampler : register(s0);\n
\n
struct VS_Output {\n
    float4 pos   : POSITION;\n
    float2 uv    : TEXCOORD0;\n
    float4 color : COLOR;\n
};\n
\n
float4 main(VS_Output input) : COLOR {\n
    float4 texel = tex2D(rm_sampler, input.uv);\n
    return texel * input.color;\n
}\n
);


static void rm_shader_state_init(RMShader_State *state) {
    int i;

    state->depth_test = false;
    state->depth_func = D3DCMP_NEVER;
    state->cull_mode  = D3DCULL_CW;
    state->fill_mode  = D3DFILL_SOLID;

    state->blend_op   = D3DBLENDOP_ADD;
    state->blend_src  = D3DBLEND_SRCALPHA;
    state->blend_dest = D3DBLEND_INVSRCALPHA;

    for (i = 0; i < 2; ++i) {
        state->stencil[i].stencil_func    = D3DCMP_ALWAYS;
        state->stencil[i].reference       = 0;
        state->stencil[i].read_mask       = 0;
        state->stencil[i].write_mask      = 0;
        state->stencil[i].stencil_fail_op = D3DSTENCILOP_KEEP;
        state->stencil[i].depth_fail_op   = D3DSTENCILOP_KEEP;
        state->stencil[i].success_op      = D3DSTENCILOP_KEEP;
    }

    state->color_mask[0] = false;
    state->color_mask[1] = false;
    state->color_mask[2] = false;
    state->color_mask[3] = false;
    state->depth_write   = true;

    state->alpha_to_coverage = false;
    state->stencil_enabled   = false;
    state->blend_enabled     = false;
}


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

#if 0
static void d3d_default_device_state_set(void) {
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

    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(rm_state.d3d_device, 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);

    IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
}
#endif

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

    rm_state.argb_texture_shader = rm_shader_create(rm_vertex_shader_source, rm_pixel_shader_source, "ARGB Texture");
    // rm_shader_set(rm_state.argb_texture_shader);

    // rm_shader_state_set_depth_test(rm_state.argb_texture_shader, 0);

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

    if (rm_state.argb_texture_shader) {
        rm_shader_free(rm_state.argb_texture_shader);
        rm_state.argb_texture_shader = null;
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

    rm_state.d3d_module = LoadLibraryW(L"d3d9.dll");
    if (!rm_state.d3d_module) {
        Log("Failed to load d3d9.");
        return false;
    }

    rm_state.direct3d_create9 = (Direct3DCreate9PROC *)GetProcAddress(rm_state.d3d_module, "Direct3DCreate9");
    if (!rm_state.direct3d_create9) {
        Log("Failed to get Direct3DCreate9.");
        return false;
    }

    rm_state.d3d9 = rm_state.direct3d_create9(D3D_SDK_VERSION);
    if (!rm_state.d3d9) {
        Log("Failed to Direct3DCreate9.");
        return false;
    }

    HRESULT hr;
    UINT adapter = D3DADAPTER_DEFAULT;
    D3DFORMAT tested_formats[] = {D3DFMT_UNKNOWN, D3DFMT_X8R8G8B8, D3DFMT_A8R8G8B8};
    D3DFORMAT format = tested_formats[1];

    // Check the device capabilities.
    hr = IDirect3D9_GetDeviceCaps(rm_state.d3d9, adapter, D3DDEVTYPE_HAL, &rm_state.d3d_caps);
    if (FAILED(hr)) {
        Log("Failed to IDirect3D9_GetDeviceCaps.");
        Log("%s", d3d_hresult_to_message(hr));
        return false;
    }
    
    rm_state.vertex_hw_processing_enabled = (rm_state.d3d_caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;
    rm_state.has_twosided_stencil_support = (rm_state.d3d_caps.DevCaps & D3DSTENCILCAPS_TWOSIDED) != 0;

    HWND hwnd = (HWND)bender_get_window_handle(window_id);

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

        switch (identifier.VendorId) {
            case 0x1002:
                rm_state.graphics_card_type = RM_GRAPHICS_CARD_ATI;
            break;

            case 0x10DE:
                rm_state.graphics_card_type = RM_GRAPHICS_CARD_NVIDIA;
            break;

            case 0x8086:
                rm_state.graphics_card_type = RM_GRAPHICS_CARD_INTEL;
            break;

            default:
                rm_state.graphics_card_type = RM_GRAPHICS_CARD_UNKNOWN;
                nb_log_print(NB_LOG_WARNING, "D3D9", "Uknown vender id: %u", identifier.VendorId);
            break;
        }
    }

    rm_state.d3d_device = d3d_device_create(D3DADAPTER_DEFAULT, hwnd);
    if (!rm_state.d3d_device) return false;

    rm_state.has_rgba_support = d3d_check_format_support(rm_state.d3d_device,
                                                         D3DFMT_A8B8G8R8);

    rm_state.has_alpha_to_coverage_support = false;
    hr = IDirect3D9_CheckDeviceFormat(rm_state.d3d9, 
                                      adapter, D3DDEVTYPE_HAL, 
                                      format, 0, D3DRTYPE_SURFACE,
                                      (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));  // NVidia check.
    if (hr == D3D_OK) {
        rm_state.has_alpha_to_coverage_support = true;
    }

    rm_state.available_texture_memory = IDirect3DDevice9_GetAvailableTextureMem(rm_state.d3d_device);
    nb_log_print(NB_LOG_NONE, "D3D9", "Available Texture Memory: %u MB",
                 rm_state.available_texture_memory/1024/1024);


    rm_state.d3d_compiler_module = LoadLibraryW(L"d3dcompiler_47.dll");
    if (!rm_state.d3d_compiler_module) {
        Log("Failed to load d3dcompiler_47.dll, attemting to load d3dcompiler_43.dll");
        rm_state.d3d_compiler_module = LoadLibraryW(L"d3dcompiler_43.dll");
        if (!rm_state.d3d_compiler_module) {
            Log("Failed to load d3dcompiler_43.dll");
            return false;
        }
    }

    rm_state.d3d_compile = (pD3DCompile)GetProcAddress(rm_state.d3d_compiler_module, "D3DCompile");
    if (!rm_state.d3d_compile) {
        Log("Failed to get D3DCompile.");
        return false;
    }


    rm_state.current_shader = null;
    // rm_shader_state_init(&rm_state.current_state);

    if (!d3d_immediate_mode_init()) {
        return false;
    }

    // d3d_default_device_state_set();

    for (u32 index = 0; index < nb_array_count(rm_state.bound_texture_ids); ++index) {
        rm_state.bound_texture_ids[index] = (u32)-1;
    }

    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width  = rm_state.d3d_params.BackBufferWidth;
    vp.Height = rm_state.d3d_params.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    IDirect3DDevice9_SetViewport(rm_state.d3d_device, &vp);

    // We make the state dirty to bind all 
    // of our states on the first frame.
    rm_state.state_flags = RM_SC_ALL_RENDER_STATES;


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


    // d3d_default_device_state_set();

    rm_state.state_flags = RM_SC_ALL_RENDER_STATES;
    rm_state.current_shader = null;
    for (u32 index = 0; index < nb_array_count(rm_state.bound_texture_ids); ++index) {
        rm_state.bound_texture_ids[index] = (u32)-1;
    }
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

NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a,
                                      bool depth, bool stencil) {
    u32 clear_flags = D3DCLEAR_TARGET;
    if (depth) clear_flags |= D3DCLEAR_ZBUFFER;
    if (stencil) clear_flags |= D3DCLEAR_STENCIL;
    IDirect3DDevice9_Clear(rm_state.d3d_device, 0, null, 
                           clear_flags,
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
            // rm_shader_state_set_cull_mode(rm_state.argb_texture_shader, RM_CW);
            // rm_shader_state_set_blend_mode(rm_state.argb_texture_shader, RM_ADD, RM_SRC_ALPHA, RM_ONE_MINUS_SRC_ALPHA);


            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZENABLE, FALSE);
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SCISSORTESTENABLE, FALSE);


            // rm_shader_texture_set(rm_state.argb_texture_shader, /*slot=*/0, /*texture_id=*/0);


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
    if (rm_state.num_immediate_vertices > MAX_IMMEDIATE_VERTICES - 6) {
        nb_write_string("rm_immediate_frame_end\n", false);
        rm_immediate_frame_end();
    }

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
                                dest[0] = 0xCC;
                                dest[1] = 0xCC;
                                dest[2] = 0xCC;
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


NB_EXTERN RMShader *
rm_shader_create(const char *vertex_shader_source,
                                 const char *pixel_shader_source,
                                 const char *shader_name) {
    RMShader *shader = null;
    ID3DBlob *compiled_shader = null, *shader_error = null;
    HRESULT hr;
    IDirect3DVertexShader9 *vs;
    IDirect3DPixelShader9  *ps;
    DWORD *shader_data;
    u32 index;

    if (shader_name) {
        assert(nb_string_length(shader_name) <= 32);
    } else {
        shader_name = "Unnamed";
    }

    UINT compile_flags = 0;
#if NB_DEBUG
    compile_flags |= D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Vertex shader.
    hr = rm_state.d3d_compile(vertex_shader_source, 
                    nb_string_length(vertex_shader_source),
                    /*pSourceName=*/shader_name, 
                    /*pDefines=*/null,
                    /*pInclude=*/null,
                    "main",
                    "vs_3_0",
                    compile_flags,
                    0,
                    &compiled_shader,
                    &shader_error);
    if (FAILED(hr)) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to compile the vertex shader.");
        nb_log_print(NB_LOG_ERROR, "D3D9", "%s", d3d_hresult_to_message(hr));

        d3d_log_shader_error(shader_error);

        return shader;
    }

#if LANGUAGE_C
    shader_data = (DWORD *)(compiled_shader->lpVtbl->GetBufferPointer(compiled_shader));//ID3D10Blob_GetBufferPointer(compiled_shader);
#else
    shader_data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreateVertexShader(rm_state.d3d_device,
                                             shader_data,
                                             &vs);
    if (FAILED(hr)) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to IDirect3DDevice9_CreateVertexShader.");
        nb_log_print(NB_LOG_ERROR, "D3D9", "%s", d3d_hresult_to_message(hr));
        return shader;
    }

#if LANGUAGE_C
    compiled_shader->lpVtbl->Release(compiled_shader);
#else
    compiled_shader->Release();
#endif

    // Pixel shader.
    compiled_shader = null;
    shader_error    = null;
    hr = rm_state.d3d_compile(pixel_shader_source, 
                    nb_string_length(pixel_shader_source),
                    /*pSourceName=*/shader_name, 
                    /*pDefines=*/null,
                    /*pInclude=*/null,
                    "main",
                    "ps_3_0",
                    compile_flags,
                    0,
                    &compiled_shader,
                    &shader_error);
    if (FAILED(hr)) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to compile the pixel shader.");
        nb_log_print(NB_LOG_ERROR, "D3D9", "%s", d3d_hresult_to_message(hr));

        d3d_log_shader_error(shader_error);

        return shader;
    }

#if LANGUAGE_C
    shader_data = (DWORD *)(compiled_shader->lpVtbl->GetBufferPointer(compiled_shader));
#else
    shader_data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreatePixelShader(rm_state.d3d_device,
                                            shader_data,
                                            &ps);
    if (FAILED(hr)) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to IDirect3DDevice9_CreatePixelShader.");
        nb_log_print(NB_LOG_ERROR, "D3D9", "%s", d3d_hresult_to_message(hr));
        return false;
    }

#if LANGUAGE_C
    compiled_shader->lpVtbl->Release(compiled_shader);
#else
    compiled_shader->Release();
#endif

    shader = New(RMShader);
    
    for (index = 0; (index < 32) && (shader_name[index] != 0); ++index) {
        shader->name[index] = shader_name[index];
    }
    shader->name[index] = 0;

    shader->vs = vs;
    shader->ps = ps;

    rm_shader_state_init(&shader->state);

    return shader;
}

NB_EXTERN RMShader *rm_shader_create_from_file(const char *vertex_shader_path,
                                               const char *pixel_shader_path,
                                               const char *shader_name) {
    char *vertex_shader_source, *pixel_shader_source;
    FILE *file;
    size_t size;
    RMShader *result;

    file = fopen(vertex_shader_path, "r");
    if (file == null) return null;

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);

    vertex_shader_source = nb_new_array(char, size + 1);
    size = fread(vertex_shader_source, 1, size, file);
    fclose(file);
    vertex_shader_source[size] = 0;


    file = fopen(pixel_shader_path, "r");
    if (file == null) return null;

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);

    pixel_shader_source = nb_new_array(char, size + 1);
    size = fread(pixel_shader_source, 1, size, file);
    fclose(file);
    pixel_shader_source[size] = 0;


    result = rm_shader_create(vertex_shader_source, 
                              pixel_shader_source, 
                              shader_name);

    nb_free(vertex_shader_source);
    nb_free(pixel_shader_source);

    return result;
}

NB_EXTERN void rm_shader_free(RMShader *shader) {
    assert(shader != null);

    if (shader->vs) IDirect3DVertexShader9_Release(shader->vs);
    if (shader->ps) IDirect3DPixelShader9_Release(shader->ps);

    nb_free(shader);
}

#if 0
static void 
rm_shader_state_set(RMShader_State *state) {
    static u32 d3d9_fill_modes[3] = {D3DFILL_SOLID, D3DFILL_WIREFRAME, D3DFILL_POINT};
    DWORD mask_flags = 0;
    
    if ((state->depth_test != rm_state.current_state.depth_test) || rm_state.force_shader_rebind) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZENABLE, (state->depth_test != 0));
        if (state->depth_test != 0) {
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZFUNC, state->depth_test);
        }
        print("rm_shader_state_set depth\n");
    }

    if ((state->cull_mode != rm_state.current_state.cull_mode) || rm_state.force_shader_rebind) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_CULLMODE, D3DCULL_NONE + state->cull_mode);
        print("rm_shader_state_set cull\n");
    }

    if ((state->fill_mode != rm_state.current_state.fill_mode) || rm_state.force_shader_rebind) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_FILLMODE, d3d9_fill_modes[state->fill_mode]);
        print("rm_shader_state_set fill\n");
    }

    if (state->blend_op   != rm_state.current_state.blend_op  ||
        state->blend_src  != rm_state.current_state.blend_src ||
        state->blend_dest != rm_state.current_state.blend_dest || rm_state.force_shader_rebind) {
        // Enable blending.
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ALPHABLENDENABLE, (state->blend_op != 0));
        if (state->blend_op) {
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_SRCBLEND, state->blend_src);
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_DESTBLEND, state->blend_dest);

            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_BLENDOP, state->blend_op);
        }
        print("rm_shader_state_set blend\n");
    }

    if (state->alpha_to_coverage != rm_state.current_state.alpha_to_coverage || rm_state.force_shader_rebind) {
        if (rm_state.has_alpha_to_coverage_support) {
            if (state->alpha_to_coverage) {
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ADAPTIVETESS_Y,
                                                (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));

                // AMD code has not been tested.
                // IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_POINTSIZE, MAKEFOURCC('A','2','M','1'));
            } else {
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ADAPTIVETESS_Y,
                                                D3DFMT_UNKNOWN);
            }
            print("rm_shader_state_set alpha_to_coverage\n");
        }
    }

    if (state->color_mask[0] != rm_state.current_state.color_mask[0] ||
        state->color_mask[1] != rm_state.current_state.color_mask[1] ||
        state->color_mask[2] != rm_state.current_state.color_mask[2] ||
        state->color_mask[3] != rm_state.current_state.color_mask[3] ||
        state->depth_write   != rm_state.current_state.depth_write   || 
        rm_state.force_shader_rebind) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_ZWRITEENABLE, state->depth_write);

        if (state->color_mask[0]) mask_flags |= D3DCOLORWRITEENABLE_RED;
        if (state->color_mask[1]) mask_flags |= D3DCOLORWRITEENABLE_GREEN;
        if (state->color_mask[2]) mask_flags |= D3DCOLORWRITEENABLE_BLUE;
        if (state->color_mask[3]) mask_flags |= D3DCOLORWRITEENABLE_ALPHA;
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_COLORWRITEENABLE, mask_flags);
        print("rm_shader_state_set mask\n");
    }

    // @Todo: Add stencil state...

    rm_state.current_state = *state;
}
#endif

NB_EXTERN void rm_shader_set(RMShader *shader) {
    if (shader == rm_state.current_shader) return;

    rm_state.current_shader = shader;

    if (shader == null) {
        IDirect3DDevice9_SetVertexShader(rm_state.d3d_device, null);
        IDirect3DDevice9_SetPixelShader(rm_state.d3d_device, null);
        return;
    }

    IDirect3DDevice9_SetVertexShader(rm_state.d3d_device, shader->vs);
    IDirect3DDevice9_SetPixelShader(rm_state.d3d_device, shader->ps);
}

NB_EXTERN void rm_shader_texture_set(RMShader *shader, u32 slot, u32 texture_id) {
    RM_Texture9 *t9;

    assert(slot != -1);

    if (texture_id == -1) {
        IDirect3DDevice9_SetTexture(rm_state.d3d_device, slot, null);
        rm_state.bound_texture_ids[slot] = texture_id;
        return;
    }

    if (shader == rm_state.current_shader) {
        if (rm_state.bound_texture_ids[slot] != texture_id) {
            t9 = rm_state.texture_pointers + texture_id;

            IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_MINFILTER, t9->min_filter);
            IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_MAGFILTER, t9->mag_filter);
            IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_ADDRESSU,  t9->wrap);
            IDirect3DDevice9_SetSamplerState(rm_state.d3d_device, 0, D3DSAMP_ADDRESSV,  t9->wrap);

            IDirect3DDevice9_SetTexture(rm_state.d3d_device, slot, (IDirect3DBaseTexture9 *)(void *)(t9->pointer));

            rm_state.bound_texture_ids[slot] = texture_id;
        }
    }
}

NB_EXTERN RMShader *rm_render_presets_get(RM_Presets preset) {
    RMShader *presets[RM_PRESET_COUNT] = {rm_state.argb_texture_shader};

    return presets[preset];
}


NB_EXTERN void 
rm_shader_state_set_depth_test(RMShader *shader, u32 depth_test) {
    static u32 d3d_cmp_funcs[] = {
        0, D3DCMP_NEVER, D3DCMP_LESS, D3DCMP_EQUAL, D3DCMP_LESSEQUAL,
        D3DCMP_GREATER, D3DCMP_NOTEQUAL, D3DCMP_GREATEREQUAL, D3DCMP_ALWAYS,
    };
    u32 depth_func = 0;
    bool depth_enabled = false;

    assert(depth_test < nb_array_count(d3d_cmp_funcs));

    depth_enabled = (depth_test != 0);
    if (depth_enabled)
        depth_func = d3d_cmp_funcs[depth_test];

    if ((depth_enabled != shader->state.depth_test) ||
        (depth_func != shader->state.depth_func)) {
        rm_state.state_flags |= RM_SC_DEPTH_TEST;
    }

    // Only apply changes if the shader is currently bound.
    if ((shader == rm_state.current_shader) &&
        (rm_state.state_flags & RM_SC_DEPTH_TEST)) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                        D3DRS_ZENABLE, 
                                        depth_enabled);
        if (depth_enabled) {
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                            D3DRS_ZFUNC, 
                                            depth_func);
        }
        printf("rm_shader_state_set_depth_test\n");

        rm_state.state_flags &= ~RM_SC_DEPTH_TEST;
    }

    shader->state.depth_func = depth_func;
    shader->state.depth_test = depth_enabled;
}

NB_EXTERN void rm_shader_state_set_cull_mode(RMShader *shader, u32 cull_mode) {
    static u32 d3d_cull_modes[] = {D3DCULL_NONE, D3DCULL_CW, D3DCULL_CCW};
    assert(cull_mode < nb_array_count(d3d_cull_modes));
    u32 d3d_cull_mode = d3d_cull_modes[cull_mode];

    if (d3d_cull_mode != shader->state.cull_mode)
        rm_state.state_flags |= RM_SC_CULL_MODE;

    // Only apply changes if the shader is currently bound.
    if ((shader == rm_state.current_shader) &&
        (rm_state.state_flags & RM_SC_CULL_MODE)) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                        D3DRS_CULLMODE, 
                                        d3d_cull_mode);
        printf("rm_shader_state_set_cull_mode\n");

        rm_state.state_flags &= ~RM_SC_CULL_MODE;
    }

    shader->state.cull_mode = d3d_cull_mode;
}

NB_EXTERN void rm_shader_state_set_fill_mode(RMShader *shader, u32 fill_mode) {
    static u32 d3d9_fill_modes[3] = {D3DFILL_SOLID, D3DFILL_WIREFRAME, D3DFILL_POINT};
    assert(fill_mode < nb_array_count(d3d9_fill_modes));
    u32 d3d_fill_mode = d3d9_fill_modes[fill_mode];

    if (d3d_fill_mode != shader->state.fill_mode)
        rm_state.state_flags |= RM_SC_FILL_MODE;

    // Only apply changes if the shader is currently bound.
    if ((shader == rm_state.current_shader) && 
        (rm_state.state_flags & RM_SC_FILL_MODE)) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                        D3DRS_FILLMODE, 
                                        d3d_fill_mode);
        printf("rm_shader_state_set_fill_mode\n");

        rm_state.state_flags &= ~RM_SC_FILL_MODE;
    }

    shader->state.fill_mode = d3d_fill_mode;
}

NB_EXTERN void 
rm_shader_state_set_blend_mode(RMShader *shader, 
                               bool enable,
                               u32 blend_op, 
                               u32 blend_src, 
                               u32 blend_dest) {
    static u32 d3d_blend_ops[] = {0, D3DBLENDOP_ADD, D3DBLENDOP_SUBTRACT,
    D3DBLENDOP_REVSUBTRACT, D3DBLENDOP_MIN, D3DBLENDOP_MAX};
    static u32 d3d_blend_modes[] = {0, D3DBLEND_ZERO , D3DBLEND_ONE, 
        D3DBLEND_SRCCOLOR, D3DBLEND_INVSRCCOLOR, D3DBLEND_SRCALPHA,
        D3DBLEND_INVSRCALPHA, D3DBLEND_DESTALPHA, D3DBLEND_INVDESTALPHA,
        D3DBLEND_DESTCOLOR, D3DBLEND_INVDESTCOLOR};
    u32 d3d_blend_op = 0, d3d_blend_src = 0, d3d_blend_dest = 0;

    if (enable) {
        assert(blend_op   < nb_array_count(d3d_blend_ops));
        assert(blend_src  < nb_array_count(d3d_blend_modes));
        assert(blend_dest < nb_array_count(d3d_blend_modes));

        d3d_blend_op   = d3d_blend_ops[blend_op];
        d3d_blend_src  = d3d_blend_modes[blend_src];
        d3d_blend_dest = d3d_blend_modes[blend_dest];
    }

    if ((enable != shader->state.blend_enabled)    ||
        (d3d_blend_op != shader->state.blend_op)   ||
        (d3d_blend_src != shader->state.blend_src) ||
        (d3d_blend_dest != shader->state.blend_dest)) {
        rm_state.state_flags |= RM_SC_BLEND_MODE;
    }

    if ((shader == rm_state.current_shader) &&
        (rm_state.state_flags & RM_SC_BLEND_MODE)) {
        // Enable blending.
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                        D3DRS_ALPHABLENDENABLE, 
                                        enable);
        if (enable) {
            assert(d3d_blend_src  > 0); // Specified blend op but not the blend src.
            assert(d3d_blend_dest > 0); // Specified blend op but not the blend dest.

            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                            D3DRS_SRCBLEND, 
                                            d3d_blend_src);
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                            D3DRS_DESTBLEND, 
                                            d3d_blend_dest);

            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                            D3DRS_BLENDOP, 
                                            d3d_blend_op);
        }
        printf("rm_shader_state_set_blend_mode\n");

        rm_state.state_flags &= ~RM_SC_BLEND_MODE;
    }

    shader->state.blend_enabled = enable;
    shader->state.blend_op   = d3d_blend_op;
    shader->state.blend_src  = d3d_blend_src;
    shader->state.blend_dest = d3d_blend_dest;
}

NB_EXTERN void 
rm_shader_state_set_alpha_to_coverage(RMShader *shader, bool alpha_to_coverage) {
    if (alpha_to_coverage != shader->state.alpha_to_coverage)
        rm_state.state_flags |= RM_SC_ALPHA_TO_COVERAGE;

    if (shader == rm_state.current_shader) {
        if (rm_state.has_alpha_to_coverage_support && 
            (rm_state.state_flags & RM_SC_ALPHA_TO_COVERAGE)) {
            if (alpha_to_coverage) {
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device,
                    D3DRS_ADAPTIVETESS_Y,
                    (D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C'));

                // AMD code has not been tested.
                // IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_POINTSIZE, MAKEFOURCC('A','2','M','1'));
            } else {
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                    D3DRS_ADAPTIVETESS_Y,
                    D3DFMT_UNKNOWN);
            }

            printf("rm_shader_state_set_alpha_to_coverage\n");

            rm_state.state_flags &= ~RM_SC_ALPHA_TO_COVERAGE;
        }
    }

    shader->state.alpha_to_coverage = alpha_to_coverage;
}

NB_EXTERN void 
rm_shader_state_set_mask(RMShader *shader, 
                         bool red, bool green, bool blue, bool alpha, 
                         bool depth) {
    DWORD mask_flags = 0;

    if ((red   != shader->state.color_mask[0]) ||
        (green != shader->state.color_mask[1]) ||
        (blue  != shader->state.color_mask[2]) ||
        (alpha != shader->state.color_mask[3]) ||
        (depth != shader->state.depth_write)) {
        rm_state.state_flags |= RM_SC_COLOR_MASK;
    }

    if ((shader == rm_state.current_shader) &&
        (rm_state.state_flags & RM_SC_COLOR_MASK)) {
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                        D3DRS_ZWRITEENABLE, 
                                        depth);

        if (red)   mask_flags |= D3DCOLORWRITEENABLE_RED;
        if (green) mask_flags |= D3DCOLORWRITEENABLE_GREEN;
        if (blue)  mask_flags |= D3DCOLORWRITEENABLE_BLUE;
        if (alpha) mask_flags |= D3DCOLORWRITEENABLE_ALPHA;
        
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device,
                                        D3DRS_COLORWRITEENABLE, 
                                        mask_flags);

        printf("rm_shader_state_set_mask\n");

        rm_state.state_flags &= ~RM_SC_COLOR_MASK;
    }

    shader->state.color_mask[0] = red;
    shader->state.color_mask[1] = green;
    shader->state.color_mask[2] = blue;
    shader->state.color_mask[3] = alpha;
    shader->state.depth_write   = depth;
}

//
// @Note: This was not thouroughly tested, and lacks for the case
// of has_twosided_stencil_support = false, we should fallback to
// traditional 2-pass method: CW, CCW...
//
NB_EXTERN void rm_shader_state_set_stencil(RMShader *shader,
                                           bool enable, 
                                           bool front, 
                                           u32 stencil_func, 
                                           u32 reference,
                                           u32 read_mask,
                                           u32 write_mask,
                                           u32 stencil_fail_op,
                                           u32 depth_fail_op,
                                           u32 success_op) {
    static u32 d3d_stencil_func[] = {
        0,
        D3DCMP_NEVER,
        D3DCMP_LESS,
        D3DCMP_EQUAL,
        D3DCMP_LESSEQUAL,
        D3DCMP_GREATER,
        D3DCMP_NOTEQUAL,
        D3DCMP_GREATEREQUAL,
        D3DCMP_ALWAYS,
    };
    static u32 d3d_stencil_op[] = {
        0,
        D3DSTENCILOP_KEEP,
        D3DSTENCILOP_ZERO,
        D3DSTENCILOP_REPLACE,
        D3DSTENCILOP_INCRSAT,
        D3DSTENCILOP_DECRSAT,
        D3DSTENCILOP_INVERT,
        D3DSTENCILOP_INCR,
        D3DSTENCILOP_DECR,
    };
    u32 d3d_func = 0, d3d_stencil_fail = 0, d3d_depth_fail = 0, d3d_success = 0;
    int index;

    assert(stencil_func < nb_array_count(d3d_stencil_func));
    assert(stencil_fail_op < nb_array_count(d3d_stencil_op));
    assert(depth_fail_op < nb_array_count(d3d_stencil_op));
    assert(success_op < nb_array_count(d3d_stencil_op));

    d3d_func = d3d_stencil_func[stencil_func];
    d3d_stencil_fail = d3d_stencil_op[stencil_fail_op];
    d3d_depth_fail   = d3d_stencil_op[depth_fail_op];
    d3d_success      = d3d_stencil_op[success_op];

    index = 0;
    if (!front) index = 1;

    if (shader == rm_state.current_shader) {
        // Enable stencil testing.
        IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                        D3DRS_STENCILENABLE, 
                                        enable);

        if (rm_state.has_twosided_stencil_support) {
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device,
                                            D3DRS_TWOSIDEDSTENCILMODE, 
                                            enable);
        }

        if (enable) {
            // Set the comparison reference value.
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                            D3DRS_STENCILREF, 
                                            reference);

            // Specify a stencil mask.
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                            D3DRS_STENCILMASK, 
                                            read_mask);

            // Specify a stencil write mask.
            IDirect3DDevice9_SetRenderState(rm_state.d3d_device,
                                            D3DRS_STENCILWRITEMASK, 
                                            write_mask);


            if (front) {
                // Specify the stencil comparison function.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device,
                                                D3DRS_STENCILFUNC, 
                                                d3d_func);

                // If stencil test fails.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_STENCILFAIL, 
                                                d3d_stencil_fail);

                // If stencil test passes and z-test fails.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_STENCILZFAIL, 
                                                d3d_depth_fail);

                // if both stencil and z-tests pass.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_STENCILPASS, 
                                                d3d_success);
            } else if (rm_state.has_twosided_stencil_support) {
                // Specify the stencil comparison function.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_CCW_STENCILFUNC, 
                                                d3d_func);

                // If stencil test fails.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_CCW_STENCILFAIL, 
                                                d3d_stencil_fail);

                // If stencil test passes and z-test fails.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_CCW_STENCILZFAIL, 
                                                d3d_depth_fail);

                // if both stencil and z-tests pass.
                IDirect3DDevice9_SetRenderState(rm_state.d3d_device, 
                                                D3DRS_CCW_STENCILPASS, 
                                                d3d_success);
            }

            printf("rm_shader_state_set_stencil\n");
        }
    }

    shader->state.stencil[index].stencil_func    = d3d_func;
    shader->state.stencil[index].reference       = reference;
    shader->state.stencil[index].read_mask       = read_mask;
    shader->state.stencil[index].write_mask      = write_mask;
    shader->state.stencil[index].stencil_fail_op = d3d_stencil_fail;
    shader->state.stencil[index].depth_fail_op   = d3d_depth_fail;
    shader->state.stencil[index].success_op      = d3d_success;
    shader->state.stencil_enabled = enable;
}
