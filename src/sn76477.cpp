// Modified for use with SoftSN Machine VCVRack Module by Matt Dwyer/8Mode LLC  matt@8mode.com
// Original License and Copyright follows
//

// license:BSD-3-Clause
// copyright-holders:Zsolt Vasvari
// thanks-to:Derrick Renaud
/*****************************************************************************

    Texas Instruments SN76477 emulator

    authors: Derrick Renaud - info
             Zsolt Vasvari  - software

    (see sn76477.h for details)
    Notes:
        * All formulas were derived by taking measurements of a real device,
          then running the data sets through the numerical analysis
          application at http://zunzun.com to come up with the functions.

    Known issues/to-do's:
        * Use RES_INF for unconnected resistor pins and treat 0 as a short
          circuit

        * VCO
            * confirm value of VCO_MAX_EXT_VOLTAGE, VCO_TO_SLF_VOLTAGE_DIFF
              VCO_CAP_VOLTAGE_MIN and VCO_CAP_VOLTAGE_MAX
            * confirm value of VCO_MIN_DUTY_CYCLE
            * get real formulas for VCO cap charging and discharging
            * get real formula for VCO duty cycle
            * what happens if no vco_res
            * what happens if no vco_cap (needed for laserbat/lazarian)

        * Attack/Decay
            * get real formulas for a/d cap charging and discharging

        * Output
            * what happens if output is taken at pin 12 with no feedback_res
              (needed for laserbat/lazarian)

 *****************************************************************************/

#include "sn76477.h"
#include <stdio.h>
#include "math.h"

#define CHECK_BOOLEAN      assert((state & 0x01) == state)
#define CHECK_POSITIVE     assert(data >= 0.0)
#define CHECK_VOLTAGE      assert((data >= 0.0) && (data <= 5.0))
#define CHECK_CAP_VOLTAGE  assert(((data >= 0.0) && (data <= 5.0)) || (data == EXTERNAL_VOLTAGE_DISCONNECT))

/*****************************************************************************
 *
 *  Constants
 *
 *****************************************************************************/

#define ONE_SHOT_CAP_VOLTAGE_MIN    (0)         /* the voltage at which the one-shot starts from (measured) */
#define ONE_SHOT_CAP_VOLTAGE_MAX    (2.5)       /* the voltage at which the one-shot finishes (measured) */
#define ONE_SHOT_CAP_VOLTAGE_RANGE  (ONE_SHOT_CAP_VOLTAGE_MAX - ONE_SHOT_CAP_VOLTAGE_MIN)

#define SLF_CAP_VOLTAGE_MIN         (0.33)      /* the voltage at the bottom peak of the SLF triangle wave (measured) */
#define SLF_CAP_VOLTAGE_MAX         (2.37)      /* the voltage at the top peak of the SLF triangle wave (measured) */

#define SLF_CAP_VOLTAGE_RANGE       (SLF_CAP_VOLTAGE_MAX - SLF_CAP_VOLTAGE_MIN)
#define VCO_MAX_EXT_VOLTAGE         (2.35)      /* the external voltage at which the VCO saturates and produces no output,
                                                   also used as the voltage threshold for the SLF */
#define VCO_TO_SLF_VOLTAGE_DIFF     (0.35)
#define VCO_CAP_VOLTAGE_MIN         (SLF_CAP_VOLTAGE_MIN)   /* the voltage at the bottom peak of the VCO triangle wave */
#define VCO_CAP_VOLTAGE_MAX         (SLF_CAP_VOLTAGE_MAX + VCO_TO_SLF_VOLTAGE_DIFF) /* the voltage at the bottom peak of the VCO triangle wave */
#define VCO_CAP_VOLTAGE_RANGE       (VCO_CAP_VOLTAGE_MAX - VCO_CAP_VOLTAGE_MIN)
#define VCO_DUTY_CYCLE_50           (5.0)       /* the high voltage that produces a 50% duty cycle */
#define VCO_MIN_DUTY_CYCLE          (18)        /* the smallest possible duty cycle, in % */

