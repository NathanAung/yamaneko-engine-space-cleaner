#pragma once

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <cstdio>
#include <imgui.h>
#include <SDL2/SDL.h>
#include "../engine/scene/Scene.h"
#include "../engine/platform/sdl/Window.h"
#include "../engine/platform/sdl/Input.h"
#include "../engine/rhi/RHI.h"
#include "../engine/scene/module/Camera.h"
#include "Player.h"
#include "Ball.h"

using namespace yamaneko_engine;

/// <summary>
/// Core state machine phases for the 5-vs-5 turn-based strategy soccer match.
/// </summary>
enum class GamePhase {
	InstructionP1, // Player 1 sequentially assigns actions to their 5 units
	InstructionP2, // Player 2 sequentially assigns actions to their 5 units
	Movement,      // All 10 units execute their assigned actions simultaneously
	Resolution,    // Evaluate goals, reset ball if scored, check win condition, return to InstructionP1
	GameOver       // Match finished (first team to 3 points wins)
};


/// <summary>
/// Match-level state data structure managed by the core loop.
/// </summary>
struct MatchState {
	static constexpr int kUnitsPerTeam = 5;
	static constexpr int kWinScore = 3;

	int p1_score = 0;
	int p2_score = 0;
	int turn_number = 1;
	int active_unit_index = 0;
	int last_scored_team = -1; // -1: No goal, 0: Player 1 scored, 1: Player 2 scored

	float movement_timer = 0.0f;
	float movement_duration = 2.0f;

	bool HasWinner() const {
		return p1_score >= kWinScore || p2_score >= kWinScore;
	}

	int GetWinningPlayer() const {
		if (p1_score >= kWinScore) return 1;
		if (p2_score >= kWinScore) return 2;
		return 0;
	}
};


/// <summary>
/// Single-keyboard input binding configuration per player.
/// </summary>
struct PlayerInputScheme {
	SDL_Keycode switch_unit_key;
	SDL_Scancode dir_up;
	SDL_Scancode dir_down;
	SDL_Scancode dir_left;
	SDL_Scancode dir_right;
	SDL_Keycode action_stay_key;
	SDL_Keycode action_move_key;
	SDL_Keycode action_shoot_key;
};


class MainScene : public Scene {
public:
	MainScene() {
		InitializeInputSchemes();
		InitializeSceneObjects();
		StartNewMatch();
	}

	void OnEnter() override {}
	void OnExit() override {}


	// =========================================================================
	// CORE GAME LOOP & STATE MACHINE
	// =========================================================================

	// Switches between the five phases of the core game loop and state machine
	void OnUpdate(float deltaTime) override {
		switch (phase_) {
		case GamePhase::InstructionP1:
			UpdateInstructionPhase(team1_units_, p1_input_scheme_, true);
			break;
		case GamePhase::InstructionP2:
			UpdateInstructionPhase(team2_units_, p2_input_scheme_, false);
			break;
		case GamePhase::Movement:
			UpdateMovementPhase(deltaTime);
			break;
		case GamePhase::Resolution:
			UpdateResolutionPhase();
			break;
		case GamePhase::GameOver:
			UpdateGameOverPhase();
			break;
		}
	}


	void OnRender() override {
		// =====================================================================
		// [VISUAL PROGRAMMER HOOK]
		// Add custom scene/field rendering here.
		// =====================================================================
	}


	void OnUIRender() override {
		// =====================================================================
		// [VISUAL PROGRAMMER HOOK]
		// Replace or extend with final in-game HUD when ready.
		// Currently renders the Core Loop & State Machine demonstration view.
		// =====================================================================
		RenderCoreLoopDemoUI();
	}


private:
	GamePhase phase_ = GamePhase::InstructionP1;
	MatchState match_{};

	PlayerInputScheme p1_input_scheme_{};
	PlayerInputScheme p2_input_scheme_{};

	std::array<Player*, MatchState::kUnitsPerTeam> team1_units_{};
	std::array<Player*, MatchState::kUnitsPerTeam> team2_units_{};
	std::vector<Player*> all_units_;
	Ball* ball_ = nullptr;
	GameObject* ball_obj_ = nullptr;

	std::string status_log_ = "";

	// =========================================================================
	// INITIALIZATION
	// =========================================================================

