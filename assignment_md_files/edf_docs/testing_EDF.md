# EDF Hardware Testing Setup
- On top of the debugger's uart hooked into three pins of the mcu, we also have outputs on nine more pins of the mcu
- Seven of these nine pins are used in combination with trace hooks of tasks to display the assigned task id of the current task being run (and note that the idle task has id 0).
- One of the eight pins is used as a "clock signal" of sorts to mark when there is a change in the task being run.
- These eight pins are probed with a saleae logic eight analyzer and the logic analyzer software is configured to use the task switch pin to update the status of output readings on the otherr seven pins to display waveforms for them, but also display their combined output in binary so that we're able to see up to 127 tasks being run in one test. Note that some smaller tests that are testing things other than a large task set use one-hot encodings for the tasks to make the waveform more readable.
- the ninth pin is a gpio output that gets written to high for a certain amount of task ticks on a deadline miss and on the breadboard it is wired to an led to give visual indication if there is a deadline miss even if it's not probed by the logic analyzer.
    - Note that if you have less than 64 tasks, you can use the highest task id bit pin of the logic analyzer to read the deadline miss output pin if you want to do analysis on the timings of when the deadline mmisses happened.

# EDF Software Testing Setup
- Tests for edf can be found in the `edf_tests` folder in the root directory. Each test exposes a function call to the `main.c` 
- `main.c` contains calls to every test function, but all of them are commented out (and have a short description on what the test is). To run a test simply uncomment the one line in `main.c` that calls the test function, then compile and flash the raspberry pi. 
- EDF tests (and all future tests) uses macros and hooks found in the `test_utils.c/h` and `task_trace.c/h` files in the root directory.

# EDF Test Methods
- For task sets that have fewer tasks (reasonably we can say < 10), we will perform an analysis on what we expect the schedule to look like from the beginning till roughly 30s which for most task sets allows us to find examples of the scheduler responding in correct ways to specific scenarios such as preempting a running task, maintaining the current task despite a task arrival, etc.
- We then run the test on hardware and monitor the output waveform and ensure the behaviour matches what manual scheduling analysis gave.
- For larger task sets to see verify the system is able to handle up to 100 tasks as the assignment states, we can instead choose to do a utilization analysis to ensure that the task set can run, then monitor the test when running on hardware to verify that there are no deadline misses and we can examine snapshots of the tasks running to verify scheduling is working as intended rather than painstakingly creating an expected schedule for all 100 tasks.