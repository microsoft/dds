#pragma once

#include "bdev.h"

// TODO: per thread SPDK context handling

//
// The main thread where we call spdk_app_start(), which will block
//
//
extern pthread_t FSAppThread;

//
// Async init, will need callback chains.
// TODO: finish callbacks needed
//
void InitializeStorage(void *InitializeCtx);

//
// The context passed to pthread create and to `FSAppStartFunction()`
//
//
struct FSAppStartContext {

};

//
// The function passed to pthread create, param should be `struct FSAppStartContext`
// create SPDK worker threads as needed
//
//
void FSAppStartFunction(void *AppStartCtx);


struct CtrlMsgHandlerCtx {

};

//
// Submit a request to the FS for processing control cq events, param should be `struct CtrlMsgHandlerCtx`
// instead of calling `CtrlMsgHandler` directly in agent thread in `ProcessCtrlCqEvents`, call this function to handle it asynchronously
//
//
void ProcessCtrlMsgAsync(void *CtrlMsgHandlerCtx);


struct ExecuteRequestsCtx {

};

//
// Submit a request to the FS for processing buff cq events, param should be `struct ExecuteRequestsCtx`
// instead of calling `ExecuteRequests` directly in agent thread in `ProcessBuffCqEvents`, call this function to handle it asynchronously
//
//
void ExecuteRequestsAsync(void *ExecuteRequestsCtx);