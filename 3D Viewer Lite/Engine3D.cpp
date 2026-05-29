#include "Engine3D.h"

// Simple timer for profiling (output to VS output window)
struct ProfileTimer {
    std::string name;
    std::chrono::high_resolution_clock::time_point start;

    ProfileTimer(const std::string& sectionName) : name(sectionName) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~ProfileTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration<float, std::milli>(end - start).count();
        std::string output = "[Profile] " + name + ": " + std::to_string(duration) + " ms\n";
        OutputDebugStringA(output.c_str());
    }
};

Engine3D::Engine3D()
{
    lightColorR = { 1.0f, 0.0f, 0.0f };
    lightColorG = { 0.0f, 1.0f, 0.0f };
    lightColorB = { 0.0f, 0.0f, 1.0f };
}

Engine3D::~Engine3D()
{
    if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }
}

bool Engine3D::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    RECT rc;
    GetClientRect(hwnd, &rc);
    m_width = rc.right - rc.left;
    m_height = rc.bottom - rc.top;

    // Create DX11 device and swap chain if not exist
    if (!pd3dDevice)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = m_width;
        sd.BufferDesc.Height = m_height;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0,
            D3D11_SDK_VERSION, &sd, &pSwapChain, &pd3dDevice, NULL, &pd3dContext
        );
        if (FAILED(hr)) return false;

        // Create DX11 render target view
        ID3D11Texture2D* pBackBufferTex = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBufferTex);
        if (pBackBufferTex) {
            pd3dDevice->CreateRenderTargetView(pBackBufferTex, NULL, &pmainRenderTargetView);
            pBackBufferTex->Release();
        }
    }

    // Create D2D factory
    if (!m_pD2DFactory)
    {
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
        if (FAILED(hr)) return false;
    }

    // Bind D2D render target to DX11 back buffer
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }

    IDXGISurface* pDxgiSurface = nullptr;
    HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (LPVOID*)&pDxgiSurface);
    if (SUCCEEDED(hr))
    {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        hr = m_pD2DFactory->CreateDxgiSurfaceRenderTarget(pDxgiSurface, &props, &m_pRenderTarget);
        pDxgiSurface->Release();
    }
    if (FAILED(hr)) return false;

    // Initialize CPU framebuffer and D2D bitmap
    m_frameBuffer.assign(m_width * m_height, 0xFF000000);

    if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
    hr = m_pRenderTarget->CreateBitmap(
        D2D1::SizeU(m_width, m_height),
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        &m_pBackBufferBitmap
    );

    QueryPerformanceFrequency(&m_freq);
    QueryPerformanceCounter(&m_lastTime);
    m_firstFrame = true;

    return SUCCEEDED(hr);
}

void Engine3D::Resize(int width, int height)
{
    m_width = width;
    m_height = height;

    if (pSwapChain)
    {
        // Release resources that depend on back buffer
        if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
        if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
        if (pmainRenderTargetView) { pmainRenderTargetView->Release(); pmainRenderTargetView = nullptr; }

        // Resize swap chain
        pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // Recreate DX11 render target view
        ID3D11Texture2D* pBackBufferTex = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBufferTex);
        if (pBackBufferTex) {
            pd3dDevice->CreateRenderTargetView(pBackBufferTex, NULL, &pmainRenderTargetView);
            pBackBufferTex->Release();
        }

        // Rebind D2D render target
        IDXGISurface* pDxgiSurface = nullptr;
        HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), (LPVOID*)&pDxgiSurface);
        if (SUCCEEDED(hr))
        {
            D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            );
            m_pD2DFactory->CreateDxgiSurfaceRenderTarget(pDxgiSurface, &props, &m_pRenderTarget);
            pDxgiSurface->Release();
        }

        // Resize CPU buffers
        m_frameBuffer.resize(width * height);
        m_depthBuffer.resize(width * height);

        if (m_pRenderTarget)
        {
            m_pRenderTarget->CreateBitmap(
                D2D1::SizeU(width, height),
                D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
                &m_pBackBufferBitmap
            );
        }
    }
}

void Engine3D::BeginDraw()
{
    if (m_pRenderTarget)
        m_pRenderTarget->BeginDraw();

    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), 0x00808080);
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), std::numeric_limits<float>::infinity());
}

