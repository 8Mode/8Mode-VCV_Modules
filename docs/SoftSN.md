# SoftSN Machine

<img align="left" width="300" height="250" src=sn76477-block-diagram.jpg>

## Theory of Operation

The SN76477 is an analog/digital hybrid sound chip released in 1978. SNSoft provides controls and simulates the analog circuitry including 4 RC oscillators, voltage level inputs and digital inputs around a SN76477 emulator. This provides a fairly accurate reproduction of the real thing in a way that could be built in electronics from 1978 - including the quirks and limitations.

[SN76477 Datasheet](sn76477.pdf)

<br/>

---
<img align="left" width="300" height="424" src=panel.png>

## Input Controls 

 * Input Panel (on left) - Provides v/oct, trig or -5/+5 CV inputs to modulate controls.
 * VCO - Adjust frequency of the VCO Oscillator
 * Attack/Decay - Adjusts attack and decay phase of 1-Shot triggers. Also provides effects in VCO/Alt mode.
 * SLF - Adjust frequency of Super Low Frequency VCO
 * Noise - Adjust noise frequency and filter
 * One Shot - When envelope mode is 1 Shot, sound is triggered by button/Trig input. Length sets sound duration.
 * Duty - Adjusts the duty cycle of the VCO. This can be used to transistion the TRI output to a SAW wave
 * Mixers - Selects which oscillators are multiplexed for output
 * Envelope - Selects how the mixer performs multiplexing
 * OSC - Selects the source for the VCO. SLF modulates the VCO with the SLF.

<br/>

## Outputs

* TRI output - Provides a TRI wave output which is tapped off the RC capacitor of the VCO. This is a constant output that cannot be controlled by the 1-Shot. It's very erratic based off the SLF frequency. I've added an AGC to the output to adjust the gain.
* SQR output - The multiplexed output of the 3 Oscillators

---
## Contributing

I welcome Issues and Pull Requests to this repository if you have suggestions for improvement.
If you enjoy those modules you can support the development by making a donation. Here's the link: [DONATE](https://paypal.me/8ModeLLC)
