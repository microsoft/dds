/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include <rte_random.h>

#include <doca_flow.h>
#include <doca_log.h>

#include "app_vnf.h"
#include "simple_fwd.h"
#include "simple_fwd_ft.h"
#include "simple_fwd_control.h"


DOCA_LOG_REGISTER(SIMPLE_FWD);

#define MAX_PORT_STR (128)	/* Maximum length of the string name of the port */

/* Convert IPv4 address to big endian */
#define BE_IPV4_ADDR(a, b, c, d) \
	(RTE_BE32(((uint32_t)a<<24) + (b<<16) + (c<<8) + d))

/* Set the MAC address with respect to the given 6 bytes */
#define SET_MAC_ADDR(addr, a, b, c, d, e, f)\
do {\
	addr[0] = a & 0xff;\
	addr[1] = b & 0xff;\
	addr[2] = c & 0xff;\
	addr[3] = d & 0xff;\
	addr[4] = e & 0xff;\
	addr[5] = f & 0xff;\
} while (0)


#define BUILD_VNI(uint24_vni) (RTE_BE32((uint32_t)uint24_vni << 8))	/* Converting VNI to bigg endian */
#define AGE_QUERY_BURST 128						/* Aging query burst */
#define PULL_TIME_OUT 10000						/* Maximum timeout for pulling */
#define MAX_TRY 10							/* Maximum tries for checking entry status in HW */
#define NB_ACTION_ARRAY (1)						/* Used as the size of muti-actions array for DOCA Flow API */

static struct simple_fwd_app *simple_fwd_ins;			/* Instance holding all allocated resources needed for a proper run */
struct doca_flow_fwd *fwd_tbl_port[SIMPLE_FWD_PORTS];		/* Flow table for each port */
struct doca_flow_fwd *sw_rss_fwd_tbl_port[SIMPLE_FWD_PORTS];	/* RSS forwarding table for each port */
struct doca_flow_fwd *fwd_miss_tbl_port[SIMPLE_FWD_PORTS];	/* Miss forwarding table for each port */

/*
 * Get the pair port identifier of a given portidentifier
 *
 * @port_id [in]: the port identifier to get the pair port identifier for
 * @return: port identifier
 */
static inline uint16_t
simple_fwd_get_pair_id(uint16_t port_id)
{
	return simple_fwd_ins->hairpin_peer[port_id];
}

/*
 * Pair a  port to its peer
 *
 * @port_id [in]: port identifier to bind it to its peer
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_build_port_pair(uint16_t port_id)
{
	int ret = 0;

	if (port_id == 0 || (port_id % 2) == 0)
		return 0;
	ret = doca_flow_port_pair(simple_fwd_ins->port[port_id ^ 1], simple_fwd_ins->port[port_id]);
	if (ret < 0) {
		DOCA_LOG_ERR("port pair %d - %d error", port_id - 1, port_id);
		return -1;
	}
	simple_fwd_ins->hairpin_peer[port_id ^ 1] = port_id;
	simple_fwd_ins->hairpin_peer[port_id] = port_id ^ 1;
	return 0;
}

/*
 * Callback funtion for removing aged flow
 *
 * @ctx [in]: the context of the aged flow to remove
 */
static void
simple_fwd_aged_flow_cb(struct simple_fwd_ft_user_ctx *ctx)
{
	struct simple_fwd_pipe_entry *entry =
		(struct simple_fwd_pipe_entry *)&ctx->data[0];

	if (entry->is_hw) {
		doca_flow_pipe_rm_entry(entry->pipe_queue, NULL, entry->hw_entry);
		entry->hw_entry = NULL;
	}
}

/*
 * Destroy flow table used by the application
 *
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_destroy_ins(void)
{
	uint16_t idx;

	if (simple_fwd_ins == NULL)
		return 0;

	simple_fwd_ft_destroy(simple_fwd_ins->ft);

	for (idx = 0; idx < simple_fwd_ins->nb_queues; idx++) {
		if (simple_fwd_ins->query_array[idx])
			free(simple_fwd_ins->query_array[idx]);
	}
	for (idx = 0; idx < SIMPLE_FWD_PORTS; idx++) {
		if (simple_fwd_ins->port[idx])
			doca_flow_port_destroy(simple_fwd_ins->port[idx]);
	}
	free(simple_fwd_ins);
	simple_fwd_ins = NULL;
	return 0;
}

/*
 * Initializes flow tables used by the application for a given port
 *
 * @port_cfg [in]: the port configuration to allocate the resources
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_create_ins(struct simple_fwd_port_cfg *port_cfg)
{
	uint16_t index;
	struct doca_flow_aged_query *entries;

	simple_fwd_ins = (struct simple_fwd_app *) calloc(1, sizeof(struct simple_fwd_app) +
	sizeof(struct doca_flow_aged_query *) * port_cfg->nb_queues);
	if (simple_fwd_ins == NULL) {
		DOCA_LOG_CRIT("failed to allocate SF");
		goto fail_init;
	}

	simple_fwd_ins->ft = simple_fwd_ft_create(SIMPLE_FWD_MAX_FLOWS,
					sizeof(struct simple_fwd_pipe_entry),
					&simple_fwd_aged_flow_cb, NULL,
					port_cfg->age_thread);
	if (simple_fwd_ins->ft == NULL) {
		DOCA_LOG_CRIT("failed to allocate FT");
		goto fail_init;
	}
	simple_fwd_ins->nb_queues = port_cfg->nb_queues;
	for (index = 0 ; index < port_cfg->nb_queues; index++) {
		entries = malloc(sizeof(*entries) * AGE_QUERY_BURST);
		if (entries == NULL)
			goto fail_init;
		simple_fwd_ins->query_array[index] = entries;
	}
	for (index = 0 ; index < SIMPLE_FWD_PORTS; index++)
		simple_fwd_ins->hairpin_peer[index] = UINT16_MAX;
	return 0;
fail_init:
	simple_fwd_destroy_ins();
	return -1;
}

/*
 * Build port forwarding, forwarding to another flow port
 *
 * @port_cfg [in]: the port configuration to build the forwarding component
 * @return: a pointer to the built FWD component, NULL otherwise
 */
