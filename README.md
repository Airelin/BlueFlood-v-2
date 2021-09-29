# About this repository
This repository inherits from two other repositories. The folder called 'BlueFlood-master' is more or less the original implementation of BlueFlood. The folder called 'contiki-ng' is my fork of Contiki-NG including BlueFlood v.2.

## BlueFlood v.2
You can find the apllication component of BlueFlood v.2 in `contiki-ng/examples/blueflood/`. The flooding mechanism is placed in the blueflood.c and blueflood.h files. They can be found in `contiki-ng/arch/platform/nrf52840/`.  

### Compiling BlueFlood v.2
You will need to navigate to `contiki-ng/examples/blueflood/` and run `make clean && make WERROR=0` there. If your testbed is not the same as my testbed at home you should consider either editing the `testbed.h` and add your devices as HOME_TESTBED or if you have access to another testbed I used you can also just go to the `project-conf.h` and change the current testbed configuration there. The project-conf.h is located next to the test-blueflood.c and the testbed.h is located next to blueflood.c and blueflood.h.  
If neither of it works for you, you can set the testbed configuration in the `project-conf.h` to `HALLOWORLD_TESTBED`. When runninig each device should output its own id you need for the testbed.h.

### Troubleshooting
If compiling leads to an error on not finding `arm-none-eabi-gcc` it might be helpful to add the directory where it can be found by: `export PATH=$PATH:<path-to-arm-none-eabi-gcc>`. For me this is:`~/opt/gnu-mcu-eclipse/arm-none-eabi-gcc/7.2.1-1.1-20180401-0515/bin`.

## Evaluation
In the directory 'Evaluation' are three different subdirectories. 'Logs' holds raw data of the experiments I evaluated, 'pyhton-evaluation-scripts' holds the evaluation scripts I used for plotting and 'Plots' holds the plots I did for the thesis. For running the python scripts I used `python 3.9` and some packets to get via `pip3 install numpy matplotlib progressbar2 python-dotenv`