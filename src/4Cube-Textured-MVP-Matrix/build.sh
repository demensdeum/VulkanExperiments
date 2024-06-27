clear
rm cube.app
rm vertexShader.spv
rm fragmentShader.spv 
glslc vertexShader.vert -o vertexShader.spv 
if [ $? -eq 0 ]; then
    echo "Vertex shader compile success"
else
    exit 1
fi
glslc fragmentShader.frag -o fragmentShader.spv 
if [ $? -eq 0 ]; then
    echo "Fragment shader compile success"
else
    exit 1
fi
clang++ main.cpp -g -lvulkan -lSDL2 -o cube.app
./cube.app