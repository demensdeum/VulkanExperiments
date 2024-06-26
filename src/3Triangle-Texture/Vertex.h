// Basic vertex data definitions

#ifndef COMMON_VERTEX_H
#define COMMON_VERTEX_H

struct Vertex3D{
	float position[3];
};

struct ColorF{
	float color[3];
};

struct Vertex3D_ColorF_pack{
	Vertex3D position;
	ColorF color;
};

#endif //COMMON_VERTEX_H