void Engine3D::EndDraw()
{
    if (m_pRenderTarget)
    {
        // Copy CPU framebuffer to GPU bitmap
        if (m_pBackBufferBitmap)
        {
            m_pBackBufferBitmap->CopyFromMemory(
                nullptr,
                m_frameBuffer.data(),
                m_width * sizeof(uint32_t)
            );
            m_pRenderTarget->DrawBitmap(m_pBackBufferBitmap);
        }

        HRESULT hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
            if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
            Initialize(m_hwnd);
        }
    }
}

int Engine3D::ScreenHeight()
{
    return m_height;
}

int Engine3D::ScreenWidth()
{
    return m_width;
}

void Engine3D::Fill(short transparency, uint32_t color)
{
    uint32_t alpha = static_cast<uint32_t>(transparency) & 0xFF;
    uint32_t target = (alpha << 24) | (color & 0x00FFFFFF);
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), target);
}

void Engine3D::Fill(int lux, int luy, int rdx, int rdy, short transparency, uint32_t color)
{
    uint32_t alpha = static_cast<uint32_t>(transparency) & 0xFF;
    uint32_t target = (alpha << 24) | (color & 0x00FFFFFF);
    for (int i = luy; i < rdy; i++)
    {
        for (int j = lux; j < rdx; j++)
        {
            if (i >= 0 && i < m_height && j >= 0 && j < m_width)
            {
                m_frameBuffer[i * m_width + j] = target;
            }
        }
    }
}

void Engine3D::Fill(triangle3D tri, short transparency, uint32_t color, int minClipY, int maxClipY)
{
    uint32_t alpha = static_cast<uint32_t>(transparency) & 0xFF;
    uint32_t target = (alpha << 24) | (color & 0x00FFFFFF);

    if (maxClipY == -1) maxClipY = m_height - 1;

    vector3D A = transformedvectors[tri.point[0]];
    vector3D B = transformedvectors[tri.point[1]];
    vector3D C = transformedvectors[tri.point[2]];

    int minX = std::max(0, (int)std::floor(std::min({ A.x, B.x, C.x })));
    int maxX = std::min(m_width - 1, (int)std::ceil(std::max({ A.x, B.x, C.x })));
    int minY = std::max(minClipY, (int)std::floor(std::min({ A.y, B.y, C.y })));
    int maxY = std::min(maxClipY, (int)std::ceil(std::max({ A.y, B.y, C.y })));

    if (minY > maxY || minX > maxX) return;

    float denominator = (B.y - C.y) * (A.x - C.x) + (C.x - B.x) * (A.y - C.y);
    if (std::abs(denominator) < 1e-6) return;
    float inv_denominator = 1.0f / denominator;

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            float px = x + 0.5f;
            float py = y + 0.5f;

            float alpha = ((B.y - C.y) * (px - C.x) + (C.x - B.x) * (py - C.y)) * inv_denominator;
            float beta = ((C.y - A.y) * (px - C.x) + (A.x - C.x) * (py - C.y)) * inv_denominator;
            float gamma = 1.0f - alpha - beta;

            if (alpha >= 0 && beta >= 0 && gamma >= 0)
            {
                float current_z = (float)(alpha * A.z + beta * B.z + gamma * C.z);
                int buffer_index = y * m_width + x;

                if (current_z < m_depthBuffer[buffer_index])
                {
                    m_depthBuffer[buffer_index] = current_z;
                    m_frameBuffer[buffer_index] = target;
                }
            }
        }
    }
}

