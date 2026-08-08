// ============================================================================
// H89 DEV - FiveM Offset Dumper
// Dear ImGui + DirectX 11 - Dark/purple terminal vibe (original style)
// ============================================================================
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdarg>

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

#if __has_include("imgui/imgui.h")
    #include "imgui/imgui.h"
    #include "imgui/imgui_impl_win32.h"
    #include "imgui/imgui_impl_dx11.h"
#else
    #include "imgui.h"
    #include "imgui_impl_win32.h"
    #include "imgui_impl_dx11.h"
#endif

#include "scanner.h"
#include "patterns.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")

// ---- Brand ----
static const wchar_t* TITLE   = L"H89 DEV";
static const wchar_t* WCLASS  = L"H89DevDumperWnd";

// Accent - deep purple (matches original vibe)
static const ImVec4 ACCENT      = {0.55f, 0.20f, 0.90f, 1.00f};
static const ImVec4 ACCENT_DIM  = {0.35f, 0.12f, 0.60f, 1.00f};
static const ImVec4 ACCENT_HOV  = {0.65f, 0.25f, 1.00f, 1.00f};
static const ImVec4 GREEN       = {0.30f, 0.90f, 0.35f, 1.00f};
static const ImVec4 RED         = {0.95f, 0.30f, 0.30f, 1.00f};
static const ImVec4 TEXT        = {0.90f, 0.90f, 0.92f, 1.00f};
static const ImVec4 TEXT_DIM    = {0.60f, 0.60f, 0.65f, 1.00f};
static const ImVec4 BG          = {0.06f, 0.05f, 0.09f, 1.00f};
static const ImVec4 BG_PANEL    = {0.09f, 0.07f, 0.12f, 1.00f};
static const ImVec4 BG_BUTTON   = {0.18f, 0.15f, 0.22f, 1.00f};
static const ImVec4 BG_BUTTON_H = {0.25f, 0.18f, 0.35f, 1.00f};

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
static LRESULT WINAPI WndProc(HWND,UINT,WPARAM,LPARAM);

// ---- Globals ----
static ID3D11Device*           g_d3dDev = nullptr;
static ID3D11DeviceContext*    g_d3dCtx = nullptr;
static IDXGISwapChain*         g_swap   = nullptr;
static ID3D11RenderTargetView* g_rtv    = nullptr;

enum Tab { SCANNER=0, PROCINFO, SETTINGS };
static Tab g_tab = SCANNER;

enum Lvl { INFO, OK, FAIL, WARN };
struct Line { Lvl lvl; std::string s; };
static std::vector<Line> g_log;
static std::mutex        g_logMtx;

static std::atomic<bool> g_running{false};
static std::thread       g_thr;
static DWORD             g_pid = 0;
static std::wstring      g_pname;
static std::string       g_build = "-";
static bool              g_autoScroll = true;

// ---- Helpers ----
static void Log(Lvl l, const char* f, ...) {
    char b[2048]; va_list a; va_start(a,f); vsnprintf(b,sizeof(b),f,a); va_end(a);
    std::lock_guard<std::mutex> lk(g_logMtx);
    g_log.push_back({l, b});
}
static void ClearLog() { std::lock_guard<std::mutex> lk(g_logMtx); g_log.clear(); }

static void CopyTxt(const std::string& s){
    if(OpenClipboard(nullptr)){
        EmptyClipboard();
        HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,s.size()+1);
        if(h){memcpy(GlobalLock(h),s.c_str(),s.size()+1);GlobalUnlock(h);SetClipboardData(CF_TEXT,h);}
        CloseClipboard();
    }
}
static std::string Hex(uintptr_t v){char b[32];sprintf(b,"0x%08llX",(unsigned long long)v);return std::string(b);}

static std::string Narrow(const std::wstring& w){
    if(w.empty()) return{};
    int sz=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);
    std::string s(sz,0);
    if(sz>0) WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),s.data(),sz,nullptr,nullptr);
    return s;
}

