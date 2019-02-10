/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H 1

#if defined(_DEBUG)
#define DEBUG_MEMORY 1
#define FULL_DEBUG_MEMORY 0 // includes default windows allocations and detects leaks on empty main :(
#endif

#if DEBUG_MEMORY
    #if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
        #define _CRTDBG_MAP_ALLOC
        #include <crtdbg.h>
        #include <errno.h>
        #define ENABLE_AUTO_MEM_CHECK()\
            int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);\
            flags |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF;\
            _CrtSetDbgFlag(flags);\
            _CrtMemState __necro_memory_state_diff;\
            _CrtMemState __necro_memory_initial_state;\
            _CrtMemState __necro_memory_end_state;\
            _CrtMemCheckpoint(&__necro_memory_initial_state);
        
#if FULL_DEBUG_MEMORY
        #define MEM_CHECK() ___CrtDumpMemoryLeaks();
#else
        #define MEM_CHECK()
#endif
        
        #define SCOPED_MEM_CHECK()\
            _CrtMemCheckpoint(&__necro_memory_end_state);\
            if (_CrtMemDifference(&__necro_memory_state_diff,&__necro_memory_initial_state,&__necro_memory_end_state))\
            {\
                _CrtMemDumpStatistics(&__necro_memory_state_diff);\
                return 1;\
            }
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
#define SCOPED_MEM_CHECK()
#endif

#endif // DEBUG_MEMORY_H