#define NOISE_MIN_CLOCK_RES         RES_K(10)   /* the maximum resistor value that still produces a noise (measured) */
#define NOISE_MAX_CLOCK_RES         RES_M(3.3)  /* the minimum resistor value that still produces a noise (measured) */
#define NOISE_CAP_VOLTAGE_MIN       (0)         /* the minimum voltage that the noise filter cap can hold (measured) */
#define NOISE_CAP_VOLTAGE_MAX       (5.0)       /* the maximum voltage that the noise filter cap can hold (measured) */
#define NOISE_CAP_VOLTAGE_RANGE     (NOISE_CAP_VOLTAGE_MAX - NOISE_CAP_VOLTAGE_MIN)
#define NOISE_CAP_HIGH_THRESHOLD    (3.35)      /* the voltage at which the filtered noise bit goes to 0 (measured) */
#define NOISE_CAP_LOW_THRESHOLD     (0.74)      /* the voltage at which the filtered noise bit goes to 1 (measured) */

#define AD_CAP_VOLTAGE_MIN          (0)         /* the minimum voltage the attack/decay cap can hold (measured) */
#define AD_CAP_VOLTAGE_MAX          (4.44)      /* the minimum voltage the attack/decay cap can hold (measured) */
#define AD_CAP_VOLTAGE_RANGE        (AD_CAP_VOLTAGE_MAX - AD_CAP_VOLTAGE_MIN)

#define OUT_CENTER_LEVEL_VOLTAGE    (2.57)      /* the voltage that gets outputted when the volumne is 0 (measured) */
#define OUT_HIGH_CLIP_THRESHOLD     (3.51)      /* the maximum voltage that can be put out (measured) */
#define OUT_LOW_CLIP_THRESHOLD      (0.715)     /* the minimum voltage that can be put out (measured) */

/* gain factors for OUT voltage in 0.1V increments (measured) */
static constexpr double out_pos_gain[] =
{
	0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.01,  /* 0.0 - 0.9V */
	0.03, 0.11, 0.15, 0.19, 0.21, 0.23, 0.26, 0.29, 0.31, 0.33,  /* 1.0 - 1.9V */
	0.36, 0.38, 0.41, 0.43, 0.46, 0.49, 0.52, 0.54, 0.57, 0.60,  /* 2.0 - 2.9V */
	0.62, 0.65, 0.68, 0.70, 0.73, 0.76, 0.80, 0.82, 0.84, 0.87,  /* 3.0 - 3.9V */
	0.90, 0.93, 0.96, 0.98, 1.00                                 /* 4.0 - 4.4V */
};

static constexpr double out_neg_gain[] =
{
	 0.00,  0.00,  0.00,  0.00,  0.00,  0.00,  0.00,  0.00,  0.00, -0.01,  /* 0.0 - 0.9V */
	-0.02, -0.09, -0.13, -0.15, -0.17, -0.19, -0.22, -0.24, -0.26, -0.28,  /* 1.0 - 1.9V */
	-0.30, -0.32, -0.34, -0.37, -0.39, -0.41, -0.44, -0.46, -0.48, -0.51,  /* 2.0 - 2.9V */
	-0.53, -0.56, -0.58, -0.60, -0.62, -0.65, -0.67, -0.69, -0.72, -0.74,  /* 3.0 - 3.9V */
	-0.76, -0.78, -0.81, -0.84, -0.85                                      /* 4.0 - 4.4V */
};


/*****************************************************************************
 *
 *  Max/min
 *
 *****************************************************************************/

#undef max
#undef min

static inline double max(double a, double b)
{
	return (a > b) ? a : b;
}


static inline double min(double a, double b)
{
	return (a < b) ? a : b;
}