static struct doca_flow_fwd*
simple_fwd_build_port_fwd(struct simple_fwd_port_cfg *port_cfg)
{
	struct doca_flow_fwd *fwd = calloc(1, sizeof(struct doca_flow_fwd));

	if (fwd == NULL) {
		DOCA_LOG_CRIT("failed to allocate fwd");
		return NULL;
	}
	fwd->type = DOCA_FLOW_FWD_PORT;
	fwd->port_id = port_cfg->port_id;
	return fwd;
}

/*
 * Build RSS forwarding
 *
 * @n_queues [in]: number of queues used for RSS hashing
 * @return: a pointer to the build FWD component, NULL otherwise
 */
static struct doca_flow_fwd*
simple_fwd_build_rss_fwd(int n_queues)
{
	int i;
	struct doca_flow_fwd *fwd = calloc(1, sizeof(struct doca_flow_fwd));
	uint16_t *queues;

	if (fwd == NULL) {
		DOCA_LOG_CRIT("failed to allocate fwd");
		return NULL;
	}
	/* rss on all queues */
	queues = malloc(sizeof(uint16_t) * n_queues);
	if (queues == NULL) {
		DOCA_LOG_CRIT("failed to allocate queues");
		free(fwd);
		return NULL;
	}

	for (i = 0; i < n_queues; i++)
		queues[i] = i;
	fwd->type = DOCA_FLOW_FWD_RSS;
	fwd->rss_queues = queues;
	fwd->rss_flags = DOCA_FLOW_RSS_IP | DOCA_FLOW_RSS_UDP;
	fwd->num_of_queues = n_queues;
	return fwd;
}

/*
 * Build pipe and adds entry, with FWD and FWD miss components
 *
 * @port_cfg [in]: the port configuration to build pipe and add entry
 * @port [in]: flow port to build the pipe and add the entry to
 * @return: the built FWD miss component
 */
static struct doca_flow_fwd *
simple_fwd_build_port_fwd_miss(struct simple_fwd_port_cfg *port_cfg,
	struct doca_flow_port *port)
{
	struct doca_flow_fwd *fwd = calloc(1, sizeof(struct doca_flow_fwd));
	struct doca_flow_fwd *fwd_miss = calloc(1, sizeof(struct doca_flow_fwd));
	struct doca_flow_pipe *next_pipe = NULL;
	struct doca_flow_pipe_cfg pipe_cfg = {0};
	struct doca_flow_actions actions = {0};
	struct doca_flow_actions *actions_array[NB_ACTION_ARRAY];
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_monitor mon = {0};
	uint16_t *queues = NULL;
	int n_queues;
	int qidx;

	if (fwd == NULL || fwd_miss == NULL)
		goto build_fail;

	/* build match */
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = IPPROTO_UDP;

	/* build pipe cfg */
	pipe_cfg.port = port;
	pipe_cfg.match = &match;
	actions_array[0] = &actions;
	pipe_cfg.actions = actions_array;
	pipe_cfg.attr.name = "NEXT_PIPE";
	pipe_cfg.attr.nb_actions = 1;
	pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;

	/* build fwd config */
	n_queues = simple_fwd_ins->nb_queues;
	queues = calloc(n_queues, sizeof(uint16_t));
	if (queues == NULL)
		goto build_fail;
	for (qidx = 0; qidx < n_queues; qidx++)
		queues[qidx] = qidx;

	fwd->type = DOCA_FLOW_FWD_RSS;
	fwd->rss_queues = queues;
	fwd->rss_flags = DOCA_FLOW_RSS_IP | DOCA_FLOW_RSS_UDP;
	fwd->num_of_queues = n_queues;

	/* build next_pipe */
	next_pipe = doca_flow_pipe_create(&pipe_cfg, fwd, NULL, &error);
	if (next_pipe == NULL) {
		DOCA_DLOG_ERR("next pipe is null.");
		goto build_fail;
	}

