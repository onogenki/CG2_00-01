#pragma once

#include "Mirror.h"
#include "Vector3.h"
#include <cstddef>
#include <vector>

// レーザーの一本分です。反射するたびにSegmentが一つ増えます。
struct LaserSegment
{
	Vector3 start{};
	Vector3 end{};
	bool hitMirror = false;
	size_t mirrorIndex = 0;
};

// 複数の鏡へ順番に当て、反射後の経路を計算するクラスです。
class Laser
{
public:
	void SetOrigin(const Vector3& origin) { origin_ = origin; }
	void SetDirection(const Vector3& direction) { direction_ = MyMath::Normalize(direction); }
	void SetMaxDistance(float distance) { maxDistance_ = distance; }
	void SetMaxReflectionCount(size_t count) { maxReflectionCount_ = count; }

	// fixedMirrorとcarryableMirrorを同じ一覧で受け取り、最も近い鏡から反射します。
	void Update(const std::vector<const Mirror*>& mirrors);
	// 床・壁などのOBBへ先に当たった場合、そこから先のLaser線分を消します。
	void ClipByObbs(const std::vector<MyMath::OBB>& blockingObbs, float padding = 0.0f);

	const std::vector<LaserSegment>& GetSegments() const { return segments_; }

private:
	Vector3 origin_{ 0.0f, 0.0f, 0.0f };
	Vector3 direction_{ 0.0f, 0.0f, 1.0f };
	float maxDistance_ = 50.0f;
	size_t maxReflectionCount_ = 8;
	std::vector<LaserSegment> segments_;
};
