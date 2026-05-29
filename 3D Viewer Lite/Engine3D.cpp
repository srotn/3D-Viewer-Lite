#include "Engine3D.h"

// 一个简单的内联计时助手
struct ProfileTimer {
    std::string name;
    std::chrono::high_resolution_clock::time_point start;

    ProfileTimer(const std::string& sectionName) : name(sectionName) {
        start = std::chrono::high_resolution_clock::now();
    }

    ~ProfileTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        float duration = std::chrono::duration<float, std::milli>(end - start).count();

        // 格式化输出到 Visual Studio 的输出窗口
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

    // 1. 创建 DX11 设备与交换链
    if (!pd3dDevice)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = m_width;
        sd.BufferDesc.Height = m_height;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // D2D 最喜欢的颜色格式
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

        // 创建 DX11 渲染目标视图
        ID3D11Texture2D* pBackBufferTex = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBufferTex);
        if (pBackBufferTex) {
            pd3dDevice->CreateRenderTargetView(pBackBufferTex, NULL, &pmainRenderTargetView);
            pBackBufferTex->Release();
        }
    }

    // 2. 创建 D2D 工厂
    if (!m_pD2DFactory)
    {
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
        if (FAILED(hr)) return false;
    }

    // 3. 【核心桥梁】：将 D2D 渲染目标绑定到 DX11 的后缓冲区
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

    // 4. 初始化 CPU 画布与显卡纹理桥梁
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
        // A. 必须先释放所有占用了后缓冲区引用的 D2D 与 DX11 资源
        if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
        if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
        if (pmainRenderTargetView) { pmainRenderTargetView->Release(); pmainRenderTargetView = nullptr; }

        // B. 调整 DX11 交换链尺寸
        pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // C. 重新创建 DX11 渲染目标视图
        ID3D11Texture2D* pBackBufferTex = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBufferTex);
        if (pBackBufferTex) {
            pd3dDevice->CreateRenderTargetView(pBackBufferTex, NULL, &pmainRenderTargetView);
            pBackBufferTex->Release();
        }

        // D. 重新将 D2D 渲染目标挂载到新的 DX11 后缓冲区
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

        // E. 重新配置 CPU 画布和显卡位图和深度缓冲图的大小
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
    // ====== 新增：清空深度缓冲区（填入无穷大） ======
    std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), std::numeric_limits<float>::infinity());
}

