#include "AudioBridge.hpp"
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
namespace recordaudio_bridge {
namespace {
constexpr char path[]="/storage/emulated/0/Android/media/com.geode.launcher/game/renders/RecordAudio.shared";
constexpr uint32_t magic=0x52415544, rate=48000, frames=rate*8;
struct Ring { uint32_t magicValue,sampleRate,channels,capacity; std::atomic<uint32_t> active; std::atomic<uint64_t> writeFrame,readFrame; alignas(64) float samples[frames*2]; };
Ring* ring=nullptr; int fd=-1,wav=-1; uint32_t dataSize=0;
void w16(uint16_t v){uint8_t b[2]={uint8_t(v),uint8_t(v>>8)};::write(wav,b,2);}
void w32(uint32_t v){uint8_t b[4]={uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)};::write(wav,b,4);}
void header(){::write(wav,"RIFF",4);w32(36+dataSize);::write(wav,"WAVEfmt ",8);w32(16);w16(1);w16(2);w32(rate);w32(rate*4);w16(4);w16(16);::write(wav,"data",4);w32(dataSize);}
}
bool start(std::string const& video){
 stop();fd=open(path,O_RDWR);if(fd<0)return false;void* p=mmap(nullptr,sizeof(Ring),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
 if(p==MAP_FAILED){close(fd);fd=-1;return false;}ring=static_cast<Ring*>(p);
 if(ring->magicValue!=magic||ring->sampleRate!=rate||ring->channels!=2||ring->capacity!=frames){munmap(ring,sizeof(Ring));ring=nullptr;close(fd);fd=-1;return false;}
 std::string out=video;auto dot=out.find_last_of('.');if(dot!=std::string::npos)out.resize(dot);out+=".wav";wav=open(out.c_str(),O_CREAT|O_TRUNC|O_WRONLY,0664);
 if(wav<0){munmap(ring,sizeof(Ring));ring=nullptr;close(fd);fd=-1;return false;}
 dataSize=0;header();ring->readFrame.store(ring->writeFrame.load(std::memory_order_acquire),std::memory_order_release);ring->active.store(1,std::memory_order_release);return true;
}
void pump(){
 if(!ring||wav<0)return;uint64_t r=ring->readFrame.load(std::memory_order_relaxed),w=ring->writeFrame.load(std::memory_order_acquire),n=std::min<uint64_t>(w-r,4096);if(!n)return;
 std::vector<int16_t> pcm(n*2);for(uint64_t i=0;i<n;++i){size_t s=((r+i)%frames)*2;pcm[i*2]=int16_t(std::clamp(ring->samples[s],-1.f,1.f)*32767.f);pcm[i*2+1]=int16_t(std::clamp(ring->samples[s+1],-1.f,1.f)*32767.f);}
 auto* b=reinterpret_cast<uint8_t const*>(pcm.data());size_t bytes=pcm.size()*2,done=0;while(done<bytes){ssize_t x=::write(wav,b+done,bytes-done);if(x<=0)break;done+=size_t(x);}dataSize+=uint32_t(done);ring->readFrame.store(r+n,std::memory_order_release);
}
void stop(){if(ring){ring->active.store(0,std::memory_order_release);pump();}if(wav>=0){lseek(wav,0,SEEK_SET);header();close(wav);wav=-1;}if(ring){munmap(ring,sizeof(Ring));ring=nullptr;}if(fd>=0){close(fd);fd=-1;}}
bool available(){return ring&&wav>=0;}
}
