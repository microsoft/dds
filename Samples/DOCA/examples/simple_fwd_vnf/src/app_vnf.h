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
#ifndef APP_VNF_H_
#define APP_VNF_H_

#include <stdint.h>

/* Holder for the packed info */
struct simple_fwd_pkt_info;

/* Holder for all functions pointers needed */
struct app_vnf {
	int (*vnf_init)(void *p);					/* A function pointer for initializing all application resources */
	int (*vnf_process_pkt)(struct simple_fwd_pkt_info *pinfo);	/* A function pointer for processing the packets */
	void (*vnf_flow_age)(uint32_t port_id, uint16_t queue);		/* A function pointer for the aging handling */
	int (*vnf_dump_stats)(uint32_t port_id);			/* A function pointer for dumping the stats */
	int (*vnf_destroy)(void);					/* A function pointer for destroying all allocated application resources */
};

#endif /* APP_VNF_H_ */
