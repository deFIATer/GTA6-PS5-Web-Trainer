/*
 * GTA VI Web Trainer — PS5 Home Screen / Itemzflow Web Shortcut Launcher (eboot.bin)
 * Opens the built-in PS5 Web Browser directly to http://127.0.0.1:8080
 * (served locally by GTA6WebTrainer.elf with full DualSense gamepad navigation).
 */

#include <stddef.h>
#include <stdint.h>

extern int sceSystemServiceLaunchWebBrowser(const char *uri, void *param);

int main(void) {
    sceSystemServiceLaunchWebBrowser("http://127.0.0.1:8080", NULL);
    return 0;
}
