#pragma once

#include <filesystem>
#include "predictor/predictor.h"

namespace linuxcomplete {

// Load predictor config from a JSON file (system or user).
void load_predictor_config_from_file(const std::filesystem::path& path, PredictorConfig& config);

// Override config values from LINUXCOMPLETE_AI_* environment variables.
void apply_predictor_env_overrides(PredictorConfig& config);

} // namespace linuxcomplete
