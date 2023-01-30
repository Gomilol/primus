#include "includes.h"

LagCompensation g_lagcomp{};;

bool LagCompensation::StartPrediction(AimPlayer* data) {
	if (data->m_records.empty() || data->m_player->dormant())
		return false;

	LagRecord* record = data->m_records[0].get();
	record->predict();
	size_t size = data->m_records.size();

	// Check if LC is broken
	bool broke_lc = false;
	if (size > 1 && (record->m_origin - data->m_records[1]->m_origin).length_2d_sqr() > 4096.f)
		broke_lc = true;
	else if (size > 2 && (data->m_records[1]->m_origin - data->m_records[2]->m_origin).length_2d_sqr() > 4096.f)
		broke_lc = true;

	if (!broke_lc)
		return false;

	// Calculate simulation time
	int simulation = game::TIME_TO_TICKS(record->m_sim_time);
	if (std::abs(g_cl.m_arrival_tick - simulation) >= 128)
		return true;

	// Calculate lag
	int lag = size <= 2 ? game::TIME_TO_TICKS(record->m_sim_time - data->m_records[1]->m_sim_time)
		: game::TIME_TO_TICKS(data->m_records[1]->m_sim_time - data->m_records[2]->m_sim_time);
	int updatedelta = g_cl.m_server_tick - record->m_tick;
	if (g_cl.m_latency_ticks <= lag - updatedelta)
		return true;
	int next = record->m_tick + 1;
	if (next + lag >= g_cl.m_arrival_tick)
		return true;

	// Calculate direction and change
	float dir = 0.f, change = 0.f;
	if (record->m_velocity.y != 0.f || record->m_velocity.x != 0.f)
		dir = math::rad_to_deg(std::atan2(record->m_velocity.y, record->m_velocity.x));
	if (size > 1) {
		float dt = record->m_sim_time - data->m_records[1]->m_sim_time;
		float prevdir = data->m_records[1]->m_velocity.y != 0.f || data->m_records[1]->m_velocity.x != 0.f
			? math::rad_to_deg(std::atan2(data->m_records[1]->m_velocity.y, data->m_records[1]->m_velocity.x))
			: 0.f;
		change = (math::NormalizedAngle(dir - prevdir) / dt) * g_csgo.m_globals->m_interval;
		change = std::min(89.f, std::max(-89.f, change));
	}

	// Store prediction data
	data->m_cur_tick = next;
	data->m_cur_dir = dir;
	data->m_cur_change = change;
	
	return false;
}

void LagCompensation::PlayerMove(LagRecord* record) {
	vec3_t start, end;
	CGameTrace trace;
	CTraceFilterWorldOnly filter;

	start = record->m_origin;
	end = start + (record->m_pred_velocity * g_csgo.m_globals->m_interval);

	g_csgo.m_engine_trace->TraceRay(Ray(start, end, record->m_mins, record->m_maxs), CONTENTS_SOLID, &filter, &trace);

	if (trace.m_fraction != 1.f) {
		for (int i = 0; i < 2; i++) {
			record->m_pred_velocity -= trace.m_plane.m_normal * record->m_pred_velocity.dot(trace.m_plane.m_normal);

			float adjust = record->m_pred_velocity.dot(trace.m_plane.m_normal);
			if (adjust < 0.f)
				record->m_pred_velocity -= (trace.m_plane.m_normal * adjust);

			start = trace.m_endpos;
			end = start + (record->m_pred_velocity * (g_csgo.m_globals->m_interval * (1.f - trace.m_fraction)));

			g_csgo.m_engine_trace->TraceRay(Ray(start, end, record->m_mins, record->m_maxs), CONTENTS_SOLID, &filter, &trace);
			if (trace.m_fraction == 1.f)
				break;
		}
	}

	record->m_pred_origin = trace.m_endpos;
	record->m_pred_flags &= ~FL_ONGROUND;

	end = record->m_pred_origin;
	end.z -= 2.f;

	g_csgo.m_engine_trace->TraceRay(Ray(record->m_pred_origin, end, record->m_mins, record->m_maxs), CONTENTS_SOLID, &filter, &trace);

	if (trace.m_fraction != 1.f && trace.m_plane.m_normal.z > 0.7f)
		record->m_pred_flags |= FL_ONGROUND;
}