	/// <summary>
	/// Initializes the input schemes for Player 1 and Player 2, mapping specific keys to unit selection, movement direction, and action assignment.
	/// </summary>
	void InitializeInputSchemes() {
		// Player 1: Tab | W, A, S, D | Z (Stay), X (Move Straight), C (Shoot)
		p1_input_scheme_ = {
			SDLK_TAB,
			SDL_SCANCODE_W,
			SDL_SCANCODE_S,
			SDL_SCANCODE_A,
			SDL_SCANCODE_D,
			SDLK_z,
			SDLK_x,
			SDLK_c
		};

		// Player 2: Right Shift | Arrow Keys | B (Stay), N (Move Straight), M (Shoot)
		p2_input_scheme_ = {
			SDLK_RSHIFT,
			SDL_SCANCODE_UP,
			SDL_SCANCODE_DOWN,
			SDL_SCANCODE_LEFT,
			SDL_SCANCODE_RIGHT,
			SDLK_b,
			SDLK_n,
			SDLK_m
		};
	}


	/// <summary>
	/// Initializes the scene objects for the match, including Player 1 and Player 2 units and the ball. Sets up their initial positions, directions, and links them to the core game loop.
	/// </summary>
	void InitializeSceneObjects() {
		// =====================================================================
		// [VISUAL PROGRAMMER: SHADER & CAMERA SETUP]
		// Uncomment and configure shader/camera when adding 3D models:
		// ---------------------------------------------------------------------
		// RHIDevice* device = RHI::GetDevice();
		// shader_ = device->CreateShader(
		//     "assets/shader/simple_vertex_shader.glsl",
		//     "assets/shader/simple_fragment_shader.glsl"
		// );
		// auto camera = std::make_unique<GameObject>("Camera", "", shader_);
		// camera->AddModule<Camera>();
		// camera->transform.position = Vector3(0.0f, 20.0f, 10.0f);
		// camera->transform.rotation = Vector3(Math::Radians(-65.0f), 0.0f, 0.0f);
		// AddGameObject(std::move(camera));
		// =====================================================================

		const std::array<Vector3, MatchState::kUnitsPerTeam> p1_home_positions = {
			Vector3(-10.0f, 0.0f,  0.0f),
			Vector3(-6.5f,  0.0f, -3.5f),
			Vector3(-6.5f,  0.0f,  3.5f),
			Vector3(-2.5f,  0.0f, -2.0f),
			Vector3(-2.5f,  0.0f,  2.0f)
		};

		const std::array<Vector3, MatchState::kUnitsPerTeam> p2_home_positions = {
			Vector3(10.0f, 0.0f,  0.0f),
			Vector3(6.5f,  0.0f, -3.5f),
			Vector3(6.5f,  0.0f,  3.5f),
			Vector3(2.5f,  0.0f, -2.0f),
			Vector3(2.5f,  0.0f,  2.0f)
		};

		// Create Player 1 Units (Model rendering disabled until Visual Programmer sets model path)
		for (int i = 0; i < MatchState::kUnitsPerTeam; ++i) {
			// [VISUAL PROGRAMMER]: Replace "" and nullptr below with your unit model path and shader_
			// e.g. std::make_unique<GameObject>("Team1", "assets/model/player/player.obj", shader_);
			auto unit_go = std::make_unique<GameObject>("Team1", "", nullptr);
			unit_go->is_active = false; // Set to true when rendering models

			Player* unit_mod = unit_go->AddModule<Player>(0, i, p1_home_positions[i], Vector3(1.0f, 0.0f, 0.0f));
			team1_units_[i] = unit_mod;
			all_units_.push_back(unit_mod);
			AddGameObject(std::move(unit_go));
		}

		// Create Player 2 Units (Model rendering disabled until Visual Programmer sets model path)
		for (int i = 0; i < MatchState::kUnitsPerTeam; ++i) {
			// [VISUAL PROGRAMMER]: Replace "" and nullptr below with your unit model path and shader_
			// e.g. std::make_unique<GameObject>("Team2", "assets/model/player/player.obj", shader_);
			auto unit_go = std::make_unique<GameObject>("Team2", "", nullptr);
			unit_go->is_active = false; // Set to true when rendering models

			Player* unit_mod = unit_go->AddModule<Player>(1, i, p2_home_positions[i], Vector3(-1.0f, 0.0f, 0.0f));
			team2_units_[i] = unit_mod;
			all_units_.push_back(unit_mod);
			AddGameObject(std::move(unit_go));
		}

		// Create Ball Object (Model rendering disabled until Visual Programmer sets model path)
		{
			// [VISUAL PROGRAMMER]: Replace "" and nullptr below with your ball model path and shader_
			// e.g. std::make_unique<GameObject>("Ball", "assets/model/ball/ball.obj", shader_);
			auto ball_go = std::make_unique<GameObject>("Ball", "", nullptr);
			ball_go->is_active = false; // Set to true when rendering models

			ball_ = ball_go->AddModule<Ball>();
			ball_obj_ = ball_go.get();
			AddGameObject(std::move(ball_go));
		}
	}


