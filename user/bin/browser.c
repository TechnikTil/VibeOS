/*
 * VibeOS Browser Launcher
 * Launches the Python browser via MicroPython
 */

#include "../lib/vibe.h"

int main(kapi_t *api, int argc, char **argv) {
    char *args[] = {"/bin/micropython", "/bin/browser.py", 0};
    return api->exec_args("/bin/micropython", 2, args);
}
