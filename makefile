
out = out/bin
inc = ext/inc
lib = ext/lib

cflags = -Wall -O3 -I$(inc)

compile_vulkan:
	gcc -c $(cflags) src/vulkan/core.c -o $(out)/vk_core.obj
	gcc -c $(cflags) src/vulkan/vram.c -o $(out)/vk_vram.obj
	gcc -c $(cflags) src/vulkan/resources.c -o $(out)/vk_resources.obj
	gcc -c $(cflags) src/vulkan/render.c -o $(out)/vk_render.obj
	gcc -c $(cflags) src/vulkan/vulkan.c -o $(out)/vulkan.obj

compile_main:
	gcc -c -Wall -O3 src/main.c -o $(out)/main.obj

compile_shaders:
	dxc -spirv -T vs_6_0 -E vertexFunc res/triangle.hlsl -Fo res/spv/triangle_v.spv
	dxc -spirv -T ps_6_0 -E fragmentFunc res/triangle.hlsl -Fo res/spv/triangle_f.spv
	dxc -spirv -T cs_6_0 -E computeFunc res/distribution.hlsl -Fo res/spv/distribution_c.spv
	./tools/shader_compiler.exe res/spv out/data/shaders.res src/vulkan/shaders.h

link:
	gcc -o out/bin/main.exe $(out)/main.obj $(out)/vk_core.obj $(out)/vk_vram.obj $(out)/vk_resources.obj $(out)/vk_render.obj $(out)/vulkan.obj -L$(lib) -lglfw3 -lvulkan-1 -lgdi32 -luser32