	/* build fwd_miss */
	fwd_miss->type = DOCA_FLOW_FWD_PIPE;
	fwd_miss->next_pipe = next_pipe;

	/* add fwd_miss entry if type is DOCA_FLOW_FWD_PIPE */
	if (!doca_flow_pipe_add_entry(0, next_pipe, &match, &actions, &mon, fwd, 0, NULL, &error))
		goto build_fail;

	return fwd_miss;
build_fail:
	if (queues != NULL)
		free(queues);
	if (fwd != NULL)
		free(fwd);
	if (fwd_miss != NULL)
		free(fwd_miss);
	return NULL;
}

/*
 * Initializes DOCA flow port
 *
 * @port_cfg [in]: the port configuration needed to initialize the flow port
 * @return: a pointer to the created DOCA Flow port
 */
static struct doca_flow_port*
simple_fwd_init_doca_port(struct simple_fwd_port_cfg *port_cfg)
{
	char port_id_str[MAX_PORT_STR];
	struct doca_flow_port_cfg doca_cfg_port;
	struct doca_flow_port *port;
	struct doca_flow_error error = {0};

	snprintf(port_id_str, MAX_PORT_STR, "%d", port_cfg->port_id);
	doca_cfg_port.type = DOCA_FLOW_PORT_DPDK_BY_ID;
	doca_cfg_port.devargs = port_id_str;
	doca_cfg_port.priv_data_size = sizeof(struct simple_fwd_port_cfg);

	if (port_cfg->port_id >= SIMPLE_FWD_PORTS) {
		DOCA_LOG_ERR("port id exceeds max ports id:%d",
			SIMPLE_FWD_PORTS);
		return NULL;
	}
	port = doca_flow_port_start(&doca_cfg_port, &error);
	if (port == NULL) {
		DOCA_LOG_ERR("failed to start port %s", error.message);
		return NULL;
	}

	*((struct simple_fwd_port_cfg *)doca_flow_port_priv_data(port)) =
		*port_cfg;
	sw_rss_fwd_tbl_port[port_cfg->port_id] =
	    simple_fwd_build_rss_fwd(port_cfg->nb_queues);

	fwd_tbl_port[port_cfg->port_id] = simple_fwd_build_port_fwd(port_cfg);
	fwd_miss_tbl_port[port_cfg->port_id] =
		simple_fwd_build_port_fwd_miss(port_cfg, port);
	return port;
}

/*
 * Retrieve the port configurations as build by the application
 *
 * @port [in]: the port to retrieve the configurations for
 * @return: port configurations
 */
static struct simple_fwd_port_cfg*
simple_fwd_get_port_cfg(struct doca_flow_port *port)
{
	return (struct simple_fwd_port_cfg *)
		doca_flow_port_priv_data(port);
}

/*
 * Build forwarding component configiration
 *
 * @port_cfg [in]: the port configuration to build for the forwarding component
 * @return: a pointer to the built forwarding component
 */
static struct doca_flow_fwd*
simple_fwd_get_fwd(struct simple_fwd_port_cfg *port_cfg)
{
	uint16_t port_id = port_cfg->port_id;
	uint16_t pair_port_id = simple_fwd_get_pair_id(port_id);

	if (pair_port_id != UINT16_MAX)
		return fwd_tbl_port[pair_port_id];
	return sw_rss_fwd_tbl_port[port_id];
}

/*
 * Buil forwarding component configiration for the miss case
 *
 * @port_cfg [in]: the port configuration to build for the forwarding component
 * @return: a pointer to the built forwarding component in miss case
 */
static struct doca_flow_fwd *
simple_fwd_get_fwd_miss(struct simple_fwd_port_cfg *port_cfg)
{
	uint16_t port_id = port_cfg->port_id;
	return fwd_miss_tbl_port[port_id];
}

/*
 * Build encap flow action
 *
 * @encap [in]: a pointer to flow encap action to build or set
 */
static void
simple_fwd_build_eth_encap(struct doca_flow_encap_action *encap)
{
	/* build basic outer encap data, need fib to get the nexthop */
	SET_MAC_ADDR(encap->src_mac, 0xac, 0x3f, 0x56, 0x3d, 0x8a, 0x27);
	SET_MAC_ADDR(encap->dst_mac, 0x7c, 0xe2, 0xbd, 0x17, 0xa1, 0xc3);
	encap->src_ip.type = DOCA_FLOW_IP4_ADDR;
	encap->src_ip.ipv4_addr = BE_IPV4_ADDR(11, 12, 13, 14);
	encap->dst_ip.type = DOCA_FLOW_IP4_ADDR;
	encap->dst_ip.ipv4_addr = BE_IPV4_ADDR(21, 22, 23, 24);
}

/*
 * Build VXLAN pipe
 *
 * @port [in]: a pointer to port for which to build the pipe
 * @return: a pointer to the created VXLAN pipe
 */
