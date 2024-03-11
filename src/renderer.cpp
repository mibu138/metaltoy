#include "renderer.h"
#include "globals.h"

#include <simd/simd.h>

static void error_msg(const char *msg)
{
    if (global_quiet)
        return;
    fprintf(stderr, "%s", msg);
}

static inline double getCurrentTimeInSeconds()
{
    using Clock = std::chrono::high_resolution_clock;
    using Ns = std::chrono::nanoseconds;
    std::chrono::time_point<Clock, Ns> tp = std::chrono::high_resolution_clock::now();
    return tp.time_since_epoch().count() / 1e9;
}

static char *
load_file(const char *relpath)
{
    size_t sz, nr;
    FILE *fd;
    const char *base;
    char *s;
    char buf[512];

    base = getenv("S");
    if (!base)
    {
        error_msg("Environment variable S is not defined. Searching current directory...\n");
        base = ".";
    }

    s = stpcpy(buf, base);
    *s++ = '/';
    s = stpcpy(s, relpath);

    fd = fopen(buf, "r");

    if (!fd)
    {
        snprintf(buf, 512, "File failed to open. Errno %d\n", errno);
        error_msg(buf);
        return nullptr;
    }

    fseek(fd, 0, SEEK_END);
    sz = ftell(fd);
    rewind(fd);

    s = (char*)malloc(sz + 1);

    nr = fread(s, 1, sz, fd);
    assert(nr == sz);

    // add a terminating null char
    s[sz] = '\0';

    fclose(fd);

    return s;
}

// return 0 on success
static int build_shader_library(MTL::Device *device, const char *shader_src, MTL::Library **out)
{
    NS::Error *error = nullptr;
    MTL::Library *lib = nullptr;

    lib = device->newLibrary(NS::String::string(
                shader_src,
                NS::UTF8StringEncoding), nullptr, &error);

    if (!lib)
    {
        error_msg(error->localizedDescription()->utf8String());
        return -1;
    }

    *out = lib;
    return 0;
}

// return 0 on success
static int build_graphics_pipeline(MTL::Device *device, MTL::Library *lib, MTL::RenderPipelineState **out)
{
    using NS::StringEncoding::UTF8StringEncoding;
    MTL::Function *vertexfn, *fragmentfn;
    NS::Error *error;
    MTL::RenderPipelineDescriptor *desc;
    MTL::RenderPipelineState *pso;
    int r = -1;

    error = nullptr;

    vertexfn = lib->newFunction( NS::String::string("vertexMain", UTF8StringEncoding) );

    if (!vertexfn)
    {
        error_msg("Failed finding vertexMain fn. Did the name change?\n");
        goto end2;
    }

    fragmentfn = lib->newFunction( NS::String::string("fragmentMain", UTF8StringEncoding) );

    if (!fragmentfn)
    {
        error_msg("Failed finding fragmentMain fn. Did the name change?\n");
        goto end3;
    }

    desc = MTL::RenderPipelineDescriptor::alloc()->init();

    desc->setVertexFunction(vertexfn);
    desc->setFragmentFunction(fragmentfn);
    desc->colorAttachments()->object(0)->
        setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

    pso = device->newRenderPipelineState( desc, &error );
    if ( !pso)
    {
        error_msg(error->localizedDescription()->utf8String() );
        goto end4;
    }

    r = 0;
    *out = pso;

end4:
    desc->release();
    fragmentfn->release();
end3:
    vertexfn->release();
end2:
    return r;
}

static int build_compute_pipeline(MTL::Device *device, MTL::Library *lib, MTL::ComputePipelineState **pipeline)
{
    NS::Error *error;
    MTL::Function *fn;
    MTL::ComputePipelineState *pso;

    fn = lib->newFunction( NS::String::string("computeMain", NS::UTF8StringEncoding) );
    if (!fn)
    {
        error_msg("Failed finding compute shader funciton\n");
        return -1;
    }

    pso = device->newComputePipelineState( fn, &error);

    fn->release();

    if (!pso)
    {
        error_msg("Failed to create compute pipeline\n");
        return -1;
    }

    *pipeline = pso;
    return 0;
}

