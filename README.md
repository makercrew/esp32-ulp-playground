# ESP32-S3 ULP Playground

This respository is a playground for working with the ultra-low power RISC-V co-processor on the ESP32-S3.
It is configured, out-of-the-box to be used with the ESP32-S3-DevKitC-1 but should be easily modified to 
work with other ESP32 boards with a ULP such as the S2, etc.

The repository is organized in a set of branches that are meant to be followed like a tutorial. Simply 
check out the branch of the lesson.

`git checkout lesson-1`

Instructions for each lesson can be found in the **lessons** folder.

## Prerequisites
- Linux
- Docker
- VSCode with the Dev Containers extension installed 
- ESP32-S3-DevKitC-1 or other ESP32 board with RISC-V ULP co-processor

## Lessons

1. [Setting Up the Dev Environment](lessons/lesson-1.md)
1. [Your First ULP Program](lessons/lesson-2.md)
1. [Wake Up the Main CPU](lessons/lesson-3.md)
1. [Under the Hood](lessons/lesson-4.md)
1. [Shared Variables](lessons/lesson-5.md)
1. [The ULP Stack](lessons/lesson-6.md)
1. [The Stack Monster 🧟](lessons/lesson-7.md)
1. [The Stack Monster: Part Deux 🧟 🧟](lessons/lesson-8.md)
1. [Detecting a ULP Crash](lessons/lesson-9.md)
1. [Reducing App Size](lessons/lesson-10.md)
1. [Make Your ULP App a Custom CMake Project](lessons/lesson-11.md)
1. [Static Analysis- Stack Usage and Call Graphs](lessons/lesson-12.md)