static struct doca_flow_pipe*
simple_fwd_build_vxlan_pipe(struct doca_flow_port *port)
{
	struct doca_flow_pipe_cfg pipe_cfg = {0};
	struct simple_fwd_port_cfg *port_cfg;
	struct doca_flow_monitor monitor = {0};
	struct doca_flow_actions actions = {0};
	struct doca_flow_actions *actions_arr[NB_ACTION_ARRAY];
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_fwd *fwd;
	struct doca_flow_fwd *fwd_miss;

	port_cfg = simple_fwd_get_port_cfg(port);

	/* build match part */
	match.out_dst_ip.ipv4_addr = UINT32_MAX;
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = DOCA_PROTO_UDP;
	match.out_dst_port = RTE_BE16(DOCA_VXLAN_DEFAULT_PORT);
	match.tun.type = DOCA_FLOW_TUN_VXLAN;
	match.tun.vxlan_tun_id = UINT32_MAX;
	match.in_dst_ip.ipv4_addr = UINT32_MAX;
	match.in_src_ip.ipv4_addr = UINT32_MAX;
	match.in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_l4_type = DOCA_PROTO_TCP;
	match.in_src_port = UINT16_MAX;
	match.in_dst_port = UINT16_MAX;

	/* build action part */
	actions.meta.mark = 5;
	actions.decap = true;
	actions.mod_dst_ip.ipv4_addr = UINT32_MAX;
	actions.mod_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	/* for vxlan pipe, do decap + modify + vxlan encap*/
	if (simple_fwd_get_pair_id(port_cfg->port_id) != UINT16_MAX) {
		actions.has_encap = true;
		simple_fwd_build_eth_encap(&actions.encap);
		actions.encap.tun.type = DOCA_FLOW_TUN_VXLAN;
		actions.encap.tun.vxlan_tun_id = BUILD_VNI(0xcdab12);
	}
	/* build monitor part */
	monitor.flags = DOCA_FLOW_MONITOR_COUNT;
	monitor.flags |= DOCA_FLOW_MONITOR_AGING;

	/* build fwd part */
	fwd = simple_fwd_get_fwd(port_cfg);
	fwd_miss = simple_fwd_get_fwd_miss(port_cfg);

	/* create pipe */
	pipe_cfg.attr.name = "VXLAN_FWD";
	pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
	pipe_cfg.port = port;
	pipe_cfg.match = &match;
	actions_arr[0] = &actions;
	pipe_cfg.actions = actions_arr;
	pipe_cfg.attr.nb_actions = 1;
	pipe_cfg.monitor = &monitor;

	return doca_flow_pipe_create(&pipe_cfg, fwd, fwd_miss, &error);
}

/*
 * Build GRE pipe
 *
 * @port [in]: a pointer to port for which to build the pipe
 * @return: a pointer to the created GRE pipe
 */
static struct doca_flow_pipe*
simple_fwd_build_gre_pipe(struct doca_flow_port *port)
{
	struct doca_flow_pipe_cfg pipe_cfg = {0};
	struct simple_fwd_port_cfg *port_cfg;
	struct doca_flow_monitor monitor = {0};
	struct doca_flow_actions actions = {0};
	struct doca_flow_actions *actions_arr[NB_ACTION_ARRAY];
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};

	port_cfg = simple_fwd_get_port_cfg(port);
	/* build match part */
	match.out_dst_ip.ipv4_addr = UINT32_MAX;
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = IPPROTO_GRE;
	match.tun.type = DOCA_FLOW_TUN_GRE;
	match.tun.protocol = RTE_BE16(DOCA_ETHER_TYPE_IPV4);
	match.tun.gre_key = UINT32_MAX;
	match.in_dst_ip.ipv4_addr = UINT32_MAX;
	match.in_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_src_ip.ipv4_addr = UINT32_MAX;
	match.in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_l4_type = IPPROTO_TCP;
	match.in_src_port = UINT16_MAX;
	match.in_dst_port = UINT16_MAX;

	/* build action part */
	actions.decap = true;
	actions.mod_dst_ip.ipv4_addr = UINT32_MAX;
	actions.mod_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	/* for gre pipe, do decap + modify + vxlan encap*/
	if (simple_fwd_get_pair_id(port_cfg->port_id) != UINT16_MAX) {
		simple_fwd_build_eth_encap(&actions.encap);
		actions.has_encap = true;
		actions.encap.tun.type = DOCA_FLOW_TUN_VXLAN;
		actions.encap.tun.vxlan_tun_id = BUILD_VNI(0xcdab12);
	}
	/* build monitor part */
	monitor.flags = DOCA_FLOW_MONITOR_COUNT;
	monitor.flags |= DOCA_FLOW_MONITOR_AGING;

	/* create pipe */
	pipe_cfg.attr.name = "GRE_FWD";
	pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
	pipe_cfg.port = port;
	pipe_cfg.match = &match;
	actions_arr[0] = &actions;
	pipe_cfg.actions = actions_arr;
	pipe_cfg.attr.nb_actions = 1;
	pipe_cfg.monitor = &monitor;

	return doca_flow_pipe_create(&pipe_cfg, NULL, NULL, &error);
}

/*
 * Build GTP pipe
 *
 * @port [in]: a pointer to port for which to build the pipe
 * @return: a pointer to the created GTP pipe
 */
