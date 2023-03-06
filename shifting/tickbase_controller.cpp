#include "tickbase_controller.hpp"
#include "../checksum.hpp"

void write_cmd_from_user(bf_write* buf, CUserCmd* in, CUserCmd* out) {
	static auto WriteUsercmdF = pattern::find(g_csgo.m_client_dll, XOR("55 8B EC 83 E4 F8 51 53 56 8B D9 8B 0D"));

	__asm
	{
		mov ecx, buf
		mov edx, in
		push out
		call WriteUsercmdF
		add esp, 4
	}
}

bool Hooks::WriteUsercmdDeltaToBuffer(int slot, bf_write* buf, int from, int to, bool isnewcommand) {
	if (g_cl.m_processing && g_csgo.m_engine->IsConnected() && g_csgo.m_engine->IsInGame()) {
		uintptr_t stackbase;
		__asm mov stackbase, ebp;
		CCLCMsg_Move_t* msg = reinterpret_cast<CCLCMsg_Move_t*>(stackbase + 0xFCC);

		if (g_tickshift.m_tick_to_shift_alternate > 0) {
			if (from != -1)
				return true;

			int32_t new_commands = msg->new_commands;
			int Next_Command = g_csgo.m_cl->m_last_outgoing_command + g_csgo.m_cl->m_choked_commands + 1;
			int CommandsToAdd = std::min(g_tickshift.m_tick_to_shift_alternate, 16);

			g_tickshift.m_tick_to_shift_alternate = 0;
			msg->new_commands = CommandsToAdd;
			msg->backup_commands = 0;
			from = -1;

			for (to = Next_Command - new_commands + 1; to <= Next_Command; to++) {
				if (!g_hooks.m_client.GetOldMethod< WriteUsercmdDeltaToBuffer_t >(23)(this, slot, buf, from, to, isnewcommand))
					return false;
				from = to;
			}

			CUserCmd* last_command = g_csgo.m_input->GetUserCmd(slot, from);
			CUserCmd nullcmd;
			CUserCmd ShiftCommand;

			if (last_command)
				nullcmd = *last_command;

			ShiftCommand = nullcmd;
			ShiftCommand.m_command_number++;
			ShiftCommand.m_tick += 100;

			for (int i = new_commands; i <= CommandsToAdd; i++) {
				write_cmd_from_user(buf, &ShiftCommand, &nullcmd);
				nullcmd = ShiftCommand;
				ShiftCommand.m_command_number++;
				ShiftCommand.m_tick++;
			}
		}
	}

	return g_hooks.m_client.GetOldMethod< WriteUsercmdDeltaToBuffer_t >(23)(this, slot, buf, from, to, isnewcommand);
}

void c_tickshift::handle_doubletap() {
//	if (GetAsyncKeyState(g_menu.main.aimbot.doubletap.get())) {
	//	m_double_tap = !m_double_tap;
	//}

	if (!m_double_tap && m_charged) {
		m_charge_timer = 0;
		m_tick_to_shift = 16;
	}
	if (!m_double_tap) return;
	if (!m_charged) {
		if (m_charge_timer > game::TIME_TO_TICKS(.5)) { // .5 seconds after shifting, lets recharge
			m_tick_to_recharge = 16;
		}
		else {
			if (!g_aimbot.m_target) {
				m_charge_timer++;
			}
			if (g_cl.m_cmd->m_buttons & IN_ATTACK && g_cl.m_weapon_fire) {
				m_charge_timer = 0;
			}
		}
	}
	if (g_input.GetKeyState(g_menu.main.movement.fakewalk.get())) {
		m_charge_timer = 0;
		m_charged = false;
	}
	if (g_cl.m_cmd->m_buttons & IN_ATTACK && g_cl.m_weapon_fire && m_charged) {
		// shot.. lets shift tickbase back so we can dt.
		m_charge_timer = 0;
		m_tick_to_shift = 16;
		m_shift_cmd = g_cl.m_cmd->m_command_number;
		m_shift_tickbase = g_cl.m_local->m_nTickBase();
		*g_cl.m_packet = false;
	}
	if (!m_charged) {
		m_charged_ticks = 0;
	}
}

void CL_Move(float accumulated_extra_samples, bool bFinalTick) {
	// local player check.
	if (!g_cl.m_processing)
		return o_CLMove(accumulated_extra_samples, bFinalTick);


	if (g_tickshift.m_tick_to_recharge > 0) {
		g_tickshift.m_tick_to_recharge--;
		g_tickshift.m_charged_ticks++;
		if (g_tickshift.m_tick_to_recharge == 0) {
			g_tickshift.m_charged = true;
		}
		return; // increment ticksforprocessing by not creating any usercmd's
	}

	o_CLMove(accumulated_extra_samples, bFinalTick); // cal original

	// shift = false
	g_tickshift.m_shifted = false;

	// decrement
	if (g_tickshift.m_tick_to_shift > 0) {
		g_tickshift.m_shifting = true;
		for (; g_tickshift.m_tick_to_shift > 0; g_tickshift.m_tick_to_shift--) {
			o_CLMove(accumulated_extra_samples, bFinalTick);
			g_tickshift.m_charged_ticks--;
		}
		g_tickshift.m_charged = false; // were just going to assume. not correct.
		g_tickshift.m_shifting = false;

		// shift = true
		g_tickshift.m_shifted = true;
	}
}
