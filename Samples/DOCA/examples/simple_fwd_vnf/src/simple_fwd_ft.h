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

#ifndef SIMPLE_FWD_FT_H_
#define SIMPLE_FWD_FT_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <rte_mbuf.h>

#include "simple_fwd_pkt.h"

struct simple_fwd_ft;		/* Flow table */
struct simple_fwd_ft_key; 	/* Keys flow table */

/* Flow table user context */
struct simple_fwd_ft_user_ctx {
	uint32_t fid;		/* Forwarding id, used for flow table */
	uint8_t data[0];	/* A pointer to the flow entry in the flow table as represented in the application */
};

/* Simple FWD flow entry representation in flow table */
struct simple_fwd_ft_entry {
	LIST_ENTRY(simple_fwd_ft_entry) next;	/* Entry pointers in the list. */
	struct simple_fwd_ft_key key;		/* Generated key of the entry */
	uint64_t expiration;			/* Expiration time */
	uint32_t age_sec;			/* Age time in seconds */
	uint64_t last_counter;			/* Last HW counter of matched packets */
	uint64_t sw_ctr;			/* SW counter of matched packets */
	uint8_t hw_off;				/* Whether or not the entry was HW offloaded */
	uint16_t buckets_index;			/* The index of the entry in the buckets */
	struct simple_fwd_ft_user_ctx user_ctx;	/* A context that can be stored and used */
};
LIST_HEAD(simple_fwd_ft_entry_head, simple_fwd_ft_entry); /* Head of the list of the flows as represented in the application */

/* Extracting the source IPv4 address for key generating */
#define simple_fwd_ft_key_get_ipv4_src(inner, pinfo)	\
	(inner ? simple_fwd_pinfo_inner_ipv4_src(pinfo)		\
	       : simple_fwd_pinfo_outer_ipv4_src(pinfo))

/* Extracting the destination IPv4 address for key generating */
#define simple_fwd_ft_key_get_ipv4_dst(inner, pinfo)	\
	(inner ? simple_fwd_pinfo_inner_ipv4_dst(pinfo)		\
	       : simple_fwd_pinfo_outer_ipv4_dst(pinfo))

/* Extracting the source port for key generating */
#define simple_fwd_ft_key_get_src_port(inner, pinfo)	\
	(inner ? simple_fwd_pinfo_inner_src_port(pinfo)		\
	       : simple_fwd_pinfo_outer_src_port(pinfo))

/* Extracting the destination port for key generating */
#define simple_fwd_ft_key_get_dst_port(inner, pinfo)	\
	(inner ? simple_fwd_pinfo_inner_dst_port(pinfo)		\
	       : simple_fwd_pinfo_outer_dst_port(pinfo))

/*
 * Create new flow table
 *
 * @nb_flows [in]: number of flows
 * @user_data_size [in]: private data for user
 * @return: pointer to new allocated flow table and NULL otherwise
 */
struct simple_fwd_ft *
simple_fwd_ft_create(int nb_flows, uint32_t user_data_size,
	void (*simple_fwd_aging_cb)(struct simple_fwd_ft_user_ctx *ctx),
	void (*simple_fwd_aging_hw_cb)(void),
	bool age_thread);

/*
 * Destroy flow table
 *
 * @ft [in]: flow table to destroy
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
simple_fwd_ft_destroy(struct simple_fwd_ft *ft);

/*
 * Add entry to flow table
 *
 * @ft [in]: flow table to add the entry to
 * @pinfo [in]: the packet info for generating the key for the new entry to add
 * @ctx [in]: simple fwd user context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
simple_fwd_ft_add_new(struct simple_fwd_ft *ft,
		      struct simple_fwd_pkt_info *pinfo,
		      struct simple_fwd_ft_user_ctx **ctx);

/*
 * Find if there is an existing entry matching the given packet's info
 *
 * @ft [in]: flow table to search in
 * @pinfo [in]: the packet info for generating the key for the search
 * @ctx [in]: simple fwd user context
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
simple_fwd_ft_find(struct simple_fwd_ft *ft,
		   struct simple_fwd_pkt_info *pinfo,
		   struct simple_fwd_ft_user_ctx **ctx);

/*
 * Remove entry from flow table if found
 *
 * @ft [in]: flow table to remove the entry from
 * @ft_entry [in]: the pointer to the entry to remove
 */
void
simple_fwd_ft_destroy_entry(struct simple_fwd_ft *ft,
			   struct simple_fwd_ft_entry *ft_entry);

/*
 * Update aging time of entry in the flow table
 *
 * @e [in]: pointer to the entry to update the age time for
 * @age_sec [in]: time of aging to set for the entry
 */
void
simple_fwd_ft_update_age_sec(struct simple_fwd_ft_entry *e, uint32_t age_sec);

/*
 * Updates the expiration time of a given entry in the flow table
 *
 * @e [in]: a pointer to the entry to update the eexpiration time for
 */
void
simple_fwd_ft_update_expiration(struct simple_fwd_ft_entry *e);

#endif /* SIMPLE_FWD_FT_H_ */
