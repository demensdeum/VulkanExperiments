clear
rm triangle.app
glslc vertexShader.vert -o vertexShader.spv 
glslc fragmentShader.frag -o fragmentShader.spv 
clang++ main.cpp -lvulkan -lSDL2 -o triangle.app
./triangle.app