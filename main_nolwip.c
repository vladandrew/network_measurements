#include <stdio.h>
#include <uk/netdev.h>
#include <uk/alloc.h>
#include <string.h>
#include <assert.h>

#define UKNETDEV_BPS 1000000000u
#define UKNETDEV_BUFLEN 2048

#define ETH_PAD_SIZE 2

///#define INTERRUPT_MODE
/* We create an array that represents a queue, we do not
 * send the packet immediatly, we wait until we receive
 * all packets. Otherwise as soon as we receive a packet
 * we send it to the target*/

#define USE_SIMPLE_QUEUE
#define USE_TX_BURST
#define USE_RX_BURST

//#define RX_ONLY_ONE_PACKET

//#define TX_NO_RETRANSMISSION

/* These headers are taken from linux */
struct	ether_header {
	uint8_t	ether_dhost[6];
	uint8_t	ether_shost[6];
	uint16_t ether_type;
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
}__attribute__((packed));

static uint16_t tx_headroom = ETH_PAD_SIZE;
static uint16_t rx_headroom = ETH_PAD_SIZE;

#ifdef USE_SIMPLE_QUEUE
/* 256 is the upper bound since this is the maximum number of tx descriptors */
struct uk_netbuf *queue[256];
uint16_t k = 0;
#endif

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
		assert(nb[i]);
	}

	return i;
}

static void inline prepare_packet(struct uk_netbuf *nb)
{

	struct ether_header *eth_header;
	struct iphdr *ip_hdr;
	struct udphdr *udp_hdr;

	eth_header = (struct ether_header *) nb->data;
	if (eth_header->ether_type == 8) {
		ip_hdr = (struct iphdr *)((char *)nb->data + sizeof(struct ether_header));
		if (ip_hdr->protocol == 0x11) {
			ip_hdr = (struct iphdr *)((char *)nb->data + sizeof(struct ether_header));
			udp_hdr = (struct udphdr *)((char *)nb->data + sizeof(struct ether_header) + sizeof(struct iphdr));

			/* Switch MAC */
			uint8_t tmp[6];
			memcpy(tmp, eth_header->ether_dhost, 6);
			memcpy(eth_header->ether_dhost, eth_header->ether_shost, 6);
			memcpy(eth_header->ether_shost, tmp, 6);

			/* Switch IP addresses */
			ip_hdr->saddr ^= ip_hdr->daddr;
			ip_hdr->daddr ^= ip_hdr->saddr;
			ip_hdr->saddr ^= ip_hdr->daddr;

			/* switch UDP PORTS */
			udp_hdr->source ^= udp_hdr->dest;
			udp_hdr->dest ^= udp_hdr->source;
			udp_hdr->source ^= udp_hdr->dest;

			/* No checksum requiere, they are 16 bits and
			 * switching them does not influence the checsum
			 * */
		}
	}
}

static inline void uknetdev_output(struct uk_netdev *dev, struct uk_netbuf *nb)
{
	int ret;
#ifndef TX_NO_RETRANSMISSION
	do {
#endif
		ret = uk_netdev_tx_one(dev, 0, nb);
#ifndef TX_NO_RETRANSMISSION
	} while(uk_netdev_status_notready(ret));
#endif
	if (ret < 0) {
		uk_netbuf_free_single(nb);
	}
}

static inline void packet_handler(struct uk_netdev *dev,
		uint16_t queue_id __unused, void *argp)
{

	struct ether_header *eth_header;
	struct iphdr *ip_hdr;
	int ret;
	struct uk_netbuf *nb;
#ifdef USE_SIMPLE_QUEUE
#ifdef USE_RX_BURST
	k = 0;
#else
	k = 0;
#endif /* USE_RX_BURST */
#endif /* USE_SIMPLE_QUEUE */

#ifdef USE_RX_BURST

	int k1;
	do {
		k1 = 255 - k;
		uk_netdev_rx_burst(dev, 0, &queue[k], &k1);
		k = k + k1;
	} while(k < 32);


#else
#ifndef RX_ONLY_ONE_PACKET
	do {
#endif
back:
		ret = uk_netdev_rx_one(dev, 0, &nb);

		if (uk_netdev_status_notready(ret)) {
			goto back;
		}

#ifndef USE_SIMPLE_QUEUE
		prepare_packet(nb);
		uknetdev_output(dev, nb);
#else
		queue[k] = nb;
		k++;
		if (k > 254) {
			goto done;
		}
#endif
#ifndef RX_ONLY_ONE_PACKET
	} while(uk_netdev_status_more(ret));
#endif
done:
	return;
#endif

}

