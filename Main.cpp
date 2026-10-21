#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <thread>
#include <atomic>
#include <string>
#include <map>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

#pragma comment(lib, "d3d9.lib")
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

std::atomic<bool> g_Running(true);
std::atomic<bool> g_AutoClickEnabled(false);

enum ClickType { CLICK_LEFT = 0, CLICK_RIGHT = 1, CLICK_MIDDLE = 2, CLICK_CUSTOM = 3 };
int g_ClickType = CLICK_LEFT;
int g_ClickDelay = 100;

int g_ToggleKey = VK_F6;
bool g_HoldMode = false;
bool g_SettingToggleKey = false;

int g_CustomClickKey = 'B';
bool g_SettingCustomKey = false;

DWORD g_KeybindLockTime = 0;
std::atomic<bool> g_ThreadRunning(true);
std::map<int, std::string> g_keyNameMap;

void InitKeyNameMap() {
    if (!g_keyNameMap.empty()) return;
    g_keyNameMap[VK_LBUTTON] = "Mouse Left";
    g_keyNameMap[VK_RBUTTON] = "Mouse Right";
    g_keyNameMap[VK_MBUTTON] = "Mouse Middle";
    g_keyNameMap[VK_XBUTTON1] = "Mouse 4";
    g_keyNameMap[VK_XBUTTON2] = "Mouse 5";

    g_keyNameMap[VK_SPACE] = "Space"; g_keyNameMap[VK_SHIFT] = "Shift";
    g_keyNameMap[VK_CONTROL] = "Ctrl"; g_keyNameMap[VK_MENU] = "Alt";
    g_keyNameMap[VK_TAB] = "Tab"; g_keyNameMap[VK_ESCAPE] = "Esc";
    g_keyNameMap[VK_RETURN] = "Enter"; g_keyNameMap[VK_BACK] = "Backspace";
    g_keyNameMap[VK_CAPITAL] = "CapsLock"; g_keyNameMap[VK_INSERT] = "Ins";
    g_keyNameMap[VK_DELETE] = "Del"; g_keyNameMap[VK_HOME] = "Home";
    g_keyNameMap[VK_END] = "End"; g_keyNameMap[VK_PRIOR] = "PageUp";
    g_keyNameMap[VK_NEXT] = "PageDown"; g_keyNameMap[VK_LEFT] = "Left";
    g_keyNameMap[VK_RIGHT] = "Right"; g_keyNameMap[VK_UP] = "Up";
    g_keyNameMap[VK_DOWN] = "Down";

    for (int i = 1; i <= 24; ++i) g_keyNameMap[VK_F1 + i - 1] = "F" + std::to_string(i);
    for (int c = '0'; c <= '9'; ++c) g_keyNameMap[c] = std::string(1, (char)c);
    for (int c = 'A'; c <= 'Z'; ++c) g_keyNameMap[c] = std::string(1, (char)c);
}

std::string GetKeyName(int vk) {
    InitKeyNameMap();
    auto it = g_keyNameMap.find(vk);
    if (it != g_keyNameMap.end()) return it->second;

    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    LONG lParam = (sc << 16);
    char buf[64] = { 0 };
    if (GetKeyNameTextA(lParam, buf, sizeof(buf)) > 0) return std::string(buf);

    char tmp[16]; sprintf_s(tmp, "0x%02X", vk & 0xFF); return std::string(tmp);
}


