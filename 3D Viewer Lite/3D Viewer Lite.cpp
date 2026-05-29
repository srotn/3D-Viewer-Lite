#include "Engine3D.h"
#include <commdlg.h>

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
            engine.zoom *= 1.05;
        }
        else {
            engine.zoom *= 0.95;
        }

        if (engine.zoom < 0.0001) engine.zoom = 0000.1;
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

    // ==================== 字体设置（放在 ImGui 初始化后，主循环前） ====================
    // 加载大号字体（使用 Windows 自带的 Consolas，找不到则用默认字体放大）
    ImFont* bigFont = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\consola.ttf", 22.0f);
    if (!bigFont) {
        // 回退：使用默认字体并放大（在窗口中使用 PushFont 不方便，这里直接创建大号默认字体）
        ImFontConfig cfg;
        cfg.SizePixels = 22.0f;
        bigFont = io.Fonts->AddFontDefault(&cfg);
    }
    io.Fonts->Build();

    // ==================== 设置黑灰风格（只需在初始化时运行一次） ====================
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.40f, 0.45f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.85f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);

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

        

        

        // ==================== 主循环中的 UI ====================
        // 1. 右上角帧率窗口
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 340, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320, 80), ImGuiCond_Always);
        ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("FPS:");
        ImGui::SameLine();
        ImGui::PushFont(bigFont);  // 使用大号字体
        ImGui::Text("%.1f", ImGui::GetIO().Framerate);
        ImGui::PopFont();
        ImGui::Text("ms: %.2f", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::End();
        
        // 2. 模型信息窗口（右侧中部）
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 340, 110), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320, 120), ImGuiCond_Always);
        ImGui::Begin("Model Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Vertices:");
        ImGui::SameLine();
        ImGui::PushFont(bigFont);
        ImGui::Text("%d", engine.verts.size());
        ImGui::PopFont();
        ImGui::Text("Triangles:");
        ImGui::SameLine();
        ImGui::PushFont(bigFont);
        ImGui::Text("%d", engine.meshInput.tris.size());
        ImGui::PopFont();
        ImGui::End();

        // 3. 左侧控制面板
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_Once);
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_None);

        // FOV 滑动条
        ImGui::SliderFloat("FOV", &engine.fov, 1.0f, 150.0f);

        // 显示开关
        ImGui::Checkbox("Fill && Light", &engine.IsFillAndLight);
        ImGui::SameLine();
        ImGui::Checkbox("Wireframe", &engine.IsWireFramePaint);

        // 加载模型按钮
        if (ImGui::Button("Load Model..."))
        {
            OPENFILENAMEW ofn = {};
            wchar_t szFile[260] = L"";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;   // 需要一个全局窗口句柄
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
            ofn.lpstrFilter = L"3D Models\0*.obj;*.ply;*.stl\0OBJ\0*.obj\0PLY\0*.ply\0STL\0*.stl\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameW(&ofn))
            {
                std::wstring ws(szFile);
                std::string path(ws.begin(), ws.end());
                engine.name = path;
                engine.OnUserCreate();
            }
        }
        ImGui::Text("Model: %s", engine.name.c_str());

        ImGui::Separator();

        // 光照调节（折叠面板）
        if (ImGui::TreeNode("Lighting"))
        {
            ImGui::Text("Red Light");
            ImGui::SliderFloat("Dir X##R", &engine.Rlight.x, -1.0f, 1.0f);
            ImGui::SliderFloat("Dir Y##R", &engine.Rlight.y, -1.0f, 1.0f);
            ImGui::SliderFloat("Dir Z##R", &engine.Rlight.z, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color##R", (float*)&engine.lightColorR);

            ImGui::Text("Green Light");
            ImGui::SliderFloat("Dir X##G", &engine.Glight.x, -1.0f, 1.0f);
            ImGui::SliderFloat("Dir Y##G", &engine.Glight.y, -1.0f, 1.0f);
            ImGui::SliderFloat("Dir Z##G", &engine.Glight.z, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color##G", (float*)&engine.lightColorG);

            ImGui::Text("Blue Light");
            ImGui::SliderFloat("Dir X##B", &engine.Blight.x, -1.0f, 1.0f);
            ImGui::SliderFloat("Dir Y##B", &engine.Blight.y, -1.0f, 1.0f);
            ImGui::SliderFloat("Dir Z##B", &engine.Blight.z, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color##B", (float*)&engine.lightColorB);

            ImGui::TreePop();
        }
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
