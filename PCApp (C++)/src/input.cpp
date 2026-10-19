#include "input.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMap>
#include <QScreen>
#include <QVector>
#include <algorithm>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
// The ABI is stable; these declarations also permit building with runtime XTest headers absent.
extern "C" {
Bool XTestFakeKeyEvent(Display *, unsigned int, Bool, unsigned long);
Bool XTestFakeButtonEvent(Display *, unsigned int, Bool, unsigned long);
Bool XTestFakeRelativeMotionEvent(Display *, int, int, unsigned long);
Bool XTestFakeMotionEvent(Display *, int, int, int, unsigned long);
}
#endif
namespace {
void check(bool ok) {
    if (!ok)
        throw std::runtime_error("Operating system rejected input");
}
#ifdef Q_OS_WIN
WORD windowsKey(int code) {
    if ((code >= 48 && code <= 90) || (code >= 112 && code <= 123))
        return static_cast<WORD>(code);
    switch (code) {
    case 10:
        return VK_RETURN;
    case 127:
        return VK_DELETE;
    case 524:
        return VK_LWIN;
    case 8:
    case 9:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 27:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 45:
        return static_cast<WORD>(code);
    case 44:
        return VK_OEM_COMMA;
    case 46:
        return VK_OEM_PERIOD;
    case 47:
        return VK_OEM_2;
    case 59:
        return VK_OEM_1;
    case 61:
        return VK_OEM_PLUS;
    case 91:
        return VK_OEM_4;
    case 92:
        return VK_OEM_5;
    case 93:
        return VK_OEM_6;
    case 222:
        return VK_OEM_7;
    default:
        throw std::runtime_error("Unsupported Java virtual key");
    }
}
void sendKey(WORD code, bool down, bool unicode = false) {
    INPUT event{};
    event.type = INPUT_KEYBOARD;
    event.ki.wVk = unicode ? 0 : code;
    event.ki.wScan = unicode ? code : 0;
    event.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (unicode ? KEYEVENTF_UNICODE : 0);
    if (!unicode && ((code >= VK_PRIOR && code <= VK_DOWN) || code == VK_INSERT ||
                     code == VK_DELETE || code == VK_LWIN))
        event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    check(SendInput(1, &event, sizeof(event)) == 1);
}
struct MonitorSearch {
    QString name;
    RECT bounds{};
    bool found = false;
};
BOOL CALLBACK findMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto *search = reinterpret_cast<MonitorSearch *>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO *>(&info)) &&
        QString::fromWCharArray(info.szDevice) == search->name) {
        search->bounds = info.rcMonitor;
        search->found = true;
        return FALSE;
    }
    return TRUE;
}
#else
KeySym xKey(int code) {
    if (code >= 65 && code <= 90)
        return static_cast<KeySym>(code + 32);
    if (code >= 48 && code <= 57)
        return static_cast<KeySym>(code);
    if (code >= 112 && code <= 123)
        return XK_F1 + code - 112;
    switch (code) {
    case 8:
        return XK_BackSpace;
    case 9:
        return XK_Tab;
    case 10:
        return XK_Return;
    case 16:
        return XK_Shift_L;
    case 17:
        return XK_Control_L;
    case 18:
        return XK_Alt_L;
    case 20:
        return XK_Caps_Lock;
    case 27:
        return XK_Escape;
    case 32:
        return XK_space;
    case 33:
        return XK_Page_Up;
    case 34:
        return XK_Page_Down;
    case 35:
        return XK_End;
    case 36:
        return XK_Home;
    case 37:
        return XK_Left;
    case 38:
        return XK_Up;
    case 39:
        return XK_Right;
    case 40:
        return XK_Down;
    case 127:
        return XK_Delete;
    case 524:
        return XK_Super_L;
    case 44:
        return XK_comma;
    case 46:
        return XK_period;
    case 47:
        return XK_slash;
    case 59:
        return XK_semicolon;
    case 61:
        return XK_equal;
    default:
        throw std::runtime_error("Unsupported Java virtual key");
    }
}
#endif
} // namespace
NativeInput::NativeInput() {
#ifndef Q_OS_WIN
    if (qEnvironmentVariable("XDG_SESSION_TYPE").compare("wayland", Qt::CaseInsensitive) == 0)
        throw std::runtime_error("Wayland control is unsupported. Log into an X11 session.");
    display = XOpenDisplay(nullptr);
    if (!display)
        throw std::runtime_error("An active X11 desktop is required");
#endif
}
NativeInput::~NativeInput() {
    releaseAll();
#ifndef Q_OS_WIN
    if (display)
        XCloseDisplay(static_cast<Display *>(display));
#endif
}
void NativeInput::move(int dx, int dy) {
#ifdef Q_OS_WIN
    POINT point{};
    check(GetCursorPos(&point));
    check(SetCursorPos(point.x + dx, point.y + dy));
#else
    check(XTestFakeRelativeMotionEvent(static_cast<Display *>(display), dx, dy, 0));
    XFlush(static_cast<Display *>(display));
#endif
}
void NativeInput::point(int monitor, double x, double y) {
    const auto screens = QGuiApplication::screens();
    if (monitor < 0 || monitor >= screens.size())
        throw std::runtime_error("Invalid monitor");
#ifdef Q_OS_WIN
    MonitorSearch search{screens[monitor]->name()};
    EnumDisplayMonitors(nullptr, nullptr, findMonitor, reinterpret_cast<LPARAM>(&search));
    if (!search.found)
        throw std::runtime_error("Cannot map this display to Windows coordinates");
    check(SetCursorPos(
        search.bounds.left + static_cast<int>(x * (search.bounds.right - search.bounds.left - 1)),
        search.bounds.top + static_cast<int>(y * (search.bounds.bottom - search.bounds.top - 1))));
#else
    const QRect bounds = screens[monitor]->geometry();
    check(XTestFakeMotionEvent(static_cast<Display *>(display), -1,
                               bounds.x() + static_cast<int>(x * (bounds.width() - 1)),
                               bounds.y() + static_cast<int>(y * (bounds.height() - 1)), 0));
    XFlush(static_cast<Display *>(display));
#endif
}
void NativeInput::button(int button, bool down) {
#ifdef Q_OS_WIN
    const DWORD flags = button == 1   ? (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
                        : button == 2 ? (down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP)
                                      : (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
    INPUT event{};
    event.type = INPUT_MOUSE;
    event.mi.dwFlags = flags;
    check(SendInput(1, &event, sizeof(event)) == 1);
#else
    const int native = button == 2 ? 2 : button == 3 ? 3 : 1;
    check(XTestFakeButtonEvent(static_cast<Display *>(display), native, down, 0));
    XFlush(static_cast<Display *>(display));
#endif
    if (down)
        buttons.insert(button);
    else
        buttons.erase(button);
}
void NativeInput::scroll(int amount) {
#ifdef Q_OS_WIN
    INPUT event{};
    event.type = INPUT_MOUSE;
    event.mi.dwFlags = MOUSEEVENTF_WHEEL;
    event.mi.mouseData = static_cast<DWORD>(-amount * WHEEL_DELTA);
    check(SendInput(1, &event, sizeof(event)) == 1);
#else
    for (int i = 0; i < std::abs(amount); ++i) {
        const int code = amount < 0 ? 4 : 5;
        XTestFakeButtonEvent(static_cast<Display *>(display), code, True, 0);
        XTestFakeButtonEvent(static_cast<Display *>(display), code, False, 0);
    }
    XFlush(static_cast<Display *>(display));
#endif
}
void NativeInput::key(int code, bool down) {
#ifdef Q_OS_WIN
    sendKey(windowsKey(code), down);
#else
    const auto native = XKeysymToKeycode(static_cast<Display *>(display), xKey(code));
    if (!native)
        throw std::runtime_error("Key unavailable in this keyboard layout");
    check(XTestFakeKeyEvent(static_cast<Display *>(display), native, down, 0));
    XFlush(static_cast<Display *>(display));
#endif
    if (down)
        keys.insert(code);
    else
        keys.erase(code);
}
void NativeInput::text(const QString &value) {
#ifdef Q_OS_WIN
    // Windows accepts UTF-16 code units directly, without touching the user's clipboard.
    for (QChar character : value) {
        sendKey(character.unicode(), true, true);
        sendKey(character.unicode(), false, true);
    }
#else
    QGuiApplication::clipboard()->setText(value);
    key(17, true);
    key(86, true);
    key(86, false);
    key(17, false);
#endif
}
void NativeInput::media(const QString &action) {
#ifdef Q_OS_WIN
    const QMap<QString, WORD> codes{
        {"PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE}, {"NEXT", VK_MEDIA_NEXT_TRACK},
        {"PREVIOUS", VK_MEDIA_PREV_TRACK},   {"VOLUME_UP", VK_VOLUME_UP},
        {"VOLUME_DOWN", VK_VOLUME_DOWN},     {"MUTE", VK_VOLUME_MUTE}};
    if (!codes.contains(action))
        throw std::runtime_error("Invalid media action");
    sendKey(codes[action], true);
    sendKey(codes[action], false);
#else
    const QMap<QString, KeySym> codes{
        {"PLAY_PAUSE", XF86XK_AudioPlay},         {"NEXT", XF86XK_AudioNext},
        {"PREVIOUS", XF86XK_AudioPrev},           {"VOLUME_UP", XF86XK_AudioRaiseVolume},
        {"VOLUME_DOWN", XF86XK_AudioLowerVolume}, {"MUTE", XF86XK_AudioMute}};
    if (!codes.contains(action))
        throw std::runtime_error("Invalid media action");
    const auto code = XKeysymToKeycode(static_cast<Display *>(display), codes[action]);
    if (!code)
        throw std::runtime_error("Media key unavailable in X11 keymap");
    XTestFakeKeyEvent(static_cast<Display *>(display), code, True, 0);
    XTestFakeKeyEvent(static_cast<Display *>(display), code, False, 0);
    XFlush(static_cast<Display *>(display));
#endif
}
void NativeInput::releaseAll() noexcept {
    const auto heldKeys = keys, heldButtons = buttons;
    for (int code : heldKeys) {
        try {
            key(code, false);
        } catch (...) {
        }
    }
    for (int code : heldButtons) {
        try {
            button(code, false);
        } catch (...) {
        }
    }
    keys.clear();
    buttons.clear();
}
