#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <rdma/rdma_cma.h>
#include <infiniband/ib.h>

#include "latency_helpers.h"
#include "affinity.h"

#define SERVER_IP "192.168.200.32"
#define LISTEN_PORT 4242
#define PING_DATA_SIZE 163840000
#define PING_COUNT 100
#define LISTEN_BACKLOG 3
#define RESOLVE_TIMEOUT_MS 2000

struct rping_rdma_info {
    uint64_t buf;
    uint32_t rkey;
    uint32_t size;
};

//
// Default max buffer size for IO...
//
//
#define RPING_BUFSIZE 64*1024
#define RPING_SQ_DEPTH 16

//
// Control block struct
//
//
struct rping_cb {
    struct ibv_comp_channel *channel;
    struct ibv_cq *cq;
    struct ibv_pd *pd;
    struct ibv_qp *qp;

    struct ibv_recv_wr rq_wr;    /* recv work request record */
    struct ibv_sge recv_sgl;    /* recv single SGE */
    struct rping_rdma_info recv_buf;/* malloc'd buffer */
    struct ibv_mr *recv_mr;        /* MR associated with this buffer */

    struct ibv_send_wr sq_wr;    /* send work requrest record */
    struct ibv_sge send_sgl;
    struct rping_rdma_info send_buf;/* single send buf */
    struct ibv_mr *send_mr;

    struct ibv_send_wr rdma_sq_wr;    /* rdma work request record */
    struct ibv_sge rdma_sgl;    /* rdma single SGE */
    char *rdma_buf;            /* used as rdma sink */
    struct ibv_mr *rdma_mr;

    uint32_t remote_rkey;        /* remote guys RKEY */
    uint64_t remote_addr;        /* remote guys TO */
    uint32_t remote_len;        /* remote guys LEN */

    char *start_buf;        /* rdma read src */
    struct ibv_mr *start_mr;

    uint16_t port;            /* dst port in NBO */
    uint32_t addr;            /* dst addr in NBO */
    char addr_str[16];            /* dst addr string */
    int count;            /* ping count */
    int progress;      /* ping progress */
    int size;            /* ping data size */
    int validate;            /* validate ping data */
    double *latencies;     /* vector for recording latencies */
    unsigned long start_time_us;
    unsigned long end_time_us;

    /* CM stuff */
    struct rdma_event_channel *cm_channel;
    struct rdma_cm_id *cm_id;    /* connection on client side,*/
                                 /* listener on server side. */
    struct rdma_cm_id *child_cm_id;    /* connection on server side */
    uint8_t is_connected;
};

static void rping_setup_wr(struct rping_cb *cb)
{
    cb->recv_sgl.addr = (uint64_t) (unsigned long) &cb->recv_buf;
    cb->recv_sgl.length = sizeof cb->recv_buf;
    cb->recv_sgl.lkey = cb->recv_mr->lkey;
    cb->rq_wr.sg_list = &cb->recv_sgl;
    cb->rq_wr.num_sge = 1;

    cb->send_sgl.addr = (uint64_t) (unsigned long) &cb->send_buf;
    cb->send_sgl.length = sizeof cb->send_buf;
    cb->send_sgl.lkey = cb->send_mr->lkey;

    cb->sq_wr.opcode = IBV_WR_SEND;
    cb->sq_wr.send_flags = IBV_SEND_SIGNALED;
    cb->sq_wr.sg_list = &cb->send_sgl;
    cb->sq_wr.num_sge = 1;

    cb->rdma_sgl.addr = (uint64_t) (unsigned long) cb->rdma_buf;
    cb->rdma_sgl.lkey = cb->rdma_mr->lkey;
    cb->rdma_sq_wr.send_flags = IBV_SEND_SIGNALED;
    cb->rdma_sq_wr.sg_list = &cb->rdma_sgl;
    cb->rdma_sq_wr.num_sge = 1;
}