void SendMouseClick(int clickType) {
    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE; in[1].type = INPUT_MOUSE;
    if (clickType == CLICK_LEFT) { in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP; }
    else if (clickType == CLICK_RIGHT) { in[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; in[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP; }
    else if (clickType == CLICK_MIDDLE) { in[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; in[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP; }
    else return;
    SendInput(2, in, sizeof(INPUT));
}

void SendKeyboardTap(WORD vk) {
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}


void ClickThreadFunc() {
    while (g_ThreadRunning) {
        if (g_AutoClickEnabled) {
            if (g_ClickType == CLICK_CUSTOM) SendKeyboardTap((WORD)g_CustomClickKey);
            else SendMouseClick(g_ClickType);
            int slept = 0; while (slept < g_ClickDelay && g_ThreadRunning) { Sleep(1); ++slept; }
        }
        else Sleep(5);
    }
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { g_Running = false; PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    InitKeyNameMap();

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, hInstance, NULL, NULL, NULL, NULL, L"AutoClickerClass", NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, L"Auto Clicker", WS_POPUP, 200, 200, 500, 320, NULL, NULL, hInstance, NULL);

    LPDIRECT3D9 d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return 1;

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE; d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD; d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    LPDIRECT3DDEVICE9 d3ddev = nullptr;
    if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &d3ddev))) {
        d3d->Release(); return 1;
    }

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd); ImGui_ImplDX9_Init(d3ddev);

    ShowWindow(hwnd, SW_SHOWDEFAULT); UpdateWindow(hwnd);

    std::thread clickThread(ClickThreadFunc);

    MSG msg; ZeroMemory(&msg, sizeof(msg));
    bool dragging = false; POINT dragStart{};
    bool prevToggleKeyState = false;

    while (g_Running) {
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_Running = false;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }


        if (g_SettingToggleKey) {
            for (int k = 1; k < 256; ++k) {
                if (GetAsyncKeyState(k) & 0x8000) {
                    g_ToggleKey = k;
                    g_SettingToggleKey = false;
                    g_KeybindLockTime = GetTickCount() + 1000;
                    break;
                }
            }
        }
        if (g_SettingCustomKey) {
            for (int k = 1; k < 256; ++k) {
                if (GetAsyncKeyState(k) & 0x8000) {
                    g_CustomClickKey = k;
                    g_SettingCustomKey = false;
                    g_KeybindLockTime = GetTickCount() + 1000;
                    break;
                }
            }
        }


        if (GetTickCount() >= g_KeybindLockTime) {
            if (g_HoldMode)
                g_AutoClickEnabled = (GetAsyncKeyState(g_ToggleKey) & 0x8000) != 0;
            else {
                bool nowPressed = (GetAsyncKeyState(g_ToggleKey) & 0x8000) != 0;
                if (nowPressed && !prevToggleKeyState) g_AutoClickEnabled = !g_AutoClickEnabled;
                prevToggleKeyState = nowPressed;
            }
        }
        else g_AutoClickEnabled = false; // ระหว่าง lock

        // ImGui frame
        ImGui_ImplDX9_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(500, 320));
        ImGui::Begin("AutoClick UI", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        // Titlebar
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        ImGui::BeginChild("TitleBar", ImVec2(ImGui::GetWindowWidth(), 28), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPos(ImVec2(8, 6)); ImGui::Text("AutoClickX (Beta)");
        ImGui::SameLine(); ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 40);
        if (ImGui::Button("X", ImVec2(32, 22))) { g_Running = false; PostQuitMessage(0); }
        ImGui::EndChild(); ImGui::PopStyleColor();


        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            POINT p; GetCursorPos(&p); RECT r; GetWindowRect(hwnd, &r);
            if (p.y >= r.top && p.y <= r.top + 28) { dragging = true; dragStart.x = p.x - r.left; dragStart.y = p.y - r.top; }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) dragging = false;
        if (dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            POINT p; GetCursorPos(&p);
            SetWindowPos(hwnd, NULL, p.x - dragStart.x, p.y - dragStart.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }


        float sidebarW = 120.0f;
        ImGui::BeginChild("Sidebar", ImVec2(sidebarW, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("Categories"); ImGui::Separator();
        static int selectedCat = 0;
        if (ImGui::Selectable("Auto Clicker", selectedCat == 0)) selectedCat = 0;
        if (ImGui::Selectable("Credit", selectedCat == 1)) selectedCat = 1;
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Main", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_NoScrollbar);
        if (selectedCat == 0) {
            ImGui::Text("Auto Clicker Settings"); ImGui::Separator();
            ImGui::Text("Status: %s", g_AutoClickEnabled ? "ON" : "OFF");
            ImGui::Checkbox("Hold Mode", &g_HoldMode);
            const char* types[] = { "Left","Right","Middle","Custom (keyboard)" };
            ImGui::Combo("Click Type", &g_ClickType, types, IM_ARRAYSIZE(types));
            if (g_ClickType == CLICK_CUSTOM) {
                ImGui::Text("Custom Key"); ImGui::SameLine();
                ImGui::Text("[%s]", GetKeyName(g_CustomClickKey).c_str());
                if (ImGui::Button("Change Custom Key")) g_SettingCustomKey = true;
                if (g_SettingCustomKey) { ImGui::SameLine(); ImGui::Text("Press any key..."); }
            }
            ImGui::Separator();
            ImGui::Text("Toggle/Hold Key"); ImGui::SameLine();
            ImGui::Text("[%s]", GetKeyName(g_ToggleKey).c_str());
            if (ImGui::Button("Change Toggle Key")) g_SettingToggleKey = true;
            if (g_SettingToggleKey) { ImGui::SameLine(); ImGui::Text("Press any key..."); }
            ImGui::SliderInt("Click Delay (ms)", &g_ClickDelay, 1, 2000);
        }
        else ImGui::Text("Made by Synectt");
        ImGui::EndChild();

        ImGui::End();

 
        ImGui::Render();
        d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA(28, 28, 28, 255), 1.0f, 0);
        if (d3ddev->BeginScene() >= 0) { ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData()); d3ddev->EndScene(); }
        d3ddev->Present(NULL, NULL, NULL, NULL);
    }

    g_ThreadRunning = false; if (clickThread.joinable()) clickThread.join();
    ImGui_ImplDX9_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (d3ddev) d3ddev->Release(); if (d3d) d3d->Release();
    DestroyWindow(hwnd); UnregisterClass(wc.lpszClassName, hInstance);
    return 0;
}
