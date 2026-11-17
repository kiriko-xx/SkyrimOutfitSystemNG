$ErrorActionPreference = 'Stop'
# Compile the entire production namespace verbatim against minimal engine doubles.
$work = Join-Path $PSScriptRoot '../build/hair-recovery-mock'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$source = Get-Content -Raw (Join-Path $PSScriptRoot '../src/cpp/HairTintRecovery.cpp')
$start = $source.IndexOf('namespace SOS::HairTintRecovery', [StringComparison]::Ordinal)
if ($start -lt 0) { throw 'Production namespace missing' }
$production = $source.Substring($start)
$cpp = @'
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
namespace RE {
inline unsigned writes = 0, queries = 0;
struct NiColor {
 float red=0, green=0, blue=0; bool tracked=false;
 NiColor()=default;
 NiColor(float r,float g,float b):red(r),green(g),blue(b){}
 NiColor(const NiColor& c):red(c.red),green(c.green),blue(c.blue){}
 NiColor& operator=(const NiColor& c) { if(tracked) ++writes; red=c.red;green=c.green;blue=c.blue;return *this; }
 friend bool operator==(const NiColor& a,const NiColor& b) {return a.red==b.red && a.green==b.green && a.blue==b.blue;}
 friend bool operator!=(const NiColor& a,const NiColor& b) {return !(a==b);}
};
template<class T> struct Ptr { T* p=nullptr; T* get() const {return p;} T* operator->() const{return p;} explicit operator bool()const{return p!=nullptr;} };
struct BSShaderMaterial {
 enum class Type {kLighting,kOther}; enum class Feature {kHairTint,kOther};
 Type type=Type::kLighting; Feature feature=Feature::kHairTint;
 virtual ~BSShaderMaterial()=default;
 Type GetType() const {return type;} Feature GetFeature() const {return feature;}
};
struct BSLightingShaderMaterialHairTint:BSShaderMaterial {NiColor tintColor;};
struct Shader {virtual ~Shader()=default;};
struct BSLightingShaderProperty:Shader {BSShaderMaterial* material=nullptr;};
struct BSGeometry; struct NiNode;
struct NiAVObject {virtual ~NiAVObject()=default;virtual BSGeometry* AsGeometry(){return nullptr;}virtual NiNode* AsNode(){return nullptr;}};
struct BSGeometry:NiAVObject {struct Data {Ptr<Shader> shaderProperty;} data;Data& GetGeometryRuntimeData(){return data;}BSGeometry* AsGeometry()override{return this;}};
struct NiNode:NiAVObject {std::vector<Ptr<NiAVObject>> children;NiNode* AsNode()override{return this;}auto& GetChildren(){return children;}};
struct BipedAnim {struct Object {Ptr<NiAVObject> partClone;};std::array<Object,32> objects;};
struct BGSHeadPart {enum class HeadPartType{kHair};};
struct TESObjectREFR {virtual ~TESObjectREFR()=default;};
struct Actor:TESObjectREFR {
 NiAVObject* hair=nullptr; std::array<BipedAnim,2> biped; bool loaded=true;
 bool Is3DLoaded(){++queries;return loaded;}
 NiAVObject* GetHeadPartObject(BGSHeadPart::HeadPartType){++queries;return hair;}
 Ptr<BipedAnim> GetBiped(bool first){++queries;return {&biped[first]};}
};
struct PlayerCharacter:Actor {static PlayerCharacter* singleton;static PlayerCharacter* GetSingleton(){++queries;return singleton;}};
PlayerCharacter* PlayerCharacter::singleton=nullptr;
struct UI {bool open=true;static UI* singleton;static UI* GetSingleton(){++queries;return singleton;}bool IsMenuOpen(const char* name){CHECK(std::string_view(name)=="RaceSex Menu");return open;}};
UI* UI::singleton=nullptr;
enum class BSEventNotifyControl{kContinue};
template<class T> struct BSTEventSource;
template<class T> struct BSTEventSink {virtual ~BSTEventSink()=default;virtual BSEventNotifyControl ProcessEvent(const T*,BSTEventSource<T>*)=0;};
template<class T> struct BSTEventSource {
 BSTEventSink<T>* sink=nullptr;unsigned registrations=0;
 void AddEventSink(BSTEventSink<T>* s){sink=s;++registrations;}
 void Send(const T* e){CHECK(sink);sink->ProcessEvent(e,this);}
};
}
template<class T> T netimmerse_cast(RE::Shader* s){return dynamic_cast<T>(s);}
namespace SKSE {
struct NiNodeUpdateEvent {RE::TESObjectREFR* reference;};
RE::BSTEventSource<NiNodeUpdateEvent> events;
RE::BSTEventSource<NiNodeUpdateEvent>* GetNiNodeUpdateEventSource(){return &events;}
namespace log {inline unsigned infos=0;template<class...T>void debug(const char*,T&&...){}template<class...T>void info(const char*,T&&...){++infos;}}
}
'@
$cpp += $production
$cpp += @'
struct Fixture {
 RE::PlayerCharacter player; RE::Actor npc; RE::UI ui;
 RE::NiNode source, target;
 RE::BSGeometry sg,tg,sg2,tg2;
 RE::BSLightingShaderProperty sp,tp,sp2,tp2;
 RE::BSLightingShaderMaterialHairTint sm,tm,sm2;
 Fixture() {
  SOS::HairTintRecovery::Reset();RE::writes=0;SKSE::log::infos=0;
  RE::PlayerCharacter::singleton=&player;RE::UI::singleton=&ui;
  player.hair=&source;player.biped[0].objects[1].partClone.p=&target;
  source.children={{&sg}};target.children={{&tg}};
  sg.data.shaderProperty.p=&sp;tg.data.shaderProperty.p=&tp;
  sg2.data.shaderProperty.p=&sp2;tg2.data.shaderProperty.p=&tp2;
  sp.material=&sm;sp2.material=&sm2;tp.material=&tm;tp2.material=&tm;
  sm.tintColor=RE::NiColor(.2f,.4f,.6f);sm2.tintColor=sm.tintColor;
  tm.tintColor=RE::NiColor(.9f,.8f,.7f);tm.tintColor.tracked=true;
 }
 void event(RE::TESObjectREFR* p=nullptr){SKSE::NiNodeUpdateEvent e{p?p:&player};auto q=RE::queries;SKSE::events.Send(&e);CHECK(RE::queries==q);}
 void drain(std::uint32_t mask=2){SOS::HairTintRecovery::Drain(&player,mask);}
 void unchanged(){CHECK(RE::writes==0);CHECK(SKSE::log::infos==0);}
};
int main(){
 using namespace SOS::HairTintRecovery;
 Install();Install();CHECK(SKSE::events.registrations==1);
 {Fixture f;f.drain();f.unchanged();}
 {Fixture f;f.event(&f.npc);f.drain();f.unchanged();}
 {Fixture f;f.event();f.ui.open=false;f.drain();f.ui.open=true;f.drain();f.unchanged();}
 {Fixture f;f.event();f.drain(0);f.drain();f.unchanged();}
 {Fixture f;f.player.hair=nullptr;f.event();f.drain();f.unchanged();}
 {Fixture f;f.source.children.push_back({&f.sg2});f.sm2.tintColor.red=.7f;f.event();f.drain();f.unchanged();}
 {Fixture f;f.sm.tintColor.red=std::numeric_limits<float>::quiet_NaN();f.event();f.drain();f.unchanged();}
 {Fixture f;f.sm.tintColor.blue=std::numeric_limits<float>::infinity();f.event();f.drain();f.unchanged();}
 {Fixture f;f.tp.material=&f.sm;f.sm.tintColor.tracked=true;f.event();f.drain();f.unchanged();}
 {Fixture f;f.target.children.push_back({&f.tg2});f.player.biped[1].objects[1].partClone.p=&f.target;f.event();f.drain();CHECK(RE::writes==1);CHECK(SKSE::log::infos==1);CHECK(f.tm.tintColor==f.sm.tintColor);}
 {Fixture f;f.event();f.drain();CHECK(RE::writes==1);CHECK(f.tm.tintColor==f.sm.tintColor);f.tm.tintColor.red=.1f;f.drain();CHECK(RE::writes==1);CHECK(f.tm.tintColor.red==.1f);}
 {Fixture f;f.event();Reset();f.drain();f.unchanged();}
 {Fixture f;f.player.biped[0].objects[1].partClone.p=&f.source;f.event();f.drain();f.unchanged();}
 {Fixture f;f.target.children.push_back({&f.source});f.event();f.drain();f.unchanged();}
 {Fixture f;f.target.children.push_back({&f.sg});f.sm.tintColor.tracked=true;f.event();f.drain();CHECK(RE::writes==1);CHECK(f.tm.tintColor==f.sm.tintColor);}
 {Fixture f;f.tm.feature=RE::BSShaderMaterial::Feature::kOther;f.event();f.drain();f.unchanged();}
 {Fixture f;f.tm.type=RE::BSShaderMaterial::Type::kOther;f.event();f.drain();f.unchanged();}
 {Fixture f;f.sm.feature=RE::BSShaderMaterial::Feature::kOther;f.event();f.drain();f.unchanged();}
 {Fixture f;std::array<RE::BSGeometry,65> g;for(auto& v:g){v.data.shaderProperty.p=&f.tp;f.target.children.push_back({&v});}f.event();f.drain();f.unchanged();}
 {Fixture f;std::array<RE::NiNode,513> n;for(auto& v:n)f.target.children.push_back({&v});f.event();f.drain();f.unchanged();}
 {Fixture f;std::array<RE::NiNode,8> n;for(unsigned i=0;i<8;++i)f.player.biped[0].objects[i].partClone.p=&n[i];f.event();f.drain(255);f.unchanged();}
 {Fixture f;std::array<RE::NiNode,65> n;f.source.children={{&n[0]}};for(unsigned i=0;i<64;++i)n[i].children={{&n[i+1]}};n[64].children={{&f.sg}};f.event();f.drain();f.unchanged();}
 {Fixture f;f.source.children.push_back({&f.source});f.target.children.push_back({&f.target});f.event();f.drain();CHECK(RE::writes==1);}
 {Fixture f;f.player.loaded=false;f.event();f.drain();f.unchanged();}
 {Fixture f;f.player.biped[0].objects[1].partClone.p=nullptr;f.player.biped[1].objects[1].partClone.p=&f.target;f.event();f.drain();CHECK(RE::writes==1);}
 std::puts("HAIR_RECOVERY_MOCK_OK: 25 scenarios; entire production namespace, not engine/event ABI validation");
}
'@
Set-Content -Path (Join-Path $work 'test_hair_recovery.cpp') -Value $cpp -Encoding utf8
$vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "Compiler environment unavailable: $vcvars" }
$cmd = 'call "' + $vcvars + '" >nul && cl /nologo /EHsc /std:c++20 test_hair_recovery.cpp /Fe:test_hair_recovery.exe && test_hair_recovery.exe'
Push-Location $work
try { cmd /c $cmd; if ($LASTEXITCODE -ne 0) { throw 'Hair recovery mock regression failed' } } finally { Pop-Location }
