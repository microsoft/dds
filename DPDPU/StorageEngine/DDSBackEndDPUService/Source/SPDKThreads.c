#include "../Include/SPDKThreads.h"
#include "SPDKThreads.h"

void InitializeStorage(void *InitializeCtx)
{
    // TODO
}

//
// Also initialize storage when starting
//
//
void FSAppStartFunction(void *AppStartCtx)
{
    struct FSAppStartContext *AppStartCtx = AppStartCtx;
    // TODO
}

void ProcessCtrlMsgAsync(void *CtrlMsgHandlerCtx)
{
    struct CtrlMsgHandlerCtx *CtrlMsgHandlerCtx = CtrlMsgHandlerCtx;
    // TODO
}

void ExecuteRequestsAsync(void *ExecuteRequestsCtx)
{
    struct ExecuteRequestsCtx *ExecuteRequestsCtx = ExecuteRequestsCtx;
    // TODO
}
