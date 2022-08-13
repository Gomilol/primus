#pragma once

class InputPrediction {
public:
	float m_curtime;
	float m_frametime;
	struct Variables_t {
		float m_flFrametime;
		float m_flCurtime;
		float m_flVelocityModifier;

		vec3_t m_vecVelocity;
		vec3_t m_vecOrigin;
		int m_fFlags;
	} m_stored_variables;

	struct PredictionData_t {
		int m_flUnpredictedFlags;
		vec3_t m_vecUnpredictedVelocity;
		vec3_t m_vecUnpredictedOrigin;
	} PredictionData;
public:
	void update( );
	void run( );
	void restore( );

	float m_perfect_accuracy;
};

extern InputPrediction g_inputpred;