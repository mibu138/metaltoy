/*
 *
 * Copyright 2022 Apple Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

#include <simd/simd.h>

#pragma region Declarations {

static inline double getCurrentTimeInSeconds()
{
    using Clock = std::chrono::high_resolution_clock;
    using Ns = std::chrono::nanoseconds;
    std::chrono::time_point<Clock, Ns> tp = std::chrono::high_resolution_clock::now();
    return tp.time_since_epoch().count() / 1e9;
}

struct State {
    double time = 0.0;
    double dt = 0.0;
};

constexpr uint32_t TEXTURE_RES = 512;
constexpr uint32_t TEXTURE_WIDTH = TEXTURE_RES;
constexpr uint32_t TEXTURE_HEIGHT = TEXTURE_RES;

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
        fprintf(stderr, "Environment variable S is not defined. Searching current directory...\n");
        base = ".";
    }

    s = stpcpy(buf, base);
    *s++ = '/';
    s = stpcpy(s, relpath);

    fd = fopen(buf, "r");

    if (!fd)
    {
        fprintf(stderr, "File failed to open. Errno %d\n", errno);
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
        fprintf(stderr, "%s\n", error->localizedDescription()->utf8String());
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
        fprintf(stderr, "Failed finding vertexMain fn. Did the name change?\n");
        goto end2;
    }

    fragmentfn = lib->newFunction( NS::String::string("fragmentMain", UTF8StringEncoding) );

    if (!fragmentfn)
    {
        fprintf(stderr, "Failed finding fragmentMain fn. Did the name change?\n");
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
        __builtin_printf( "%s", error->localizedDescription()->utf8String() );
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
        fprintf(stderr, "Failed finding compute shader funciton\n");
        return -1;
    }

    pso = device->newComputePipelineState( fn, &error);

    fn->release();

    if (!pso)
    {
        fprintf(stderr, "Failed to create compute pipeline\n");
        return -1;
    }

    *pipeline = pso;
    return 0;
}


class Renderer
{
    public:
        Renderer( MTL::Device* pDevice );
        ~Renderer();
        void draw( MTK::View* pView, const State & );
        void buildBuffers();
        void buildTexture();
        void generateTexture();
        void buildPipelinesIfNeedTo();

    private:
        MTL::Device* _device;
        MTL::CommandQueue* _cmdqueue;
        MTL::RenderPipelineState *_pso = nullptr;
        MTL::ComputePipelineState *_computepso = nullptr;
        MTL::Buffer *_indexbuffer;
        MTL::Buffer *_positionbuffer;
        MTL::Buffer *_colorbuffer;
        MTL::Buffer *_uvbuffer;
        MTL::Texture *_texture;
        char *_shadersrc = nullptr;
        bool _shadererror = true;
};

class MyMTKViewDelegate : public MTK::ViewDelegate
{
    public:
        MyMTKViewDelegate( MTL::Device* pDevice );
        virtual ~MyMTKViewDelegate() override;
        virtual void drawInMTKView( MTK::View* pView ) override;

    private:
        State state;
        Renderer* _pRenderer;
};

class MyAppDelegate : public NS::ApplicationDelegate
{
    public:
        ~MyAppDelegate();

        NS::Menu* createMenuBar();

        virtual void applicationWillFinishLaunching( NS::Notification* pNotification ) override;
        virtual void applicationDidFinishLaunching( NS::Notification* pNotification ) override;
        virtual bool applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender ) override;

    private:
        NS::Window* _pWindow;
        MTK::View* _pMtkView;
        MTL::Device* _pDevice;
        MyMTKViewDelegate* _pViewDelegate = nullptr;
};

#pragma endregion Declarations }


int main( int argc, char* argv[] )
{
    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate( &del );
    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}


#pragma mark - AppDelegate
#pragma region AppDelegate {

MyAppDelegate::~MyAppDelegate()
{
    _pMtkView->release();
    _pWindow->release();
    _pDevice->release();
    delete _pViewDelegate;
}

NS::Menu* MyAppDelegate::createMenuBar()
{
    using NS::StringEncoding::UTF8StringEncoding;

    NS::Menu* pMainMenu = NS::Menu::alloc()->init();
    NS::MenuItem* pAppMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* pAppMenu = NS::Menu::alloc()->init( NS::String::string( "Appname", UTF8StringEncoding ) );

    NS::String* appName = NS::RunningApplication::currentApplication()->localizedName();
    NS::String* quitItemName = NS::String::string( "Quit ", UTF8StringEncoding )->stringByAppendingString( appName );
    SEL quitCb = NS::MenuItem::registerActionCallback( "appQuit", [](void*,SEL,const NS::Object* pSender){
        auto pApp = NS::Application::sharedApplication();
        pApp->terminate( pSender );
    } );

    NS::MenuItem* pAppQuitItem = pAppMenu->addItem( quitItemName, quitCb, NS::String::string( "q", UTF8StringEncoding ) );
    pAppQuitItem->setKeyEquivalentModifierMask( NS::EventModifierFlagCommand );
    pAppMenuItem->setSubmenu( pAppMenu );

    NS::MenuItem* pWindowMenuItem = NS::MenuItem::alloc()->init();
    NS::Menu* pWindowMenu = NS::Menu::alloc()->init( NS::String::string( "Window", UTF8StringEncoding ) );

    SEL closeWindowCb = NS::MenuItem::registerActionCallback( "windowClose", [](void*, SEL, const NS::Object*){
        auto pApp = NS::Application::sharedApplication();
            pApp->windows()->object< NS::Window >(0)->close();
    } );
    NS::MenuItem* pCloseWindowItem = pWindowMenu->addItem( NS::String::string( "Close Window", UTF8StringEncoding ), closeWindowCb, NS::String::string( "w", UTF8StringEncoding ) );
    pCloseWindowItem->setKeyEquivalentModifierMask( NS::EventModifierFlagCommand );

    pWindowMenuItem->setSubmenu( pWindowMenu );

    pMainMenu->addItem( pAppMenuItem );
    pMainMenu->addItem( pWindowMenuItem );

    pAppMenuItem->release();
    pWindowMenuItem->release();
    pAppMenu->release();
    pWindowMenu->release();

    return pMainMenu->autorelease();
}

void MyAppDelegate::applicationWillFinishLaunching( NS::Notification* pNotification )
{
    NS::Menu* pMenu = createMenuBar();
    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
    pApp->setMainMenu( pMenu );
    pApp->setActivationPolicy( NS::ActivationPolicy::ActivationPolicyRegular );
}

void MyAppDelegate::applicationDidFinishLaunching( NS::Notification* pNotification )
{
    CGRect frame = (CGRect){ {100.0, 100.0}, {TEXTURE_WIDTH, TEXTURE_HEIGHT} };

    _pWindow = NS::Window::alloc()->init(
        frame,
        NS::WindowStyleMaskClosable|NS::WindowStyleMaskTitled,
        NS::BackingStoreBuffered,
        false );

    _pDevice = MTL::CreateSystemDefaultDevice();

    _pMtkView = MTK::View::alloc()->init( frame, _pDevice );
    _pMtkView->setColorPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB );
    _pMtkView->setClearColor( MTL::ClearColor::Make( 0.0, 0.8, 1.0, 1.0 ) );

    _pViewDelegate = new MyMTKViewDelegate( _pDevice );
    _pMtkView->setDelegate( _pViewDelegate );

    _pWindow->setContentView( _pMtkView );
    _pWindow->setTitle( NS::String::string( "00 - Window", NS::StringEncoding::UTF8StringEncoding ) );

    _pWindow->makeKeyAndOrderFront( nullptr );

    NS::Application* pApp = reinterpret_cast< NS::Application* >( pNotification->object() );
    pApp->activateIgnoringOtherApps( true );
}

bool MyAppDelegate::applicationShouldTerminateAfterLastWindowClosed( NS::Application* pSender )
{
    return true;
}

#pragma endregion AppDelegate }


#pragma mark - ViewDelegate
#pragma region ViewDelegate {

MyMTKViewDelegate::MyMTKViewDelegate( MTL::Device* pDevice )
: MTK::ViewDelegate()
, _pRenderer( new Renderer( pDevice ) )
{
    state.time = getCurrentTimeInSeconds();
}

MyMTKViewDelegate::~MyMTKViewDelegate()
{
    delete _pRenderer;
}

void MyMTKViewDelegate::drawInMTKView( MTK::View* pView )
{
    double t = getCurrentTimeInSeconds();
    state.dt = t - state.time;
    state.time = t;
    _pRenderer->draw( pView, state );
}

#pragma endregion ViewDelegate }


#pragma mark - Renderer
#pragma region Renderer {

Renderer::Renderer( MTL::Device* pDevice )
: _device( pDevice->retain() )
{
    _cmdqueue = _device->newCommandQueue();

    buildBuffers();
    buildTexture();
}

Renderer::~Renderer()
{
    _cmdqueue->release();
    _device->release();
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
        fprintf(stderr, "Error reading shader source file. Did it move?\n");
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

    printf("Shader has changed! Rebuilding pipelines...\n");

    // assume error until proven otherwise
    _shadererror = true;

    // the shader has changed
    // printf(" =========== New Shader source =========\n%s\n", new_shadersrc);

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

    er = build_graphics_pipeline(_device, shaderlib, &renderpipeline);

    if (er)
    {
        shaderlib->release();
        computepipeline->release();
        return;
    }

    shaderlib->release();

    if (_pso)
        _pso->release();
    if (_computepso)
        _computepso->release();

    _shadererror = false;
    _pso = renderpipeline;
    _computepso = computepipeline;

    printf("Pipeline rebuilding complete.\n");
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
}

void Renderer::buildTexture()
{
    MTL::TextureDescriptor *td = MTL::TextureDescriptor::alloc()->init();

    td->setWidth(TEXTURE_WIDTH);
    td->setHeight(TEXTURE_HEIGHT);
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

    cmdbuf = _cmdqueue->commandBuffer();
    assert(cmdbuf);

    enc = cmdbuf->computeCommandEncoder();

    enc->setComputePipelineState(_computepso);
    enc->setTexture(_texture, 0);

    gridsize = MTL::Size::Make(TEXTURE_WIDTH, TEXTURE_HEIGHT, 1);

    tgs = _computepso->maxTotalThreadsPerThreadgroup();

    thread_group_size = MTL::Size::Make(tgs, 1, 1);

    enc->dispatchThreads( gridsize, thread_group_size );
    enc->endEncoding();

    cmdbuf->commit();
}

void Renderer::draw( MTK::View* pView, const State &state)
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

        enc->setRenderPipelineState( _pso );
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

#pragma endregion Renderer }
