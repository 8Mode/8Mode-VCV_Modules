#include "widgets.hpp"


BGKnob::BGKnob(const char* svg, int dim) {
	setSVG(SVG::load(assetPlugin(plugin, svg)));
	box.size = Vec(dim, dim);
	shadow->blurRadius = 1.0;
	// k->shadow->opacity = 0.15;
	shadow->box.pos = Vec(0.0, 2.0);
}


Knob16::Knob16() : BGKnob("res/8Mode_Knob1.svg", 46) {
	shadow->box.pos = Vec(0.0, 0);
}


Snap_8M_Knob::Snap_8M_Knob() {
setSVG(SVG::load(assetPlugin(plugin,"res/8Mode_Knob1.svg")));
	shadow->box.pos = Vec(0.0, 0);
	snap = true;
	minAngle = 0.3*M_PI;
	maxAngle = 0.725*M_PI;

}



Button18::Button18() {
	addFrame(SVG::load(assetPlugin(plugin, "res/button_18px_0.svg")));
	addFrame(SVG::load(assetPlugin(plugin, "res/button_18px_1.svg")));
	box.size = Vec(18, 18);
}

StatefulButton::StatefulButton(const char* offSVGPath, const char* onSVGPath) {
	shadow = new CircularShadow();
	addChild(shadow);

	_svgWidget = new SVGWidget();
	addChild(_svgWidget);

	auto svg = SVG::load(assetPlugin(plugin, offSVGPath));
	_frames.push_back(svg);
	_frames.push_back(SVG::load(assetPlugin(plugin, onSVGPath)));

	_svgWidget->setSVG(svg);
	box.size = _svgWidget->box.size;
	shadow->box.size = _svgWidget->box.size;
	shadow->blurRadius = 1.0;
	shadow->box.pos = Vec(0.0, 1.0);
}

void StatefulButton::step() {
	FramebufferWidget::step();
}

void StatefulButton::onDragStart(EventDragStart& e) {
	_svgWidget->setSVG(_frames[1]);
	dirty = true;

	if (value >= maxValue) {
		setValue(minValue);
	}
	else {
		setValue(value + 1.0);
	}
}

void StatefulButton::onDragEnd(EventDragEnd& e) {
	_svgWidget->setSVG(_frames[0]);
	dirty = true;
}

StatefulButton18::StatefulButton18() : StatefulButton("res/button_18px_0.svg", "res/button_18px_1.svg") {
}

SliderSwitch::SliderSwitch() {
	shadow = new CircularShadow();
	addChild(shadow);
	shadow->box.size = Vec();
}

EModeSlider::EModeSlider() {
	addFrame(SVG::load(assetPlugin(plugin, "res/8Mode_ss_0.svg")));
	addFrame(SVG::load(assetPlugin(plugin, "res/8Mode_ss_1.svg")));
	//shadow->box.size = Vec(14.0, 24.0);
	//shadow->blurRadius = 1.0;
	//shadow->box.pos = Vec(0.0, 7.0);
}





