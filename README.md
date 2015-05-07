Screen Space Ambient Occlusion
===
This is sort of an implementation of [Scalable Ambient Obscurance] by McGuire et al. however I make
a few simplifying shortcuts in my implementation and don't achieve as good performance or quality.
I was also unable to get their new recommended estimator to behave so this implementation still uses
the Alchemy AO estimator initially recommended in the paper. I have a somewhat longer write up
available on [my webpage](http://www.willusher.io/courses/cs6610/) since this was initially implemented
as a class project.

Building
---
The build system is a bit awkward at the moment as I developed this project and [glt](https://github.com/Twinklebear/glt)
simultaneously so it's included as a git submodule. The project also depends on SDL2 and GLM which CMake is typically
able to find. If not you can pass `-DSDL2=path/to/sdl2` and `-DGLM=path/to/glm` when running CMake to help it out.
I also use [imgui](https://github.com/ocornut/imgui) which you should download and drop under `external/imgui`, later
I plan to add a download step to fetch imgui to the CMake build.

Running
---
You can pass any OBJ file through the command line but the camera and other settings for the AO are only really
configured for a slightly modified version of [Crytek Sponza](https://drive.google.com/file/d/0B-l_lLEMo1YeaDFEdVlZTWdlek0/view?usp=sharing)
that uses the OBJ file from [McGuire's meshes page](http://graphics.cs.williams.edu/data/meshes.xml) but the textures
from the original [Crytek Sponza](http://www.crytek.com/cryengine/cryengine3/downloads).

Images
---
Full render combining AO with all other effects:
![Full render](http://i.imgur.com/v7wWg9O.png)

Ambient occlusion values only:
![AO only](http://i.imgur.com/byM8iNh.png)

I also made a [short video](https://www.youtube.com/watch?v=Sd9wY19Cib0) which you can watch if you want to
see the effect in action without running the project yourself.

