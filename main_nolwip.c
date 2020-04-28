#include <stdio.h>
#include <uk/netdev.h>
#include <uk/alloc.h>
#include <string.h>
#include <assert.h>

#define UKNETDEV_BPS 1000000000u
#define UKNETDEV_BUFLEN 2048

#define ETH_PAD_SIZE 2

#define INTERRUPT_MODE

static uint16_t tx_headroom = ETH_PAD_SIZE;
static uint16_t rx_headroom = ETH_PAD_SIZE;

struct uk_netbuf *alloc_netbuf(struct uk_alloc *a, size_t alloc_size,
		size_t headroom)
{
	void *allocation;
	struct uk_netbuf *b;

	allocation = uk_malloc(a, alloc_size);
	if (unlikely(!allocation))
		goto err_out;

	b = uk_netbuf_prepare_buf(allocation, alloc_size,
			headroom, 0, NULL);
	if (unlikely(!b)) {
		goto err_free_allocation;
	}

	b->_a = a;
	b->len = b->buflen - headroom;

	return b;

err_free_allocation:
	uk_free(a, allocation);
err_out:
	return NULL;
}

static uint16_t netif_alloc_rxpkts(void *argp, struct uk_netbuf *nb[],
		uint16_t count)
{
	struct uk_alloc *a;
	uint16_t i;

	UK_ASSERT(argp);

	a = (struct uk_alloc *) argp;

	for (i = 0; i < count; ++i) {
		nb[i] = alloc_netbuf(a, UKNETDEV_BUFLEN, rx_headroom);
		if (!nb[i]) {
			/* we run out of memory */
			printf("Ran out of memory\n");
			break;
		}
	}

	return i;
}

struct	ether_header {
	uint8_t	ether_dhost[6];
	uint8_t	ether_shost[6];
	uint8_t	ether_type;
}__attribute__((packed));

struct udphdr {
	uint16_t source;
	uint16_t dest;
	uint16_t len;
	uint16_t check;
}__attribute__((packed));

struct iphdr 
{
	uint8_t	ihl:4,
		version:4;
	uint8_t	tos;
	uint16_t	tot_len;
	uint16_t	id;
	uint16_t	frag_off;
	uint8_t	ttl;
	uint8_t	protocol;
	uint16_t	check;
	uint32_t	saddr;
	uint32_t	daddr;
	/*The options start here. */
}__attribute__((packed));



static __inline uint16_t __bswap_16(uint16_t __x)
{
	return __x<<8 | __x>>8;
}

#define bswap_16(x) __bswap_16(x)

uint16_t ntohs(uint16_t n)
{
	union { int i; char c; } u = { 1 };
	return u.c ? bswap_16(n) : n;
}

struct uk_netbuf *queue[2000];
int k = 0;

static inline void packet_handler(struct uk_netdev *dev,
		uint16_t queue_id __unused, void *argp)
{

	struct ether_header *eth_header;
	struct iphdr *ip_hdr;
	int ret;
	k = 0;

	printf("%d\n", k);
	do {
		struct uk_netbuf *nb;
		ret = uk_netdev_rx_one(dev, 0, &nb);

		if (uk_netdev_status_notready(ret)) {
			continue;
		}
		queue[k] = nb;
		k++;

	} while(uk_netdev_status_more(ret));
	//printf("%d\n",k);
	printf("%d\n", k);


	uk_sched_yield();

}

