#pragma once
#include "assets.hpp"
namespace tenkaichi {
void diagnosticRoot(const std::string& path);
void status(const char* phase,const std::string& detail);
int renderResource(const Image& image);
}
