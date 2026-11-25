#include <3ds.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net.h"
#include "protocol.h"

#define SOC_ALIGN     0x1000
#define SOC_BUFSIZE   0x80000   /* 512 KB: suficiente e poupa RAM no Old 3DS */

static u32 *soc_buf = NULL;
static int sock = -1;

static int open_socket(void)
{
    if (sock >= 0) { close(sock); sock = -1; }
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = 0;                   /* porta efemera */
    a.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr *)&a, sizeof(a));

    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    int rcv = 128 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv)); /* melhor esforco */

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    return 1;
}

int net_init(void)
{
    soc_buf = (u32 *)memalign(SOC_ALIGN, SOC_BUFSIZE);
    if (!soc_buf) return 0;
    if (R_FAILED(socInit(soc_buf, SOC_BUFSIZE))) {
        free(soc_buf);
        soc_buf = NULL;
        return 0;
    }
    return open_socket();
}

void net_exit(void)
{
    if (sock >= 0) close(sock);
    sock = -1;
    if (soc_buf) {
        socExit();
        free(soc_buf);
        soc_buf = NULL;
    }
}

int net_reopen(void) { return open_socket(); }

uint32_t net_local_ip(void)
{
    u32 wifi = 0;
    if (R_FAILED(ACU_GetWifiStatus(&wifi)) || wifi == 0) return 0;
    return (uint32_t)gethostid();
}

int net_wifi_strength(void)
{
    return osGetWifiStrength();
}

int net_send(uint32_t ip, const void *buf, int len)
{
    if (sock < 0 && !open_socket()) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(KO_PORT);
    a.sin_addr.s_addr = ip;
    int r = sendto(sock, buf, (size_t)len, 0, (struct sockaddr *)&a, sizeof(a));
    if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) open_socket();
    return r;
}

int net_recv(void *buf, int cap, uint32_t *from_ip)
{
    if (sock < 0 && !open_socket()) return -1;
    struct sockaddr_in a;
    socklen_t al = sizeof(a);
    int r = recvfrom(sock, buf, (size_t)cap, 0, (struct sockaddr *)&a, &al);
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        open_socket();
        return -1;
    }
    if (from_ip) *from_ip = a.sin_addr.s_addr;
    return r;
}

void net_discover(void)
{
    uint8_t p[KO_HDR_SIZE];
    ko_header(p, KO_T_DISCOVER);
    net_send(0xFFFFFFFFu, p, sizeof(p));               /* 255.255.255.255 */
    uint32_t me = net_local_ip();
    if (me) {
        /* broadcast da sub-rede /24 (ip em ordem de rede: ultimo octeto no byte alto) */
        uint32_t bc = me | htonl(0x000000FFu);
        net_send(bc, p, sizeof(p));
    }
}
