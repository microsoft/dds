/*
 * Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */

#ifndef SIMPLE_FWD_PKT_H_
#define SIMPLE_FWD_PKT_H_

#include <stdint.h>
#include <stdbool.h>

#include <doca_flow_net.h>

#define IPV4 (4)	/* IPv4 address length in bytes */
#define IPV6 (6)	/* IPv6 address length in bytes */

/**
 *  Packet format, used internally for parsing.
 *  points to relevant point in packet and
 *  classify it.
 */
struct simple_fwd_pkt_format {

	uint8_t *l2;		/* Pointer to Layer 2 format */
	uint8_t *l3;		/* Pointer to Layer 3 format */
	uint8_t *l4;		/* Pointer to Layer 4 format */

	uint8_t l3_type;	/* Layer 2 protocol type */
	uint8_t l4_type;	/* Layer 3 protocol type */

	/* if tunnel it is the internal, if no tunnel then outer*/
	uint8_t *l7;
};

/**
 *  Packet's tunneling parsing result.
 *  points to relevant point in packet and
 *  classify it.
 */
struct simple_fwd_pkt_tun_format {
	bool l2;				/* Flag representing whether or not layer 2 is found */
	enum doca_flow_tun_type type;		/* Tunneling type (GRE, GTP or VXLAN) */

	/* Packet's tunneling parsing result represented as either GTP, GRE or VXLAN tunneling */
	union {
		struct {
			doca_be32_t vni;	/* VXLAN VNI */
		};
		struct {
			doca_be32_t gre_key;	/* GRE key value */
			doca_be16_t proto;	/* GRE protocol type */
		};
		struct {
			uint8_t gtp_msg_type;	/* GTP message type */
			uint8_t gtp_flags;	/* GTP flags */
			doca_be32_t teid;	/* GTP tied */
		};
	};
};

/**
 *  Packet parsing result.
 *  points to relevant point in packet and
 *  classify it.
 */
struct simple_fwd_pkt_info {
	void *orig_data;	/* Pointer ro the packet raw data; before being formatted */
	uint16_t orig_port_id;	/* Port identifier from which the packet was received */
	uint16_t pipe_queue;	/* The pipe queue of the received packet, this should be the same as the RX queue index */
	uint32_t rss_hash;	/* RSS hash value */

	struct simple_fwd_pkt_format outer;	/* Outer packet parsing result */
	enum doca_flow_tun_type tun_type;	/* Tunneling type (GRE, GTP or VXLAN) */
	struct simple_fwd_pkt_tun_format tun;	/* Tunneling parsing result*/
	struct simple_fwd_pkt_format inner;	/* Inner packet parsing result */
	int len;				/* Length, in bytes, of the packet */
};

/*
 * Packet's key, for entry search.
 * computed from packet's parsing result, based on the 5-tuple and the tunneling type.
 */
struct simple_fwd_ft_key {
	doca_be32_t ipv4_1;	/* First Ipv4 address */
	doca_be32_t ipv4_2;	/* Second Ipv4 address */
	doca_be16_t port_1;	/* First port address */
	doca_be16_t port_2;	/* Second port address */
	doca_be32_t vni;	/* VNI value */
	uint8_t protocol;	/* Protocol type */
	uint8_t tun_type;	/* Supported tunneling type (GRE, GTP or VXLAN) */
	uint16_t port_id;	/* Port identifier on which the packet was received */
	uint8_t pad[4];		/* Padding bytes in the packet */
	uint32_t rss_hash;	/* RSS hash value */
};

/*
 * Parses the packet and extract the relevant headers, outer/inner in addition to the tunnels.
 *
 * @data [in]: packet raw data
 * @len [in]: the length of the packet's raw data in bytes
 * @pinfo [out]: extracted packet's info
 * @return: 0 on success and negative value otherwise
 */
int
simple_fwd_parse_packet(uint8_t *data, int len,
			struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the outer destination MAC address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: outer destination MAC address
 */
uint8_t*
simple_fwd_pinfo_outer_mac_dst(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the outer source MAC address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: outer source MAC address
 */
uint8_t*
simple_fwd_pinfo_outer_mac_src(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the outer destination IPv4 address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: outer destination IPv4 address
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be32_t
simple_fwd_pinfo_outer_ipv4_dst(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the outer source IPv4 address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: outer source IPv4 address
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be32_t
simple_fwd_pinfo_outer_ipv4_src(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the inner source IPv4 address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: inner source IPv4 address
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be32_t
simple_fwd_pinfo_inner_ipv4_src(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the inner destination IPv4 address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: inner destination IPv4 address
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be32_t
simple_fwd_pinfo_inner_ipv4_dst(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the inner source port address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: inner source port
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be16_t
simple_fwd_pinfo_inner_src_port(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the inner destination port address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: inner destination port
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be16_t
simple_fwd_pinfo_inner_dst_port(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the outer source port address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: outer source port
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be16_t
simple_fwd_pinfo_outer_src_port(struct simple_fwd_pkt_info *pinfo);

/*
 * Extracts the outer destination port address from the packet's info
 *
 * @pinfo [in]: the packet's info
 * @return: outer destination port
 *
 * @NOTE: the returned value is converted to big endian
 */
doca_be16_t
simple_fwd_pinfo_outer_dst_port(struct simple_fwd_pkt_info *pinfo);

/*
 * Decap the packet's header if the tunneling is VXLAN
 *
 * @pinfo [in]: the packet's info
 */
void
simple_fwd_pinfo_decap(struct simple_fwd_pkt_info *pinfo);

#endif /* SIMPLE_FWD_PKT_H_ */
