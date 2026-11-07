#pragma once
#include "assets.hpp"
#include "engine.hpp"
namespace tenkaichi {
void diagnosticRoot(const std::string& path);
void status(const char* phase,const std::string& detail);
int renderEngine(Engine& engine);
}
