#include "schedulingConfig.h"

#if ( configUSE_EDF == 1 )
    #include "edf_tests/test_1.h"
    #include "edf_tests/test_2.h"
    #include "edf_tests/test_3.h"
    #include "edf_tests/test_4.h"
    #include "edf_tests/test_5.h"
    #include "edf_tests/test_6.h"
    #include "edf_tests/test_7.h"
    #include "edf_tests/test_8.h"
#endif

int main( void )
{
    #if ( configUSE_EDF == 1 )
        // Simple edf implicit deadlinetest case with 3 tasks added at startup with a fairly low utilization.
        // edf_1_run(); 

        // Higher utilization (but still < 1.0) edf implicit deadline test with 4 tasks added at startup.
        // edf_2_run(); 

        // EDF admission control test, attempts to add unschedulable task at startup and after 10s, then adds a schedulable task.
        // edf_3_run(); 

        // Constrained deadline EDF test with 3 tasks (D != T).
        // edf_4_run();

        // Higher utilization Constrained deadline EDF test with 4 tasks (D < T).
        //  edf_5_run(); 

        // Constrained deadline admission control test (expected reject then accept).
        // edf_6_run();

        // Implicit deadline admission control test (utilization path: expected reject then accept).
        // edf_7_run();

        // Seven tasks with binary-encoded trace IDs and two intentional single deadline-miss events.
        edf_8_run();
    
    #elif ( configUSE_SRP == 1 ) // configUSE_EDF == 1
        // srp_1_run();
        // srp_2_run();
        // srp_3_run();
        // srp_4_run();
        // srp_5_run();
        // srp_6_run();
    #endif

    

    for( ;; )
    {
    }
}
