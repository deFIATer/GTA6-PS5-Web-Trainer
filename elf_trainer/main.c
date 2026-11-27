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
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ps5/kernel.h>

#include "native_patches.h"
#include "tile_assets.h"
#include "vehicles_data.h"
#include "web_assets.h"

#define HTTP_PORT 8080
#define TRAINER_VERSION "ELF v1.2.0"
#define PS5DBG_MAGIC 0xFFAABBCCu
#define CMD_SUCCESS  0x80000000u

#define CMD_PROC_LIST    0xBDAA0001u
#define CMD_PROC_READ    0xBDAA0002u
#define CMD_PROC_WRITE   0xBDAA0003u
#define CMD_PROC_MAPS    0xBDAA0004u
#define CMD_PROC_PROTECT 0xBDAA0008u
#define CMD_PROC_ALLOC   0xBDAA000Bu
#define CMD_PROC_SCAN_START 0xBDAACC01u
#define CMD_PROC_AUTH       0xBDAACCFFu
#define CMD_PROC_AUTH_MAGIC 0xBB40E64Du
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
#define OFF_STREAM_REQUEST     0x1908520ULL
#define OFF_STREAM_READY_A     0x693730ULL
#define OFF_STREAM_READY_B     0x685DF0ULL
#define OFF_STREAM_RELEASE     0x6B9DA0ULL
#define OFF_LOCAL_PLAYER_PED   0xDCDBCB0ULL
#define OFF_LOCAL_PLAYER_VEH   0xDCEBCE0ULL
#define OFF_GAME_CLOCK         0xDCEC6D0ULL
#define OFF_STREAM_CTX         0xCC64B30ULL
#define OFF_ARCHETYPE_MAP      0xE561800ULL
#define OFF_LAST_CHEAT_VEH     0xA753E4CULL
#define OFF_WANTED_LEVEL       0x5EFCULL

typedef struct {
    int seq;
    char speaker[48];
    char text[256];
} SubEntry;

#define SUB_HIST_MAX 16

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
    bool auto_connect;
    bool hook_installed;
    bool god_mode;
    bool never_wanted;
    bool freeze_time;
    bool ps5_notify;
    bool native_patch_on[NATIVE_PATCHES_COUNT];
    bool lektor_enabled;
    bool lektor_read_speaker;
    int lektor_rate;
    int subtitle_seq;
    SubEntry sub_hist[SUB_HIST_MAX];
    int sub_hist_cnt;
    char sub_last_text[256];
    uint64_t money_addrs[32];
    int money_addr_cnt;
    char money_mode[24];
    char money_note[192];
    volatile bool money_cancel;
    uint64_t money_watch_addrs[64];
    int32_t money_watch_vals[64];
    int money_watch_cnt;
    int frozen_h;
    int frozen_m;
    char last_error[256];
} TrainerState;

static TrainerState G = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .sock = -1,
    .host = "127.0.0.1",
    .port = 744,
    .auto_connect = true,
    .ps5_notify = true,
    .lektor_rate = 195,
    .money_mode = "idle",
    .frozen_h = 12,
    .frozen_m = 0,
};

static int g_listen_fd = -1;

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

static int tcp_connect_timeout(const char *ip, int port, int timeout_sec) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct timeval tv = {.tv_sec = timeout_sec, .tv_usec = 0};
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

static int tcp_connect(const char *ip, int port) {
    return tcp_connect_timeout(ip, port, 3);
}

/* ---------- ps5debug-NG protocol ---------- */

