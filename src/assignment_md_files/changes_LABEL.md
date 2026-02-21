# Changes to FreeRTOS Kernel #
## Configs ##
- Added extra configuration file that would be included inside application code named `FreeRTOSExtConfigs.h`, which would be included inside `FreeRTOSConfigs.h`. This would allow for the user to choose whether the EDF and SRP struct parameters, algorithms, etc. are defined.
## Data Structures ##
- Added parameters to task control block (TCB) type in `task.c` which would keep track of periodic task parameters. Would only be defined if EDF is enabled in the extension configs.
## Functions/Macros ##
