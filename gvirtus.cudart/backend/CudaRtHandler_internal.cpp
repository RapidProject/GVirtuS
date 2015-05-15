/*
 * gVirtuS -- A GPGPU transparent virtualization component.
 *
 * Copyright (C) 2009-2010  The University of Napoli Parthenope at Naples.
 *
 * This file is part of gVirtuS.
 *
 * gVirtuS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gVirtuS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gVirtuS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by: Giuseppe Coviello <giuseppe.coviello@uniparthenope.it>,
 *             Department of Applied Science
 */

#include "CudaRtHandler.h"

#include <string>
#include <iostream>
#include <cstdio>
#include <vector>

#include "CudaUtil.h"

using namespace std;

typedef struct __cudaFatCudaBinary2HeaderRec {
    unsigned int            magic;
    unsigned int            version;
    unsigned long long int  length;
} __cudaFatCudaBinary2Header;

enum FatBin2EntryType {
        FATBIN_2_PTX = 0x1
};

typedef struct __cudaFatCudaBinary2EntryRec {
        unsigned int           type;
        unsigned int           binary;
        unsigned long long int binarySize;
        unsigned int           unknown2;
        unsigned int           kindOffset;
        unsigned int           unknown3;
        unsigned int           unknown4;
        unsigned int           name;
        unsigned int           nameSize;
        unsigned long long int flags;
        unsigned long long int unknown7;
        unsigned long long int uncompressedBinarySize;
} __cudaFatCudaBinary2Entry;


long long COMPRESSED_PTX=0x0000000000001000LL;

typedef struct __cudaFatCudaBinaryRec2 {
        int magic;
        int version;
        const unsigned long long* fatbinData;
        char* f;
} __cudaFatCudaBinary2;







extern "C" {
    extern void** __cudaRegisterFatBinary(void *fatCubin);
    extern void __cudaUnregisterFatBinary(void **fatCubinHandle);
    extern void __cudaRegisterFunction(void **fatCubinHandle,
            const char *hostFun, char *deviceFun, const char *deviceName,
            int thread_limit, uint3 *tid, uint3 *bid, dim3 *bDim, dim3 *gDim,
            int *wSize);
    extern void __cudaRegisterVar(void **fatCubinHandle, char *hostVar,
            char *deviceAddress, const char *deviceName, int ext, int size,
            int constant, int global);
    extern void __cudaRegisterSharedVar(void **fatCubinHandle, void **devicePtr,
            size_t size, size_t alignment, int storage);
    extern void __cudaRegisterShared(void **fatCubinHandle, void **devicePtr);
    extern void __cudaRegisterTexture(void **fatCubinHandle,
            const textureReference *hostVar, void **deviceAddress, char *deviceName,
            int dim, int norm, int ext);
}

static bool initialized = false;
static char **constStrings;
static size_t constStrings_size = 0;
static size_t constStrings_length = 0;
static void ** fatCubinHandlers[2048];
static void * fatCubins[2048];
static const textureReference * texrefHandlers[2048];
static const textureReference * texref[2048];

static void init() {
    constStrings_size = 2048;
    constStrings = (char **) malloc(sizeof (char *) * constStrings_size);
    for (int i = 0; i < 2048; i++) {
        constStrings[i] = NULL;
        fatCubinHandlers[i] = NULL;
        fatCubins[i] = NULL;
    }
    initialized = true;
}

const char *get_const_string(const char *s) {
    if (!initialized)
        init();
    size_t i;
    for (i = 0; i < constStrings_length; i++)
        if (!strcmp(s, constStrings[i]))
            return constStrings[i];
    if (i >= constStrings_size) {
        constStrings_size += 2048;
        constStrings = (char **) realloc(constStrings, sizeof (char *) * constStrings_size);
    }
    constStrings[constStrings_length] = strdup(s);
    return constStrings[constStrings_length++];
}

void addFatBinary(void **handler, void *bin) {
    if (!initialized)
        init();
    int i;
    for (i = 0; fatCubinHandlers[i] != NULL && i < 2048; i++);
    if (i >= 2048)
        throw "Ahi ahi ahi";
    fatCubinHandlers[i] = handler;
    fatCubins[i] = bin;
}