void Engine3D::Drawline(int x1, int y1, int x2, int y2, short transparency, uint32_t color)
{
    uint32_t alpha = static_cast<uint32_t>(transparency) & 0xFF;
    uint32_t target = (alpha << 24) | (color & 0x00FFFFFF);

    // 1. bottom-right, X dominant
    {
        int y_1 = y1;
        if (x1 < x2 && y2 > y1 && x2 - x1 >= y2 - y1)
        {
            for (int x_1 = x1; x_1 <= x2; x_1++)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((x_1 - x1) * (y2 - y1) * 2 > (2 * (y_1 - y1) + 1) * (x2 - x1)) y_1++;
            }
        }
    }
    // 2. top-right, X dominant
    {
        int y_1 = y1;
        if (x1 < x2 && y2 < y1 && (x2 - x1) >= (y1 - y2))
        {
            for (int x_1 = x1; x_1 <= x2; x_1++)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((x_1 - x1) * (y1 - y2) * 2 > (2 * (y1 - y_1) + 1) * (x2 - x1)) y_1--;
            }
        }
    }
    // 3. bottom-left, X dominant
    {
        int y_1 = y1;
        if (x1 > x2 && y2 > y1 && (x1 - x2) >= (y2 - y1))
        {
            for (int x_1 = x1; x_1 >= x2; x_1--)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((x1 - x_1) * (y2 - y1) * 2 > (2 * (y_1 - y1) + 1) * (x1 - x2)) y_1++;
            }
        }
    }
    // 4. top-left, X dominant
    {
        int y_1 = y1;
        if (x1 > x2 && y2 < y1 && (x1 - x2) >= (y1 - y2))
        {
            for (int x_1 = x1; x_1 >= x2; x_1--)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((x1 - x_1) * (y1 - y2) * 2 > (2 * (y1 - y_1) + 1) * (x1 - x2)) y_1--;
            }
        }
    }
    // 5. bottom-right, Y dominant
    {
        int x_1 = x1;
        if (x1 < x2 && y2 > y1 && (y2 - y1) > (x2 - x1))
        {
            for (int y_1 = y1; y_1 <= y2; y_1++)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((y_1 - y1) * (x2 - x1) * 2 > (2 * (x_1 - x1) + 1) * (y2 - y1)) x_1++;
            }
        }
    }
    // 6. top-right, Y dominant
    {
        int x_1 = x1;
        if (x1 < x2 && y2 < y1 && (y1 - y2) >(x2 - x1))
        {
            for (int y_1 = y1; y_1 >= y2; y_1--)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((y1 - y_1) * (x2 - x1) * 2 > (2 * (x_1 - x1) + 1) * (y1 - y2)) x_1++;
            }
        }
    }
    // 7. bottom-left, Y dominant
    {
        int x_1 = x1;
        if (x1 > x2 && y2 > y1 && (y2 - y1) > (x1 - x2))
        {
            for (int y_1 = y1; y_1 <= y2; y_1++)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((y_1 - y1) * (x1 - x2) * 2 > (2 * (x1 - x_1) + 1) * (y2 - y1)) x_1--;
            }
        }
    }
    // 8. top-left, Y dominant
    {
        int x_1 = x1;
        if (x1 > x2 && y2 < y1 && (y1 - y2) >(x1 - x2))
        {
            for (int y_1 = y1; y_1 >= y2; y_1--)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
                if ((y1 - y_1) * (x1 - x2) * 2 > (2 * (x1 - x_1) + 1) * (y1 - y2)) x_1--;
            }
        }
    }
    // 9. pure horizontal right
    {
        int y_1 = y1;
        if (y1 == y2 && x1 <= x2)
        {
            for (int x_1 = x1; x_1 <= x2; x_1++)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
            }
        }
    }
    // 10. pure horizontal left
    {
        int y_1 = y1;
        if (y1 == y2 && x1 > x2)
        {
            for (int x_1 = x1; x_1 >= x2; x_1--)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
            }
        }
    }
    // 11. pure vertical down
    {
        int x_1 = x1;
        if (x1 == x2 && y1 < y2)
        {
            for (int y_1 = y1; y_1 <= y2; y_1++)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
            }
        }
    }
    // 12. pure vertical up
    {
        int x_1 = x1;
        if (x1 == x2 && y1 > y2)
        {
            for (int y_1 = y1; y_1 >= y2; y_1--)
            {
                if (x_1 >= 0 && x_1 < m_width && y_1 >= 0 && y_1 < m_height)
                {
                    m_frameBuffer[y_1 * m_width + x_1] = target;
                }
            }
        }
    }
}

void Engine3D::DrawTriangle(triangle3D tri, short transparency, uint32_t color)
{
    for (int i = 0; i < 3; ++i)
    {
        int j = (i + 1) % 3;
        Drawline((int)transformedvectors[tri.point[i]].x, (int)transformedvectors[tri.point[i]].y,
            (int)transformedvectors[tri.point[j]].x, (int)transformedvectors[tri.point[j]].y,
            255, color);
    }
}

