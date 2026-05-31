// Dynamic host-guard module export.
//
// This file keeps the ABI-facing export separate from the C++ module logic.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "host_guard_module.h"

GARCON_EXPORT_MODULE(garcon::modules::host_guard_module, "host-guard")
