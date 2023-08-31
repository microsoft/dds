#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <string.h>
#include <sys/time.h>
#include <time.h>

#define DB_PAGE_SIZE 8192
#define READ_QUEUE_DEPTH 16
#define WRITE_QUEUE_DEPTH 16
#define TOTAL_READS 10000000

struct IOContext_t {
	char *Buff;
	void *BenchCtx;
};

/*
 * We'll use this struct to gather housekeeping benchmark_context to pass between
 * our events and callbacks.
 */
struct benchmark_context_t {
	struct spdk_bdev *bdev;
	struct spdk_bdev_desc *bdev_desc;
	struct spdk_io_channel *bdev_io_channel;
	char *bdev_name;
	struct spdk_bdev_io_wait_entry bdev_io_wait;
	size_t total_writes;
	size_t writes_issued; // should be protected by lock
	size_t writes_completed; // should be protected by lock
	size_t reads_issued; // should be protected by lock
	size_t reads_completed; // should be protected by lock
	struct timeval tv_start;
	struct timeval tv_end;
	struct IOContext_t *read_ctxs[READ_QUEUE_DEPTH];
	struct IOContext_t *write_ctxs[WRITE_QUEUE_DEPTH];
};

void InitializeContexts(struct benchmark_context_t* Ctx, size_t DevCapacity);
void PrepareData(struct benchmark_context_t* Ctx);
void ReleaseContexts(struct benchmark_context_t *Ctx);

//
// Initialize contexts
//
void InitializeContexts(struct benchmark_context_t* Ctx, size_t DevCapacity) {
	for (int i = 0; i != READ_QUEUE_DEPTH; i++) {
		struct IOContext_t* IOContext = malloc(sizeof(struct IOContext_t));
		IOContext->Buff = NULL;
		IOContext->BenchCtx = Ctx;
		Ctx->read_ctxs[i] = IOContext;
	}

	for (int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		struct IOContext_t* IOContext = malloc(sizeof(struct IOContext_t));
		IOContext->Buff = NULL;
		IOContext->BenchCtx = Ctx;
		Ctx->write_ctxs[i] = IOContext;
	}

	Ctx->total_writes = DevCapacity / DB_PAGE_SIZE;
}

//
// Populate a buffer
//
void PrepareData(struct benchmark_context_t* Ctx) {
	for (int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		struct IOContext_t *IOContext = Ctx->write_ctxs[i];
		memset(IOContext->Buff, 42, DB_PAGE_SIZE);
	}
}

//
// Release contexts
//
void ReleaseContexts(struct benchmark_context_t *Ctx) {
	for (int i = 0; i != READ_QUEUE_DEPTH; i++) {
		free(Ctx->read_ctxs[i]);
	}

	for (int i = 0; i != WRITE_QUEUE_DEPTH; i++) {
		free(Ctx->write_ctxs[i]);
	}
}

#endif
