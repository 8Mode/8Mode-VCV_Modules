#include "softSN.hpp"
#include "sn76477.h"
#include "rescap.h"

struct SN_VCO: Module
{
	enum ParamIds
	{
		m_noise_clock_res,
		m_noise_filter_res,
		m_decay_res,
		m_attack_res,
		m_vco_res,
		m_pitch_voltage,
		m_slf_res,
		M_MIXER_A_PARAM,
		M_MIXER_B_PARAM,
		M_MIXER_C_PARAM,
		M_ENV_KNOB,
		VCO_SELECT_PARAM,
		ONE_SHOT_PARAM,
		ONE_SHOT_CAP_PARAM,
		m_envelope_1,
		m_envelope_2,
		NUM_PARAMS
	};
	enum InputIds
	{
		EXT_VCO, SLF_EXT, ONE_SHOT_GATE_PARAM, ATTACK_MOD_PARAM,
		DECAY_MOD_PARAM, NOISE_FREQ_MOD_PARAM, NOISE_FILTER_MOD_PARAM,
		ONE_SHOT_LENGTH_MOD_PARAM, DUTY_MOD_PARAM, NUM_INPUTS
	};
	enum OutputIds
	{
		SINE_OUTPUT, TRI_OUTPUT, RESOUT, CAPOUT, NUM_OUTPUTS
	};
	enum LightIds
	{
		PIN1, NUM_LIGHTS
	};

	int acc = 0;
	double triout = 0;
	double Power = 0;
	float energy = 0;
	float sample = 0;
	float output_power_normal = 0;
	float K = 0;

	dsp::SchmittTrigger OneShotTrigger;

	void onSampleRateChange() override;

	sn76477_device sn;

	SN_VCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SN_VCO::m_noise_clock_res, 10000, 3300000, 0.0, "");
		configParam(SN_VCO::m_noise_filter_res, 1, 100000000, 0.0, "");
		configParam(SN_VCO::m_decay_res, 1, 20000000, 10000000, "");
		configParam(SN_VCO::m_attack_res, 1, 5000000, 10, "");
		configParam(SN_VCO::m_vco_res, 0, 8, 4, "");
		configParam(SN_VCO::m_slf_res, 0, 16, 8, "");
		configParam(SN_VCO::M_MIXER_A_PARAM, 0.0, 1.0, 1.0, "");
		configParam(SN_VCO::M_MIXER_B_PARAM, 0.0, 1.0, 0.0, "");
		configParam(SN_VCO::M_MIXER_C_PARAM, 0.0, 1.0, 0.0, "");
		configParam(SN_VCO::VCO_SELECT_PARAM, 0.0, 1.0, 0.0, "");
		configParam(SN_VCO::M_ENV_KNOB, 0, 3, 0, "");
		configParam(SN_VCO::ONE_SHOT_PARAM, 0.0, 1.0, 0.0, "");
		configParam(SN_VCO::ONE_SHOT_CAP_PARAM, 10, 2000, 500, "");
		configParam(SN_VCO::m_pitch_voltage, 0, 4.55, 2.30, "");
		sn.set_amp_res(100);
		sn.set_feedback_res(100);
		sn.set_m_our_sample_rate(APP->engine->getSampleRate());
		sn.device_start();
	}
	void process(const ProcessArgs& args) override;
};


void SN_VCO::onSampleRateChange()
{
	sn.set_m_our_sample_rate(APP->engine->getSampleRate());
}

void SN_VCO::process(const ProcessArgs& args)
{
	params[M_MIXER_A_PARAM].setValue(round(params[M_MIXER_A_PARAM].getValue()));
	params[M_MIXER_B_PARAM].setValue(round(params[M_MIXER_B_PARAM].getValue()));
	params[M_MIXER_C_PARAM].setValue(round(params[M_MIXER_C_PARAM].getValue()));
	params[M_ENV_KNOB].setValue(round(params[M_ENV_KNOB].getValue()));
	params[VCO_SELECT_PARAM].setValue(round(params[VCO_SELECT_PARAM].getValue()));

	// Calculate VCO and SLF Oscillator Voltages
	double volts = 1.752 * powf(2.0f, -1 * (inputs[EXT_VCO].getVoltage() + (params[m_vco_res].getValue() - 4 + (params[VCO_SELECT_PARAM].getValue() * 6.223494))));
	double slf_volts = 1.283184 * powf(2.0f, -1 * (inputs[SLF_EXT].getVoltage() + (params[m_slf_res].getValue() - 8 + (params[VCO_SELECT_PARAM].getValue() * 6.223494))));

	// Applies parameters to SN76447 emulator
	float attack_res = params[m_attack_res].getValue() + (((inputs[ATTACK_MOD_PARAM].getVoltage() * 20) / 100) * 5000000);
	float decay_res = params[m_decay_res].getValue() + (((inputs[DECAY_MOD_PARAM].getVoltage() * 20) / 100) * 20000000);
	float noise_clock_res = params[m_noise_clock_res].getValue() + (((inputs[NOISE_FREQ_MOD_PARAM].getVoltage() * 20) / 100) * 3300000);
	float noise_filter_res = params[m_noise_filter_res].getValue() + (((inputs[NOISE_FILTER_MOD_PARAM].getVoltage() * 20) / 100) * 100000000);
	float one_shot_length = ((params[ONE_SHOT_CAP_PARAM].getValue() ) + (((inputs[ONE_SHOT_LENGTH_MOD_PARAM].getVoltage() * 20) / 100) * 2000)) / 1000000000;
	float duty_cycle = params[m_pitch_voltage].getValue() + (((inputs[DUTY_MOD_PARAM].getVoltage() * 20) / 100) * 4.55);

	sn.set_vco_params(2.30, 0, volts);
	sn.set_slf_params(CAP_U(.047), slf_volts);
	sn.set_noise_params(noise_clock_res, noise_filter_res, CAP_P(470));
	sn.set_decay_res(decay_res);
	sn.set_attack_params(0.00000005, attack_res);
	sn.set_pitch_voltage(duty_cycle);
	sn.set_mixer_params(params[M_MIXER_A_PARAM].getValue(), params[M_MIXER_B_PARAM].getValue(), params[M_MIXER_C_PARAM].getValue());
	sn.set_envelope(params[M_ENV_KNOB].getValue());
	sn.set_vco_mode(params[VCO_SELECT_PARAM].getValue());
	sn.set_oneshot_params(one_shot_length, 5000000);

	// One Shot Trigger
	if (params[ONE_SHOT_PARAM].getValue())
		sn.shot_trigger();
	if (OneShotTrigger.process(inputs[ONE_SHOT_GATE_PARAM].getVoltage()))
		sn.shot_trigger();

	// Attempt at AGC for TRI output.

	Rsamples sam = sn.sound_stream_update(1);

	double sine = (5.0 * (double) sam.s1 / 25000) + 1.3;
	outputs[SINE_OUTPUT].setVoltage(sine);

	triout = sam.s2;

	if (params[VCO_SELECT_PARAM].getValue())
	{
		sample = (float) triout - 1.5;
	}
	else
	{
		sample = (float) triout;
	}
	energy += sample * sample;
	acc = acc + 1;

	if (acc == 16000)
	{
		output_power_normal = (float) pow((double) 10, (double) (-30 / 10));
		K = (float) sqrt((output_power_normal * 16000) / energy);
		energy = 0;
		acc = 0;
	}

	if (!params[VCO_SELECT_PARAM].getValue())
	{
		outputs[TRI_OUTPUT].setVoltage((sample * K * 6000.5) - 190);
	}
	else
	{
		outputs[TRI_OUTPUT].setVoltage(sample * K * 100.5);
	}

}

