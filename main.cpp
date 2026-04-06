#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <chrono>
#include <vector>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:windows")

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

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

class GameObject;

class Component {
public:
    GameObject* pOwner = nullptr;
    bool isStarted = false;

    virtual void Start() = 0;
    virtual void Input() {}
    virtual void Update(float dt) = 0;
    virtual void Render() {}

    virtual ~Component() {}
};

class GameObject {
public:
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    std::vector<Component*> components;

    GameObject(std::string n) {
        name = n;
    }

    ~GameObject() {
        for (int i = 0; i < (int)components.size(); i++) {
            delete components[i];
        }
    }

    void AddComponent(Component* pComp) {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

class ControllerComponent : public Component {
public:
    int keyUp, keyDown, keyLeft, keyRight;
    float speed;
    bool moveUp, moveDown, moveLeft, moveRight;

    ControllerComponent(int up, int down, int left, int right) {
        keyUp = up; keyDown = down; keyLeft = left; keyRight = right;
    }

    void Start() override {
        speed = 1.5f;
        moveUp = moveDown = moveLeft = moveRight = false;
    }

    void Input() override {
        moveUp = (GetAsyncKeyState(keyUp) & 0x8000);
        moveDown = (GetAsyncKeyState(keyDown) & 0x8000);
        moveLeft = (GetAsyncKeyState(keyLeft) & 0x8000);
        moveRight = (GetAsyncKeyState(keyRight) & 0x8000);
    }

    void Update(float dt) override {
        float velocityX = 0.0f;
        float velocityY = 0.0f;

        if (moveLeft)  velocityX -= speed;
        if (moveRight) velocityX += speed;
        if (moveUp)    velocityY += speed;
        if (moveDown)  velocityY -= speed;

        pOwner->x = pOwner->x + (velocityX * dt);
        pOwner->y = pOwner->y + (velocityY * dt);

        if (pOwner->x < -1.0f) pOwner->x = -1.0f;
        if (pOwner->x > 1.0f)  pOwner->x = 1.0f;
        if (pOwner->y < -1.0f) pOwner->y = -1.0f;
        if (pOwner->y > 1.0f)  pOwner->y = 1.0f;
    }
};

class RendererComponent : public Component {
public:
    Vertex localVertices[3];

    RendererComponent(Vertex v1, Vertex v2, Vertex v3) {
        localVertices[0] = v1;
        localVertices[1] = v2;
        localVertices[2] = v3;
    }

    void Start() override {}
    void Update(float dt) override {}

    void Render() override {
        Vertex worldVertices[3];
        for (int i = 0; i < 3; i++) {
            worldVertices[i] = localVertices[i];
            worldVertices[i].x += pOwner->x;
            worldVertices[i].y += pOwner->y;
        }

        g_pImmediateContext->UpdateSubresource(g_pVertexBuffer, 0, nullptr, worldVertices, 0, 0);
        g_pImmediateContext->Draw(3, 0);
    }
};

class GameLoop {
public:
    bool isRunning;
    std::vector<GameObject*> gameWorld;
    std::chrono::high_resolution_clock::time_point prevTime;
    float deltaTime;

    GameLoop() { Initialize(); }
    ~GameLoop() {
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            delete gameWorld[i];
        }
    }

    void Initialize() {
        isRunning = true;
        gameWorld.clear();
        prevTime = std::chrono::high_resolution_clock::now();
        deltaTime = 0.0f;
    }

    void Input() {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            isRunning = false;
            PostQuitMessage(0);
        }

        static bool fKeyPrev = false;
        bool fKeyCurr = (GetAsyncKeyState('F') & 0x8000);
        if (fKeyCurr && !fKeyPrev) {
            static bool isFullScreen = false;
            isFullScreen = !isFullScreen;
            if (g_pSwapChain) g_pSwapChain->SetFullscreenState(isFullScreen, nullptr);
        }
        fKeyPrev = fKeyCurr;

        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->Input();
            }
        }
    }

    void Update() {
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                if (!gameWorld[i]->components[j]->isStarted) {
                    gameWorld[i]->components[j]->Start();
                    gameWorld[i]->components[j]->isStarted = true;
                }
            }
        }

        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->Update(deltaTime);
            }
        }
    }

    void Render() {
        float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

        UINT stride = sizeof(Vertex), offset = 0;
        g_pImmediateContext->IASetInputLayout(g_pInputLayout);
        g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->Render();
            }
        }

        g_pSwapChain->Present(0, 0);
    }

    void Run() {
        MSG msg = { 0 };
        while (isRunning) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) isRunning = false;
            }
            else {
                auto currentTime = std::chrono::high_resolution_clock::now();
                std::chrono::duration<float> elapsed = currentTime - prevTime;
                deltaTime = elapsed.count();
                prevTime = currentTime;

                Input();
                Update();
                Render();
            }
        }
    }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"Lecture04Engine";
    RegisterClassExW(&wcex);

    RECT wr = { 0, 0, 800, 600 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"Lecture04Engine", L"삼각형 두개 돌리기", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = { 0 };
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
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

    D3D11_BUFFER_DESC bd = { sizeof(Vertex) * 3, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);

    D3D11_VIEWPORT vp = { 0, 0, 800.0f, 600.0f, 0.0f, 1.0f };
    g_pImmediateContext->RSSetViewports(1, &vp);

    GameLoop gLoop;

    GameObject* player1 = new GameObject("Player1");
    player1->x = 0.5f;
    player1->AddComponent(new ControllerComponent(VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT));
    player1->AddComponent(new RendererComponent(
        Vertex{ 0.0f,   0.385f, 0.5f,  1.0f, 1.0f, 1.0f, 1.0f }, 
        Vertex{ 0.25f, -0.192f, 0.5f,  1.0f, 1.0f, 1.0f, 1.0f }, 
        Vertex{ -0.25f, -0.192f, 0.5f,  1.0f, 1.0f, 1.0f, 1.0f }  
    ));
    gLoop.gameWorld.push_back(player1);

    GameObject* player2 = new GameObject("Player2");
    player2->x = -0.5f;
    player2->AddComponent(new ControllerComponent('W', 'S', 'A', 'D'));
    player2->AddComponent(new RendererComponent(
        Vertex{ 0.0f,  -0.385f, 0.5f,  0.0f, 0.0f, 0.0f, 1.0f }, 
        Vertex{ -0.25f,  0.192f, 0.5f,  0.0f, 0.0f, 0.0f, 1.0f }, 
        Vertex{ 0.25f,  0.192f, 0.5f,  0.0f, 0.0f, 0.0f, 1.0f } 
    ));
    gLoop.gameWorld.push_back(player2);

    gLoop.Run();

    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (vsBlob) vsBlob->Release();
    if (psBlob) psBlob->Release();

    return 0;
}