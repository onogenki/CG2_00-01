#pragma once

struct Vector2 {
	float x, y;

	Vector2& operator+=(const Vector2& other) {
		x += other.x;
		y += other.y;
		return *this;
	}

	Vector2 operator+(const Vector2& other) const {
		return { x + other.x, y + other.y };
	}


};