void Engine3D::EndDraw()
{
    if (m_pRenderTarget)
    {
        // ----------------- 【核心桥梁：将 CPU 纯手写像素统一上传 GPU】 -----------------
        if (m_pBackBufferBitmap)
        {
            // A. 把我们写满像素数据的 m_frameBuffer 整个复制到显卡纹理中
            m_pBackBufferBitmap->CopyFromMemory(
                nullptr,
                m_frameBuffer.data(),
                m_width * sizeof(uint32_t) // 步长：每行有多少字节
            );

            // B. 用显卡把这块纹理直接拉伸/贴到屏幕上（极速一枪头提交）
            m_pRenderTarget->DrawBitmap(m_pBackBufferBitmap);
        }
        // -------------------------------------------------------------------------

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
    //color mixing
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

    // 默认不传参时，maxClipY 自动设为屏幕最底端
    if (maxClipY == -1) maxClipY = m_height - 1;

    vector3D A = transformedvectors[tri.point[0]];
    vector3D B = transformedvectors[tri.point[1]];
    vector3D C = transformedvectors[tri.point[2]];

    // 【核心改动】：让包围盒的 Y 范围不仅裁剪到屏幕，还要裁剪到当前线程负责的 [minClipY, maxClipY] 区域
    int minX = std::max(0, (int)std::floor(std::min({ A.x, B.x, C.x })));
    int maxX = std::min(m_width - 1, (int)std::ceil(std::max({ A.x, B.x, C.x })));
    int minY = std::max(minClipY, (int)std::floor(std::min({ A.y, B.y, C.y })));
    int maxY = std::min(maxClipY, (int)std::ceil(std::max({ A.y, B.y, C.y })));

    // 如果整个三角形都不在这个线程的管辖范围内，直接退出
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

                

                // 【此时绝对安全】：因为不同的 y 严格属于不同的线程，绝无冲突！
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
    //color mixing
    uint32_t alpha = static_cast<uint32_t>(transparency) & 0xFF;
    uint32_t target = (alpha << 24) | (color & 0x00FFFFFF);

    // 1.右下，x主导
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
    // 2. 右上, X主导
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
                // y_1 应该减小。当理想纵向进度比当前大 0.5 像素时执行 y_1--
                if ((x_1 - x1) * (y1 - y2) * 2 > (2 * (y1 - y_1) + 1) * (x2 - x1)) y_1--;
            }
        }
    }
    // 3. 左下, X主导
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
                // x_1 在减小，进度为 (x1 - x_1)。当理想纵向进度大 0.5 时执行 y_1++
                if ((x1 - x_1) * (y2 - y1) * 2 > (2 * (y_1 - y1) + 1) * (x1 - x2)) y_1++;
            }
        }
    }
    // 4. 左上, X主导
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
                // x_1 和 y_1 都在减小
                if ((x1 - x_1) * (y1 - y2) * 2 > (2 * (y1 - y_1) + 1) * (x1 - x2)) y_1--;
            }
        }
    }
    // 5. 右下, Y主导
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
                // 交换 X 和 Y 的角色，当理想横向进度比当前大 0.5 时执行 x_1++
                if ((y_1 - y1) * (x2 - x1) * 2 > (2 * (x_1 - x1) + 1) * (y2 - y1)) x_1++;
            }
        }
    }
    // 6. 右上, Y主导
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
                // y_1 减小，进度为 (y1 - y_1)，x_1 应该增加
                if ((y1 - y_1) * (x2 - x1) * 2 > (2 * (x_1 - x1) + 1) * (y1 - y2)) x_1++;
            }
        }
    }
    // 7. 左下, Y主导
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
                // y_1 增加，x_1 应该减小
                if ((y_1 - y1) * (x1 - x2) * 2 > (2 * (x1 - x_1) + 1) * (y2 - y1)) x_1--;
            }
        }
    }
    // 8. 左上, Y主导
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
                // y_1 和 x_1 都在减小
                if ((y1 - y_1) * (x1 - x2) * 2 > (2 * (x1 - x_1) + 1) * (y1 - y2)) x_1--;
            }
        }
    }
    // 9. 纯水平向右 (包含起点终点重合的点)
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
                // 纯水平线，y_1 始终保持不变，无需误差判定
            }
        }
    }
    // 10. 纯水平向左
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
                // 纯水平线，y_1 始终保持不变，无需误差判定
            }
        }
    }
    // 11. 纯垂直向下
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
                // 纯垂直线，x_1 始终保持不变，无需误差判定
            }
        }
    }
    // 12. 纯垂直向上
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
                // 纯垂直线，x_1 始终保持不变，无需误差判定
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
        //points process
        //parallel process
