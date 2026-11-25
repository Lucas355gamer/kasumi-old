#ifndef KO_NET_H
#define KO_NET_H
#include <stdint.h>

int      net_init(void);          /* 1 = ok */
void     net_exit(void);
int      net_reopen(void);        /* recria o socket (ex.: depois de fechar a tampa) */
uint32_t net_local_ip(void);      /* 0 se sem Wi-Fi */
int      net_wifi_strength(void); /* 0..3 */
int      net_send(uint32_t ip, const void *buf, int len);
/* nao bloqueante: >0 bytes, 0 nada, <0 erro (socket recriado automaticamente) */
int      net_recv(void *buf, int cap, uint32_t *from_ip);
void     net_discover(void);      /* broadcast DISCOVER */

#endif
