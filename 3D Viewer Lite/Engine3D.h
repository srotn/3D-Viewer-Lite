#pragma once
#include "Structs.h"
#include "OutDependence.h"

class Engine3D
{
public:
	Engine3D();
	~Engine3D();

	void SetHDC(HDC hdc) { m_hdc = hdc; }

	// 初始化 Direct2D 资源，传入窗口句柄
	bool Initialize(HWND hwnd);

	// 当窗口大小改变时调用
	void Resize(int width, int height);

	// 开始一帧绘制
	void BeginDraw();

	// 结束一帧绘制并提交
	void EndDraw();

	void test() {
		Drawline(10 + testx, 10 + testy, 200 + testx, 10 + testy, abs((i / 4) % 64 - 32), RGB(i % 256, (i + 64) % 256, (i + 128) % 256)); // 红色水平线
		testx = (testx + 1) % (ScreenWidth() - 190);
		testy = (testy + 1) % (ScreenHeight() - 10);
		i = (i + 1) % 512;

		triangle3D tri = { 10, 10, 0, 0, 100, 0, 100, 0, 0 };
		DrawTriangle(tri, 0, 0xffffff);
	}

	int ScreenHeight();

	int ScreenWidth();

	void Fill(triangle3D tri, short transparency, int color);

	void Fill(int lux, int luy, int rdx, int rdy, short transparency, COLORREF color);

	void Fill(short transparency, int color);

	void Drawline(int x1, int y1, int x2, int y2, int width, COLORREF color);

	void DrawTriangle(triangle3D tri, short transparency, int color);

	void DrawMesh3D(mesh3D& Centered);

	mesh3D MoveToCenter(mesh3D mesh);
	
	double fov = 90;
	double distance;
	double unit;

	int testx = 0;
	int testy = 0;
	int i = 0;

private:

	// Direct2D 核心对象
	ID2D1Factory* m_pD2DFactory = nullptr;
	ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

	// 笔刷缓存（可选，避免频繁创建）
	std::unordered_map<COLORREF, ID2D1SolidColorBrush*> m_brushCache;

	// 辅助函数：获取或创建对应颜色的笔刷
	ID2D1SolidColorBrush* GetBrush(COLORREF color);

	mesh3D meshTetrahedron;
	mesh3D Centered1;
	mesh3D meshCube;
	mesh3D Centered2;

	mesh3D meshInput;
	mesh3D CenteredInput;

	HDC m_hdc;
	
	int m_width = 0, m_height = 0;
	HWND m_hwnd = nullptr;

public:
	bool OnUserCreate();

	bool OnUserUpdate(float fElapsedTime);
};
