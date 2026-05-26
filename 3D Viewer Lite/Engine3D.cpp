#include "Engine3D.h"

Engine3D::Engine3D()
{
}

Engine3D::~Engine3D()
{
    // 清理笔刷缓存（防空检查）
    for (auto& pair : m_brushCache)
    {
        if (pair.second) pair.second->Release();
    }
    m_brushCache.clear();

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

    QueryPerformanceFrequency(&m_freq);
    QueryPerformanceCounter(&m_lastTime);
    m_firstFrame = true;

    return SUCCEEDED(hr);
}

void Engine3D::Resize(int width, int height)
{
    if (m_pRenderTarget)
    {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }
    m_width = width;
    m_height = height;
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
        HRESULT hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET)
        {
            // 先释放并清空与旧渲染目标绑定的资源（笔刷等）
            for (auto& pair : m_brushCache)
            {
                if (pair.second) pair.second->Release();
            }
            m_brushCache.clear();

            if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }

            // 重新初始化渲染目标（factory 已由 Initialize 管理）
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

ID2D1SolidColorBrush* Engine3D::GetBrush(COLORREF color)
{
    // 如果没有有效渲染目标，直接返回空，调用方应检查返回值
    if (!m_pRenderTarget) return nullptr;

    auto it = m_brushCache.find(color);
    if (it != m_brushCache.end())
    {
        return it->second;
    }

    float r = GetRValue(color) / 255.0f;
    float g = GetGValue(color) / 255.0f;
    float b = GetBValue(color) / 255.0f;
    ID2D1SolidColorBrush* brush = nullptr;
    HRESULT hr = m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(r, g, b),
        &brush
    );
    if (SUCCEEDED(hr) && brush)
    {
        m_brushCache[color] = brush;
        return brush;
    }
    // 创建失败返回 nullptr
    return nullptr;
}

void Engine3D::Fill(short transparency, int color)
{
    // 透明参数忽略，Direct2D 可以设置笔刷透明度，这里简单填充
    if (!m_pRenderTarget) return;
    COLORREF col = static_cast<COLORREF>(color);
    ID2D1SolidColorBrush* brush = GetBrush(col);
    if (brush)
    {
        // 设置透明度（可选）
        brush->SetOpacity(transparency / 255.0f);
        m_pRenderTarget->Clear(D2D1::ColorF(GetRValue(col) / 255.0f, GetGValue(col) / 255.0f, GetBValue(col) / 255.0f));
    }
}

void Engine3D::Fill(int lux, int luy, int rdx, int rdy, short transparency, COLORREF color)
{
    if (!m_pRenderTarget) return;
    ID2D1SolidColorBrush* brush = GetBrush(color);
    if (brush)
    {
        brush->SetOpacity(transparency / 255.0f);
        D2D1_RECT_F rect = D2D1::RectF((float)lux, (float)luy, (float)rdx, (float)rdy);
        m_pRenderTarget->FillRectangle(rect, brush);
        brush->SetOpacity(1.0f);
    }
}

void Engine3D::Fill(triangle3D tri, short transparency, int color)
{
    // 可选：实现三角形填充，使用路径几何或简单的三角形填充
    // 这里简单调用 DrawTriangle 绘制边框
    DrawTriangle(tri, transparency, color);
}

void Engine3D::Drawline(int x1, int y1, int x2, int y2, int width, COLORREF color)
{
    if (!m_pRenderTarget) return;
    ID2D1SolidColorBrush* brush = GetBrush(color);
    if (brush)
    {
        m_pRenderTarget->DrawLine(
            D2D1::Point2F((float)x1, (float)y1),
            D2D1::Point2F((float)x2, (float)y2),
            brush,
            (float)width
        );
    }
}

void Engine3D::DrawTriangle(triangle3D tri, short transparency, int color)
{
    for (int i = 0; i < 3; ++i)
    {
        int j = (i + 1) % 3;
        Drawline((int)tri.point[i].x, (int)tri.point[i].y,
            (int)tri.point[j].x, (int)tri.point[j].y,
            1, color);
    }
}