static struct doca_flow_pipe*
simple_fwd_build_gtp_pipe(struct doca_flow_port *port)
{
	struct doca_flow_pipe_cfg pipe_cfg = {0};
	struct doca_flow_monitor monitor = {0};
	struct doca_flow_actions actions = {0};
	struct doca_flow_actions *actions_arr[NB_ACTION_ARRAY];
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_pipe *gtp_pipe;

	/* build match part */
	match.out_dst_ip.ipv4_addr = 0xffffffff;
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = DOCA_PROTO_UDP;
	match.out_dst_port = DOCA_GTPU_PORT;
	match.tun.type = DOCA_FLOW_TUN_GTPU;
	match.tun.gtp_teid = 0xffffffff;
	match.in_dst_ip.ipv4_addr = 0xffffffff;
	match.in_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_src_ip.ipv4_addr = 0xffffffff;
	match.in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_l4_type = IPPROTO_TCP;
	match.in_src_port = 0xffff;

	/* build action part */
	actions.decap = true;
	actions.mod_dst_ip.ipv4_addr = 0xffffffff;
	actions.mod_dst_ip.type = DOCA_FLOW_IP4_ADDR;

	/* build monitor part */
	monitor.flags = DOCA_FLOW_MONITOR_COUNT;
	monitor.flags |= DOCA_FLOW_MONITOR_AGING;

	/* create pipe */
	pipe_cfg.attr.name = "GTP_FWD";
	pipe_cfg.attr.type = DOCA_FLOW_PIPE_BASIC;
	pipe_cfg.port = port;
	pipe_cfg.match = &match;
	actions_arr[0] = &actions;
	pipe_cfg.actions = actions_arr;
	pipe_cfg.attr.nb_actions = 1;
	pipe_cfg.monitor = &monitor;

	gtp_pipe = doca_flow_pipe_create(&pipe_cfg, NULL, NULL, &error);
	if (!gtp_pipe)
		DOCA_LOG_ERR("gtp pipe failed creation - %s (%u)", error.message, error.type);
	return gtp_pipe;
}

/*
 * Initialize simple FWD application DOCA Flow ports and pipes
 *
 * @port_cfg [in]: a pointer to the port configuration to initialize
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_init_ports_and_pipes(struct simple_fwd_port_cfg *port_cfg)
{
	struct doca_flow_error error = {0};
	struct doca_flow_port *port;
	struct doca_flow_pipe *pipe;

	struct doca_flow_cfg cfg = {
		.queues = port_cfg->nb_queues,
		.mode_args = "vnf",
		.resource.nb_meters = port_cfg->nb_meters,
		.resource.nb_counters = port_cfg->nb_counters,
	};
	uint16_t index;

	if (doca_flow_init(&cfg, &error)) {
		DOCA_LOG_ERR("failed to init doca:%s", error.message);
		return -1;
	}
	/* build doca port */
	for (index = 0; index < SIMPLE_FWD_PORTS; index++) {
		port_cfg->port_id = index;
		port = simple_fwd_init_doca_port(port_cfg);
		if (port == NULL) {
			DOCA_LOG_ERR("failed to start port %d %s",
				index, error.message);
			return -1;
		}
		simple_fwd_ins->port[index] = port;
		if (port_cfg->is_hairpin && simple_fwd_build_port_pair(index) < 0) {
			DOCA_LOG_ERR("failed to pair ports");
			return -1;
		}
	}

	/* build pipe on each port */
	for (index = 0; index < SIMPLE_FWD_PORTS; index++) {
		port = simple_fwd_ins->port[index];
		pipe = simple_fwd_build_gtp_pipe(port);
		if (pipe == NULL)
			return -1;
		simple_fwd_ins->pipe_gtp[index] = pipe;

		pipe = simple_fwd_build_gre_pipe(port);
		if (pipe == NULL)
			return -1;
		simple_fwd_ins->pipe_gre[index] = pipe;

		pipe = simple_fwd_build_vxlan_pipe(port);
		if (pipe == NULL)
			return -1;
		simple_fwd_ins->pipe_vxlan[index] = pipe;
		/* build control pipe and entries*/
		pipe = simple_fwd_build_control_pipe(port);
		if (!pipe)
			return -1;
		simple_fwd_ins->pipe_control[index] = pipe;

		if (simple_fwd_build_vxlan_control(simple_fwd_ins->pipe_vxlan[index], simple_fwd_ins->pipe_control[index]))
			return -1;

		if (simple_fwd_build_gre_control(simple_fwd_ins->pipe_gre[index], simple_fwd_ins->pipe_control[index]))
			return -1;

		if (simple_fwd_build_gtp_control(simple_fwd_ins->pipe_gtp[index], simple_fwd_ins->pipe_control[index]))
			return -1;
		}
	return 0;
}