	// =========================================================================
	// MATCH & STATE MACHINE TRANSITIONS
	// =========================================================================

	/// <summary>
	/// Starts a new match by resetting the match state, resetting kickoff positions for all units and the ball, and entering the instruction phase for Player 1.
	/// </summary>
	void StartNewMatch() {
		match_ = MatchState{};
		ResetKickoffPositions();
		EnterInstructionPhaseP1("Match Started! Player 1: Assign actions for your 5 units.");
	}


	/// <summary>
	/// Resets the positions of all units to their home positions and resets the ball to its spawn position for the kickoff phase.
	/// </summary>
	void ResetKickoffPositions() {
		if (ball_) {
			ball_->ResetBall();
		}
		for (auto* unit : all_units_) {
			unit->ResetForKickoff();
		}
	}


	/// <summary>
	/// Transitions the game state to the Instruction Phase for Player 1, resetting the active unit index and preparing all units for action assignment.
	/// </summary>
	void EnterInstructionPhaseP1(const std::string& message = "Instruction Phase: Player 1's turn to assign unit actions.") {
		phase_ = GamePhase::InstructionP1;
		match_.active_unit_index = 0;
		for (auto* unit : all_units_) {
			unit->BeginInstructionPhase();
		}
		status_log_ = message;
	}


	/// <summary>
	/// Transitions the game state to the Instruction Phase for Player 2, resetting the active unit index and preparing all units for action assignment.
	/// </summary>
	void EnterInstructionPhaseP2() {
		phase_ = GamePhase::InstructionP2;
		match_.active_unit_index = 0;
		status_log_ = "Instruction Phase: Player 1 ready. Player 2's turn to assign unit actions.";
	}


	/// <summary>
	/// Transitions the game state to the Movement Phase, resetting the movement timer and preparing all units to execute their assigned actions simultaneously.
	/// </summary>
	void EnterMovementPhase() {
		phase_ = GamePhase::Movement;
		match_.movement_timer = 0.0f;
		match_.last_scored_team = -1;
		status_log_ = "Movement Phase: Executing all 10 unit actions simultaneously...";
		OnMovementPhaseStart();
	}


	/// <summary>
	/// Transitions the game state to the Resolution Phase, recording which team scored (if any) and preparing to evaluate goals, update scores, and check for a winner.
	/// </summary>
	void EnterResolutionPhase(int scored_team) {
		phase_ = GamePhase::Resolution;
		match_.last_scored_team = scored_team;
	}

	// =========================================================================
	// PHASE 1 & 2: SEQUENTIAL INSTRUCTION INPUT ARCHITECTURE
	// =========================================================================