void Engine3D::DrawMesh3D(mesh3D& Centered, float fElapsedTime)
{
    // 原有的旋转逻辑保持不变（略，和原来一样）
    // ...
    // 注意：原代码中直接修改了 Centered.tris 的点坐标，这里维持原逻辑

    static double angle = 0;
    angle = fElapsedTime * 1.57;

    for (int i = 0; i < Centered.tris.size(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            double xspin = Centered.tris[i].point[j].x;
            double zspin = Centered.tris[i].point[j].z;
            double r = sqrt(xspin * xspin + zspin * zspin);
            double theta = atan2(zspin, xspin);
            theta = theta + angle;
            zspin = r * sin(theta);
            xspin = r * cos(theta);
            Centered.tris[i].point[j].x = xspin;
            Centered.tris[i].point[j].z = zspin;
        }
    }

    mesh3D Projected = Centered;
    for (int i = 0; i < Projected.tris.size(); i++)
    {
        for (int j = 0; j < 3; j++)
        {
            double xspin = Centered.tris[i].point[j].x;
            double zspin = Centered.tris[i].point[j].z;
            zspin = zspin += 2 * distance;
            Projected.tris[i].point[j].x = 1.5 * unit * xspin * distance / (distance + zspin) + ScreenWidth() / 2.0;
            Projected.tris[i].point[j].y = 1.5 * unit * Projected.tris[i].point[j].y * distance / (distance + zspin) + ScreenHeight() / 2.0;
        }
        double NormalValue = (Projected.tris[i].point[1].x - Projected.tris[i].point[0].x) * (Projected.tris[i].point[2].y - Projected.tris[i].point[0].y) - (Projected.tris[i].point[1].y - Projected.tris[i].point[0].y) * (Projected.tris[i].point[2].x - Projected.tris[i].point[0].x);
   
        if (NormalValue < 0)
        {
            DrawTriangle(Projected.tris[i], 128, RGB(0, 0, 0));
        }

        //lighting
        double NormalVector[3] = {
            (Projected.tris[i].point[1].y - Projected.tris[i].point[0].y) * (Projected.tris[i].point[2].z - Projected.tris[i].point[0].z) - (Projected.tris[i].point[1].z - Projected.tris[i].point[0].z) * (Projected.tris[i].point[2].y - Projected.tris[i].point[0].y),
            (Projected.tris[i].point[1].z - Projected.tris[i].point[0].z) * (Projected.tris[i].point[2].x - Projected.tris[i].point[0].x) - (Projected.tris[i].point[1].x - Projected.tris[i].point[0].x) * (Projected.tris[i].point[2].z - Projected.tris[i].point[0].z),
            (Projected.tris[i].point[1].x - Projected.tris[i].point[0].x) * (Projected.tris[i].point[2].y - Projected.tris[i].point[0].y) - (Projected.tris[i].point[1].y - Projected.tris[i].point[0].y) * (Projected.tris[i].point[2].x - Projected.tris[i].point[0].x)
        };
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
    std::ifstream file("icosahedron.obj");
    if (!file.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return false;
    }

    std::vector<vector3D> points;
    char type;
    while (file >> type)
    {
        if (type == 'g')
        {
            // 处理 g 类型（这里忽略）
            std::string restOfLine;
            std::getline(file, restOfLine);
            continue;
        }
        else if (type == 'v')
        {
            vector3D point;
            if (!(file >> point.x >> point.y >> point.z)) {
                // 格式错误，跳出或忽略该行
                break;
            }
			point.x *= 2; // 放大顶点坐标以适应显示
			point.y *= 2;
			point.z *= 2;
            points.push_back(point);
        }
        else if (type == 'f')
        {
            // 以字符串读取三项，支持 "v" 或 "v/vt/vn" 格式
            std::string a, b, c;
            if (!(file >> a >> b >> c)) {
                // 行格式不对，跳过
                continue;
            }
            auto parseVertexIndex = [](const std::string& token)->int {
                size_t pos = token.find('/');
                std::string num = (pos == std::string::npos) ? token : token.substr(0, pos);
                try {
                    return std::stoi(num);
                } catch (...) {
                    return 0;
                }
            };

            int idx[3];
            idx[0] = parseVertexIndex(a);
            idx[1] = parseVertexIndex(b);
            idx[2] = parseVertexIndex(c);

            // 验证索引有效性（OBJ 索引从 1 开始）
            if (idx[0] <= 0 || idx[1] <= 0 || idx[2] <= 0) continue;
            if (static_cast<size_t>(idx[0]) > points.size() || static_cast<size_t>(idx[1]) > points.size() || static_cast<size_t>(idx[2]) > points.size())
            {
                // 索引越界，跳过该面
                continue;
            }

            triangle3D tri;
            for (int i = 0; i < 3; ++i)
            {
                tri.point[i] = points[idx[i] - 1];
            }
            meshInput.tris.push_back(tri);
        }
        else
        {
            // 忽略未知行类型（例如 vn, vt 等）
            std::string rest;
            std::getline(file, rest);
            continue;
        }
    }

    CenteredInput = MoveToCenter(meshInput);

    return true;
}

bool Engine3D::OnUserUpdate(float fElapsedTime)
{
    //WORK
    Fill(255, 0x404040);


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

	DrawMesh3D(CenteredInput, fElapsedTime);

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

