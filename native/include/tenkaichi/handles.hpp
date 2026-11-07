#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>
namespace tenkaichi {
template<class Tag> struct Handle {
    std::uint32_t index=UINT32_MAX,generation=0;
    explicit operator bool()const{return index!=UINT32_MAX&&generation!=0;}
    bool operator==(Handle other)const{return index==other.index&&generation==other.generation;}
    bool operator!=(Handle other)const{return !(*this==other);}
};
/* Single-writer table. IDs survive vector relocation; pointers returned by get
   are borrowed until the next mutation. A wrapping generation retires a slot. */
template<class T,class Tag> class HandleTable {
    struct Slot {std::optional<T> value;std::uint32_t generation=1;};
    std::vector<Slot> slots_;std::size_t maximum_,live_=0;
public:
    using Id=Handle<Tag>;
    explicit HandleTable(std::size_t maximum):maximum_(maximum){}
    Id insert(T value){
        for(std::size_t i=0;i<slots_.size();++i)if(!slots_[i].value&&slots_[i].generation){slots_[i].value=std::move(value);++live_;return {std::uint32_t(i),slots_[i].generation};}
        if(slots_.size()>=maximum_||slots_.size()>=UINT32_MAX)return {};
        slots_.push_back({std::move(value),1});++live_;return {std::uint32_t(slots_.size()-1),1};
    }
    T* get(Id id){return id.index<slots_.size()&&id.generation&&slots_[id.index].generation==id.generation&&slots_[id.index].value?&*slots_[id.index].value:nullptr;}
    const T* get(Id id)const{return const_cast<HandleTable*>(this)->get(id);}
    bool erase(Id id){if(!get(id))return false;auto& s=slots_[id.index];s.value.reset();++s.generation;--live_;return true;}
    template<class F>void each(F f){for(std::size_t i=0;i<slots_.size();++i)if(slots_[i].value)f(Id{std::uint32_t(i),slots_[i].generation},*slots_[i].value);}
    template<class F>void each(F f)const{for(std::size_t i=0;i<slots_.size();++i)if(slots_[i].value)f(Id{std::uint32_t(i),slots_[i].generation},*slots_[i].value);}
    std::size_t size()const{return live_;}
    std::size_t indexSpan()const{return slots_.size();}
};
}
