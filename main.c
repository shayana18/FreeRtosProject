#include "schedulingConfig.h"

#if ( configUSE_EDF == 1 )
    #include "edf_tests/test_1.h"
    #include "edf_tests/test_2.h"
    #include "edf_tests/test_3.h"
    #include "edf_tests/test_4.h"
    #include "edf_tests/test_5.h"
    #include "edf_tests/test_6.h"
#endif

int main( void )
{
    #if ( configUSE_EDF == 1 )
        /* Uncomment this single line to run the EDF test case. */
        // Simple edf implicit deadlinetest case with 3 tasks added at startup with a fairly low utilization.
        // test_1_run(); 

        // Higher utilization (but still < 1.0) edf implicit deadline test with 4 tasks added at startup.
        // test_2_run(); 

        // EDF admission control test, attempts to add unschedulable task at startup and after 10s, then adds a schedulable task.
        // test_3_run(); 

        // Explicit deadline EDF test with 3 tasks (D != T).
        // test_4_run();

        // Higher utilization explicit deadline EDF test with 4 tasks (D < T).
        //  test_5_run(); 

        // Explicit deadline admission control test (expected reject then accept).
        // test_6_run(); 
    #endif

    for( ;; )
    {
    }
}
