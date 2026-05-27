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
    if (!m_pRenderTarget) return;

	//set brush opacity for this fill operation
    COLORREF col = static_cast<COLORREF>(color);
    ID2D1SolidColorBrush* brush = GetBrush(col);
    if (!brush) return;

    float oldOpacity = brush->GetOpacity();
    brush->SetOpacity(transparency / 255.0f);

	//create path geometry for the triangle
    ID2D1PathGeometry* pGeometry = nullptr;
    HRESULT hr = m_pD2DFactory->CreatePathGeometry(&pGeometry);
    if (SUCCEEDED(hr))
    {
        ID2D1GeometrySink* pSink = nullptr;
        hr = pGeometry->Open(&pSink);
        if (SUCCEEDED(hr))
        {
			//convert triangle points to D2D1_POINT_2F
            D2D1_POINT_2F points[3] = {
                { (float)tri.point[0].x, (float)tri.point[0].y },
                { (float)tri.point[1].x, (float)tri.point[1].y },
                { (float)tri.point[2].x, (float)tri.point[2].y }
            };

            pSink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);
            pSink->AddLine(points[1]);
            pSink->AddLine(points[2]);
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);

            pSink->Close();
        }
        pSink->Release();

        //fill
        m_pRenderTarget->FillGeometry(pGeometry, brush);
        pGeometry->Release();
    }

    brush->SetOpacity(oldOpacity);
    
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

        Fill(tri, 128, RGB(R_Intensity, G_Intensity, B_Intensity));
        
        //4 draw wireframe
        DrawTriangle(tri, 128, RGB(50, 50, 50));
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