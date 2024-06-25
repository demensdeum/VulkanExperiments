clear
rm red-triangle.app
glslc vertexShader.vert -o vertexShader.spv 
glslc fragmentShader.frag -o fragmentShader.spv 
clang++ main.cpp -lvulkan -lSDL2 -o red-triangle.app
./red-triangle.app