Renderer::Renderer( MTL::Device* pDevice )
: _device( pDevice->retain() )
{
    _cmdqueue = _device->newCommandQueue();
    _starttime = getCurrentTimeInSeconds();

    buildBuffers();
    buildTexture();
    buildRenderPipeline();
}

Renderer::~Renderer()
{
    _cmdqueue->release();
    _device->release();
}

void Renderer::buildRenderPipeline()
{
    int er;
    MTL::Library *lib;
    MTL::RenderPipelineState *pso;
    char *src = load_file("src/quad.metal");

    assert(src && "quad shader not found");

    er = build_shader_library(_device, src, &lib);
    assert(!er && "Failed to build quad shader library");

    er = build_graphics_pipeline(_device, lib, &pso);
    assert(!er && "Failed to build quad pipeline");

    lib->release();
    free(src);

    _renderpso = pso;
}

void Renderer::buildPipelinesIfNeedTo()
{
    char *new_shadersrc;
    const char *old_shadersrc;
    MTL::Library *shaderlib;
    MTL::ComputePipelineState *computepipeline;
    MTL::RenderPipelineState *renderpipeline;
    int er = 0;

    // TODO check file time stamp first

    old_shadersrc = _shadersrc;
    new_shadersrc = load_file("src/shader.metal");

    if (!new_shadersrc)
    {
        error_msg("Error reading shader source file. Did it move?\n");
        return;
    }

    // check if the source is the same as before. if it is, do nothing
    if (old_shadersrc)
    {
        if (!strcmp(old_shadersrc, new_shadersrc))
        {
            // nothing has changed, we can exit early
            free(new_shadersrc);
            return;
        }

        // free the old
        free(_shadersrc);
    }

    // assign the new
    _shadersrc = new_shadersrc;

    error_msg("Shader has changed! Rebuilding pipelines...\n");

    // assume error until proven otherwise
    _shadererror = true;

    er = build_shader_library(_device, new_shadersrc, &shaderlib);

    if (er)
    {
        return;
    }

    er = build_compute_pipeline(_device, shaderlib, &computepipeline);

    if (er)
    {
        shaderlib->release();
        return;
    }

    shaderlib->release();

    if (_computepso)
        _computepso->release();

    _shadererror = false;
    _computepso = computepipeline;

    error_msg("Pipeline rebuilding complete.\n");
}

void Renderer::buildBuffers()
{
    constexpr size_t NumVertices = 4;
    size_t possize, colorsize, uvsize, indexsize;
    MTL::Buffer *posbuf, *colorbuf, *uvbuf, *indexbuf;

    simd::float3 positions[NumVertices] =
    {
        { -1.0f, +1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f },
        { +1.0f, +1.0f, 0.0f },
        { +1.0f, -1.0f, 0.0f },
    };

    simd::float3 colors[NumVertices] =
    {
        {  1.0f, 0.3f, 0.2f },
        {  0.8f, 1.0, 0.0f },
        {  0.8f, 0.0f, 1.0 },
        {  1.0f, 0.3f, 0.2f },
    };

    simd::float2 uvs[NumVertices] =
    {
        {  0.0f, 0.0f },
        {  0.0f, 1.0f },
        {  1.0f, 0.0f },
        {  1.0f, 1.0f },
    };

    uint16_t indices[] = {
        0, 1, 2, 2, 1, 3
    };

    possize = NumVertices * sizeof( simd::float3 );
    colorsize = NumVertices * sizeof( simd::float3 );
    uvsize = NumVertices * sizeof( simd::float2 );
    indexsize = sizeof(indices);

    posbuf = _device->newBuffer( possize, MTL::ResourceStorageModeManaged );
    colorbuf = _device->newBuffer( colorsize, MTL::ResourceStorageModeManaged );
    uvbuf = _device->newBuffer( uvsize, MTL::ResourceStorageModeManaged );
    indexbuf = _device->newBuffer( indexsize, MTL::ResourceStorageModeManaged );

    memcpy( posbuf->contents(), positions, possize);
    memcpy( colorbuf->contents(), colors, colorsize);
    memcpy( uvbuf->contents(), uvs, uvsize);
    memcpy( indexbuf->contents(), indices, indexsize);

    posbuf->didModifyRange( NS::Range::Make( 0, posbuf->length() ) );
    colorbuf->didModifyRange( NS::Range::Make( 0, colorbuf->length() ) );
    uvbuf->didModifyRange( NS::Range::Make( 0, uvbuf->length() ) );
    indexbuf->didModifyRange( NS::Range::Make( 0, indexbuf->length() ));

    _positionbuffer = posbuf;
    _colorbuffer = colorbuf;
    _uvbuffer = uvbuf;
    _indexbuffer = indexbuf;
    _dynbuffer = _device->newBuffer( sizeof(float), MTL::ResourceStorageModeManaged );
}