#pragma omp parallel for
        for (int i = 0; i < verts.size(); i++)
        {
            vector3D vpoint = verts[i];

            //step1 rotation
            vpoint = MtimesV(Rotation, vpoint);
            vpoint.x *= zoom; // 放大顶点坐标以适应显示
            vpoint.y *= zoom;
            vpoint.z *= zoom;

            //step2 projection
            float x = vpoint.x;
            float y = vpoint.y;
            float z = vpoint.z;
            vpoint.x = 1.5 * unit * x * distance / (distance + z) + ScreenWidth() / 2.0;
            vpoint.y = 1.5 * unit * y * distance / (distance + z) + ScreenHeight() / 2.0;

            transformedvectors[i] = vpoint;
        }
    }

    int num_threads = omp_get_max_threads(); // 获取当前系统的最大可用CPU线程数
    int strip_height = m_height / num_threads; // 计算每个线程分到的屏幕高度

    // =================================================================
    // 终极优化：二维无锁并行分箱架构
    // =================================================================

    struct PreparedTriangle {
        triangle3D tri;
        uint32_t color;
    };

    // 二维任务箱：localBoxes[几何线程ID][目标屏幕条带t]
    // 每个线程只写自己专属的一行，绝对不会产生数据竞争(Data Race)，因此不需要任何锁！
    std::vector<std::vector<std::vector<PreparedTriangle>>> localBoxes(
        num_threads, std::vector<std::vector<PreparedTriangle>>(num_threads)
    );

    {
        ProfileTimer t_binning("   [DrawMesh3D] P2-1: Geometry & Binning Pass");

        // 【优化一】把灯光的归一化（包含昂贵的 sqrt 耗时操作）提到循环外面！一帧只算一次
        vector3D rLightNorm = Rlight.normalize();
        vector3D gLightNorm = Glight.normalize();
        vector3D bLightNorm = Blight.normalize();

        // 【优化二】让几何与分箱阶段也享受多线程并发！
#pragma omp parallel for
        for (int i = 0; i < (int)meshInput.tris.size(); i++)
        {
            int tid = omp_get_thread_num(); // 获取当前执行任务的线程 ID
            triangle3D tri = meshInput.tris[i];

            // 1. 背面剔除
            float NormalValue = (transformedvectors[tri.point[1]].x - transformedvectors[tri.point[0]].x) * (transformedvectors[tri.point[2]].y - transformedvectors[tri.point[0]].y) - (transformedvectors[tri.point[1]].y - transformedvectors[tri.point[0]].y) * (transformedvectors[tri.point[2]].x - transformedvectors[tri.point[0]].x);
            if (NormalValue > 0) continue;

            // 2. 光照计算（使用外面算好的归一化向量，纯乘加运算，极快）
            tri.NormalVector = MtimesV(Rotation, tri.NormalVector);

            float R_Intensity = -256 * tri.NormalVector.dot(rLightNorm);
            float G_Intensity = -256 * tri.NormalVector.dot(gLightNorm);
            float B_Intensity = -256 * tri.NormalVector.dot(bLightNorm);

            if (R_Intensity < 0) R_Intensity = 0;
            if (G_Intensity < 0) G_Intensity = 0;
            if (B_Intensity < 0) B_Intensity = 0;

            // 计算最终 RGB，每个通道 = 对应光源强度 * 对应光源颜色分量
            float finalR = R_Intensity * lightColorR.x + G_Intensity * lightColorG.x + B_Intensity * lightColorB.x;
            float finalG = R_Intensity * lightColorR.y + G_Intensity * lightColorG.y + B_Intensity * lightColorB.y;
            float finalB = R_Intensity * lightColorR.z + G_Intensity * lightColorG.z + B_Intensity * lightColorB.z;
            // 除以 3 保持亮度范围，可根据需要调整
            finalR /= 3.0f; finalG /= 3.0f; finalB /= 3.0f;
            // 裁剪到 0-255
            BYTE r = (BYTE)std::min(255.0f, std::max(0.0f, finalR));
            BYTE g = (BYTE)std::min(255.0f, std::max(0.0f, finalG));
            BYTE b = (BYTE)std::min(255.0f, std::max(0.0f, finalB));
            uint32_t finalColor = RGB(r, g, b);

            // 3. 计算该三角形在屏幕上的实际 Y 轴边界
            float ay = transformedvectors[tri.point[0]].y;
            float by = transformedvectors[tri.point[1]].y;
            float cy = transformedvectors[tri.point[2]].y;
            int triMinY = (int)std::floor(std::min({ ay, by, cy }));
            int triMaxY = (int)std::ceil(std::max({ ay, by, cy }));

            // 【优化三】用 O(1) 的数学计算直接定位目标条带范围，彻底干掉整个屏幕宽度的循环判断
            int start_t = std::max(0, triMinY / strip_height);
            int end_t = std::min(num_threads - 1, triMaxY / strip_height);

            PreparedTriangle pt = { tri, finalColor };
            for (int t = start_t; t <= end_t; t++)
            {
                // 往自己线程对应的私有格子里塞数据，无锁并发，内存吞吐量拉满
                localBoxes[tid][t].push_back(pt);
            }
        }
    }

    // 2. 纯净的并行光栅化阶段
    if (IsFillAndLight)
    {
        ProfileTimer t_raster_pure("   [DrawMesh3D] P2-2: Pure Multi-Thread Rasterization");

#pragma omp parallel for schedule(dynamic, 1)
        for (int t = 0; t < num_threads; t++)
        {
            int minClipY = t * strip_height;
            int maxClipY = (t == num_threads - 1) ? (m_height - 1) : (minClipY + strip_height - 1);

            // 当前渲染线程 t，去收集所有几何线程 tid 投递到第 t 个条带里的三角形
            for (int tid = 0; tid < num_threads; tid++)
            {
                for (size_t i = 0; i < localBoxes[tid][t].size(); i++)
                {
                    Fill(localBoxes[tid][t][i].tri, 128, localBoxes[tid][t][i].color, minClipY, maxClipY);
                }
            }
        }
    }

    // ==========================================
    // 阶段 3：绘制线框（保持单线程串行，避免多线程画线冲突）
    // ==========================================
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
    
    return;
}