void Engine3D::DrawMesh3D(float fElapsedTime)
{
    {
        ProfileTimer t_geom("   [DrawMesh3D] P1: Vertex Transform & Projection");
        // Transform and project all vertices in parallel
#pragma omp parallel for
        for (int i = 0; i < verts.size(); i++)
        {
            vector3D vpoint = verts[i];
            vpoint = MtimesV(Rotation, vpoint);
            vpoint.x *= zoom;
            vpoint.y *= zoom;
            vpoint.z *= zoom;

            float x = vpoint.x;
            float y = vpoint.y;
            float z = vpoint.z;
            vpoint.x = 1.5 * unit * x * distance / (distance + z) + ScreenWidth() / 2.0;
            vpoint.y = 1.5 * unit * y * distance / (distance + z) + ScreenHeight() / 2.0;

            transformedvectors[i] = vpoint;
        }
    }

    int num_threads = omp_get_max_threads();
    int strip_height = m_height / num_threads;

    struct PreparedTriangle {
        triangle3D tri;
        uint32_t color;
    };

    // 2D binning: localBoxes[geometry_thread][screen_strip]
    std::vector<std::vector<std::vector<PreparedTriangle>>> localBoxes(
        num_threads, std::vector<std::vector<PreparedTriangle>>(num_threads)
    );

    {
        ProfileTimer t_binning("   [DrawMesh3D] P2-1: Geometry & Binning Pass");

        // Pre-normalize light directions (once per frame)
        vector3D rLightNorm = Rlight.normalize();
        vector3D gLightNorm = Glight.normalize();
        vector3D bLightNorm = Blight.normalize();

#pragma omp parallel for
        for (int i = 0; i < (int)meshInput.tris.size(); i++)
        {
            int tid = omp_get_thread_num();
            triangle3D tri = meshInput.tris[i];

            // Backface culling
            float NormalValue = (transformedvectors[tri.point[1]].x - transformedvectors[tri.point[0]].x) * (transformedvectors[tri.point[2]].y - transformedvectors[tri.point[0]].y) - (transformedvectors[tri.point[1]].y - transformedvectors[tri.point[0]].y) * (transformedvectors[tri.point[2]].x - transformedvectors[tri.point[0]].x);
            if (NormalValue > 0) continue;

            // Lighting calculation
            tri.NormalVector = MtimesV(Rotation, tri.NormalVector);

            float R_Intensity = -256 * tri.NormalVector.dot(rLightNorm);
            float G_Intensity = -256 * tri.NormalVector.dot(gLightNorm);
            float B_Intensity = -256 * tri.NormalVector.dot(bLightNorm);

            if (R_Intensity < 0) R_Intensity = 0;
            if (G_Intensity < 0) G_Intensity = 0;
            if (B_Intensity < 0) B_Intensity = 0;

            float finalR = R_Intensity * lightColorR.x + G_Intensity * lightColorG.x + B_Intensity * lightColorB.x;
            float finalG = R_Intensity * lightColorR.y + G_Intensity * lightColorG.y + B_Intensity * lightColorB.y;
            float finalB = R_Intensity * lightColorR.z + G_Intensity * lightColorG.z + B_Intensity * lightColorB.z;
            finalR /= 3.0f; finalG /= 3.0f; finalB /= 3.0f;
            BYTE r = (BYTE)std::min(255.0f, std::max(0.0f, finalR));
            BYTE g = (BYTE)std::min(255.0f, std::max(0.0f, finalG));
            BYTE b = (BYTE)std::min(255.0f, std::max(0.0f, finalB));
            uint32_t finalColor = RGB(r, g, b);

            // Compute Y range of triangle to determine which screen strips it belongs to
            float ay = transformedvectors[tri.point[0]].y;
            float by = transformedvectors[tri.point[1]].y;
            float cy = transformedvectors[tri.point[2]].y;
            int triMinY = (int)std::floor(std::min({ ay, by, cy }));
            int triMaxY = (int)std::ceil(std::max({ ay, by, cy }));

            int start_t = std::max(0, triMinY / strip_height);
            int end_t = std::min(num_threads - 1, triMaxY / strip_height);

            PreparedTriangle pt = { tri, finalColor };
            for (int t = start_t; t <= end_t; t++)
            {
                localBoxes[tid][t].push_back(pt);
            }
        }
    }

    // Parallel rasterization phase
    if (IsFillAndLight)
    {
        ProfileTimer t_raster_pure("   [DrawMesh3D] P2-2: Pure Multi-Thread Rasterization");

#pragma omp parallel for schedule(dynamic, 1)
        for (int t = 0; t < num_threads; t++)
        {
            int minClipY = t * strip_height;
            int maxClipY = (t == num_threads - 1) ? (m_height - 1) : (minClipY + strip_height - 1);

            for (int tid = 0; tid < num_threads; tid++)
            {
                for (size_t i = 0; i < localBoxes[tid][t].size(); i++)
                {
                    Fill(localBoxes[tid][t][i].tri, 128, localBoxes[tid][t][i].color, minClipY, maxClipY);
                }
            }
        }
    }

    // Serial wireframe drawing (single-threaded to avoid conflicts)
    if (IsWireFramePaint)
    {
        ProfileTimer t_wire("   [DrawMesh3D] P3: Serial WireFrame Paint");
        for (int i = 0; i < (int)meshInput.tris.size(); i++)
        {
            triangle3D tri = meshInput.tris[i];
            float NormalValue = (transformedvectors[tri.point[1]].x - transformedvectors[tri.point[0]].x) * (transformedvectors[tri.point[2]].y - transformedvectors[tri.point[0]].y) - (transformedvectors[tri.point[1]].y - transformedvectors[tri.point[0]].y) * (transformedvectors[tri.point[2]].x - transformedvectors[tri.point[0]].x);
            if (NormalValue > 0) continue;

            DrawTriangle(tri, 128, 0x00ff0000);
        }
    }
}

