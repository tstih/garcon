// Dynamic JWT-auth module export.
//
// This file keeps the ABI-facing export separate from the C++ module logic.
//
// Copyright 2025 Tomaz Stih. All rights reserved.
// MIT License.
#include "jwt_auth_module.h"

GARCON_EXPORT_MODULE(garcon::modules::jwt_auth_module, "jwt-auth")