void sn76477_device::device_start()
{

			m_enable=0;
			m_envelope_mode=0;
			m_vco_mode=0;
			m_mixer_mode=0;
			m_one_shot_res=0;
			m_one_shot_cap=0;
			m_one_shot_cap_voltage_ext=0;
			m_slf_res=0;
			m_slf_cap=0;
			m_slf_cap_voltage_ext=0;
			m_vco_voltage=0;
			m_vco_res=0;
			m_vco_cap=0;
			m_vco_cap_voltage_ext=0;
			m_noise_clock_res=0;
			m_noise_clock_ext=0;
			m_noise_clock=0;
			m_noise_filter_res=0;
			m_noise_filter_cap=0;
			m_noise_filter_cap_voltage_ext=0;
			m_attack_res=0;
			m_decay_res=0;
			m_attack_decay_cap=0;
			m_attack_decay_cap_voltage_ext=0;
			//m_amplitude_res=0;
			//m_feedback_res=0;
			m_pitch_voltage=0;
			m_one_shot_cap_voltage=0;
			m_one_shot_running_ff=0;
			m_slf_cap_voltage=0;
			m_slf_out_ff=0;
			m_vco_cap_voltage=0;
			m_vco_out_ff=0;
			m_vco_alt_pos_edge_ff=0;
			m_noise_filter_cap_voltage=0;
			m_real_noise_bit_ff=0;
			m_filtered_noise_bit_ff=0;
			m_noise_gen_count=0;
			m_attack_decay_cap_voltage=0;
			m_rng=0;
			m_mixer_a=0;
			m_mixer_b=0;
			m_mixer_c=0;
			m_envelope_1=0;
			m_envelope_2=0;

    m_enable = 0;
	//m_our_sample_rate=44100;
   // m_amplitude_res=100000;
   // m_feedback_res=22000;
    //m_noise_clock_res=2000000;
	m_one_shot_cap_voltage = ONE_SHOT_CAP_VOLTAGE_MIN;

    m_slf_cap_voltage = SLF_CAP_VOLTAGE_MIN;
    m_vco_cap_voltage = VCO_CAP_VOLTAGE_MIN;
    m_noise_filter_cap_voltage = NOISE_CAP_VOLTAGE_MIN;
    m_attack_decay_cap_voltage = AD_CAP_VOLTAGE_MIN;

    m_vco_mode=1;
    m_vco_cap_voltage_ext=false;
    m_one_shot_cap_voltage_ext=false;
    m_slf_cap_voltage_ext=false;
    m_noise_filter_cap_voltage_ext=false;
    m_attack_decay_cap_voltage_ext=false;


    //m_noise_filter_cap_voltage=5;
	m_real_noise_bit_ff=1;
   m_filtered_noise_bit_ff=0;
    m_noise_gen_count=1;
intialize_noise();

}


/*****************************************************************************
 *
 *  Functions for computing frequencies, voltages and similar values based
 *  on the hardware itself.  Do NOT put anything emulation specific here,
 *  such as calculations based on sample_rate.
 *
 *****************************************************************************/

double sn76477_device::compute_one_shot_cap_charging_rate() /* in V/sec */
{
	/* this formula was derived using the data points below

	 Res (kohms)  Cap (uF)   Time (millisec)
	     47         0.33         11.84
	     47         1.0          36.2
	     47         1.5          52.1
	     47         2.0          76.4
	    100         0.33         24.4
	    100         1.0          75.2
	    100         1.5         108.5
	    100         2.0         158.4
	*/

	double ret = 0;

	if ((m_one_shot_res > 0) && (m_one_shot_cap > 0))
	{
		ret = ONE_SHOT_CAP_VOLTAGE_RANGE / (0.8024 * m_one_shot_res * m_one_shot_cap + 0.002079);
	}
	else if (m_one_shot_cap > 0)
	{
		/* if no resistor, there is no current to charge the cap,
		   effectively making the one-shot time effectively infinite */
		ret = +1e-30;
	}
	else if (m_one_shot_res > 0)
	{
		/* if no cap, the voltage changes extremely fast,
		   effectively making the one-shot time 0 */
		ret = +1e+30;

	}

	return ret;
}


