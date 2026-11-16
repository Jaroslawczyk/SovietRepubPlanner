// dinput8.dll proxy: loads with the game, hooks IDXGISwapChain::Present and draws the ImGui panel.
#include "common.h"
#include <d3d11.h>
#include <cstdarg>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------- helpers ----------
std::wstring GameDir() {
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s = buf;
    return s.substr(0, s.find_last_of(L"\\/") + 1);
}
std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
static bool g_logStarted = false;
void Log(const char* fmt, ...) {
    FILE* f = _wfopen((GameDir() + L"DistrictPlanner.log").c_str(), g_logStarted ? L"a" : L"w");
    g_logStarted = true;
    if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

// ---------- dinput8 proxy ----------
typedef HRESULT(WINAPI* DI8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    static DI8Create_t real = nullptr;
    if (!real) {
        wchar_t path[MAX_PATH]; GetSystemDirectoryW(path, MAX_PATH);
        HMODULE h = LoadLibraryW((std::wstring(path) + L"\\dinput8.dll").c_str());
        if (h) real = (DI8Create_t)GetProcAddress(h, "DirectInput8Create");
    }
    return real ? real(inst, ver, riid, out, outer) : E_FAIL;
}

// ---------- overlay ----------
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDXGISwapChain*, UINT, UINT);
static Present_t o_Present = nullptr;
static bool g_init = false, g_failed = false;
static IDXGISwapChain* g_sc = nullptr;
static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static HWND g_hwnd = nullptr;
static std::string g_iniPath;

// Input reaches ImGui through a WH_GETMESSAGE hook instead of subclassing the window: the game re-installs
// its own window procedure during start-up, and a subclass set before that ends up in an endless call loop.
static HHOOK g_msgHook = nullptr;
static LRESULT CALLBACK GetMsgProc(int code, WPARAM wp, LPARAM lp) {
    MSG* m = (MSG*)lp;
    if (code == HC_ACTION && wp == PM_REMOVE && m && m->hwnd == g_hwnd && ImGui::GetCurrentContext()) {
        UINT msg = m->message;
        ImGui_ImplWin32_WndProcHandler(m->hwnd, msg, m->wParam, m->lParam);
        ImGuiIO& io = ImGui::GetIO();
        bool mouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) && msg != WM_MOUSEMOVE;
        bool buttonMsg = mouseMsg && msg != WM_MOUSEWHEEL && msg != WM_MOUSEHWHEEL;
        bool keyMsg = msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR;
        if ((mouseMsg && io.WantCaptureMouse) || (buttonMsg && ui::WantsGameMouse()) || (keyMsg && io.WantTextInput))
            m->message = WM_NULL;      // the game does not see this message
    }
    return CallNextHookEx(g_msgHook, code, wp, lp);
}

static bool InitOverlay(IDXGISwapChain* sc) {
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_dev))) return false;
    g_dev->GetImmediateContext(&g_ctx);
    DXGI_SWAP_CHAIN_DESC d{}; sc->GetDesc(&d);
    g_hwnd = d.OutputWindow; g_sc = sc;
    Log("overlay: swapchain %p, window %p, %ux%u", sc, g_hwnd, d.BufferDesc.Width, d.BufferDesc.Height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    g_iniPath = WideToUtf8(GameDir() + L"DistrictPlanner.ini");
    io.IniFilename = g_iniPath.c_str();
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 6; st.FrameRounding = 4; st.Colors[ImGuiCol_WindowBg].w = 0.94f;
    wchar_t win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
    std::string font = WideToUtf8(std::wstring(win) + L"\\Fonts\\segoeui.ttf");
    if (GetFileAttributesW((std::wstring(win) + L"\\Fonts\\segoeui.ttf").c_str()) == INVALID_FILE_ATTRIBUTES ||
        !io.Fonts->AddFontFromFileTTF(font.c_str(), 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic()))
        io.Fonts->AddFontDefault();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    g_msgHook = SetWindowsHookExW(WH_GETMESSAGE, GetMsgProc, nullptr, GetWindowThreadProcessId(g_hwnd, nullptr));
    Log("message hook %p", g_msgHook);
    return true;
}

