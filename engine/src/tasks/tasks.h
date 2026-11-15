// Internal factories for the three tasks (the public entry point is spike::make_task in spike/task.h).
#pragma once
#include <memory>
#include "spike/task.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_reflex();
std::unique_ptr< Task > make_lamprey();
std::unique_ptr< Task > make_worm();
} // namespace spike::tasks
