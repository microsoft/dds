#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_flow.h>

#include <doca_argp.h>
#include <doca_flow.h>
#include <doca_log.h>

#include <dpdk_utils.h>
#include <utils.h>

#include "AppEchoTCP.h"
#include "DDSBOWCore.h"

DOCA_LOG_REGISTER(DDS_BOW_Core);

/*
 * Callback function for the app signature
 *
 * @Param [in]: parameter indicates whther or not to set the app signature
 * @Config [out]: application configuration to set the app signature
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
AppSignatureCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;

    if (ParseAppSignature((char *)Param, &appConfig->AppSig) == 0) {
        char sigStr[64] = {0};
        GetAppSignatureStr(&appConfig->AppSig, sigStr);
        RTE_LOG(NOTICE, USER1,
                "PEPO parameter: application signature = %s\n",
                sigStr);

        return DOCA_SUCCESS;
    } else {
        DOCA_LOG_ERR("Invalid application signature");
        return DOCA_ERROR_INVALID_VALUE;
    }
}

/*
 * Callback function for the path to the offload predicate and function
 *
 * @Param [in]: parameter indicates whther or not to set the path
 * @Config [out]: application configuration to set the path
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
UdfPathCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;

    strcpy(appConfig->UdfPath, (char *)Param);
    RTE_LOG(NOTICE, USER1,
                "UDF code path = %s\n",
                appConfig->UdfPath);
    return DOCA_SUCCESS;
}

/*
 * Callback function for fwd rules
 *
 * @Param [in]: parameter indicates whther or not to set the app signature
 * @Config [out]: application configuration to set the app signature
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
FwdRulesCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;

    printf("Parsing fwd rules %s\n", (char*)Param);
    if (ParseFwdRules((char *)Param, &appConfig->FwdRules) == 0) {
        return DOCA_SUCCESS;
    } else {
        DOCA_LOG_ERR("Invalid fwd rules");
        return DOCA_ERROR_INVALID_VALUE;
    }
}

/*
 * Callback function for determining if DPDK is used
 *
 * @Param [in]: parameter indicates whther or not to set UseDPDK
 * @Config [out]: application configuration to set the app signature
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
UseDPDKCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;
    int useDPDK = *(int *)Param;
    appConfig->UseDPDK = useDPDK;
    RTE_LOG(NOTICE, USER1,
                "PEPO parameter: use DPDK = %u\n",
                useDPDK);

    return DOCA_SUCCESS;
}

/*
 * Callback function for setting number of cores
 *
 * @Param [in]: number of cores to set
 * @Config [out]: application configuration for setting the number of cores
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
NumCoresCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;
    int numCores = *(int *)Param;

    appConfig->NumCores = numCores;
    RTE_LOG(NOTICE, USER1,
                "Number of cores = %u\n",
                numCores);
    return DOCA_SUCCESS;
}

/*
 * Callback function for setting host port
 *
 * @Param [in]: host port to set
 * @Config [out]: application configuration for setting the host port
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
HostPortCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;
    int port = *(int *)Param;

    appConfig->HostPort = (uint16_t)port;
    RTE_LOG(NOTICE, USER1,
                "Host port = %d\n",
                port);
    return DOCA_SUCCESS;
}

/*
 * Callback function for setting core list
 *
 * @Param [in]: core list string to set
 * @Config [out]: application configuration for setting the core list string
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
CoreListCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;
    
    strcpy(appConfig->CoreList, (char *)Param);
    RTE_LOG(NOTICE, USER1,
                "Core list = %s\n",
                appConfig->CoreList);
    return DOCA_SUCCESS;
}

/*
 * Callback function for setting host IPv4 address
 *
 * @Param [in]: host IPv4 string to set
 * @Config [out]: application configuration for setting the host IPv4 string
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
HostIPv4Callback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;
    
    strcpy(appConfig->HostIPv4, (char *)Param);
    RTE_LOG(NOTICE, USER1,
                "Host IPv4 = %s\n",
                appConfig->HostIPv4);
    return DOCA_SUCCESS;
}

/*
 * Callback function for setting host Mac address
 *
 * @Param [in]: host Mac string to set
 * @Config [out]: application configuration for setting the host Mac string
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
static doca_error_t
HostMacCallback(
    void *Param,
    void *Config
) {
    struct DDSBOWConfig *appConfig = (struct DDSBOWConfig *)Config;
    
    strcpy(appConfig->HostMac, (char *)Param);
    RTE_LOG(NOTICE, USER1,
                "Host Mac = %s\n",
                appConfig->HostMac);
    return DOCA_SUCCESS;
}

/*
 * Registers all flags used by the application for DOCA argument parser, so that when parsing
 * it can be parsed accordingly
 * @return: DOCA_SUCCESS on success and DOCA_ERROR otherwise
 */