double sn76477_device::compute_one_shot_cap_discharging_rate() /* in V/sec */
{
	/* this formula was derived using the data points below

	Cap (uF)   Time (microsec)
	  0.33           300
	  1.0            850
	  1.5           1300
	  2.0           1900
	*/

	double ret = 0;

	if ((m_one_shot_res > 0) && (m_one_shot_cap > 0))
	{
		ret = ONE_SHOT_CAP_VOLTAGE_RANGE / (854.7 * m_one_shot_cap + 0.00001795);
	}
	else if (m_one_shot_res > 0)
	{
		/* if no cap, the voltage changes extremely fast,
		   effectively making the one-shot time 0 */
		ret = +1e+30;
	}

	return ret;
}


double sn76477_device::compute_slf_cap_charging_rate() /* in V/sec */
{
	/* this formula was derived using the data points below

	Res (kohms)  Cap (uF)   Time (millisec)
	     47        0.47          14.3
	    120        0.47          35.6
	    200        0.47          59.2
	     47        1.00          28.6
	    120        1.00          71.6
	    200        1.00         119.0
	*/
	double ret = 0;

	if ((m_slf_res > 0) && (m_slf_cap > 0))
	{

		ret = 0.64 * 2 * VCO_CAP_VOLTAGE_RANGE / m_slf_res;
	}

	return ret;
}


double sn76477_device::compute_slf_cap_discharging_rate() /* in V/sec */
{
	/* this formula was derived using the data points below

	Res (kohms)  Cap (uF)   Time (millisec)
	     47        0.47          13.32
	    120        0.47          32.92
	    200        0.47          54.4
	     47        1.00          26.68
	    120        1.00          66.2
	    200        1.00         109.6
	*/
	double ret = 0;


	if ((m_slf_res > 0))
	{

		ret = 0.64 * 2 * VCO_CAP_VOLTAGE_RANGE / m_slf_res;

	}

	return ret;
}


double sn76477_device::compute_vco_cap_charging_discharging_rate() /* in V/sec */
{
	double ret = 0;


	if (m_vco_res > 0)
	{

		ret = 0.64 * 2 * VCO_CAP_VOLTAGE_RANGE / m_vco_res;
	}

	return ret;
}


double sn76477_device::compute_vco_duty_cycle() /* no measure, just a number */
{
	double ret = 0.5;   /* 50% */

	if ((m_vco_voltage > 0) && (m_pitch_voltage != VCO_DUTY_CYCLE_50))
	{
		ret = max(0.5 * (m_pitch_voltage / m_vco_voltage), (VCO_MIN_DUTY_CYCLE / 100.0));

		ret = min(ret, 1);
	}

	return ret;
}


uint32_t sn76477_device::compute_noise_gen_freq() /* in Hz */
{
	/* this formula was derived using the data points below

	 Res (ohms)   Freq (Hz)
	    10k         97493
	    12k         83333
	    15k         68493
	    22k         49164
	    27k         41166
	    33k         34449
	    36k         31969
	    47k         25126
	    56k         21322
	    68k         17721.5
	    82k         15089.2
	    100k        12712.0
	    150k         8746.4
	    220k         6122.4
	    270k         5101.5
	    330k         4217.2
	    390k         3614.5
	    470k         3081.7
	    680k         2132.7
	    820k         1801.8
	      1M         1459.9
	    2.2M          705.13
	    3.3M          487.59
	*/

	uint32_t ret = 0;

	if ((m_noise_clock_res >= NOISE_MIN_CLOCK_RES) &&
		(m_noise_clock_res <= NOISE_MAX_CLOCK_RES))
	{
		ret = 339100000 * pow(m_noise_clock_res, -0.8849);
	}

	return ret;
}


