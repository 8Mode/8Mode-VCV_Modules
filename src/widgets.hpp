#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;

struct Button18 : SvgSwitch {
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

struct StatefulButton : ParamWidget {
	std::vector<std::shared_ptr<Svg>> _frames;
	SvgWidget* _svgWidget; // deleted elsewhere.
	CircularShadow* shadow = NULL;

	StatefulButton(const char* offSvgPath, const char* onSvgPath);
	void onDragStart(const event::DragStart& e) override;
	void onDragEnd(const event::DragEnd& e) override;
};

struct StatefulButton18 : StatefulButton {
	StatefulButton18();
};

struct SliderSwitch : SvgSwitch {
	CircularShadow* shadow = NULL;
	SliderSwitch();
};

struct EModeSlider : SliderSwitch {
	EModeSlider();
};
