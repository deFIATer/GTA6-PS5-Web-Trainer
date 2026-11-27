/*
 * GTA VI (PPSA38500 01.002.001) Standalone PS5 Web Trainer (.elf)
 * Runs directly on the PS5 console (port 8080) and talks to local ps5debug-NG (127.0.0.1:744).
 * Open http://127.0.0.1:8080 in PS5 browser or http://<PS5_IP>:8080 on your phone — no PC needed!
 *
 * Game offsets & vehicle list based on basic-menu-src by XeMo.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "vehicles_data.h"
#include "web_assets.h"

#define HTTP_PORT 8080
#define PS5DBG_MAGIC 0xFFAABBCCu
#define CMD_SUCCESS  0x80000000u

#define CMD_PROC_LIST    0xBDAA0001u
#define CMD_PROC_READ    0xBDAA0002u
#define CMD_PROC_WRITE   0xBDAA0003u
#define CMD_PROC_MAPS    0xBDAA0004u
#define CMD_PROC_PROTECT 0xBDAA0008u
#define CMD_PROC_ALLOC   0xBDAA000Bu
#define CMD_CONSOLE_NOTIFY 0xBDDD0004u

/* PPSA38500 01.002.001 offsets relative to eboot.bin base */
#define OFF_GOT_PAD_READ_STATE 0xA3EA780ULL
#define OFF_CHEAT_HEAL_REPAIR  0x53775C0ULL
#define OFF_CHEAT_WANTED_UP    0x5377AC0ULL
#define OFF_CHEAT_WANTED_DOWN  0x5377D50ULL
#define OFF_CHEAT_WEATHER      0x53780C0ULL
#define OFF_SPAWN_VEHICLE      0x5378380ULL
#define OFF_CLOCK_SET_TIME     0x537AFD0ULL
#define OFF_GET_PLAYER_COMP    0x1637050ULL
#define OFF_SET_PLAYER_WANTED  0x53ACD60ULL
#define OFF_HEALTH_RESTORE     0x4F0C3B0ULL
#define OFF_VEHICLE_FIX        0x80FEF80ULL
#define OFF_LOCAL_PLAYER_PED   0xDCDBCB0ULL
#define OFF_LOCAL_PLAYER_VEH   0xDCEBCE0ULL
#define OFF_GAME_CLOCK         0xDCEC6D0ULL
#define OFF_WANTED_LEVEL       0x5EFCULL

typedef struct {
    pthread_mutex_t lock;
    int sock;
    char host[64];
    int port;
    int pid;
    uint64_t base;
    uint64_t ctrl_addr;
    uint64_t hook_addr;
    uint64_t orig_pad;
    bool connected;
    bool hook_installed;
    bool god_mode;
    bool never_wanted;
    bool freeze_time;
    bool ps5_notify;
    int frozen_h;
    int frozen_m;
    char last_error[256];
} TrainerState;

static TrainerState G = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .sock = -1,
    .host = "127.0.0.1",
    .port = 744,
    .ps5_notify = true,
    .frozen_h = 12,
    .frozen_m = 0,
};

/* ---------- Low-level TCP helpers ---------- */