static void Frame(IDXGISwapChain* sc) {
    if (g_failed) return;
    // The game also presents from helper threads with a tiny stack (loading screen); ImGui must not run there.
    ULONG_PTR lo = 0, hi = 0;
    GetCurrentThreadStackLimits(&lo, &hi);
    static DWORD lastTid = 0; static int logged = 0;
    DWORD tid = GetCurrentThreadId();
    size_t freeStack = (size_t)((ULONG_PTR)&lo - lo);
    if ((tid != lastTid || freeStack < 96 * 1024) && logged < 12) {
        ++logged;
        Log("present on thread %lu, stack %u KB, free %u KB", tid, (unsigned)((hi - lo) / 1024), (unsigned)(freeStack / 1024));
    }
    lastTid = tid;
    if (freeStack < 96 * 1024) return;
    static bool inFrame = false;
    if (inFrame) { if (logged < 12) { ++logged; Log("re-entrant Present, skipped"); } return; }
    struct Guard { Guard() { inFrame = true; } ~Guard() { inFrame = false; } } guard;
    if (!g_init) {
        if (!InitOverlay(sc)) { g_failed = true; Log("overlay: init failed"); return; }
        g_init = true;
    }
    if (sc != g_sc) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    eng::BeginFrame();
    ui::Draw();
    ImGuiIO& io = ImGui::GetIO();
    eng::g_blockMouseButtons = io.WantCaptureMouse || ui::WantsGameMouse();
    eng::g_blockMouseMove = io.WantCaptureMouse;
    eng::g_blockKeys = io.WantTextInput;
    ImGui::Render();

    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb))) return;
    ID3D11RenderTargetView* rtv = nullptr;
    g_dev->CreateRenderTargetView(bb, nullptr, &rtv);
    bb->Release();
    if (!rtv) return;
    ID3D11RenderTargetView* oldRtv = nullptr; ID3D11DepthStencilView* oldDsv = nullptr;
    g_ctx->OMGetRenderTargets(1, &oldRtv, &oldDsv);
    g_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_ctx->OMSetRenderTargets(1, &oldRtv, oldDsv);
    if (oldRtv) oldRtv->Release();
    if (oldDsv) oldDsv->Release();
    rtv->Release();
}

static HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST)) Frame(sc);
    return o_Present(sc, sync, flags);
}

static DWORD WINAPI InitThread(LPVOID) {
    catalog::Load();
    // Hooking Present before the game (and the Steam overlay) has set up its own swap chain makes the game
    // crash at start-up, so wait until the engine is running and then give it a few more seconds.
    for (int i = 0; i < 600 && !eng::CameraSeen(); ++i) Sleep(100);
    Sleep(5000);
    // A throwaway swap chain gives us the IDXGISwapChain vtable shared with the game's swap chain.
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"DistrictPlannerDummy";
    RegisterClassExW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 16, 16, nullptr, nullptr, wc.hInstance, nullptr);
    for (int attempt = 0; attempt < 20; ++attempt) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = w;
        sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
        IDXGISwapChain* sc = nullptr; ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                   D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx);
        if (SUCCEEDED(hr) && sc) {
            void** vt = *(void***)sc;
            DWORD old;
            if (VirtualProtect(&vt[8], sizeof(void*), PAGE_READWRITE, &old)) {
                o_Present = (Present_t)vt[8];
                vt[8] = (void*)HookPresent;
                VirtualProtect(&vt[8], sizeof(void*), old, &old);
                Log("Present hooked (original %p)", o_Present);
            }
            sc->Release(); ctx->Release(); dev->Release();
            break;
        }
        Log("dummy swap chain failed: 0x%08X", (unsigned)hr);
        Sleep(500);
    }
    DestroyWindow(w);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring name = exe; name = name.substr(name.find_last_of(L"\\/") + 1);
        if (_wcsicmp(name.c_str(), L"SOVIET64.exe") != 0) return TRUE;   // act as a plain proxy elsewhere
        Log("DistrictPlanner loaded into %ls", name.c_str());
        eng::InstallIatHooks();
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
