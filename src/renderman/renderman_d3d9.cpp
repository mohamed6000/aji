#include "renderman.h"

#include <d3d9.h>
#include <d3dcompiler.h>

#if COMPILER_CL
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

static bool rm_initted;

static bool d3d_device_lost;

static D3DPRESENT_PARAMETERS  d3d_params;
static IDirect3DDevice9       *d3d_device;
static IDirect3DVertexBuffer9 *immediate_vb;
static IDirect3DVertexDeclaration9 *d3d_vertex_layout;
static IDirect3DVertexShader9 *vertex_shader;
static IDirect3DPixelShader9  *pixel_shader;

typedef struct {
    float x, y, z;
} Immediate_Vertex;

#define MAX_IMMEDIATE_VERTICES 2048

static s32 current_vertex_per_primitive = 3;
static u32 num_immediate_vertices;
static Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];


char rm_vertex_shader_source[] = 
// "float4x4 world_view_proj : register(c0);"
"float4 main(float3 pos : POSITION) : POSITION {"
// "   return mul(float4(pos, 1.0f), world_view_proj);"
"   return float4(pos, 1.0f);"
"}";

char rm_pixel_shader_source[] = 
"float4 main() : COLOR {"
"   return float4(1.0f, 1.0f, 1.0f, 1.0f);"
"}";


NB_EXTERN bool rm_init(u32 window_id) {
    if (rm_initted) return true;

    const char *old_ident = nb_logger_push_ident("D3D9");
    u32 old_mode = nb_logger_push_mode(NB_LOG_ERROR);

    IDirect3D9 *d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d9) {
        Log("Failed to Direct3DCreate9.");
        return false;
    }

    HRESULT hr;
    UINT adapter = D3DADAPTER_DEFAULT;
    D3DFORMAT format = D3DFMT_X8R8G8B8;

    // Check the device capabilities.
    D3DCAPS9 caps = {0};
    hr = IDirect3D9_GetDeviceCaps(d3d9, adapter, D3DDEVTYPE_HAL, &caps);
    if (FAILED(hr)) {
        Log("Failed to IDirect3D9_GetDeviceCaps.");
        return false;
    }
    
    bool vertex_hardware_processing_enabled = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

    HWND hwnd = (HWND)b_get_window_handle(window_id);

    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    d3d_params.Windowed   = TRUE;
    d3d_params.SwapEffect = D3DSWAPEFFECT_DISCARD;//D3DSWAPEFFECT_COPY;
    d3d_params.BackBufferFormat = format;
    d3d_params.hDeviceWindow    = hwnd;
    d3d_params.BackBufferWidth  = client_rect.right  - client_rect.left;
    d3d_params.BackBufferHeight = client_rect.bottom - client_rect.top;
    d3d_params.EnableAutoDepthStencil = TRUE;
    d3d_params.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3d_params.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;  // vsync on.
    // d3d_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;  // vsync off.


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
        Log("Failed to IDirect3D9_CreateDevice.");
        return false;
    }

    // Immediate non-FVF vertex buffer.
    hr = IDirect3DDevice9_CreateVertexBuffer(d3d_device, 
                                             size_of(immediate_vertices),
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             0, // No more FVF D3DFVF_XYZ,
                                             D3DPOOL_DEFAULT,
                                             &immediate_vb, null);
    if (FAILED(hr)) {
        Log("Failed to create the immediate vertex buffer.");
        return false;
    }

    // Vertex declaration.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/mapping-fvf-codes-to-a-directx-9-declaration
    D3DVERTEXELEMENT9 vertex_elements[] = {
        {/*stream=*/0, /*offset=*/0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, /*usage_index=*/0},
        D3DDECL_END()
    };

    hr = IDirect3DDevice9_CreateVertexDeclaration(d3d_device,
                                                  vertex_elements,
                                                  &d3d_vertex_layout);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreateVertexDeclaration.");
        return false;
    }

    // Vertex shader.
    ID3DBlob *compiled_shader = null;
    hr = D3DCompile(rm_vertex_shader_source, 
                    size_of(rm_vertex_shader_source),
                    /*pSourceName=*/null, 
                    /*pDefines=*/null,
                    /*pInclude=*/null,
                    "main",
                    "vs_3_0",
                    0,
                    0,
                    &compiled_shader,
                    null);
    if (FAILED(hr)) {
        Log("Failed to compile the vertex shader.");
        return false;
    }

#if LANGUAGE_C
    DWORD *data = (DWORD *)ID3D10Blob_GetBufferPointer(compiled_shader);
