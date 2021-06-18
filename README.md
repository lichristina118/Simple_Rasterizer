# Introduction

The purpose of this document is to give a brief overview of my final
code for Assignment 2: Rasterization. I will cover some of the various
passes used such as geometry pass, shadow pass, lighting pass – which
includes ambient light, point light, sun sky, and occlusion – and the
bloom filter used in the sky model. Examples of the program in action as
well as a presentation of its major features can be found in the video
submission. This document will focus on computational and implementation
details.

# Pipeline Overview

(30,38)(0,0) (15,36)<span>(0,-1)<span>2</span></span>
(15,28)(0,-4)<span>7</span><span>(0,-1)<span>2</span></span>
(15,31)<span>(0,-1)<span>1</span></span>
(8,35)<span>(1,0)<span>7</span></span>
(8,31)<span>(1,0)<span>7</span></span>
(8,31)<span>(0,1)<span>4</span></span>
(20,33)<span>(1,0)<span>2</span></span>
(22,33)<span>(0,-1)<span>16</span></span>
(22,25)(0,-4)<span>3</span><span>(-1,0)<span>2</span></span>
(15,15)<span>(1,0)<span>7</span></span>
(22,15)<span>(0,-1)<span>6</span></span>
(22,9)<span>(-1,0)<span>2</span></span>
(10,32)<span>(10,2)<span>Geometry pass</span></span>
(10,28)<span>(10,2)<span>Shadow pass</span></span>
(10,24)<span>(10,2)<span>Point light pass</span></span>
(10,20)<span>(10,2)<span>Ambient light pass</span></span>
(10,16)<span>(10,2)<span>Sun-sky pass</span></span>
(8,12)<span>(14,2)<span>Blur pass (horizontal+vertical)</span></span>
(10, 8)<span>(10,2)<span>Merge pass</span></span> (9,
4)<span>(12,2)<span>Gamma correction pass</span></span> (10,
0)<span>(10,2)<span>Screen</span></span> (13,37)<span>Object
model</span> (9,26.7)<span>shadow map</span>
(23,25)<span>g-buffers</span> (23,24)<span>depth buffer</span>
(5.5,22.7)<span>accumulation buffer</span> (5.5,18.7)<span>accumulation
buffer</span> (4,10.7)<span>tempbuffer2 (mipmaps)</span>
(8,6.7)<span>merge buffer</span> (23,12)<span>accumulation buffer</span>

# Geometry Pass