static int rping_setup_buffers(struct rping_cb *cb)
{
    int ret;

    printf("rping_setup_buffers called on cb %p\n", cb);

    cb->recv_mr = ibv_reg_mr(cb->pd, &cb->recv_buf, sizeof cb->recv_buf,
                 IBV_ACCESS_LOCAL_WRITE);
    if (!cb->recv_mr) {
        fprintf(stderr, "recv_buf reg_mr failed\n");
        return errno;
    }

    cb->send_mr = ibv_reg_mr(cb->pd, &cb->send_buf, sizeof cb->send_buf, 0);
    if (!cb->send_mr) {
        fprintf(stderr, "send_buf reg_mr failed\n");
        ret = errno;
        goto err1;
    }

    cb->rdma_buf = malloc(cb->size);
    if (!cb->rdma_buf) {
        fprintf(stderr, "rdma_buf malloc failed\n");
        ret = -ENOMEM;
        goto err2;
    }

    cb->rdma_mr = ibv_reg_mr(cb->pd, cb->rdma_buf, cb->size,
                 IBV_ACCESS_LOCAL_WRITE |
                 IBV_ACCESS_REMOTE_READ |
                 IBV_ACCESS_REMOTE_WRITE);
    if (!cb->rdma_mr) {
        fprintf(stderr, "rdma_buf reg_mr failed\n");
        ret = errno;
        goto err3;
    }

    rping_setup_wr(cb);
    printf("allocated & registered buffers...\n");
    return 0;
    
    ibv_dereg_mr(cb->rdma_mr);
err3:
    free(cb->rdma_buf);
err2:
    ibv_dereg_mr(cb->send_mr);
err1:
    ibv_dereg_mr(cb->recv_mr);
    return ret;
}

static int rping_create_qp(struct rping_cb *cb)
{
    struct ibv_qp_init_attr init_attr;
    int ret;

    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.cap.max_send_wr = RPING_SQ_DEPTH;
    init_attr.cap.max_recv_wr = 2;
    init_attr.cap.max_recv_sge = 1;
    init_attr.cap.max_send_sge = 1;
    init_attr.qp_type = IBV_QPT_RC;
    init_attr.send_cq = cb->cq;
    init_attr.recv_cq = cb->cq;

    ret = rdma_create_qp(cb->child_cm_id, cb->pd, &init_attr);
    if (!ret)
        cb->qp = cb->child_cm_id->qp;

    return ret;
}

static void rping_free_buffers(struct rping_cb *cb)
{
    printf("rping_free_buffers called on cb %p\n", cb);
    ibv_dereg_mr(cb->recv_mr);
    ibv_dereg_mr(cb->send_mr);
    ibv_dereg_mr(cb->rdma_mr);
    free(cb->rdma_buf);
}

static int rping_setup_qp(struct rping_cb *cb, struct rdma_cm_id *cm_id)
{
    int ret;

    cb->pd = ibv_alloc_pd(cm_id->verbs);
    if (!cb->pd) {
        fprintf(stderr, "ibv_alloc_pd failed\n");
        return errno;
    }
    printf("created pd %p\n", cb->pd);
    cb->channel = ibv_create_comp_channel(cm_id->verbs);
    if (!cb->channel) {
        fprintf(stderr, "ibv_create_comp_channel failed\n");
        ret = errno;
        goto err1;
    }
    printf("created channel %p\n", cb->channel);

    cb->cq = ibv_create_cq(cm_id->verbs, RPING_SQ_DEPTH * 2, cb,
                cb->channel, 0);
    if (!cb->cq) {
        fprintf(stderr, "ibv_create_cq failed\n");
        ret = errno;
        goto err2;
    }
    printf("created cq %p\n", cb->cq);

    ret = ibv_req_notify_cq(cb->cq, 0);
    if (ret) {
        fprintf(stderr, "ibv_create_cq failed\n");
        ret = errno;
        goto err3;
    }

    ret = rping_create_qp(cb);
    if (ret) {
        fprintf(stderr, "rping_create_qp failed: %d\n", ret);
        goto err3;
    }
    printf("created qp %p\n", cb->qp);
    return 0;

err3:
    ibv_destroy_cq(cb->cq);
err2:
    ibv_destroy_comp_channel(cb->channel);
err1:
    ibv_dealloc_pd(cb->pd);
    return ret;
}

