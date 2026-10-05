#include "native.c"
#include <assert.h>
int main(void) {
    g_dpi = 96;
    /* Default compact mode must fit more rows than the old 28px layout. */
    assert(D(28) >= 25 && D(28) < 28);
    assert(D(13) >= 10);
    g_dpi = 192;
    assert(D(28) < 56);
    for (int mode = 0; mode < 2; ++mode) {
        g_density = mode;
        g_dpi = 96;
        int row = D(28);
        assert(row > D(21));
        g_dpi = 192;
        assert(D(28) >= row * 2 - 1 && D(28) <= row * 2 + 1);
    }
    GetFullPathNameW(L"bin\\density-test.ini", MAX_PATH, g_settings_path, NULL);
    DeleteFileW(g_settings_path);g_refresh_minutes=-1;LoadSettings();assert(g_refresh_minutes==30);
    g_workday_started = CurrentFileTime();
    for (int mode = 0; mode < 2; ++mode) {
        g_density = mode;
        SaveSettings();
        g_density = -1;
        LoadSettings();
        assert(g_density == mode);
    }
    WritePrivateProfileStringW(L"Interface", L"InterfaceSize", L"99", g_settings_path);
    LoadSettings();
    assert(g_density == 0);
    puts("Density tests passed");
    return 0;
}
