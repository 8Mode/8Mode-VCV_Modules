#include "SNsoft.hpp"
#include "sn76477.h"
#include "rescap.h"
#include "dsp/digital.hpp"

struct SN_VCO: Module
{
	enum ParamIds
	{
		PITCH_PARAM,
		V_PARAM,
		m_noise_clock_res,
		m_noise_filter_res,
		m_decay_res,
		m_attack_res,
		m_amplitude_res,
		m_feedback_res,
		m_vco_cap,
		m_vco_res,
		m_pitch_voltage,
		m_slf_cap,
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
		EXT_VCO, SLF_EXT, ONE_SHOT_GATE_PARAM, ATTACK_MOD_PARAM, DECAY_MOD_PARAM, NOISE_FREQ_MOD_PARAM, NOISE_FILTER_MOD_PARAM, ONE_SHOT_LENGTH_MOD_PARAM, DUTY_MOD_PARAM, NUM_INPUTS
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

	SchmittTrigger OneShotTrigger;

	void onSampleRateChange() override;
	sn76477_device sn;

	SN_VCO() :
			Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS)
	{

		sn.set_amp_res(100);
		sn.set_feedback_res(100);
		sn.set_m_our_sample_rate(engineGetSampleRate());
		sn.device_start();
	}
	void step() override;

};

void SN_VCO::onSampleRateChange()
{
	sn.set_m_our_sample_rate(engineGetSampleRate());
}

void SN_VCO::step()
{

	// Calculate VCO and SLF Oscillator Voltages
	double volts = 1.752 * powf(2.0f, -1 * (inputs[EXT_VCO].value + (params[m_vco_res].value - 4 + (params[VCO_SELECT_PARAM].value * 6.223494))));
	double slf_volts = 1.283184 * powf(2.0f, -1 * (inputs[SLF_EXT].value + (params[m_slf_res].value - 8 + (params[VCO_SELECT_PARAM].value * 6.223494))));

	// Applies parameters to SN76447 emulator
	float attack_res = params[m_attack_res].value + (((inputs[ATTACK_MOD_PARAM].value * 20) / 100) * 5000000);
	float decay_res = params[m_decay_res].value + (((inputs[DECAY_MOD_PARAM].value * 20) / 100) * 20000000);
	float noise_clock_res = params[m_noise_clock_res].value + (((inputs[NOISE_FREQ_MOD_PARAM].value * 20) / 100) * 3300000);
	float noise_filter_res = params[m_noise_filter_res].value + (((inputs[NOISE_FILTER_MOD_PARAM].value * 20) / 100) * 100000000);
	float one_shot_length = ((params[ONE_SHOT_CAP_PARAM].value ) + (((inputs[ONE_SHOT_LENGTH_MOD_PARAM].value * 20) / 100) * 2000)) / 1000000000;
	float duty_cycle = params[m_pitch_voltage].value + (((inputs[DUTY_MOD_PARAM].value * 20) / 100) * 4.55);

	sn.set_vco_params(2.30, (double) (params[m_vco_cap].value / 1000000), volts);
	sn.set_slf_params(CAP_U(.047), slf_volts);
	sn.set_noise_params(noise_clock_res, noise_filter_res, CAP_P(470));
	sn.set_decay_res(decay_res);
	sn.set_attack_params(0.00000005, attack_res);
	sn.set_pitch_voltage(duty_cycle);
	sn.set_mixer_params(params[M_MIXER_A_PARAM].value, params[M_MIXER_B_PARAM].value, params[M_MIXER_C_PARAM].value);
	sn.set_envelope(params[M_ENV_KNOB].value);
	sn.set_vco_mode(params[VCO_SELECT_PARAM].value);
	sn.set_oneshot_params(one_shot_length, 5000000);

	if (params[ONE_SHOT_PARAM].value)
		sn.shot_trigger();
	if (OneShotTrigger.process(inputs[ONE_SHOT_GATE_PARAM].value))
		sn.shot_trigger();

	Rsamples sam = sn.sound_stream_update(1);

	double sine = (5.0 * (double) sam.s1 / 25000) + 1.3;
	outputs[SINE_OUTPUT].value = sine;

	// Attempt at AGC for TRI output.

	triout = sam.s2;

	if (params[VCO_SELECT_PARAM].value)
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
		//Power = 10 * log10(energy / 16000);
		K = (float) sqrt((output_power_normal * 16000) / energy);
		energy = 0;
		acc = 0;
	}

	//outputs[RESOUT].value = noise_filter_res;
	//outputs[CAPOUT].value = 1;

	if (!params[VCO_SELECT_PARAM].value)
	{
		outputs[TRI_OUTPUT].value = (sample * K * 6000.5) - 190;
	}
	else
	{
		outputs[TRI_OUTPUT].value = sample * K * 100.5;
	}

	//float light = 1/duty_cycle/4.55;
	//lights[PIN1].value = clamp(light, 0.0f, 1.0f);

}

