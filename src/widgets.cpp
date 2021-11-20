#include "widgets.hpp"


BGKnob::BGKnob(const char* svg, int dim) {
	setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, svg)));
	box.size = Vec(dim, dim);
	shadow->blurRadius = 1.0;
	shadow->box.pos = Vec(0.0, 2.0);
}

Knob16::Knob16() : BGKnob("res/8Mode_Knob1.svg", 46) {
	shadow->box.pos = Vec(0.0, 0);
}

Snap_8M_Knob::Snap_8M_Knob() {
setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/8Mode_Knob1.svg")));
	shadow->box.pos = Vec(0.0, 0);
	snap = true;
	minAngle = 0.3*M_PI;
	maxAngle = 0.725*M_PI;
}

Button18::Button18() {
	addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/button_18px_0.svg")));
	addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/button_18px_1.svg")));
	box.size = Vec(18, 18);
}

StatefulButton::StatefulButton(const char* offSvgPath, const char* onSvgPath) {
	shadow = new CircularShadow();
	addChild(shadow);

	_svgWidget = new SvgWidget();
	addChild(_svgWidget);

	auto svg = APP->window->loadSvg(asset::plugin(pluginInstance, offSvgPath));
	_frames.push_back(svg);
	_frames.push_back(APP->window->loadSvg(asset::plugin(pluginInstance, onSvgPath)));

	_svgWidget->setSvg(svg);
	box.size = _svgWidget->box.size;
	shadow->box.size = _svgWidget->box.size;
	shadow->blurRadius = 1.0;
	shadow->box.pos = Vec(0.0, 1.0);
}

void StatefulButton::onDragStart(const event::DragStart& e) {

    ParamQuantity* paramQuantity = getParamQuantity();

	_svgWidget->setSvg(_frames[1]);
	if (paramQuantity) {
    _svgWidget->setSvg(_frames[1]);
    if (paramQuantity->getValue() >= paramQuantity->getMaxValue()) {
      paramQuantity->setValue(paramQuantity->getMinValue());
    }
    else {
      paramQuantity->setValue(paramQuantity->getValue() + 1.0);
    }
  }
}

void StatefulButton::onDragEnd(const event::DragEnd& e) {
	_svgWidget->setSvg(_frames[0]);
}

StatefulButton18::StatefulButton18() : StatefulButton("res/button_18px_0.svg", "res/button_18px_1.svg") {
}

SliderSwitch::SliderSwitch() {
	shadow = new CircularShadow();
	addChild(shadow);
	shadow->box.size = Vec();
}

EModeSlider::EModeSlider() {
	addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/8Mode_ss_0.svg")));
	addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/8Mode_ss_1.svg")));
}
