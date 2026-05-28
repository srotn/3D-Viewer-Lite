#include "Engine3D.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
Engine3D engine;  // 全局引擎对象

int mouse_x;
int mouse_y;

ID3D11Device* pd3dDevice = NULL;
ID3D11DeviceContext* pd3dContext = NULL;
IDXGISwapChain* pSwapChain = NULL;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // 【关键钩子】：优先处理 ImGui 消息
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;
    
    switch (uMsg)
    {
    case WM_CREATE:
        if (!engine.Initialize(hwnd))
        {
            return -1;
        }
        engine.OnUserCreate();
        return 0;

    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        engine.Resize(width, height);
        return 0;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_MOUSEMOVE:
    {
        bool rightButtonDown = (wParam & MK_RBUTTON) != 0;

        if (rightButtonDown)
        {
            engine.UpdateYawAndPitch(LOWORD(lParam) - mouse_x, HIWORD(lParam) - mouse_y);
        }

        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);

        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

        if (zDelta > 0) {
            engine.zoom *= 1.1;
        }
        else {
            engine.zoom /= 1.1;
        }

        if (engine.zoom < 0.1) engine.zoom = 0.1;
        return 0;
    }
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_PRIOR:
            engine.FovPlus();
            break;

        case VK_NEXT:
            engine.FovMinus();
            break;

        case 'L':
            engine.IsWireFramePaint ^= true;
            break;

        case 'F':
            engine.IsFillAndLight ^= true;
            break;

        case VK_UP:
            engine.Rlight = engine.MtimesV(engine.Rx_positive, engine.Rlight);
            engine.Glight = engine.MtimesV(engine.Rx_positive, engine.Glight);
            engine.Blight = engine.MtimesV(engine.Rx_positive, engine.Blight);
            break;

        case VK_DOWN:
            engine.Rlight = engine.MtimesV(engine.Rx_negative, engine.Rlight);
            engine.Glight = engine.MtimesV(engine.Rx_negative, engine.Glight);
            engine.Blight = engine.MtimesV(engine.Rx_negative, engine.Blight);
            break;

        case VK_RIGHT:
            engine.Rlight = engine.MtimesV(engine.Ry_positive, engine.Rlight);
            engine.Glight = engine.MtimesV(engine.Ry_positive, engine.Glight);
            engine.Blight = engine.MtimesV(engine.Ry_positive, engine.Blight);
            break;

        case VK_LEFT:
            engine.Rlight = engine.MtimesV(engine.Ry_negative, engine.Rlight);
            engine.Glight = engine.MtimesV(engine.Ry_negative, engine.Glight);
            engine.Blight = engine.MtimesV(engine.Ry_negative, engine.Blight);
            break;
        case '1':
            engine.name = engine.testobjects[0];
            engine.OnUserCreate();
            break;
        case '2':
            engine.name = engine.testobjects[1];
            engine.OnUserCreate();
            break;
        case '3':
            engine.name = engine.testobjects[2];
            engine.OnUserCreate();
            break;
        case '4':

            break;
        case '5':

            break;
        case '6':

            break;
        case '7':

            break;
        case '8':

            break;
        }
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void InitDX11Device(HWND hwnd) 
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
        D3D11_SDK_VERSION, &sd, &pSwapChain, &pd3dDevice, NULL, &pd3dContext);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
    // 窗口注册与创建保持不变
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

    // 【此时 engine 在 WM_CREATE 中已经通过 Initialize 自动把 DX11 设备创建好了】

    // 初始化 ImGui 上下文
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 直接使用 engine 内部创建好的 DX11 设备和上下文初始化 ImGui 后端
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(engine.pd3dDevice, engine.pd3dContext);

    MSG msg = {};
    while (true)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 1. 渲染 3D 场景（数据此时被 D2D 推送到了 DX11 后缓冲区中）
        engine.Render();

        // 2. 激活 DX11 渲染目标视图，让 ImGui 明确绘制的目标画布
        engine.pd3dContext->OMSetRenderTargets(1, &engine.pmainRenderTargetView, NULL);

        // 3. ImGui 驱动新帧
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --- 你的 UI 面板代码 ---
        ImGui::Begin("Engine Debug");
        ImGui::Text("Application average %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
        // ----------------------

        // 4. 将 UI 画面同样绘制到 DX11 后缓冲区中（实现完美叠加）
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // 5. 【终极一枪】：由 DX11 交换链统一垂直同步刷新到屏幕上！
        engine.pSwapChain->Present(1, 0);
    }
    return msg.wParam;
}