struct MyModuleWidget: ModuleWidget
{
	MyModuleWidget(SN_VCO *module) :
			ModuleWidget(module)
	{
		setPanel(SVG::load(assetPlugin(plugin, "res/SNsoft_Panel.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(ParamWidget::create<Knob16>(M_NOISE_CLOCK_RES_POSITION, module, SN_VCO::m_noise_clock_res, 10000, 3300000, 0.0));
		addParam(ParamWidget::create<Knob16>(M_NOISE_FILTER_RES_POSITION, module, SN_VCO::m_noise_filter_res, 1, 100000000, 0.0));
		addParam(ParamWidget::create<Knob16>(M_DECAY_RES_POSITION, module, SN_VCO::m_decay_res, 1, 20000000, 10000000));
		addParam(ParamWidget::create<Knob16>(M_ATTACK_RES_POSITION, module, SN_VCO::m_attack_res, 1, 5000000, 10));
		addParam(ParamWidget::create<Knob16>(M_VCO_RES_POSITION, module, SN_VCO::m_vco_res, 0, 8, 4));
		addParam(ParamWidget::create<Knob16>(M_SLF_RES_POSITION, module, SN_VCO::m_slf_res, 0, 16, 8));
		addParam(ParamWidget::create<EModeSlider>(M_MIXER_A_POSITION, module, SN_VCO::M_MIXER_A_PARAM, 0.0, 1.0, 1.0));
		addParam(ParamWidget::create<EModeSlider>(M_MIXER_B_POSITION, module, SN_VCO::M_MIXER_B_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<EModeSlider>(M_MIXER_C_POSITION, module, SN_VCO::M_MIXER_C_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<EModeSlider>(VCO_SELECT_POSITION, module, SN_VCO::VCO_SELECT_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<Snap_8M_Knob>(M_ENV_KNOB_POSITION, module, SN_VCO::M_ENV_KNOB, 0, 3, 0));
		addParam(ParamWidget::create<CKD6>(ONE_SHOT_POSITION, module, SN_VCO::ONE_SHOT_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<Knob16>(ONE_SHOT_CAP_POSITION, module, SN_VCO::ONE_SHOT_CAP_PARAM, 10, 2000, 500));
		addParam(ParamWidget::create<Knob16>(M_PITCH_VOLTAGE_POSITION, module, SN_VCO::m_pitch_voltage, 0, 4.55, 2.30));

		addInput(Port::create<PJ301MPort>(SLF_EXT_POSITION, Port::INPUT, module, SN_VCO::SLF_EXT));
		addInput(Port::create<PJ301MPort>(EXT_VCO_POSITION, Port::INPUT, module, SN_VCO::EXT_VCO));
		addInput(Port::create<PJ301MPort>(ONE_SHOT_GATE_POSITION, Port::INPUT, module, SN_VCO::ONE_SHOT_GATE_PARAM));
		addInput(Port::create<PJ301MPort>(ATTACK_MOD_POSITION, Port::INPUT, module, SN_VCO::ATTACK_MOD_PARAM));
		addInput(Port::create<PJ301MPort>(DECAY_MOD_POSITION, Port::INPUT, module, SN_VCO::DECAY_MOD_PARAM));
		addInput(Port::create<PJ301MPort>(NOISE_FREQ_MOD_POSITION, Port::INPUT, module, SN_VCO::NOISE_FREQ_MOD_PARAM));
		addInput(Port::create<PJ301MPort>(NOISE_FILTER_MOD_POSITION, Port::INPUT, module, SN_VCO::NOISE_FILTER_MOD_PARAM));
		addInput(Port::create<PJ301MPort>(ONE_SHOT_LENGTH_MOD_POSITION, Port::INPUT, module, SN_VCO::ONE_SHOT_LENGTH_MOD_PARAM));
		addInput(Port::create<PJ301MPort>(DUTY_MOD_POSITION, Port::INPUT, module, SN_VCO::DUTY_MOD_PARAM));

		addOutput(Port::create<PJ301MPort>(SINE_POSITION, Port::OUTPUT, module, SN_VCO::SINE_OUTPUT));
		addOutput(Port::create<PJ301MPort>(TRI_OUT_POSITION, Port::OUTPUT, module, SN_VCO::TRI_OUTPUT));
		//addOutput(Port::create<PJ301MPort>(RESOUT_POSITION, Port::OUTPUT, module, SN_VCO::RESOUT));
		//addOutput(Port::create<PJ301MPort>(CAPOUT_POSITION, Port::OUTPUT, module, SN_VCO::CAPOUT));

		//addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(55, 269), module, SN_VCO::PIN1));
		//addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(41, 59), module, SN_VCO::PIN1));


	}
};

// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelsoftSN = Model::create<SN_VCO, MyModuleWidget>("8Mode", "softSN", "softSN Machine", OSCILLATOR_TAG);