static int send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_all(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int tcp_connect(const char *ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* ---------- ps5debug-NG protocol ---------- */

static int dbg_send_cmd(int fd, uint32_t cmd, const void *body, uint32_t datalen) {
    uint32_t hdr[3] = {PS5DBG_MAGIC, cmd, datalen};
    if (send_all(fd, hdr, sizeof(hdr)) < 0) return -1;
    if (datalen > 0 && body) {
        if (send_all(fd, body, datalen) < 0) return -1;
    }
    return 0;
}

static int dbg_expect_ok(int fd) {
    uint32_t st = 0;
    if (recv_all(fd, &st, 4) < 0) return -1;
    return (st == CMD_SUCCESS) ? 0 : -1;
}

static int dbg_notify(const char *msg) {
    if (!G.ps5_notify || !msg || !msg[0]) return 0;
    int fd = tcp_connect(G.host, G.port);
    if (fd < 0) return -1;
    char full[256];
    snprintf(full, sizeof(full), "GTA VI Trainer\n%s", msg);
    uint32_t len = (uint32_t)strlen(full) + 1;
    uint32_t body[2] = {222u, len};
    int rc = -1;
    if (dbg_send_cmd(fd, CMD_CONSOLE_NOTIFY, body, 8) == 0 &&
        send_all(fd, full, len) == 0 &&
        dbg_expect_ok(fd) == 0) {
        rc = 0;
    }
    close(fd);
    return rc;
}

static int dbg_read(int fd, int pid, uint64_t addr, void *out, uint32_t len) {
    struct __attribute__((packed)) { uint32_t pid; uint64_t addr; uint32_t len; } req = {
        (uint32_t)pid, addr, len
    };
    if (dbg_send_cmd(fd, CMD_PROC_READ, &req, sizeof(req)) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    return recv_all(fd, out, len);
}

static int dbg_write(int fd, int pid, uint64_t addr, const void *data, uint32_t len) {
    struct __attribute__((packed)) { uint32_t pid; uint64_t addr; uint32_t len; } req = {
        (uint32_t)pid, addr, len
    };
    if (dbg_send_cmd(fd, CMD_PROC_WRITE, &req, sizeof(req)) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    if (send_all(fd, data, len) < 0) return -1;
    return dbg_expect_ok(fd);
}

static uint64_t dbg_alloc(int fd, int pid, uint32_t len) {
    struct __attribute__((packed)) { uint32_t pid; uint32_t len; } req = {(uint32_t)pid, len};
    if (dbg_send_cmd(fd, CMD_PROC_ALLOC, &req, sizeof(req)) < 0) return 0;
    if (dbg_expect_ok(fd) < 0) return 0;
    uint64_t addr = 0;
    if (recv_all(fd, &addr, 8) < 0) return 0;
    return addr;
}

static int dbg_protect(int fd, int pid, uint64_t addr, uint32_t len, uint32_t prot) {
    struct __attribute__((packed)) { uint32_t pid; uint64_t addr; uint32_t len; uint32_t prot; } req = {
        (uint32_t)pid, addr, len, prot
    };
    if (dbg_send_cmd(fd, CMD_PROC_PROTECT, &req, sizeof(req)) < 0) return -1;
    return dbg_expect_ok(fd);
}

static int dbg_find_eboot_pid(int fd) {
    if (dbg_send_cmd(fd, CMD_PROC_LIST, NULL, 0) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    uint32_t count = 0;
    if (recv_all(fd, &count, 4) < 0 || count > 4096) return -1;
    int found = -1;
    for (uint32_t i = 0; i < count; i++) {
        struct __attribute__((packed)) { char name[32]; int32_t pid; } entry;
        if (recv_all(fd, &entry, sizeof(entry)) < 0) return -1;
        entry.name[31] = '\0';
        if (strstr(entry.name, "eboot") != NULL) found = entry.pid;
    }
    return found;
}

static uint64_t dbg_find_eboot_base(int fd, int pid) {
    uint32_t req = (uint32_t)pid;
    if (dbg_send_cmd(fd, CMD_PROC_MAPS, &req, 4) < 0) return 0;
    if (dbg_expect_ok(fd) < 0) return 0;
    uint32_t count = 0;
    if (recv_all(fd, &count, 4) < 0 || count > 32768) return 0;
    uint64_t base = 0;
    for (uint32_t i = 0; i < count; i++) {
        struct __attribute__((packed)) {
            char name[32]; uint64_t start; uint64_t end; uint64_t offset; uint32_t prot;
        } m;
        if (recv_all(fd, &m, sizeof(m)) < 0) return 0;
        m.name[31] = '\0';
        if (base == 0 && (m.prot & 4) && strstr(m.name, "executable") != NULL) {
            base = m.start;
        }
    }
    return base ? base : 0x400000ULL;
}

/* 111-byte x86-64 GOT hook for scePadReadState (identical to Python trainer) */
static size_t build_pad_hook(uint64_t ctrl_addr, uint8_t *out) {
    uint8_t c[128];
    size_t n = 0;
    c[n++] = 0x55;                                     /* push rbp */
    c[n++] = 0x48; c[n++] = 0x89; c[n++] = 0xE5;       /* mov rbp, rsp */
    c[n++] = 0x41; c[n++] = 0x57;                      /* push r15 */
    c[n++] = 0x53;                                     /* push rbx */
    c[n++] = 0x57; c[n++] = 0x56; c[n++] = 0x52; c[n++] = 0x51; /* push rdi,rsi,rdx,rcx */
    c[n++] = 0x41; c[n++] = 0x50; c[n++] = 0x41; c[n++] = 0x51; /* push r8,r9 */
    c[n++] = 0x49; c[n++] = 0xBF;                      /* movabs r15, ctrl_addr */
    memcpy(c + n, &ctrl_addr, 8); n += 8;
    c[n++] = 0x41; c[n++] = 0xFF; c[n++] = 0x47; c[n++] = 0x0C; /* inc dword [r15+0x0C] */
    c[n++] = 0x41; c[n++] = 0x83; c[n++] = 0x7F; c[n++] = 0x08; c[n++] = 0x01; /* cmp [r15+8], 1 */
    c[n++] = 0x75; size_t jnz_off = n++;               /* jne skip */
    c[n++] = 0x41; c[n++] = 0xC7; c[n++] = 0x47; c[n++] = 0x08; /* mov dword [r15+8], 2 */
    c[n++] = 0x02; c[n++] = 0x00; c[n++] = 0x00; c[n++] = 0x00;
    c[n++] = 0x48; c[n++] = 0x83; c[n++] = 0xEC; c[n++] = 0x10; /* sub rsp, 0x10 */
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x47; c[n++] = 0x50; /* mov rax, [r15+0x50] */
    c[n++] = 0x48; c[n++] = 0x89; c[n++] = 0x44; c[n++] = 0x24; c[n++] = 0x08; /* mov [rsp+8], rax */
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x47; c[n++] = 0x48; /* mov rax, [r15+0x48] */
    c[n++] = 0x48; c[n++] = 0x89; c[n++] = 0x04; c[n++] = 0x24; /* mov [rsp], rax */
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x7F; c[n++] = 0x18; /* rdi */
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x77; c[n++] = 0x20; /* rsi */
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x57; c[n++] = 0x28; /* rdx */
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x4F; c[n++] = 0x30; /* rcx */
    c[n++] = 0x4D; c[n++] = 0x8B; c[n++] = 0x47; c[n++] = 0x38; /* r8 */
    c[n++] = 0x4D; c[n++] = 0x8B; c[n++] = 0x4F; c[n++] = 0x40; /* r9 */
    c[n++] = 0x41; c[n++] = 0xFF; c[n++] = 0x57; c[n++] = 0x10; /* call [r15+0x10] */
    c[n++] = 0x48; c[n++] = 0x83; c[n++] = 0xC4; c[n++] = 0x10; /* add rsp, 0x10 */
    c[n++] = 0x49; c[n++] = 0x89; c[n++] = 0x47; c[n++] = 0x58; /* mov [r15+0x58], rax */
    c[n++] = 0x41; c[n++] = 0xC7; c[n++] = 0x47; c[n++] = 0x08; /* mov dword [r15+8], 3 */
    c[n++] = 0x03; c[n++] = 0x00; c[n++] = 0x00; c[n++] = 0x00;
    size_t skip = n;
    c[jnz_off] = (uint8_t)(skip - (jnz_off + 1));
    c[n++] = 0x49; c[n++] = 0x8B; c[n++] = 0x07;       /* mov rax, [r15] */
    c[n++] = 0x41; c[n++] = 0x59; c[n++] = 0x41; c[n++] = 0x58; /* pop r9,r8 */
    c[n++] = 0x59; c[n++] = 0x5A; c[n++] = 0x5E; c[n++] = 0x5F; /* pop rcx,rdx,rsi,rdi */
    c[n++] = 0x5B; c[n++] = 0x41; c[n++] = 0x5F; c[n++] = 0x5D; /* pop rbx,r15,rbp */
    c[n++] = 0xFF; c[n++] = 0xE0;                      /* jmp rax */
    memcpy(out, c, n);
    return n;
}

static uint64_t game_call(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    if (!G.connected || !G.hook_installed || G.sock < 0) return 0;
    uint64_t args[9] = {fn, a1, a2, a3, a4, a5, 0, 0, 0};
    if (dbg_write(G.sock, G.pid, G.ctrl_addr + 0x10, args, sizeof(args)) < 0) return 0;
    uint32_t one = 1;
    if (dbg_write(G.sock, G.pid, G.ctrl_addr + 0x08, &one, 4) < 0) return 0;
    for (int i = 0; i < 200; i++) {
        uint32_t st = 0;
        if (dbg_read(G.sock, G.pid, G.ctrl_addr + 0x08, &st, 4) < 0) return 0;
        if (st == 3) {
            uint64_t res = 0;
            dbg_read(G.sock, G.pid, G.ctrl_addr + 0x58, &res, 8);
            uint32_t zero = 0;
            dbg_write(G.sock, G.pid, G.ctrl_addr + 0x08, &zero, 4);
            return res;
        }
        usleep(10000);
    }
    return 0;
}

static int trainer_connect_locked(const char *ip, int port) {
    if (G.sock >= 0) { close(G.sock); G.sock = -1; }
    G.connected = false;
    G.hook_installed = false;
    if (ip && ip[0]) snprintf(G.host, sizeof(G.host), "%s", ip);
    if (port > 0) G.port = port;

    G.sock = tcp_connect(G.host, G.port);
    if (G.sock < 0) {
        snprintf(G.last_error, sizeof(G.last_error), "Cannot connect to ps5debug-NG at %s:%d", G.host, G.port);
        return -1;
    }
    int pid = dbg_find_eboot_pid(G.sock);
    if (pid <= 0) {
        snprintf(G.last_error, sizeof(G.last_error), "GTA VI (eboot.bin) is not running");
        return -1;
    }
    G.pid = pid;
    G.base = dbg_find_eboot_base(G.sock, pid);

    /* Install main-thread GOT hook on scePadReadState */
    uint64_t orig = 0;
    if (dbg_read(G.sock, pid, G.base + OFF_GOT_PAD_READ_STATE, &orig, 8) == 0 && orig != 0) {
        uint64_t ctrl = dbg_alloc(G.sock, pid, 0x4000);
        uint64_t hook = dbg_alloc(G.sock, pid, 0x4000);
        if (ctrl && hook) {
            uint8_t zeros[256] = {0};
            dbg_write(G.sock, pid, ctrl, zeros, sizeof(zeros));
            dbg_write(G.sock, pid, ctrl, &orig, 8);
            uint8_t code[128];
            size_t clen = build_pad_hook(ctrl, code);
            dbg_write(G.sock, pid, hook, code, (uint32_t)clen);
            dbg_protect(G.sock, pid, hook, 0x4000, 5);
            dbg_write(G.sock, pid, G.base + OFF_GOT_PAD_READ_STATE, &hook, 8);
            G.ctrl_addr = ctrl;
            G.hook_addr = hook;
            G.orig_pad = orig;
            G.hook_installed = true;
        }
    }
    G.connected = true;
    G.last_error[0] = '\0';
    return 0;
}

static uint64_t get_ped_and_handle(uint32_t *out_handle) {
    uint64_t ped = 0;
    if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_PED, &ped, 8) < 0 || !ped) return 0;
    uint32_t h = 0;
    dbg_read(G.sock, G.pid, ped + 0x9C, &h, 4);
    if (out_handle) *out_handle = h;
    return ped;
}

/* ---------- Background loop (God Mode / Never Wanted / Freeze Time) ---------- */

static void *bg_thread_fn(void *arg) {
    (void)arg;
    int tick = 0;
    for (;;) {
        usleep(250000);
        tick++;
        pthread_mutex_lock(&G.lock);
        if (!G.connected && (tick % 12 == 0)) {
            if (trainer_connect_locked(G.host, G.port) == 0) {
                dbg_notify("Auto-connected to GTA VI!");
            }
        }
        if (G.connected && G.hook_installed && (G.god_mode || G.never_wanted || G.freeze_time)) {
            if (G.god_mode) {
                uint32_t h = 0;
                uint64_t ped = get_ped_and_handle(&h);
                if (ped && h) {
                    game_call(G.base + OFF_HEALTH_RESTORE, h, 0, 0, 0, 0);
                    uint64_t veh = 0;
                    if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_VEH, &veh, 8) == 0 && veh) {
                        game_call(G.base + OFF_VEHICLE_FIX, veh, 0, 0, 0, 0);
                    }
                }
            }
            if (G.never_wanted && (tick % 4 == 0)) {
                uint64_t ped = get_ped_and_handle(NULL);
                if (ped) game_call(G.base + OFF_SET_PLAYER_WANTED, ped, 0, 1, 1, 1);
            }
            if (G.freeze_time && (tick % 2 == 0)) {
                game_call(G.base + OFF_CLOCK_SET_TIME, (uint64_t)G.frozen_h, (uint64_t)G.frozen_m, 0, 0, 0);
            }
        }
        pthread_mutex_unlock(&G.lock);
    }
    return NULL;
}

/* ---------- Tiny JSON helpers ---------- */

static bool json_get_str(const char *body, const char *key, char *out, size_t out_sz) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(body, pat);
    if (!p) return false;
    p = strchr(p + strlen(pat), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_sz) out[i++] = *p++;
    out[i] = '\0';
    return true;
}

