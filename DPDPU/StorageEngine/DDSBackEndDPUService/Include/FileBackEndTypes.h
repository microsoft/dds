#pragma once

#include "DDSTypes.h"

//
// Context for a pending control plane request
//
//
typedef struct {
    RequestIdT RequestId;
    BufferT Request;
    BufferT Response;
} ControlPlaneRequestContext;