// ---- D3D ----
static bool D3DInit(HWND h){
    DXGI_SWAP_CHAIN_DESC d={};
    d.BufferCount=2; d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    d.BufferDesc.RefreshRate.Numerator=60; d.BufferDesc.RefreshRate.Denominator=1;
    d.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; d.OutputWindow=h;
    d.SampleDesc.Count=1; d.Windowed=TRUE; d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl; D3D_FEATURE_LEVEL fa[2]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    if(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,fa,2,D3D11_SDK_VERSION,
        &d,&g_swap,&g_d3dDev,&fl,&g_d3dCtx)<0) return false;
    ID3D11Texture2D* bb=nullptr; g_swap->GetBuffer(0,IID_PPV_ARGS(&bb));
    if(bb){g_d3dDev->CreateRenderTargetView(bb,nullptr,&g_rtv);bb->Release();}
    return true;
}
static void KRT(){if(g_rtv){g_rtv->Release();g_rtv=nullptr;}}
static void CRT(){
    ID3D11Texture2D* bb=nullptr;
    g_swap->GetBuffer(0,IID_PPV_ARGS(&bb));
    if(bb){g_d3dDev->CreateRenderTargetView(bb,nullptr,&g_rtv);bb->Release();}
}

// ---- Theme ----
static void ApplyTheme(){
    ImGui::StyleColorsDark();
    auto& s=ImGui::GetStyle(); auto* c=s.Colors;
    c[ImGuiCol_Text]=TEXT; c[ImGuiCol_TextDisabled]=TEXT_DIM;
    c[ImGuiCol_WindowBg]=BG; c[ImGuiCol_ChildBg]=BG_PANEL; c[ImGuiCol_PopupBg]=BG_PANEL;
    c[ImGuiCol_Border]=ImVec4(0.30f,0.18f,0.50f,0.40f);
    c[ImGuiCol_FrameBg]=BG_BUTTON; c[ImGuiCol_FrameBgHovered]=BG_BUTTON_H;
    c[ImGuiCol_FrameBgActive]=ACCENT_DIM;
    c[ImGuiCol_TitleBg]=BG; c[ImGuiCol_TitleBgActive]=BG_PANEL;
    c[ImGuiCol_MenuBarBg]=BG;
    c[ImGuiCol_ScrollbarBg]=BG; c[ImGuiCol_ScrollbarGrab]=ACCENT_DIM;
    c[ImGuiCol_ScrollbarGrabHovered]=ACCENT_HOV; c[ImGuiCol_ScrollbarGrabActive]=ACCENT;
    c[ImGuiCol_Button]=BG_BUTTON; c[ImGuiCol_ButtonHovered]=BG_BUTTON_H; c[ImGuiCol_ButtonActive]=ACCENT_DIM;
    c[ImGuiCol_Header]=ACCENT_DIM; c[ImGuiCol_HeaderHovered]=ACCENT; c[ImGuiCol_HeaderActive]=ACCENT_HOV;
    c[ImGuiCol_Separator]=ImVec4(0.30f,0.18f,0.50f,0.50f);
    c[ImGuiCol_ResizeGrip]={0,0,0,0}; c[ImGuiCol_ResizeGripHovered]=ACCENT_DIM;
    c[ImGuiCol_CheckMark]=ACCENT;
    s.WindowRounding=4.f; s.ChildRounding=4.f; s.FrameRounding=4.f;
    s.ScrollbarRounding=3.f; s.GrabRounding=3.f;
    s.WindowBorderSize=1.f; s.FrameBorderSize=0.f;
    s.WindowPadding={10,10}; s.FramePadding={8,6};
    s.ItemSpacing={8,6}; s.ItemInnerSpacing={6,4}; s.ScrollbarSize=10.f;
}

