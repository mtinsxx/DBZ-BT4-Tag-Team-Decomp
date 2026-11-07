#pragma once
#include "assets.hpp"
#include <functional>
namespace tenkaichi {
struct PreparationProgress {
    std::string phase;
    unsigned completed=0,total=1,reused=0;
    std::uint64_t source_bytes=0,total_source_bytes=63488,output_bytes=0;
};
using PreparationObserver=std::function<bool(const PreparationProgress&)>;
/* Scope is the proven portrait converter only. NEVER implies game readiness.
   All paths are fixed internal names; no resource-controlled filesystem paths. */
bool prepareResources(const std::string& root,const std::string& profile,const PreparationObserver& observer,std::string& error,const Resource& resource=portrait0);
bool loadPreparedPortrait(const std::string& root,const std::string& profile,Image& image,std::string& error,const Resource& resource=portrait0);
bool clearPreparedResources(const std::string& root,std::string& error);
std::string sha256(Bytes bytes);
}