void Engine3D::MoveToCenter()
{
    float cx = 0, cy = 0, cz = 0;

    for (const vector3D& point : verts)
    {
        cx += point.x;
        cy += point.y;
        cz += point.z;
    }

    cx /= static_cast<float>(verts.size());
    cy /= static_cast<float>(verts.size());
    cz /= static_cast<float>(verts.size());

    for (int i = 0; i < verts.size(); i++)
    {
        verts[i].x -= cx;
        verts[i].y -= cy;
        verts[i].z -= cz;
    }
}

bool Engine3D::OnUserCreate()
{
    verts.clear();
    zoom = 1;
    yaw = 0;
    pitch = 0;

    std::string ext = name.substr(name.find_last_of('.'));
    if (ext == ".obj")
        meshInput = LoadFromObjectFile(name);
    else if (ext == ".ply")
        meshInput = LoadFromPlyFile(name);
    else if (ext == ".stl")
        meshInput = LoadFromStlFile(name);
    else
        OutputDebugStringA("Unsupported file format.\n");

    MoveToCenter();

    // Auto-calculate zoom based on model bounding box
    float minX = 1e30f, minY = 1e30f, minZ = 1e30f;
    float maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;

    for (const auto& v : verts)
    {
        if (v.x < minX) minX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.z < minZ) minZ = v.z;
        if (v.x > maxX) maxX = v.x;
        if (v.y > maxY) maxY = v.y;
        if (v.z > maxZ) maxZ = v.z;
    }

    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    float maxDimension = std::max({ sizeX, sizeY, sizeZ });

    if (maxDimension < 0.0001f)
        maxDimension = 1.0f;

    const float fillRatio = 0.7f;
    int screenDim = std::min(m_width, m_height);
    float unitVal = std::sqrt((float)(m_width * m_width + m_height * m_height)) / 16.0f;
    zoom = (screenDim * fillRatio) / (1.5f * unitVal * maxDimension);

    // Precompute triangle normals
    for (int i = 0; i < meshInput.tris.size(); i++)
    {
        triangle3D tri = meshInput.tris[i];

        vector3D NormalVector = {
            (verts[tri.point[1]].y - verts[tri.point[0]].y) * (verts[tri.point[2]].z - verts[tri.point[0]].z) - (verts[tri.point[1]].z - verts[tri.point[0]].z) * (verts[tri.point[2]].y - verts[tri.point[0]].y),
            (verts[tri.point[1]].z - verts[tri.point[0]].z) * (verts[tri.point[2]].x - verts[tri.point[0]].x) - (verts[tri.point[1]].x - verts[tri.point[0]].x) * (verts[tri.point[2]].z - verts[tri.point[0]].z),
            (verts[tri.point[1]].x - verts[tri.point[0]].x) * (verts[tri.point[2]].y - verts[tri.point[0]].y) - (verts[tri.point[1]].y - verts[tri.point[0]].y) * (verts[tri.point[2]].x - verts[tri.point[0]].x)
        };

        NormalVector = NormalVector.normalize();
        meshInput.tris[i].NormalVector = NormalVector;
    }
    transformedvectors = verts;
    return true;
}

