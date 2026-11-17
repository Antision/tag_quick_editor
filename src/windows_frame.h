#ifndef WINDOWS_FRAME_H
#define WINDOWS_FRAME_H

// Win32 细节,只给无边框窗口用。单独放一个头文件是为了两处 nativeEvent
// (主窗口、日志窗口)共用同一份解释,而不是各抄一遍。

#ifdef _WIN32
#include <windows.h>

/**
 * @brief 最大化时系统给窗口多加的那圈"看不见的边框"有多厚(物理像素)。
 *
 * 无边框窗口最大化时,Windows 会把窗口矩形按看不见的边框往外撑,实测窗口矩形 =
 * 工作区每边 +9 物理像素(-9,-9 2578x1408 对工作区 0,0 2560x1390):它假设客户区
 * 随后会被非客户区裁回来,而我们不画非客户区。这 9 像素就是"最大化后内容超出屏幕"
 * 的来源,内容要自己往里让这么多(见 MainWindow::updateMaximizedContentInset)。
 *
 * 这里只是量出厚度,不改任何矩形:对**半透明**窗口不能靠 WM_NCCALCSIZE 把客户区
 * 收窄来让内容(见 teClampMaximizedClientArea 的说明)。
 */
inline int teMaximizedFrameOvershoot(HWND hwnd)
{
    if (!IsZoomed(hwnd))
        return 0;
    RECT wr = { 0, 0, 0, 0 };
    if (!GetWindowRect(hwnd, &wr))
        return 0;
    RECT proposed = wr;
    HMONITOR monitor = MonitorFromRect(&proposed, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(monitor, &info))
        return 0;
    const int dx = ((wr.right - wr.left) - (info.rcWork.right - info.rcWork.left)) / 2;
    const int dy = ((wr.bottom - wr.top) - (info.rcWork.bottom - info.rcWork.top)) / 2;
    return dx > dy ? dx : dy;
}

/**
 * @brief WM_NCCALCSIZE 的共用处理:窗口最大化时,客户区只取当前屏幕的工作区。
 *
 * **只给不透明窗口用**(日志窗口)。半透明窗口(主窗口,WA_TranslucentBackground)
 * 一旦让客户区小于窗口矩形,Qt 的画布就再也不跟着放大:窗口是 2578x1408 了,画布还
 * 停在最大化之前的尺寸,于是屏幕左上角一块是界面、其余全是透明的,点最大化看起来
 * 完全没效果。实测(probe):不透明 + 收窄 → 铺满;半透明 + 收窄 → 只画出 22.5%。
 * 半透明窗口改成"不动客户区,内容自己往里让 teMaximizedFrameOvershoot()"。
 *
 * 只往小里收,绝不把客户区报得比窗口还大:窗口进入最大化状态和窗口被放到最大化
 * 矩形上不是同一时刻,过渡帧里系统发来的还是原来的窗口矩形。
 *
 * 只在 wParam != 0 时调用;返回后调用方应把 *result 置 0 并 return true。
 */
inline void teClampMaximizedClientArea(MSG* msg)
{
    if (!IsZoomed(msg->hwnd))
        return;
    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
    // 用"提议的窗口矩形"去找显示器,而不是用窗口当前位置:从一块屏最大化到另一块
    // 屏时,窗口此刻还在旧屏幕上,按旧屏幕的工作区收窄会算出一个只有半块屏的客户区。
    RECT proposed = params->rgrc[0];
    HMONITOR monitor = MonitorFromRect(&proposed, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = { sizeof(MONITORINFO) };
    if (!GetMonitorInfo(monitor, &info))
        return;
    const RECT work = info.rcWork;
    RECT& client = params->rgrc[0];
    if (client.left < work.left)
        client.left = work.left;
    if (client.top < work.top)
        client.top = work.top;
    if (client.right > work.right)
        client.right = work.right;
    if (client.bottom > work.bottom)
        client.bottom = work.bottom;
}

#endif // _WIN32
#endif // WINDOWS_FRAME_H
