// Small native Windows demo of the real interpreter. Not a game launcher.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core/machine.h"
#include <array>
#include <cstdint>
#include <string>
namespace {
constexpr int run_button=101;
HWND result_label=nullptr;
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND title=CreateWindowExW(0,L"STATIC",L"NIGHTFALL  /  ДЕМО",WS_CHILD|WS_VISIBLE,28,24,460,28,hwnd,nullptr,nullptr,nullptr);
        HWND info=CreateWindowExW(0,L"STATIC",L"Исследовательский прототип, а не эмулятор PS5.\r\nИгры, PS5 и GTA VI пока не запускаются.\r\n\r\nКнопка выполняет оригинальную тестовую программу в ядре NIGHTFALL.",WS_CHILD|WS_VISIBLE,28,60,480,90,hwnd,nullptr,nullptr,nullptr);
        HWND button=CreateWindowExW(0,L"BUTTON",L"Запустить тест ядра",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,28,162,210,38,hwnd,reinterpret_cast<HMENU>(run_button),nullptr,nullptr);
        result_label=CreateWindowExW(0,L"STATIC",L"Результат: ожидание запуска",WS_CHILD|WS_VISIBLE,28,220,480,54,hwnd,nullptr,nullptr,nullptr);
        for (HWND control : {title,info,button,result_label}) SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp)==run_button && HIWORD(wp)==BN_CLICKED) {
            constexpr std::array<std::uint8_t,17> program{0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4};
            nightfall::Machine machine(4096);
            if (!machine.memory.write(0,program)) { SetWindowTextW(result_label,L"Ошибка подготовки тестовой памяти."); return 0; }
            const auto report=machine.run_detailed(3);
            if (report.result==nightfall::RunResult::halted && machine.cpu.state.rax==42)
                SetWindowTextW(result_label,L"Тест выполнен: RAX = 42, 3 инструкции.\r\nЭто не запуск игры.");
            else SetWindowTextW(result_label,L"Тест ядра завершился с ошибкой.");
            return 0;
        }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int show) {
    const wchar_t class_name[]=L"NightfallDemoWindow";
    WNDCLASSW wc{}; wc.lpfnWndProc=window_proc; wc.hInstance=instance; wc.lpszClassName=class_name;
    wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if (!RegisterClassW(&wc)) return 1;
    HWND window=CreateWindowExW(0,class_name,L"NIGHTFALL — демо ядра",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                                CW_USEDEFAULT,CW_USEDEFAULT,550,330,nullptr,nullptr,instance,nullptr);
    if (!window) return 1;
    ShowWindow(window,show); UpdateWindow(window);
    MSG message{}; while (GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return static_cast<int>(message.wParam);
}