static bool json_get_int(const char *body, const char *key, int *out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(body, pat);
    if (!p) return false;
    p = strchr(p + strlen(pat), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 't') { *out = 1; return true; }
    if (*p == 'f') { *out = 0; return true; }
    *out = atoi(p);
    return true;
}

static void build_status_json(char *buf, size_t sz) {
    int h = -1, m = -1, wanted = -1;
    bool in_veh = false;
    if (G.connected && G.sock >= 0) {
        int32_t clk[6] = {0};
        if (dbg_read(G.sock, G.pid, G.base + OFF_GAME_CLOCK, clk, sizeof(clk)) == 0) {
            h = clk[4]; m = clk[5];
        }
        uint64_t veh = 0;
        if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_VEH, &veh, 8) == 0) in_veh = (veh != 0);
    }
    snprintf(buf, sz,
        "{\"connected\":%s,\"version_ok\":%s,\"pid\":%d,\"titleid\":\"PPSA38500\",\"app_ver\":\"01.002.001\","
        "\"fw\":\"PS5\",\"kstuff\":true,\"etahen\":true,\"hook\":%s,\"in_vehicle\":%s,"
        "\"god_mode\":%s,\"never_wanted\":%s,\"freeze_time\":%s,\"ps5_notify\":%s,"
        "\"frozen_time\":\"%02d:%02d\",\"hour\":%d,\"minute\":%d,\"wanted\":%d,"
        "\"money\":-1,\"money_mode\":\"idle\",\"lektor_enabled\":false,\"lektor_mac_say\":false,"
        "\"lektor_read_speaker\":false,\"lektor_rate\":195,\"subtitle_seq\":0}",
        G.connected ? "true" : "false",
        G.connected ? "true" : "false",
        G.pid,
        G.hook_installed ? "true" : "false",
        in_veh ? "true" : "false",
        G.god_mode ? "true" : "false",
        G.never_wanted ? "true" : "false",
        G.freeze_time ? "true" : "false",
        G.ps5_notify ? "true" : "false",
        G.frozen_h, G.frozen_m, h, m, wanted
    );
}

