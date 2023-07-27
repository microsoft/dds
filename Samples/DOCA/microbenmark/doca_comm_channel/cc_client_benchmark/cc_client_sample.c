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
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include <doca_comm_channel.h>
#include <doca_log.h>

#include "common.h"
#include "latency_helpers.h"

#define MAX_MSG_SIZE 4080		/* Comm Channel maximum message size */
#define CC_MAX_QUEUE_SIZE 10		/* Maximum amount of message in queue */
#define BILLION  1000000000L

DOCA_LOG_REGISTER(CC_CLIENT);

static bool end_sample;		/* Shared variable to allow for a proper shutdown */

/*
 * Signal handler
 *
 * @signum [in]: Signal number to handle
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
 * Run DOCA Comm Channel client sample
 *
 * @server_name [in]: Server Name
 * @dev_pci_addr [in]: PCI address for device
 * @text [in]: Client message
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
create_comm_channel_client(const char *server_name, struct doca_pci_bdf *dev_pci_addr, const char *text, int msg_size, int test_rounds)
{
	doca_error_t result;
	char rcv_buf[MAX_MSG_SIZE];
	size_t msg_len;

	/* Define Comm Channel endpoint attributes */
	struct doca_comm_channel_ep_t *ep = NULL;
	struct doca_comm_channel_addr_t *peer_addr = NULL;
	struct doca_dev *cc_dev = NULL;

	struct timespec start, stop;
	double roundtrip, total_latency;
	double *latencies;

	/* Signal the while loop to stop */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Create Comm Channel endpoint */
	result = doca_comm_channel_ep_create(&ep);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create Comm Channel client endpoint: %s", doca_get_error_string(result));
		return result;
	}

	/* Open DOCA device according to the given PCI address */
	result = open_doca_device_with_pci(dev_pci_addr, NULL, &cc_dev);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to open Comm Channel DOCA device based on PCI address");
		doca_comm_channel_ep_destroy(ep);
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

	/* Connect to server node */
	result = doca_comm_channel_ep_connect(ep, server_name, &peer_addr);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Couldn't establish a connection with the server: %s", doca_get_error_string(result));
		goto destroy_cc;
	}

	/* Make sure peer address is valid */
	while ((result = doca_comm_channel_peer_addr_update_info(peer_addr)) == DOCA_ERROR_CONNECTION_INPROGRESS) {
		if (end_sample) {
			result = DOCA_ERROR_UNEXPECTED;
			break;
		}
		usleep(1);
	}
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to validate the connection with the DPU: %s", doca_get_error_string(result));
		return result;
	}

	DOCA_LOG_INFO("Connection to server was established successfully");

	// result = doca_comm_channel_ep_sendto(ep, text, (size_t)msg_size, DOCA_CC_MSG_FLAG_NONE, peer_addr);
	DOCA_LOG_INFO("Benchmarking %d bytes for %d rounds", msg_size, test_rounds);
	total_latency = 0;
	latencies = malloc(sizeof(double) * test_rounds);
	for (size_t i = 0; i != test_rounds; i++) {
		latencies[i] = 0;
	}
	for (size_t i = 0; i != test_rounds; i++) {
		/* Send client message */
		if(clock_gettime( CLOCK_REALTIME, &start) == -1) {
			perror("clock gettime");
			return -1;
		}
		while ((result = doca_comm_channel_ep_sendto(ep, text, (size_t)msg_size, DOCA_CC_MSG_FLAG_NONE, peer_addr)) ==
		       DOCA_ERROR_AGAIN) {
			if (end_sample) {
				result = DOCA_ERROR_UNEXPECTED;
				break;
			}
		}
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Message was not sent: %s", doca_get_error_string(result));
			goto destroy_cc;
		}

		/* Receive server's response */
		msg_len = msg_size;
		result = doca_comm_channel_ep_recvfrom(ep, (void *)rcv_buf, &msg_len, DOCA_CC_MSG_FLAG_NONE, &peer_addr);

		while ((result = doca_comm_channel_ep_recvfrom(ep, (void *)rcv_buf, &msg_len, DOCA_CC_MSG_FLAG_NONE,
							       &peer_addr)) == DOCA_ERROR_AGAIN) {
			if (end_sample) {
				result = DOCA_ERROR_UNEXPECTED;
				break;
			}
			msg_len = msg_size;
		}
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Message was not received: %s", doca_get_error_string(result));
			goto destroy_cc;
		}

		if(clock_gettime( CLOCK_REALTIME, &stop) == -1) {
			perror("clock gettime");
			return -1;
		}

		roundtrip = (stop.tv_sec - start.tv_sec) * (double)BILLION
			     + (double)(stop.tv_nsec - start.tv_nsec);
		roundtrip = roundtrip / 1000.0;
		latencies[i] = roundtrip;
		total_latency += roundtrip;
	}

	Statistics LatencyStats;
	Percentiles PercentileStats;
	GetStatistics(latencies, (size_t)test_rounds, &LatencyStats, &PercentileStats);
	printf(
		"Result for %d requests of %d bytes (%.2lf seconds): %.2lf RPS, Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
		test_rounds,
		msg_size,
		(total_latency / 1000000),
		(test_rounds / total_latency * 1000000),
		LatencyStats.Min,
		LatencyStats.Max,
		PercentileStats.P50,
		PercentileStats.P90,
		PercentileStats.P99,
		PercentileStats.P99p9,
		PercentileStats.P99p99,
		LatencyStats.StandardError);

	// DOCA_LOG_INFO("Average roundtrip latency: %.2lf us", (total_latency / test_rounds));

	free(latencies);

destroy_cc:

	/* Disconnect from current connection */
	if (peer_addr != NULL)
		result = doca_comm_channel_ep_disconnect(ep, peer_addr);

	/* Destroy Comm Channel endpoint */
	doca_comm_channel_ep_destroy(ep);

	/* Destroy Comm Channel DOCA device */
	doca_dev_close(cc_dev);

	return result;
}