bool Engine3D::OnUserUpdate(float fElapsedTime)
{
    Fill(255, 0x404040);

    CreateRotationMatrix(yaw, pitch);
    float fov_rad = fov * 3.1415926535 / 180.0;
    if (fov <= 0.0)
    {
        distance = 10000.0;
    }
    else
    {
        distance = 4.0 / tan(fov_rad * 0.5);
        const float MIN_DIST = 1.5;
        if (distance < MIN_DIST) distance = MIN_DIST;
        if (distance > 1000.0) distance = 1000.0;
    }
    unit = sqrt(ScreenHeight() * ScreenHeight() + ScreenWidth() * ScreenWidth()) / 16;

    DrawMesh3D(fElapsedTime);

    return true;
}

void Engine3D::Render()
{
    ProfileTimer t_total("=== Total Render Frame ===");

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float deltaTime = 0.016f;

    if (!m_firstFrame)
    {
        deltaTime = (float)((now.QuadPart - m_lastTime.QuadPart) / (float)m_freq.QuadPart);
        if (deltaTime > 0.1f) deltaTime = 0.016f;
    }
    else
    {
        m_firstFrame = false;
    }
    m_lastTime = now;

    {
        ProfileTimer t_begin("1. BeginDraw (Memory Buffer Clear)");
        BeginDraw();
    }

    {
        ProfileTimer t_update("2. OnUserUpdate (Entire Logic & Rasterization)");
        OnUserUpdate(deltaTime);
    }

    EndDraw();
}

void Engine3D::UpdateYawAndPitch(int delta_x, int delta_y)
{
    yaw -= static_cast<float>(delta_x) / static_cast<float>(400) * 3.1415926;
    pitch += static_cast<float>(delta_y) / static_cast<float>(400) * 3.1415926;
    if (pitch >= 3.14159 / 2) pitch = 3.14159 / 2;
    if (pitch <= -3.14159 / 2) pitch = -3.14159 / 2;
}

void Engine3D::CreateRotationMatrix(float yaw, float pitch)
{
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);
    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    Rotation = {
        {
        { cosYaw,               0,                  sinYaw,             0 },
        { sinPitch * sinYaw,    cosPitch,           -sinPitch * cosYaw, 0 },
        { -cosPitch * sinYaw,   sinPitch,           cosPitch * cosYaw,  0 },
        { 0,                    0,                  0,                  1 }
        }
    };
}

mesh3D Engine3D::LoadFromObjectFile(std::string filename)
{
    std::ifstream f(filename);
    mesh3D mesh;

    std::string line;
    while (std::getline(f, line))
    {
        std::string junk;
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == 'v' && line[1] == ' ')
        {
            std::stringstream ss(line);
            vector3D v;
            ss >> junk >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }
        else if (line[0] == 'f' && line[1] == ' ')
        {
            std::stringstream ss(line);
            std::vector<std::string> face(4);
            face[3] = "";
            ss >> junk >> face[0] >> face[1] >> face[2] >> face[3];

            int v[4] = { 0 };
            int vt[4] = { 0 };
            int vn[4] = { 0 };

            for (int i = 0; i < 4; i++)
            {
                if (!face[i].empty())
                {
                    int j = 0;
                    while (face[i][j])
                    {
                        if (face[i][j] == '/')
                        {
                            face[i][j] = ' ';
                        }
                        j++;
                    }
                }
            }
            for (int i = 0; i < 4; i++)
            {
                std::stringstream block(face[i]);
                block >> v[i] >> vt[i] >> vn[i];
            }
            mesh.tris.push_back({ v[0] - 1, v[1] - 1, v[2] - 1 });
            if (v[3] != 0) mesh.tris.push_back({ v[0] - 1, v[2] - 1, v[3] - 1 });
        }
    }
    return mesh;
}

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

struct PlyProperty
{
    std::string name;
    std::string type;
    bool isList = false;
    std::string listCountType;
    std::string listIndexType;
};

