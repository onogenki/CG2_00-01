#pragma once
#include"Vector3.h"
#include"Quaternion.h"

struct Transform
{
	Vector3 scale;
	Vector3 rotate;//Eulerでの回転
	Vector3 translate;
};

struct QuaternionTransform
{
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};