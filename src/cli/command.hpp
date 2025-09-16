#pragma once
#include <functional>

namespace gitfly::cli {
using command_fn = int (*)(int /*argc*/, char ** /*argv*/);
} // namespace gitfly::cli
