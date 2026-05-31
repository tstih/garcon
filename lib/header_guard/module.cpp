// Dynamic header-guard module export.
//
// This file keeps the ABI-facing export separate from the C++ module logic.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "header_guard_module.h"

GARCON_EXPORT_MODULE(garcon::modules::header_guard_module, "header-guard")