// ---- Scan ----
static void DoScan(){
    Log(INFO,"Awaiting for FiveM...");
    Scanner::ProcessInfo pi{g_pid,g_pname}; std::string b=g_build;
    if(!Scanner::FindFiveM(pi,b)){Log(FAIL,"FiveM process not found! Start FiveM first.");g_running=false;return;}
    g_pid=pi.pid; g_pname=pi.name; g_build=b;
    Log(OK,"FiveM found! (Build %s)",g_build.c_str());
    Log(INFO,"Caching game module in memory for optimized scanning...");

    auto m=Scanner::CacheModule(pi,L"gtaprocess");
    if(!m.valid) m=Scanner::CacheModule(pi,L"fivem_b");
    if(!m.valid) m=Scanner::CacheModule(pi,L"fivem");
    if(!m.valid){Log(FAIL,"Failed to cache FiveM module. Try running as Administrator.");g_running=false;return;}

    auto pats=Patterns::GetDefaultPatterns();
    for(auto& p:pats){
        uintptr_t a=Scanner::FindPattern(m,p);
        if(a){Log(OK,"%s -> %s",p.name,Hex(a-m.base).c_str());}
        else  Log(FAIL,"%s -> [NOT FOUND]",p.name);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    // Dump header
    std::string out="offsets_"+g_build+".hpp";
    std::ofstream f(out);
    if(f.is_open()){
        f<<"#pragma once\n// H89 DEV - FiveM Offsets (build "<<g_build<<")\n\nnamespace Offsets {\n";
        for(auto& p:pats){
            uintptr_t a=Scanner::FindPattern(m,p);
            std::string k=p.name; std::replace(k.begin(),k.end(),' ','_');
            f<<"    constexpr uintptr_t "<<k<<" = 0x"<<std::hex<<(a?a-m.base:0)<<";\n";
        }
        f<<"}\n"; f.close();
        Log(OK,"Dumped offsets to %s",out.c_str());
    }
    g_running=false;
}

// ---- UI ----
static ImVec4 LvlCol(Lvl l){
    switch(l){case INFO:return ACCENT;case OK:return GREEN;case FAIL:return RED;case WARN:return ImVec4(1,0.85f,0.2f,1);}
    return {1,1,1,1};
}
static const char* LvlTag(Lvl l){
    switch(l){case INFO:return"[INFO]";case OK:return"[ OK ]";case FAIL:return"[FAIL]";case WARN:return"[WARN]";}
    return "";
}

static void Sidebar(){
    struct N{const char* t;Tab id;};
    N ns[]={{"Scanner",SCANNER},{"Process Info",PROCINFO},{"Settings",SETTINGS}};
    for(auto& n:ns){
        bool sel=(g_tab==n.id);
        if(sel){
            ImGui::PushStyleColor(ImGuiCol_Button,ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ACCENT_HOV);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ACCENT);
        }else{
            ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.18f,0.10f,0.28f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,ACCENT_DIM);
        }
        if(ImGui::Button(n.t,ImVec2(-1,42))) g_tab=n.id;
        ImGui::PopStyleColor(3);
    }
}

