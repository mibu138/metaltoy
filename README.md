# Metal Toy

## Overview

Based on learn-metal-cpp examples and inspired by Shader Toy, Metal Toy allows on the fly shader recompilation. It is a shader playground, and can be used for testing, prototyping, or just messing around with compute shaders in a native environment. It uses the Metal API directly, and so is very performant. It is also a good small project for poking around in Metal.

## Building

Run the following commands from a shell.

    git clone https://github.com/mibu138/metaltoy.git
    cd metaltoy
    source env.sh
    cd build
    cmake ..
    cmake --build .

Source env.sh sets up some convenient environment variables. One of which helps metaltoy to locate the shader source files, which is loads at run time. It also adds the output binary directory to the path.

## Running

You should now be able to run metaltoy from the shell.

    metaltoy

A window should open with the mandelbrot set. In a text editor, open `src/shader.metal`. This file contains the shader being run. Edits to it will immediately be reflected in the image on the screen.
