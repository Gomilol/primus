#include "includes.h"

bool Hooks::ShouldDrawParticles( ) {
	return g_hooks.m_client_mode.GetOldMethod< ShouldDrawParticles_t >( IClientMode::SHOULDDRAWPARTICLES )( this );
}

bool Hooks::ShouldDrawFog( ) {
	// remove fog.
	if( g_menu.main.visuals.nofog.get( ) )
		return false;

	return g_hooks.m_client_mode.GetOldMethod< ShouldDrawFog_t >( IClientMode::SHOULDDRAWFOG )( this );
}

void Hooks::OverrideView( CViewSetup* view ) {
	// damn son.
	g_cl.m_local = g_csgo.m_entlist->GetClientEntity< Player* >( g_csgo.m_engine->GetLocalPlayer( ) );

	// g_grenades.think( );
	g_visuals.ThirdpersonThink( );

    // call original.
	g_hooks.m_client_mode.GetOldMethod< OverrideView_t >( IClientMode::OVERRIDEVIEW )( this, view );

    // remove scope edge blur.
	if( g_menu.main.visuals.noscope.get( ) ) {
		if( g_cl.m_local && g_cl.m_local->m_bIsScoped( ) )
            view->m_edge_blur = 0;
	}
}

bool Hooks::CreateMove( float time, CUserCmd* cmd ) {
	Stack   stack;
	bool    ret;

	// let original run first.
	ret = g_hooks.m_client_mode.GetOldMethod< CreateMove_t >( IClientMode::CREATEMOVE )( this, time, cmd );

	// called from CInput::ExtraMouseSample -> return original.
	if( !cmd->m_command_number )
		return ret;

	// if we arrived here, called from -> CInput::CreateMove
	// call EngineClient::SetViewAngles according to what the original returns.
	if( ret )
		g_csgo.m_engine->SetViewAngles( cmd->m_view_angles );

	// random_seed isn't generated in ClientMode::CreateMove yet, we must set generate it ourselves.
	cmd->m_random_seed = g_csgo.MD5_PseudoRandom( cmd->m_command_number ) & 0x7fffffff;

	// get bSendPacket off the stack.
	g_cl.m_packet = stack.next( ).local( 0x1c ).as< bool* >( );


	// menu shit for moving
	if (g_gui.m_open) {
		// are we IN_ATTACK?
		if (cmd->m_buttons & IN_ATTACK)
			cmd->m_buttons &= ~IN_ATTACK;

		// are we IN_ATTACK2?
		if (cmd->m_buttons & IN_ATTACK2)
			cmd->m_buttons &= ~IN_ATTACK2;
	}


	// get bFinalTick off the stack.
	g_cl.m_final_packet = stack.next( ).local( 0x1b ).as< bool* >( );

	// invoke move function.
	g_cl.OnTick( cmd );

	void UpdateInformation(); {
		if (!g_cl.m_local || !g_cl.m_processing)
			return false;

		if (g_cl.m_lag > 0)
			return false;

		CCSGOPlayerAnimState* state = g_cl.m_local->m_PlayerAnimState();
		if (!state)
			return false;

		
			if (g_csgo.m_cl->m_choked_commands > 0)
				return false;

		// update time.
		g_cl.m_anim_frame = g_csgo.m_globals->m_curtime - g_cl.m_anim_time;
		g_cl.m_anim_time = g_csgo.m_globals->m_curtime;

		// current angle will be animated.
		g_cl.m_angle = g_cl.m_cmd->m_view_angles;

		// fix landing anim.
		if (state->m_land && !state->m_dip_air && state->m_dip_cycle > 0.f)
			g_cl.m_angle.x = -12.f;

		math::clamp(g_cl.m_angle.x, -90.f, 90.f);
		g_cl.m_angle.normalize();

		// set lby to predicted value.
		//g_cl.m_local->m_flLowerBodyYawTarget() = g_cl.m_body;

		// pull the lower body direction towards the eye direction, but only when the player is moving
		if (state->m_ground) {
			// from csgo src sdk.
			const float CSGO_ANIM_LOWER_REALIGN_DELAY = 1.1f;

			// we are moving.
			if (state->m_speed > 0.1f || fabsf(state->m_fall_velocity) > 100.f) {
				g_cl.m_body_pred = g_cl.m_anim_time + (CSGO_ANIM_LOWER_REALIGN_DELAY * 0.2f);
				g_cl.m_body = g_cl.m_angle.y;
			}

			// we arent moving.
			else {
				// time for an update.
				if (g_cl.m_anim_time > g_cl.m_body_pred) {
					g_cl.m_body_pred = g_cl.m_anim_time + CSGO_ANIM_LOWER_REALIGN_DELAY;
					g_cl.m_body = g_cl.m_angle.y;
				}
			}
		}

		// save updated data.
		g_cl.m_rotation = g_cl.m_local->m_angAbsRotation();
		g_cl.m_speed = state->m_speed;
		g_cl.m_ground = state->m_ground;
	}
	return false;
}

bool Hooks::DoPostScreenSpaceEffects( CViewSetup* setup ) {
	g_visuals.RenderGlow( );

	return g_hooks.m_client_mode.GetOldMethod< DoPostScreenSpaceEffects_t >( IClientMode::DOPOSTSPACESCREENEFFECTS )( this, setup );
}