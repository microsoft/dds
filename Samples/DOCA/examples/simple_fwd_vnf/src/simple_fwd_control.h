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

#ifndef SIMPLE_FWD_CONTROL_H_
#define SIMPLE_FWD_CONTROL_H_

#include <stdint.h>
#include <stdbool.h>

#include <doca_flow.h>

/*
 * Adds VXLAN entry to the control pipe
 *
 * @next_pipe [in]: the pipe to forward to the matched VXLAN packets
 * @control_pipe [in]: the control pipe to add the entry to
 * @return: 0 on success and negative value otherwise
 */
int
simple_fwd_build_vxlan_control(struct doca_flow_pipe *next_pipe, struct doca_flow_pipe *control_pipe);

/*
 * Adds GTP entry to the control pipe
 *
 * @next_pipe [in]: the pipe to forward to the matched GTP packets
 * @control_pipe [in]: the control pipe to add the entry to
 * @return: 0 on success and negative value otherwise
 */
int
simple_fwd_build_gtp_control(struct doca_flow_pipe *next_pipe, struct doca_flow_pipe *control_pipe);

/*
 * Adds GRE entry to the control pipe
 *
 * @next_pipe [in]: the pipe to forward to the matched GRE packets
 * @control_pipe [in]: the control pipe to add the entry to
 * @return: 0 on success and negative value otherwise
 */
int
simple_fwd_build_gre_control(struct doca_flow_pipe *next_pipe, struct doca_flow_pipe *control_pipe);

/*
 * Builds the control pipe for the given port
 *
 * @port [in]: DOCA flow port to build the control pipe for
 * @return: a pointer to the created control pipe, NULL otherwise
 */
struct doca_flow_pipe *
simple_fwd_build_control_pipe(struct doca_flow_port *port);

#endif /* SIMPLE_FWD_CONTROL_H_ */
