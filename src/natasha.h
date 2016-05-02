#ifndef CORE_H_
#define CORE_H_

#include <rte_ethdev.h>
#include <rte_rwlock.h>

#include <jit/jit.h>

/*
 * Logging configuration.
 */
#ifdef DEBUG
    #define LOG_LEVEL RTE_LOG_DEBUG
    #define LOG_DEBUG(log_type, fmt, args...) do {  \
        RTE_LOG(DEBUG, log_type, fmt, ##args);      \
    } while (0)
#else
    #define LOG_LEVEL RTE_LOG_INFO
    #define LOG_DEBUG(log_type, fmt, args...) do{} while(0)
#endif

// Configure log level type "APP": RTE_LOG(level, >> APP <<).
#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1


/*
 * Application configuration.
 */

// Forward declaration. Defined under "Workers and queues configuration".
struct core;

struct app_config_port {
    int mtu;
    struct app_config_port_ip_addr {
        struct ip_vlan {
            uint32_t ip;
            int vlan;
        } addr;
        struct app_config_port_ip_addr *next;
    } *ip_addresses;
};

// Software configuration.
/*
 * Size of the first, second and third row of the NAT lookup table.
 */
static const int lkp_fs = 256; // 2^8
static const int lkp_ss = 256; // 2^8
static const int lkp_ts = 65536; // 2^16

struct app_config {

    // If 1, this configuration has been used at least once and in case of
    // reload (see config.c/app_config_relaod_all), old configuration is no
    // longer used by the core and can safely be freed.
    volatile int used;

    struct app_config_port ports[RTE_MAX_ETHPORTS];

    /*
     * Contain NAT rules. The rule "10.1.2.3 -> 212.10.11.12" is stored as
     * two entries, as follow:
     *
     * - nat_lookup = table of 256 (2^8) int **
     * - nat_lookup[10] = table of 256 (2^8) int *
     * - nat_lookup[10][1] = table of 65536 (2^16) int
     * - nat_lookup[10][1][2 << 16 & 3] = 212.10.11.12
     *
     * and:
     *
     * - nat_lookup = table of 256 (2^8) int **
     * - nat_lookup[212] = table of 256 (2^8) int *
     * - nat_lookup[212][10] = table of 65536 (2^16) int
     * - nat_lookup[212][10][11 << 16 & 12] = 10.1.2.3
     */
    uint32_t ***nat_lookup;

    // The JIT function called to process every packet.
    int (*process_pkt)(struct rte_mbuf *, uint8_t, struct core *);
};


/*
 * Workers and queues configuration.
 */

// Network receive queue.
struct rx_queue {
    uint16_t id;
};

#define MAX_TX_BURST 32
// Network transmit queue.
struct tx_queue {
    uint16_t id;

    // Packets to send.
    struct rte_mbuf *pkts[MAX_TX_BURST];

    // Number of packets in pkts.
    uint16_t len;
};

// A core and its queues. Each core has one rx queue and one tx queue per port.
struct core {
    int id;

    struct app_config *app_config;

    struct rx_queue rx_queues[RTE_MAX_ETHPORTS];
    struct tx_queue tx_queues[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

/*
 * Prototypes.
 */

// config.c
struct app_config *app_config_load(int argc, char **argv,
                                   unsigned int socket_id);
void app_config_free(struct app_config *config);
int support_per_queue_statistics(uint8_t port);
int app_config_reload_all(struct core *cores, int argc, char **argv,
                          int out_fd);
jit_value_t call_natasha(jit_function_t jit_function,
                         int (*func)(struct rte_mbuf *, uint8_t, struct core *, void *),
                         void *data,
                         size_t datasize);

// stats.c
void stats_display(int fd);
int stats_reset(int fd);

// pkt.c
uint16_t tx_send(struct rte_mbuf *pkt, uint8_t port, struct tx_queue *queue);
uint16_t tx_flush(uint8_t port, struct tx_queue *queue);

int is_natasha_ip(struct app_config *app_config,
                  uint32_t ip, int vlan);
int is_natasha_port_ip(struct app_config *app_config,
                       uint32_t ip, int vlan, uint8_t port);

// arp.c
int arp_handle(struct rte_mbuf *pkt, uint8_t port, struct core *core);
int ipv4_handle(struct rte_mbuf *pkt, uint8_t port, struct core *core);

// adm.c
int adm_server(struct core *cores, int argc, char **argv);

/*
 * Utility macros.
 */
#define IPv4_FMT            "%i.%i.%i.%i"
#define IPv4_FMTARGS(ip)    ((ip) >> 24) & 0xff,   \
                            ((ip) >> 16) & 0xff,   \
                            ((ip) >>  8) & 0xff,    \
                            ((ip) >>  0) & 0xff

#define MAC_FMT                 "%x:%x:%x:%x:%x:%x"
#define MAC_FMTARGS(ether_addr) (ether_addr).addr_bytes[0], \
                                (ether_addr).addr_bytes[1], \
                                (ether_addr).addr_bytes[2], \
                                (ether_addr).addr_bytes[3], \
                                (ether_addr).addr_bytes[4], \
                                (ether_addr).addr_bytes[5]

#endif
