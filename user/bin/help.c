/*
 * VibeOS Help - opens browser with help documentation
 */

#include "../lib/vibe.h"

int main(kapi_t *k, int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Launch browser in kiosk mode with help documentation (call micropython directly)
    char *args[] = { "/bin/micropython", "/bin/browser.py", "--kiosk", "file:///etc/help/index.html" };
    return k->exec_args("/bin/micropython", 4, args);
}