static uint32_t read_uint32_le(std::ifstream& f)
{
    uint32_t val;
    f.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

static float read_float_le(std::ifstream& f)
{
    float val;
    f.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

mesh3D Engine3D::LoadFromPlyFile(std::string filename)
{
    std::ifstream f(filename, std::ios::binary);
    mesh3D mesh;
    if (!f.is_open())
    {
        OutputDebugStringA(("PLY: Cannot open " + filename + "\n").c_str());
        return mesh;
    }

    std::string line;
    std::string format;
    int vertexCount = 0, faceCount = 0;

    std::vector<PlyProperty> vertexProps;
    std::vector<PlyProperty> faceProps;
    bool inVertex = false, inFace = false;

    // Parse header
    while (std::getline(f, line))
    {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty() || line[0] == '#')
            continue;

        if (line == "end_header")
            break;

        std::stringstream ss(line);
        std::string keyword;
        ss >> keyword;
        std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);

        if (keyword == "format")
        {
            ss >> format;
            std::string version;
            ss >> version;
        }
        else if (keyword == "element")
        {
            inVertex = inFace = false;
            std::string elemType;
            int count;
            ss >> elemType >> count;
            std::transform(elemType.begin(), elemType.end(), elemType.begin(), ::tolower);
            if (elemType == "vertex")
            {
                vertexCount = count;
                inVertex = true;
            }
            else if (elemType == "face")
            {
                faceCount = count;
                inFace = true;
            }
        }
        else if (keyword == "property")
        {
            if (inVertex)
            {
                PlyProperty prop;
                ss >> prop.type >> prop.name;
                vertexProps.push_back(prop);
            }
            else if (inFace)
            {
                std::string first;
                ss >> first;
                if (first == "list")
                {
                    PlyProperty prop;
                    prop.isList = true;
                    ss >> prop.listCountType >> prop.listIndexType >> prop.name;
                    faceProps.push_back(prop);
                }
                else
                {
                    PlyProperty prop;
                    prop.type = first;
                    ss >> prop.name;
                    faceProps.push_back(prop);
                }
            }
        }
    }

    if (vertexCount == 0 || faceCount == 0)
    {
        OutputDebugStringA("PLY: Header missing vertex or face element.\n");
        return mesh;
    }

    bool isBinary = (format.find("binary") != std::string::npos);

    if (!isBinary)
    {
        // ASCII PLY
        verts.reserve(verts.size() + vertexCount);
        for (int i = 0; i < vertexCount; ++i)
        {
            if (!std::getline(f, line)) break;
            std::stringstream ss(line);
            vector3D v;
            ss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }

        int minIdx = INT_MAX, maxIdx = INT_MIN;
        mesh.tris.reserve(faceCount);
        for (int i = 0; i < faceCount; ++i)
        {
            if (!std::getline(f, line)) break;
            std::stringstream ss(line);
            int n;
            ss >> n;
            if (n < 3) continue;
            std::vector<int> idx(n);
            for (int j = 0; j < n; ++j)
            {
                ss >> idx[j];
                if (idx[j] < minIdx) minIdx = idx[j];
                if (idx[j] > maxIdx) maxIdx = idx[j];
            }
            mesh.tris.push_back({ idx[0], idx[1], idx[2] });
            for (int j = 3; j < n; ++j)
                mesh.tris.push_back({ idx[0], idx[j - 1], idx[j] });
        }

        if (minIdx == 1 && maxIdx <= (int)verts.size())
        {
            for (auto& tri : mesh.tris)
            {
                tri.point[0]--; tri.point[1]--; tri.point[2]--;
            }
        }
    }
    else
    {
        // Binary PLY
        verts.reserve(verts.size() + vertexCount);
        for (int i = 0; i < vertexCount; ++i)
        {
            vector3D v;
            for (auto& prop : vertexProps)
            {
                if (prop.name == "x")
                {
                    v.x = read_float_le(f);
                }
                else if (prop.name == "y")
                {
                    v.y = read_float_le(f);
                }
                else if (prop.name == "z")
                {
                    v.z = read_float_le(f);
                }
                else
                {
                    if (prop.type == "float")
                        read_float_le(f);
                    else if (prop.type == "uchar")
                        f.ignore(1);
                    else if (prop.type == "int")
                        f.ignore(4);
                    else if (prop.type == "uint")
                        f.ignore(4);
                    else
                        f.ignore(4);
                }
            }
            verts.push_back(v);
        }

        mesh.tris.reserve(faceCount);
        int minIdx = INT_MAX, maxIdx = INT_MIN;

        for (int i = 0; i < faceCount; ++i)
        {
            for (auto& prop : faceProps)
            {
                if (prop.isList)
                    continue;
                if (prop.type == "uchar")       f.ignore(1);
                else if (prop.type == "float")  f.ignore(4);
                else if (prop.type == "int")    f.ignore(4);
                else if (prop.type == "uint")   f.ignore(4);
                else                             f.ignore(4);
            }

            for (auto& prop : faceProps)
            {
                if (!prop.isList) continue;

                int n = 0;
                if (prop.listCountType == "uchar")
                {
                    unsigned char count;
                    f.read(reinterpret_cast<char*>(&count), 1);
                    n = count;
                }
                else if (prop.listCountType == "int" || prop.listCountType == "uint")
                {
                    n = read_uint32_le(f);
                }
                else
                {
                    n = read_uint32_le(f);
                }

                if (n < 3) break;

                std::vector<int> idx(n);
                for (int j = 0; j < n; ++j)
                {
                    if (prop.listIndexType == "int" || prop.listIndexType == "uint")
                    {
                        int val = (int)read_uint32_le(f);
                        idx[j] = val;
                        if (val < minIdx) minIdx = val;
                        if (val > maxIdx) maxIdx = val;
                    }
                    else
                    {
                        f.ignore(4);
                    }
                }

                mesh.tris.push_back({ idx[0], idx[1], idx[2] });
                for (int j = 3; j < n; ++j)
                    mesh.tris.push_back({ idx[0], idx[j - 1], idx[j] });
            }
        }

        if (minIdx == 1 && maxIdx <= (int)verts.size())
        {
            for (auto& tri : mesh.tris)
            {
                tri.point[0]--; tri.point[1]--; tri.point[2]--;
            }
        }
    }

    char buf[256];
    sprintf_s(buf, "PLY loaded: %d vertices, %d triangles\n", (int)verts.size(), (int)mesh.tris.size());
    OutputDebugStringA(buf);
    return mesh;
}

