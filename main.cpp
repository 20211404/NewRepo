#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

class CPPGameTimer {
public:
    CPPGameTimer() {
        prevTime = std::chrono::steady_clock::now();
    }

    float Update() {
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> frameTime = currentTime - prevTime;
        deltaTime = frameTime.count();
        prevTime = currentTime;
        return deltaTime;
    }

    float GetDeltaTime() const { return deltaTime; }

private:
    std::chrono::steady_clock::time_point prevTime;
    float deltaTime = 0.0f;
};

typedef struct _Vertex {
    float x, y, z;
    float r, g, b, a;
} Vertex;

typedef struct _GameContext {
    float playerX;
    float playerY;
    bool isRunning;
    bool keyStates[256];
} GameContext;

GameContext g_Game = { 0.0f, 0.0f, true, {false} };

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;

const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };
PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}
float4 PS(PS_INPUT input) : SV_Target { return input.col; }
)";

Vertex starVertices[] = {
    {  0.0f,       0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f },
    {  0.433013f, -0.25f, 0.5f,  1.0f, 1.0f, 1.0f, 1.0f },
    { -0.433013f, -0.25f, 0.5f,  1.0f, 1.0f, 1.0f, 1.0f },
    {  0.0f,      -0.5f,  0.5f,  0.0f, 0.0f, 0.0f, 1.0f },
    { -0.433013f,  0.25f, 0.5f,  0.0f, 0.0f, 0.0f, 1.0f },
    {  0.433013f,  0.25f, 0.5f,  0.0f, 0.0f, 0.0f, 1.0f }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_KEYDOWN:
        g_Game.keyStates[wParam] = true;
        if (wParam >= 'A' && wParam <= 'Z') g_Game.keyStates[wParam + 32] = true;
        break;
    case WM_KEYUP:
        g_Game.keyStates[wParam] = false;
        if (wParam >= 'A' && wParam <= 'Z') g_Game.keyStates[wParam + 32] = false;
        break;
    case WM_DESTROY:
        g_Game.isRunning = false;
        PostQuitMessage(0);
        break;
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void Update(float deltaTime) {
    float speed = 1.0f;
    float distance = speed * deltaTime;

    if (g_Game.keyStates['a']) g_Game.playerX -= distance;
    if (g_Game.keyStates['d']) g_Game.playerX += distance;
    if (g_Game.keyStates['w']) g_Game.playerY += distance;
    if (g_Game.keyStates['s']) g_Game.playerY -= distance;

    if (g_Game.playerX < -1.0f) g_Game.playerX = -1.0f;
    if (g_Game.playerX > 1.0f) g_Game.playerX = 1.0f;
    if (g_Game.playerY < -1.0f) g_Game.playerY = -1.0f;
    if (g_Game.playerY > 1.0f) g_Game.playerY = 1.0f;
}

void Render(float displayFPS, float deltaTime) {

    system("cls");
    printf("===     프레임레이트 표시중..     ===\n");
    printf("FPS : %.2f \n", displayFPS); 
    printf("DT  : %.4f sec\n", deltaTime);
    printf("Pos : (%.2f, %.2f)\n", g_Game.playerX, g_Game.playerY);
    printf("=====================================\n");

    float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    Vertex currentVertices[6];
    for (int i = 0; i < 6; i++) {
        currentVertices[i] = starVertices[i];
        currentVertices[i].x += g_Game.playerX;
        currentVertices[i].y += g_Game.playerY;
    }

    g_pImmediateContext->UpdateSubresource(g_pVertexBuffer, 0, nullptr, currentVertices, 0, 0);

    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

    g_pImmediateContext->Draw(6, 0);
    g_pSwapChain->Present(0, 0);
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"Star600x600";
    RegisterClassExW(&wcex);

    RECT wr = { 0, 0, 600, 600 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"Star600x600", L"Moving Star (600x600)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 600;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);

    D3D11_BUFFER_DESC bd = { sizeof(starVertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { starVertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

    D3D11_VIEWPORT vp = { 0, 0, 600.0f, 600.0f, 0.0f, 1.0f };
    g_pImmediateContext->RSSetViewports(1, &vp);
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    CPPGameTimer timer;
    const float TARGET_FRAME_TIME = 1.0f / 45.0f;
    float timeSinceLastFrame = 0.0f;

    float fpsTimeAccumulator = 0.0f;
    int frameCount = 0;
    float displayFPS = 0.0f;

    MSG msg = { 0 };
    while (g_Game.isRunning) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_Game.isRunning = false;
        }
        else {
            float dt = timer.Update();
            timeSinceLastFrame += dt;

            if (timeSinceLastFrame >= TARGET_FRAME_TIME) {
                Update(timeSinceLastFrame);

                frameCount++;                      
                fpsTimeAccumulator += timeSinceLastFrame; 

                if (fpsTimeAccumulator >= 1.0f) {
                    displayFPS = (float)frameCount / fpsTimeAccumulator;
                    frameCount = 0;               
                    fpsTimeAccumulator -= 1.0f;   
                }

                Render(displayFPS, timeSinceLastFrame);

                timeSinceLastFrame = 0.0f;
            }
            else {
                Sleep(1);
            }
        }
    }
    return (int)msg.wParam;
}
//dd