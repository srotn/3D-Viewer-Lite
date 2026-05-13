#include "Engine3D.h"


#define ture true


Engine3D engine;  // 全局引擎对象

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (!engine.Initialize(hwnd))
        {
            return -1;
        }
        engine.OnUserCreate();
        SetTimer(hwnd, 1, 5, NULL);  // 约200 FPS
        return 0;

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        engine.Resize(width, height);
        return 0;
    }

    case WM_TIMER:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        // 开始 Direct2D 绘制
        engine.BeginDraw();
        // 注意：OnUserUpdate 内部会调用 Fill、Drawline 等，
        // 需要确保这些函数能访问到有效的渲染目标
        engine.OnUserUpdate(0.005f);
        engine.EndDraw();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 窗口注册与创建（不变）
    const wchar_t CLASS_NAME[] = L"3DViewerWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"3D Viewer Lite", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