mesh3D Engine3D::LoadFromStlFile(std::string filename)
{
    mesh3D mesh;

    std::ifstream test(filename, std::ios::binary);
    if (!test) return mesh;

    char header[80] = {};
    test.read(header, 80);
    std::string headerStr(header, 80);
    bool isAscii = (headerStr.substr(0, 5) == "solid" && headerStr.find("facet") == std::string::npos);
    test.close();

    if (isAscii)
    {
        std::ifstream f(filename);
        std::string line;
        int vCount = 0;
        while (std::getline(f, line))
        {
            if (line.find("vertex") != std::string::npos)
            {
                std::stringstream ss(line);
                std::string junk;
                vector3D v;
                ss >> junk >> v.x >> v.y >> v.z;
                verts.push_back(v);
                vCount++;
                if (vCount % 3 == 0)
                {
                    int idx0 = (int)verts.size() - 3;
                    int idx1 = (int)verts.size() - 2;
                    int idx2 = (int)verts.size() - 1;
                    mesh.tris.push_back({ idx0, idx1, idx2 });
                }
            }
        }
    }
    else
    {
        std::ifstream f(filename, std::ios::binary);
        f.seekg(80);
        uint32_t triCount = 0;
        f.read((char*)&triCount, 4);

        verts.reserve(verts.size() + triCount * 3);
        mesh.tris.reserve(triCount);

        for (uint32_t i = 0; i < triCount; ++i)
        {
            float normal[3];
            f.read((char*)normal, 12);
            for (int j = 0; j < 3; ++j)
            {
                float coord[3];
                f.read((char*)coord, 12);
                vector3D v = { coord[0], coord[1], coord[2] };
                verts.push_back(v);
            }
            uint16_t attrib;
            f.read((char*)&attrib, 2);

            int baseIdx = (int)verts.size() - 3;
            mesh.tris.push_back({ baseIdx, baseIdx + 1, baseIdx + 2 });
        }
    }
    return mesh;
}

void Engine3D::FovPlus()
{
    if (fov < 150) fov += 5;
}

void Engine3D::FovMinus()
{
    if (fov > 0) fov -= 5;
}