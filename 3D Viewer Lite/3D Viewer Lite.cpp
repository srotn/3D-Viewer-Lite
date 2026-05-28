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

void InitDX11Device(HWND hwnd) {
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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

    InitDX11Device(hwnd);

    ImGui::CreateContext();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(pd3dDevice, pd3dContext);

    MSG msg = {};
    while (true)
    {
        // 处理所有等待的消息（非阻塞）
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                // 返回最终退出代码
                return (int)msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 所有消息处理完毕后，立即渲染一帧（无上限帧率）
        engine.Render();

        // 2. ImGui 渲染 (在引擎渲染后，覆盖在最上方)
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --- 在这里添加你的 UI 代码 ---
        ImGui::Begin("Engine Debug");
        // 1. 基础性能监控
        ImGui::Text("Application average %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();
        // ----------------------------

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    }
    return msg.wParam;
}