void removeFatBinary(void **handler) {
    int i;
    for (i = 0; i < 2048; i++) {
        if (fatCubinHandlers[i] == handler) {
            free(fatCubins[i]);
            fatCubinHandlers[i] = NULL;
            return;
        }
    }

}

void addTexture(struct textureReference *handler,
        struct textureReference *ref) {
    if (!initialized)
        init();
    int i;
    for (i = 0; texrefHandlers[i] != NULL && i < 2048; i++);
    if (i >= 2048)
        throw "Ahi ahi ahi";
    texrefHandlers[i] = handler;
    texref[i] = ref;
}

const textureReference *getTexture(const textureReference *handler) {
    int i;
    for (i = 0; i < 2048; i++) {
        if (texrefHandlers[i] == handler) {
            return texref[i];
        }
    }
    throw "Texture not found!";
    return NULL;
}

/*CUDA_ROUTINE_HANDLER(RegisterFatBinary) {
    char * handler = input_buffer->AssignString();
    __cudaFatCudaBinary * fatBin =
            CudaUtil::UnmarshalFatCudaBinary(input_buffer);
    void **fatCubinHandler = __cudaRegisterFatBinary((void *) fatBin);
    pThis->RegisterFatBinary(handler, fatCubinHandler);
    return new Result(cudaSuccess);
}*/

/*
typedef struct {
  int magic;
  int version;
  const unsigned long long* data;
  void *filename_or_fatbins;

} __fatBinC_Wrapper_t;
*/

CUDA_ROUTINE_HANDLER(RegisterFatBinary) {
    long int cudaFatMAGIC =0x1ee55a01;
    long int cudaFatMAGIC2=0x466243b1;
    long int cudaFatMAGIC3=0xba55ed50;
    long int cudaFatVERSION=0x00000004;
    char * handler = input_buffer->AssignString();
    const char* _name;
    const char* _ptx;  
    __fatBinC_Wrapper_t * fatBin = CudaUtil::UnmarshalFatCudaBinaryV2(input_buffer);
    //cout << "UnmarshalFatCudaBinaryV2 called" << endl;
    //cout << "data: " << fatBin->data[0] << endl;
    //fprintf(stderr,"data= %p\n",fatBin->data);
    void **bin = __cudaRegisterFatBinary((void *) fatBin);
    
    if(*((int*)(*bin)) == cudaFatMAGIC2) {
        __cudaFatCudaBinary2* binary = (__cudaFatCudaBinary2*) *bin;
        //fprintf(stderr,"magic: %x\n",binary->magic);
        //fprintf(stderr,"version: %x\n",binary->version);
        __cudaFatCudaBinary2Header* header = (__cudaFatCudaBinary2Header*)binary->fatbinData;
        //fprintf(stderr,"Size: %d bytes\n",header->length);
        char* base = (char*)(header + 1);
        long long unsigned int offset = 0;
        __cudaFatCudaBinary2EntryRec* entry = (__cudaFatCudaBinary2EntryRec*)(base);
        while (!(entry->type & FATBIN_2_PTX) && offset < header->length) {
           entry = (__cudaFatCudaBinary2EntryRec*)(base + offset);
           offset += entry->binary + entry->binarySize;
        }
        _name = (char*)entry + entry->name;

        if (entry->type & FATBIN_2_PTX) {
            _ptx  = (char*)entry + entry->binary;
        } else {
          _ptx = 0;
        }

        fprintf(stderr,"Filename: %s \n",_name);
/*
    if(entry->flags & COMPRESSED_PTX)
    {
      _decompressedPTX.resize(entry->uncompressedBinarySize + 1);
      _decompressPTX(entry->binarySize);
    }
*/
    }
    

    //fprintf(stderr, "fatCubinHandler: %p\n", *fatCubinHandler);
    pThis->RegisterFatBinary(handler, bin);
    //for( int i=0; i< 2048;i++)
    //    fprintf(stderr,"%x ",(((char*)(*fatCubinHandler))+i));
    //fprintf(stderr,"\n");
    return new Result(cudaSuccess);
}




