#include "App/MessageLoop.h"
#include <windows.h>
#include <dwmapi.h>
#include <timeapi.h>
#include <algorithm>
#include "Core/Async.h"
#include "UI/Window.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

namespace CustomCursor::App {

namespace {

// High-resolution waitable timer (Windows 10 1803+): plain waits are rounded up to the 15.6 ms
// system tick, which made the ~60 fps animations land on uneven frames. Older systems get a 1 ms
// timer resolution instead.
HANDLE PacingTimer() {
    static HANDLE timer = [] {
        HANDLE t = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!t) timeBeginPeriod(1);
        return t;
    }();
    return timer;
}

// Arms the pacing timer `seconds` from now (false when there is no high-resolution timer)
bool ArmTimer(double seconds) {
    HANDLE timer = PacingTimer();
    if (!timer) return false;
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(std::max(0.0, seconds) * 1e7);   // relative, 100 ns units
    return SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0) != FALSE;
}

void SleepUntil(double deadline) {
    const double left = deadline - Async::Now();
    if (left <= 0.0) return;
    if (ArmTimer(left)) WaitForSingleObject(PacingTimer(), INFINITE);
    else Sleep(static_cast<DWORD>(left * 1000.0));
}

} // namespace

int RunMessageLoop() {
    MSG msg{};
    double lastPresent = 0.0;
    while (true) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return static_cast<int>(msg.wParam);
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Async::Pump();

        bool rendered = false;
        // Copy: a frame may open or close windows
        const auto windows = UI::Window::All();
        for (UI::Window* w : windows) {
            auto& all = UI::Window::All();
            if (std::find(all.begin(), all.end(), w) == all.end()) continue;
            if (w->NeedsFrame()) {
                w->Frame();
                rendered = true;
            }
        }

        if (rendered) {
            // Pace to the compositor (one frame per refresh, every window presents with interval 0).
            // DwmFlush returns at once while DWM isn't composing (a fullscreen game in front, a locked
            // screen): then cap the loop at the refresh rate itself instead of spinning.
            const double period = UI::Window::RefreshPeriod();
            if (FAILED(DwmFlush()) || Async::Now() - lastPresent < period * 0.75) SleepUntil(lastPresent + period);
            lastPresent = Async::Now();
        } else {
            HANDLE handles[2];
            DWORD count = 0;
            if (HANDLE wake = static_cast<HANDLE>(Async::WakeEvent())) handles[count++] = wake;
            DWORD timeout = Async::NextTimerDelay();
            for (UI::Window* w : UI::Window::All()) timeout = std::min<DWORD>(timeout, w->NextFrameDelay());
            if (timeout != INFINITE && timeout > 0 && ArmTimer(timeout / 1000.0)) {
                handles[count++] = PacingTimer();
                timeout = INFINITE;
            }
            MsgWaitForMultipleObjectsEx(count, count ? handles : nullptr, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }
}

} // namespace CustomCursor::App
