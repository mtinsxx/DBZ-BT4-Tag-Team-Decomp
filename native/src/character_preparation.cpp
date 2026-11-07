#include "tenkaichi/character_preparation.hpp"
#include "tenkaichi/inspection.hpp"
#include "tenkaichi/native_codec.hpp"
#include "tenkaichi/record_cache.hpp"
#include "tenkaichi/preparation.hpp"
#include <exception>
#include <stdexcept>
namespace tenkaichi {
namespace {
NativeRecordKey key(const Resource& r,const char* converter){
    if(!r.path||!r.bytes||r.bytes>4*1024*1024)throw std::runtime_error("Character preparation: invalid catalogue resource");
    const auto identity=std::string(r.path)+"\nbytes="+std::to_string(r.bytes)+"\ncrc32="+std::to_string(r.crc)+"\n";
    const auto source=sha256({reinterpret_cast<const std::uint8_t*>(identity.data()),identity.size()});
    const auto text=std::string(converter)+"\n"+source;return {sha256({reinterpret_cast<const std::uint8_t*>(text.data()),text.size()}),converter,source};
}
}
bool prepareCharacterResources(const std::string& root,const std::string& profile,const CharacterSourceSet& sources,const CharacterPreparationObserver& observe,std::string& error){
    error.clear();try{
        const NativeRecordKey records[]={key(sources.model,"bt4-skeleton-native-v1"),key(sources.motion_a,"bt4-motion-entry1-native-v1")};
        const Resource source[]={sources.model,sources.motion_a};
        CharacterPreparationProgress progress;progress.catalogue_source_bytes=sources.model.bytes+sources.motion_a.bytes;
        auto emit=[&](const std::string& phase){progress.phase=phase;if(observe&&!observe(progress))throw std::runtime_error("Character preparation interrupted; committed records are reusable");};
        for(unsigned i=0;i<2;++i){
            emit("CHECKING_RECORD_"+std::to_string(i));std::vector<std::uint8_t> payload;std::string sourceHash,issue;
            bool reused=loadNativeRecord(root,profile,records[i],payload,sourceHash,issue);
            if(reused){if(!i){Skeleton s;reused=decodeSkeletonResource({payload.data(),payload.size()},s,issue);}else{AnimationClip c;reused=decodeAnimationResource({payload.data(),payload.size()},c,issue);}}
            if(reused){++progress.completed;++progress.reused;progress.output_bytes+=payload.size();emit("RECORD_REUSED_"+std::to_string(i));continue;}
            if(!checkNativeRecordStorage(root,profile,i?16*1024*1024:4096,error))return false;
            emit("READING_RECORD_"+std::to_string(i));std::vector<std::uint8_t> bytes;
            if(!readVerifiedResource(source[i],bytes,error))return false;
            progress.source_bytes_processed+=bytes.size();
            sourceHash=sha256({bytes.data(),bytes.size()});Bytes entry;
            if(!packEntry({bytes.data(),bytes.size()},i?1:3,entry,error)||!entry.data){if(error.empty())error="Character preparation: required pack entry is empty";return false;}
            if(!i){Skeleton s;if(!decodeSkeleton(entry,s,error)||!encodeSkeletonResource(s,payload,error))return false;}
            else{AnimationClip c;if(!decodePackedAnimation(entry,c,error)||!encodeAnimationResource(c,payload,error))return false;}
            emit("COMMITTING_RECORD_"+std::to_string(i));
            if(!saveNativeRecord(root,profile,records[i],{payload.data(),payload.size()},sourceHash,error))return false;
            ++progress.completed;progress.output_bytes+=payload.size();emit("RECORD_COMMITTED_"+std::to_string(i));
        }
        CharacterFoundation checked;if(!loadPreparedCharacter(root,profile,sources,checked,error))return false;
        emit("SUPPORTED_CHARACTER_SUBSET_READY_GAME_INCOMPLETE");return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool loadPreparedCharacter(const std::string& root,const std::string& profile,const CharacterSourceSet& sources,CharacterFoundation& output,std::string& error){
    output={};error.clear();try{
        CharacterFoundation c;std::vector<std::uint8_t> payload;
        if(!loadNativeRecord(root,profile,key(sources.model,"bt4-skeleton-native-v1"),payload,c.model_sha256,error)||!decodeSkeletonResource({payload.data(),payload.size()},c.skeleton,error))return false;
        if(!loadNativeRecord(root,profile,key(sources.motion_a,"bt4-motion-entry1-native-v1"),payload,c.motion_a_sha256,error)||!decodeAnimationResource({payload.data(),payload.size()},c.first_motion,error))return false;
        c.metadata="Prepared native skeleton + first motion reused; geometry/materials/motion-B incomplete; vertex bind space unverified\n";
        output=std::move(c);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
