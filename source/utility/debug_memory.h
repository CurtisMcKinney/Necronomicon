/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H 1

#if defined(_DEBUG)
#define DEBUG_MEMORY 1
#endif

#if defined(DEBUG_MEMORY)
    #if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
        #define _CRTDBG_MAP_ALLOC
        #include <crtdbg.h>
        #define MEM_CHECK() _CrtDumpMemoryLeaks()
        #define ENABLE_AUTO_MEM_CHECK() _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
    #elif defined(__unix)
        // TODO: ADD MEMORY CHECK FOR NON-WINDOWS PLATFORMS!
        #define MEM_CHECK()
        #define ENABLE_AUTO_MEM_CHECK()
    #else
        #define MEM_CHECK()
        #define ENABLE_AUTO_MEM_CHECK()
    #endif
#else
#define MEM_CHECK()
#define ENABLE_AUTO_MEM_CHECK()
#endif

#endif // DEBUG_MEMORY_H