struct SN_VCOWidget : ModuleWidget {
	SN_VCOWidget(SN_VCO *module) {
    setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SNsoft_Panel.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<Knob16>(M_NOISE_CLOCK_RES_POSITION, module, SN_VCO::m_noise_clock_res));
		addParam(createParam<Knob16>(M_NOISE_FILTER_RES_POSITION, module, SN_VCO::m_noise_filter_res));
		addParam(createParam<Knob16>(M_DECAY_RES_POSITION, module, SN_VCO::m_decay_res));
		addParam(createParam<Knob16>(M_ATTACK_RES_POSITION, module, SN_VCO::m_attack_res));
		addParam(createParam<Knob16>(M_VCO_RES_POSITION, module, SN_VCO::m_vco_res));
		addParam(createParam<Knob16>(M_SLF_RES_POSITION, module, SN_VCO::m_slf_res));
		addParam(createParam<EModeSlider>(M_MIXER_A_POSITION, module, SN_VCO::M_MIXER_A_PARAM));
		addParam(createParam<EModeSlider>(M_MIXER_B_POSITION, module, SN_VCO::M_MIXER_B_PARAM));
		addParam(createParam<EModeSlider>(M_MIXER_C_POSITION, module, SN_VCO::M_MIXER_C_PARAM));
		addParam(createParam<EModeSlider>(VCO_SELECT_POSITION, module, SN_VCO::VCO_SELECT_PARAM));
		addParam(createParam<Snap_8M_Knob>(M_ENV_KNOB_POSITION, module, SN_VCO::M_ENV_KNOB));
		addParam(createParam<CKD6>(ONE_SHOT_POSITION, module, SN_VCO::ONE_SHOT_PARAM));
		addParam(createParam<Knob16>(ONE_SHOT_CAP_POSITION, module, SN_VCO::ONE_SHOT_CAP_PARAM));
		addParam(createParam<Knob16>(M_PITCH_VOLTAGE_POSITION, module, SN_VCO::m_pitch_voltage));

		addInput(createInput<PJ301MPort>(SLF_EXT_POSITION, module, SN_VCO::SLF_EXT));
		addInput(createInput<PJ301MPort>(EXT_VCO_POSITION, module, SN_VCO::EXT_VCO));
		addInput(createInput<PJ301MPort>(ONE_SHOT_GATE_POSITION, module, SN_VCO::ONE_SHOT_GATE_PARAM));
		addInput(createInput<PJ301MPort>(ATTACK_MOD_POSITION, module, SN_VCO::ATTACK_MOD_PARAM));
		addInput(createInput<PJ301MPort>(DECAY_MOD_POSITION, module, SN_VCO::DECAY_MOD_PARAM));
		addInput(createInput<PJ301MPort>(NOISE_FREQ_MOD_POSITION, module, SN_VCO::NOISE_FREQ_MOD_PARAM));
		addInput(createInput<PJ301MPort>(NOISE_FILTER_MOD_POSITION, module, SN_VCO::NOISE_FILTER_MOD_PARAM));
		addInput(createInput<PJ301MPort>(ONE_SHOT_LENGTH_MOD_POSITION, module, SN_VCO::ONE_SHOT_LENGTH_MOD_PARAM));
		addInput(createInput<PJ301MPort>(DUTY_MOD_POSITION, module, SN_VCO::DUTY_MOD_PARAM));

		addOutput(createOutput<PJ301MPort>(SINE_POSITION, module, SN_VCO::SINE_OUTPUT));
		addOutput(createOutput<PJ301MPort>(TRI_OUT_POSITION, module, SN_VCO::TRI_OUTPUT));
	}
};

Model *modelsoftSN = createModel<SN_VCO, SN_VCOWidget>("softSN");
