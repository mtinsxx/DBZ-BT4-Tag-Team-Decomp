#pragma once
#include "animation.hpp"
#include <array>
#include <functional>
namespace tenkaichi {
struct TickAdvance {unsigned executed=0;std::uint64_t pending_ticks=0;};
class FixedStep {
    std::uint64_t credit_=0,tick_=0;
public:
    // Exact rational 30 Hz; retained debt, bounded work per presentation.
    bool advance(std::uint64_t elapsedNs,unsigned budget,const std::function<bool(std::uint64_t)>& tick,TickAdvance& out,std::string& error);
    std::uint64_t ticks()const{return tick_;}
};
struct BattleClock {
    std::uint32_t ticks=0;std::uint64_t milliseconds=0;
    void step(); // Original 34,32,34 millisecond sequence, saturation 9:59:59.999.
};
struct InputRecord {std::uint32_t buttons=0,commands=0;std::array<std::uint8_t,2> stick{{127,127}};};
struct InputCounters {
    std::array<std::uint8_t,32> held{},not_held{},since_press{},since_release{},previous_since_press{};
};
struct FighterInput {
    std::uint32_t held=0,previous=0,pressed=0,released=0,command_held=0,command_pressed=0,command_released=0;
    std::array<float,2> raw{},previous_raw{},smooth{},previous_smooth{};
    float stick_turn=0;
    InputCounters buttons,commands;
    bool update(const InputRecord& record,bool actorLoading,std::string& error);
};
std::uint8_t stickToByte(float value);
float byteToStick(std::uint8_t value);
std::uint32_t remapButtons(std::uint32_t raw,const std::array<std::uint32_t,31>& masks);
struct FiredEvent {std::uint64_t attributes=0;std::uint32_t argument=0;};
struct AnimationEvents {
    std::vector<FiredEvent> fired;int hit_index=-1;unsigned hit_count=0,dropped=0;
    const AnimationEvent* active_volume=nullptr; // Borrowed from immutable clip for the current tick.
};
bool selectAnimationEvents(const AnimationClip& clip,float previous,float current,AnimationEvents& out,std::string& error);
// Exact BtlObjAnim_Step modes: loop discards overshoot; Once sets both frames
// to the end. This is distinct from a generic modulo animation clock.
void stepMotion(float length,float step,float& frame,float& previous,std::uint8_t& mode);
}