The geometry pass, performed by the fragment shader
<span>`geopass.fs`</span>, requires a diffuse reflectance, and three
<span>`float`</span>’s: alpha, eta, and k\_s, in order to produce 4
<span>`vec3`</span>’s for the g-buffers: <span>`gNormal`</span> (surface
normals), <span>`gDiffuse_r`</span>, <span>`gAlpha`</span> ( =
<span>`vec3(alpha, eta, k_s)`</span>, and <span>`gConvert`</span>, with
each element of <span>`gConvert`</span> equaling 1 if the corresponding
element in <span>`gAlpha`</span> is flipped. In order to make sure that
all of the g-buffer values are in the range \([0,1]\), I stored
<span>`gNormal`</span> as <span>`(vNormal + 1.0) / 2.0`</span> with
<span>`vNormal`</span> as the vertex normal in world space. If
<span>`alpha > 1.0`</span> then I set `gAlpha.x = 1.0/alpha` and
`gConvert.x = 1.0`; similarly for <span>`eta`</span> and
<span>`k_s`</span>. There result is the following image showing all four
g-buffers:

![Normals, Diffuse Reflectance, Alpha/Eta/K\_s, and Convert
G-Buffers](gbuffers.png)

# Shadow Pass

For each point light, I rendered the geometry using the light position
as the camera position and generates a shadow-map buffer.

![Shadow Map](shadowmap.png)

# Lighting Pass

## Point light

For each point light, I used the g-buffers and the shadow map as the
input to generate the light and shadow effects. The result is blended
into the accumulation buffer including diffuse reflection and specular
reflection.

## Ambient Light

For each ambient light, the program calculates the ambient occlusion
effect by sampling random points within a hemisphere on top of the
fragment. If a point is within the geometry, it will be counted as
ambient occlusion.

![Point Light and Ambient Light Occlusion](ambientocclusion.png)

## Sun Sky

This adds the sun-sky effect using the Preetham model.

# Blur Pass and Merge Pass

The blur pass executes 4 blurs each with a horizon blur pass and a
vertical blur pass:

  - blur1: standard deviation: 6.2, radius: 24, mipmap level 1

  - blur2: standard deviation: 24.9 radius: 80, mipmap level 2

  - blur3: standard deviation: 81.0, radius: 243, mipmap level 3

  - blur4: standard deviation: 263, radius: 799, mipmap level 4

The result of each blur is saved into a mipmap of the temporary frame
buffer.  
The merge pass merges the above 4 blur results and the original image
with the weighted factors using the Spencer model:  
\(0.8843g(x) + 0.1g(6.2,x) + 0.012g(24.9,x) + 0.0027g(81.0,x) + 0.001(263,x)\)  
The result is saved into a frame buffer called mergebuffer.

![Blurred Sun-sky](blur.png)

# Gamma Correction Pass

This reads from the merge buffer, performs the gamma correction, and
renders the image to screen.

![Completed Bunny Scene](bunny.png)

# Implementation

For this project, the skybox and its mirror reflection are added to both
the forward and deferred renderer. In particular, a separate "skybox
pass" is added to draw the skybox images. To optimize the performance,
the depth value z for a pixel is always set to w (which is 1) in the
skybox vertex shader such that the skybox pixel will not be drawn if
there are anything in front of to it. The depth test is enabled and
`GL_LEQUAL` is set for depth function before the rendering.

![Forward Rendering with No Mirror Reflection](forward.png)

For the reflection in deferred rendering, a separate "skybox mirror
reflection" pass is added, where the normals from the g-buffers
generated by the geometry pass are used to calculate the reflection
vectors in the fragment shader. The reflection vector is used to sample
the skybox for pixel color, followed by the mirror reflection color
being added to the existing pixel color, as generated by the light and
shadow passes to form the final color for the pixel.

![Deferred Rendering with No Mirror Reflection](deferred.png)

The following data flow diagram depicts how skybox and mirror reflection
passes are plugged in into the deferred rendering process:

(14,24)(0,0) (4,19)<span>(4,2)<span>Geometry pass</span></span>
(4,15)<span>(4,2)</span>(4.7,16.2)<span>Shadow
and</span>(4.7,15.7)<span>light passes</span>
(4,11)<span>(4,2)</span>(4.5,12.2)<span>Skybox
mirror</span>(4.5,11.7)<span>reflection pass</span>
(10,11)<span>(4,2)</span>(10.7,12.2)<span>Sun-sky,
blur,</span>(10.7,11.7)<span>merge passes</span>
(4,7)<span>(4,2)<span>Skybox pass</span></span>
(4,3)<span>(4,2)</span>(4.2,4.2)<span>Gamma
correction</span>(4.2,3.7)<span>pass</span>
(6,23)(0,-4)<span>6</span><span>(0,-1)<span>2</span></span>
(6,18)<span>(1,0)<span>3</span></span>
(9,18)<span>(0,-1)<span>10</span></span>
(9,12.3)<span>(-1,0)<span>1</span></span>
(9,11.7)<span>(1,0)<span>1</span></span>
(9,8)<span>(-1,0)<span>1</span></span>
(2,13)<span>(0,-1)<span>5</span></span>
(2,12)<span>(1,0)<span>2</span></span>
(2,8)<span>(1,0)<span>2</span></span>
(6,14)<span>(1,0)<span>6</span></span>
(12,14)<span>(0,-1)<span>1</span></span>
(12,11)<span>(0,-1)<span>5</span></span>
(12,6)<span>(-1,0)<span>6</span></span> (4.7,23.5)<span>Object
module</span> (6.8,18.2)<span>g-buffers</span> (0,13.2)<span>Skybox
cubemap</span> (1.9,14.2)<span>accumulation buffer</span>
(6.2,5.5)<span>skybox buffer or merge buffer</span> (5,0.5)<span>To
Screen</span>
