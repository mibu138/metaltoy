#include "app.h"
#include "globals.h"
#include <stdlib.h>

unsigned int global_texture_width = 512;
unsigned int global_texture_height = 512;

int main( int argc, char* argv[] )
{
    if (argc == 2)
    {
        int res = ::atoi(argv[1]);

        if (res < 1 || res > 4096)
        {
            fprintf(stderr, "Resolution argument %d out of range. Must be between 1 and 4096.\n", res);
            return -1;
        }

        global_texture_width = res;
        global_texture_height = res;
    }

    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate( &del );
    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}
