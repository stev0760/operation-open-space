/* Oracle compatibility shim, loaded into the game's winevdm process as a native version.dll.
 *
 * Wine 11.0 returns NULL from CreateCompatibleBitmap(NULL, cx, cy): win32u passes the
 * NULL DC's PLANES/BITSPIXEL (0/0) on to NtGdiCreateBitmap, which rejects planes != 1.
 * Windows returns a monochrome bitmap for a NULL DC (ReactOS NtGdiCreateCompatibleBitmap:
 * "if (!hDC) return GreCreateBitmap(Width, Height, 1, 1, 0);"). The game creates
 * bitmaps this way during the New Game intro and faults on the NULL result.
 *
 * DllMain points gdi32!CreateCompatibleBitmap at shim_ccb: NULL DC -> 1 bpp bitmap,
 * everything else -> win32u!NtGdiCreateCompatibleBitmap, exactly as gdi32 does.
 * Each NULL-DC call is logged to stderr as "inspace-shim: ..." so traces show it.
 * The version.dll exports forward to kernelbase (as Wine's own version.dll does),
 * except VerInstallFileA/W, which the game never calls and which fail here.
 */
#include <windows.h>
#include <stdio.h>

typedef HBITMAP (WINAPI *ccb_fn)(HDC, INT, INT);
static ccb_fn nt_ccb;

static void shim_log(const char *msg)
{
    DWORD n;
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err && err != INVALID_HANDLE_VALUE) WriteFile(err, msg, lstrlenA(msg), &n, NULL);
}

static HBITMAP WINAPI shim_ccb(HDC hdc, INT width, INT height)
{
    char buf[128];
    HBITMAP bmp;

    if (!width || !height) return (HBITMAP)GetStockObject(STOCK_LAST + 1); /* gdi32's own rule */
    if (hdc) return nt_ccb(hdc, width, height);
    bmp = (width > 0 && height > 0) ? CreateBitmap(width, height, 1, 1, NULL) : NULL;
    snprintf(buf, sizeof(buf), "inspace-shim: CreateCompatibleBitmap(NULL,%d,%d) -> 1bpp %p\n",
             width, height, (void *)bmp);
    shim_log(buf);
    return bmp;
}

static void install_hook(void)
{
    HMODULE gdi = GetModuleHandleA("gdi32.dll"), w32u = GetModuleHandleA("win32u.dll");
    BYTE *target;
    DWORD old;

    if (!gdi) gdi = LoadLibraryA("gdi32.dll");
    if (!w32u) w32u = LoadLibraryA("win32u.dll");
    target = gdi ? (BYTE *)GetProcAddress(gdi, "CreateCompatibleBitmap") : NULL;
    nt_ccb = w32u ? (ccb_fn)GetProcAddress(w32u, "NtGdiCreateCompatibleBitmap") : NULL;
    if (!target || !nt_ccb || !VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old))
    {
        shim_log("inspace-shim: hook NOT installed\n");
        return;
    }
    target[0] = 0xE9; /* jmp rel32 */
    *(DWORD *)(target + 1) = (DWORD)((BYTE *)shim_ccb - (target + 5));
    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    shim_log("inspace-shim: gdi32!CreateCompatibleBitmap hooked (NULL DC -> 1bpp)\n");
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(inst);
        install_hook();
    }
    return TRUE;
}

DWORD WINAPI VerInstallFileA(DWORD flags, LPSTR src, LPSTR dst, LPSTR srcdir, LPSTR dstdir,
                             LPSTR curdir, LPSTR tmp, PUINT len)
{
    (void)flags; (void)src; (void)dst; (void)srcdir; (void)dstdir; (void)curdir; (void)tmp; (void)len;
    return VIF_CANNOTREADSRC;
}

DWORD WINAPI VerInstallFileW(DWORD flags, LPWSTR src, LPWSTR dst, LPWSTR srcdir, LPWSTR dstdir,
                             LPWSTR curdir, LPWSTR tmp, PUINT len)
{
    (void)flags; (void)src; (void)dst; (void)srcdir; (void)dstdir; (void)curdir; (void)tmp; (void)len;
    return VIF_CANNOTREADSRC;
}