	/// <summary>
	/// Updates the Instruction Phase for the specified team, handling input for switching active units, setting movement direction, and assigning actions (Stay, Move Straight, Shoot) to the currently active unit. Advances to the next unconfirmed unit or transitions to the next phase when all units have confirmed their actions.
	/// </summary>
	void UpdateInstructionPhase(
		std::array<Player*, MatchState::kUnitsPerTeam>& team_units,
		const PlayerInputScheme& scheme,
		bool is_player_1
	) {
		// 1. Switch Active Unit (Tab for P1, Right Shift for P2)
		if (Input::GetKeyDown(scheme.switch_unit_key)) {
			match_.active_unit_index = (match_.active_unit_index + 1) % MatchState::kUnitsPerTeam;
		}

		Player* active_unit = team_units[match_.active_unit_index];

		// 2. Set Direction (WASD for P1, Arrow Keys for P2)
		Vector3 dir_input(0.0f, 0.0f, 0.0f);
		if (Input::GetKey(scheme.dir_up))    dir_input.z -= 1.0f;
		if (Input::GetKey(scheme.dir_down))  dir_input.z += 1.0f;
		if (Input::GetKey(scheme.dir_left))  dir_input.x -= 1.0f;
		if (Input::GetKey(scheme.dir_right)) dir_input.x += 1.0f;

		if (glm::length(dir_input) > 0.01f) {
			active_unit->SetDirection(dir_input);
		}

		// 3. Assign Action (Stay: Z/N, Move Straight: X/M, Shoot: C/Comma)
		if (Input::GetKeyDown(scheme.action_stay_key)) {
			active_unit->AssignAction(ActionType::Stay);
			AdvanceInstructionSelection(team_units, is_player_1);
		} else if (Input::GetKeyDown(scheme.action_move_key)) {
			active_unit->AssignAction(ActionType::MoveStraight);
			AdvanceInstructionSelection(team_units, is_player_1);
		} else if (Input::GetKeyDown(scheme.action_shoot_key)) {
			active_unit->AssignAction(ActionType::Shoot);
			AdvanceInstructionSelection(team_units, is_player_1);
		}
	}


	/// <summary>
	/// Advances the active unit selection to the next unconfirmed unit in the team. If all units have confirmed their actions, transitions to the next phase (Player 2 Instruction Phase or Movement Phase) based on which player's turn it is.
	/// </summary>
	void AdvanceInstructionSelection(
		const std::array<Player*, MatchState::kUnitsPerTeam>& team_units,
		bool is_player_1
	) {
		// Check if all 5 units on this team have had an action assigned
		bool all_confirmed = true;
		for (const auto* unit : team_units) {
			if (!unit->command.is_confirmed) {
				all_confirmed = false;
				break;
			}
		}

		if (all_confirmed) {
			if (is_player_1) {
				EnterInstructionPhaseP2();
			} else {
				EnterMovementPhase();
			}
			return;
		}

		// Advance active unit cursor to the next unconfirmed unit
		for (int offset = 1; offset <= MatchState::kUnitsPerTeam; ++offset) {
			int next_idx = (match_.active_unit_index + offset) % MatchState::kUnitsPerTeam;
			if (!team_units[next_idx]->command.is_confirmed) {
				match_.active_unit_index = next_idx;
				return;
			}
		}
	}


	// =========================================================================
	// PHASE 3: MOVEMENT PHASE & GAMEPLAY HOOKS
	// =========================================================================

	/// <summary>
	/// Called at the start of the Movement Phase to initialize any per-turn physics velocities or trigger immediate kicks for units and the ball. This is a hook for the Gameplay Programmer to implement movement mechanics.
	/// </summary>
	void OnMovementPhaseStart() {
		// TODO (Gameplay Programmer):
		// Initialize per-turn physics velocities or trigger immediate kicks here.
	}


	/// <summary>
	/// Updates the Movement Phase by executing all units' assigned actions simultaneously, updating the ball's state, simulating collisions and turnovers, checking for goals, and transitioning to the Resolution Phase if a goal is scored or the movement timer expires.
	/// </summary>
	void UpdateMovementPhase(float deltaTime) {
		// Call hooks for Gameplay Mechanics Programmer
		for (auto* unit : all_units_) {
			unit->ExecuteMovementAction(deltaTime);
		}
		if (ball_) {
			ball_->UpdateGameplayMechanics(deltaTime);
		}
		SimulateCollisionsAndTurnovers(deltaTime);

		// Check if Gameplay Programmer's goal check detected a goal
		int scored_team = EvaluateGoalScored();
		if (scored_team != -1) {
			EnterResolutionPhase(scored_team);
			return;
		}

		// [CORE LOOP DEMO KEYS]: Allow simulating a goal during Movement Phase
		// Press [1] to simulate Player 1 scoring, [2] to simulate Player 2 scoring.
		if (Input::GetKeyDown(SDLK_1)) {
			EnterResolutionPhase(0);
			return;
		}
		if (Input::GetKeyDown(SDLK_2)) {
			EnterResolutionPhase(1);
			return;
		}

		match_.movement_timer += deltaTime;
		if (match_.movement_timer >= match_.movement_duration) {
			EnterResolutionPhase(-1);
		}
	}


