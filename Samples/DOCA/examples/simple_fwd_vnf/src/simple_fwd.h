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

#ifndef SIMPLE_FWD_H_
#define SIMPLE_FWD_H_

#include <stdint.h>
#include <stdbool.h>

#include <doca_flow.h>

#include "simple_fwd_pkt.h"
#include "simple_fwd_port.h"

#define SIMPLE_FWD_PORTS (2)		/* Number of ports used by the application */
#define SIMPLE_FWD_MAX_FLOWS (8096)	/* Maximum number of flows used/added by the application at a given time */

/* Application resources, such as flow table, pipes and hairpin peers */
struct simple_fwd_app {
	struct simple_fwd_ft *ft;				/* Flow table, used for stprng flows */
	uint16_t hairpin_peer[SIMPLE_FWD_PORTS];		/* Binded pair ports array*/
	struct doca_flow_port *port[SIMPLE_FWD_PORTS];		/* DOCA Flow ports array used by the application */
	struct doca_flow_pipe *pipe_vxlan[SIMPLE_FWD_PORTS];	/* VXLAN pipe of each port */
	struct doca_flow_pipe *pipe_gre[SIMPLE_FWD_PORTS];	/* GRE pipe of each port */
	struct doca_flow_pipe *pipe_gtp[SIMPLE_FWD_PORTS];	/* GTP pipe of each port */
	struct doca_flow_pipe *pipe_control[SIMPLE_FWD_PORTS];	/* control pipe of each port */
	uint16_t nb_queues;					/* flow age query item buffer */
	struct doca_flow_aged_query *query_array[0];		/* buffer for flow aged query items */
};

/* Simple FWD flow entry representation */
struct simple_fwd_pipe_entry {
	bool is_hw;				/* Wether the entry in HW or not */
	uint64_t total_pkts;			/* Total number of packets matched the flow */
	uint64_t total_bytes;			/* Total number of bytes matched the flow */
	uint16_t pipe_queue;			/* Pipe queue of the flow entry */
	struct doca_flow_pipe_entry *hw_entry;	/* a pointer for the flow entry in hw */
};

/*
 * fills struct app_vnf with init/destroy, process and other needed pointer functions.
 *
 * @return: a pointer to struct app_vnf which contains all needed pointer functions
 */
struct app_vnf* simple_fwd_get_vnf(void);

#endif /* SIMPLE_FWD_H_ */
