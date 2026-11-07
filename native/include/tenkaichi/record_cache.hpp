#pragma once
#include "assets.hpp"
namespace tenkaichi {
struct NativeRecordKey {std::string id,converter,source_identity;};
/* Versioned per-resource cache, SHA-256 identity/output/manifest. Private
   records only; no code, raw ISO copy or driver pipeline cache is stored. */
bool loadNativeRecord(const std::string& root,const std::string& profile,const NativeRecordKey& key,std::vector<std::uint8_t>& payload,std::string& sourceHash,std::string& error);
bool checkNativeRecordStorage(const std::string& root,const std::string& profile,std::size_t upperBound,std::string& error);
bool saveNativeRecord(const std::string& root,const std::string& profile,const NativeRecordKey& key,Bytes payload,const std::string& sourceHash,std::string& error);
}