	/// <summary>
	/// Simulates collisions and turnovers during the Movement Phase
	/// </summary>
	void SimulateCollisionsAndTurnovers(float deltaTime) {
		(void)deltaTime;
		// =====================================================================
		// [GAMEPLAY PROGRAMMER HOOK]
		// Implement collision detection for:
		// 1. Unit colliding with free ball -> ball_->SetHolder(unit) (Dribble state)
		// 2. Unit colliding with opponent in dribble state -> Turnover (steal)
		// 3. Unit intersecting trajectory of shot/pass -> Turnover (interception)
		// =====================================================================
	}


	/// <summary>
	/// Evaluates whether a goal has been scored during the Movement Phase. Returns 0 if Player 1 scored, 1 if Player 2 scored, or -1 if no goal was scored. This is a hook for the Gameplay Programmer to implement goal detection logic based on ball position and goal areas.
	/// </summary>
	int EvaluateGoalScored() {
		// =====================================================================
		// [GAMEPLAY PROGRAMMER HOOK]
		// Return 0 if Player 1 scored, 1 if Player 2 scored, or -1 if no goal.
		// =====================================================================
		return -1;
	}


	// =========================================================================
	// PHASE 4: RESOLUTION PHASE (GOALS, BALL RESET, WIN CONDITION)
	// =========================================================================

	/// <summary>
	/// Updates the Resolution Phase by evaluating which team scored (if any), updating the match score, resetting the ball and units to kickoff positions, checking for a winner, and transitioning to the next turn or Game Over phase as appropriate.
	/// </summary>
	void UpdateResolutionPhase() {
		if (match_.last_scored_team == 0) {
			match_.p1_score++;
			ResetKickoffPositions();
			status_log_ = "Resolution: Player 1 scored! Ball & units reset to kickoff positions.";
		} else if (match_.last_scored_team == 1) {
			match_.p2_score++;
			ResetKickoffPositions();
			status_log_ = "Resolution: Player 2 scored! Ball & units reset to kickoff positions.";
		} else {
			status_log_ = "Resolution: No goal scored. Proceeding to next turn.";
		}

		// Evaluate Win Condition (First to 3 points wins)
		if (match_.HasWinner()) {
			phase_ = GamePhase::GameOver;
			status_log_ = "Match Over! Player " + std::to_string(match_.GetWinningPlayer()) + " reached 3 points and wins!";
			return;
		}

		// Advance turn and return to Instruction Phase
		match_.turn_number++;
		EnterInstructionPhaseP1(status_log_ + " -> Turn " + std::to_string(match_.turn_number) + " (Player 1 Instruction)");
	}


	// =========================================================================
	// PHASE 5: GAME OVER PHASE
	// =========================================================================
	void UpdateGameOverPhase() {
		if (Input::GetKeyDown(SDLK_SPACE) || Input::GetKeyDown(SDLK_RETURN)) {
			StartNewMatch();
		}
	}


	// =========================================================================
	// CORE LOOP DEMONSTRATION UI
	// =========================================================================
	static const char* ActionToString(ActionType action) {
		switch (action) {
		case ActionType::Stay:         return "Stay";
		case ActionType::MoveStraight: return "Move Straight";
		case ActionType::Shoot:        return "Shoot / Pass";
		}
		return "Unknown";
	}

	static const char* PhaseToString(GamePhase phase) {
		switch (phase) {
		case GamePhase::InstructionP1: return "INSTRUCTION PHASE - PLAYER 1";
		case GamePhase::InstructionP2: return "INSTRUCTION PHASE - PLAYER 2";
		case GamePhase::Movement:      return "MOVEMENT PHASE (Simultaneous Execution)";
		case GamePhase::Resolution:    return "RESOLUTION PHASE";
		case GamePhase::GameOver:      return "GAME OVER";
		}
		return "";
	}