#else
    DWORD *data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreateVertexShader(d3d_device,
                                             data,
                                             &vertex_shader);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreateVertexShader.");
        return false;
    }

    // Pixel shader.
    hr = D3DCompile(rm_pixel_shader_source, 
                    size_of(rm_pixel_shader_source),
                    /*pSourceName=*/null, 
                    /*pDefines=*/null,
                    /*pInclude=*/null,
                    "main",
                    "ps_3_0",
                    0,
                    0,
                    &compiled_shader,
                    null);
    if (FAILED(hr)) {
        Log("Failed to compile the pixel shader.");
        return false;
    }

#if LANGUAGE_C
    data = (DWORD *)ID3D10Blob_GetBufferPointer(compiled_shader);
#else
    data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreatePixelShader(d3d_device,
                                            data,
                                            &pixel_shader);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreatePixelShader.");
        return false;
    }

    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width  = d3d_params.BackBufferWidth;
    vp.Height = d3d_params.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    IDirect3DDevice9_SetViewport(d3d_device, &vp);

    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_FILLMODE,  D3DFILL_SOLID);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_ZWRITEENABLE,    FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_BLENDOP,   D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_SRCBLENDALPHA,  D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_SCISSORTESTENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_FOGENABLE,         FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_RANGEFOGENABLE,    FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_SPECULARENABLE,    FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_STENCILENABLE,     FALSE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_CLIPPING,          TRUE);
    IDirect3DDevice9_SetRenderState(d3d_device, D3DRS_LIGHTING,          FALSE);

    nb_logger_push_mode(old_mode);
    nb_logger_push_ident(old_ident);

    rm_initted = true;

    return true;
}

NB_EXTERN void rm_finish(void) {
    if (d3d_device) {
        IDirect3DDevice9_Release(d3d_device);
        d3d_device = null;
    }
}

static void d3d_reset_device(void) {
    // @Todo: Free resources.
    HRESULT hr = IDirect3DDevice9_Reset(d3d_device, &d3d_params);
    assert(hr != D3DERR_INVALIDCALL);
    // @Todo: Init resources.
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

NB_EXTERN void rm_swap_buffers(u32 window_id) {
    UNUSED(window_id);
    HRESULT hr;

    immediate_vertices[0].x = -0.5f;
    immediate_vertices[0].y = -0.5f;
    immediate_vertices[0].z = 0;

    immediate_vertices[1].x = 0;
    immediate_vertices[1].y = 0.5f;
    immediate_vertices[1].z = 0;

    immediate_vertices[2].x = 0.5f;
    immediate_vertices[2].y = -0.5f;
    immediate_vertices[2].z = 0;

    num_immediate_vertices = 3;

    // Test device lost state.
    if (d3d_device_lost) {
        hr = IDirect3DDevice9_TestCooperativeLevel(d3d_device);
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

        d3d_device_lost = false;
    }

    if (FAILED(IDirect3DDevice9_BeginScene(d3d_device))) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to IDirect3DDevice9_BeginScene.");
    }


    if (num_immediate_vertices) {
        void *locked_vb = null;
        if (SUCCEEDED(IDirect3DVertexBuffer9_Lock(immediate_vb, 
            0, 
            num_immediate_vertices*size_of(Immediate_Vertex), 
            &locked_vb, D3DLOCK_DISCARD))) {
            memcpy(locked_vb, 
                   immediate_vertices, 
                   num_immediate_vertices*size_of(Immediate_Vertex));

            IDirect3DVertexBuffer9_Unlock(immediate_vb);
        }

        IDirect3DDevice9_SetStreamSource(d3d_device,
                                         0,
                                         immediate_vb,
                                         0,
                                         size_of(Immediate_Vertex));

        // IDirect3DDevice9_SetFVF(d3d_device, D3DFVF_XYZ);
        IDirect3DDevice9_SetVertexDeclaration(d3d_device, d3d_vertex_layout);

        // IDirect3DDevice9_SetVertexShaderConstantF(d3d_device, 0, &wvp_transposed.m[0][0], 4);

        IDirect3DDevice9_SetVertexShader(d3d_device, vertex_shader);
        IDirect3DDevice9_SetPixelShader(d3d_device, pixel_shader);

        UINT num_primitives = num_immediate_vertices / 3;
        IDirect3DDevice9_DrawPrimitive(d3d_device,
                                       D3DPT_TRIANGLELIST, 
                                       0, num_primitives);

        num_immediate_vertices = 0;
    }

    IDirect3DDevice9_EndScene(d3d_device);
    
    hr = IDirect3DDevice9_Present(d3d_device, 0, 0, 0, 0);
    if (hr == D3DERR_DEVICELOST) {
        d3d_device_lost = true;
    }
}

NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a) {
    IDirect3DDevice9_Clear(d3d_device, 0, null, 
                           D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                           D3DCOLOR_COLORVALUE(r,g,b,a),
                           1.0f, 0);
}