int main(void)
{
	struct uk_alloc *a;
	struct uk_netdev *dev;
	struct uk_netdev_conf dev_conf;
	struct uk_netdev_rxqueue_conf rxq_conf;
	struct uk_netdev_txqueue_conf txq_conf;
	int devid = 0;
	int ret, i;

	a = uk_alloc_get_default();
	assert(a != NULL);

	dev = uk_netdev_get(devid);
	assert(dev != NULL);

	struct uk_netdev_info info;
	uk_netdev_info_get(dev, &info);
	assert(info.max_tx_queues);
	assert(info.max_rx_queues);

	assert(uk_netdev_state_get(dev) == UK_NETDEV_UNCONFIGURED);

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
	rxq_conf.a = a;
	rxq_conf.alloc_rxpkts = netif_alloc_rxpkts;
	rxq_conf.alloc_rxpkts_argp = a;
	/* No threads */
#ifdef INTERRUPT_MODE
	printf("Running in intr\n");
	rxq_conf.callback = packet_handler;
	rxq_conf.callback_cookie = packet_handler;
#ifdef CONFIG_LIBUKNETDEV_DISPATCHERTHREADS
	rxq_conf.s = uk_sched_get_default();
	assert(rxq_conf.s);
#endif /* CONFIG_LIBUKNETDEV_DISPATCHERTHREADS */
#else
	printf("Running busy waiting\n");
	rxq_conf.callback = NULL;
	rxq_conf.callback_cookie = NULL;
#endif /* INTERRUPT_MODE */

	ret = uk_netdev_rxq_configure(dev, 0, 0, &rxq_conf);
	assert(ret >= 0);

	/*  Configure the TX queue*/
	txq_conf.a = a;
	ret = uk_netdev_txq_configure(dev, 0, 0, &txq_conf);
	assert(ret >= 0);

	/* GET mTU */
	uint16_t mtu = uk_netdev_mtu_get(dev);
	assert(mtu == 1500);

	/* Start the netdev */
	ret = uk_netdev_start(dev);

#ifndef INTERRUPT_MODE
	ret = uk_netdev_rxq_intr_disable(dev, 0);
	assert(ret >= 0);
#else
	ret = uk_netdev_rxq_intr_enable(dev, 0);
	assert(ret >= 0 );
#endif

	struct ether_header *eth_header;
	struct iphdr *ip_hdr;
#ifdef USE_TX_BURST
	printf("Using TX burst\n");
#endif

#ifdef USE_RX_BURST
	printf("Using RX burst\n");
#endif

#ifdef USE_SIMPLE_QUEUE
		struct uk_netbuf *nb;
#endif
	while (1) {

#ifdef INTERRUPT_MODE
		uk_sched_yield();
#else
		packet_handler(dev, 0, NULL);
#endif
		/* We echo all the packets that are in queue */
#ifdef USE_SIMPLE_QUEUE
#ifdef USE_TX_BURST
		for (i = 0; i < k; i++) {
			nb = queue[i];
			prepare_packet(nb);
		}
		uk_netdev_tx_burst(dev, 0, &queue[0], &k);
#else
		for (i = 0; i < k; i++) {
			nb = queue[i];
			prepare_packet(nb);
			uknetdev_output(dev, nb);
		}
#endif /* USE_TX_BURST */
#endif /* USE_SIMPLE_QUEUE */
	}
	return 0;
}