	void RenderCoreLoopDemoUI() {
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoCollapse;

		if (ImGui::Begin("Core Game Loop & State Machine Monitor", nullptr, flags)) {
			ImGui::SetWindowFontScale(1.3f);
			ImGui::Text("5v5 Turn-Based Strategy Soccer - Core Loop & Input Architecture");
			ImGui::Separator();

			// Match & State Machine Overview
			ImGui::Text("Current Phase : %s", PhaseToString(phase_));
			ImGui::Text("Turn Number   : %d", match_.turn_number);
			ImGui::Text("Score (Win=3) : Player 1 [%d]  -  [%d] Player 2", match_.p1_score, match_.p2_score);
			ImGui::Text("Status Log    : %s", status_log_.c_str());

			if (phase_ == GamePhase::Movement) {
				ImGui::TextColored(
					ImVec4(0.4f, 1.0f, 0.5f, 1.0f),
					"Movement Timer: %.2f / %.2f s   (Demo: Press [1] for P1 Goal, [2] for P2 Goal)",
					match_.movement_timer,
					match_.movement_duration
				);
			} else if (phase_ == GamePhase::GameOver) {
				ImGui::TextColored(
					ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
					"WINNER: PLAYER %d!  Press [Space] or [Enter] to start a new match.",
					match_.GetWinningPlayer()
				);
			}

			ImGui::Separator();

			// Two-column display for Player 1 & Player 2 unit data structures
			ImGui::Columns(2, "TeamsColumns", true);

			// Player 1 Table
			bool p1_active = (phase_ == GamePhase::InstructionP1);
			ImGui::TextColored(
				p1_active ? ImVec4(0.3f, 0.8f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
				"PLAYER 1 UNITS %s", p1_active ? "[ACTIVE INPUT]" : ""
			);
			ImGui::Text("Controls: [Tab] Switch | [WASD] Dir | [Z] Stay | [X] Move | [C] Shoot");
			ImGui::Spacing();
			for (int i = 0; i < MatchState::kUnitsPerTeam; ++i) {
				const Player* u = team1_units_[i];
				bool is_sel = p1_active && (match_.active_unit_index == i);
				ImGui::TextColored(
					is_sel ? ImVec4(1.0f, 0.95f, 0.3f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
					"%s P1-Unit %d | Pos(%.1f, %.1f) | Dir(%.2f, %.2f) | Action: %-13s [%s]",
					is_sel ? ">>" : "  ",
					i + 1,
					u->GetTransform().position.x,
					u->GetTransform().position.z,
					u->command.direction.x,
					u->command.direction.z,
					ActionToString(u->command.action),
					u->command.is_confirmed ? "CONFIRMED" : "PENDING"
				);
			}

			ImGui::NextColumn();

			// Player 2 Table
			bool p2_active = (phase_ == GamePhase::InstructionP2);
			ImGui::TextColored(
				p2_active ? ImVec4(1.0f, 0.45f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
				"PLAYER 2 UNITS %s", p2_active ? "[ACTIVE INPUT]" : ""
			);
			ImGui::Text("Controls: [R-Shift] Switch | [Arrows] Dir | [B] Stay | [N] Move | [M] Shoot");
			ImGui::Spacing();
			for (int i = 0; i < MatchState::kUnitsPerTeam; ++i) {
				const Player* u = team2_units_[i];
				bool is_sel = p2_active && (match_.active_unit_index == i);
				ImGui::TextColored(
					is_sel ? ImVec4(1.0f, 0.95f, 0.3f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
					"%s P2-Unit %d | Pos(%.1f, %.1f) | Dir(%.2f, %.2f) | Action: %-13s [%s]",
					is_sel ? ">>" : "  ",
					i + 1,
					u->GetTransform().position.x,
					u->GetTransform().position.z,
					u->command.direction.x,
					u->command.direction.z,
					ActionToString(u->command.action),
					u->command.is_confirmed ? "CONFIRMED" : "PENDING"
				);
			}

			ImGui::Columns(1);
			ImGui::Separator();

			// Ball State Data Structure Display
			if (ball_ && ball_obj_) {
				std::string holder_str = "None (Loose Ball)";
				if (ball_->holder) {
					holder_str = "Player " + std::to_string(ball_->holder->team_id + 1) +
						" - Unit " + std::to_string(ball_->holder->unit_index + 1) + " (Dribbling)";
				}
				ImGui::Text(
					"Ball State | Pos(%.1f, %.1f, %.1f) | Vel(%.1f, %.1f, %.1f) | Holder: %s",
					ball_obj_->transform.position.x,
					ball_obj_->transform.position.y,
					ball_obj_->transform.position.z,
					ball_->velocity.x,
					ball_->velocity.y,
					ball_->velocity.z,
					holder_str.c_str()
				);
			}
		}
		ImGui::End();
	}
};
