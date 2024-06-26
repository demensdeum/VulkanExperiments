#pragma once

struct Vertex3D{
	float position[3];
};

struct UV{
	float uv[2];
};

struct Vertex3D_UV{
	Vertex3D position;
	UV uv;
};
