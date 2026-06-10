// Dynamic rate-limit module export.
//
// This file keeps the ABI-facing export separate from the C++ module logic.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "rate_limit_module.h"

GARCON_EXPORT_MODULE(garcon::modules::rate_limit_module, "rate-limit")