/*
 * Initialize simple FWD application resources
 *
 * @p [in]: a pointer to the port configuration
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_init(void *p)
{
	struct simple_fwd_port_cfg *port_cfg;
	int ret = 0;

	port_cfg = (struct simple_fwd_port_cfg *)p;
	ret = simple_fwd_create_ins(port_cfg);
	if (ret)
		return ret;
	return simple_fwd_init_ports_and_pipes(port_cfg);
}

/*
 * Setting tunneling type in the match component
 *
 * @pinfo [in]: the packet info as represented in the application
 * @match [out]: match component to set the tunneling type in based on the packet info provided
 */
static inline void
simple_fwd_match_set_tun(struct simple_fwd_pkt_info *pinfo,
			 struct doca_flow_match *match)
{
	if (!pinfo->tun_type)
		return;
	match->tun.type = pinfo->tun_type;
	switch (match->tun.type) {
	case DOCA_FLOW_TUN_VXLAN:
		match->tun.vxlan_tun_id = pinfo->tun.vni;
		break;
	case DOCA_FLOW_TUN_GRE:
		match->tun.gre_key = pinfo->tun.gre_key;
		break;
	case DOCA_FLOW_TUN_GTPU:
		match->tun.gtp_teid = pinfo->tun.teid;
		break;
	default:
		DOCA_LOG_WARN("unsupport tun type:%u", match->tun.type);
		break;
	}
}

/*
 * Build match component
 *
 * @pinfo [in]: the packet info as represented in the application
 * @match [out]: the match component to build
 */
static void
simple_fwd_build_entry_match(struct simple_fwd_pkt_info *pinfo,
			     struct doca_flow_match *match)
{
	memset(match, 0x0, sizeof(*match));
	/* set match all fields, pipe will select which field to match */
	memcpy(match->out_dst_mac, simple_fwd_pinfo_outer_mac_dst(pinfo),
		DOCA_ETHER_ADDR_LEN);
	memcpy(match->out_src_mac, simple_fwd_pinfo_outer_mac_src(pinfo),
		DOCA_ETHER_ADDR_LEN);
	match->out_dst_ip.ipv4_addr = simple_fwd_pinfo_outer_ipv4_dst(pinfo);
	match->out_src_ip.ipv4_addr = simple_fwd_pinfo_outer_ipv4_src(pinfo);
	match->out_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match->out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match->out_src_port = simple_fwd_pinfo_outer_src_port(pinfo);
	match->out_dst_port = simple_fwd_pinfo_outer_dst_port(pinfo);
	match->out_l4_type = pinfo->outer.l4_type;
	if (!pinfo->tun_type)
		return;
	simple_fwd_match_set_tun(pinfo, match);
	match->in_dst_ip.ipv4_addr = simple_fwd_pinfo_inner_ipv4_dst(pinfo);
	match->in_src_ip.ipv4_addr = simple_fwd_pinfo_inner_ipv4_src(pinfo);
	match->in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match->in_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match->in_l4_type = pinfo->inner.l4_type;
	match->in_src_port = simple_fwd_pinfo_inner_src_port(pinfo);
	match->in_dst_port = simple_fwd_pinfo_inner_dst_port(pinfo);
}

/*
 * Build action component
 *
 * @pinfo [in]: the packet info as represented in the application
 * @action [out]: the action component to build
 */
static void
simple_fwd_build_entry_action(struct simple_fwd_pkt_info *pinfo,
			      struct doca_flow_actions *action)
{
	/* include all modify action cases*/
	SET_MAC_ADDR(action->mod_dst_mac, 0x0c, 0x42, 0xa1, 0x4b, 0xc5, 0x8c);
	action->mod_dst_ip.ipv4_addr = BE_IPV4_ADDR(18, 18, 18, 18);
	action->mod_dst_port = RTE_BE16(55555);

	/* set vxlan encap data, pipe will decide if do encap */
	action->has_encap = true;
	/*
	 * we have a basic encap data when create pipe, there we do
	 * some modify to test the modify encap and decap.
	 */
	memset(action->encap.src_mac, 0xaa, sizeof(action->encap.src_mac));
	memset(action->encap.dst_mac, 0xbb, sizeof(action->encap.src_mac));
	action->encap.src_ip.type = DOCA_FLOW_IP4_ADDR;
	action->encap.src_ip.ipv4_addr = BE_IPV4_ADDR(172, 18, 21, 22);
	action->encap.dst_ip.type = DOCA_FLOW_IP4_ADDR;
	action->encap.dst_ip.ipv4_addr = BE_IPV4_ADDR(155, 27, 12, 38);
	/*both vxlan/gre after decap will do vxlan encap.*/
	action->encap.tun.type = DOCA_FLOW_TUN_VXLAN;
	action->encap.tun.vxlan_tun_id = BUILD_VNI(0xadadad);
	action->meta.mark = 6;
}

/*
 * Build monitor component
 *
 * @pinfo [in]: the packet info as represented in the application
 * @user_ctx [in]: the user context, found in monitor structure
 * @monitor [out]: the monitor component to build
 */
static void
simple_fwd_build_entry_monitor(struct simple_fwd_pkt_info *pinfo,
			       void *user_ctx,
			       struct doca_flow_monitor *monitor)
{
	monitor->flags = DOCA_FLOW_MONITOR_COUNT;
	monitor->flags |= DOCA_FLOW_MONITOR_AGING;
	/* flows will be aged out in 5 - 60s */
	monitor->aging = (uint32_t)rte_rand() % 55 + 5;
	monitor->user_data = (uint64_t)user_ctx;
}

