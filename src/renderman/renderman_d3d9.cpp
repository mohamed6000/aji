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

typedef struct {
    float x, y, z;
    float r, g, b, a;
} Immediate_Vertex;

#define MAX_IMMEDIATE_VERTICES 2048

typedef struct {
    IDirect3D9                  *d3d9;
    IDirect3DDevice9            *d3d_device;
    IDirect3DVertexBuffer9      *immediate_vb;
    IDirect3DVertexDeclaration9 *d3d_vertex_layout;
    IDirect3DVertexShader9      *vertex_shader;
    IDirect3DPixelShader9       *pixel_shader;
    D3DPRESENT_PARAMETERS       d3d_params;

    u32 num_immediate_vertices;
    Immediate_Vertex immediate_vertices[MAX_IMMEDIATE_VERTICES];

    float pixels_to_proj_matrix[4][4];
    
    bool d3d_device_lost;
} Renderman_State;

static Renderman_State rm_state;
static bool rm_initted;


// @Todo: One shader source.
char rm_vertex_shader_source[] = 
"float4x4 wvp : register(c0);"
"struct VS_Output {"
"   float4 pos   : POSITION;"
"   float4 color : COLOR;"
"};"
"VS_Output main(float3 pos : POSITION, float4 color : COLOR) {"
"   VS_Output result;"
"   result.pos   = mul(float4(pos, 1.0f), wvp);"
"   result.color = color;"
"   return result;"
"}";

char rm_pixel_shader_source[] = 
"struct VS_Output {"
"   float4 pos   : POSITION;"
"   float4 color : COLOR;"
"};"
"float4 main(VS_Output input) : COLOR {"
"   return input.color;"
"}";


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
    D3DFORMAT format = D3DFMT_X8R8G8B8;

    // Check the device capabilities.
    D3DCAPS9 caps = {0};
    hr = IDirect3D9_GetDeviceCaps(rm_state.d3d9, adapter, D3DDEVTYPE_HAL, &caps);
    if (FAILED(hr)) {
        Log("Failed to IDirect3D9_GetDeviceCaps.");
        return false;
    }
    
    bool vertex_hardware_processing_enabled = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

    HWND hwnd = (HWND)b_get_window_handle(window_id);

    RECT client_rect;
    GetClientRect(hwnd, &client_rect);

    rm_state.d3d_params.Windowed   = TRUE;
    rm_state.d3d_params.SwapEffect = D3DSWAPEFFECT_DISCARD;//D3DSWAPEFFECT_COPY;
    rm_state.d3d_params.BackBufferFormat = format;
    rm_state.d3d_params.hDeviceWindow    = hwnd;
    rm_state.d3d_params.BackBufferWidth  = client_rect.right  - client_rect.left;
    rm_state.d3d_params.BackBufferHeight = client_rect.bottom - client_rect.top;
    rm_state.d3d_params.EnableAutoDepthStencil = TRUE;
    rm_state.d3d_params.AutoDepthStencilFormat = D3DFMT_D24S8;
    rm_state.d3d_params.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;  // vsync on.
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

    hr = IDirect3D9_CreateDevice(rm_state.d3d9, 
                                 adapter,
                                 D3DDEVTYPE_HAL,
                                 hwnd,
                                 vertex_hardware_processing_enabled ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                 &rm_state.d3d_params,
                                 &rm_state.d3d_device);
    if (FAILED(hr) && vertex_hardware_processing_enabled) {
        hr = IDirect3D9_CreateDevice(rm_state.d3d9, 
                                     adapter,
                                     D3DDEVTYPE_HAL,
                                     hwnd,
                                     D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                     &rm_state.d3d_params,
                                     &rm_state.d3d_device);
    }

    if (FAILED(hr)) {
        Log("Failed to IDirect3D9_CreateDevice.");
        return false;
    }

    // Immediate non-FVF vertex buffer.
    hr = IDirect3DDevice9_CreateVertexBuffer(rm_state.d3d_device, 
                                             size_of(rm_state.immediate_vertices),
                                             D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                                             0, // No more FVF D3DFVF_XYZ,
                                             D3DPOOL_DEFAULT,
                                             &rm_state.immediate_vb, null);
    if (FAILED(hr)) {
        Log("Failed to create the immediate vertex buffer.");
        return false;
    }

    // Vertex declaration.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d9/mapping-fvf-codes-to-a-directx-9-declaration
    D3DVERTEXELEMENT9 vertex_elements[] = {
        {/*stream=*/0, /*offset=*/0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, /*usage_index=*/0},
        {/*stream=*/0, /*offset=*/12, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, /*usage_index=*/0},

        D3DDECL_END()
    };

    hr = IDirect3DDevice9_CreateVertexDeclaration(rm_state.d3d_device,
                                                  vertex_elements,
                                                  &rm_state.d3d_vertex_layout);
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
    DWORD *data = (DWORD *)(compiled_shader->lpVtbl->GetBufferPointer(compiled_shader));//ID3D10Blob_GetBufferPointer(compiled_shader);
