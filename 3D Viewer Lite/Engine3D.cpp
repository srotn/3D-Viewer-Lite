#include "Engine3D.h"

Engine3D::Engine3D()
{
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

    // 只在尚未创建 factory 时创建
    if (!m_pD2DFactory)
    {
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
        if (FAILED(hr)) return false;
    }

    RECT rc;
    GetClientRect(hwnd, &rc);
    m_width = rc.right - rc.left;
    m_height = rc.bottom - rc.top;

    // 释放旧渲染目标（若存在），以便安全重建
    if (m_pRenderTarget)
    {
        m_pRenderTarget->Release();
        m_pRenderTarget = nullptr;
    }

    HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(m_width, m_height)),
        &m_pRenderTarget
    );

    if (FAILED(hr)) return false;

    // ----------------- 【初始化 CPU 画布与显卡纹理桥梁】 -----------------
    // 1. 初始化 CPU 内存画布大小
    m_frameBuffer.assign(m_width * m_height, 0xFF000000); // 默认为全黑不透明

    // 2. 创建一个同等大小的 GPU 纹理位图
    if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
    hr = m_pRenderTarget->CreateBitmap(
        D2D1::SizeU(m_width, m_height),
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        &m_pBackBufferBitmap
    );
    // ------------------------------------------------------------------

    QueryPerformanceFrequency(&m_freq);
    QueryPerformanceCounter(&m_lastTime);
    m_firstFrame = true;

    return SUCCEEDED(hr);
}

void Engine3D::Resize(int width, int height)
{
    m_width = width;
    m_height = height;

    if (m_pRenderTarget)
    {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));

        // 随着窗口缩放，同步重新分配 CPU 画布和显卡位图的大小
        m_frameBuffer.resize(width * height);

        if (m_pBackBufferBitmap) { m_pBackBufferBitmap->Release(); m_pBackBufferBitmap = nullptr; }
        m_pRenderTarget->CreateBitmap(
            D2D1::SizeU(width, height),
            D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
            &m_pBackBufferBitmap
        );
    }
}

void Engine3D::BeginDraw()
{
    if (m_pRenderTarget)
        m_pRenderTarget->BeginDraw();
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

//----------------------------------------------------
//核心渲染工作区域
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
        for(int j = lux; j < rdx; j++)
        {
            if (i >= 0 && i < m_height && j >= 0 && j < m_width)
            {
				m_frameBuffer[i * m_width + j] = target;
			}
        }
    }
}