/*
 * Selects the pipe based on the tunneling type
 *
 * @pinfo [in]: the packet info as represented in the application
 * @return: a pointer for the selected pipe on success and NULL otherwise
 */
static struct doca_flow_pipe*
simple_fwd_select_pipe(struct simple_fwd_pkt_info *pinfo)
{
	if (pinfo->tun_type == DOCA_FLOW_TUN_GRE)
		return simple_fwd_ins->pipe_gre[pinfo->orig_port_id];
	if (pinfo->tun_type == DOCA_FLOW_TUN_VXLAN)
		return simple_fwd_ins->pipe_vxlan[pinfo->orig_port_id];
	if (pinfo->tun_type == DOCA_FLOW_TUN_GTPU)
		return simple_fwd_ins->pipe_gtp[pinfo->orig_port_id];
	return NULL;
}

/*
 * Selects the forwarding type based on tthe tunneling type and port configuration
 *
 * @pinfo [in]: the packet info as represented in the application
 * @return: a pointer for the selected forwarding component on success and NULL otherwise
 *
 * @NOTE: for vxlan case, test fwd is defined in pipe, for other cases, test fwd is defined in each entry.
 */
static struct doca_flow_fwd*
simple_fwd_select_fwd(struct simple_fwd_pkt_info *pinfo)
{
	struct doca_flow_port *port;
	struct simple_fwd_port_cfg *port_cfg;

	/*
	 * for vxlan case, test fwd is defined in pipe, for
	 * other cases, test fwd is defined in each entry.
	 */
	if (pinfo->tun_type == DOCA_FLOW_TUN_VXLAN)
		return NULL;

	port = simple_fwd_ins->port[pinfo->orig_port_id];
	port_cfg = simple_fwd_get_port_cfg(port);
	return simple_fwd_get_fwd(port_cfg);
}

/*
 * Adds new entry, with respect to the packet info, to the flow table
 *
 * @pinfo [in]: the packet info as represented in the application
 * @user_ctx [in]: user context
 * @age_sec [out]: Aging time for the created entry in seconds
 * @return: created entry pointer on success and NULL otherwise
 */
static struct doca_flow_pipe_entry*
simple_fwd_pipe_add_entry(struct simple_fwd_pkt_info *pinfo,
			  void *user_ctx, uint32_t *age_sec)
{
	struct doca_flow_match match;
	struct doca_flow_monitor monitor = {0};
	struct doca_flow_actions action = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_pipe *pipe;
	struct doca_flow_fwd *fwd = NULL;
	struct doca_flow_pipe_entry *entry;
	int count = 0;

	pipe = simple_fwd_select_pipe(pinfo);
	if (pipe == NULL) {
		DOCA_LOG_WARN("failed to select pipe on this packet");
		return NULL;
	}
	fwd = simple_fwd_select_fwd(pinfo);
	simple_fwd_build_entry_match(pinfo, &match);
	simple_fwd_build_entry_action(pinfo, &action);
	simple_fwd_build_entry_monitor(pinfo, user_ctx, &monitor);
	entry = doca_flow_pipe_add_entry(pinfo->pipe_queue,
		pipe, &match, &action, &monitor, fwd, DOCA_FLOW_NO_WAIT, NULL, &error);
	if (!entry) {
		DOCA_LOG_ERR("failed adding entry to pipe: error=%s, type=%u",
			     error.message, error.type);
		return NULL;
	}

	while (doca_flow_pipe_entry_get_status(entry) == DOCA_FLOW_ENTRY_STATUS_IN_PROCESS) {
		count++;
		if (count > MAX_TRY) {
			DOCA_LOG_ERR("failed adding entry to pipe: status is in_progress");
			goto error;
		}
		if (!doca_flow_entries_process(simple_fwd_ins->port[pinfo->orig_port_id],
				pinfo->pipe_queue, PULL_TIME_OUT, 0))
			continue;
	}
	if (doca_flow_pipe_entry_get_status(entry) == DOCA_FLOW_ENTRY_STATUS_ERROR) {
		DOCA_LOG_ERR("failed adding entry to pipe: status is error");
		goto error;
	}
	*age_sec = monitor.aging;
	return entry;

error:
	doca_flow_pipe_rm_entry(pinfo->pipe_queue, NULL, entry);
	return NULL;
}

/*
 * Currently we only can get the ft_entry ctx, but for the aging,
 * we need get the ft_entry pointer, add destroy the ft entry.
 */
#define GET_FT_ENTRY(ctx) \
	container_of(ctx, struct simple_fwd_ft_entry, user_ctx)

