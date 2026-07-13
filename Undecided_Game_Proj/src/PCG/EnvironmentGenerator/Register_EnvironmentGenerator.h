#pragma once

#include "EnvironmentGenerator.hpp"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initializeEnvironmentGenerator(ModuleInitializationLevel level);
void uninitializeEnvironmentGenerator(ModuleInitializationLevel level);
