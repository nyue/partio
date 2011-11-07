

#include <Partio.h>
#include <iostream>
#include <cmath>
#include <stdexcept>

using namespace Partio;
int main(int argc,char *argv[])
{
    ParticlesDataMutable* p=create();
    ParticleAttribute posAttr=p->addAttribute("position",VECTOR,3);
    ParticleAttribute strAttr=p->addAttribute("filename",INDEXEDSTR,1);

    int test1Token=p->registerIndexedStr(strAttr,"test1");
    int test2Token=p->registerIndexedStr(strAttr,"test2");
    
    int p2=p->addParticle();
    for(int i=0;i<5;i++){
        int p1=p->addParticle();
        float* f=p->dataWrite<float>(posAttr,i);
        int* tok=p->dataWrite<int>(strAttr,i);
        tok[0]=i%2==0 ? test1Token : test2Token;
        f[0]=i;
        f[1]=0;
        f[2]=0;
    }


    Partio::write("foo.bgeo",*p);

    return 0;
}