static void rping_free_qp(struct rping_cb *cb)
{
    ibv_destroy_qp(cb->qp);
    ibv_destroy_cq(cb->cq);
    ibv_destroy_comp_channel(cb->channel);
    ibv_dealloc_pd(cb->pd);
}

static int rping_accept(struct rping_cb *cb)
{
    struct rdma_conn_param conn_param;
    int ret;

    printf("accepting client connection request\n");

    memset(&conn_param, 0, sizeof conn_param);
    conn_param.responder_resources = 4;
    conn_param.initiator_depth = 4;

    ret = rdma_accept(cb->child_cm_id, &conn_param);
    if (ret) {
        fprintf(stderr, "rdma_accept error: %d\n", ret);
        return ret;
    }
    
    return 0;
}

static int process_cm_events(struct rping_cb *cb, struct rdma_cm_event *event) {
    int ret = 0;
    struct ibv_recv_wr *bad_wr;
    struct ibv_send_wr *bad_send_wr;

    switch(event->event) {
        case RDMA_CM_EVENT_ADDR_RESOLVED:
        {
            printf("CM: RDMA_CM_EVENT_ADDR_RESOLVED\n");
            ret = rdma_resolve_route(event->id, RESOLVE_TIMEOUT_MS);
            if (ret) {
                fprintf(stderr, "rdma_resolve_route error %d\n", ret);
            }
            rdma_ack_cm_event(event);
            break;
        }
        case RDMA_CM_EVENT_ROUTE_RESOLVED:
        {
            printf("CM: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
            rdma_ack_cm_event(event);
            break;
        }
        case RDMA_CM_EVENT_CONNECT_REQUEST:
        {
            cb->child_cm_id = event->id;
            printf("CM: child cma %p\n", cb->child_cm_id);
            rdma_ack_cm_event(event);

            ret = rping_setup_qp(cb, cb->child_cm_id);
            if (ret) {
                fprintf(stderr, "setup_qp failed: %d\n", ret);
                ret = -1;
                break;
            }

            ret = rping_setup_buffers(cb);
            if (ret) {
                fprintf(stderr, "rping_setup_buffers failed: %d\n", ret);
                rping_free_qp(cb);
                ret = -1;
                break;
            }

            ret = ibv_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
            if (ret) {
                fprintf(stderr, "ibv_post_recv failed: %d\n", ret);
                rping_free_buffers(cb);
            }

            ret = rping_accept(cb);
            if (ret) {
                fprintf(stderr, "connect error %d\n", ret);
                ret = -1;
            }

            break;
        }
        case RDMA_CM_EVENT_ESTABLISHED:
        {
            printf("CM: RDMA_CM_EVENT_ESTABLISHED\n");
            rdma_ack_cm_event(event);
        cb->is_connected = 1;
        
        /*
        cb->rdma_sq_wr.opcode = IBV_WR_RDMA_READ;
        cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
        cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
        cb->rdma_sq_wr.sg_list->length = cb->size;

        ret = ibv_post_send(cb->qp, &cb->rdma_sq_wr, &bad_send_wr);
        if (ret) {
            fprintf(stderr, "post send error %d\n", ret);
        }
        printf("performed an RDMA read\n");
        */
            
            break;
        }
        case RDMA_CM_EVENT_ADDR_ERROR:
        case RDMA_CM_EVENT_ROUTE_ERROR:
        case RDMA_CM_EVENT_CONNECT_ERROR:
        case RDMA_CM_EVENT_UNREACHABLE:
        case RDMA_CM_EVENT_REJECTED:
        {
            fprintf(stderr, "cma event %s, error %d\n",
                rdma_event_str(event->event), event->status);
            ret = -1;
            rdma_ack_cm_event(event);
            break;
        }
        case RDMA_CM_EVENT_DISCONNECTED:
        {
            fprintf(stderr, "DISCONNECT EVENT...\n");
            rdma_ack_cm_event(event);
            break;
        }
        case RDMA_CM_EVENT_DEVICE_REMOVAL:
        {
            fprintf(stderr, "cma detected device removal!!!!\n");
            ret = -1;
            rdma_ack_cm_event(event);
            break;
        }
        default:
            fprintf(stderr, "oof bad type!\n");
            ret = -1;
            rdma_ack_cm_event(event);
            break;
    }

    return ret;
}

static int process_cq_events(struct rping_cb *cb)
{
    struct ibv_wc wc;
    struct ibv_send_wr *bad_wr;
    int ret;
    struct timeval tv_begin;
    struct timeval tv_end;

    if ((ret = ibv_poll_cq(cb->cq, 1, &wc)) == 1) {
        ret = 0;

        if (wc.status) {
            fprintf(stderr, "cq completion failed status %d (%s)\n",
                wc.status, ibv_wc_status_str(wc.status));
            if (wc.status != IBV_WC_WR_FLUSH_ERR)
                ret = -1;
            goto error;
        }

        switch (wc.opcode) {
        case IBV_WC_SEND:
            /*
            printf("send completion\n");
            */
            break;

        case IBV_WC_RDMA_WRITE:
            /*
            printf("rdma write completion\n");
            */
            /*
            printf("rdma read completion\n");
            printf("rdma read data = %d\n", *((char*)cb->rdma_buf));
            */
            gettimeofday(&tv_end, NULL);
            cb->latencies[cb->progress] = (1000000 * tv_end.tv_sec + tv_end.tv_usec) - (1000000 * tv_begin.tv_sec + tv_begin.tv_usec);
            cb->progress++;

            if (cb->progress != cb->count) {
                gettimeofday(&tv_begin, NULL);
                cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
                cb->rdma_sq_wr.sg_list->length = cb->size;
                ret = ibv_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
                /*
                if (ret) {
                    fprintf(stderr, "post send error %d\n", ret);
                }
                printf("performed an RDMA read\n");
                */
            }
            else {
                cb->end_time_us = 1000000 * tv_end.tv_sec + tv_end.tv_usec;
                double total_latency = cb->end_time_us - cb->start_time_us;
                printf("rping is finished in %.2lf seconds\n", (total_latency)/1000000.0);

                /*
                for(int i = 0; i != cb->count; i++) {
                    printf("%.2lf,", cb->latencies[i]);
                }
                printf("\n");
                */

                Statistics LatencyStats;
                Percentiles PercentileStats;
                GetStatistics(cb->latencies, (size_t)cb->count, &LatencyStats, &PercentileStats);
                printf(
                        "Result for %d requests of %d bytes (%.2lf seconds): %.2lf RPS, Min: %.2lf, Max: %.2lf, 50th: %.2lf, 90th: %.2lf, 99th: %.2lf, 99.9th: %.2lf, 99.99th: %.2lf, StdErr: %.2lf\n",
                        cb->count,
                        cb->size,
                        ((total_latency) / 1000000),
                        (cb->count / total_latency * 1000000),
                        LatencyStats.Min,
                        LatencyStats.Max,
                        PercentileStats.P50,
                        PercentileStats.P90,
                        PercentileStats.P99,
                        PercentileStats.P99p9,
                        PercentileStats.P99p99,
                        LatencyStats.StandardError);
            }
            break;

        case IBV_WC_RDMA_READ:
            break;

        case IBV_WC_RECV:
            printf("recv completion\n");
            printf("recved %u bytes of data\n", wc.byte_len);
            printf("buf address = %p\n", (void*)cb->recv_buf.buf);
            printf("buf token = %x\n", cb->recv_buf.rkey);
            printf("buf size = %u\n", cb->recv_buf.size);
            cb->remote_rkey = cb->recv_buf.rkey;
            cb->remote_addr = cb->recv_buf.buf;
            cb->remote_len  = cb->recv_buf.size;

            /*
            *(int*)(&cb->send_buf) = 2;
            *(int*)((char*)(&cb->send_buf) + sizeof(int)) = 42;
            ret = ibv_post_send(cb->qp, &cb->sq_wr, &bad_wr);
            if (ret) {
                fprintf(stderr, "post send error %d\n", ret);
                break;
            }
            printf("server told the client its id\n");
            */
        
            cb->rdma_sq_wr.opcode = IBV_WR_RDMA_WRITE;
            cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;

            gettimeofday(&tv_begin, NULL);
            cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
            cb->rdma_sq_wr.sg_list->length = cb->size;

            ret = ibv_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
            cb->start_time_us = 1000000 * tv_begin.tv_sec + tv_begin.tv_usec;
            if (ret) {
                fprintf(stderr, "post send error %d\n", ret);
            }

            break;

        default:
            printf("unknown!!!!! completion\n");
            ret = -1;
            goto error;
        }
    }
    if (ret) {
        fprintf(stderr, "poll error %d\n", ret);
        goto error;
    }
    // ibv_ack_cq_events(cb->cq, 1);
    return 0;

error:
    // ibv_ack_cq_events(cb->cq, 1);
    return ret;
}

int set_nonblocking(struct rdma_event_channel *channel) {
    int flags = fcntl(channel->fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    if (fcntl(channel->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        return -1;
    }

    return 0;
}

int main() {
    struct rping_cb *cb;
    struct sockaddr_in sin;
    int ret;
    struct rdma_cm_event *event;
    struct ibv_cq *ev_cq;
    void *ev_ctx;

    cb = malloc(sizeof(*cb));
    if (!cb) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    memset(cb, 0, sizeof(*cb));
    cb->size = PING_DATA_SIZE;
    cb->port = htons(LISTEN_PORT);
    strcpy(cb->addr_str, SERVER_IP);
    cb->addr = inet_addr(SERVER_IP);
    cb->count = PING_COUNT;
    cb->progress = 0;
    cb->is_connected = 0;
    cb->latencies = malloc(sizeof(double) * PING_COUNT);
    if (!cb->latencies) {
        fprintf(stderr, "malloc for latencies failed\n");
        free(cb);
        return 1;
    }
    
    cb->cm_channel = rdma_create_event_channel();
    if (!cb->cm_channel) {
        ret = errno;
        fprintf(stderr, "rdma_create_event_channel error %d\n", ret);
        goto out;
    }

    ret = set_nonblocking(cb->cm_channel);
    if (ret) {
        fprintf(stderr, "failed to set non-blocking\n");
        goto out;
    }

    ret = rdma_create_id(cb->cm_channel, &cb->cm_id, cb, RDMA_PS_TCP);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_create_id error %d\n", ret);
        goto out;
    }
    printf("Created cm_id %p\n", cb->cm_id);

    // Bind to local address
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = cb->addr;
    sin.sin_port = cb->port;

    ret = rdma_bind_addr(cb->cm_id, (struct sockaddr *) &sin);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_bind_addr error %d\n", ret);
        goto out;
    }
    printf("rdma_bind_addr succeeded\n");

    // Listen for incoming connections
    ret = rdma_listen(cb->cm_id, LISTEN_BACKLOG);
    if (ret) {
        ret = errno;
        fprintf(stderr, "rdma_listen error %d\n", ret);
        goto out;
    }

    while (1) {
	//    bind_to_core(7);
        if (!cb->is_connected) {
            ret = rdma_get_cm_event(cb->cm_channel, &event);
            if (ret && errno != EAGAIN) {
                ret = errno;
                fprintf(stderr, "rdma_get_cm_event error %d\n", ret);
                goto out;
            }
            else if (!ret) {
                printf("cma_event type %s cma_id %p (%s)\n",
                rdma_event_str(event->event), event->id,
                (event->id == cb->cm_id) ? "parent" : "child");

                ret = process_cm_events(cb, event);
                if (ret) {
                    fprintf(stderr, "process_cm_events error %d\n", ret);
                    goto out;
                }
            }
        }
        else {
            /*
            ret = ibv_get_cq_event(cb->channel, &ev_cq, &ev_ctx);
            if (ret) {
                fprintf(stderr, "Failed to get cq event!\n");
                goto out;
            }
            if (ev_cq != cb->cq) {
                fprintf(stderr, "Unkown CQ!\n");
                goto out;
            }
            ret = ibv_req_notify_cq(cb->cq, 0);
            if (ret) {
                fprintf(stderr, "Failed to set notify!\n");
                goto out;
            }
            */
            ret = process_cq_events(cb);
            if (ret) {
                fprintf(stderr, "process_cq_events error %d!\n", ret);
                goto out;
            }
        }
    }

out:
    free(cb->latencies);
    free(cb);
    return ret;
}
