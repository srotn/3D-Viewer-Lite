#pragma once
#include "ExternalDependence.h"

class Engine3D
{
public:
    Engine3D();
    ~Engine3D();

    // Initialize Direct2D and DX11, pass window handle
    bool Initialize(HWND hwnd);

    // Called when window size changes
    void Resize(int width, int height);

    // Start a new frame (clear off-screen buffer)
    void BeginDraw();

    // Finish frame and present to screen
    void EndDraw();

    int ScreenHeight();
    int ScreenWidth();

    void Fill(triangle3D tri, short transparency, uint32_t color, int minClipY = 0, int maxClipY = -1);
    void Fill(int lux, int luy, int rdx, int rdy, short transparency, uint32_t color);
    void Fill(short transparency, uint32_t color);

    void Drawline(int x1, int y1, int x2, int y2, short transparency, uint32_t color);
    void DrawTriangle(triangle3D tri, short transparency, uint32_t color);
    void DrawMesh3D(float fElapsedTime);

    inline vector3D MtimesV(const matrix& m, const vector3D& v);
    void MoveToCenter();
    void Render();
    void UpdateYawAndPitch(int x, int y);
    void CreateRotationMatrix(float yaw, float pitch);

    mesh3D LoadFromObjectFile(std::string filename);
    mesh3D LoadFromPlyFile(std::string filename);
    mesh3D LoadFromStlFile(std::string filename);

    void FovPlus();
    void FovMinus();

    std::string name = "Old Teapot.obj";
    float fov = 60;
    float zoom = 1;
    vector3D Rlight = { -1, 1, 1 };
    vector3D Glight = { -1, 1, 1 };
    vector3D Blight = { -1, 1, 1 };
    float distance;
    float unit;
    float yaw;
    float pitch;
    bool IsWireFramePaint = false;
    bool IsFillAndLight = true;

    matrix Rotation;
    matrix Projection;

    matrix Rx_positive = { {
        { 1.0f,  0.0f,      0.0f,      0.0f },
        { 0.0f,  0.996195f, 0.087156f, 0.0f },
        { 0.0f, -0.087156f, 0.996195f, 0.0f },
        { 0.0f,  0.0f,      0.0f,      1.0f }
    } };

    matrix Rx_negative = { {
        { 1.0f,  0.0f,      0.0f,      0.0f },
        { 0.0f,  0.996195f, -0.087156f,0.0f },
        { 0.0f,  0.087156f, 0.996195f, 0.0f },
        { 0.0f,  0.0f,      0.0f,      1.0f }
    } };

    matrix Ry_positive = { {
        { 0.996195f, 0.0f, -0.087156f, 0.0f },
        { 0.0f,      1.0f,  0.0f,      0.0f },
        { 0.087156f, 0.0f,  0.996195f, 0.0f },
        { 0.0f,      0.0f,  0.0f,      1.0f }
    } };

    matrix Ry_negative = { {
        { 0.996195f, 0.0f,  0.087156f, 0.0f },
        { 0.0f,      1.0f,  0.0f,      0.0f },
        { -0.087156f,0.0f,  0.996195f, 0.0f },
        { 0.0f,      0.0f,  0.0f,      1.0f }
    } };

    std::vector<vector3D> verts;
    std::vector<int> indices;
    std::vector<vector3D> transformedvectors;

    // DX11 resources exposed for ImGui
    ID3D11Device* pd3dDevice = nullptr;
    ID3D11DeviceContext* pd3dContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11RenderTargetView* pmainRenderTargetView = nullptr;

    vector3D lightColorR = { 1.0f, 1.0f, 1.0f };
    vector3D lightColorG = { 1.0f, 1.0f, 1.0f };
    vector3D lightColorB = { 1.0f, 1.0f, 1.0f };

    mesh3D meshInput;

private:
    // Direct2D objects
    ID2D1Factory* m_pD2DFactory = nullptr;
    ID2D1RenderTarget* m_pRenderTarget = nullptr;

    // CPU framebuffer and Z-buffer
    std::vector<uint32_t> m_frameBuffer;
    std::vector<float> m_depthBuffer;

    ID2D1Bitmap* m_pBackBufferBitmap = nullptr;

    int m_width = 0, m_height = 0;
    HWND m_hwnd = nullptr;

public:
    bool OnUserCreate();
    bool OnUserUpdate(float fElapsedTime);

private:
    LARGE_INTEGER m_lastTime;
    LARGE_INTEGER m_freq;
    bool m_firstFrame;
};

inline vector3D Engine3D::MtimesV(const matrix& m, const vector3D& v)
{
    vector3D result;
    result.x = m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3];
    result.y = m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3];
    result.z = m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3];
    return result;
}