#include "rack.hpp"

using namespace rack;

extern Plugin *plugin;

struct Button18 : SVGSwitch, MomentarySwitch {
	Button18();
};

struct BGKnob : RoundKnob {
	BGKnob(const char* svg, int dim);
};
struct Knob16 : BGKnob {
	Knob16();
};

struct Snap_8M_Knob : RoundKnob {
	Snap_8M_Knob();
};



struct StatefulButton : ParamWidget, FramebufferWidget {
	std::vector<std::shared_ptr<SVG>> _frames;
	SVGWidget* _svgWidget; // deleted elsewhere.
	CircularShadow* shadow = NULL;

	StatefulButton(const char* offSVGPath, const char* onSVGPath);
	void step() override;
	void onDragStart(EventDragStart& e) override;
	void onDragEnd(EventDragEnd& e) override;
};

struct StatefulButton18 : StatefulButton {
	StatefulButton18();
};

struct SliderSwitch : SVGSwitch, ToggleSwitch {
	CircularShadow* shadow = NULL;
	SliderSwitch();
};

struct EModeSlider : SliderSwitch {
	EModeSlider();
};
