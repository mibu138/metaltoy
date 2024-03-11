#include "app.h"
#include "globals.h"
#include <stdlib.h>

unsigned int global_texture_width = 512;
unsigned int global_texture_height = 512;
unsigned int global_window_width = 512;
unsigned int global_window_height = 512;
bool global_quiet = false;

int main( int argc, char* argv[] )
{
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i) {
            const char *arg = argv[i];
            if (arg[0] == '-')
            {
                switch (arg[1])
                {
                    case 'q': global_quiet = true; break;
                    default: fprintf(stderr, "Unknown argument %s\n", arg); return 1;
                }
                continue;
            }

            int res = ::atoi(argv[1]);

            if (res < 1 || res > 4096)
            {
                fprintf(stderr,
                        "Resolution argument %d out of range. Must be between 1 and 4096.\n", res);
                return -1;
            }

            global_window_width = res;
            global_window_height = res;
            global_texture_width = res * 4;
            global_texture_height = res * 4;
        }
    }

    NS::AutoreleasePool* pAutoreleasePool = NS::AutoreleasePool::alloc()->init();

    MyAppDelegate del;

    NS::Application* pSharedApplication = NS::Application::sharedApplication();
    pSharedApplication->setDelegate( &del );
    pSharedApplication->run();

    pAutoreleasePool->release();

    return 0;
}
