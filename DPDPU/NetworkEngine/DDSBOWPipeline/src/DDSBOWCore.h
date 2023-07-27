#ifndef DDS_BOW_CORE_H_
#define DDS_BOW_CORE_H_

#include <offload_rules.h>

#include "DDSTrafficDirecting.h"

/* DDS BOW configuration */
struct DDSBOWConfig {
	struct DDSBOWAppSignature AppSig;		/* Application signature */
	struct DDSBOWFwdRuleList FwdRules;			/* Forwarding rules */
	bool UseDPDK;				/* Use DPDK or not */
	uint32_t NumCores; 			/* Number of cores */
	char CoreList[16];			/* List of cores */
	char HostIPv4[16];			/* Host IPv4 address */
	uint16_t HostPort; 			/* Host port */
	char HostMac[18]; 			/* Host MAC address */
	char UdfPath[64];			/* Path to the offload predicate and function code */
};

/*
 * Registers all parameters with DOCA ARGP.
 *
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
RegisterDDSBOWParams(void);

#endif /* DDS_BOW_CORE_H_ */
