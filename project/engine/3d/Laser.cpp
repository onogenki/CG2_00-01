#include "Laser.h"

#include "Collision.h"
#include "MyMath.h"
#include <limits>

using namespace MyMath;

void Laser::Update(const std::vector<const Mirror*>& mirrors)
{
	segments_.clear();
	Vector3 rayOrigin = origin_;
	Vector3 rayDirection = Normalize(direction_);
	float remainingDistance = maxDistance_;
	constexpr float kSurfaceOffset = 0.01f;

	for (size_t reflectionCount = 0;
		reflectionCount <= maxReflectionCount_ && remainingDistance > 0.0f;
		++reflectionCount) {
		float nearestDistance = (std::numeric_limits<float>::max)();
		Mirror::RayHit nearestHit{};
		size_t nearestMirrorIndex = 0;

		for (size_t mirrorIndex = 0; mirrorIndex < mirrors.size(); ++mirrorIndex) {
			if (!mirrors[mirrorIndex]) {
				continue;
			}
			const Mirror::RayHit hit = mirrors[mirrorIndex]->IntersectRay(
				rayOrigin,
				rayDirection,
				remainingDistance);
			if (hit.isHit && hit.distance < nearestDistance) {
				nearestDistance = hit.distance;
				nearestHit = hit;
				nearestMirrorIndex = mirrorIndex;
			}
		}

		if (!nearestHit.isHit) {
			segments_.push_back({
				rayOrigin,
				{
					rayOrigin.x + rayDirection.x * remainingDistance,
					rayOrigin.y + rayDirection.y * remainingDistance,
					rayOrigin.z + rayDirection.z * remainingDistance,
				},
				false,
				0,
			});
			break;
		}

		segments_.push_back({
			rayOrigin,
			nearestHit.position,
			true,
			nearestMirrorIndex,
		});
		remainingDistance -= nearestDistance;
		rayDirection = Normalize(mirrors[nearestMirrorIndex]->ReflectDirection(rayDirection));
		rayOrigin = {
			nearestHit.position.x + rayDirection.x * kSurfaceOffset,
			nearestHit.position.y + rayDirection.y * kSurfaceOffset,
			nearestHit.position.z + rayDirection.z * kSurfaceOffset,
		};
		remainingDistance -= kSurfaceOffset;
	}
}

void Laser::ClipByObbs(const std::vector<OBB>& blockingObbs, float padding)
{
	for (size_t segmentIndex = 0; segmentIndex < segments_.size(); ++segmentIndex) {
		LaserSegment& segment = segments_[segmentIndex];
		float nearestT = 1.0f;
		for (const OBB& blockingObb : blockingObbs) {
			const Collision::SegmentHit hit = Collision::SegmentOBB(
				segment.start,
				segment.end,
				blockingObb,
				padding);
			if (hit.isHit && hit.t < nearestT) {
				nearestT = hit.t;
			}
		}
		if (nearestT >= 0.9999f) {
			continue;
		}

		// 鏡へ届く前に床・壁へ当たったため、反射せずその位置でLightを止めます。
		segment.end = {
			segment.start.x + (segment.end.x - segment.start.x) * nearestT,
			segment.start.y + (segment.end.y - segment.start.y) * nearestT,
			segment.start.z + (segment.end.z - segment.start.z) * nearestT,
		};
		segment.hitMirror = false;
		segment.mirrorIndex = 0;
		segments_.erase(segments_.begin() + static_cast<std::ptrdiff_t>(segmentIndex + 1), segments_.end());
		return;
	}
}
