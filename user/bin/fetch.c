/*
 * VibeOS fetch command
 *
 * Usage: fetch <hostname> [path]
 * Example: fetch example.com /
 *          fetch httpbin.org /get
 */

#include "../lib/vibe.h"

static kapi_t *k;

// Simple output helper
static void out_puts(const char *s) {
    if (k->stdio_puts) k->stdio_puts(s);
    else k->puts(s);
}

static void out_putc(char c) {
    if (k->stdio_putc) k->stdio_putc(c);
    else k->putc(c);
}

// Print a number
static void out_num(int n) {
    if (n < 0) {
        out_putc('-');
        n = -n;
    }
    if (n == 0) {
        out_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        out_putc(buf[--i]);
    }
}

int main(kapi_t *kapi, int argc, char **argv) {
    k = kapi;

    if (argc < 2) {
        out_puts("Usage: fetch <hostname> [path]\n");
        out_puts("Example: fetch example.com /\n");
        return 1;
    }

    const char *hostname = argv[1];
    const char *path = argc > 2 ? argv[2] : "/";

    // Resolve hostname
    out_puts("Resolving ");
    out_puts(hostname);
    out_puts("...\n");

    uint32_t ip = k->dns_resolve(hostname);
    if (ip == 0) {
        out_puts("Could not resolve hostname\n");
        return 1;
    }

    // Connect to port 80
    out_puts("Connecting...\n");
    int sock = k->tcp_connect(ip, 80);
    if (sock < 0) {
        out_puts("Connection failed\n");
        return 1;
    }

    // Build HTTP request
    char request[512];
    char *p = request;

    // GET /path HTTP/1.0\r\n
    const char *get = "GET ";
    while (*get) *p++ = *get++;
    while (*path) *p++ = *path++;
    const char *http = " HTTP/1.0\r\n";
    while (*http) *p++ = *http++;

    // Host: hostname\r\n
    const char *host = "Host: ";
    while (*host) *p++ = *host++;
    const char *h = hostname;
    while (*h) *p++ = *h++;
    *p++ = '\r'; *p++ = '\n';

    // Connection: close\r\n
    const char *conn = "Connection: close\r\n";
    while (*conn) *p++ = *conn++;

    // End of headers
    *p++ = '\r'; *p++ = '\n';
    *p = '\0';

    int req_len = p - request;

    // Send request
    out_puts("Sending request...\n\n");
    int sent = k->tcp_send(sock, request, req_len);
    if (sent < 0) {
        out_puts("Failed to send request\n");
        k->tcp_close(sock);
        return 1;
    }

    // Receive response
    char buf[1024];
    int total = 0;

    while (1) {
        int n = k->tcp_recv(sock, buf, sizeof(buf) - 1);
        if (n < 0) {
            // Connection closed
            break;
        }
        if (n == 0) {
            // No data yet, poll and retry
            k->net_poll();
            k->sleep_ms(10);
            continue;
        }

        // Print received data
        buf[n] = '\0';
        out_puts(buf);
        total += n;
    }

    out_puts("\n\n--- ");
    out_num(total);
    out_puts(" bytes received ---\n");

    k->tcp_close(sock);
    return 0;
}
