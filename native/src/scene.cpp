#include "tenkaichi/scene.hpp"
namespace tenkaichi {
EntityId Scene::create(const LocalPose& pose,EntityId parent){
    if(!validPose(pose)||(parent&&!get(parent)))return {};
    Entity e;e.local=pose;e.parent=parent;e.world=localMatrix(pose);
    auto ancestor=parent;unsigned depth=0;while(ancestor){const auto* p=get(ancestor);if(!p||++depth>=128||!validPose(p->local))return {};e.world=combine(e.world,localMatrix(p->local));ancestor=p->parent;}if(!finiteMatrix(e.world))return {};return entities_.insert(std::move(e));
}
bool Scene::destroy(EntityId id){
    bool child=false;entities_.each([&](EntityId,const Entity& e){child|=e.parent==id;});return !child&&entities_.erase(id);
}
bool Scene::reparent(EntityId id,EntityId parent){
    if(!get(id)||(parent&&!get(parent)))return false;
    auto cursor=parent;unsigned depth=0;
    while(cursor){if(cursor==id||++depth>128)return false;auto* p=get(cursor);if(!p)return false;cursor=p->parent;}
    get(id)->parent=parent;return true;
}
bool Scene::updateTransforms(std::string& error){
    error.clear();bool ok=true;const auto span=entities_.indexSpan();world_scratch_.resize(span);marks_.assign(span,0);depths_.assign(span,0);
    std::vector<EntityId> chain;chain.reserve(128);
    entities_.each([&](EntityId id,const Entity&){
        if(!ok||marks_[id.index]==2)return;
        chain.clear();auto cursor=id;
        while(cursor){
            const auto* e=get(cursor);
            if(!e||!validPose(e->local)||marks_[cursor.index]==1){ok=false;error="Scene: stale parent, cycle or invalid local transform";return;}
            if(marks_[cursor.index]==2)break;
            marks_[cursor.index]=1;chain.push_back(cursor);if(chain.size()>128){ok=false;error="Scene: hierarchy depth budget exceeded";return;}cursor=e->parent;
        }
        Matrix4 parent=cursor?world_scratch_[cursor.index]:Matrix4{};unsigned depth=cursor?depths_[cursor.index]:0;
        for(auto i=chain.rbegin();i!=chain.rend();++i){
            if(++depth>128){ok=false;error="Scene: hierarchy depth budget exceeded";return;}
            const auto* e=get(*i);auto world=combine(localMatrix(e->local),parent);
            if(!finiteMatrix(world)){ok=false;error="Scene: world transform overflow";return;}
            world_scratch_[i->index]=world;depths_[i->index]=depth;marks_[i->index]=2;parent=world;
        }
    });
    if(!ok)return false;
    entities_.each([&](EntityId id,Entity& e){e.world=world_scratch_[id.index];});return true;
}
}
