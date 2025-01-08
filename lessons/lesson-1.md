# Lesson 1

This is the barebones repository. Nothing is set up for the ULP at this point. It's just a way for you 
to do a test with your hardware and development environment. 

This repo takes a Docker-first approach and assumes you are on Linux. It can definitely work on Windows 
but container development is pretty cumbersome there. If you are on Windows just install ESP-IDF locally.
You'll have to figure out how to modify certain steps that deal with connecting over serial, building, 
and flashing.

1. Clone the repository
1. `git checkout lesson-1`
1. Open the repo in VSCode
1. Plug in your dev board
1. Run `ls /dev/tty*` to see what your dev board enumerates as (i.e /dev/ttyUSB0)
1. Modify the **.devcontainer.json** file in this project. In the `runArgs` section uncomment the line that adds a device to the container and modify the line to be the device path from Step 5
1. Reopen the project in a Dev Container. This will incur a one time hit while the image is built and the container is launched. This can take about 5-10 minutes.
1. Open a terminal in VSCode (you should be inside a dev container at this point)
1. Run `idf.py --version` to confirm the IDF environment is properly configured
1. Run `idf.py build` to build the project.
1. Run `idf.py flash monitor`

The project will build and flash to your dev board with output like this:
```sh
...
I (246) main_task: Started on CPU0
I (286) main_task: Calling app_main()
I (286) main: ESP32-S3 ULP Playground
I (286) main: Main loop
I (5286) main: Main loop
I (10286) main: Main loop
...
```

Congratulations! You're all set up!

All work for subsequent lessons will be done from within the Dev Container in VSCode.

> [!IMPORTANT]
> Any time you start the container 
> in VSCode it will look for your device. If it's unplugged the container will fail to launch. Either 
> comment out the line in devcontainer.json or momentarily plug in your device so the Dev Container can 
> launch. It does NOT have to be constantly plugged in while editing code in the Dev Container or doing 
> builds. Just at container launch time.
