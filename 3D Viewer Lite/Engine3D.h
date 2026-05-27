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

	void Fill(triangle3D tri, short transparency, int color);

	void Fill(int lux, int luy, int rdx, int rdy, short transparency, COLORREF color);

	void Fill(short transparency, int color);

	void Drawline(int x1, int y1, int x2, int y2, int width, COLORREF color);

	void DrawTriangle(triangle3D tri, short transparency, int color);

	void DrawMesh3D(mesh3D Centered, float fElapsedTime);

	mesh3D MoveToCenter(mesh3D mesh);

	void Render();   // 新增：直接执行一帧渲染

	void UpdateYawAndPitch(int x, int y);

	void CreateRotationMatrix(double yaw, double pitch);

	vector3D MtimesV(matrix m, vector3D v);
	
	char name[256] = "icosahedron.obj";
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

	// 笔刷缓存（可选，避免频繁创建）
	std::unordered_map<COLORREF, ID2D1SolidColorBrush*> m_brushCache;

	// 辅助函数：获取或创建对应颜色的笔刷（在当前活动渲染目标上）
	ID2D1SolidColorBrush* GetBrush(COLORREF color);

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
