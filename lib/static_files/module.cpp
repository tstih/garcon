// Dynamic static-files module export.
//
// This file keeps the ABI-facing export separate from the C++ module logic.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "static_files_module.h"

GARCON_EXPORT_MODULE(garcon::modules::static_files_module, "static-files")