void Renderer::buildTexture()
{
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::alloc()->init();

    td->setWidth(global_texture_width);
    td->setHeight(global_texture_height);
    td->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    td->setTextureType(MTL::TextureType2D);
    td->setStorageMode(MTL::StorageModeManaged);
    td->setUsage(MTL::ResourceUsageSample | MTL::ResourceUsageRead | MTL::ResourceUsageWrite);

    MTL::Texture *tex = _device->newTexture(td);
    _texture = tex;

    td->release();
}

void Renderer::generateTexture()
{
    MTL::CommandBuffer *cmdbuf;
    MTL::ComputeCommandEncoder *enc;
    MTL::Size gridsize, thread_group_size;
    NS::UInteger tgs;

    float *time = reinterpret_cast<float*>(_dynbuffer->contents());
    *time = getCurrentTimeInSeconds() - _starttime;
    _dynbuffer->didModifyRange(NS::Range::Make(0, sizeof(float)));

    cmdbuf = _cmdqueue->commandBuffer();
    assert(cmdbuf);

    enc = cmdbuf->computeCommandEncoder();

    enc->setComputePipelineState(_computepso);
    enc->setTexture(_texture, 0);
    enc->setBuffer(_dynbuffer, 0, 0);

    gridsize = MTL::Size::Make(global_texture_width, global_texture_height, 1);

    tgs = _computepso->maxTotalThreadsPerThreadgroup();

    thread_group_size = MTL::Size::Make(tgs, 1, 1);

    enc->dispatchThreads( gridsize, thread_group_size );
    enc->endEncoding();

    cmdbuf->commit();
}

void Renderer::draw( MTK::View* pView )
{
    buildPipelinesIfNeedTo();

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::CommandBuffer* cmd = _cmdqueue->commandBuffer();
    MTL::RenderPassDescriptor* prd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* enc = cmd->renderCommandEncoder( prd );

    if (_shadererror)
    {
        auto clear = MTL::ClearColor::Make(0.9, 0.4, 0.9, 1.0);
        pView->setClearColor(clear);
    }
    else
    {
        generateTexture();

        enc->setRenderPipelineState( _renderpso );
        enc->setVertexBuffer(_positionbuffer, 0, 0);
        enc->setVertexBuffer(_colorbuffer, 0, 1);
        enc->setVertexBuffer(_uvbuffer, 0, 2);
        enc->setFragmentTexture(_texture, 0);
        enc->drawIndexedPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle,
                6, MTL::IndexTypeUInt16, _indexbuffer, 0 );
    }

    enc->endEncoding();
    cmd->presentDrawable( pView->currentDrawable() );
    cmd->commit();

    pool->release();
}
