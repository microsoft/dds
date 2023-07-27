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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <rte_mbuf.h>

#include <doca_flow.h>
#include <doca_log.h>

#include "app_vnf.h"
#include "simple_fwd.h"
#include "simple_fwd_control.h"

DOCA_LOG_REGISTER(SIMPLE_FWD);

struct doca_flow_fwd c_fwd;				/* FWD component for adding entries to the control pipe */

/*
 * Build forwarding for the control pipe
 *
 * @next_pipe [in]: the pipe to forward to the matched packets
 * @return: a pointer to the built FWD component, NULL otherwise
 */
static struct doca_flow_fwd *
simple_fwd_build_control_fwd(struct doca_flow_pipe *next_pipe)
{
	memset(&c_fwd, 0, sizeof(struct doca_flow_fwd));
	c_fwd.type = DOCA_FLOW_FWD_PIPE;
	c_fwd.next_pipe = next_pipe;
	return &c_fwd;
}

int
simple_fwd_build_vxlan_control(struct doca_flow_pipe *next_pipe, struct doca_flow_pipe *control_pipe)
{
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_fwd *fwd = NULL;
	uint8_t pri;

	/* build match part */
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = IPPROTO_UDP;
	match.out_dst_port = rte_cpu_to_be_16(DOCA_VXLAN_DEFAULT_PORT);
	match.tun.type = DOCA_FLOW_TUN_VXLAN;
	match.in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_l4_type = IPPROTO_TCP;

	/* build fwd part */
	fwd = simple_fwd_build_control_fwd(next_pipe);
	pri = 1;
	if (!doca_flow_pipe_control_add_entry(0, pri, control_pipe, &match, NULL,
			NULL, NULL, NULL, fwd, &error))
		return -1;

	return 0;
}

int
simple_fwd_build_gtp_control(struct doca_flow_pipe *next_pipe, struct doca_flow_pipe *control_pipe)
{
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_fwd *fwd = NULL;
	uint8_t pri;

	/* build match part */
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = DOCA_PROTO_UDP;
	match.out_dst_port = DOCA_GTPU_PORT;
	match.tun.type = DOCA_FLOW_TUN_GTPU;
	match.in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_l4_type = IPPROTO_TCP;

	/* build fwd part */
	fwd = simple_fwd_build_control_fwd(next_pipe);
	pri = 1;
	if (!doca_flow_pipe_control_add_entry(0, pri, control_pipe, &match, NULL,
			NULL, NULL, NULL, fwd, &error))
		return -1;
	return 0;
}

int
simple_fwd_build_gre_control(struct doca_flow_pipe *next_pipe, struct doca_flow_pipe *control_pipe)
{
	struct doca_flow_match match = {0};
	struct doca_flow_error error = {0};
	struct doca_flow_fwd *fwd = NULL;
	uint8_t pri;

	/* build match part */
	match.out_dst_ip.type = DOCA_FLOW_IP4_ADDR;
	match.out_l4_type = IPPROTO_GRE;
	match.tun.type = DOCA_FLOW_TUN_GRE;
	match.in_src_ip.type = DOCA_FLOW_IP4_ADDR;
	match.in_l4_type = IPPROTO_TCP;

	/* build fwd part */
	fwd = simple_fwd_build_control_fwd(next_pipe);
	pri = 1;
	if (!doca_flow_pipe_control_add_entry(0, pri, control_pipe, &match, NULL,
			NULL, NULL, NULL, fwd, &error))
		return -1;
	return 0;
}

struct doca_flow_pipe *
simple_fwd_build_control_pipe(struct doca_flow_port *port)
{
	struct doca_flow_pipe_cfg pipe_cfg = {0};
	struct doca_flow_error error = {0};

	/* create pipe */
	pipe_cfg.attr.name = "CONTROL_PIPE";
	pipe_cfg.attr.type = DOCA_FLOW_PIPE_CONTROL;
	pipe_cfg.port = port;
	pipe_cfg.attr.is_root = true;

	return doca_flow_pipe_create(&pipe_cfg, NULL, NULL,  &error);
}


