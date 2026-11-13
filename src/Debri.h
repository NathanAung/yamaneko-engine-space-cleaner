#pragma once

#include <cmath>
#include "../engine/Engine.h"
#include "Player.h"

using namespace yamaneko_engine;

class Debri : public Module {
public:
	Vector3 velocity = Vector3(0.0f);
	Player* holder = nullptr;
	float shoot_speed = 13.5f;
	float friction = 1.15f;
	float dribble_offset = 0.85f;
	float steal_cooldown = 0.0f;

	void OnInitialize() override {
		if (!GetGameObject()) return;
		GetGameObject()->radius = 0.40f;
		GetTransform().scale = Vector3(0.25f, 0.25f, 0.25f);
		ResetBall();
	}

	void ResetBall() {
		if (holder) {
			holder->is_dribbling = false;
			holder = nullptr;
		}
		velocity = Vector3(0.0f);
		steal_cooldown = 0.0f;
		if (GetGameObject()) {
			GetTransform().position = Vector3(0.0f, 0.0f, 0.0f);
			GetTransform().rotation = Vector3(0.0f, 0.0f, 0.0f);
		}
	}

	void AttachToUnit(Player* unit) {
		if (!unit || !GetGameObject()) return;
		if (holder && holder != unit) {
			holder->is_dribbling = false;
		}
		holder = unit;
		unit->is_dribbling = true;
		velocity = Vector3(0.0f);
		UpdateDribblePosition();
	}

	void UpdateDribblePosition() {
		if (!holder || !GetGameObject()) return;
		GetTransform().position = holder->GetTransform().position + holder->assigned_direction * dribble_offset;
		GetTransform().position.y = 0.0f;
	}

	void Kick(const Vector3& dir, Player* kicker) {
		Vector3 flat_dir(dir.x, 0.0f, dir.z);
		float len = glm::length(flat_dir);
		Vector3 norm_dir = (len > 0.0001f) ? (flat_dir / len) : Vector3(1.0f, 0.0f, 0.0f);

		if (holder) {
			holder->is_dribbling = false;
			holder = nullptr;
		}
		if (kicker) {
			kicker->is_dribbling = false;
			kicker->has_kicked_this_turn = true;
			kicker->kick_immunity_timer = 0.25f;
			if (GetGameObject()) {
				GetTransform().position = kicker->GetTransform().position + norm_dir * (dribble_offset + 0.15f);
				GetTransform().position.y = 0.0f;
			}
		}
		velocity = norm_dir * shoot_speed;
	}

	void OnUpdate(float delta_time) override {
		if (steal_cooldown > 0.0f) {
			steal_cooldown -= delta_time;
			if (steal_cooldown < 0.0f) {
				steal_cooldown = 0.0f;
			}
		}
	}
};
