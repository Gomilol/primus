#pragma once
#include "../includes.h"

namespace tickbase_controll {

	void round_reset();
	void reset_value();

	void copy_command(CUserCmd* cmd, int tickbase_shift);
	bool double_tap();
}

typedef void(*CLMove_t)(float accumulated_extra_samples, bool bFinalTick);
inline CLMove_t o_CLMove;
void CL_Move(float accumulated_extra_samples, bool bFinalTick);
class c_tickshift {
public:

	bool m_double_tap;

	void handle_doubletap();
	bool m_shifting;
	bool m_charged;
	int m_shift_cmd;
	int m_shift_tickbase;
	int m_charged_ticks;
	int m_charge_timer;
	int m_tick_to_shift;
	int m_tick_to_shift_alternate;
	int m_tick_to_recharge;
	bool m_shifted;
};

inline c_tickshift g_tickshift;