/* ---------- HTTP Server ---------- */

static void http_send(int fd, int code, const char *ct, const void *body, size_t len) {
    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        code, ct, len);
    send_all(fd, hdr, (size_t)hlen);
    if (len > 0 && body) send_all(fd, body, len);
}

static void handle_client(int cfd) {
    char req[8192];
    ssize_t n = recv(cfd, req, sizeof(req) - 1, 0);
    if (n <= 0) { close(cfd); return; }
    req[n] = '\0';

    char method[16] = {0}, path[256] = {0};
    sscanf(req, "%15s %255s", method, path);
    const char *body = strstr(req, "\r\n\r\n");
    body = body ? body + 4 : "";

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            http_send(cfd, 200, "text/html; charset=utf-8", ASSET_INDEX_HTML, ASSET_INDEX_HTML_len);
        } else if (strcmp(path, "/styles.css") == 0) {
            http_send(cfd, 200, "text/css; charset=utf-8", ASSET_STYLES_CSS, ASSET_STYLES_CSS_len);
        } else if (strcmp(path, "/app.js") == 0) {
            http_send(cfd, 200, "application/javascript; charset=utf-8", ASSET_APP_JS, ASSET_APP_JS_len);
        } else if (strcmp(path, "/manifest.json") == 0) {
            http_send(cfd, 200, "application/manifest+json", ASSET_MANIFEST_JSON, ASSET_MANIFEST_JSON_len);
        } else if (strcmp(path, "/icon.svg") == 0) {
            http_send(cfd, 200, "image/svg+xml", ASSET_ICON_SVG, ASSET_ICON_SVG_len);
        } else if (strcmp(path, "/api/vehicles") == 0) {
            http_send(cfd, 200, "application/json; charset=utf-8", ASSET_VEHICLES_JSON, ASSET_VEHICLES_JSON_len);
        } else if (strcmp(path, "/api/status") == 0) {
            char st[1024];
            pthread_mutex_lock(&G.lock);
            build_status_json(st, sizeof(st));
            pthread_mutex_unlock(&G.lock);
            http_send(cfd, 200, "application/json; charset=utf-8", st, strlen(st));
        } else {
            http_send(cfd, 404, "application/json", "{\"error\":\"Not found\"}", 21);
        }
        close(cfd);
        return;
    }

    char msg[256] = "OK";
    bool ok = true;

    pthread_mutex_lock(&G.lock);
    if (strcmp(path, "/api/connect") == 0) {
        char ip[64] = "127.0.0.1";
        int port = 744;
        json_get_str(body, "ip", ip, sizeof(ip));
        json_get_int(body, "port", &port);
        if (trainer_connect_locked(ip, port) == 0) {
            snprintf(msg, sizeof(msg), "Connected to GTA VI (PID %d)", G.pid);
            dbg_notify("Connected! Ready.");
        } else {
            ok = false;
            snprintf(msg, sizeof(msg), "%s", G.last_error);
        }
    } else if (!G.connected || !G.hook_installed) {
        ok = false;
        snprintf(msg, sizeof(msg), "Not connected to PS5. Click Connect PS5 first.");
    } else if (strcmp(path, "/api/heal") == 0) {
        uint32_t h = 0;
        uint64_t ped = get_ped_and_handle(&h);
        if (ped && h) {
            game_call(G.base + OFF_CHEAT_HEAL_REPAIR, 0, 0, 0, 0, 0);
            game_call(G.base + OFF_HEALTH_RESTORE, h, 0, 0, 0, 0);
            uint64_t veh = 0;
            if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_VEH, &veh, 8) == 0 && veh) {
                game_call(G.base + OFF_VEHICLE_FIX, veh, 0, 0, 0, 0);
            }
            snprintf(msg, sizeof(msg), "Healed + Armor + Repaired");
            dbg_notify(msg);
        }
    } else if (strcmp(path, "/api/heal_only") == 0) {
        uint32_t h = 0;
        if (get_ped_and_handle(&h) && h) game_call(G.base + OFF_HEALTH_RESTORE, h, 0, 0, 0, 0);
        snprintf(msg, sizeof(msg), "Player healed (100%% HP)");
        dbg_notify(msg);
    } else if (strcmp(path, "/api/armor_only") == 0) {
        game_call(G.base + OFF_CHEAT_HEAL_REPAIR, 0, 0, 0, 0, 0);
        snprintf(msg, sizeof(msg), "Full armor added");
        dbg_notify(msg);
    } else if (strcmp(path, "/api/repair_car") == 0) {
        uint64_t veh = 0;
        if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_VEH, &veh, 8) == 0 && veh) {
            game_call(G.base + OFF_VEHICLE_FIX, veh, 0, 0, 0, 0);
            snprintf(msg, sizeof(msg), "Vehicle repaired");
            dbg_notify(msg);
        } else {
            ok = false;
            snprintf(msg, sizeof(msg), "You are not in a vehicle");
        }
    } else if (strcmp(path, "/api/wanted") == 0) {
        int lvl = 0;
        json_get_int(body, "level", &lvl);
        uint64_t ped = get_ped_and_handle(NULL);
        if (ped) game_call(G.base + OFF_SET_PLAYER_WANTED, ped, (uint64_t)lvl, 1, 1, 1);
        snprintf(msg, sizeof(msg), "Wanted Level set to %d", lvl);
        dbg_notify(msg);
    } else if (strcmp(path, "/api/wanted_delta") == 0) {
        int up = 1;
        json_get_int(body, "up", &up);
        game_call(G.base + (up ? OFF_CHEAT_WANTED_UP : OFF_CHEAT_WANTED_DOWN), 0, 0, 0, 0, 0);
        snprintf(msg, sizeof(msg), up ? "Wanted +1" : "Wanted -1");
        dbg_notify(msg);
    } else if (strcmp(path, "/api/weather") == 0) {
        game_call(G.base + OFF_CHEAT_WEATHER, 0, 0, 0, 0, 0);
        snprintf(msg, sizeof(msg), "Weather changed");
        dbg_notify(msg);
    } else if (strcmp(path, "/api/time") == 0) {
        int h = 12, m = 0;
        json_get_int(body, "hour", &h);
        json_get_int(body, "minute", &m);
        game_call(G.base + OFF_CLOCK_SET_TIME, (uint64_t)h, (uint64_t)m, 0, 0, 0);
        G.frozen_h = h; G.frozen_m = m;
        snprintf(msg, sizeof(msg), "Time set to %02d:%02d", h, m);
        dbg_notify(msg);
    } else if (strcmp(path, "/api/spawn") == 0) {
        char vname[64] = {0};
        json_get_str(body, "name", vname, sizeof(vname));
        uint64_t hash = 0;
        for (size_t i = 0; i < VEHICLE_HASHES_COUNT; i++) {
            if (strcasecmp(VEHICLE_HASHES[i].name, vname) == 0) {
                hash = VEHICLE_HASHES[i].hash;
                break;
            }
        }
        if (hash) {
            game_call(G.base + OFF_SPAWN_VEHICLE, hash, 0, 0, 0, 0);
            snprintf(msg, sizeof(msg), "Spawned %s", vname);
            dbg_notify(msg);
        } else {
            ok = false;
            snprintf(msg, sizeof(msg), "Unknown vehicle: %s", vname);
        }
    } else if (strcmp(path, "/api/toggle") == 0) {
        char name[64] = {0};
        int en = 0;
        json_get_str(body, "name", name, sizeof(name));
        json_get_int(body, "enabled", &en);
        if (strcmp(name, "god_mode") == 0) G.god_mode = (en != 0);
        else if (strcmp(name, "never_wanted") == 0) G.never_wanted = (en != 0);
        else if (strcmp(name, "freeze_time") == 0) G.freeze_time = (en != 0);
        else if (strcmp(name, "ps5_notify") == 0) G.ps5_notify = (en != 0);
        snprintf(msg, sizeof(msg), "%s: %s", name, en ? "ON" : "OFF");
        dbg_notify(msg);
    } else {
        ok = false;
        snprintf(msg, sizeof(msg), "Not available in standalone ELF v1 yet");
    }

    char st[1024], resp[1600];
    build_status_json(st, sizeof(st));
    pthread_mutex_unlock(&G.lock);

    snprintf(resp, sizeof(resp), "{\"%s\":\"%s\",\"status\":%s}", ok ? "message" : "error", msg, st);
    http_send(cfd, ok ? 200 : 400, "application/json; charset=utf-8", resp, strlen(resp));
    close(cfd);
}

int main(void) {
    pthread_t bg;
    pthread_create(&bg, NULL, bg_thread_fn, NULL);
    pthread_detach(bg);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) return 1;
    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(HTTP_PORT);

    if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(sfd);
        return 1;
    }
    listen(sfd, 16);
    dbg_notify("Web Trainer ELF started! Port 8080");

    for (;;) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd >= 0) handle_client(cfd);
    }
    return 0;
}
