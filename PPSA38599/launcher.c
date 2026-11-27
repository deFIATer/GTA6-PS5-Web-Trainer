/*
 * GTA VI Web Trainer — PS5 Media App Web Browser Launcher (eboot.bin)
 * Hides splash screen and opens the standard PS5 Web Browser window directly to
 * http://127.0.0.1:8080 (served locally by GTA6WebTrainer.elf).
 */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t size;
    uint32_t reserved[3];
} SceSystemServiceLaunchWebBrowserParam;

extern int sceSystemServiceHideSplashScreen(void);
extern int sceSystemServiceLaunchWebBrowser(const char *uri, const SceSystemServiceLaunchWebBrowserParam *param);

int main(void) {
    sceSystemServiceHideSplashScreen();
    SceSystemServiceLaunchWebBrowserParam param = {
        .size = sizeof(SceSystemServiceLaunchWebBrowserParam),
        .reserved = {0, 0, 0},
    };
    sceSystemServiceLaunchWebBrowser("http://127.0.0.1:8080", &param);
    return 0;
}
