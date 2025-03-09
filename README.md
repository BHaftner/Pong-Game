# C++ Pong Game

A simple Pong game implemented in C++ using OpenGL, GLFW, STB_easy_font, and MiniAudio.
The goal was to be as low level as possible with minor exceptions.

## Features
- Realistic paddle and ball physics
- Sound effects for collisions
- AI opponent with adjustable difficulty
- Mouse-controlled player paddle

## Dependencies
- OpenGL
- GLFW
- GLAD
- MiniAudio
- STB Easy Font

## Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/BHaftner/Pong-Game.git
   cd Pong-Game
   
2. Install dependencies:
   - GLFW: I installed via MSYS2 Pacman --> pacman -S mingw-w64-x86_64-glfw
   - GLAD: Already in the repo. If curious I downloaded from https://glad.dav1d.de/ with the settings,
      Language: C/C++
      Specification: OpenGL
      Profile: Core
      API (gl): 3.3
   - MiniAudio: In the include folder or via https://github.com/nothings/stb/blob/master/stb_easy_font.h
   - STB Easy Font: In the include folder or via https://miniaud.io/
   
3. Compile:
   My compile script for Windows is in the git repo. Compilation will vary depending on your C++ compiler.
   Something along the lines of C:\msys64\mingw64\lib -lglfw3 -lopengl32 -lgdi32 and then remember to compile with include/glad.c
   Note, gdi32.lib is necessary for GLFW hence why it's in the compile script.

## Screenshots
![image](https://github.com/user-attachments/assets/2da3b841-67d6-44b8-a622-fd871b076418)
![image](https://github.com/user-attachments/assets/781fffde-a11a-4f15-9e14-64a1d1560ca7)