int main(void)
{
	struct uk_alloc *a;
	struct uk_netdev *dev;
	struct uk_netdev_conf dev_conf;
	struct uk_netdev_rxqueue_conf rxq_conf;
	struct uk_netdev_txqueue_conf txq_conf;
	int devid = 0;
	int ret;

	printf("%d\n", uk_netdev_count());
	a = uk_alloc_get_default();

	dev = uk_netdev_get(devid);

	struct uk_netdev_info info;
	/* Get device informations */
	uk_netdev_info_get(dev, &info);
	if (!info.max_rx_queues || !info.max_tx_queues) {
		printf("Opsie\n");
	}

	rx_headroom = (rx_headroom < info.nb_encap_rx)
		? info.nb_encap_rx : rx_headroom;
	tx_headroom = (tx_headroom < info.nb_encap_tx)
		? info.nb_encap_tx : tx_headroom;

	dev_conf.nb_rx_queues = 1;
	dev_conf.nb_tx_queues = 1;

	/* Configure the device */
	ret = uk_netdev_configure(dev, &dev_conf);
	assert(ret >= 0);

	/* Configure the RX queue */
	printf("Configuring RX queue\n");
	rxq_conf.a = a;
	rxq_conf.alloc_rxpkts = netif_alloc_rxpkts;
	rxq_conf.alloc_rxpkts_argp = a;
	/* No threads */
#ifdef INTERRUPT_MODE
	rxq_conf.callback = packet_handler;
	rxq_conf.callback_cookie = NULL;
	rxq_conf.s = uk_sched_get_default();
#else
	rxq_conf.callback = NULL;
	rxq_conf.callback_cookie = NULL;
#endif

	ret = uk_netdev_rxq_configure(dev, 0, 0, &rxq_conf);
	assert(ret >= 0);
	printf("Configured RX queue\n");

	/* TX Queue */
	txq_conf.a = a;
	ret = uk_netdev_txq_configure(dev, 0, 0, &txq_conf);
	printf("Configured TX queue\n");

	/* GET mTU */
	uint16_t mtu = uk_netdev_mtu_get(dev);
	printf("MTU: %d\n", mtu);

	ret = uk_netdev_start(dev);

	struct uk_hwaddr *hw;
	hw = uk_netdev_hwaddr_get(dev);
	printf("MAC %x:%x:%x:%x:%x\n", hw->addr_bytes[0],hw->addr_bytes[1], hw->addr_bytes[2], hw->addr_bytes[3], hw->addr_bytes[4], hw->addr_bytes[5] );

	ret = uk_netdev_rxq_intr_enable(dev, 0);
	if (ret < 0) {
		printf("Opsie\n");
	}
	/* Receive packet */
	struct uk_netbuf *nb;
	printf("ETH %d IP %d UDP %d\n", sizeof(struct ether_header), sizeof(struct iphdr), sizeof(struct udphdr));

#ifndef INTERRUPT_MODE
	uk_netdev_rxq_intr_disable(dev, 0);
#endif
	struct ether_header *eth_header;
	struct iphdr *ip_hdr;
	while (1) {

#ifdef INTERRUPT_MODE
		uk_sched_yield();
#else
		packet_handler(dev, 0, NULL);
#endif
		/* We echo all the packets that are in queue */
		for (int i = 0; i < k; i++) {
			//k = 120;
			struct uk_netbuf *nb;
			nb = queue[i];

			eth_header = (struct ether_header *) nb->data;


			if (eth_header->ether_type == 8) {
				/* Eth has a pad of 1 */
				ip_hdr = (struct iphdr *)((char *)nb->data + sizeof(struct ether_header) + 1);
				if (ip_hdr->protocol == 0x11) {
					//printf("sending response\n");
					ip_hdr = (struct iphdr *)((char *)nb->data + sizeof(struct ether_header) + 1);
					struct udphdr * udp_hdr = (struct udphdr *)((char *)nb->data + sizeof(struct ether_header) + 1 + sizeof(struct iphdr));

					uint8_t tmp[6];
					memcpy(tmp, eth_header->ether_dhost, 6);
					memcpy(eth_header->ether_dhost, eth_header->ether_shost, 6);
					memcpy(eth_header->ether_shost, tmp, 6);

					ip_hdr->saddr ^= ip_hdr->daddr;
					ip_hdr->daddr ^= ip_hdr->saddr;
					ip_hdr->saddr ^= ip_hdr->daddr;

					udp_hdr->source ^= udp_hdr->dest;
					udp_hdr->dest ^= udp_hdr->source;
					udp_hdr->source ^= udp_hdr->dest;
					do {
						ret = uk_netdev_tx_one(dev, 0, nb);
					} while(uk_netdev_status_notready(ret));

				}
			}
		}

	}
	return 0;
}