double sn76477_device::compute_noise_filter_cap_charging_rate() /* in V/sec */
{
	/* this formula was derived using the data points below

	 R*C        Time (sec)
	.000068     .0000184
	.0001496    .0000378
	.0002244    .0000548
	.0003196    .000077
	.0015       .000248
	.0033       .000540
	.00495      .000792
	.00705      .001096
	*/


	double ret = 0;

	if ((m_noise_filter_res > 0) && (m_noise_filter_cap > 0))
	{
		ret = NOISE_CAP_VOLTAGE_RANGE / (0.1571 * m_noise_filter_res * m_noise_filter_cap + 0.00001430);
	}
	else if (m_noise_filter_cap > 0)
	{
		/* if no resistor, there is no current to charge the cap,
		   effectively making the filter's output constants */
		ret = +1e-30;
	}
	else if (m_noise_filter_res > 0)
	{
		/* if no cap, the voltage changes extremely fast,
		   effectively disabling the filter */
		ret = +1e+30;
	}

return ret;
}


double sn76477_device::compute_noise_filter_cap_discharging_rate() /* in V/sec */
{
	/* this formula was derived using the data points below

	 R*C        Time (sec)
	.000068     .000016
	.0001496    .0000322
	.0002244    .0000472
	.0003196    .0000654
	.0015       .000219
	.0033       .000468
	.00495      .000676
	.00705      .000948
	*/

	double ret = 0;

	if ((m_noise_filter_res > 0) && (m_noise_filter_cap > 0))
	{
		ret = NOISE_CAP_VOLTAGE_RANGE / (0.1331 * m_noise_filter_res * m_noise_filter_cap + 0.00001734);
	}
	else if (m_noise_filter_cap > 0)
	{
		/* if no resistor, there is no current to charge the cap,
		   effectively making the filter's output constants */

		ret = +1e-30;
	}
	else if (m_noise_filter_res > 0)
	{
		/* if no cap, the voltage changes extremely fast,
		   effectively disabling the filter */

		ret = +1e+30;
	}

	return ret;
}


double sn76477_device::compute_attack_decay_cap_charging_rate()  /* in V/sec */
{
	double ret = 0;

	if ((m_attack_res > 0) && (m_attack_decay_cap > 0))
	{
		ret = AD_CAP_VOLTAGE_RANGE / (m_attack_res * m_attack_decay_cap);
	}
	else if (m_attack_decay_cap > 0)
	{
		/* if no resistor, there is no current to charge the cap,
		   effectively making the attack time infinite */
		ret = +1e-30;
	}
	else if (m_attack_res > 0)
	{
		/* if no cap, the voltage changes extremely fast,
		   effectively making the attack time 0 */
		ret = +1e+30;
	}

	return ret;
}


double sn76477_device::compute_attack_decay_cap_discharging_rate()  /* in V/sec */
{
	double ret = 0;

	if ((m_decay_res > 0) && (m_attack_decay_cap > 0))
	{
		ret = AD_CAP_VOLTAGE_RANGE / (m_decay_res * m_attack_decay_cap);
	}
	else if (m_attack_decay_cap > 0)
	{
		/* if no resistor, there is no current to charge the cap,
		   effectively making the decay time infinite */
		ret = +1e-30;
	}
	else if (m_attack_res > 0)
	{
		/* if no cap, the voltage changes extremely fast,
		   effectively making the decay time 0 */
		ret = +1e+30;
	}

	return ret;
}


double sn76477_device::compute_center_to_peak_voltage_out()
{
	/* this formula was derived using the data points below

	 Ra (kohms)  Rf (kohms)   Voltage
	    150         47          1.28
	    200         47          0.96
	     47         22          1.8
	    100         22          0.87
	    150         22          0.6
	    200         22          0.45
	     47         10          0.81
	    100         10          0.4
	    150         10          0.27
	*/

	double ret = 0;

	if (m_amplitude_res > 0)
	{
		ret = 3.818 * (m_feedback_res / m_amplitude_res) + 0.03;
	}

	return ret;
}



/*****************************************************************************
 *
 *  Noise generator
 *
 *****************************************************************************/

