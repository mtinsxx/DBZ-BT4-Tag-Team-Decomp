#pragma once
#include "character_content.hpp"
#include <functional>
namespace tenkaichi {
struct CharacterPreparationProgress {
    std::string phase;unsigned completed=0,total=2,reused=0;
    std::uint64_t source_bytes_processed=0,catalogue_source_bytes=0,output_bytes=0;
};
using CharacterPreparationObserver=std::function<bool(const CharacterPreparationProgress&)>;
// Supported subset only: native skeleton + motion entry 1, NOT every motion,
// model geometry, materials, animation B, stage, battle or complete game data.
bool prepareCharacterResources(const std::string& root,const std::string& profile,const CharacterSourceSet& sources,const CharacterPreparationObserver& observer,std::string& error);
bool loadPreparedCharacter(const std::string& root,const std::string& profile,const CharacterSourceSet& sources,CharacterFoundation& character,std::string& error);
}