static int dbg_send_cmd(int fd, uint32_t cmd, const void *body, uint32_t datalen) {
    uint32_t hdr[3] = {PS5DBG_MAGIC, cmd, datalen};
    if (datalen > 0 && body && datalen <= 128) {
        uint8_t pkt[12 + 128];
        memcpy(pkt, hdr, 12);
        memcpy(pkt + 12, body, datalen);
        return send_all(fd, pkt, 12 + datalen);
    }
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

static int dbg_auth(int fd) {
    uint32_t req[2] = {CMD_PROC_AUTH_MAGIC, 2u};
    if (dbg_send_cmd(fd, CMD_PROC_AUTH, req, 8) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    uint16_t clen = 0;
    if (recv_all(fd, &clen, 2) < 0 || clen != 64) return -1;
    uint8_t chal[64], resp[64];
    if (recv_all(fd, chal, 64) < 0) return -1;
    uint32_t s1 = 200, s2 = 300, s3 = 400, s4 = 500;
    for (int i = 0; i < 64; i++) {
        s1 = ((s1 << 18) & 0xFFF80000u) ^ ((s1 ^ (s1 << 6)) >> 13);
        s2 = ((s2 << 2)  & 0xFFFFFFE0u) ^ ((s2 ^ (s2 << 2)) >> 27);
        s3 = ((s3 << 7)  & 0xFFFFF800u) ^ ((s3 ^ (s3 << 13)) >> 21);
        s4 = ((s4 << 13) & 0xFFF00000u) ^ ((s4 ^ (s4 << 3)) >> 12);
        uint8_t k = (uint8_t)((s1 ^ s2 ^ s3 ^ s4) & 0xFFu);
        resp[i] = chal[i] ^ k;
    }
    if (send_all(fd, resp, 64) < 0) return -1;
    return dbg_expect_ok(fd);
}

static int dbg_scan_u32_align(int fd, int pid, uint64_t addr, uint32_t length, uint8_t align, uint32_t val, uint32_t *out_offs, int max_out) {
    struct __attribute__((packed)) {
        uint32_t pid; uint64_t addr; uint32_t length;
        uint8_t vtype; uint8_t ctype; uint8_t align; uint32_t lenData;
    } req = {(uint32_t)pid, addr, length, 4, 0, align, 4};
    if (dbg_send_cmd(fd, CMD_PROC_SCAN_START, &req, sizeof(req)) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    if (send_all(fd, &val, 4) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    int cnt = 0;
    for (;;) {
        uint64_t blen = 0;
        if (recv_all(fd, &blen, 8) < 0) return -1;
        if (blen == 0xFFFFFFFFFFFFFFFFULL) break;
        uint8_t *blk = (uint8_t *)malloc((size_t)blen);
        if (!blk) return -1;
        if (recv_all(fd, blk, (size_t)blen) < 0) { free(blk); return -1; }
        for (uint64_t i = 0; i + 8 <= blen; i += 8) {
            if (cnt < max_out) {
                uint32_t off = 0;
                memcpy(&off, blk + i, 4);
                out_offs[cnt++] = off;
            }
        }
        free(blk);
    }
    if (dbg_expect_ok(fd) < 0) return -1;
    return cnt;
}

static int dbg_scan_u32(int fd, int pid, uint64_t addr, uint32_t length, uint32_t val, uint32_t *out_offs, int max_out) {
    return dbg_scan_u32_align(fd, pid, addr, length, 1, val, out_offs, max_out);
}

static int dbg_scan_u64_align(int fd, int pid, uint64_t addr, uint32_t length, uint8_t align, uint64_t val, uint32_t *out_offs, int max_out) {
    struct __attribute__((packed)) {
        uint32_t pid; uint64_t addr; uint32_t length;
        uint8_t vtype; uint8_t ctype; uint8_t align; uint32_t lenData;
    } req = {(uint32_t)pid, addr, length, 6, 0, align, 8}; /* 6 = UINT64 */
    if (dbg_send_cmd(fd, CMD_PROC_SCAN_START, &req, sizeof(req)) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    if (send_all(fd, &val, 8) < 0) return -1;
    if (dbg_expect_ok(fd) < 0) return -1;
    int cnt = 0;
    for (;;) {
        uint64_t blen = 0;
        if (recv_all(fd, &blen, 8) < 0) return -1;
        if (blen == 0xFFFFFFFFFFFFFFFFULL) break;
        uint8_t *blk = (uint8_t *)malloc((size_t)blen);
        if (!blk) return -1;
        if (recv_all(fd, blk, (size_t)blen) < 0) { free(blk); return -1; }
        for (uint64_t i = 0; i + 12 <= blen; i += 12) {
            if (cnt < max_out) {
                uint32_t off = 0;
                memcpy(&off, blk + i, 4);
                out_offs[cnt++] = off;
            }
        }
        free(blk);
    }
    if (dbg_expect_ok(fd) < 0) return -1;
    return cnt;
}

static int dbg_scan_u64(int fd, int pid, uint64_t addr, uint32_t length, uint64_t val, uint32_t *out_offs, int max_out) {
    return dbg_scan_u64_align(fd, pid, addr, length, 8, val, out_offs, max_out);
}

static pthread_mutex_t g_notify_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_notify_pending[256] = {0};
static bool g_notify_has_msg = false;

static int dbg_notify(const char *msg) {
    if (!G.ps5_notify || !msg || !msg[0]) return 0;
    pthread_mutex_lock(&g_notify_lock);
    snprintf(g_notify_pending, sizeof(g_notify_pending), "%s", msg);
    g_notify_has_msg = true;
    pthread_mutex_unlock(&g_notify_lock);
    return 0;
}

static void *notify_thread_fn(void *arg) {
    (void)arg;
    for (;;) {
        char msg[256] = {0};
        bool has = false;
        pthread_mutex_lock(&g_notify_lock);
        if (g_notify_has_msg) {
            snprintf(msg, sizeof(msg), "%s", g_notify_pending);
            g_notify_has_msg = false;
            has = true;
        }
        pthread_mutex_unlock(&g_notify_lock);
        if (!has) { usleep(150000); continue; }
        if (!G.ps5_notify) continue;

        int fd = tcp_connect(G.host, G.port);
        if (fd >= 0) {
            char full[288];
            snprintf(full, sizeof(full), "GTA VI Trainer\n%s", msg);
            uint32_t len = (uint32_t)strlen(full) + 1;
            uint32_t body[2] = {222u, len};
            if (dbg_send_cmd(fd, CMD_CONSOLE_NOTIFY, body, 8) == 0 &&
                send_all(fd, full, len) == 0) {
                dbg_expect_ok(fd);
            }
            close(fd);
        }
        /* Pace notifications by 3s so PS5 never stacks a long queue */
        usleep(3000000);
    }
    return NULL;
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
            char name[32]; uint64_t start; uint64_t end; uint64_t offset; uint16_t prot;
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

static void drop_broken_conn_locked(void) {
    if (G.sock >= 0) { close(G.sock); G.sock = -1; }
    G.connected = false;
    G.hook_installed = false;
    G.pid = 0;
}

static uint64_t game_call8(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                           uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8) {
    if (!G.connected || !G.hook_installed || G.sock < 0) return 0;
    /* Ensure hook slot is idle before writing args (prevents main-thread race / crash) */
    uint32_t cur_st = 0;
    if (dbg_read(G.sock, G.pid, G.ctrl_addr + 0x08, &cur_st, 4) < 0) {
        drop_broken_conn_locked();
        return 0;
    }
    if (cur_st != 0) return 0;

    uint64_t args[9] = {fn, a1, a2, a3, a4, a5, a6, a7, a8};
    if (dbg_write(G.sock, G.pid, G.ctrl_addr + 0x10, args, sizeof(args)) < 0) {
        drop_broken_conn_locked();
        return 0;
    }
    uint32_t one = 1;
    if (dbg_write(G.sock, G.pid, G.ctrl_addr + 0x08, &one, 4) < 0) {
        drop_broken_conn_locked();
        return 0;
    }
    for (int i = 0; i < 150; i++) {
        uint32_t st = 0;
        if (dbg_read(G.sock, G.pid, G.ctrl_addr + 0x08, &st, 4) < 0) {
            drop_broken_conn_locked();
            return 0;
        }
        if (st == 3) {
            uint64_t res = 0;
            dbg_read(G.sock, G.pid, G.ctrl_addr + 0x58, &res, 8);
            uint32_t zero = 0;
            dbg_write(G.sock, G.pid, G.ctrl_addr + 0x08, &zero, 4);
            return res;
        }
        usleep(10000);
    }
    /* Timed out waiting for main thread — reset state to 0 if still pending */
    uint32_t zero = 0;
    dbg_write(G.sock, G.pid, G.ctrl_addr + 0x08, &zero, 4);
    return 0;
}

static uint64_t game_call(uint64_t fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    return game_call8(fn, a1, a2, a3, a4, a5, 0, 0, 0);
}

static bool lookup_model_id(uint64_t model_hash, uint32_t *out_val) {
    uint64_t amap = G.base + OFF_ARCHETYPE_MAP;
    uint32_t buckets = 0;
    if (dbg_read(G.sock, G.pid, amap + 4, &buckets, 4) < 0 || buckets == 0) return false;
    uint64_t b = amap + 8 + (model_hash % (uint64_t)buckets) * 0x58ULL;
    uint8_t bd[0x58];
    if (dbg_read(G.sock, G.pid, b, bd, sizeof(bd)) < 0) return false;

    uint32_t n = 0, ex = 0;
    uint16_t cnt = 0;
    uint64_t keys_ptr = 0, vals_ptr = 0, exl_ptr = 0, arr_ptr = 0;
    memcpy(&n, bd + 4, 4);
    memcpy(&keys_ptr, bd + 8, 8);
    memcpy(&vals_ptr, bd + 0x18, 8);
    memcpy(&ex, bd + 0x24, 4);
    memcpy(&exl_ptr, bd + 0x28, 8);
    memcpy(&arr_ptr, bd + 0x48, 8);
    memcpy(&cnt, bd + 0x50, 2);

    bool found = false;
    uint32_t val = 0;
    if (n && keys_ptr && vals_ptr && n <= 65536) {
        uint64_t *keys = (uint64_t *)malloc(8ULL * n);
        uint32_t *vals = (uint32_t *)malloc(4ULL * n);
        if (keys && vals &&
            dbg_read(G.sock, G.pid, keys_ptr, keys, 8u * n) == 0 &&
            dbg_read(G.sock, G.pid, vals_ptr, vals, 4u * n) == 0) {
            for (uint32_t i = 0; i < n; i++) {
                if (keys[i] == model_hash) { val = vals[i]; found = true; break; }
            }
        }
        free(keys); free(vals);
    }
    if (!found && arr_ptr && cnt) {
        uint8_t *arr = (uint8_t *)malloc(16ULL * cnt);
        if (arr && dbg_read(G.sock, G.pid, arr_ptr, arr, 16u * cnt) == 0) {
            for (uint32_t i = 0; i < cnt; i++) {
                uint64_t k = 0;
                memcpy(&k, arr + 16ULL * i, 8);
                if (k == model_hash) { memcpy(&val, arr + 16ULL * i + 8, 4); found = true; break; }
            }
        }
        free(arr);
    }
    if (!found) return false;
    if (ex && exl_ptr && ex <= 65536) {
        uint32_t *exl = (uint32_t *)malloc(4ULL * ex);
        if (exl && dbg_read(G.sock, G.pid, exl_ptr, exl, 4u * ex) == 0) {
            for (uint32_t i = 0; i < ex; i++) {
                if (((val ^ exl[i]) & 0x3FFFFFu) == 0) { free(exl); return false; }
            }
        }
        free(exl);
    }
    *out_val = val;
    return true;
}

static bool spawn_vehicle_streamed(uint64_t hash, char *err_buf, size_t err_sz) {
    /* 1. Verify player ped exists (never spawn in menus / loading screens) */
    uint64_t ped = 0;
    if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_PED, &ped, 8) < 0) {
        drop_broken_conn_locked();
        snprintf(err_buf, err_sz, "Lost connection to PS5");
        return false;
    }
    if (!ped) {
        snprintf(err_buf, err_sz, "Enter Story Mode first (no player ped)");
        return false;
    }
    /* 2. Verify main-thread hook is actively ticking */
    uint32_t t1 = 0, t2 = 0;
    if (dbg_read(G.sock, G.pid, G.ctrl_addr + 0x0C, &t1, 4) < 0) {
        drop_broken_conn_locked();
        snprintf(err_buf, err_sz, "Lost connection to PS5");
        return false;
    }
    usleep(120000);
    if (dbg_read(G.sock, G.pid, G.ctrl_addr + 0x0C, &t2, 4) < 0 || t2 == t1) {
        snprintf(err_buf, err_sz, "Game main thread paused (unpause game first)");
        return false;
    }

    uint32_t model_id = 0;
    if (!lookup_model_id(hash, &model_id)) {
        snprintf(err_buf, err_sz, "Model not found in ArchetypeMap");
        return false;
    }
    uint64_t req_ptr = G.ctrl_addr + 0x100;
    uint64_t model_ptr = G.ctrl_addr + 0x110;
    uint64_t aux_ptr = G.ctrl_addr + 0x120;
    uint8_t zeros[16] = {0};
    dbg_write(G.sock, G.pid, req_ptr, zeros, 4);
    dbg_write(G.sock, G.pid, model_ptr, &model_id, 4);
    dbg_write(G.sock, G.pid, aux_ptr, zeros, 16);

    game_call8(G.base + OFF_STREAM_REQUEST, req_ptr, model_ptr, G.base + OFF_STREAM_CTX,
               0, 9, 1, 0, aux_ptr);

    bool loaded = false;
    for (int i = 0; i < 50; i++) {
        if (!G.connected) break;
        uint16_t req_val = 0;
        if (dbg_read(G.sock, G.pid, req_ptr, &req_val, 2) == 0 && (req_val & 0x3FFFu)) {
            uint64_t a = game_call(G.base + OFF_STREAM_READY_A, req_ptr, 0, 0, 0, 0) & 0xFF;
            uint64_t b = game_call(G.base + OFF_STREAM_READY_B, req_ptr, 0, 0, 0, 0) & 0xFF;
            if (a && b) { loaded = true; break; }
        }
        usleep(80000);
    }

    if (loaded && G.connected) {
        uint32_t clear_handle = 0x1FFFFu;
        dbg_write(G.sock, G.pid, G.base + OFF_LAST_CHEAT_VEH, &clear_handle, 4);
        game_call(G.base + OFF_SPAWN_VEHICLE, hash, 0, 0, 0, 0);
    } else if (!loaded) {
        snprintf(err_buf, err_sz, "Model stream timeout (not available here)");
    }

    if (G.connected) {
        uint16_t req_val = 0;
        if (dbg_read(G.sock, G.pid, req_ptr, &req_val, 2) == 0 && (req_val & 0x3FFFu)) {
            game_call(G.base + OFF_STREAM_RELEASE, req_ptr, 0, 0, 0, 0);
        }
    }
    return loaded;
}

static void trainer_disconnect_locked(void) {
    if (G.sock >= 0 && G.hook_installed && G.orig_pad != 0 && G.pid > 0 && G.base != 0) {
        dbg_write(G.sock, G.pid, G.base + OFF_GOT_PAD_READ_STATE, &G.orig_pad, 8);
    }
    if (G.sock >= 0) {
        close(G.sock);
        G.sock = -1;
    }
    G.connected = false;
    G.hook_installed = false;
    G.pid = 0;
    G.god_mode = false;
    G.never_wanted = false;
    G.freeze_time = false;
    G.lektor_enabled = false;
}

static int trainer_connect_locked(const char *ip, int port) {
    trainer_disconnect_locked();
    G.auto_connect = true;
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

/* ---------- Money Scanner (Dedicated TCP Channel in C) ---------- */

static int money_scan_stats_c(const char *host, int port, int pid, uint64_t *out_addrs, int32_t *out_vals, int max_out) {
    int fd = tcp_connect_timeout(host, port, 30);
    if (fd < 0) return -1;
    if (dbg_auth(fd) < 0) { close(fd); return -1; }
    uint32_t req = (uint32_t)pid;
    if (dbg_send_cmd(fd, CMD_PROC_MAPS, &req, 4) < 0 || dbg_expect_ok(fd) < 0) { close(fd); return -1; }
    uint32_t mcnt = 0;
    if (recv_all(fd, &mcnt, 4) < 0 || mcnt > 32768) { close(fd); return -1; }
    typedef struct { uint64_t s, e; } Range;
    Range ranges[64];
    int rcnt = 0;
    for (uint32_t i = 0; i < mcnt; i++) {
        struct __attribute__((packed)) { char name[32]; uint64_t s, e, off; uint16_t prot; } m;
        if (recv_all(fd, &m, sizeof(m)) < 0) { close(fd); return -1; }
        if ((m.prot & 3) == 3 && !(m.prot & 4) && m.name[0] == '\0' &&
            m.s >= 0x1000000000ULL && m.s < 0x1060000000ULL && (m.e - m.s) >= 0x100000ULL && rcnt < 64) {
            ranges[rcnt].s = m.s; ranges[rcnt].e = m.e; rcnt++;
        }
    }
    int found = 0;
    static const uint64_t STAT_HDR = 0x000000011E720953ULL;
    for (int r = 0; r < rcnt && !G.money_cancel; r++) {
        uint64_t addr = ranges[r].s;
        while (addr < ranges[r].e && !G.money_cancel) {
            uint32_t len = (uint32_t)((ranges[r].e - addr > 0x10000000ULL) ? 0x10000000ULL : (ranges[r].e - addr));
            uint32_t offs[128];
            int n = dbg_scan_u64(fd, pid, addr, len, STAT_HDR, offs, 128);
            if (n < 0) break;
            for (int i = 0; i < n && found < max_out && !G.money_cancel; i++) {
                uint64_t va = addr + offs[i] + 8;
                int32_t v = 0;
                if (dbg_read(fd, pid, va, &v, 4) == 0 && v >= 0 && v <= 2000000000) {
                    out_addrs[found] = va;
                    out_vals[found] = v;
                    found++;
                }
            }
            addr += len;
        }
    }
    close(fd);
    return found;
}

static int money_scan_hud_c(const char *host, int port, int pid, int32_t amount, uint64_t *out_addrs, int max_out) {
    int fd = tcp_connect_timeout(host, port, 30);
    if (fd < 0) return -1;
    if (dbg_auth(fd) < 0) { close(fd); return -1; }
    uint32_t req = (uint32_t)pid;
    if (dbg_send_cmd(fd, CMD_PROC_MAPS, &req, 4) < 0 || dbg_expect_ok(fd) < 0) { close(fd); return -1; }
    uint32_t mcnt = 0;
    if (recv_all(fd, &mcnt, 4) < 0 || mcnt > 32768) { close(fd); return -1; }
    typedef struct { uint64_t s, e; } Range;
    Range ranges[64];
    int rcnt = 0;
    for (uint32_t i = 0; i < mcnt; i++) {
        struct __attribute__((packed)) { char name[32]; uint64_t s, e, off; uint16_t prot; } m;
        if (recv_all(fd, &m, sizeof(m)) < 0) { close(fd); return -1; }
        if ((m.prot & 3) == 3 && !(m.prot & 4) && m.name[0] == '\0' &&
            m.s >= 0x1100000000ULL && m.s < 0x1200000000ULL && (m.e - m.s) >= 0x100000ULL && rcnt < 64) {
            ranges[rcnt].s = m.s; ranges[rcnt].e = m.e; rcnt++;
        }
    }
    int found = 0;
    uint32_t *offs = (uint32_t *)malloc(4096 * sizeof(uint32_t));
    if (!offs) { close(fd); return -1; }
    uint64_t pat = 0x00010000ULL | (((uint64_t)(uint32_t)amount) << 32);
    for (int r = 0; r < rcnt && !G.money_cancel; r++) {
        uint64_t addr = ranges[r].s;
        while (addr < ranges[r].e && !G.money_cancel) {
            uint32_t len = (uint32_t)((ranges[r].e - addr > 0x08000000ULL) ? 0x08000000ULL : (ranges[r].e - addr));
            int n = dbg_scan_u64(fd, pid, addr, len, pat, offs, 4096);
            if (n < 0) break;
            for (int i = 0; i < n && found + 1 < max_out && !G.money_cancel; i++) {
                uint64_t a = addr + offs[i];
                uint32_t ctx[6];
                if (dbg_read(fd, pid, a, ctx, sizeof(ctx)) == 0) {
                    if (ctx[0] == 0x00010000u && (int32_t)ctx[1] == amount && (int32_t)ctx[2] == amount &&
                        ctx[4] == 5u && ctx[5] == 10u) {
                        out_addrs[found++] = a + 4;
                        out_addrs[found++] = a + 8;
                    }
                }
            }
            addr += len;
        }
    }
    free(offs);
    close(fd);
    return found;
}

static int money_scan_master_c(const char *host, int port, int pid, int32_t amount, uint64_t *out_addrs, int max_out) {
    int fd = tcp_connect_timeout(host, port, 30);
    if (fd < 0) return -1;
    if (dbg_auth(fd) < 0) { close(fd); return -1; }
    uint32_t req = (uint32_t)pid;
    if (dbg_send_cmd(fd, CMD_PROC_MAPS, &req, 4) < 0 || dbg_expect_ok(fd) < 0) { close(fd); return -1; }
    uint32_t mcnt = 0;
    if (recv_all(fd, &mcnt, 4) < 0 || mcnt > 32768) { close(fd); return -1; }
    typedef struct { uint64_t s, e; int kind; } MRange;
    MRange ranges[64];
    int rcnt = 0;
    for (uint32_t i = 0; i < mcnt; i++) {
        struct __attribute__((packed)) { char name[32]; uint64_t s, e, off; uint16_t prot; } m;
        if (recv_all(fd, &m, sizeof(m)) < 0) { close(fd); return -1; }
        if ((m.prot & 3) != 3 || (m.prot & 4) || m.name[0] != '\0' || (m.e - m.s) < 0x10000ULL || rcnt >= 64) continue;
        int kind = 0;
        if (m.s >= 0xF000000000ULL && m.s < 0xF100000000ULL) kind = 1;
        else if (m.s >= 0xFA00000000ULL && m.s < 0xFB00000000ULL) kind = 2;
        else if (m.s >= 0x1100000000ULL && m.s < 0x1200000000ULL) kind = 3;
        if (kind) { ranges[rcnt].s = m.s; ranges[rcnt].e = m.e; ranges[rcnt].kind = kind; rcnt++; }
    }
    int found = 0;
    uint32_t *offs = (uint32_t *)malloc(4096 * sizeof(uint32_t));
    if (!offs) { close(fd); return -1; }
    uint64_t pat_f0 = 0x00000804ULL | (((uint64_t)(uint32_t)amount) << 32);
    static const uint64_t CASH_ITEM_TAG = 0x6F0A8D016FC2266BULL;
    /* Pass order: kind 1 (0xf0 RAGEVM) -> kind 3 (0x11 unique cash item) -> kind 2 (0xfa script) */
    static const int pass_order[3] = {1, 3, 2};
    for (int p = 0; p < 3 && !G.money_cancel; p++) {
      int want_kind = pass_order[p];
      for (int r = 0; r < rcnt && !G.money_cancel; r++) {
        if (ranges[r].kind != want_kind) continue;
        uint64_t addr = ranges[r].s;
        while (addr < ranges[r].e && !G.money_cancel) {
            uint32_t len = (uint32_t)((ranges[r].e - addr > 0x08000000ULL) ? 0x08000000ULL : (ranges[r].e - addr));
            if (ranges[r].kind == 1) {
                /* 0xf0.. RAGEVM: scan UINT64 [0x00000804, amount] (align 4) -> ~8 hits in 0.04s! */
                int n = dbg_scan_u64_align(fd, pid, addr, len, 4, pat_f0, offs, 4096);
                if (n < 0) break;
                for (int i = 0; i < n && found < max_out && !G.money_cancel; i++) {
                    uint64_t a = addr + offs[i];
                    uint32_t ctx[4];
                    if (dbg_read(fd, pid, a - 4, ctx, sizeof(ctx)) == 0) {
                        if ((ctx[0] & 0x00FFFFFFu) == 0x00AF2E50u && ctx[1] == 0x00000804u && (int32_t)ctx[2] == amount) {
                            out_addrs[found++] = a + 4;
                        }
                    }
                }
            } else if (ranges[r].kind == 3) {
                /* 0x11.. authoritative cash inventory item: unique 64-bit tag 0x6F0A8D016FC2266B at +16 (0 false hits!) */
                int n = dbg_scan_u64(fd, pid, addr, len, CASH_ITEM_TAG, offs, 4096);
                if (n < 0) break;
                for (int i = 0; i < n && found < max_out && !G.money_cancel; i++) {
                    uint64_t a = addr + offs[i] - 16;
                    struct __attribute__((packed)) { int32_t v; uint32_t z; uint64_t flag; } c;
                    if (dbg_read(fd, pid, a, &c, sizeof(c)) == 0 &&
                        c.v == amount && c.z == 0 && c.flag == 0x8000000000000000ULL) {
                        out_addrs[found++] = a;
                    }
                }
            } else {
                /* 0xfa.. script wallet: scan tag 0x6F0A8D01 (align 4) */
                int n = dbg_scan_u32_align(fd, pid, addr, len, 4, 0x6F0A8D01u, offs, 4096);
                if (n < 0) break;
                for (int i = 0; i < n && found < max_out && !G.money_cancel; i++) {
                    uint64_t a = addr + offs[i] + 12;
                    int32_t v = 0;
                    if (dbg_read(fd, pid, a, &v, 4) == 0 && v == amount) {
                        out_addrs[found++] = a;
                    }
                }
            }
            addr += len;
        }
      }
    }
    free(offs);
    close(fd);
    return found;
}

typedef struct {
    char host[64];
    int port;
    int pid;
    int32_t amount;
    bool has_filter;
    uint64_t stat_addrs[16];
    int stat_cnt;
} MoneyTask;

static void *money_manual_worker(void *arg) {
    MoneyTask *mt = (MoneyTask *)arg;
    uint64_t found[32];
    int cnt = 0;
    int mcnt = money_scan_master_c(mt->host, mt->port, mt->pid, mt->amount, found, 12);
    if (mcnt > 0) cnt += mcnt;
    if (!G.money_cancel) {
        int hcnt = money_scan_hud_c(mt->host, mt->port, mt->pid, mt->amount, found + cnt, 28 - cnt);
        if (hcnt > 0) cnt += hcnt;
    }
    if (!G.money_cancel) {
        uint64_t saddrs[64]; int32_t svals[64];
        int scnt = money_scan_stats_c(mt->host, mt->port, mt->pid, saddrs, svals, 64);
        for (int i = 0; i < scnt && cnt < 32; i++) {
            if (svals[i] == mt->amount) found[cnt++] = saddrs[i];
        }
    }
    pthread_mutex_lock(&G.lock);
    if (!G.money_cancel && G.pid == mt->pid && strcmp(G.money_mode, "manual_scanning") == 0) {
        if (cnt > 0) {
            memcpy(G.money_addrs, found, (size_t)cnt * sizeof(uint64_t));
            G.money_addr_cnt = cnt;
            snprintf(G.money_mode, sizeof(G.money_mode), "locked");
            snprintf(G.money_note, sizeof(G.money_note), "Wallet locked ($%d, %d addresses)!", mt->amount, cnt);
            dbg_notify(G.money_note);
        } else {
            snprintf(G.money_mode, sizeof(G.money_mode), "idle");
            snprintf(G.money_note, sizeof(G.money_note), "Amount $%d not found. Try Auto-Detect.", mt->amount);
        }
    }
    pthread_mutex_unlock(&G.lock);
    free(mt);
    return NULL;
}

static void *money_auto_worker(void *arg) {
    MoneyTask *mt = (MoneyTask *)arg;
    uint64_t saddrs[64]; int32_t svals[64];
    int scnt = money_scan_stats_c(mt->host, mt->port, mt->pid, saddrs, svals, 64);
    pthread_mutex_lock(&G.lock);
    if (!G.money_cancel && G.pid == mt->pid && strcmp(G.money_mode, "auto_scanning") == 0 && scnt > 0) {
        int wcnt = 0;
        if (mt->has_filter) {
            for (int i = 0; i < scnt && wcnt < 64; i++) {
                if (svals[i] == mt->amount) {
                    G.money_watch_addrs[wcnt] = saddrs[i];
                    G.money_watch_vals[wcnt] = svals[i];
                    wcnt++;
                }
            }
        }
        if (wcnt == 0) {
            wcnt = scnt > 64 ? 64 : scnt;
            for (int i = 0; i < wcnt; i++) {
                G.money_watch_addrs[i] = saddrs[i];
                G.money_watch_vals[i] = svals[i];
            }
        }
        G.money_watch_cnt = wcnt;
        snprintf(G.money_mode, sizeof(G.money_mode), "auto_watching");
        snprintf(G.money_note, sizeof(G.money_note),
                 "AUTO active (%d stats) — earn or spend $1 in game to capture!", wcnt);
    } else if (strcmp(G.money_mode, "auto_scanning") == 0) {
        snprintf(G.money_mode, sizeof(G.money_mode), "idle");
        snprintf(G.money_note, sizeof(G.money_note), "Auto scan failed or cancelled.");
    }
    pthread_mutex_unlock(&G.lock);
    free(mt);
    return NULL;
}

static void *money_enrich_hud_worker(void *arg) {
    MoneyTask *mt = (MoneyTask *)arg;
    uint64_t extra[24];
    int ecnt = 0;
    int mcnt = money_scan_master_c(mt->host, mt->port, mt->pid, mt->amount, extra, 12);
    if (mcnt > 0) ecnt += mcnt;
    if (!G.money_cancel) {
        int hcnt = money_scan_hud_c(mt->host, mt->port, mt->pid, mt->amount, extra + ecnt, 24 - ecnt);
        if (hcnt > 0) ecnt += hcnt;
    }
    pthread_mutex_lock(&G.lock);
    if (!G.money_cancel && G.pid == mt->pid && strcmp(G.money_mode, "auto_scanning") == 0) {
        uint64_t merged[32]; int tot = 0;
        for (int i = 0; i < ecnt && tot < 32; i++) merged[tot++] = extra[i];
        for (int i = 0; i < mt->stat_cnt && tot < 32; i++) {
            bool dup = false;
            for (int j = 0; j < tot; j++) { if (merged[j] == mt->stat_addrs[i]) { dup = true; break; } }
            if (!dup) merged[tot++] = mt->stat_addrs[i];
        }
        memcpy(G.money_addrs, merged, (size_t)tot * sizeof(uint64_t));
        G.money_addr_cnt = tot;
        snprintf(G.money_mode, sizeof(G.money_mode), "locked");
        if (ecnt > 0) {
            snprintf(G.money_note, sizeof(G.money_note), "Wallet ready ($%d, %d addrs: Master+HUD+Stats)!", mt->amount, tot);
        } else {
            snprintf(G.money_note, sizeof(G.money_note), "Wallet locked ($%d, %d stats — try Manual if HUD unchanged)", mt->amount, tot);
        }
        dbg_notify(G.money_note);
    }
    pthread_mutex_unlock(&G.lock);
    free(mt);
    return NULL;
}

/* ---------- Background loop (God Mode / Never Wanted / Freeze Time / Auto Money) ---------- */

static void *bg_thread_fn(void *arg) {
    (void)arg;
    int tick = 0;
    for (;;) {
        usleep(250000);
        tick++;
        pthread_mutex_lock(&G.lock);
        if (!G.connected && G.auto_connect && (tick % 12 == 0)) {
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
        if (G.connected && strcmp(G.money_mode, "auto_watching") == 0 && G.money_watch_cnt > 0 && (tick % 6 == 0)) {
            uint64_t ch_addrs[32]; int32_t ch_val = -1; int ch_cnt = 0;
            for (int i = 0; i < G.money_watch_cnt; i++) {
                int32_t v = 0;
                if (dbg_read(G.sock, G.pid, G.money_watch_addrs[i], &v, 4) == 0 &&
                    v != G.money_watch_vals[i] && v >= 0 && v <= 2000000000) {
                    if (ch_cnt == 0) ch_val = v;
                    if (v == ch_val && ch_cnt < 32) ch_addrs[ch_cnt++] = G.money_watch_addrs[i];
                }
            }
            if (ch_cnt > 0) {
                snprintf(G.money_mode, sizeof(G.money_mode), "auto_scanning");
                snprintf(G.money_note, sizeof(G.money_note), "Detected $%d! Locking Master wallet & HUD (~8s)...", ch_val);
                MoneyTask *mt = (MoneyTask *)calloc(1, sizeof(MoneyTask));
                if (mt) {
                    snprintf(mt->host, sizeof(mt->host), "%s", G.host);
                    mt->port = G.port; mt->pid = G.pid; mt->amount = ch_val;
                    mt->stat_cnt = ch_cnt > 16 ? 16 : ch_cnt;
                    memcpy(mt->stat_addrs, ch_addrs, (size_t)mt->stat_cnt * sizeof(uint64_t));
                    pthread_t th;
                    pthread_create(&th, NULL, money_enrich_hud_worker, mt);
                    pthread_detach(th);
                }
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

/* ---------- Dedicated Live Subtitle Scanner Thread (:~s~) ---------- */

static bool parse_subtitle_buf(const uint8_t *raw, size_t len, size_t mpos, char *out_spk, size_t spk_sz, char *out_txt, size_t txt_sz) {
    size_t start = mpos;
    while (start > 0 && raw[start - 1] >= 0x20 && raw[start - 1] != 0x7F) start--;
    size_t end = mpos;
    while (end < len && raw[end] != 0) end++;
    if (end <= mpos + 4) return false;

    /* Speaker: everything after last '~' before ':~s~' */
    size_t spk_start = start;
    for (size_t i = start; i < mpos; i++) {
        if (raw[i] == '~') spk_start = i + 1;
    }
    size_t si = 0;
    for (size_t i = spk_start; i < mpos && si + 1 < spk_sz; i++) {
        if (raw[i] >= 0x20) out_spk[si++] = (char)raw[i];
    }
    while (si > 0 && out_spk[si - 1] == ' ') si--;
    out_spk[si] = '\0';

    /* Dialogue: strip ~...~ tags after ':~s~' */
    size_t di = 0;
    bool in_tag = false;
    for (size_t i = mpos + 4; i < end && di + 1 < txt_sz; i++) {
        uint8_t ch = raw[i];
        if (ch == '~') { in_tag = !in_tag; continue; }
        if (in_tag) continue;
        if (ch == '"' || ch == '\\') ch = '\'';
        if (ch == '\r' || ch == '\n') ch = ' ';
        if (ch < 0x20) continue;
        if (ch == ' ' && (di == 0 || out_txt[di - 1] == ' ')) continue;
        out_txt[di++] = (char)ch;
    }
    while (di > 0 && out_txt[di - 1] == ' ') di--;
    out_txt[di] = '\0';
    return di >= 2;
}

static void *subtitle_thread_fn(void *arg) {
    (void)arg;
    int sfd = -1;
    int last_pid = 0;
    const uint32_t marker = 0x7E737E3Au; /* ":~s~" little-endian */
    for (;;) {
        bool enabled = false;
        int pid = 0;
        char host[64];
        int port = 744;
        pthread_mutex_lock(&G.lock);
        enabled = G.lektor_enabled && G.connected && G.pid > 0;
        pid = G.pid;
        snprintf(host, sizeof(host), "%s", G.host);
        port = G.port;
        pthread_mutex_unlock(&G.lock);

        if (!enabled) {
            if (sfd >= 0) { close(sfd); sfd = -1; }
            usleep(400000);
            continue;
        }
        if (sfd < 0 || last_pid != pid) {
            if (sfd >= 0) close(sfd);
            sfd = tcp_connect(host, port);
            if (sfd < 0 || dbg_auth(sfd) < 0) {
                if (sfd >= 0) { close(sfd); sfd = -1; }
                usleep(1500000);
                continue;
            }
            last_pid = pid;
        }
        uint32_t offs[16];
        int n = dbg_scan_u32(sfd, pid, 0x1030000000ULL, 0x10000000u, marker, offs, 16);
        if (n < 0) {
            close(sfd); sfd = -1;
            usleep(1000000);
            continue;
        }
        for (int i = 0; i < n; i++) {
            uint64_t a = 0x1030000000ULL + offs[i];
            uint8_t raw[384];
            if (dbg_read(sfd, pid, a - 64, raw, sizeof(raw)) < 0) continue;
            char spk[48], txt[256];
            if (!parse_subtitle_buf(raw, sizeof(raw), 64, spk, sizeof(spk), txt, sizeof(txt))) continue;
            pthread_mutex_lock(&G.lock);
            if (strcmp(txt, G.sub_last_text) != 0) {
                snprintf(G.sub_last_text, sizeof(G.sub_last_text), "%s", txt);
                G.subtitle_seq++;
                if (G.sub_hist_cnt < SUB_HIST_MAX) G.sub_hist_cnt++;
                for (int k = G.sub_hist_cnt - 1; k > 0; k--) G.sub_hist[k] = G.sub_hist[k - 1];
                G.sub_hist[0].seq = G.subtitle_seq;
                snprintf(G.sub_hist[0].speaker, sizeof(G.sub_hist[0].speaker), "%s", spk);
                snprintf(G.sub_hist[0].text, sizeof(G.sub_hist[0].text), "%s", txt);
            }
            pthread_mutex_unlock(&G.lock);
            break;
        }
        usleep(350000);
    }
    return NULL;
}

static void build_status_json(char *buf, size_t sz) {
    int h = -1, m = -1, wanted = -1, money_val = -1;
    bool in_veh = false;
    if (G.connected && G.sock >= 0) {
        int32_t clk[6] = {0};
        if (dbg_read(G.sock, G.pid, G.base + OFF_GAME_CLOCK, clk, sizeof(clk)) == 0) {
            h = clk[4]; m = clk[5];
        }
        uint64_t veh = 0;
        if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_VEH, &veh, 8) == 0) in_veh = (veh != 0);
        if (G.money_addr_cnt > 0) {
            int32_t mv = -1;
            if (dbg_read(G.sock, G.pid, G.money_addrs[0], &mv, 4) == 0 && mv >= 0) money_val = mv;
        }
    }
    int pos = snprintf(buf, sz,
        "{\"connected\":%s,\"version_ok\":%s,\"pid\":%d,\"titleid\":\"PPSA38500\",\"app_ver\":\"01.002.001\","
        "\"trainer_version\":\"%s\",\"is_elf\":true,"
        "\"fw\":\"PS5\",\"kstuff\":true,\"etahen\":true,\"hook\":%s,\"in_vehicle\":%s,"
        "\"god_mode\":%s,\"never_wanted\":%s,\"freeze_time\":%s,\"ps5_notify\":%s,"
        "\"unlimited_health\":%s,\"unlimited_stamina\":%s,\"infinite_ammo\":%s,"
        "\"max_relationship\":%s,\"max_criminal_profile\":%s,"
        "\"frozen_time\":\"%02d:%02d\",\"hour\":%d,\"minute\":%d,\"wanted\":%d,"
        "\"money\":%d,\"money_mode\":\"%s\",\"money_watch_note\":\"%s\",\"lektor_enabled\":%s,\"lektor_mac_say\":false,"
        "\"lektor_read_speaker\":%s,\"lektor_rate\":%d,\"subtitle_seq\":%d",
        G.connected ? "true" : "false",
        G.connected ? "true" : "false",
        G.pid,
        TRAINER_VERSION,
        G.hook_installed ? "true" : "false",
        in_veh ? "true" : "false",
        G.god_mode ? "true" : "false",
        G.never_wanted ? "true" : "false",
        G.freeze_time ? "true" : "false",
        G.ps5_notify ? "true" : "false",
        G.native_patch_on[0] ? "true" : "false",
        G.native_patch_on[1] ? "true" : "false",
        G.native_patch_on[2] ? "true" : "false",
        G.native_patch_on[3] ? "true" : "false",
        G.native_patch_on[4] ? "true" : "false",
        G.frozen_h, G.frozen_m, h, m, wanted,
        money_val, G.money_mode, G.money_note,
        G.lektor_enabled ? "true" : "false",
        G.lektor_read_speaker ? "true" : "false",
        G.lektor_rate,
        G.subtitle_seq
    );
    if (G.sub_hist_cnt > 0 && pos > 0 && (size_t)pos + 512 < sz) {
        pos += snprintf(buf + pos, sz - (size_t)pos,
            ",\"subtitle_current\":{\"seq\":%d,\"time\":\"live\",\"speaker\":\"%s\",\"text\":\"%s\"},\"subtitle_history\":[",
            G.sub_hist[0].seq, G.sub_hist[0].speaker, G.sub_hist[0].text);
        for (int i = 0; i < G.sub_hist_cnt && (size_t)pos + 320 < sz; i++) {
            pos += snprintf(buf + pos, sz - (size_t)pos, "%s{\"seq\":%d,\"time\":\"live\",\"speaker\":\"%s\",\"text\":\"%s\"}",
                i ? "," : "", G.sub_hist[i].seq, G.sub_hist[i].speaker, G.sub_hist[i].text);
        }
        pos += snprintf(buf + pos, sz - (size_t)pos, "]");
    }
    snprintf(buf + pos, sz - (size_t)pos, "}");
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

/* ---- waypoint (map marker) teleport ------------------------------------- */
#define OFF_WAYPOINT 0xAF93FC0ULL   /* x, y, z, present flag (float each) */

static bool read_waypoint_pos(float *x, float *y, float *z, bool *present) {
    float w[4] = {0, 0, 0, 0};
    if (!G.connected || G.pid <= 0 || G.sock < 0) return false;
    if (dbg_read(G.sock, G.pid, G.base + OFF_WAYPOINT, w, 16) < 0) return false;
    *x = w[0]; *y = w[1]; *z = w[2];
    if (present) *present = (w[3] > 0.5f);
    return true;
}

/* Teleport the player (or the vehicle being driven) to the current map waypoint.
 * Entity SetMatrix vtable slot 0x178, fresh per-call matrix buffer, +4 m z offset
 * (verified: lower offsets spawn under the ground). */
static bool teleport_to_waypoint(char *err, size_t errsz) {
    float wx = 0, wy = 0, wz = 0;
    bool present = false;
    if (!read_waypoint_pos(&wx, &wy, &wz, &present)) {
        snprintf(err, errsz, "cannot read waypoint");
        return false;
    }
    if (!present || (fabsf(wx) < 1.0f && fabsf(wy) < 1.0f)) {
        snprintf(err, errsz, "no waypoint on the map");
        return false;
    }
    uint64_t ped = 0;
    if (dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_PED, &ped, 8) < 0 || !ped) {
        snprintf(err, errsz, "enter Story Mode first (no player ped)");
        return false;
    }
    uint64_t veh = 0;
    (void)dbg_read(G.sock, G.pid, G.base + OFF_LOCAL_PLAYER_VEH, &veh, 8);
    uint64_t ent = veh ? veh : ped;

    uint8_t m[64];
    if (dbg_read(G.sock, G.pid, ent + 0x40, m, 64) < 0) {
        snprintf(err, errsz, "cannot read placement matrix");
        return false;
    }
    float tz = wz + 4.0f;
    int sx = (int)(wx / 32.0f), sy = (int)(wy / 32.0f);
    float *mf = (float *)m;
    mf[12] = wx - 32.0f * (float)sx;
    mf[13] = wy - 32.0f * (float)sy;
    mf[14] = tz;
    int16_t sec[2] = {(int16_t)sx, (int16_t)sy};
    memcpy(&mf[15], sec, 4);

    uint64_t buf = dbg_alloc(G.sock, G.pid, 0x40);
    if (!buf) { snprintf(err, errsz, "alloc failed"); return false; }
    if (dbg_write(G.sock, G.pid, buf, m, 64) < 0) {
        snprintf(err, errsz, "write matrix failed");
        return false;
    }
    uint64_t vt = 0, fn = 0;
    if (dbg_read(G.sock, G.pid, ent, &vt, 8) < 0 || !vt ||
        dbg_read(G.sock, G.pid, vt + 0x178, &fn, 8) < 0 || !fn) {
        snprintf(err, errsz, "SetMatrix vtable slot missing");
        return false;
    }
    game_call8(fn, ent, buf, 1, 1, 0, 0, 0, 0);
    return true;
}

static void handle_client(int cfd) {
    struct timeval ctv = {.tv_sec = 2, .tv_usec = 0};
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &ctv, sizeof(ctv));
    setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &ctv, sizeof(ctv));
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
        } else if (strcmp(path, "/api/waypoint") == 0) {
            float wx = 0, wy = 0, wz = 0;
            bool present = false;
            char stw[256];
            bool okw;
            pthread_mutex_lock(&G.lock);
            okw = read_waypoint_pos(&wx, &wy, &wz, &present);
            pthread_mutex_unlock(&G.lock);
            if (okw)
                snprintf(stw, sizeof(stw), "{\"ok\":true,\"present\":%s,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}",
                         present ? "true" : "false", (double)wx, (double)wy, (double)wz);
            else
                snprintf(stw, sizeof(stw), "{\"ok\":false}");
            http_send(cfd, 200, "application/json; charset=utf-8", stw, strlen(stw));
        } else if (strcmp(path, "/api/status") == 0) {
            char st[6144];
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
    } else if (strcmp(path, "/api/disconnect") == 0) {
        G.auto_connect = false;
        trainer_disconnect_locked();
        snprintf(msg, sizeof(msg), "Disconnected from PS5 (hook restored)");
        dbg_notify("Disconnected from GTA VI.");
    } else if (strcmp(path, "/api/shutdown") == 0) {
        G.auto_connect = false;
        trainer_disconnect_locked();
        dbg_notify("ELF Server stopped (port 8080 free)");
        pthread_mutex_unlock(&G.lock);
        const char *bye = "{\"message\":\"ELF Server stopped. Port 8080 is now free for update.\"}";
        http_send(cfd, 200, "application/json; charset=utf-8", bye, strlen(bye));
        close(cfd);
        if (g_listen_fd >= 0) close(g_listen_fd);
        usleep(150000);
        _exit(0);
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
    } else if (strcmp(path, "/api/teleport_waypoint") == 0) {
            char terr[128] = {0};
            if (teleport_to_waypoint(terr, sizeof(terr))) {
                snprintf(msg, sizeof(msg), "teleported to waypoint");
                dbg_notify(msg);
            } else {
                ok = false;
                snprintf(msg, sizeof(msg), "%s", terr);
            }
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
            char serr[128] = {0};
            if (spawn_vehicle_streamed(hash, serr, sizeof(serr))) {
                snprintf(msg, sizeof(msg), "Spawned %s", vname);
                dbg_notify(msg);
            } else {
                ok = false;
                snprintf(msg, sizeof(msg), "%s: %s", vname, serr);
            }
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
        else if (strcmp(name, "lektor_enabled") == 0) G.lektor_enabled = (en != 0);
        else if (strcmp(name, "lektor_read_speaker") == 0) G.lektor_read_speaker = (en != 0);
        else {
            bool matched_np = false;
            for (int pi = 0; pi < NATIVE_PATCHES_COUNT; pi++) {
                if (strcmp(name, NATIVE_PATCHES[pi].key) == 0) {
                    matched_np = true;
                    const NativePatchDef *pd = &NATIVE_PATCHES[pi];
                    bool enable = (en != 0);
                    for (size_t k = 0; k < pd->count; k++) {
                        size_t idx = enable ? k : (pd->count - 1 - k);
                        const NativePatchEntry *pe = &pd->entries[idx];
                        uint64_t addr = G.base + pe->offset;
                        uint64_t page = addr & ~0x3FFFULL;
                        uint32_t plen = (uint32_t)(((addr + pe->len - page) + 0x3FFFULL) & ~0x3FFFULL);
                        dbg_protect(G.sock, G.pid, page, plen, 7);
                        dbg_write(G.sock, G.pid, addr, enable ? pe->on_bytes : pe->off_bytes, (uint32_t)pe->len);
                        dbg_protect(G.sock, G.pid, page, plen, 5);
                    }
                    G.native_patch_on[pi] = enable;
                    snprintf(msg, sizeof(msg), "%s: %s", pd->label, enable ? "ON" : "OFF");
                    dbg_notify(msg);
                    break;
                }
            }
            if (!matched_np) {
                snprintf(msg, sizeof(msg), "%s: %s", name, en ? "ON" : "OFF");
                dbg_notify(msg);
            }
            goto toggle_done;
        }
        snprintf(msg, sizeof(msg), "%s: %s", name, en ? "ON" : "OFF");
        dbg_notify(msg);
        toggle_done:;
    } else if (strcmp(path, "/api/money/find") == 0) {
        int amt = 0;
        json_get_int(body, "amount", &amt);
        if (amt <= 0) {
            ok = false;
            snprintf(msg, sizeof(msg), "Enter exact cash > 0 (use Auto-Detect for $0)");
        } else {
            G.money_cancel = false;
            G.money_addr_cnt = 0;
            snprintf(G.money_mode, sizeof(G.money_mode), "manual_scanning");
            snprintf(G.money_note, sizeof(G.money_note), "MANUAL: scanning PS5 RAM for $%d (~12s)...", amt);
            MoneyTask *mt = (MoneyTask *)calloc(1, sizeof(MoneyTask));
            snprintf(mt->host, sizeof(mt->host), "%s", G.host);
            mt->port = G.port; mt->pid = G.pid; mt->amount = amt;
            pthread_t th; pthread_create(&th, NULL, money_manual_worker, mt); pthread_detach(th);
            snprintf(msg, sizeof(msg), "Started manual search for $%d...", amt);
        }
    } else if (strcmp(path, "/api/money/watch") == 0) {
        int amt = -1;
        bool has_f = json_get_int(body, "amount", &amt) && amt >= 0;
        if (!has_f) {
            ok = false;
            snprintf(msg, sizeof(msg), "Enter your current cash first (0 or higher)");
        } else {
            G.money_cancel = false;
            G.money_addr_cnt = 0;
            G.money_watch_cnt = 0;
            snprintf(G.money_mode, sizeof(G.money_mode), "auto_scanning");
            snprintf(G.money_note, sizeof(G.money_note), "AUTO: indexing wallet stats for $%d (~10s)...", amt);
            MoneyTask *mt = (MoneyTask *)calloc(1, sizeof(MoneyTask));
            snprintf(mt->host, sizeof(mt->host), "%s", G.host);
            mt->port = G.port; mt->pid = G.pid; mt->amount = amt; mt->has_filter = true;
            pthread_t th; pthread_create(&th, NULL, money_auto_worker, mt); pthread_detach(th);
            snprintf(msg, sizeof(msg), "Started Auto-Detect wallet watcher ($%d)...", amt);
        }
    } else if (strcmp(path, "/api/money/add") == 0) {
        int delta = 0;
        json_get_int(body, "delta", &delta);
        if (G.money_addr_cnt <= 0) {
            ok = false;
            snprintf(msg, sizeof(msg), "Find wallet first (Manual or Auto-Detect)");
        } else {
            int32_t cur = 0;
            dbg_read(G.sock, G.pid, G.money_addrs[0], &cur, 4);
            int64_t nv = (int64_t)cur + (int64_t)delta;
            if (nv < 0) nv = 0;
            if (nv > 2000000000LL) nv = 2000000000LL;
            int32_t nv32 = (int32_t)nv;
            for (int i = 0; i < G.money_addr_cnt; i++) dbg_write(G.sock, G.pid, G.money_addrs[i], &nv32, 4);
            snprintf(msg, sizeof(msg), "+$%d -> now $%d (%d addrs)", delta, nv32, G.money_addr_cnt);
            dbg_notify(msg);
        }
    } else if (strcmp(path, "/api/money/set") == 0) {
        int amt = 1000000;
        json_get_int(body, "amount", &amt);
        if (G.money_addr_cnt <= 0) {
            ok = false;
            snprintf(msg, sizeof(msg), "Find wallet first (Manual or Auto-Detect)");
        } else {
            if (amt < 0) amt = 0;
            int32_t v = (int32_t)amt;
            for (int i = 0; i < G.money_addr_cnt; i++) dbg_write(G.sock, G.pid, G.money_addrs[i], &v, 4);
            snprintf(msg, sizeof(msg), "Cash set to $%d (%d addrs)", v, G.money_addr_cnt);
            dbg_notify(msg);
        }
    } else if (strcmp(path, "/api/money/reset") == 0) {
        G.money_cancel = true;
        G.money_addr_cnt = 0;
        G.money_watch_cnt = 0;
        snprintf(G.money_mode, sizeof(G.money_mode), "idle");
        G.money_note[0] = '\0';
        snprintf(msg, sizeof(msg), "Wallet mode reset");
    } else if (strcmp(path, "/api/lektor/rate") == 0) {
        int r = 195;
        json_get_int(body, "rate", &r);
        G.lektor_rate = r;
        snprintf(msg, sizeof(msg), "Voiceover rate: %d", r);
    } else {
        ok = false;
        snprintf(msg, sizeof(msg), "Not available in standalone ELF yet");
    }

    char st[6144], resp[6600];
    build_status_json(st, sizeof(st));
    pthread_mutex_unlock(&G.lock);

    snprintf(resp, sizeof(resp), "{\"%s\":\"%s\",\"status\":%s}", ok ? "message" : "error", msg, st);
    http_send(cfd, ok ? 200 : 400, "application/json; charset=utf-8", resp, strlen(resp));
    close(cfd);
}

/* ---------- Automatic PS5 Home-Screen Media Tile Registration (PPSA38599) ---------- */

extern int sceAppInstUtilInitialize(void);
extern int sceAppInstUtilTerminate(void);
extern int sceAppInstUtilAppInstallAll(void *);

static void write_file_bytes(const char *path, const unsigned char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(data, 1, len, f);
    fclose(f);
    chmod(path, 0644);
}

static void *tile_thread_fn(void *arg) {
    (void)arg;
    const char *tid = "PPSA38599";
    struct stat st1, st2;
    if (stat("/user/app/PPSA38599/sce_sys/param.json", &st1) == 0 &&
        stat("/user/app/PPSA38599/sce_sys/icon0.png", &st2) == 0 &&
        (size_t)st2.st_size == ASSET_ICON0_PNG_len) {
        return NULL; /* Tile already installed with latest icon — skip re-registration */
    }
    mkdir("/user/app", 0700);
    mkdir("/user/app/PPSA38599", 0755);
    mkdir("/user/app/PPSA38599/sce_sys", 0755);
    write_file_bytes("/user/app/PPSA38599/sce_sys/param.json", ASSET_PARAM_JSON, ASSET_PARAM_JSON_len);
    write_file_bytes("/user/app/PPSA38599/sce_sys/icon0.png", ASSET_ICON0_PNG, ASSET_ICON0_PNG_len);

    if (sceAppInstUtilInitialize() == 0) {
        typedef int (*install_dir_fn)(const char *, const char *, void *);
        install_dir_fn install_dir = NULL;
        uint32_t handle = 0;
        if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &handle) == 0) {
            install_dir = (install_dir_fn)kernel_dynlib_resolve(-1, handle, "Wudg3Xe3heE");
        }
        int rc = install_dir ? install_dir(tid, "/user/app/", NULL) : sceAppInstUtilAppInstallAll(NULL);
        sceAppInstUtilTerminate();
        if (rc == 0) dbg_notify("Media tile GTA VI Web Trainer installed!");
    }
    return NULL;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    pthread_t bg, sub, ntf, tile;
    pthread_create(&bg, NULL, bg_thread_fn, NULL);
    pthread_detach(bg);
    pthread_create(&sub, NULL, subtitle_thread_fn, NULL);
    pthread_detach(sub);
    pthread_create(&ntf, NULL, notify_thread_fn, NULL);
    pthread_detach(ntf);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) return 1;
    g_listen_fd = sfd;
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
    dbg_notify(TRAINER_VERSION " by deFIATer/czupryneq (Port 8080)");

    /* Register Media tile asynchronously AFTER HTTP server is already live on :8080 */
    pthread_create(&tile, NULL, tile_thread_fn, NULL);
    pthread_detach(tile);

    for (;;) {
        int cfd = accept(sfd, NULL, NULL);
        if (cfd >= 0) handle_client(cfd);
    }
    return 0;
}
