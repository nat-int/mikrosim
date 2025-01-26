# mikrosim

small life simulator

## build instructions

To build this project, you'll need to have the following installed:

 - c++ compiler (with c++23 support (no need for modules) (c++20 might be enough, not sure))
 - cmake and a build system which cmake supports
 - glm
 - glfw3
 - vulkan
 - vulkan memory allocator
 - boost (math)

and optionally:

 - glslc (to compile shaders)
 - vulkan validation layers

the libraries should be installable like this on ubuntu

```sh
sudo apt install build-essential cmake
sudo apt install vulkan-tools libvulkan-dev spirv-tools
# now you can test those by running vkcube
sudo apt install libglfw3-dev libglm-dev libboost-dev
# then for vulkan memory allocator - compile from source and install:
git clone https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
cd VulkanMemoryAllocator
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --build build -t install

# optional - validation layers
sudo apt install vulkan-validationlayers-dev
```

or for arch

```sh
sudo pacman -S gcc cmake
sudo pacman -S vulkan-devel # (contains validation layers)
sudo pacman -S glm boost boost-libs glfw-wayland # or glfw-x11
# vulkan-memory-allocator is on AUR, or you can compile and install it manually like above

# optional - glslc
sudo pacman -S shaderc
```

then you can build and run the project like this:

```sh
git clone https://github.com/nat-int/mikrosim.git
cd mikrosim
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DVULKAN_VALIDATION=OFF -DCOMPILE_SHADERS=OFF
cmake --build build
cd out
./mikrosim
# or: mikrosim.exe # on windows
```

or if you want a more development-friendly setup:

```sh
git clone https://github.com/nat-int/mikrosim.git
cd mikrosim
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DVULKAN_VALIDATION=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
cd out
./mikrosim
# or: mikrosim.exe # on windows
```

