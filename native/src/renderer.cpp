#include "tenkaichi/runtime.hpp"
#include "resource_shaders.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <memory>
namespace tenkaichi {
struct Gpu {
    SDL_Window* window=nullptr;SDL_GPUDevice* device=nullptr;
    SDL_GPUShader *vs=nullptr,*fs=nullptr;SDL_GPUGraphicsPipeline* pipeline=nullptr;
    SDL_GPUSampler* sampler=nullptr;SDL_GPUTexture *asset=nullptr,*target=nullptr;
    Uint32 w=0,h=0;bool claimed=false;
    ~Gpu() {
        if(device) {
            SDL_WaitForGPUIdle(device);
            if(asset)SDL_ReleaseGPUTexture(device,asset);if(target)SDL_ReleaseGPUTexture(device,target);
            if(sampler)SDL_ReleaseGPUSampler(device,sampler);if(pipeline)SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
            if(vs)SDL_ReleaseGPUShader(device,vs);if(fs)SDL_ReleaseGPUShader(device,fs);
            if(claimed)SDL_ReleaseWindowFromGPUDevice(device,window);SDL_DestroyGPUDevice(device);
        }
        if(window)SDL_DestroyWindow(window);SDL_Quit();
    }
};
static bool wait(SDL_GPUDevice* device,SDL_GPUFence* fence) {
    if(!fence)return false;bool ok=SDL_WaitForGPUFences(device,true,&fence,1);
    SDL_ReleaseGPUFence(device,fence);return ok;
}
static bool upload(Gpu& gpu,const Image& image) {
    SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;ti.width=image.width;ti.height=image.height;ti.layer_count_or_depth=1;ti.num_levels=1;
    gpu.asset=SDL_CreateGPUTexture(gpu.device,&ti);if(!gpu.asset)return false;
    SDL_GPUTransferBufferCreateInfo bi{};bi.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;bi.size=Uint32(image.rgba.size());
    auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&bi);if(!transfer)return false;
    void* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);
    if(!mapped){SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return false;}
    std::memcpy(mapped,image.rgba.data(),image.rgba.size());SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
    auto* cmd=SDL_AcquireGPUCommandBuffer(gpu.device);auto* pass=cmd?SDL_BeginGPUCopyPass(cmd):nullptr;
    if(!pass){if(cmd)SDL_CancelGPUCommandBuffer(cmd);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return false;}
    SDL_GPUTextureTransferInfo src{};src.transfer_buffer=transfer;src.pixels_per_row=image.width;src.rows_per_layer=image.height;
    SDL_GPUTextureRegion dst{};dst.texture=gpu.asset;dst.w=image.width;dst.h=image.height;dst.d=1;
    SDL_UploadToGPUTexture(pass,&src,&dst,false);SDL_EndGPUCopyPass(pass);
    bool ok=wait(gpu.device,SDL_SubmitGPUCommandBufferAndAcquireFence(cmd));
    SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return ok;
}
static bool capture(Gpu& gpu) {
    SDL_GPUTransferBufferCreateInfo bi{};bi.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;bi.size=gpu.w*gpu.h*4;
    auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&bi);if(!transfer)return false;
    auto* cmd=SDL_AcquireGPUCommandBuffer(gpu.device);auto* pass=cmd?SDL_BeginGPUCopyPass(cmd):nullptr;
    if(!pass){if(cmd)SDL_CancelGPUCommandBuffer(cmd);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return false;}
    SDL_GPUTextureRegion src{};src.texture=gpu.target;src.w=gpu.w;src.h=gpu.h;src.d=1;
    SDL_GPUTextureTransferInfo dst{};dst.transfer_buffer=transfer;dst.pixels_per_row=gpu.w;dst.rows_per_layer=gpu.h;
    SDL_DownloadFromGPUTexture(pass,&src,&dst);SDL_EndGPUCopyPass(pass);
    if(!wait(gpu.device,SDL_SubmitGPUCommandBufferAndAcquireFence(cmd))){SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);return false;}
    void* pixels=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);bool ok=false;
    if(pixels) {
        auto* surface=SDL_CreateSurfaceFrom(int(gpu.w),int(gpu.h),SDL_PIXELFORMAT_RGBA32,pixels,int(gpu.w*4));
        if(surface){ok=SDL_SavePNG(surface,"native-first-frame.png.tmp");SDL_DestroySurface(surface);}
        SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
    }
    SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
    return ok && std::rename("native-first-frame.png.tmp","native-first-frame.png")==0;
}
int renderResource(const Image& image) {
    Gpu gpu;auto fail=[&](const char* operation){status("ERROR",std::string(operation)+": "+SDL_GetError());return 2;};
    if(image.width!=256||image.height!=512||image.rgba.size()!=256*512*4){status("ERROR","Invalid native image contract");return 2;}
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS))return fail("SDL video");
    gpu.device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,true,"vulkan");if(!gpu.device)return fail("Vulkan device");
    status("VULKAN",std::string("actual backend=")+SDL_GetGPUDeviceDriver(gpu.device));
    gpu.window=SDL_CreateWindow("Tenkaichi Mobile — recurso original",960,540,SDL_WINDOW_RESIZABLE);
    if(!gpu.window)return fail("Android window");
    if(!SDL_ClaimWindowForGPUDevice(gpu.device,gpu.window))return fail("Vulkan surface claim");gpu.claimed=true;
    SDL_GPUShaderCreateInfo si{};si.code=vertex;si.code_size=sizeof(vertex);si.entrypoint="main";si.format=SDL_GPU_SHADERFORMAT_SPIRV;
    si.stage=SDL_GPU_SHADERSTAGE_VERTEX;si.num_uniform_buffers=1;gpu.vs=SDL_CreateGPUShader(gpu.device,&si);
    si={};si.code=fragment;si.code_size=sizeof(fragment);si.entrypoint="main";si.format=SDL_GPU_SHADERFORMAT_SPIRV;
    si.stage=SDL_GPU_SHADERSTAGE_FRAGMENT;si.num_samplers=1;gpu.fs=SDL_CreateGPUShader(gpu.device,&si);
    if(!gpu.vs||!gpu.fs)return fail("resource SPIR-V shaders");
    SDL_GPUColorTargetDescription color{};color.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color.blend_state.enable_blend=true;color.blend_state.src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;color.blend_state.color_blend_op=SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;color.blend_state.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.alpha_blend_op=SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo pi{};pi.vertex_shader=gpu.vs;pi.fragment_shader=gpu.fs;
    pi.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pi.target_info.num_color_targets=1;pi.target_info.color_target_descriptions=&color;
    gpu.pipeline=SDL_CreateGPUGraphicsPipeline(gpu.device,&pi);if(!gpu.pipeline)return fail("native resource pipeline");
    SDL_GPUSamplerCreateInfo sm{};sm.min_filter=sm.mag_filter=SDL_GPU_FILTER_LINEAR;sm.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sm.address_mode_u=sm.address_mode_v=sm.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    gpu.sampler=SDL_CreateGPUSampler(gpu.device,&sm);if(!gpu.sampler)return fail("resource sampler");
    if(!upload(gpu,image))return fail("native texture upload/fence");
    status("UPLOADED","Native RGBA8 256x512 upload fence complete");
    bool first=true;bool running=true;
    while(running) {
        SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_EVENT_QUIT ||
            (event.type==SDL_EVENT_KEY_DOWN&&event.key.key==SDLK_ESCAPE))running=false;
        if(!running)break;
        auto* cmd=SDL_AcquireGPUCommandBuffer(gpu.device);if(!cmd)return fail("acquire command");
        SDL_GPUTexture* swap=nullptr;Uint32 w=0,h=0;
        if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmd,gpu.window,&swap,&w,&h)) {SDL_CancelGPUCommandBuffer(cmd);return fail("swapchain acquisition");}
        if(!swap){SDL_CancelGPUCommandBuffer(cmd);SDL_Delay(16);continue;}
        if(w>4096||h>4096||!w||!h){SDL_CancelGPUCommandBuffer(cmd);status("ERROR","Unsupported drawable extent "+std::to_string(w)+"x"+std::to_string(h));return 2;}
        if(gpu.w!=w||gpu.h!=h) {
            status("DRAWABLE",std::to_string(w)+"x"+std::to_string(h)+" native render target");
            SDL_WaitForGPUIdle(gpu.device);if(gpu.target)SDL_ReleaseGPUTexture(gpu.device,gpu.target);
            SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;ti.width=w;ti.height=h;ti.layer_count_or_depth=1;ti.num_levels=1;
            gpu.target=SDL_CreateGPUTexture(gpu.device,&ti);gpu.w=w;gpu.h=h;
            if(!gpu.target){SDL_CancelGPUCommandBuffer(cmd);return fail("native frame target");}
        }
        SDL_GPUColorTargetInfo ct{};ct.texture=gpu.target;ct.load_op=SDL_GPU_LOADOP_CLEAR;ct.store_op=SDL_GPU_STOREOP_STORE;ct.clear_color={0.04f,0.04f,0.04f,1};
        auto* pass=SDL_BeginGPURenderPass(cmd,&ct,1,nullptr);if(!pass){SDL_CancelGPUCommandBuffer(cmd);return fail("render pass");}
        SDL_BindGPUGraphicsPipeline(pass,gpu.pipeline);
        float view[4]={1,1,0,0};float aspect=float(w)/float(h),resource=float(image.width)/float(image.height);
        if(aspect>resource)view[0]=resource/aspect;else view[1]=aspect/resource;
        SDL_PushGPUVertexUniformData(cmd,0,view,sizeof(view));
        SDL_GPUTextureSamplerBinding binding{gpu.asset,gpu.sampler};SDL_BindGPUFragmentSamplers(pass,0,&binding,1);
        SDL_DrawGPUPrimitives(pass,6,1,0,0);SDL_EndGPURenderPass(pass);
        SDL_GPUBlitInfo blit{};blit.source.texture=gpu.target;blit.source.w=w;blit.source.h=h;
        blit.destination.texture=swap;blit.destination.w=w;blit.destination.h=h;blit.load_op=SDL_GPU_LOADOP_DONT_CARE;blit.filter=SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(cmd,&blit);
        if(first) {
            if(!wait(gpu.device,SDL_SubmitGPUCommandBufferAndAcquireFence(cmd)))return fail("first Vulkan submission/fence");
            status("SUBMITTED","Real ISO texture drawn as a native resource quad; fence complete. Physical display still needs owner observation.");
            status("SUBMITTED",capture(gpu)?"GPU readback saved: native-first-frame.png":"GPU submitted; readback failed: "+std::string(SDL_GetError()));first=false;
        } else if(!SDL_SubmitGPUCommandBuffer(cmd))return fail("Vulkan submit");
    }
    status("STOPPED","Resource viewer closed. No gameplay implemented.");return 0;
}
}