#else
    DWORD *data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreateVertexShader(rm_state.d3d_device,
                                             data,
                                             &rm_state.vertex_shader);
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
    data = (DWORD *)(compiled_shader->lpVtbl->GetBufferPointer(compiled_shader));
#else
    data = (DWORD *)compiled_shader->GetBufferPointer();
#endif
    hr = IDirect3DDevice9_CreatePixelShader(rm_state.d3d_device,
                                            data,
                                            &rm_state.pixel_shader);
    if (FAILED(hr)) {
        Log("Failed to IDirect3DDevice9_CreatePixelShader.");
        return false;
    }

    D3DVIEWPORT9 vp;
    vp.X = vp.Y = 0;
    vp.Width  = rm_state.d3d_params.BackBufferWidth;
    vp.Height = rm_state.d3d_params.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;

    IDirect3DDevice9_SetViewport(rm_state.d3d_device, &vp);

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

    nb_logger_push_mode(old_mode);
    nb_logger_push_ident(old_ident);

    rm_initted = true;

    return true;
}

NB_EXTERN void rm_finish(void) {
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

    if (rm_state.d3d_device) {
        IDirect3DDevice9_Release(rm_state.d3d_device);
        rm_state.d3d_device = null;
    }

    if (rm_state.d3d9) {
        IDirect3D9_Release(rm_state.d3d9);
        rm_state.d3d9 = null;
    }
}

static void d3d_reset_device(void) {
    // @Todo: Free resources.
    HRESULT hr = IDirect3DDevice9_Reset(rm_state.d3d_device, &rm_state.d3d_params);
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

    if (FAILED(IDirect3DDevice9_BeginScene(rm_state.d3d_device))) {
        nb_log_print(NB_LOG_ERROR, "D3D9", "Failed to IDirect3DDevice9_BeginScene.");
    }


    if (rm_state.num_immediate_vertices) {
        // IDirect3DDevice9_SetRenderState(rm_state.d3d_device, D3DRS_CULLMODE, D3DCULL_CW);

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
    
    hr = IDirect3DDevice9_Present(rm_state.d3d_device, 0, 0, 0, 0);
    if (hr == D3DERR_DEVICELOST) {
        rm_state.d3d_device_lost = true;
    }
}

NB_EXTERN void rm_clear_render_target(float r, float g, float b, float a) {
    IDirect3DDevice9_Clear(rm_state.d3d_device, 0, null, 
                           D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                           D3DCOLOR_COLORVALUE(r,g,b,a),
                           1.0f, 0);
}



NB_INLINE void rm_put_vertex(Immediate_Vertex *dest, 
                             float x, float y, float z,
                             float r, float g, float b, float a) {
    dest->x = x;
    dest->y = y;
    dest->z = z;

    dest->r = r;
    dest->g = g;
    dest->b = b;
    dest->a = a;
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

NB_EXTERN void 
rm_immediate_quad(float x0, float y0, float x1, float y1,
                  float r, float g, float b, float a) {
    Immediate_Vertex *dest = rm_state.immediate_vertices + rm_state.num_immediate_vertices;

    rm_put_vertex(dest,   x0, y0, 0, r, g, b, a);
    rm_put_vertex(dest+1, x1, y0, 0, r, g, b, a);
    rm_put_vertex(dest+2, x1, y1, 0, r, g, b, a);

    rm_put_vertex(dest+3, x0, y0, 0, r, g, b, a);
    rm_put_vertex(dest+4, x1, y1, 0, r, g, b, a);
    rm_put_vertex(dest+5, x0, y1, 0, r, g, b, a);

    rm_state.num_immediate_vertices += 6;
}