CUDA_ROUTINE_HANDLER(UnregisterFatBinary) {
    char * handler = input_buffer->AssignString();
    void **fatCubinHandle = pThis->GetFatBinary(handler);

    __cudaUnregisterFatBinary(fatCubinHandle);

    pThis->UnregisterFatBinary(handler);

    return new Result(cudaSuccess);
}

CUDA_ROUTINE_HANDLER(RegisterFunction) {
    long int cudaFatMAGIC =0x1ee55a01;
    long int cudaFatMAGIC2=0x466243b1;
    long int cudaFatMAGIC3=0xba55ed50;
    long int cudaFatVERSION=0x00000004;
    const char* _name;
    const char* _ptx;  
    fprintf(stderr, "Handling RegisterFunction\n"); 
    char * handler = input_buffer->AssignString();
    void **fatCubinHandle = pThis->GetFatBinary(handler);
    if(*((int*)(*fatCubinHandle)) == cudaFatMAGIC2) {
        __cudaFatCudaBinary2* binary = (__cudaFatCudaBinary2*) *fatCubinHandle;
        //fprintf(stderr,"magic: %x\n",binary->magic);
        //fprintf(stderr,"version: %x\n",binary->version);
        __cudaFatCudaBinary2Header* header = (__cudaFatCudaBinary2Header*)binary->fatbinData;
        //fprintf(stderr,"Size: %d bytes\n",header->length);
        char* base = (char*)(header + 1);
        long long unsigned int offset = 0;
        __cudaFatCudaBinary2EntryRec* entry = (__cudaFatCudaBinary2EntryRec*)(base);
        while (!(entry->type & FATBIN_2_PTX) && offset < header->length) {
           entry = (__cudaFatCudaBinary2EntryRec*)(base + offset);
           offset += entry->binary + entry->binarySize;
        }
        _name = (char*)entry + entry->name;

        if (entry->type & FATBIN_2_PTX) {
            _ptx  = (char*)entry + entry->binary;
        } else {
            _ptx = 0;
        }

        fprintf(stderr,"Filename: %s\n",_name);
/*
        if(entry->flags & COMPRESSED_PTX) {
            _decompressedPTX.resize(entry->uncompressedBinarySize + 1);
            _decompressPTX(entry->binarySize);
        }
*/
    }   
 
    //const char *hostfun = strdup(input_buffer->AssignString());
    const char* hostfun = (const char*)(input_buffer->Get<uint64_t> ());
    //const char *hostfun = (const char*)input_buffer->Assign<size_t>();
    //const char * entry = (char*)malloc(CudaUtil::MarshaledHostPointerSize);
    //char * entry;
    //sscanf(hostfun, "%p", &entry);
    
    char *deviceFun = strdup(input_buffer->AssignString());


    //fprintf(stderr, "Handler: %s\n", handler);
    //fprintf(stderr,"*****hostfun string: %p\n",hostfun);
    const char *deviceName = strdup(input_buffer->AssignString());
    int thread_limit = input_buffer->Get<int>();
    uint3 *tid = input_buffer->Assign<uint3 > ();
    uint3 *bid = input_buffer->Assign<uint3 > ();
    dim3 *bDim = input_buffer->Assign<dim3 > ();
    dim3 *gDim = input_buffer->Assign<dim3 > ();
    int *wSize = input_buffer->Assign<int>();
    //sprintf(entry,"%zu",hostfun);
    //fprintf(stderr, "handler: %s hostFun: %s fatCubinHandle: %p deviceName: %s deviceFun: %s\n", handler, hostfun, fatCubinHandle, deviceName, deviceFun);
    //fprintf(stderr,"*****entry pointer: %p\n",hostfun);
    
    __cudaRegisterFunction(fatCubinHandle, hostfun, deviceFun, deviceName,
            thread_limit, tid, bid, bDim, gDim, wSize);

    //fprintf(stderr,"*****after __cudaRegisterFunction\n");
    //pThis->RegisterDeviceFunction(hostfun, deviceFun);
    //fprintf(stderr,"*****after RegisterDeviceFunction\n");
    Buffer * output_buffer = new Buffer();
    output_buffer->AddString(deviceFun);
    output_buffer->Add(tid);
    output_buffer->Add(bid);
    output_buffer->Add(bDim);
    output_buffer->Add(gDim);
    output_buffer->Add(wSize);

    return new Result(cudaSuccess, output_buffer);
}

