/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
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

#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#include <doca_comm_channel.h>
#include <doca_log.h>

#include "common.h"

#define MAX_MSG_SIZE 4080		/* Comm Channel maximum message size */
#define CC_MAX_QUEUE_SIZE 10		/* Maximum amount of message in queue */

DOCA_LOG_REGISTER(CC_SERVER);

static bool end_sample;		/* Shared variable to allow for a proper shutdown */

/*
 * Signals handler function
 * Once a signal is received by the application, update the shared variable between the send/receive threads and quit
 *
 * @signum [in]: signal number
 */
static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		DOCA_LOG_INFO("Signal %d received, preparing to exit...", signum);
		end_sample = true;
	}
}

/*
 * Run DOCA Comm Channel server sample
 *
 * @server_name [in]: Server Name
 * @dev_pci_addr [in]: PCI address for device
 * @rep_pci_addr [in]: PCI address for device representor
 * @text [in]: Server message
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
create_comm_channel_server(const char *server_name, struct doca_pci_bdf *dev_pci_addr, struct doca_pci_bdf *rep_pci_addr, const char *text)
{
	doca_error_t result;
	char rcv_buf[MAX_MSG_SIZE];
	size_t msg_len;

	/* Define Comm Channel endpoint attributes */
	struct doca_comm_channel_ep_t *ep = NULL;
	struct doca_comm_channel_addr_t *peer_addr = NULL;
	struct doca_dev *cc_dev = NULL;
	struct doca_dev_rep *cc_dev_rep = NULL;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Create Comm Channel endpoint */
	result = doca_comm_channel_ep_create(&ep);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create Comm Channel endpoint: %s", doca_get_error_string(result));
		return result;
	}

	/* Open DOCA device according to the given PCI address */
	result = open_doca_device_with_pci(dev_pci_addr, NULL, &cc_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to open Comm Channel DOCA device based on PCI address");
		doca_comm_channel_ep_destroy(ep);
		return result;
	}

	/* Open DOCA device representor according to the given PCI address */
	result = open_doca_device_rep_with_pci(cc_dev, DOCA_DEV_REP_FILTER_NET, rep_pci_addr, &cc_dev_rep);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to open Comm Channel DOCA device representor based on PCI address");
		doca_comm_channel_ep_destroy(ep);
		doca_dev_close(cc_dev);
		return result;
	}

	/* Set all endpoint properties */
	result = doca_comm_channel_ep_set_device(ep, cc_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set device property");
		goto destroy_cc;
	}

	result = doca_comm_channel_ep_set_max_msg_size(ep, MAX_MSG_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set max_msg_size property");
		goto destroy_cc;
	}

	result = doca_comm_channel_ep_set_send_queue_size(ep, CC_MAX_QUEUE_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set snd_queue_size property");
		goto destroy_cc;
	}

	result = doca_comm_channel_ep_set_recv_queue_size(ep, CC_MAX_QUEUE_SIZE);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set rcv_queue_size property");
		goto destroy_cc;
	}

	result = doca_comm_channel_ep_set_device_rep(ep, cc_dev_rep);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set DOCA device representor property");
		goto destroy_cc;
	}

	/* Start listen for new connections */
	result = doca_comm_channel_ep_listen(ep, server_name);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Comm Channel server couldn't start listening: %s", doca_get_error_string(result));
		goto destroy_cc;
	}

	DOCA_LOG_INFO("Server started Listening, waiting for new connections");

	/* Wait to receive and respond messages from the client */
	while(true) {
		msg_len = MAX_MSG_SIZE;
		while ((result = doca_comm_channel_ep_recvfrom(ep, (void *)rcv_buf, &msg_len, DOCA_CC_MSG_FLAG_NONE,
							       &peer_addr)) == DOCA_ERROR_AGAIN) {
			if (end_sample) {
				result = DOCA_ERROR_UNEXPECTED;
				break;
			}
			msg_len = MAX_MSG_SIZE;
		}
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Message was not received: %s", doca_get_error_string(result));
			goto destroy_cc;
		}

		/* Send a response of the same size to client */
		while ((result = doca_comm_channel_ep_sendto(ep, text, msg_len, DOCA_CC_MSG_FLAG_NONE, peer_addr)) ==
		       DOCA_ERROR_AGAIN) {
			if (end_sample) {
				result = DOCA_ERROR_UNEXPECTED;
				break;
			}
		}
		if (result != DOCA_SUCCESS)
			DOCA_LOG_WARN("Response was not sent successfully: %s", doca_get_error_string(result));
	}

destroy_cc:

	/* Disconnect from current connection */
	if (peer_addr != NULL)
		doca_comm_channel_ep_disconnect(ep, peer_addr);

	/* Destroy Comm Channel endpoint */
	doca_comm_channel_ep_destroy(ep);

	/* Destroy Comm Channel DOCA device representor */
	doca_dev_rep_close(cc_dev_rep);

	/* Destroy Comm Channel DOCA device */
	doca_dev_close(cc_dev);

	return result;
}