void sn76477_device::intialize_noise()
{
	m_rng = 0;
}


inline uint32_t sn76477_device::generate_next_real_noise_bit()
{
	uint32_t out = ((m_rng >> 28) & 1) ^ ((m_rng >> 0) & 1);

		/* if bits 0-4 and 28 are all zero then force the output to 1 */
	if ((m_rng & 0x1000001f) == 0)
	{
		out = 1;
	}

	m_rng = (m_rng >> 1) | (out << 30);

	return out;
}




Rsamples sn76477_device::sound_stream_update(int samples)
{
	double one_shot_cap_charging_step;
	double one_shot_cap_discharging_step;
	double slf_cap_charging_step;
	double slf_cap_discharging_step;
	double vco_duty_cycle_multiplier;
	double vco_cap_charging_step;
	double vco_cap_discharging_step;
	double vco_cap_voltage_max;
	uint32_t noise_gen_freq;
	double noise_filter_cap_charging_step;
	double noise_filter_cap_discharging_step;
	double attack_decay_cap_charging_step;
	double attack_decay_cap_discharging_step;
	int    attack_decay_cap_charging;
	double voltage_out;
	double center_to_peak_voltage_out;


	m_mixer_mode= (m_mixer_a & 0b00000001) | (m_mixer_b << 1 & 0b00000010) | (m_mixer_c << 2 & 0b00000100);



	one_shot_cap_charging_step = compute_one_shot_cap_charging_rate() / m_our_sample_rate;
	one_shot_cap_discharging_step = compute_one_shot_cap_discharging_rate() / m_our_sample_rate;

	slf_cap_charging_step = compute_slf_cap_charging_rate() / m_our_sample_rate;
	slf_cap_discharging_step = compute_slf_cap_discharging_rate() / m_our_sample_rate;

	vco_duty_cycle_multiplier = (1 - compute_vco_duty_cycle()) * 2;

	vco_cap_charging_step =    compute_vco_cap_charging_discharging_rate() / vco_duty_cycle_multiplier / m_our_sample_rate;
	vco_cap_discharging_step = compute_vco_cap_charging_discharging_rate() * vco_duty_cycle_multiplier / m_our_sample_rate;

	noise_filter_cap_charging_step = compute_noise_filter_cap_charging_rate() / m_our_sample_rate;
	noise_filter_cap_discharging_step = compute_noise_filter_cap_discharging_rate() / m_our_sample_rate;
	noise_gen_freq = compute_noise_gen_freq();

	attack_decay_cap_charging_step = compute_attack_decay_cap_charging_rate() / m_our_sample_rate;
	attack_decay_cap_discharging_step = compute_attack_decay_cap_discharging_rate() / m_our_sample_rate;

	center_to_peak_voltage_out = compute_center_to_peak_voltage_out();


	/* process 'samples' number of samples */

	samples=6;
	while (samples--)
	{

		/* update the one-shot cap voltage */
		if (!m_one_shot_cap_voltage_ext)
		{
			if (m_one_shot_running_ff)
			{
				/* charging */
				m_one_shot_cap_voltage = min(m_one_shot_cap_voltage + one_shot_cap_charging_step, ONE_SHOT_CAP_VOLTAGE_MAX);
			}
			else
			{
				/* discharging */
				m_one_shot_cap_voltage = max(m_one_shot_cap_voltage - one_shot_cap_discharging_step, ONE_SHOT_CAP_VOLTAGE_MIN);
			}
		}

		if (m_one_shot_cap_voltage >= ONE_SHOT_CAP_VOLTAGE_MAX)
		{
			m_one_shot_running_ff = 0;
		}


		/* update the SLF (super low frequency oscillator) */
		if (!m_slf_cap_voltage_ext)
		{
			/* internal */
			if (!m_slf_out_ff)
			{
				/* charging */
				m_slf_cap_voltage = min(m_slf_cap_voltage + slf_cap_charging_step, SLF_CAP_VOLTAGE_MAX);
			}
			else
			{
				/* discharging */
				m_slf_cap_voltage = max(m_slf_cap_voltage - slf_cap_discharging_step, SLF_CAP_VOLTAGE_MIN);
			}
		}

		if (m_slf_cap_voltage >= SLF_CAP_VOLTAGE_MAX)
		{
			m_slf_out_ff = 1;
		}
		else if (m_slf_cap_voltage <= SLF_CAP_VOLTAGE_MIN)
		{
			m_slf_out_ff = 0;
		}


		/* update the VCO (voltage controlled oscillator) */
		if (m_vco_mode)
		{
			/* VCO is controlled by SLF */
			vco_cap_voltage_max =  m_slf_cap_voltage + VCO_TO_SLF_VOLTAGE_DIFF;
		}
		else
		{
			/* VCO is controlled by external voltage */

			vco_cap_voltage_max =  VCO_TO_SLF_VOLTAGE_DIFF;
		}

		if (!m_vco_cap_voltage_ext)
		{
			if (!m_vco_out_ff)
			{
				/* charging */
				m_vco_cap_voltage = min(m_vco_cap_voltage + vco_cap_charging_step, vco_cap_voltage_max);

			}
			else
			{
				/* discharging */
				m_vco_cap_voltage = max(m_vco_cap_voltage - vco_cap_discharging_step, VCO_CAP_VOLTAGE_MIN);

			}
		}

		if (m_vco_cap_voltage >= vco_cap_voltage_max)
		{

			if (!m_vco_out_ff)
			{
				/* positive edge */
				m_vco_alt_pos_edge_ff = !m_vco_alt_pos_edge_ff;



			}

			m_vco_out_ff = 1;
		}
		else if (m_vco_cap_voltage <= VCO_CAP_VOLTAGE_MIN)
		{
			m_vco_out_ff = 0;


		}


		/* update the noise generator */
		while (!m_noise_clock_ext && (m_noise_gen_count <= noise_gen_freq))
		{
			m_noise_gen_count = m_noise_gen_count + m_our_sample_rate;

			m_real_noise_bit_ff = generate_next_real_noise_bit();
		}



		m_noise_gen_count = m_noise_gen_count - noise_gen_freq;
		if(m_noise_gen_count >=1000000) m_noise_gen_count=noise_gen_freq+m_our_sample_rate+1;
		m_noise_filter_cap_voltage_ext=0;

		//printf("%u:%u:%u\n",m_noise_gen_count,noise_gen_freq,m_our_sample_rate);

		/* update the noise filter */
		if (!m_noise_filter_cap_voltage_ext)
		{
			/* internal */
			if (m_real_noise_bit_ff)
			{
				/* charging */
				m_noise_filter_cap_voltage = min(m_noise_filter_cap_voltage + noise_filter_cap_charging_step, NOISE_CAP_VOLTAGE_MAX);
			}
			else
			{
				/* discharging */
				m_noise_filter_cap_voltage = max(m_noise_filter_cap_voltage - noise_filter_cap_discharging_step, NOISE_CAP_VOLTAGE_MIN);
			}
		}


		/* check the thresholds */
		if (m_noise_filter_cap_voltage >= NOISE_CAP_HIGH_THRESHOLD)
		{
			m_filtered_noise_bit_ff = 0;
		}
		else if (m_noise_filter_cap_voltage <= NOISE_CAP_LOW_THRESHOLD)
		{
			m_filtered_noise_bit_ff = 1;
		}


		/* based on the envelope mode figure out the attack/decay phase we are in */
		switch (m_envelope_mode)
		{
		case 0:     /* VCO */
			attack_decay_cap_charging = m_vco_out_ff;
			break;

		case 1:     /* one-shot */
			attack_decay_cap_charging = m_one_shot_running_ff;
			break;

		case 2:
		default:    /* mixer only */
			attack_decay_cap_charging = 1;  /* never a decay phase */
			break;

		case 3:     /* VCO with alternating polarity */
			attack_decay_cap_charging = m_vco_out_ff && m_vco_alt_pos_edge_ff;
			break;


		}


		/* update a/d cap voltage */
		if (!m_attack_decay_cap_voltage_ext)
		{
			if (attack_decay_cap_charging)
			{
				if (attack_decay_cap_charging_step > 0)
				{
					m_attack_decay_cap_voltage = min(m_attack_decay_cap_voltage + attack_decay_cap_charging_step, AD_CAP_VOLTAGE_MAX);
				}
				else
				{
					/* no attack, voltage to max instantly */
					m_attack_decay_cap_voltage = AD_CAP_VOLTAGE_MAX;
				}
			}
			else
			{
				/* discharging */
				if (attack_decay_cap_discharging_step > 0)
				{
					m_attack_decay_cap_voltage = max(m_attack_decay_cap_voltage - attack_decay_cap_discharging_step, AD_CAP_VOLTAGE_MIN);
				}
				else
				{
					/* no decay, voltage to min instantly */
					m_attack_decay_cap_voltage = AD_CAP_VOLTAGE_MIN;
				}
			}
		}


		/* mix the output, if enabled, or not saturated by the VCO */

		if (!m_enable && (m_vco_cap_voltage <= VCO_CAP_VOLTAGE_MAX))
		{
			uint32_t out;

			/* enabled */
			switch (m_mixer_mode)
			{
			case 1:     /* VCO */
				out = m_vco_out_ff;
				break;

			case 2:     /* SLF */
				out = m_slf_out_ff;
				break;

			case 4:     /* noise */
				out = m_filtered_noise_bit_ff;

				break;

			case 5:     /* VCO and noise */
				out = m_vco_out_ff & m_filtered_noise_bit_ff;
				break;

			case 6:     /* SLF and noise */
				out = m_slf_out_ff & m_filtered_noise_bit_ff;
				break;

			case 7:     /* VCO, SLF and noise */
				out = m_vco_out_ff & m_slf_out_ff & m_filtered_noise_bit_ff;
				break;

			case 3:     /* VCO and SLF */
				out = m_vco_out_ff & m_slf_out_ff;
				break;

			case 0:     /* inhibit */
						out = 0;
						break;

			default:
				out = 0;
				break;
			}

			/* determine the OUT voltage from the attack/delay cap voltage and clip it */
			if (out)

			{
				voltage_out = OUT_CENTER_LEVEL_VOLTAGE + center_to_peak_voltage_out * out_pos_gain[(int)(m_attack_decay_cap_voltage * 10)],
				voltage_out = min(voltage_out, OUT_HIGH_CLIP_THRESHOLD);

			}
			else
			{
				voltage_out = OUT_CENTER_LEVEL_VOLTAGE + center_to_peak_voltage_out * out_neg_gain[(int)(m_attack_decay_cap_voltage * 10)],
				voltage_out = max(voltage_out, OUT_LOW_CLIP_THRESHOLD);
			}
		}
		else
		{
			/* disabled */
			voltage_out = OUT_CENTER_LEVEL_VOLTAGE;


		}


		/* convert it to a signed 16-bit sample,
		   -32767 = OUT_LOW_CLIP_THRESHOLD
		        0 = OUT_CENTER_LEVEL_VOLTAGE
		    32767 = 2 * OUT_CENTER_LEVEL_VOLTAGE + OUT_LOW_CLIP_THRESHOLD

		              / Vout - Vmin    \
		    sample = |  ----------- - 1 | * 32767
		              \ Vcen - Vmin    /
		 */


	}

	double sample=(((voltage_out - OUT_LOW_CLIP_THRESHOLD) / (OUT_CENTER_LEVEL_VOLTAGE - OUT_LOW_CLIP_THRESHOLD)) - 1) * 32767;

	Rsamples sam;
	sam.s1=sample;
	sam.s2=m_vco_cap_voltage;

	return(sam);

}