/*
 * Adds new flow, with respect to the packet info, to the flow table
 *
 * @pinfo [in]: the packet info as represented in the application
 * @ctx [in]: user context
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_handle_new_flow(struct simple_fwd_pkt_info *pinfo,
			   struct simple_fwd_ft_user_ctx **ctx)
{
	doca_error_t result;
	struct simple_fwd_pipe_entry *entry = NULL;
	struct simple_fwd_ft_entry *ft_entry;
	uint32_t age_sec;

	result = simple_fwd_ft_add_new(simple_fwd_ins->ft, pinfo, ctx);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_DBG("failed create new entry");
		return -1;
	}
	ft_entry = GET_FT_ENTRY(*ctx);
	entry = (struct simple_fwd_pipe_entry *)&(*ctx)->data[0];
	entry->pipe_queue = pinfo->pipe_queue;
	entry->hw_entry = simple_fwd_pipe_add_entry(pinfo, (void *)(*ctx), &age_sec);
	if (entry->hw_entry == NULL) {
		simple_fwd_ft_destroy_entry(simple_fwd_ins->ft, ft_entry);
		return -1;
	}
	simple_fwd_ft_update_age_sec(ft_entry, age_sec);
	simple_fwd_ft_update_expiration(ft_entry);
	entry->is_hw = true;

	return 0;
}

/*
 * Checks whether or not the received packet info is new.
 *
 * @pinfo [in]: the packet info as represented in the application
 * @return: true on success and false otherwise
 */
static bool
simple_fwd_need_new_ft(struct simple_fwd_pkt_info *pinfo)
{
	if (pinfo->outer.l3_type != IPV4) {
		DOCA_LOG_WARN("outer.l3_type %u not supported",
			pinfo->outer.l3_type);
		return false;
	}
	if ((pinfo->outer.l4_type != DOCA_PROTO_TCP) &&
		(pinfo->outer.l4_type != DOCA_PROTO_UDP) &&
		(pinfo->outer.l4_type != DOCA_PROTO_GRE)) {
		DOCA_LOG_WARN("outer.l4_type %u not supported",
			pinfo->outer.l4_type);
		return false;
	}
	return true;
}

/*
 * Adjust the mbuf pointer, to point on the packet's raw data
 *
 * @pinfo [in]: packet info representation  in the application
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_handle_packet(struct simple_fwd_pkt_info *pinfo)
{
	struct simple_fwd_ft_user_ctx *ctx = NULL;
	struct simple_fwd_pipe_entry *entry = NULL;

	if (!simple_fwd_need_new_ft(pinfo))
		return -1;
	if (simple_fwd_ft_find(simple_fwd_ins->ft, pinfo, &ctx) != DOCA_SUCCESS) {
		if (simple_fwd_handle_new_flow(pinfo, &ctx))
			return -1;
	}
	entry = (struct simple_fwd_pipe_entry *)&ctx->data[0];
	entry->total_pkts++;
	return 0;
}

/*
 * Handles aged flows
 *
 * @port_id [in]: port identifier of the port to handle its aged flows
 * @queue [in]: queue index of the queue to handle its aged flows
 */
static void
simple_fwd_handle_aging(uint32_t port_id, uint16_t queue)
{
#define MAX_HANDLING_TIME_MS 10	/*ms*/
	struct doca_flow_aged_query *entries;
	struct simple_fwd_ft_entry *ft_entry;
	int idex, ret;

	if (queue > simple_fwd_ins->nb_queues)
		return;
	entries = simple_fwd_ins->query_array[queue];
	ret = doca_flow_aging_handle(simple_fwd_ins->port[port_id], queue, MAX_HANDLING_TIME_MS,
		entries, AGE_QUERY_BURST);
	for (idex = 0; idex < ret; idex++) {
		ft_entry = GET_FT_ENTRY((void *)entries[idex].user_data);
		simple_fwd_ft_destroy_entry(simple_fwd_ins->ft, ft_entry);
	}
}


/*
 * Dump stats of the given port identifier
 *
 * @port_id [in]: port identifier to dump its stats
 * @return: 0 on success and non-zero value on failure
 */
static int
simple_fwd_dump_stats(uint32_t port_id)
{
	return simple_fwd_dump_port_stats(port_id, simple_fwd_ins->port[port_id]);
}

/*
 * Destroy application allocated resources
 *
 * @return: 0 on success and negative value otherwise
 */
static int
simple_fwd_destroy(void)
{
	simple_fwd_destroy_ins();
	doca_flow_destroy();
	return 0;
}

/* Stores all functions pointers used by the application */
struct app_vnf simple_fwd_vnf = {
	.vnf_init = &simple_fwd_init,			/* Simple Forward initialization resouces function pointer */
	.vnf_process_pkt = &simple_fwd_handle_packet,	/* Simple Forward packet processing function pointer */
	.vnf_flow_age = &simple_fwd_handle_aging,	/* Simple Forward aging handling function pointer */
	.vnf_dump_stats = &simple_fwd_dump_stats,	/* Simple Forward dumping stats function pointer */
	.vnf_destroy = &simple_fwd_destroy,		/* Simple Forward destroy allocated resources function pointer */
};

/*
 * Sets and stores all function pointers, in order to  call them later in the application
 */
struct app_vnf*
simple_fwd_get_vnf(void)
{
	return &simple_fwd_vnf;
}