static void TabScanner(){
    ImGui::TextColored(TEXT,"Pattern Scanner");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        BG_BUTTON);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_BUTTON_H);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ACCENT_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,8.f);
    bool can=!g_running;
    if(ImGui::Button(g_running?"Scanning...":"Start Dump",ImVec2(-1,60)) && can){
        ClearLog(); g_running=true;
        if(g_thr.joinable())g_thr.join();
        g_thr=std::thread(DoScan);
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor(3);

    ImGui::Spacing();
    // Console header with Copy + Clear buttons aligned to the right
    float panelW = ImGui::GetContentRegionAvail().x;
    float btn1W = 80.0f, btn2W = 60.0f, gap = 6.0f;
    ImGui::TextColored(TEXT,"Console Log:");
    ImGui::SameLine(panelW - btn1W - btn2W - gap);
    if(ImGui::Button("Copy Logs", ImVec2(btn1W, 0))){
        std::string all;
        {
            std::lock_guard<std::mutex> lk(g_logMtx);
            for(auto& ln:g_log){
                all += LvlTag(ln.lvl);
                all += " ";
                all += ln.s;
                all += "\n";
            }
        }
        CopyTxt(all);
        Log(INFO,"Console log copied to clipboard.");
    }
    ImGui::SameLine();
    if(ImGui::Button("Clear", ImVec2(btn2W, 0))) ClearLog();
    ImGui::Separator();

    ImGui::BeginChild("##log",ImVec2(0,0),true,0);
    {
        std::lock_guard<std::mutex> lk(g_logMtx);
        for(auto& ln:g_log){
            ImGui::TextColored(LvlCol(ln.lvl),"%s",LvlTag(ln.lvl));
            ImGui::SameLine(0,4);
            ImGui::TextColored(ImVec4(0.88f,0.88f,0.92f,1),"%s",ln.s.c_str());
        }
        if(g_autoScroll && ImGui::GetScrollY()>=ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
    }
    ImGui::EndChild();
}
static void TabProc(){
    ImGui::Text("Process Information");
    ImGui::Separator(); ImGui::Spacing();
    ImGui::Text("Process Name : "); ImGui::SameLine();
    ImGui::TextColored(ACCENT,"%s",Narrow(g_pname).c_str());
    ImGui::Text("PID          : "); ImGui::SameLine();
    ImGui::TextColored(GREEN,"%u",(uint32_t)g_pid);
    ImGui::Text("Build        : "); ImGui::SameLine();
    ImGui::TextColored(GREEN,"%s",g_build.c_str());
    ImGui::Spacing();
    if(ImGui::Button("Refresh",ImVec2(120,30))){
        Scanner::ProcessInfo pi{g_pid,g_pname};
        Scanner::FindFiveM(pi,g_build); g_pid=pi.pid; g_pname=pi.name;
        Log(INFO,"Process list refreshed.");
    }
}
static void TabSettings(){
    ImGui::Text("Settings"); ImGui::Separator(); ImGui::Spacing();
    ImGui::Checkbox("Auto-scroll log",&g_autoScroll);
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if(ImGui::Button("Clear Console",ImVec2(160,30))) ClearLog();
}
// ---- WinMain ----
int APIENTRY WinMain(HINSTANCE hi,HINSTANCE,LPSTR,int ncs){
    WNDCLASSEXW wc={};
    wc.cbSize=sizeof(wc); wc.style=CS_CLASSDC; wc.lpfnWndProc=WndProc; wc.hInstance=hi;
    wc.lpszClassName=WCLASS; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);
    HWND hw=CreateWindowExW(0,WCLASS,TITLE,WS_OVERLAPPEDWINDOW,100,100,900,680,nullptr,nullptr,hi,nullptr);
    if(!D3DInit(hw)){KRT();g_swap->Release();g_d3dCtx->Release();g_d3dDev->Release();DestroyWindow(hw);UnregisterClassW(WCLASS,hi);return 1;}
    ShowWindow(hw,ncs); UpdateWindow(hw);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename=nullptr; io.LogFilename=nullptr;
    io.Fonts->AddFontDefault();
    ApplyTheme();
    ImGui_ImplWin32_Init(hw); ImGui_ImplDX11_Init(g_d3dDev,g_d3dCtx);

    Log(INFO,"H89 DEV - FiveM Offset Dumper");
    Log(INFO,"Press Start Dump to begin.");

    MSG msg={};
    while(msg.message!=WM_QUIT){
        if(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);continue;}
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.f);
        ImGui::Begin("##main",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();

        // Title bar (just like original - simple text line)
        ImGui::TextColored(ACCENT,"H89 DEV");
        ImGui::Separator();
        ImGui::Spacing();

        // Sidebar + Content
        ImGui::Columns(2,"##c",false);
        ImGui::SetColumnWidth(0,200);
        Sidebar();
        ImGui::NextColumn();
        switch(g_tab){
            case SCANNER:  TabScanner();  break;
            case PROCINFO: TabProc();     break;
            case SETTINGS: TabSettings(); break;
        }
        ImGui::Columns(1);
        ImGui::End();

        ImGui::Render();
        const float cc[4]={0.06f,0.05f,0.09f,1.f};
        g_d3dCtx->OMSetRenderTargets(1,&g_rtv,nullptr);
        g_d3dCtx->ClearRenderTargetView(g_rtv,cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1,0);
    }
    g_running=false; if(g_thr.joinable())g_thr.join();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    KRT(); g_swap->Release(); g_d3dCtx->Release(); g_d3dDev->Release();
    DestroyWindow(hw); UnregisterClassW(WCLASS,hi);
    return 0;
}
static LRESULT WINAPI WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(ImGui_ImplWin32_WndProcHandler(h,m,w,l))return true;
    switch(m){
        case WM_SIZE:if(g_d3dDev&&w!=SIZE_MINIMIZED){KRT();g_swap->ResizeBuffers(0,(UINT)LOWORD(l),(UINT)HIWORD(l),DXGI_FORMAT_UNKNOWN,0);CRT();}return 0;
        case WM_SYSCOMMAND:if((w&0xfff0)==SC_KEYMENU)return 0;break;
        case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
