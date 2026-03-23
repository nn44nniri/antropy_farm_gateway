#pragma once
#include "antropy_farm_gateway/types.hpp"
#include <string>

namespace antropy::gateway {
Settings load_settings(const std::string& path);
}
