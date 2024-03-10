#ifndef METALTOY_RENDERER_H
#define METALTOY_RENDERER_H

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

class Renderer
{
    public:
        Renderer( MTL::Device* pDevice );
        ~Renderer();
        void draw( MTK::View* pView );
        void buildBuffers();
        void buildTexture();
        void buildRenderPipeline();
        void generateTexture();
        void buildPipelinesIfNeedTo();

    private:
        MTL::Device* _device;
        MTL::CommandQueue* _cmdqueue;
        MTL::RenderPipelineState *_renderpso = nullptr;
        MTL::ComputePipelineState *_computepso = nullptr;
        MTL::Buffer *_indexbuffer;
        MTL::Buffer *_positionbuffer;
        MTL::Buffer *_colorbuffer;
        MTL::Buffer *_uvbuffer;
        MTL::Texture *_texture;
        char *_shadersrc = nullptr;
        bool _shadererror = true;
};

#endif