bool Engine3D::OnUserCreate()
{
    // read obj file
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

    // ===== 新增：自动计算合适的初始缩放 =====
    // 1. 计算模型的包围盒
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

    // 防止模型为空或所有顶点重合
    if (maxDimension < 0.0001f)
        maxDimension = 1.0f;

    // 2. 设定模型在屏幕上占据的比例（例如 70%）
    const float fillRatio = 0.7f;

    // 3. 计算当前屏幕相关的参数
    int screenDim = std::min(m_width, m_height);          // 取屏幕较小的维度，保证所有方向可见
    float unitVal = std::sqrt((float)(m_width * m_width + m_height * m_height)) / 16.0f; // 与渲染时一致

    // 4. 计算 zoom
    // 屏幕坐标 x_screen ≈ 1.5 * unit * zoom * x_world   （忽略透视深度）
    // 因此 maxDimension * 1.5 * unit * zoom = screenDim * fillRatio
    zoom = (screenDim * fillRatio) / (1.5f * unitVal * maxDimension);

    // ===== 自动缩放结束 =====


    for (int i = 0; i < meshInput.tris.size(); i++)
    {
        triangle3D tri = meshInput.tris[i];

        vector3D NormalVector = {
            (verts[tri.point[1]].y - verts[tri.point[0]].y)* (verts[tri.point[2]].z - verts[tri.point[0]].z) - (verts[tri.point[1]].z - verts[tri.point[0]].z) * (verts[tri.point[2]].y - verts[tri.point[0]].y),
            (verts[tri.point[1]].z - verts[tri.point[0]].z)* (verts[tri.point[2]].x - verts[tri.point[0]].x) - (verts[tri.point[1]].x - verts[tri.point[0]].x) * (verts[tri.point[2]].z - verts[tri.point[0]].z),
            (verts[tri.point[1]].x - verts[tri.point[0]].x)* (verts[tri.point[2]].y - verts[tri.point[0]].y) - (verts[tri.point[1]].y - verts[tri.point[0]].y) * (verts[tri.point[2]].x - verts[tri.point[0]].x)
        };

        NormalVector = NormalVector.normalize();
        meshInput.tris[i].NormalVector = NormalVector;
    }
    transformedvectors = verts;
    return true;
}

bool Engine3D::OnUserUpdate(float fElapsedTime)
{
    //WORK
    Fill(255, 0x404040);

    CreateRotationMatrix(yaw, pitch);
    float fov_rad = fov * 3.1415926535 / 180.0;
    if (fov <= 0.0)
    {
        distance = 10000.0;   // 正交
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
    // 整个 Render 函数的生命周期，统计单帧的总总耗时
    ProfileTimer t_total("=== Total Render Frame ===");
    
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float deltaTime = 0.016f;   // 默认值

    if (!m_firstFrame)
    {
        deltaTime = (float)((now.QuadPart - m_lastTime.QuadPart) / (float)m_freq.QuadPart);
        // 限制最大增量（比如 0.1 秒），防止调试断点导致跳跃太大
        if (deltaTime > 0.1f) deltaTime = 0.016f;
    }
    else
    {
        m_firstFrame = false;
    }
    m_lastTime = now;

    // 1. 监控 BeginDraw (清空画布与 Z-Buffer 耗时)
    {
        ProfileTimer t_begin("1. BeginDraw (Memory Buffer Clear)");
        BeginDraw();
    }

    // 2. 监控核心逻辑、几何计算与多线程光栅化 (调用 DrawMesh3D 的地方)
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
    // 限制 pitch 在 -90 到 +90 度之间
    if (pitch >= 3.14159 / 2) pitch = 3.14159 / 2;
    if (pitch <= -3.14159 / 2) pitch = -3.14159 / 2;
}

void Engine3D::CreateRotationMatrix(float yaw, float pitch)
{
    matrix rotation;
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
        { // 读取顶点
            std::stringstream ss(line);
            vector3D v;
            ss >> junk >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }
        else if (line[0] == 'f' && line[1] == ' ')
        { // 读取面索引
            std::stringstream ss(line);
            std::vector<std::string> face(4);
            face[3] = ""; // 确保第四个元素存在，避免越界
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
            // OBJ是从1开始索引的，C++ vector是从0开始的，所以要 -1
            mesh.tris.push_back({v[0] - 1, v[1] - 1, v[2] - 1});
            if (v[3] != 0) mesh.tris.push_back({v[0] - 1, v[2] - 1, v[3] - 1});
        }
    }
    return mesh;
}

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

// 用于描述 PLY 属性
struct PlyProperty
{
    std::string name;
    std::string type;           // "float", "uchar", "int" 等
    bool isList = false;
    std::string listCountType;  // 长度的类型
    std::string listIndexType;  // 索引的类型
};

// 辅助：读取一个小端 uint32（跨平台兼容）
static uint32_t read_uint32_le(std::ifstream& f)
{
    uint32_t val;
    f.read(reinterpret_cast<char*>(&val), sizeof(val));
    return val;
}

