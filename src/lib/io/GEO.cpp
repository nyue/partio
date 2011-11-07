/*
PARTIO SOFTWARE
Copyright 2010 Disney Enterprises, Inc. All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

* The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
Studios" or the names of its contributors may NOT be used to
endorse or promote products derived from this software without
specific prior written permission from Walt Disney Pictures.

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
*/

#include "../Partio.h"
#include "../core/ParticleHeaders.h"
#include "ZIP.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <memory>


namespace Partio{

template<ParticleAttributeType ETYPE>
void readGeoAttr(std::istream& f,const ParticleAttribute& attr,ParticleAccessor& accessor,ParticlesDataMutable::iterator& iterator)
{
    //std::cout<<"reading "<<TypeName(attr.type)<<std::endl;
    typedef typename ETYPE_TO_TYPE<ETYPE>::TYPE TYPE;
    TYPE* data=accessor.raw<TYPE>(iterator);
    for(int k=0;k<attr.count;k++){
        f>>data[k];
    }
}

std::string scanString(std::istream& input)
{
    // TODO: this code does not check for buf overrun
    // TODO: this code doesn't properly check for FEOF condition
    char buf[4096];
    char *ptr=buf;
    char c;
    while(input.good()){
        input.get(c);
        if(!isspace(c)) break;
    }
    if(!input.good()) return "";

    if(c!='"'){
        while(input.good()){
            *ptr++=c;
            input.get(c);
            if(isspace(c)) break;
        }
    }else{
        while(input.good()){
            input.get(c);
            if(c=='\\'){
                input.get(c);
                *ptr++=c;
            }else if(c=='"'){
                break;
            }else{
                *ptr++=c;
            }
        }
    }
    *ptr++=0;
    return std::string(buf);
}

ParticlesDataMutable* readGEO(const char* filename,const bool headersOnly)
{
    std::auto_ptr<std::istream> input(Gzip_In(filename,std::ios::in));
    if(!*input){
        std::cerr<<"Partio: Can't open particle data file: "<<filename<<std::endl;
        return 0;
    }
    int NPoints=0, NPointAttrib=0;

    ParticlesDataMutable* simple=0;
    if(headersOnly) simple=new ParticleHeaders;
    else simple=create();

    // read NPoints and NPointAttrib
    std::string word;
    while(input->good()){
        *input>>word;
        if(word=="NPoints") *input>>NPoints;
        else if(word=="NPointAttrib"){*input>>NPointAttrib;break;}
    }
    // skip until PointAttrib
    while(input->good()){
        *input>>word;
        if(word=="PointAttrib") break;
    }
    // read attribute descriptions
    int attrInfoRead = 0;
    
    ParticleAttribute positionAttr=simple->addAttribute("position",VECTOR,3);
    ParticleAccessor positionAccessor(positionAttr);

    std::vector<ParticleAttribute> attrs;
    std::vector<ParticleAccessor> accessors;
    while (input->good() && attrInfoRead < NPointAttrib) {
        std::string attrName, attrType;
        int nvals = 0;
        *input >> attrName >> nvals >> attrType;
        if(attrType=="index"){
            std::cerr<<"Partio: attr '"<<attrName<<"' of type index (string) found, treating as integer"<<std::endl;
            int nIndices=0;
            *input>>nIndices;
            for(int j=0;j<nIndices;j++){
                std::string indexName;
                //*input>>indexName;
                indexName=scanString(*input);
                std::cerr<<"Partio:    index "<<j<<" is "<<indexName<<std::endl;
            }
            attrs.push_back(simple->addAttribute(attrName.c_str(),INT,1));
            accessors.push_back(ParticleAccessor(attrs.back()));
            attrInfoRead++;
            
        }else{
            for (int i=0;i<nvals;i++) {
                float defval;
                *input>>defval;
            }
            ParticleAttributeType type;
            // TODO: fix for other attribute types
            if(attrType=="float") type=FLOAT;
            else if(attrType=="vector") type=VECTOR;
            else if(attrType=="int") type=INT;
            else{
                std::cerr<<"Partio: unknown attribute "<<attrType<<" type... aborting"<<std::endl;
                type=NONE;
            }
            attrs.push_back(simple->addAttribute(attrName.c_str(),type,nvals));
            accessors.push_back(ParticleAccessor(attrs.back()));
            attrInfoRead++;
        }
    }

    simple->addParticles(NPoints);

    ParticlesDataMutable::iterator iterator=simple->begin();
    iterator.addAccessor(positionAccessor);
    for(size_t i=0;i<accessors.size();i++) iterator.addAccessor(accessors[i]);

    if(headersOnly) return simple; // escape before we try to touch data
    
    
    float fval;
    // TODO: fix
    for(ParticlesDataMutable::iterator end=simple->end();iterator!=end  && input->good();++iterator){
        float* posInternal=positionAccessor.raw<float>(iterator); 
        for(int i=0;i<3;i++) *input>>posInternal[i];
        *input>>fval;
        //std::cout<<"saw "<<posInternal[0]<<" "<<posInternal[1]<<" "<<posInternal[2]<<std::endl;
        
        // skip open paren
        char paren = 0;
        *input>> paren;
        if (paren != '(') break;
        
        // read additional attribute values
        for (unsigned int i=0;i<attrs.size();i++){
            switch(attrs[i].type){
                case NONE: assert(false);break;
                case FLOAT: readGeoAttr<FLOAT>(*input,attrs[i],accessors[i],iterator);break;
                case VECTOR: readGeoAttr<VECTOR>(*input,attrs[i],accessors[i],iterator);break;
                case INT: readGeoAttr<INT>(*input,attrs[i],accessors[i],iterator);break;
            }
        }
        // skip closing parenthes
        *input >> paren;
        if (paren != ')') break;
    }
    return simple;
}


template<class T>
void writeType(std::ostream& output,const ParticlesData& p,const ParticleAttribute& attrib,
    const ParticleAccessor& accessor,const ParticlesData::const_iterator& iterator)
{
    const T* data=accessor.raw<T>(iterator);
    for(int i=0;i<attrib.count;i++){
        if(i>0) output<<" ";
        output<<data[i];
    }
}

bool writeGEO(const char* filename,const ParticlesData& p,const bool compressed)
{
    std::auto_ptr<std::ostream> output(
        compressed ? 
        Gzip_Out(filename,std::ios::out)
        :new std::ofstream(filename,std::ios::out));

    *output<<"PGEOMETRY V5"<<std::endl;
    *output<<"NPoints "<<p.numParticles()<<" NPrims "<<1<<std::endl;
    *output<<"NPointGroups "<<0<<" NPrimGroups "<<0<<std::endl;
    *output<<"NPointAttrib "<<p.numAttributes()-1<<" NVertexAttrib "<<0<<" NPrimAttrib 1 NAttrib 0"<<std::endl;

    if(p.numAttributes()-1>0) *output<<"PointAttrib"<<std::endl;
    ParticleAttribute positionHandle;
    ParticleAccessor positionAccessor(positionHandle);
    bool foundPosition=false;
    std::vector<ParticleAttribute> handles;
    std::vector<ParticleAccessor> accessors;
    for(int i=0;i<p.numAttributes();i++){
        ParticleAttribute attrib;
        p.attributeInfo(i,attrib);
        if(attrib.name=="position"){
            positionHandle=attrib;
            positionAccessor=ParticleAccessor(positionHandle);
            foundPosition=true;
        }else{
            handles.push_back(attrib);
            accessors.push_back(ParticleAccessor(handles.back()));
            std::string typestring;
            switch(attrib.type){
                case NONE: assert(false);typestring="int";break;
                case VECTOR: typestring="vector";break;
                case FLOAT: typestring="float";break;
                case INT: typestring="int";break;
                case INDEXEDSTR: typestring="index";break;
            }
            *output<<attrib.name<<" "<<attrib.count<<" "<<typestring;
            for(int k=0;k<attrib.count;k++) *output<<" "<<0;
        }
        *output<<std::endl;
    }
    if(!foundPosition){
        std::cerr<<"Partio: didn't find attr 'position' while trying to write GEO"<<std::endl;
        return false;
    }

    ParticlesData::const_iterator iterator=p.begin();
    iterator.addAccessor(positionAccessor);
    for(size_t i=0;i<accessors.size();i++) iterator.addAccessor(accessors[i]);

    for(ParticlesData::const_iterator end=p.end();iterator!=end;++iterator){
        const Data<float,3>& point=positionAccessor.data<Data<float,3> >(iterator);
        *output<<point[0]<<" "<<point[1]<<" "<<point[2]<<" 1";
        if(handles.size()) *output<<" (";
        for(unsigned int aindex=0;aindex<handles.size();aindex++){
            if(aindex>0) *output<<"\t";
            ParticleAttribute& handle=handles[aindex];
            ParticleAccessor& accessor=accessors[aindex];
            switch(handle.type){
                case NONE: assert(false);break;
                case FLOAT: writeType<ETYPE_TO_TYPE<FLOAT>::TYPE>(*output,p,handle,accessor,iterator);break;
                case INT: writeType<ETYPE_TO_TYPE<INT>::TYPE>(*output,p,handle,accessor,iterator);break;
                case VECTOR: writeType<ETYPE_TO_TYPE<VECTOR>::TYPE>(*output,p,handle,accessor,iterator);break;
            }
        }        
        if(handles.size()) *output<<")";
        *output<<std::endl;
    }

    *output<<"PrimitiveAttrib"<<std::endl;
    *output<<"generator 1 index 1 papi"<<std::endl;
    *output<<"Part "<<p.numParticles();
    for(int i=0;i<p.numParticles();i++)
        *output<<" "<<i;
    *output<<" [0]\nbeginExtra"<<std::endl;
    *output<<"endExtra"<<std::endl;

    // success
    return true;
}

}
