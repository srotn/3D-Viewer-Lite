#pragma once
#include "ExternalDependence.h"

class Engine3D
{
public:
	Engine3D();
	~Engine3D();

	// 初始化 Direct2D 资源，传入窗口句柄
	bool Initialize(HWND hwnd);

	// 当窗口大小改变时调用
	void Resize(int width, int height);

	// 开始一帧绘制（现在绘制到离屏）
	void BeginDraw();

	// 结束一帧绘制并提交（把离屏内容绘制到窗口）
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

	void FovPlus();

	void FovMinus();

	std::vector<std::string> testobjects = { "Skull.obj", "Old Teapot.obj", "icosahedron.obj", "little fan.obj", "111.obj", "3d_Isometric_Cube_Designs.obj"};
	std::string name = testobjects[1];
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
		{-0.087156f, 0.0f,  0.996195f, 0.0f },
		{ 0.0f,      0.0f,  0.0f,      1.0f }
	} };

	std::vector<vector3D> verts; // 所有顶点
	std::vector<int> indices; //所有面索引 三个一组
	std::vector<vector3D> transformedvectors; //投影后顶点

	// ====== 新增：暴露给 ImGui 使用的 DX11 核心接口 ======
	ID3D11Device* pd3dDevice = nullptr;
	ID3D11DeviceContext* pd3dContext = nullptr;
	IDXGISwapChain* pSwapChain = nullptr;
	ID3D11RenderTargetView* pmainRenderTargetView = nullptr;

private:

	// Direct2D 核心对象
	ID2D1Factory* m_pD2DFactory = nullptr;
	ID2D1RenderTarget* m_pRenderTarget = nullptr;

	// 屏幕缓冲区
	std::vector<uint32_t> m_frameBuffer;

	// ====== 新增 Z-Buffer ======
	std::vector<float> m_depthBuffer;    // 存储每个像素的深度值 (Z)

	// 位图纹理
	ID2D1Bitmap* m_pBackBufferBitmap = nullptr;

	mesh3D meshInput;

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