// 辅助：读取一个小端 float
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
    std::string format;               // "ascii", "binary_little_endian" 等
    int vertexCount = 0, faceCount = 0;

    // 存储顶点和面元素的属性定义
    std::vector<PlyProperty> vertexProps;
    std::vector<PlyProperty> faceProps;
    bool inVertex = false, inFace = false;

    // ---------- 解析头部 ----------
    while (std::getline(f, line))
    {
        // 去除行首尾空白
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
            std::string version; // 忽略
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
                // 例：property float x
                PlyProperty prop;
                ss >> prop.type >> prop.name;
                vertexProps.push_back(prop);
            }
            else if (inFace)
            {
                // 可能为普通属性或 list
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
                    // 普通属性，例如 property uchar red
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

    // 是否为二进制模式
    bool isBinary = (format.find("binary") != std::string::npos);

    if (!isBinary)
    {
        // ========== ASCII 模式（保留原逻辑）==========
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
        // ========== 二进制模式 ==========
        // 当前文件指针已经在 end_header 之后，也就是二进制数据起始处

        // 读取顶点数据
        verts.reserve(verts.size() + vertexCount);
        for (int i = 0; i < vertexCount; ++i)
        {
            vector3D v;
            bool xyzRead = false;
            // 按照顶点属性顺序读取
            for (auto& prop : vertexProps)
            {
                if (prop.name == "x")
                {
                    v.x = read_float_le(f);
                    xyzRead = true;
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
                    // 跳过不需要的属性
                    if (prop.type == "float")
                        read_float_le(f);
                    else if (prop.type == "uchar")
                        f.ignore(1);
                    else if (prop.type == "int")
                        f.ignore(4);
                    else if (prop.type == "uint")
                        f.ignore(4);
                    else
                        f.ignore(4); // 未知类型，跳过 4 字节
                }
            }
            verts.push_back(v);
        }

        // 读取面数据
        mesh.tris.reserve(faceCount);
        int minIdx = INT_MAX, maxIdx = INT_MIN;

        for (int i = 0; i < faceCount; ++i)
        {
            // 先处理所有非 list 属性（如颜色），直接跳过
            for (auto& prop : faceProps)
            {
                if (prop.isList)
                    continue; // list 稍后处理
                // 跳过固定属性
                if (prop.type == "uchar")       f.ignore(1);
                else if (prop.type == "float")  f.ignore(4);
                else if (prop.type == "int")    f.ignore(4);
                else if (prop.type == "uint")   f.ignore(4);
                else                             f.ignore(4);
            }

            // 读取 list 属性（顶点索引）
            for (auto& prop : faceProps)
            {
                if (!prop.isList) continue;

                // 读取长度计数
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
                    // 默认尝试读一个 int
                    n = read_uint32_le(f);
                }

                if (n < 3) break; // 非法面

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
                        // 其他类型暂不处理
                        f.ignore(4);
                    }
                }

                // 扇形剖分
                mesh.tris.push_back({ idx[0], idx[1], idx[2] });
                for (int j = 3; j < n; ++j)
                    mesh.tris.push_back({ idx[0], idx[j - 1], idx[j] });
            }
        }

        // 索引偏移处理（如果是 1‑based）
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

    // 先用文本方式试探文件头，判断是 ASCII 还是二进制
    std::ifstream test(filename, std::ios::binary);
    if (!test) return mesh;

    char header[80] = {};
    test.read(header, 80);
    std::string headerStr(header, 80);
    // ASCII STL 以 "solid" 开头
    bool isAscii = (headerStr.substr(0, 5) == "solid" && headerStr.find("facet") == std::string::npos);
    test.close();

    if (isAscii)
    {
        std::ifstream f(filename);
        std::string line;
        vector3D tmp;   // 用于跳过法线
        int vCount = 0; // 累计顶点索引
        while (std::getline(f, line))
        {
            // 查找 "vertex"
            if (line.find("vertex") != std::string::npos)
            {
                std::stringstream ss(line);
                std::string junk;
                vector3D v;
                ss >> junk >> v.x >> v.y >> v.z;
                verts.push_back(v);
                vCount++;
                // 每三个顶点组成一个三角形面
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
    else // 二进制 STL
    {
        std::ifstream f(filename, std::ios::binary);
        f.seekg(80);                     // 跳过 80 字节头
        uint32_t triCount = 0;
        f.read((char*)&triCount, 4);     // 三角形数量 (小端)

        verts.reserve(verts.size() + triCount * 3);
        mesh.tris.reserve(triCount);

        for (uint32_t i = 0; i < triCount; ++i)
        {
            // 法线 (12字节) + 3个顶点 (各12字节) + 属性计数 (2字节)
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
    if (fov < 150)fov += 5;
}

void Engine3D::FovMinus()
{
    if (fov > 0)fov -= 5;
}