CUDA_ROUTINE_HANDLER(RegisterVar) {
    char * handler = input_buffer->AssignString();
    void **fatCubinHandle = pThis->GetFatBinary(handler);
    char *hostVar = (char *)
        CudaUtil::UnmarshalPointer(input_buffer->AssignString());
    char *deviceAddress = strdup(input_buffer->AssignString());
    const char *deviceName = strdup(input_buffer->AssignString());
    int ext = input_buffer->Get<int>();
    int size = input_buffer->Get<int>();
    int constant = input_buffer->Get<int>();
    int global = input_buffer->Get<int>();

    __cudaRegisterVar(fatCubinHandle, hostVar, deviceAddress, deviceName, ext,
            size, constant, global);

    //cout << "Registered Var " << deviceAddress << " with handler "
    //        << (void *) hostVar << endl;

    return new Result(cudaSuccess);
}

CUDA_ROUTINE_HANDLER(RegisterSharedVar) {
    char * handler = input_buffer->AssignString();
    void **fatCubinHandle = pThis->GetFatBinary(handler);
    void **devicePtr = (void **) input_buffer->AssignString();
    size_t size = input_buffer->Get<size_t>();
    size_t alignment = input_buffer->Get<size_t>();
    int storage = input_buffer->Get<int>();

    __cudaRegisterSharedVar(fatCubinHandle, devicePtr, size, alignment, storage);

    cout << "Registered SharedVar " << (char *) devicePtr << endl;

    return new Result(cudaSuccess);
}

CUDA_ROUTINE_HANDLER(RegisterShared) {
    char * handler = input_buffer->AssignString();
    void **fatCubinHandle = pThis->GetFatBinary(handler);
    char *devPtr = strdup(input_buffer->AssignString());
    __cudaRegisterShared(fatCubinHandle, (void **) devPtr);
    cout << "Registerd Shared " << (char *) devPtr << " for " << fatCubinHandle << endl;
    return new Result(cudaSuccess);
}

CUDA_ROUTINE_HANDLER(RegisterTexture) {
    char * handler = input_buffer->AssignString();
    void **fatCubinHandle = pThis->GetFatBinary(handler);

    textureReference * texture = new textureReference;
    memmove(texture, input_buffer->Assign<textureReference>(),
        sizeof (textureReference));

    void *hostVarPtr = (void *) input_buffer->Get<uint64_t>();
    addTexture((textureReference *) hostVarPtr, texture);

    const char *deviceAddress = get_const_string(input_buffer->AssignString());
    const char *deviceName = get_const_string(input_buffer->AssignString());

    int dim = input_buffer->Get<int>();
    int norm = input_buffer->Get<int>();
    int ext = input_buffer->Get<int>();
    
    __cudaRegisterTexture(fatCubinHandle, texture, (void **) deviceAddress,
           (char *) deviceName, dim, norm, ext);
#if 0
    handler = input_buffer->AssignString();
    textureReference *hostVar = new textureReference;
    memmove(hostVar, input_buffer->Assign<textureReference > (),
            sizeof (textureReference));
    void **deviceAddress = (void **) input_buffer->AssignAll<char>();
    char *deviceName = strdup(input_buffer->AssignString());
    int dim = input_buffer->Get<int>();
    int norm = input_buffer->Get<int>();
    int ext = input_buffer->Get<int>();

    __cudaRegisterTexture(fatCubinHandle, hostVar, deviceAddress, deviceName,
            dim, norm, ext);

    pThis->RegisterTexture(handler, hostVar);
#endif
    return new Result(cudaSuccess);
}