doca_error_t
RegisterDDSBOWParams() {
    doca_error_t result;
    struct doca_argp_param *appSignatureParam, *udfPathParam, *fwdRulesParam;
    struct doca_argp_param *useDPDKParam, *numCoresParam, *coreListParam;
    struct doca_argp_param *hostIPv4Param, *hostPortParam, *hostMacParam;

    /* Create and register HW offload param */
    result = doca_argp_param_create(&appSignatureParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(appSignatureParam, "s");
    doca_argp_param_set_long_name(appSignatureParam, "app-sig");
    doca_argp_param_set_description(appSignatureParam, "Set application signature");
    doca_argp_param_set_callback(appSignatureParam, AppSignatureCallback);
    doca_argp_param_set_type(appSignatureParam, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(appSignatureParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register offload predicate param */
    result = doca_argp_param_create(&udfPathParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(udfPathParam, "u");
    doca_argp_param_set_long_name(udfPathParam, "udf-path");
    doca_argp_param_set_description(udfPathParam, "Set path to UDFs");
    doca_argp_param_set_callback(udfPathParam, UdfPathCallback);
    doca_argp_param_set_type(udfPathParam, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(udfPathParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register fwd rules param */
    result = doca_argp_param_create(&fwdRulesParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(fwdRulesParam, "f");
    doca_argp_param_set_long_name(fwdRulesParam, "fwd-rules");
    doca_argp_param_set_description(fwdRulesParam, "Set forwarding rules");
    doca_argp_param_set_callback(fwdRulesParam, FwdRulesCallback);
    doca_argp_param_set_type(fwdRulesParam, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(fwdRulesParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register use DPDK param */
    result = doca_argp_param_create(&useDPDKParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(useDPDKParam, "d");
    doca_argp_param_set_long_name(useDPDKParam, "use-dpdk");
    doca_argp_param_set_arguments(useDPDKParam, "<num>");
    doca_argp_param_set_description(useDPDKParam, "Set if use DPDK");
    doca_argp_param_set_callback(useDPDKParam, UseDPDKCallback);
    doca_argp_param_set_type(useDPDKParam, DOCA_ARGP_TYPE_INT);
    result = doca_argp_register_param(useDPDKParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register number of cores param */
    result = doca_argp_param_create(&numCoresParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(numCoresParam, "c");
    doca_argp_param_set_long_name(numCoresParam, "num-cores");
    doca_argp_param_set_arguments(numCoresParam, "<num>");
    doca_argp_param_set_description(numCoresParam, "Set the number of cores to use");
    doca_argp_param_set_callback(numCoresParam, NumCoresCallback);
    doca_argp_param_set_type(numCoresParam, DOCA_ARGP_TYPE_INT);
    result = doca_argp_register_param(numCoresParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register core list param */
    result = doca_argp_param_create(&coreListParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(coreListParam, "l");
    doca_argp_param_set_long_name(coreListParam, "core-list");
    doca_argp_param_set_arguments(coreListParam, "<str>");
    doca_argp_param_set_description(coreListParam, "Set the list of cores to use");
    doca_argp_param_set_callback(coreListParam, CoreListCallback);
    doca_argp_param_set_type(coreListParam, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(coreListParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register host IPv4 param */
    result = doca_argp_param_create(&hostIPv4Param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(hostIPv4Param, "i");
    doca_argp_param_set_long_name(hostIPv4Param, "host-ipv4");
    doca_argp_param_set_arguments(hostIPv4Param, "<str>");
    doca_argp_param_set_description(hostIPv4Param, "Set host IPv4 address");
    doca_argp_param_set_callback(hostIPv4Param, HostIPv4Callback);
    doca_argp_param_set_type(hostIPv4Param, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(hostIPv4Param);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register host MAC param */
    result = doca_argp_param_create(&hostMacParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(hostMacParam, "h");
    doca_argp_param_set_long_name(hostMacParam, "host-mac");
    doca_argp_param_set_arguments(hostMacParam, "<str>");
    doca_argp_param_set_description(hostMacParam, "Set host MAC address");
    doca_argp_param_set_callback(hostMacParam, HostMacCallback);
    doca_argp_param_set_type(hostMacParam, DOCA_ARGP_TYPE_STRING);
    result = doca_argp_register_param(hostMacParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Create and register host port param */
    result = doca_argp_param_create(&hostPortParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to create ARGP param: %s", doca_get_error_string(result));
        return result;
    }
    doca_argp_param_set_short_name(hostPortParam, "p");
    doca_argp_param_set_long_name(hostPortParam, "host-port");
    doca_argp_param_set_arguments(hostPortParam, "<num>");
    doca_argp_param_set_description(hostPortParam, "Set host IPv4 port");
    doca_argp_param_set_callback(hostPortParam, HostPortCallback);
    doca_argp_param_set_type(hostPortParam, DOCA_ARGP_TYPE_INT);
    result = doca_argp_register_param(hostPortParam);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register program param: %s", doca_get_error_string(result));
        return result;
    }

    /* Register version callback for DOCA SDK & RUNTIME */
    result = doca_argp_register_version_callback(sdk_version_callback);
    if (result != DOCA_SUCCESS) {
        DOCA_LOG_ERR("Failed to register version callback: %s", doca_get_error_string(result));
        return result;
    }
    return DOCA_SUCCESS;
}
