#include "tenkaichi/runtime.hpp"
#include "tenkaichi/presentation.hpp"
#include "tenkaichi/preparation.hpp"
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
// Once a swapchain image has been acquired SDL forbids cancelling its command
// buffer. Submit a clear on resize/error paths so the acquired image is released.
static bool releaseAcquired(SDL_GPUCommandBuffer* cmd,SDL_GPUTexture* swap) {
    SDL_GPUColorTargetInfo ct{};ct.texture=swap;ct.load_op=SDL_GPU_LOADOP_CLEAR;
    ct.store_op=SDL_GPU_STOREOP_STORE;ct.clear_color={0.04f,0.04f,0.04f,1};
    if(auto* pass=SDL_BeginGPURenderPass(cmd,&ct,1,nullptr))SDL_EndGPURenderPass(pass);
    return SDL_SubmitGPUCommandBuffer(cmd);
}
static void viewport(SDL_GPURenderPass* pass,const PresentationRect& rect,Uint32 w,Uint32 h) {
    SDL_GPUViewport view{rect.x,rect.y,rect.w,rect.h,0,1};SDL_SetGPUViewport(pass,&view);
    SDL_Rect scissor{0,0,int(w),int(h)};SDL_SetGPUScissor(pass,&scissor);
}
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
// Check the exact uploaded pixels with a native-size, nearest, no-blend pass.
// This isolates presentation from BPE/CLUT; it is not an original-game screenshot.
static bool verifyPixels(Gpu& gpu,const Image& image) {
    struct Probe {
        Gpu& gpu;SDL_GPUTexture* target=nullptr;SDL_GPUGraphicsPipeline* pipeline=nullptr;
        SDL_GPUSampler* sampler=nullptr;SDL_GPUTransferBuffer* transfer=nullptr;
        ~Probe(){SDL_WaitForGPUIdle(gpu.device);
            if(transfer)SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
            if(sampler)SDL_ReleaseGPUSampler(gpu.device,sampler);
            if(pipeline)SDL_ReleaseGPUGraphicsPipeline(gpu.device,pipeline);
            if(target)SDL_ReleaseGPUTexture(gpu.device,target);}
    } probe{gpu};
    SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;ti.width=image.width;ti.height=image.height;ti.layer_count_or_depth=1;ti.num_levels=1;
    probe.target=SDL_CreateGPUTexture(gpu.device,&ti);
    SDL_GPUColorTargetDescription color{};color.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo pi{};pi.vertex_shader=gpu.vs;pi.fragment_shader=gpu.fs;
    pi.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;pi.target_info.num_color_targets=1;pi.target_info.color_target_descriptions=&color;
    probe.pipeline=SDL_CreateGPUGraphicsPipeline(gpu.device,&pi);
    SDL_GPUSamplerCreateInfo sm{};sm.min_filter=sm.mag_filter=SDL_GPU_FILTER_NEAREST;sm.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sm.address_mode_u=sm.address_mode_v=sm.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    probe.sampler=SDL_CreateGPUSampler(gpu.device,&sm);
    SDL_GPUTransferBufferCreateInfo bi{};bi.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;bi.size=Uint32(image.rgba.size());
    probe.transfer=SDL_CreateGPUTransferBuffer(gpu.device,&bi);
    if(!probe.target||!probe.pipeline||!probe.sampler||!probe.transfer)return false;
    auto* cmd=SDL_AcquireGPUCommandBuffer(gpu.device);if(!cmd)return false;
    SDL_GPUColorTargetInfo ct{};ct.texture=probe.target;ct.load_op=SDL_GPU_LOADOP_CLEAR;ct.store_op=SDL_GPU_STOREOP_STORE;
    auto* pass=SDL_BeginGPURenderPass(cmd,&ct,1,nullptr);if(!pass){SDL_CancelGPUCommandBuffer(cmd);return false;}
    viewport(pass,{0,0,float(image.width),float(image.height)},image.width,image.height);
    SDL_BindGPUGraphicsPipeline(pass,probe.pipeline);const float view[4]={1,1,0,0};
    SDL_PushGPUVertexUniformData(cmd,0,view,sizeof(view));
    SDL_GPUTextureSamplerBinding binding{gpu.asset,probe.sampler};SDL_BindGPUFragmentSamplers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,6,1,0,0);SDL_EndGPURenderPass(pass);
    auto* copy=SDL_BeginGPUCopyPass(cmd);if(!copy){SDL_CancelGPUCommandBuffer(cmd);return false;}
    SDL_GPUTextureRegion src{};src.texture=probe.target;src.w=image.width;src.h=image.height;src.d=1;
    SDL_GPUTextureTransferInfo dst{};dst.transfer_buffer=probe.transfer;dst.pixels_per_row=image.width;dst.rows_per_layer=image.height;
    SDL_DownloadFromGPUTexture(copy,&src,&dst);SDL_EndGPUCopyPass(copy);
    if(!wait(gpu.device,SDL_SubmitGPUCommandBufferAndAcquireFence(cmd)))return false;
    const auto* pixels=static_cast<const std::uint8_t*>(SDL_MapGPUTransferBuffer(gpu.device,probe.transfer,false));if(!pixels)return false;
    std::size_t different=0;for(std::size_t i=0;i<image.rgba.size();++i)different+=pixels[i]!=image.rgba[i];
    const auto detail="converted-sha256="+sha256({image.rgba.data(),image.rgba.size()})+
        " gpu-native-size-sha256="+sha256({pixels,image.rgba.size()})+" mismatched-bytes="+std::to_string(different)+
        " compared-bytes="+std::to_string(image.rgba.size())+"; GPU vs converted cache, not original-game screenshot";
    SDL_UnmapGPUTransferBuffer(gpu.device,probe.transfer);
    if(FILE* out=std::fopen("native-pixel-check.txt.tmp","w")){std::fprintf(out,"%s\n",detail.c_str());
        if(!std::fclose(out))std::rename("native-pixel-check.txt.tmp","native-pixel-check.txt");}
    status("PIXEL_CHECK",detail);
    if(different){SDL_SetError("Native-size GPU readback differs from converted pixels; see native-pixel-check.txt");return false;}
    return true;
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
int renderEngine(Engine& engine) {
    SceneFrame initial;std::string engineError;
    if(!engine.frame(initial,engineError)||initial.images.size()!=1||!initial.poses.empty()){
        status("ERROR",engineError.empty()?"Presentation adapter requires one prepared image; native mesh rendering remains unavailable":engineError);return 2;
    }
    auto presentationTransform=[](const Matrix4& matrix){for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)if(matrix.m[i][j]!=(i==j?1.0f:0.0f))return false;return true;};
    if(!presentationTransform(initial.images.front().world)){status("ERROR","World-space image transform requires a scene renderer beyond the preserved presentation adapter");return 2;}
    const auto resource=initial.images.front().resource;
    const auto imageLease=initial.images.front().image;const auto& image=*imageLease;
    Gpu gpu;auto fail=[&](const char* operation){status("ERROR",std::string(operation)+": "+SDL_GetError());return 2;};
    if(image.width!=256||image.height!=512||image.rgba.size()!=256*512*4){status("ERROR","Invalid native image contract");return 2;}
    if(!SDL_SetHintWithPriority(SDL_HINT_ORIENTATIONS,"LandscapeLeft LandscapeRight",SDL_HINT_OVERRIDE))return fail("landscape orientation policy");
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
    if(!verifyPixels(gpu,image))return fail("native-size texture/shader pixel comparison");
    status("UPLOADED","Native RGBA8 256x512 upload fence complete");
    bool first=true;bool running=true;bool recreate=false;
    unsigned generation=0,mismatches=0;Uint64 previousTickNs=SDL_GetTicksNS();
    status("SCENE_READY","Integrated engine scene, immutable asset handles and rational 30 Hz clock; battle readiness remains blocked");
    while(running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_EVENT_QUIT||(event.type==SDL_EVENT_KEY_DOWN&&event.key.key==SDLK_ESCAPE))running=false;
            if(event.type==SDL_EVENT_WILL_ENTER_BACKGROUND){engine.suspend(true);previousTickNs=SDL_GetTicksNS();}
            if(event.type==SDL_EVENT_DID_ENTER_FOREGROUND){engine.suspend(false);previousTickNs=SDL_GetTicksNS();}
            if(event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED||event.type==SDL_EVENT_WINDOW_RESIZED||
               event.type==SDL_EVENT_WINDOW_DISPLAY_CHANGED||event.type==SDL_EVENT_DISPLAY_ORIENTATION||
               event.type==SDL_EVENT_DID_ENTER_FOREGROUND) {
                recreate=true;
                SDL_Log("presentation invalidation event=%u",event.type);
            }
        }
        if(!running)break;
        const auto now=SDL_GetTicksNS();TickAdvance simulation;
        if(!engine.advance(now-previousTickNs,simulation,engineError)){status("ERROR",engineError);return 2;}
        previousTickNs=now;
        SceneFrame scene;
        if(!engine.frame(scene,engineError)||scene.images.size()!=1||scene.images.front().resource!=resource||!scene.poses.empty()||!presentationTransform(scene.images.front().world)){
            status("ERROR",engineError.empty()?"Scene needs native mesh/multi-resource renderer not implemented by this presentation adapter":engineError);return 2;
        }
        int logicalW=0,logicalH=0,pixelW=0,pixelH=0;
        if(!SDL_GetWindowSize(gpu.window,&logicalW,&logicalH)||!SDL_GetWindowSizeInPixels(gpu.window,&pixelW,&pixelH))return fail("drawable dimensions");
        if(pixelW<=0||pixelH<=0){SDL_Delay(16);continue;}
        auto* cmd=SDL_AcquireGPUCommandBuffer(gpu.device);if(!cmd)return fail("acquire command");
        SDL_GPUTexture* swap=nullptr;Uint32 w=0,h=0;
        if(!SDL_WaitAndAcquireGPUSwapchainTexture(cmd,gpu.window,&swap,&w,&h)) {SDL_CancelGPUCommandBuffer(cmd);return fail("swapchain acquisition");}
        if(!swap){SDL_CancelGPUCommandBuffer(cmd);SDL_Delay(16);continue;}
        if(w>4096||h>4096||!w||!h){releaseAcquired(cmd,swap);status("ERROR","Unsupported drawable extent "+std::to_string(w)+"x"+std::to_string(h));return 2;}
        // A resize may race acquisition. Never stretch a stale image over a
        // differently shaped Android surface; return it and renew the claim.
        if(w!=Uint32(pixelW)||h!=Uint32(pixelH)) {
            ++mismatches;
            if(mismatches==1||mismatches%30==0)status("RESIZING","drawable="+std::to_string(pixelW)+"x"+std::to_string(pixelH)+" swapchain="+std::to_string(w)+"x"+std::to_string(h)+" mismatch-attempt="+std::to_string(mismatches));
            if(!releaseAcquired(cmd,swap)||!SDL_WaitForGPUIdle(gpu.device))return fail("resize image release");
            SDL_ReleaseWindowFromGPUDevice(gpu.device,gpu.window);gpu.claimed=false;
            if(!SDL_ClaimWindowForGPUDevice(gpu.device,gpu.window))return fail("resize surface reclaim");
            gpu.claimed=true;recreate=true;
            if(mismatches>=120){status("ERROR","Drawable/swapchain extent mismatch persists after surface reclaim; see presentation diagnostics");return 2;}
            SDL_Delay(16);continue;
        }
        mismatches=0;
        PresentationRect rect;
        if(!fitResource(image.width,image.height,w,h,rect)){releaseAcquired(cmd,swap);status("ERROR","Invalid aspect-fit dimensions");return 2;}
        if(gpu.w!=w||gpu.h!=h||recreate) {
            if(!SDL_WaitForGPUIdle(gpu.device)){releaseAcquired(cmd,swap);return fail("presentation idle");}
            if(gpu.target)SDL_ReleaseGPUTexture(gpu.device,gpu.target);
            SDL_GPUTextureCreateInfo ti{};ti.type=SDL_GPU_TEXTURETYPE_2D;ti.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            ti.usage=SDL_GPU_TEXTUREUSAGE_COLOR_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;ti.width=w;ti.height=h;ti.layer_count_or_depth=1;ti.num_levels=1;
            gpu.target=SDL_CreateGPUTexture(gpu.device,&ti);gpu.w=w;gpu.h=h;
            if(!gpu.target){releaseAcquired(cmd,swap);return fail("native frame target");}
            recreate=false;first=true;++generation;
            char geometry[512];std::snprintf(geometry,sizeof(geometry),"generation=%u logical=%dx%d drawable=%dx%d swapchain=%ux%u target=%ux%u source=%ux%u viewport=(%.3f,%.3f %.3fx%.3f) source-ratio=%.8f viewport-ratio=%.8f",
                generation,logicalW,logicalH,pixelW,pixelH,w,h,gpu.w,gpu.h,image.width,image.height,rect.x,rect.y,rect.w,rect.h,double(image.width)/image.height,double(rect.w)/rect.h);
            status("DRAWABLE",geometry);
        }
        SDL_GPUColorTargetInfo ct{};ct.texture=gpu.target;ct.load_op=SDL_GPU_LOADOP_CLEAR;ct.store_op=SDL_GPU_STOREOP_STORE;ct.clear_color={0.04f,0.04f,0.04f,1};
        auto* pass=SDL_BeginGPURenderPass(cmd,&ct,1,nullptr);if(!pass){releaseAcquired(cmd,swap);return fail("render pass");}
        SDL_BindGPUGraphicsPipeline(pass,gpu.pipeline);
        viewport(pass,rect,w,h);
        float view[4]={1,1,0,0}; // Scaling is applied exactly once, in the viewport.
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
