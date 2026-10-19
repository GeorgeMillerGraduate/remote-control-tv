#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

// Native x64 bootstrapper. It never requests elevation or changes system policy.
// The console build process remains visible, including all errors and downloads.
static BOOL is_file(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}
static BOOL join(wchar_t *out, size_t capacity, const wchar_t *root, const wchar_t *suffix) {
    if (wcslen(root) + wcslen(suffix) + 1 >= capacity) return FALSE;
    wcscpy(out, root);
    wcscat(out, suffix);
    return TRUE;
}
static int show_error(const wchar_t *message) {
    MessageBoxW(NULL, message, L"Jenga Remote", MB_OK | MB_ICONERROR);
    return 1;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR commandLine, int show) {
    (void)instance; (void)previous; (void)commandLine; (void)show;
    wchar_t root[32768], script[32768], source[32768], app[32768], work[32768];
    DWORD size = GetModuleFileNameW(NULL, root, 32768);
    if (!size || size >= 32768) return show_error(L"Cannot resolve the launcher's location.");
    wchar_t *last = wcsrchr(root, L'\\');
    if (!last) return show_error(L"Cannot resolve the project folder.");
    *last = L'\0';
    if (!join(app, 32768, root, L"\\dist\\JengaRemote\\JengaRemote.exe") ||
        !join(work, 32768, root, L"\\dist\\JengaRemote") ||
        !join(script, 32768, root, L"\\build-windows.ps1") ||
        !join(source, 32768, root, L"\\CMakeLists.txt"))
        return show_error(L"Project path is too long. Extract it into a shorter folder.");

    if (is_file(app)) {
        HINSTANCE result = ShellExecuteW(NULL, L"open", app, NULL, work, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) return show_error(L"Cannot open the deployed application. Keep its complete dist folder, including DLLs, together.");
        return 0;
    }
    if (!is_file(script) || !is_file(source)) return show_error(
        L"Extract the whole archive first. Place this launcher alongside build-windows.ps1, CMakeLists.txt and the src folder, then double-click it again.");

    wchar_t system[32768], powershell[32768], arguments[32768];
    DWORD length = GetSystemDirectoryW(system, 32768);
    if (!length || length >= 32768 || !join(powershell,32768,system,L"\\WindowsPowerShell\\v1.0\\powershell.exe"))
        return show_error(L"Cannot locate Windows PowerShell.");
    if (wcslen(script) + 128 >= 32768) return show_error(L"Project path is too long.");
    wcscpy(arguments,L"-NoLogo -NoProfile -ExecutionPolicy Bypass -File \"");
    wcscat(arguments,script);wcscat(arguments,L"\" -Launch -PauseOnError");
    HINSTANCE result = ShellExecuteW(NULL,L"open",powershell,arguments,root,SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) return show_error(L"Could not start the visible build console. Run BUILD-WINDOWS.cmd to see the error.");
    return 0;
}