void LagCompensation::AirAccelerate(LagRecord* record, ang_t angle, float fmove, float smove) {
	vec3_t fwd, right, wishvel, wishdir;
	float maxspeed, wishspeed, currentspeed, addspeed, accelspeed;

		// determine movement angles and get forward and right vectors
		math::AngleVectors(angle, &fwd, &right);
	fwd.z = 0.f;
	right.z = 0.f;

	// normalize vectors
	fwd.normalize();
	right.normalize();

	// determine x and y parts of velocity
	wishvel.x = fwd.x * fmove + right.x * smove;
	wishvel.y = fwd.y * fmove + right.y * smove;
	wishvel.z = 0.f;

	// get maxspeed and wish speed
	maxspeed = record->m_player->m_flMaxspeed();
	wishspeed = wishvel.normalize();
	wishspeed = std::min(wishspeed, maxspeed);
	wishspeed = std::min(wishspeed, 30.f);
	wishdir = wishvel;

	// determine current speed and addspeed
	currentspeed = record->m_pred_velocity.dot(wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0.f)
		return;

	// determine acceleration speed
	accelspeed = g_csgo.sv_airaccelerate->GetFloat() * wishspeed * g_csgo.m_globals->m_interval;
	accelspeed = std::min(accelspeed, addspeed);

	// add acceleration
	record->m_pred_velocity += (wishdir * accelspeed);
}

void LagCompensation::PredictAnimations(CCSGOPlayerAnimState* state, LagRecord* record) {
	struct AnimBackup_t {
		float  curtime;
		float  frametime;
		int    flags;
		int    eflags;
		vec3_t velocity;
	};

	// get player ptr.
	Player* player = record->m_player;

	// backup data.
	AnimBackup_t backup;
	backup.curtime = g_csgo.m_globals->m_curtime;
	backup.frametime = g_csgo.m_globals->m_frametime;
	backup.flags = player->m_fFlags();
	backup.eflags = player->m_iEFlags();
	backup.velocity = player->m_vecAbsVelocity();

	// set globals appropriately for animation.
	g_csgo.m_globals->m_curtime = record->m_pred_time;
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;

	// EFL_DIRTY_ABSVELOCITY
	// skip call to C_BaseEntity::CalcAbsoluteVelocity
	player->m_iEFlags() &= ~0x1000;

	// set predicted flags and velocity.
	player->m_fFlags() = record->m_pred_flags;
	player->m_vecAbsVelocity() = record->m_pred_velocity;

	// enable re-animation in the same frame if animated already.
	if (state->m_frame >= g_csgo.m_globals->m_frame)
		state->m_frame = g_csgo.m_globals->m_frame - 1;

	bool fake = g_menu.main.aimbot.correct.get();

	// rerun the resolver since we edited the origin.
	if (fake)
		g_resolver.ResolveAngles(player, record);

	// update animations.
	game::UpdateAnimationState(state, record->m_eye_angles);

	// rerun the pose correction cuz we are re-setupping them.
	if (fake)
		g_resolver.ResolvePoses(player, record);

	// get new rotation poses and layers.
	player->GetPoseParameters(record->m_poses);
	player->GetAnimLayers(record->m_layers);
	record->m_abs_ang = player->GetAbsAngles();

	// restore globals.
	g_csgo.m_globals->m_curtime = backup.curtime;
	g_csgo.m_globals->m_frametime = backup.frametime;

	// restore player data.
	player->m_fFlags() = backup.flags;
	player->m_iEFlags() = backup.eflags;
	player->m_vecAbsVelocity() = backup.velocity;
}