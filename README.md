# Aerkanis
An Real-time implementation of https://www.guerrilla-games.com/read/nubis-cubed, a highly detailed and immersive voxel-based cloud renderer and modeling approach by Guerrilla Games, with Vulkan and Slang.

The name, **Aerkanis**, is composed of **Aer**(air/atmosphere) + vul**kan** + nub**is** ).

## Overview
### Tech Stack
* **Graphics API:** Vulkan 1.4
* **Shading Language:** Slang
* **Language:** C++20 (Modern RAII)

## Features TODO
- **Basic Pipeline**
- **Cloud Algorithm**
  - [] nubis1 Height Gradient Interpolation
  - [] nubis2 Vertical Profile Model
  - [] nubis3 3D Voxel Ray Marching
  - [] thundercloud
- **Environment**
  - [] sky
      - [] skybox
      - [] time rolling
  - [] thunder
  - [] height/depth fog
- **Post Processing**
  - [] screen space weather(rain)
  - [] god ray
  - [] bloom, blur, etc.
- **Interaction**
  - [] going through
