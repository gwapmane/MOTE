#include <cstdio>
#include "../core/grain.h"
#include "../core/sources.h"
int main(){
  mote::Engine e; e.prepare(48000);
  auto s = mote::sources::make(1, 48000);
  e.setSamples(s.get(), s.get());
  mote::Params p; p.density=0.6f; 
  e.setParams(p); e.noteOn(60,1.0f);
  std::vector<float> l(48000), r(48000);
  e.process(l.data(), r.data(), 48000);
  double sum=0; float pk=0; for(int i=0;i<48000;++i){ sum+=l[i]*l[i]; pk=std::max(pk,std::fabs(l[i])); }
  std::printf("rms %.4f peak %.4f grains %d\n", std::sqrt(sum/48000), pk, e.activeGrains());
}