void Engine3D::Fill(triangle3D tri, short transparency, uint32_t color)
{
    //color mixing
    uint32_t alpha = static_cast<uint32_t>(transparency) & 0xFF;
    uint32_t target = (alpha << 24) | (color & 0x00FFFFFF);


    
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
    {
        // 4. 左上, X主导
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
    {
        // 5. 右下, Y主导
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
    {
        // 6. 右上, Y主导
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
    {
        // 7. 左下, Y主导
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
    {
        // 8. 左上, Y主导
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
    {
        // 9. 纯水平向右 (包含起点终点重合的点)
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
    {
        // 10. 纯水平向左
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
    {
        // 11. 纯垂直向下
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
    {
        // 12. 纯垂直向上
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
        Drawline((int)tri.point[i].x, (int)tri.point[i].y,
            (int)tri.point[j].x, (int)tri.point[j].y,
            255, color);
    }
}
//----------------------------------------------------

void Engine3D::DrawMesh3D(mesh3D Centered, float fElapsedTime)
{
    mesh3D Projected = Centered;
    mesh3D PreProcessed = {};
    PreProcessed.tris.reserve(Centered.tris.size());

	//1 preprocessing: rotation, backface culling, projection
    for (int i = 0; i < Centered.tris.size(); i++)
    {
        //step1 rotation
        for (int j = 0; j < 3; j++)
        {

            Projected.tris[i].point[j] = MtimesV(RotationYaw, Projected.tris[i].point[j]);
            Projected.tris[i].point[j] = MtimesV(RotationPitch, Projected.tris[i].point[j]);
            Projected.tris[i].point[j].x *= zoom; // 放大顶点坐标以适应显示
            Projected.tris[i].point[j].y *= zoom;
            Projected.tris[i].point[j].z *= zoom;
        }

        //step2 backface culling
        double px = Projected.tris[i].point[1].x;
        double py = Projected.tris[i].point[1].y;
        double pz = Projected.tris[i].point[1].z;

        vector3D NormalVector = {
            (Projected.tris[i].point[1].y - Projected.tris[i].point[0].y) * (Projected.tris[i].point[2].z - Projected.tris[i].point[0].z) - (Projected.tris[i].point[1].z - Projected.tris[i].point[0].z) * (Projected.tris[i].point[2].y - Projected.tris[i].point[0].y),
            (Projected.tris[i].point[1].z - Projected.tris[i].point[0].z) * (Projected.tris[i].point[2].x - Projected.tris[i].point[0].x) - (Projected.tris[i].point[1].x - Projected.tris[i].point[0].x) * (Projected.tris[i].point[2].z - Projected.tris[i].point[0].z),
            (Projected.tris[i].point[1].x - Projected.tris[i].point[0].x) * (Projected.tris[i].point[2].y - Projected.tris[i].point[0].y) - (Projected.tris[i].point[1].y - Projected.tris[i].point[0].y) * (Projected.tris[i].point[2].x - Projected.tris[i].point[0].x)
        };
		NormalVector = NormalVector.normalize();
        vector3D ViewVector = { px, py, pz + distance };

        double NormalValue = NormalVector.dot(ViewVector);
        if (NormalValue < 0)
        {
            continue;
        }

		//store NormalVector and ViewVector for lighting calculation in the future
        Projected.tris[i].NormalVector = NormalVector;
        Projected.tris[i].ViewVector = ViewVector;

        //step3 projection
        for (int j = 0; j < 3; j++)
        {

            double x = Projected.tris[i].point[j].x;
            double y = Projected.tris[i].point[j].y;
            double z = Projected.tris[i].point[j].z;
            Projected.tris[i].point[j].x = 1.5 * unit * x * distance / (distance + z) + ScreenWidth() / 2.0;
            Projected.tris[i].point[j].y = 1.5 * unit * y * distance / (distance + z) + ScreenHeight() / 2.0;
        }
		PreProcessed.tris.push_back(Projected.tris[i]);
    }

	//2 sorting by depth (Painter's algorithm)
    std::vector<std::pair<double, size_t>> depthIndices;
    depthIndices.reserve(PreProcessed.tris.size());

    for (size_t i = 0; i < PreProcessed.tris.size(); i++)
    {
        double aDepth = (PreProcessed.tris[i].point[0].z + PreProcessed.tris[i].point[1].z + PreProcessed.tris[i].point[2].z) / 3.0;
        depthIndices.push_back({ aDepth, i });
    }

    std::sort(depthIndices.begin(), depthIndices.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
        });

    //3 fill & lighting
	for (int i = 0; i < depthIndices.size(); i++)
    {
        size_t idx = depthIndices[i].second;
        const auto& tri = PreProcessed.tris[idx];

        double R_Intensity = 256 * tri.NormalVector.dot(Rlight.normalize());
        double G_Intensity = 256 * tri.NormalVector.dot(Glight.normalize());
        double B_Intensity = 256 * tri.NormalVector.dot(Blight.normalize());

		if (R_Intensity < 0) R_Intensity = 0;
        if (G_Intensity < 0) G_Intensity = 0;
        if (B_Intensity < 0) B_Intensity = 0;

        //Fill(tri, 128, RGB(R_Intensity, G_Intensity, B_Intensity));
        
        //4 draw wireframe
        DrawTriangle(tri, 256, 0x00ffffff);
    }
}

mesh3D Engine3D::MoveToCenter(mesh3D mesh)
{
    double cx = 0, cy = 0, cz = 0;
    std::set<vector3D> allpoints;

    for (const triangle3D& tri : mesh.tris)
    {
        for (int i = 0; i < 3; i++)
        {
            allpoints.insert(tri.point[i]);
        }
    }

    if (allpoints.empty())
    {
        // 无顶点，直接返回原始网格（避免除以0）
        return mesh;
    }

    for (const vector3D& point : allpoints)
    {
        cx += point.x;
        cy += point.y;
        cz += point.z;
    }

    cx /= static_cast<double>(allpoints.size());
    cy /= static_cast<double>(allpoints.size());
    cz /= static_cast<double>(allpoints.size());

    for (size_t i = 0; i < mesh.tris.size(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            mesh.tris[i].point[j].x -= cx;
            mesh.tris[i].point[j].y -= cy;
            mesh.tris[i].point[j].z -= cz;
        }
    }

    return mesh;
}

bool Engine3D::OnUserCreate()
{
    // read obj file
	meshInput = LoadFromObjectFile(name);

    meshInput = MoveToCenter(meshInput);

    return true;
}

bool Engine3D::OnUserUpdate(float fElapsedTime)
{
    //WORK
    Fill(255, 0x404040);

	CreateRotationMatrix(yaw, pitch);
    double fov_rad = fov * 3.1415926535 / 180.0;
    if (fov <= 0.0)
    {
        distance = 10000.0;   // 正交
    }
    else
    {
        distance = 4.0 / tan(fov_rad * 0.5);
        const double MIN_DIST = 1.5;
        if (distance < MIN_DIST) distance = MIN_DIST;
        if (distance > 1000.0) distance = 1000.0;
    }
    unit = sqrt(ScreenHeight() * ScreenHeight() + ScreenWidth() * ScreenWidth()) / 16;

	DrawMesh3D(meshInput, fElapsedTime);

    return true;
}

void Engine3D::Render()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float deltaTime = 0.016f;   // 默认值

    if (!m_firstFrame)
    {
        deltaTime = (float)((now.QuadPart - m_lastTime.QuadPart) / (double)m_freq.QuadPart);
        // 限制最大增量（比如 0.1 秒），防止调试断点导致跳跃太大
        if (deltaTime > 0.1f) deltaTime = 0.016f;
    }
    else
    {
        m_firstFrame = false;
    }
    m_lastTime = now;

    BeginDraw();
    OnUserUpdate(deltaTime);
    EndDraw();
}

void Engine3D::UpdateYawAndPitch(int delta_x, int delta_y)
{
    yaw -= static_cast<double>(delta_x) / static_cast<double>(400) * 3.1415926;
    pitch += static_cast<double>(delta_y) / static_cast<double>(400) * 3.1415926;
	// 限制 pitch 在 -90 到 +90 度之间
    if (pitch >= 3.14159 / 2) pitch = 3.14159 / 2;
    if (pitch <= -3.14159 / 2) pitch = -3.14159 / 2;
}

void Engine3D::CreateRotationMatrix(double yaw, double pitch)
{
    matrix rotation;
    double cosYaw = cos(yaw);
    double sinYaw = sin(yaw);
    double cosPitch = cos(pitch);
    double sinPitch = sin(pitch);
    RotationYaw = {
        {
        { cosYaw, 0, sinYaw, 0 },
        { 0, 1, 0, 0 },
        { -sinYaw, 0, cosYaw, 0 },
        { 0, 0, 0, 1 }
        }
    };
    RotationPitch = {
        {
        { 1, 0, 0, 0 },
        { 0, cosPitch, -sinPitch, 0 },
        { 0, sinPitch, cosPitch, 0 },
        { 0, 0, 0, 1 }
        }
	};
}

vector3D Engine3D::MtimesV(matrix m, vector3D v)
{
    vector3D result;
    result.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3];
    result.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3];
    result.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3];
    return result;
}

mesh3D Engine3D::LoadFromObjectFile(std::string filename) 
{
    std::ifstream f(filename);
    mesh3D mesh;
    std::vector<vector3D> verts; // 暂存所有顶点

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
           
            int v[4] = {0};
            int vt[4] = {0};
			int vn[4] = {0};

            for(int i = 0; i < 4; i++) 
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
            mesh.tris.push_back({ verts[v[0] - 1], verts[v[1] - 1], verts[v[2] - 1] });
            if (v[3] != 0) mesh.tris.push_back({ verts[v[0] - 1], verts[v[2] - 1], verts[v[3] - 1] });
        }
    }
    return mesh;
}