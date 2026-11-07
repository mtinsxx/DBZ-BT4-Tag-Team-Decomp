#include "tenkaichi/engine.hpp"
#include "fixtures.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace tenkaichi;
static bool close(float a,float b){return std::fabs(a-b)<0.0001f;}
static void f32(std::vector<std::uint8_t>& v,unsigned at,float value){std::uint32_t b;std::memcpy(&b,&value,4);word(v,at,b);}
static void half(std::vector<std::uint8_t>& v,unsigned at,unsigned n){v[at]=std::uint8_t(n);v[at+1]=std::uint8_t(n>>8);}
static Skeleton skeleton(){
    std::vector<std::uint8_t> model(0x70+4*0x60);word(model,0x6c,0x70);
    const unsigned ids[]={0,2,5,7},pops[]={0,1,2,1};
    for(unsigned i=0;i<4;++i){const auto at=0x70+i*0x60;word(model,at,0x60);half(model,at+4,pops[i]);half(model,at+6,i==3);half(model,at+10,ids[i]);f32(model,at+0x30,float(i+1));}
    Skeleton out;std::string error;assert(decodeSkeleton({model.data(),model.size()},out,error));
    assert(out.joints[0].parent==-1&&out.joints[1].parent==0&&out.joints[2].parent==0&&out.joints[3].parent==-1);
    assert(!out.vertex_bind_space_verified&&close(out.rest_global[1].m[3][0],3));
    auto bad=model;half(bad,0x70+4,2);assert(!decodeSkeleton({bad.data(),bad.size()},out,error)&&out.joints.empty());
    bad=model;half(bad,0x70+0x60+10,0);assert(!decodeSkeleton({bad.data(),bad.size()},out,error));
    assert(decodeSkeleton({model.data(),model.size()},out,error));return out;
}
static AnimationClip clip(){
    AnimationClip clip;clip.last_frame=10;auto root=std::make_shared<AnimationTrack>();
    root->keys={AnimationKey{0,{0,0,0},{}},AnimationKey{10,{10,0,0},{}}};clip.tracks[0]=root;
    auto child=std::make_shared<AnimationTrack>();child->keys={AnimationKey{0,{0,2,0},{}},AnimationKey{10,{0,12,0},{}}};clip.tracks[2]=child;
    AnimationEvent e;e.attributes=8;e.frame=2;e.argument=1;clip.events.push_back(e);e.argument=2;clip.events.push_back(e);return clip;
}
static void transforms(){
    const float q=std::sqrt(0.5f);LocalPose root{{10,0,0},{0,0,q,q}};auto m=localMatrix(root);
    auto p=transformPoint({1,0,0},m);assert(close(p.x,10)&&close(p.y,1));
    auto inverse=inverseRigid(m);p=transformPoint(p,inverse);assert(close(p.x,1)&&close(p.y,0));
    const auto identity=combine(m,inverse);for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)assert(close(identity.m[i][j],i==j?1.0f:0.0f));
    Scene scene(3);auto a=scene.create(root),b=scene.create({{1,0,0},{}},a);assert(a&&b);
    std::string error;assert(scene.updateTransforms(error));assert(close(scene.get(b)->world.m[3][1],1));
    assert(!scene.reparent(a,b)&&!scene.destroy(a));assert(scene.destroy(b));auto c=scene.create();assert(c&&c!=b&&!scene.get(b));
    assert(scene.reparent(a,c)&&scene.updateTransforms(error));
    scene.get(c)->parent=a;assert(!scene.updateTransforms(error)); // Public component corruption is detected.
}
static void poses(){
    auto s=skeleton();auto c=clip();std::string error;Pose p;
    assert(evaluatePose(s,{&c,nullptr,5,0,0,nullptr},{},p,error));
    assert(close(p.global[1].m[3][0],5)&&close(p.global[1].m[3][1],7));
    std::vector<LocalPose> previous(p.local.size(),LocalPose{{20,0,0},{}});
    assert(evaluatePose(s,{&c,nullptr,5,0,1,&previous},{},p,error));
    assert(close(p.local[0].translation.x,5)&&close(p.local[1].translation.x,20)); // Root excluded from transition.
    auto secondary=c;secondary.last_frame=20;auto t=std::make_shared<AnimationTrack>();t->keys={AnimationKey{0,{0,0,0},{}},AnimationKey{20,{0,20,0},{}}};secondary.tracks[2]=t;
    assert(evaluatePose(s,{&c,&secondary,5,0.5f,0,nullptr},{},p,error)&&close(p.local[1].translation.y,8.5f));
    for(std::size_t i=0;i<s.joints.size();++i){auto identity=combine(s.rest_global[i],s.inverse_rest_global[i]);assert(close(identity.m[3][0],0));}
    s.joints[0].parent=4;assert(!evaluatePose(s,{&c},{},p,error));
}
static void simulation(){
    FixedStep a,b;TickAdvance done;std::string error;std::uint64_t countA=0,countB=0;
    auto tickA=[&](std::uint64_t id){assert(id==countA++);return true;};auto tickB=[&](std::uint64_t id){assert(id==countB++);return true;};
    for(unsigned i=0;i<60;++i)assert(a.advance(16666667,8,tickA,done,error));
    for(unsigned i=0;i<144;++i)assert(b.advance(i<143?6944444:6944508,8,tickB,done,error));
    assert(countA==30&&countB==30);
    assert(a.advance(1000000000,2,tickA,done,error)&&done.executed==2&&done.pending_ticks==28);
    while(done.pending_ticks){assert(a.advance(0,8,tickA,done,error));}
    assert(countA==60);
    assert(!a.advance(UINT64_MAX,8,tickA,done,error));
    BattleClock clock;clock.step();assert(clock.milliseconds==34);clock.step();assert(clock.milliseconds==66);clock.step();assert(clock.milliseconds==100);
    clock.milliseconds=35999990;clock.step();assert(clock.milliseconds==35999999);
    FighterInput input;InputRecord r;r.buttons=1;r.stick={{255,127}};
    assert(input.update(r,false,error)&&input.pressed==1&&input.buttons.held[0]==1&&input.raw[0]>1);
    for(unsigned i=0;i<200;++i){assert(input.update(r,false,error));}
    assert(input.buttons.held[0]==100);
    r.stick={{127,254}};assert(input.update(r,false,error)&&close(input.stick_turn,-3.14159265f/2));
    r.stick={{128,0}};assert(input.update(r,false,error)&&input.stick_turn>3.0f&&input.stick_turn<3.14159265f);
    const auto held=input.buttons.held;assert(input.update({},true,error)&&input.buttons.held==held&&input.released==1);
    std::array<std::uint32_t,31> masks{};masks[15]=3;masks[0]=1;assert(remapButtons(1,masks)==1&&remapButtons(3,masks)==0x8001);
    assert(stickToByte(-1)==0&&stickToByte(0)==127&&stickToByte(1)==254);
    auto events=clip();AnimationEvents selected;assert(selectAnimationEvents(events,0,2,selected,error)&&selected.fired.size()==1&&selected.fired[0].argument==3);
    assert(selectAnimationEvents(events,2,3,selected,error)&&selected.fired.empty());
    float frame=9,prev=8;std::uint8_t mode=2;stepMotion(10,3,frame,prev,mode);assert(frame==0&&prev==0&&mode==2);
    frame=9;mode=1;stepMotion(10,3,frame,prev,mode);assert(frame==10&&prev==10&&mode==0);
}
static void hit_geometry(){
    Skeleton s;Joint joint;joint.source_node=3;s.joints.push_back(joint);std::string error;assert(finalizeSkeleton(s,error));
    auto c=clip();Pose pose;assert(evaluatePose(s,{&c},{},pose,error));
    AnimationEvent event;event.attributes=1;event.has_volume=true;event.argument=1;event.sphere=true;event.offset={1,2,3};event.radii={4,5,6};
    std::vector<WorldAttackVolume> volumes;assert(buildAttackVolumes(s,pose,event,0.5f,volumes,error)&&volumes.size()==1);
    assert(close(volumes[0].center.x,1)&&close(volumes[0].bounds.min.x,-1)&&close(volumes[0].bounds.max.z,5));
    event.sphere=false;assert(buildAttackVolumes(s,pose,event,1,volumes,error)&&close(volumes[0].bounds.max.y,7));
    event.argument=2;assert(!buildAttackVolumes(s,pose,event,1,volumes,error)&&volumes.empty());
    assert(attackAttachmentNode(1)==7&&attackAttachmentNode(15)==0&&attackAttachmentNode(19)==-1);
}
static void integration(){
    Engine engine;std::string error;auto s=engine.assets().addSkeleton(skeleton());auto c=engine.assets().addClip(clip());assert(s&&c);
    const auto entity=engine.addAnimatedEntity(s,c,{},error);assert(entity&&engine.play(entity,c,Animator::Mode::Loop,0,error));
    InputRecord input;input.buttons=1;assert(engine.setInput(input,error));TickAdvance advance;
    assert(engine.advance(100000000,advance,error)&&advance.executed==3);assert(engine.input().held==1);
    assert(engine.events().size()==3&&engine.events()[1].tick==1&&engine.events()[1].fired[0].argument==3);
    SceneFrame frame;assert(engine.frame(frame,error)&&frame.simulation_tick==3&&frame.poses.size()==1&&close(frame.poses[0].pose.global[0].m[3][0],3));
    engine.pause(true);assert(engine.advance(100000000,advance,error)&&engine.ticks()==6);assert(engine.frame(frame,error)&&close(frame.poses[0].pose.global[0].m[3][0],3));
    engine.suspend(true);assert(engine.advance(1000000000,advance,error)&&engine.ticks()==6);engine.suspend(false);engine.pause(false);
    auto lease=engine.assets().clip(c);assert(!engine.assets().release(c));lease.reset();
    assert(engine.assets().release(c));assert(!engine.advance(34000000,advance,error)&&engine.faulted());assert(!engine.frame(frame,error));
    AssetLibrary immutable;auto source=clip();auto mutableTrack=std::make_shared<AnimationTrack>(*source.tracks[0]);source.tracks[0]=mutableTrack;
    auto frozen=immutable.addClip(source);mutableTrack->keys[0].translation.x=99;assert(immutable.clip(frozen)->tracks[0]->keys[0].translation.x==0);
    assert(!engine.gameReadiness().ready&&!engine.gameReadiness().missing.empty());
    Engine small({1,1,1,8});assert(!small.assets().addClip(clip()));
    Engine invalid;auto is=invalid.assets().addSkeleton(skeleton());auto ic=invalid.assets().addClip(clip());
    auto bad=invalid.addAnimatedEntity(is,ic,{},error);invalid.scene().get(bad)->animator->mode=Animator::Mode(255);
    assert(!invalid.advance(34000000,advance,error)&&invalid.faulted());
    const auto& order=originalBattleOrder();assert(order.front()==BattlePhase::Input&&order[10]==BattlePhase::Camera0&&order.back()==BattlePhase::Visibility);
}
int main(){transforms();poses();simulation();hit_geometry();integration();std::puts("Integrated engine: stable handles, resource leases, hierarchy, original skeleton fields, pose/layers, 30Hz debt, input counters, events and fault/readiness gates passed (fixtures only)");}
