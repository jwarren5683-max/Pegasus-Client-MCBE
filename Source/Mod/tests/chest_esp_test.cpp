#include "../src/modules/ChestEspContainers.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace utility::modules::chest_esp;
void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
int main(){
    Storage storage{{-1,-60,-17},{-1,-60,-17,0,-59,-16},Kind::chest};
    check(valid_bounds(storage),"negative-coordinate chest bounds valid");
    storage.bounds[3]=-1;check(!valid_bounds(storage),"zero-size bounds rejected");
    storage.bounds[3]=0;storage.bounds[1]=std::numeric_limits<float>::quiet_NaN();
    check(!valid_bounds(storage),"torn non-finite bounds rejected");
    storage.bounds[1]=-60;storage.position.x=std::numeric_limits<int>::max();
    check(!valid_bounds(storage),"corrupt world coordinate rejected before adding one");
    storage.position.x=-1;storage.bounds[0]=-100;check(!valid_bounds(storage),"wrong-position actor bounds rejected");
    check(read<unsigned>(reinterpret_cast<void*>(1))==0,"retired actor read fails safely");
    check(identifier(reinterpret_cast<void*>(1)).empty(),"retired block identifier fails safely");
    check(classify("minecraft:chest")==Kind::chest&&classify("minecraft:trapped_chest")==Kind::trapped&&
        classify("minecraft:barrel")==Kind::barrel&&classify("minecraft:waxed_oxidized_copper_chest")==Kind::copper&&
        classify("minecraft:blue_shulker_box")==Kind::shulker,"five independent storage families classified");
    // Replay the actual RPM/map traversal without calling Minecraft functions.
    std::array<unsigned char,0x260> player{};int dimension{},other_dimension{};
    std::array<unsigned char,0x50> region{};
    std::array<unsigned char,0x88> source{};
    std::array<unsigned char,32> chunks_head{},chunk_node{},actors_head{},actor_node{};
    std::array<unsigned char,0x13B0> chunk{};
    std::array<unsigned char,0x80> actor{};
    std::array<unsigned char,0x70> block{};
    std::array<unsigned char,0x110> legacy{};
    const auto put=[]<class T>(auto& v,std::size_t at,T value){std::memcpy(v.data()+at,&value,sizeof(value));};
    put(player,0x1C8,static_cast<void*>(&dimension));
    put(region,0x3A,short{-64});put(region,0x38,short{320});
    put(source,0x78,static_cast<void*>(chunks_head.data()));put(source,0x80,std::size_t{1});
    put(chunks_head,0,static_cast<void*>(chunk_node.data()));put(chunk_node,0,static_cast<void*>(chunks_head.data()));
    put(chunk_node,0x10,int{-1});put(chunk_node,0x14,int{-2});put(chunk_node,0x18,static_cast<void*>(chunk.data()));
    put(chunk,0x78,int{-1});put(chunk,0x7C,int{-2});
    put(chunk,0x13A0,static_cast<void*>(actors_head.data()));put(chunk,0x13A8,std::size_t{1});
    put(actors_head,0,static_cast<void*>(actor_node.data()));put(actor_node,0,static_cast<void*>(actors_head.data()));
    put(actor_node,0x10,std::uint32_t{0x40F0F});put(actor_node,0x18,static_cast<void*>(actor.data()));
    const Position position{-1,-60,-17};put(actor,8,position);
    put(block,0x68,static_cast<void*>(legacy.data()));
    constexpr char name[]="minecraft:chest";std::memcpy(legacy.data()+0xE8,name,sizeof(name));
    put(legacy,0xF8,std::size_t{sizeof(name)-1});put(legacy,0x100,std::size_t{15});
    unsigned chunks{},blocks{};std::vector<Storage> result;
    const auto get_chunk=[&](void*,const void*)->void*{++chunks;return chunk.data();};
    const auto get_block=[&](void*,const Position* p)->void*{++blocks;check(p->x==-1&&p->y==-60&&p->z==-17,"correct block query coordinates");return block.data();};
    const float bounds[]{-1,-60,-17,0,-59,-16};
    for(const auto& layout:{profile_12645,profile_12650}){
        actor.fill(0);put(actor,8,position);std::memcpy(actor.data()+layout.bounds,bounds,sizeof(bounds));
        check(capture_loaded(player.data(),&dimension,region.data(),source.data(),layout,result,get_chunk,get_block)&&
            result.size()==1&&result[0].kind==Kind::chest,"both native bounds profiles publish the placed chest");
    }
    const auto before=blocks;put(actor_node,0x10,std::uint32_t{0});
    check(capture_loaded(player.data(),&dimension,region.data(),source.data(),profile_12650,result,get_chunk,get_block)&&
        result.empty()&&blocks==before,"stale packed block position rejected before any native block lookup");
    put(actor_node,0x10,std::uint32_t{0x40F0F});put(player,0x1C8,static_cast<void*>(&other_dimension));
    check(!capture_loaded(player.data(),&dimension,region.data(),source.data(),profile_12650,result,get_chunk,get_block)&&result.empty(),"dimension transition clears the whole published storage frame");
    put(player,0x1C8,static_cast<void*>(&dimension));put(chunk_node,0,reinterpret_cast<void*>(1));
    check(!capture_loaded(player.data(),&dimension,region.data(),source.data(),profile_12650,result,get_chunk,get_block)&&result.empty(),"broken loaded chunk list fails closed");
    std::puts("PASS: chest bounds, identifiers and retired-storage reads");
}

