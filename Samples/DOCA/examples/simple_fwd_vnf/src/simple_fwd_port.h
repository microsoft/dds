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

#ifndef SIMPLE_FWD_PORT_H_
#define SIMPLE_FWD_PORT_H_

#define NUM_OF_PORTS (2) /* Number of ports used */

/* Simple FWD application's port configuration */
struct simple_fwd_port_cfg {
	uint16_t port_id;	/* Port identifier for the application */
	uint16_t nb_queues;	/* Number of initialized queues descriptors (RX/TX) of the port */
	uint32_t nb_meters;	/* Number of meters of the port used by the application */
	uint32_t nb_counters;	/* Number of counters for the port used by the application */
	uint16_t is_hairpin;	/* Whether or not hairpin queues is neede for this port */
	bool age_thread;	/* Whether or not aging is handled by a dedicated thread */
};

/*
 * Dump port stats
 *
 * @port_id [in]: Port identifier
 * @port [in]: DOCA flow port to dump the stats for
 * @return: 0 on success and non-zero value on failure
 */
int
simple_fwd_dump_port_stats(uint16_t port_id, struct doca_flow_port *port);

#endif /* SIMPLE_FWD_PORT_H_ */
