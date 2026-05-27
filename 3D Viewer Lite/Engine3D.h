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

	void Fill(triangle3D tri, short transparency, uint32_t color);

	void Fill(int lux, int luy, int rdx, int rdy, short transparency, uint32_t color);

	void Fill(short transparency, uint32_t color);

	void Drawline(int x1, int y1, int x2, int y2, short transparency, uint32_t color);

	void DrawTriangle(triangle3D tri, short transparency, uint32_t color);

	void DrawMesh3D(const mesh3D& Projected, float fElapsedTime);

	inline vector3D MtimesV(const matrix& m, const vector3D& v);

	mesh3D MoveToCenter(mesh3D mesh);

	void Render();

	void UpdateYawAndPitch(int x, int y);

	void CreateRotationMatrix(double yaw, double pitch);

	mesh3D LoadFromObjectFile(std::string filename);

	
	char name[256] = "Old Teapot.obj";
	//char name[256] = "Axis.obj";
	double fov = 60;
	double zoom = 1;
	vector3D Rlight = { -1, 1, 1 };
	vector3D Glight = { -1, 1, 1 };
	vector3D Blight = { -1, 1, 1 };
	double distance;
	double unit;
	double yaw;
	double pitch;

	matrix RotationYaw;
	matrix RotationPitch;
	matrix Projection;

private:

	// Direct2D 核心对象
	ID2D1Factory* m_pD2DFactory = nullptr;
	ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

	// 屏幕缓冲区
	std::vector<uint32_t> m_frameBuffer;

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
