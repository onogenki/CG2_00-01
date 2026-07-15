#pragma once

#include "Transform.h"

#include <algorithm>
#include <cmath>
#include <vector>

class ReturnPlaybackState
{
public:
	bool IsReturning() const { return returning_; }
	void SetReturning(bool returning) { returning_ = returning; }
	void Reset() { returning_ = false; }

private:
	bool returning_ = false;
};

class TransformPlaybackController
{
public:
	void RecordEdit(const Transform& before, const Transform& after, float elapsedSeconds)
	{
		if (NearlyEqual(before, after)) {
			return;
		}

		const bool createsNewBranch =
			!HasHistory() || returning_ || movingForward_ || canMoveForward_ ||
			playbackTime_ < duration_ - kEpsilon_;
		if (createsNewBranch) {
			keyframes_.clear();
			duration_ = 0.0f;
			playbackTime_ = 0.0f;
			keyframes_.push_back({ before, 0.0f });
		}

		const float editDuration = (std::max)(elapsedSeconds, kMinimumEditDurationSeconds_);
		duration_ += editDuration;
		keyframes_.push_back({ after, duration_ });
		CompactKeyframesIfNeeded();
		playbackTime_ = duration_;
		returning_ = false;
		movingForward_ = false;
		canMoveForward_ = false;
	}

	void SetReturning(bool returning)
	{
		if (returning && HasHistory()) {
			returning_ = true;
			movingForward_ = false;
			canMoveForward_ = false;
			return;
		}
		if (!returning && returning_) {
			returning_ = false;
			canMoveForward_ = HasHistory() && playbackTime_ < duration_ - kEpsilon_;
		}
	}

	bool StartMoveForward()
	{
		if (!HasHistory() || returning_ || !canMoveForward_ || playbackTime_ >= duration_ - kEpsilon_) {
			return false;
		}
		movingForward_ = true;
		canMoveForward_ = false;
		return true;
	}

	void Update(Transform& transform, float deltaTime)
	{
		const float step = (std::max)(0.0f, deltaTime);
		if (returning_) {
			playbackTime_ = (std::max)(0.0f, playbackTime_ - step);
			transform = Sample(playbackTime_);
			if (playbackTime_ <= kEpsilon_) {
				playbackTime_ = 0.0f;
				transform = keyframes_.front().transform;
				returning_ = false;
				canMoveForward_ = true;
			}
		} else if (movingForward_) {
			playbackTime_ = (std::min)(duration_, playbackTime_ + step);
			transform = Sample(playbackTime_);
			if (playbackTime_ >= duration_ - kEpsilon_) {
				playbackTime_ = duration_;
				transform = keyframes_.back().transform;
				movingForward_ = false;
			}
		}
	}

	bool IsReturning() const { return returning_; }
	bool IsMovingForward() const { return movingForward_; }
	bool HasHistory() const { return keyframes_.size() >= 2 && duration_ > kEpsilon_; }
	bool CanMoveForward() const { return canMoveForward_ && !returning_ && !movingForward_; }
	float GetProgress() const { return duration_ > kEpsilon_ ? playbackTime_ / duration_ : 1.0f; }
	float GetPlaybackTime() const { return playbackTime_; }
	float GetDuration() const { return duration_; }

	void Reset()
	{
		keyframes_.clear();
		playbackTime_ = 0.0f;
		duration_ = 0.0f;
		returning_ = false;
		movingForward_ = false;
		canMoveForward_ = false;
	}

private:
	static bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) <= kEpsilon_;
	}

	static bool NearlyEqual(const Vector3& lhs, const Vector3& rhs)
	{
		return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
	}

	static bool NearlyEqual(const Transform& lhs, const Transform& rhs)
	{
		return NearlyEqual(lhs.scale, rhs.scale) &&
			NearlyEqual(lhs.rotate, rhs.rotate) &&
			NearlyEqual(lhs.translate, rhs.translate);
	}

	static Vector3 Interpolate(const Vector3& from, const Vector3& to, float progress)
	{
		return {
			from.x + (to.x - from.x) * progress,
			from.y + (to.y - from.y) * progress,
			from.z + (to.z - from.z) * progress
		};
	}

	static Transform Interpolate(const Transform& from, const Transform& to, float progress)
	{
		return {
			Interpolate(from.scale, to.scale, progress),
			Interpolate(from.rotate, to.rotate, progress),
			Interpolate(from.translate, to.translate, progress)
		};
	}

	struct Keyframe
	{
		Transform transform{};
		float time = 0.0f;
	};

	Transform Sample(float time) const
	{
		if (keyframes_.empty()) {
			return {};
		}
		if (time <= keyframes_.front().time) {
			return keyframes_.front().transform;
		}
		if (time >= keyframes_.back().time) {
			return keyframes_.back().transform;
		}

		const auto upper = std::lower_bound(
			keyframes_.begin(),
			keyframes_.end(),
			time,
			[](const Keyframe& keyframe, float value) { return keyframe.time < value; });
		const auto lower = upper - 1;
		const float segmentDuration = upper->time - lower->time;
		const float progress = segmentDuration > kEpsilon_
			? (time - lower->time) / segmentDuration
			: 1.0f;
		return Interpolate(lower->transform, upper->transform, progress);
	}

	void CompactKeyframesIfNeeded()
	{
		if (keyframes_.size() <= kMaximumKeyframes_) {
			return;
		}

		std::vector<Keyframe> compacted;
		compacted.reserve(keyframes_.size() / 2 + 2);
		compacted.push_back(keyframes_.front());
		for (size_t index = 2; index + 1 < keyframes_.size(); index += 2) {
			compacted.push_back(keyframes_[index]);
		}
		compacted.push_back(keyframes_.back());
		keyframes_.swap(compacted);
	}

	static constexpr size_t kMaximumKeyframes_ = 2048;
	static constexpr float kMinimumEditDurationSeconds_ = 1.0f / 240.0f;
	static constexpr float kEpsilon_ = 0.0001f;
	std::vector<Keyframe> keyframes_;
	float playbackTime_ = 0.0f;
	float duration_ = 0.0f;
	bool returning_ = false;
	bool movingForward_ = false;
	bool canMoveForward_ = false;
};
