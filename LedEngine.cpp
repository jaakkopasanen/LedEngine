#include "Arduino.h"
#include "LedEngine.h"

LedEngine::LedEngine(const uint8_t redPin, const uint8_t greenPin, const uint8_t bluePin, const uint8_t warmPin, const uint8_t coldPin, uint16_t pwmRange) {

	// Copy parameters
	redPin_ = redPin;
	greenPin_ = greenPin;
	bluePin_ = bluePin;
	warmPin_ = warmPin;
	coldPin_ = coldPin;
	pwmRange_ = pwmRange;

	// Set LED pins as output and set them off
	analogWriteRange(pwmRange_);
	pinMode(redPin_, OUTPUT); analogWrite(redPin_, 0);
	pinMode(greenPin_, OUTPUT); analogWrite(greenPin_, 0);
	pinMode(bluePin_, OUTPUT); analogWrite(bluePin_, 0);
	pinMode(warmPin_, OUTPUT); analogWrite(warmPin_, 0);
	pinMode(coldPin_, OUTPUT); analogWrite(coldPin_, 0);

	// Set default color and set light off
	setOnOff(false);
	setColorTemperature(50, 1900);
}

bool LedEngine::getOnOff() {
	return onOff_;
}

void LedEngine::setOnOff(const bool onOff) {
	onOff_ = onOff;
	if (onOff_) {
		setRaw(getRaw());
	}
	else {
		analogWrite(redPin_, 0);
		analogWrite(greenPin_, 0);
		analogWrite(bluePin_, 0);
	}
}

RGB LedEngine::getRaw() {
	return raw_;
}

void LedEngine::setRaw(const RGB raw) {
	// Copy
	raw_.R = raw.R;
	raw_.G = raw.G;
	raw_.B = raw.B;

	// Limit values in the range 0..1
	if (raw_.R < 0) raw_.R = 0.0;
	if (raw_.R > 1) raw_.R = 1.0;
	if (raw_.G < 0) raw_.G = 0.0;
	if (raw_.G > 1) raw_.G = 1.0;
	if (raw_.B < 0) raw_.B = 0.0;
	if (raw_.B > 1) raw_.B = 1.0;

	// Convert input floats to integers in the pwm range
	int pwmR = static_cast<int>(raw_.R * pwmRange_ + 0.5);
	int pwmG = static_cast<int>(raw_.G * pwmRange_ + 0.5);
	int pwmB = static_cast<int>(raw_.B * pwmRange_ + 0.5);

	// Write PWMs
	if (onOff_) {
		analogWrite(redPin_, pwmR);
		analogWrite(greenPin_, pwmG);
		analogWrite(bluePin_, pwmB);
	}

	// Save values
	raw_.R = static_cast<float>(pwmR) / pwmRange_;
	raw_.G = static_cast<float>(pwmG) / pwmRange_;
	raw_.B = static_cast<float>(pwmB) / pwmRange_;

	// Cannot be sure that current color is result of higher level color setter
	// unset luv_ and T_, respective setters will save the values afterwards
	luv_.L = -1.0;
	luv_.u = -1.0;
	luv_.v = -1.0;
	T_ = -1;
}

Luv LedEngine::getCie1976Ucs() {
	return luv_;
}

void LedEngine::setCie1976Ucs(const Luv target) {

	// Save values
	luv_.L = target.L;
	luv_.u = target.u;
	luv_.v = target.v;

	// Limit lightness to zero from below
	if (luv_.L < 0) luv_.L = 0;

	// Convert to raw PWM values
	RGB raw;

	// Coefficients
	raw.R = findCoefficient_(target, redUv_, greenUv_, blueUv_, redToGreenFit_, greenToBlueFit_);
	raw.G = findCoefficient_(target, greenUv_, blueUv_, redUv_, greenToBlueFit_, blueToRedFit_);
	raw.B = findCoefficient_(target, blueUv_, redUv_, greenUv_, blueToRedFit_, redToGreenFit_);

	// Luma produced by the current raw values
	float Y = (raw.R * redLum_ + raw.G * greenLum_ + raw.B * blueLum_) / maxLum_;

	// Luma level needed for requested lightness
	float Y_target = (target.L + 16) / 116;
	Y_target = Y_target * Y_target * Y_target; // Y_target^3

	// Luma factor
	float C = Y_target / Y;

	// Scale raw values to produce target luma
	raw.R = raw.R * C;
	raw.G = raw.G * C;
	raw.B = raw.B * C;

	// Find max scaled raw value
	float maxRaw = raw.R;
	if (raw.G > maxRaw) maxRaw = raw.G;
	if (raw.B > maxRaw) maxRaw = raw.B;

	// Nothing can be more than at max power, limit coefficients to 1
	if (maxRaw > 1) {
		raw.R = raw.R * (1.0 / maxRaw);
		raw.G = raw.G * (1.0 / maxRaw);
		raw.B = raw.B * (1.0 / maxRaw);
	}

	// Write PWMs
	setRaw(raw);
}

uint16_t LedEngine::getColorTemperature() {
	return T_;
}

void LedEngine::setColorTemperature(const float L, const uint16_t T) {

	// These cryptic looking formulas are a result of polynomial least squares fit of
	// CIE1976UCS coordinates vs color temperature

	// Fit variable has been transformed to z-score in order to avoid floating point precision problems
	double x = (T - 5500.0) / 2599.0;
	double x2 = x*x;
	double x3 = x*x*x;
	double x4 = x*x*x*x;
	double u = (-0.0001747*x3 + 0.1833*x2 + 0.872*x + 1.227) / (x2 + 4.813*x + 5.933);
	double v = (0.000311*x4 + 0.0009124*x3 + 0.3856*x2 + 1.873*x + 2.619) / (x2 + 4.323*x + 5.485);

	// Construct CIE 1976 UCS values from internal lightness and newly calculated u', v' coordinates
	Luv luv = { luv_.L, u, v };

	// Lightness is given and valid
	if (L > 0) luv.L = L;

	setCie1976Ucs(luv);

	// Save color temperature
	T_ = T;
}

void LedEngine::calibrate(const Luv redUv, const Luv greenUv, const Luv blueUv, const float redLum,
	const float greenLum, const float blueLum, const float redToGreenFit[3], const float greenToBlueFit[3],
	const float blueToRedFit[3]) {

	// CIE 1976 UCS coordinates
	redUv_.u = redUv.u;
	redUv_.v = redUv.v;
	greenUv_.u = greenUv.u;
	greenUv_.v = greenUv.v;
	blueUv_.u = blueUv.u;
	blueUv_.v = blueUv.v;

	// Luminous fluxes
	redLum_ = redLum;
	greenLum_ = greenLum;
	blueLum_ = blueLum;

	// Fit functions
	for (uint8_t i = 0; i < 3; ++i) {
		redToGreenFit_[i] = redToGreenFit[i];
		greenToBlueFit_[i] = greenToBlueFit[i];
		blueToRedFit_[i] = blueToRedFit[i];
	}

	// Update current color
	if (T_ >= 1000) {
		setColorTemperature(luv_.L, T_);
	}
	else if (luv_.L >= 0) {
		setCie1976Ucs(luv_);
	}
}

Luv LedEngine::getRedUv() { return redUv_; }

Luv LedEngine::getGreenUv() { return greenUv_; }

Luv LedEngine::getBlueUv() { return blueUv_; }

float LedEngine::getRedLum() { return redLum_; }

float LedEngine::getGreenLum() { return greenLum_; }

float LedEngine::getBlueLum() { return blueLum_; }

float * LedEngine::getRedToGreenFit() { return redToGreenFit_; }

float * LedEngine::getGreenToBlueFit() { return greenToBlueFit_; }

float * LedEngine::getBlueToRedFit() { return blueToRedFit_; }

float LedEngine::findCoefficient_(const Luv PT, const Luv P0, const Luv P1, const Luv P2, const float rightHandFit[3], const float leftHandFit[3]) {

	double PTu = PT.u;
	double PTv = PT.v;
	double P0u = P0.u;
	double P0v = P0.v;
	double P1u = P1.u;
	double P1v = P1.v;
	double P2u = P2.u;
	double P2v = P2.v;
	double Rp1 = rightHandFit[0];
	double Rp2 = rightHandFit[1];
	double Rq1 = rightHandFit[2];
	double Lp1 = leftHandFit[0];
	double Lp2 = leftHandFit[1];
	double Lq1 = leftHandFit[2];

	double dR;
	if (Lp1 < 0) {
		dR = (sqrt((P0u*P0u)*(P1v*P1v) + (P1u*P1u)*(P0v*P0v) + (P0u*P0u)*(PTv*PTv) + (P0v*P0v)*(PTu*PTu) + (P1u*P1u)*(PTv*PTv) + (P1v*P1v)*(PTu*PTu) - Lp1*(P0u*P0u)*(P1v*P1v)*2.0 - Lp1*(P1u*P1u)*(P0v*P0v)*2.0 - Lp2*(P0u*P0u)*(P1v*P1v)*2.0 - Lp2*(P1u*P1u)*(P0v*P0v)*2.0 + Lq1*(P0u*P0u)*(P1v*P1v)*2.0 + Lq1*(P1u*P1u)*(P0v*P0v)*2.0 - Lp1*(P0u*P0u)*(PTv*PTv)*2.0 - Lp1*(P0v*P0v)*(PTu*PTu)*2.0 - Lp2*(P0u*P0u)*(PTv*PTv)*4.0 - Lp2*(P0v*P0v)*(PTu*PTu)*4.0 + Lq1*(P0u*P0u)*(PTv*PTv)*2.0 + Lq1*(P0v*P0v)*(PTu*PTu)*2.0 + Lq1*(P1u*P1u)*(PTv*PTv)*2.0 + Lq1*(P1v*P1v)*(PTu*PTu)*2.0 + (Lp1*Lp1)*(P0u*P0u)*(P1v*P1v) + (Lp1*Lp1)*(P1u*P1u)*(P0v*P0v) + (Lp2*Lp2)*(P0u*P0u)*(P1v*P1v) + (Lp2*Lp2)*(P1u*P1u)*(P0v*P0v) + (Lp1*Lp1)*(P1u*P1u)*(P2v*P2v) + (Lp1*Lp1)*(P2u*P2u)*(P1v*P1v) + (Lp2*Lp2)*(P0u*P0u)*(P2v*P2v) + (Lp2*Lp2)*(P2u*P2u)*(P0v*P0v) + (Lp2*Lp2)*(P1u*P1u)*(P2v*P2v) + (Lp2*Lp2)*(P2u*P2u)*(P1v*P1v) + (Lq1*Lq1)*(P0u*P0u)*(P1v*P1v) + (Lq1*Lq1)*(P1u*P1u)*(P0v*P0v) + (Lp1*Lp1)*(P0u*P0u)*(PTv*PTv) + (Lp1*Lp1)*(P0v*P0v)*(PTu*PTu) + (Lp1*Lp1)*(P2u*P2u)*(PTv*PTv) + (Lp1*Lp1)*(P2v*P2v)*(PTu*PTu) + (Lq1*Lq1)*(P0u*P0u)*(PTv*PTv) + (Lq1*Lq1)*(P0v*P0v)*(PTu*PTu) + (Lq1*Lq1)*(P1u*P1u)*(PTv*PTv) + (Lq1*Lq1)*(P1v*P1v)*(PTu*PTu) - P0u*P1u*(PTv*PTv)*2.0 - P0u*(P1v*P1v)*PTu*2.0 - P1u*(P0v*P0v)*PTu*2.0 - P0v*P1v*(PTu*PTu)*2.0 - (P0u*P0u)*P1v*PTv*2.0 - (P1u*P1u)*P0v*PTv*2.0 + Lp1*Lp2*(P0u*P0u)*(P1v*P1v)*2.0 + Lp1*Lp2*(P1u*P1u)*(P0v*P0v)*2.0 + Lp1*Lp2*(P1u*P1u)*(P2v*P2v)*2.0 + Lp1*Lp2*(P2u*P2u)*(P1v*P1v)*2.0 - Lp1*Lq1*(P0u*P0u)*(P1v*P1v)*2.0 - Lp1*Lq1*(P1u*P1u)*(P0v*P0v)*2.0 - Lp2*Lq1*(P0u*P0u)*(P1v*P1v)*2.0 - Lp2*Lq1*(P1u*P1u)*(P0v*P0v)*2.0 + Lp1*Lq1*(P0u*P0u)*(PTv*PTv)*2.0 + Lp1*Lq1*(P0v*P0v)*(PTu*PTu)*2.0 - (Lp1*Lp1)*P0u*P2u*(P1v*P1v)*2.0 - (Lp2*Lp2)*P0u*P1u*(P2v*P2v)*2.0 - (Lp2*Lp2)*P0u*P2u*(P1v*P1v)*2.0 - (Lp2*Lp2)*P1u*P2u*(P0v*P0v)*2.0 - (Lp1*Lp1)*(P1u*P1u)*P0v*P2v*2.0 - (Lp2*Lp2)*(P0u*P0u)*P1v*P2v*2.0 - (Lp2*Lp2)*(P1u*P1u)*P0v*P2v*2.0 - (Lp2*Lp2)*(P2u*P2u)*P0v*P1v*2.0 - (Lp1*Lp1)*P1u*(P0v*P0v)*PTu*2.0 - (Lp1*Lp1)*P0u*P2u*(PTv*PTv)*2.0 - (Lp1*Lp1)*P1u*(P2v*P2v)*PTu*2.0 - (Lp1*Lp1)*(P0u*P0u)*P1v*PTv*2.0 - (Lp1*Lp1)*P0v*P2v*(PTu*PTu)*2.0 - (Lp1*Lp1)*(P2u*P2u)*P1v*PTv*2.0 - (Lq1*Lq1)*P0u*P1u*(PTv*PTv)*2.0 - (Lq1*Lq1)*P0u*(P1v*P1v)*PTu*2.0 - (Lq1*Lq1)*P1u*(P0v*P0v)*PTu*2.0 - (Lq1*Lq1)*P0v*P1v*(PTu*PTu)*2.0 - (Lq1*Lq1)*(P0u*P0u)*P1v*PTv*2.0 - (Lq1*Lq1)*(P1u*P1u)*P0v*PTv*2.0 - P0u*P1u*P0v*P1v*2.0 + P0u*P1u*P0v*PTv*2.0 + P0u*P0v*P1v*PTu*2.0 + P0u*P1u*P1v*PTv*2.0 + P1u*P0v*P1v*PTu*2.0 - P0u*P0v*PTu*PTv*2.0 + P0u*P1v*PTu*PTv*2.0 + P1u*P0v*PTu*PTv*2.0 - P1u*P1v*PTu*PTv*2.0 + Lp1*P0u*P2u*(P1v*P1v)*2.0 + Lp2*P0u*P2u*(P1v*P1v)*2.0 - Lp2*P1u*P2u*(P0v*P0v)*2.0 + Lp1*(P1u*P1u)*P0v*P2v*2.0 - Lp2*(P0u*P0u)*P1v*P2v*2.0 + Lp2*(P1u*P1u)*P0v*P2v*2.0 + Lp1*P0u*P1u*(PTv*PTv)*2.0 + Lp1*P0u*(P1v*P1v)*PTu*2.0 + Lp1*P1u*(P0v*P0v)*PTu*4.0 + Lp1*P0u*P2u*(PTv*PTv)*2.0 + Lp2*P0u*P1u*(PTv*PTv)*4.0 + Lp2*P0u*(P1v*P1v)*PTu*2.0 + Lp2*P1u*(P0v*P0v)*PTu*6.0 - Lp1*P1u*P2u*(PTv*PTv)*2.0 - Lp1*P2u*(P1v*P1v)*PTu*2.0 + Lp2*P0u*P2u*(PTv*PTv)*4.0 + Lp2*P2u*(P0v*P0v)*PTu*2.0 - Lp2*P1u*P2u*(PTv*PTv)*4.0 - Lp2*P2u*(P1v*P1v)*PTu*2.0 + Lp1*P0v*P1v*(PTu*PTu)*2.0 + Lp1*(P0u*P0u)*P1v*PTv*4.0 + Lp1*(P1u*P1u)*P0v*PTv*2.0 + Lp1*P0v*P2v*(PTu*PTu)*2.0 + Lp2*P0v*P1v*(PTu*PTu)*4.0 + Lp2*(P0u*P0u)*P1v*PTv*6.0 + Lp2*(P1u*P1u)*P0v*PTv*2.0 - Lp1*P1v*P2v*(PTu*PTu)*2.0 - Lp1*(P1u*P1u)*P2v*PTv*2.0 + Lp2*P0v*P2v*(PTu*PTu)*4.0 + Lp2*(P0u*P0u)*P2v*PTv*2.0 - Lp2*P1v*P2v*(PTu*PTu)*4.0 - Lp2*(P1u*P1u)*P2v*PTv*2.0 - Lq1*P0u*P1u*(PTv*PTv)*4.0 - Lq1*P0u*(P1v*P1v)*PTu*4.0 - Lq1*P1u*(P0v*P0v)*PTu*4.0 - Lq1*P0v*P1v*(PTu*PTu)*4.0 - Lq1*(P0u*P0u)*P1v*PTv*4.0 - Lq1*(P1u*P1u)*P0v*PTv*4.0 - Lp1*Lp2*P0u*P1u*(P2v*P2v)*2.0 - Lp1*Lp2*P0u*P2u*(P1v*P1v)*4.0 - Lp1*Lp2*P1u*P2u*(P0v*P0v)*2.0 - Lp1*Lp2*(P0u*P0u)*P1v*P2v*2.0 - Lp1*Lp2*(P1u*P1u)*P0v*P2v*4.0 - Lp1*Lp2*(P2u*P2u)*P0v*P1v*2.0 + Lp1*Lq1*P0u*P2u*(P1v*P1v)*2.0 + Lp1*Lq1*P1u*P2u*(P0v*P0v)*4.0 + Lp2*Lq1*P0u*P2u*(P1v*P1v)*2.0 + Lp2*Lq1*P1u*P2u*(P0v*P0v)*2.0 + Lp1*Lq1*(P0u*P0u)*P1v*P2v*4.0 + Lp1*Lq1*(P1u*P1u)*P0v*P2v*2.0 + Lp2*Lq1*(P0u*P0u)*P1v*P2v*2.0 + Lp2*Lq1*(P1u*P1u)*P0v*P2v*2.0 - Lp1*Lp2*P1u*(P0v*P0v)*PTu*2.0 + Lp1*Lp2*P0u*(P2v*P2v)*PTu*2.0 + Lp1*Lp2*P2u*(P0v*P0v)*PTu*2.0 - Lp1*Lp2*P1u*(P2v*P2v)*PTu*2.0 - Lp1*Lp2*(P0u*P0u)*P1v*PTv*2.0 + Lp1*Lp2*(P0u*P0u)*P2v*PTv*2.0 + Lp1*Lp2*(P2u*P2u)*P0v*PTv*2.0 - Lp1*Lp2*(P2u*P2u)*P1v*PTv*2.0 - Lp1*Lq1*P0u*P1u*(PTv*PTv)*2.0 + Lp1*Lq1*P0u*(P1v*P1v)*PTu*2.0 - Lp1*Lq1*P0u*P2u*(PTv*PTv)*2.0 - Lp1*Lq1*P2u*(P0v*P0v)*PTu*4.0 + Lp2*Lq1*P0u*(P1v*P1v)*PTu*2.0 + Lp2*Lq1*P1u*(P0v*P0v)*PTu*2.0 + Lp1*Lq1*P1u*P2u*(PTv*PTv)*2.0 - Lp1*Lq1*P2u*(P1v*P1v)*PTu*2.0 - Lp2*Lq1*P2u*(P0v*P0v)*PTu*2.0 - Lp2*Lq1*P2u*(P1v*P1v)*PTu*2.0 - Lp1*Lq1*P0v*P1v*(PTu*PTu)*2.0 + Lp1*Lq1*(P1u*P1u)*P0v*PTv*2.0 - Lp1*Lq1*P0v*P2v*(PTu*PTu)*2.0 - Lp1*Lq1*(P0u*P0u)*P2v*PTv*4.0 + Lp2*Lq1*(P0u*P0u)*P1v*PTv*2.0 + Lp2*Lq1*(P1u*P1u)*P0v*PTv*2.0 + Lp1*Lq1*P1v*P2v*(PTu*PTu)*2.0 - Lp1*Lq1*(P1u*P1u)*P2v*PTv*2.0 - Lp2*Lq1*(P0u*P0u)*P2v*PTv*2.0 - Lp2*Lq1*(P1u*P1u)*P2v*PTv*2.0 - (Lp1*Lp1)*P0u*P1u*P0v*P1v*2.0 - (Lp2*Lp2)*P0u*P1u*P0v*P1v*2.0 + (Lp1*Lp1)*P0u*P1u*P1v*P2v*2.0 + (Lp1*Lp1)*P1u*P2u*P0v*P1v*2.0 + (Lp2*Lp2)*P0u*P1u*P0v*P2v*2.0 + (Lp2*Lp2)*P0u*P2u*P0v*P1v*2.0 + (Lp2*Lp2)*P0u*P1u*P1v*P2v*2.0 - (Lp2*Lp2)*P0u*P2u*P0v*P2v*2.0 + (Lp2*Lp2)*P1u*P2u*P0v*P1v*2.0 - (Lp1*Lp1)*P1u*P2u*P1v*P2v*2.0 + (Lp2*Lp2)*P0u*P2u*P1v*P2v*2.0 + (Lp2*Lp2)*P1u*P2u*P0v*P2v*2.0 - (Lp2*Lp2)*P1u*P2u*P1v*P2v*2.0 - (Lq1*Lq1)*P0u*P1u*P0v*P1v*2.0 + (Lp1*Lp1)*P0u*P1u*P0v*PTv*2.0 + (Lp1*Lp1)*P0u*P0v*P1v*PTu*2.0 - (Lp1*Lp1)*P0u*P1u*P2v*PTv*2.0 + (Lp1*Lp1)*P0u*P2u*P1v*PTv*4.0 - (Lp1*Lp1)*P0u*P1v*P2v*PTu*2.0 - (Lp1*Lp1)*P1u*P2u*P0v*PTv*2.0 + (Lp1*Lp1)*P1u*P0v*P2v*PTu*4.0 - (Lp1*Lp1)*P2u*P0v*P1v*PTu*2.0 + (Lp1*Lp1)*P1u*P2u*P2v*PTv*2.0 + (Lp1*Lp1)*P2u*P1v*P2v*PTu*2.0 + (Lq1*Lq1)*P0u*P1u*P0v*PTv*2.0 + (Lq1*Lq1)*P0u*P0v*P1v*PTu*2.0 + (Lq1*Lq1)*P0u*P1u*P1v*PTv*2.0 + (Lq1*Lq1)*P1u*P0v*P1v*PTu*2.0 - (Lp1*Lp1)*P0u*P0v*PTu*PTv*2.0 + (Lp1*Lp1)*P0u*P2v*PTu*PTv*2.0 + (Lp1*Lp1)*P2u*P0v*PTu*PTv*2.0 - (Lp1*Lp1)*P2u*P2v*PTu*PTv*2.0 - (Lq1*Lq1)*P0u*P0v*PTu*PTv*2.0 + (Lq1*Lq1)*P0u*P1v*PTu*PTv*2.0 + (Lq1*Lq1)*P1u*P0v*PTu*PTv*2.0 - (Lq1*Lq1)*P1u*P1v*PTu*PTv*2.0 + Lp1*P0u*P1u*P0v*P1v*4.0 + Lp2*P0u*P1u*P0v*P1v*4.0 - Lp1*P0u*P1u*P1v*P2v*2.0 - Lp1*P1u*P2u*P0v*P1v*2.0 + Lp2*P0u*P1u*P0v*P2v*2.0 + Lp2*P0u*P2u*P0v*P1v*2.0 - Lp2*P0u*P1u*P1v*P2v*2.0 - Lp2*P1u*P2u*P0v*P1v*2.0 - Lq1*P0u*P1u*P0v*P1v*4.0 - Lp1*P0u*P1u*P0v*PTv*4.0 - Lp1*P0u*P0v*P1v*PTu*4.0 - Lp1*P0u*P1u*P1v*PTv*2.0 - Lp1*P1u*P0v*P1v*PTu*2.0 - Lp2*P0u*P1u*P0v*PTv*6.0 - Lp2*P0u*P0v*P1v*PTu*6.0 + Lp1*P0u*P1u*P2v*PTv*2.0 - Lp1*P0u*P2u*P1v*PTv*4.0 + Lp1*P0u*P1v*P2v*PTu*2.0 + Lp1*P1u*P2u*P0v*PTv*2.0 - Lp1*P1u*P0v*P2v*PTu*4.0 + Lp1*P2u*P0v*P1v*PTu*2.0 - Lp2*P0u*P1u*P1v*PTv*2.0 - Lp2*P0u*P2u*P0v*PTv*2.0 - Lp2*P0u*P0v*P2v*PTu*2.0 - Lp2*P1u*P0v*P1v*PTu*2.0 + Lp1*P1u*P2u*P1v*PTv*2.0 + Lp1*P1u*P1v*P2v*PTu*2.0 - Lp2*P0u*P2u*P1v*PTv*6.0 + Lp2*P0u*P1v*P2v*PTu*6.0 + Lp2*P1u*P2u*P0v*PTv*6.0 - Lp2*P1u*P0v*P2v*PTu*6.0 + Lp2*P1u*P2u*P1v*PTv*2.0 + Lp2*P1u*P1v*P2v*PTu*2.0 + Lq1*P0u*P1u*P0v*PTv*4.0 + Lq1*P0u*P0v*P1v*PTu*4.0 + Lq1*P0u*P1u*P1v*PTv*4.0 + Lq1*P1u*P0v*P1v*PTu*4.0 + Lp1*P0u*P0v*PTu*PTv*4.0 - Lp1*P0u*P1v*PTu*PTv*2.0 - Lp1*P1u*P0v*PTu*PTv*2.0 + Lp2*P0u*P0v*PTu*PTv*8.0 - Lp1*P0u*P2v*PTu*PTv*2.0 - Lp1*P2u*P0v*PTu*PTv*2.0 - Lp2*P0u*P1v*PTu*PTv*4.0 - Lp2*P1u*P0v*PTu*PTv*4.0 + Lp1*P1u*P2v*PTu*PTv*2.0 + Lp1*P2u*P1v*PTu*PTv*2.0 - Lp2*P0u*P2v*PTu*PTv*4.0 - Lp2*P2u*P0v*PTu*PTv*4.0 + Lp2*P1u*P2v*PTu*PTv*4.0 + Lp2*P2u*P1v*PTu*PTv*4.0 - Lq1*P0u*P0v*PTu*PTv*4.0 + Lq1*P0u*P1v*PTu*PTv*4.0 + Lq1*P1u*P0v*PTu*PTv*4.0 - Lq1*P1u*P1v*PTu*PTv*4.0 - Lp1*Lp2*P0u*P1u*P0v*P1v*4.0 + Lp1*Lp2*P0u*P1u*P0v*P2v*2.0 + Lp1*Lp2*P0u*P2u*P0v*P1v*2.0 + Lp1*Lp2*P0u*P1u*P1v*P2v*4.0 + Lp1*Lp2*P1u*P2u*P0v*P1v*4.0 + Lp1*Lp2*P0u*P2u*P1v*P2v*2.0 + Lp1*Lp2*P1u*P2u*P0v*P2v*2.0 - Lp1*Lp2*P1u*P2u*P1v*P2v*4.0 + Lp1*Lq1*P0u*P1u*P0v*P1v*4.0 - Lp1*Lq1*P0u*P1u*P0v*P2v*4.0 - Lp1*Lq1*P0u*P2u*P0v*P1v*4.0 + Lp2*Lq1*P0u*P1u*P0v*P1v*4.0 - Lp1*Lq1*P0u*P1u*P1v*P2v*2.0 - Lp1*Lq1*P1u*P2u*P0v*P1v*2.0 - Lp2*Lq1*P0u*P1u*P0v*P2v*2.0 - Lp2*Lq1*P0u*P2u*P0v*P1v*2.0 - Lp2*Lq1*P0u*P1u*P1v*P2v*2.0 - Lp2*Lq1*P1u*P2u*P0v*P1v*2.0 + Lp1*Lp2*P0u*P1u*P0v*PTv*2.0 + Lp1*Lp2*P0u*P0v*P1v*PTu*2.0 - Lp1*Lp2*P0u*P2u*P0v*PTv*2.0 - Lp1*Lp2*P0u*P0v*P2v*PTu*2.0 - Lp1*Lp2*P0u*P1u*P2v*PTv*2.0 + Lp1*Lp2*P0u*P2u*P1v*PTv*4.0 - Lp1*Lp2*P0u*P1v*P2v*PTu*2.0 - Lp1*Lp2*P1u*P2u*P0v*PTv*2.0 + Lp1*Lp2*P1u*P0v*P2v*PTu*4.0 - Lp1*Lp2*P2u*P0v*P1v*PTu*2.0 - Lp1*Lp2*P0u*P2u*P2v*PTv*2.0 - Lp1*Lp2*P2u*P0v*P2v*PTu*2.0 + Lp1*Lp2*P1u*P2u*P2v*PTv*2.0 + Lp1*Lp2*P2u*P1v*P2v*PTu*2.0 - Lp1*Lq1*P0u*P1u*P1v*PTv*2.0 + Lp1*Lq1*P0u*P2u*P0v*PTv*4.0 + Lp1*Lq1*P0u*P0v*P2v*PTu*4.0 - Lp1*Lq1*P1u*P0v*P1v*PTu*2.0 - Lp2*Lq1*P0u*P1u*P0v*PTv*2.0 - Lp2*Lq1*P0u*P0v*P1v*PTu*2.0 + Lp1*Lq1*P0u*P1u*P2v*PTv*6.0 - Lp1*Lq1*P0u*P1v*P2v*PTu*6.0 - Lp1*Lq1*P1u*P2u*P0v*PTv*6.0 + Lp1*Lq1*P2u*P0v*P1v*PTu*6.0 - Lp2*Lq1*P0u*P1u*P1v*PTv*2.0 + Lp2*Lq1*P0u*P2u*P0v*PTv*2.0 + Lp2*Lq1*P0u*P0v*P2v*PTu*2.0 - Lp2*Lq1*P1u*P0v*P1v*PTu*2.0 + Lp1*Lq1*P1u*P2u*P1v*PTv*2.0 + Lp1*Lq1*P1u*P1v*P2v*PTu*2.0 + Lp2*Lq1*P0u*P1u*P2v*PTv*4.0 - Lp2*Lq1*P0u*P2u*P1v*PTv*2.0 - Lp2*Lq1*P0u*P1v*P2v*PTu*2.0 - Lp2*Lq1*P1u*P2u*P0v*PTv*2.0 - Lp2*Lq1*P1u*P0v*P2v*PTu*2.0 + Lp2*Lq1*P2u*P0v*P1v*PTu*4.0 + Lp2*Lq1*P1u*P2u*P1v*PTv*2.0 + Lp2*Lq1*P1u*P1v*P2v*PTu*2.0 - Lp1*Lq1*P0u*P0v*PTu*PTv*4.0 + Lp1*Lq1*P0u*P1v*PTu*PTv*2.0 + Lp1*Lq1*P1u*P0v*PTu*PTv*2.0 + Lp1*Lq1*P0u*P2v*PTu*PTv*2.0 + Lp1*Lq1*P2u*P0v*PTu*PTv*2.0 - Lp1*Lq1*P1u*P2v*PTu*PTv*2.0 - Lp1*Lq1*P2u*P1v*PTu*PTv*2.0)*(1.0 / 2.0) + P0u*P1v*(1.0 / 2.0) - P1u*P0v*(1.0 / 2.0) - P0u*PTv*(1.0 / 2.0) + P0v*PTu*(1.0 / 2.0) + P1u*PTv*(1.0 / 2.0) - P1v*PTu*(1.0 / 2.0) - Lp1*P0u*P1v*(1.0 / 2.0) + Lp1*P1u*P0v*(1.0 / 2.0) + Lp1*P0u*P2v - Lp1*P2u*P0v - Lp2*P0u*P1v*(1.0 / 2.0) + Lp2*P1u*P0v*(1.0 / 2.0) - Lp1*P1u*P2v*(1.0 / 2.0) + Lp1*P2u*P1v*(1.0 / 2.0) + Lp2*P0u*P2v*(1.0 / 2.0) - Lp2*P2u*P0v*(1.0 / 2.0) - Lp2*P1u*P2v*(1.0 / 2.0) + Lp2*P2u*P1v*(1.0 / 2.0) + Lq1*P0u*P1v*(1.0 / 2.0) - Lq1*P1u*P0v*(1.0 / 2.0) - Lp1*P0u*PTv*(1.0 / 2.0) + Lp1*P0v*PTu*(1.0 / 2.0) + Lp1*P2u*PTv*(1.0 / 2.0) - Lp1*P2v*PTu*(1.0 / 2.0) - Lq1*P0u*PTv*(1.0 / 2.0) + Lq1*P0v*PTu*(1.0 / 2.0) + Lq1*P1u*PTv*(1.0 / 2.0) - Lq1*P1v*PTu*(1.0 / 2.0)) / (P0u*P1v - P1u*P0v - P0u*PTv + P0v*PTu + P1u*PTv - P1v*PTu - Lp1*P0u*P1v + Lp1*P1u*P0v + Lp1*P0u*P2v - Lp1*P2u*P0v - Lp1*P1u*P2v + Lp1*P2u*P1v);
	}
	else {
		dR = 1.0 - (sqrt(Lp1*Lp1*P0u*P0u*P2v*P2v - 2 * Lp1*Lp1*P0u*P0u*P2v*PTv + Lp1*Lp1*P0u*P0u*PTv*PTv - 2 * Lp1*Lp1*P0u*P2u*P0v*P2v + 2 * Lp1*Lp1*P0u*P2u*P0v*PTv + 2 * Lp1*Lp1*P0u*P2u*P2v*PTv - 2 * Lp1*Lp1*P0u*P2u*PTv*PTv + 2 * Lp1*Lp1*P0u*P0v*P2v*PTu - 2 * Lp1*Lp1*P0u*P0v*PTu*PTv - 2 * Lp1*Lp1*P0u*P2v*P2v*PTu + 2 * Lp1*Lp1*P0u*P2v*PTu*PTv + Lp1*Lp1*P2u*P2u*P0v*P0v - 2 * Lp1*Lp1*P2u*P2u*P0v*PTv + Lp1*Lp1*P2u*P2u*PTv*PTv - 2 * Lp1*Lp1*P2u*P0v*P0v*PTu + 2 * Lp1*Lp1*P2u*P0v*P2v*PTu + 2 * Lp1*Lp1*P2u*P0v*PTu*PTv - 2 * Lp1*Lp1*P2u*P2v*PTu*PTv + Lp1*Lp1*P0v*P0v*PTu*PTu - 2 * Lp1*Lp1*P0v*P2v*PTu*PTu + Lp1*Lp1*P2v*P2v*PTu*PTu - 2 * Lp1*Lp2*P0u*P0u*P1v*P2v + 2 * Lp1*Lp2*P0u*P0u*P1v*PTv + 2 * Lp1*Lp2*P0u*P0u*P2v*P2v - 2 * Lp1*Lp2*P0u*P0u*P2v*PTv + 2 * Lp1*Lp2*P0u*P1u*P0v*P2v - 2 * Lp1*Lp2*P0u*P1u*P0v*PTv - 2 * Lp1*Lp2*P0u*P1u*P2v*P2v + 2 * Lp1*Lp2*P0u*P1u*P2v*PTv + 2 * Lp1*Lp2*P0u*P2u*P0v*P1v - 4 * Lp1*Lp2*P0u*P2u*P0v*P2v + 2 * Lp1*Lp2*P0u*P2u*P0v*PTv + 2 * Lp1*Lp2*P0u*P2u*P1v*P2v - 4 * Lp1*Lp2*P0u*P2u*P1v*PTv + 2 * Lp1*Lp2*P0u*P2u*P2v*PTv - 2 * Lp1*Lp2*P0u*P0v*P1v*PTu + 2 * Lp1*Lp2*P0u*P0v*P2v*PTu + 2 * Lp1*Lp2*P0u*P1v*P2v*PTu - 2 * Lp1*Lp2*P0u*P2v*P2v*PTu - 2 * Lp1*Lp2*P1u*P2u*P0v*P0v + 2 * Lp1*Lp2*P1u*P2u*P0v*P2v + 2 * Lp1*Lp2*P1u*P2u*P0v*PTv - 2 * Lp1*Lp2*P1u*P2u*P2v*PTv + 2 * Lp1*Lp2*P1u*P0v*P0v*PTu - 4 * Lp1*Lp2*P1u*P0v*P2v*PTu + 2 * Lp1*Lp2*P1u*P2v*P2v*PTu + 2 * Lp1*Lp2*P2u*P2u*P0v*P0v - 2 * Lp1*Lp2*P2u*P2u*P0v*P1v - 2 * Lp1*Lp2*P2u*P2u*P0v*PTv + 2 * Lp1*Lp2*P2u*P2u*P1v*PTv - 2 * Lp1*Lp2*P2u*P0v*P0v*PTu + 2 * Lp1*Lp2*P2u*P0v*P1v*PTu + 2 * Lp1*Lp2*P2u*P0v*P2v*PTu - 2 * Lp1*Lp2*P2u*P1v*P2v*PTu - 2 * Lp1*Lq1*P0u*P0u*P1v*P2v + 2 * Lp1*Lq1*P0u*P0u*P1v*PTv + 2 * Lp1*Lq1*P0u*P0u*P2v*PTv - 2 * Lp1*Lq1*P0u*P0u*PTv*PTv + 2 * Lp1*Lq1*P0u*P1u*P0v*P2v - 2 * Lp1*Lq1*P0u*P1u*P0v*PTv - 2 * Lp1*Lq1*P0u*P1u*P2v*PTv + 2 * Lp1*Lq1*P0u*P1u*PTv*PTv + 2 * Lp1*Lq1*P0u*P2u*P0v*P1v - 2 * Lp1*Lq1*P0u*P2u*P0v*PTv - 2 * Lp1*Lq1*P0u*P2u*P1v*PTv + 2 * Lp1*Lq1*P0u*P2u*PTv*PTv - 2 * Lp1*Lq1*P0u*P0v*P1v*PTu - 2 * Lp1*Lq1*P0u*P0v*P2v*PTu + 4 * Lp1*Lq1*P0u*P0v*PTu*PTv + 4 * Lp1*Lq1*P0u*P1v*P2v*PTu - 2 * Lp1*Lq1*P0u*P1v*PTu*PTv - 2 * Lp1*Lq1*P0u*P2v*PTu*PTv - 2 * Lp1*Lq1*P1u*P2u*P0v*P0v + 4 * Lp1*Lq1*P1u*P2u*P0v*PTv - 2 * Lp1*Lq1*P1u*P2u*PTv*PTv + 2 * Lp1*Lq1*P1u*P0v*P0v*PTu - 2 * Lp1*Lq1*P1u*P0v*P2v*PTu - 2 * Lp1*Lq1*P1u*P0v*PTu*PTv + 2 * Lp1*Lq1*P1u*P2v*PTu*PTv + 2 * Lp1*Lq1*P2u*P0v*P0v*PTu - 2 * Lp1*Lq1*P2u*P0v*P1v*PTu - 2 * Lp1*Lq1*P2u*P0v*PTu*PTv + 2 * Lp1*Lq1*P2u*P1v*PTu*PTv - 2 * Lp1*Lq1*P0v*P0v*PTu*PTu + 2 * Lp1*Lq1*P0v*P1v*PTu*PTu + 2 * Lp1*Lq1*P0v*P2v*PTu*PTu - 2 * Lp1*Lq1*P1v*P2v*PTu*PTu + Lp2*Lp2*P0u*P0u*P1v*P1v - 2 * Lp2*Lp2*P0u*P0u*P1v*P2v + Lp2*Lp2*P0u*P0u*P2v*P2v - 2 * Lp2*Lp2*P0u*P1u*P0v*P1v + 2 * Lp2*Lp2*P0u*P1u*P0v*P2v + 2 * Lp2*Lp2*P0u*P1u*P1v*P2v - 2 * Lp2*Lp2*P0u*P1u*P2v*P2v + 2 * Lp2*Lp2*P0u*P2u*P0v*P1v - 2 * Lp2*Lp2*P0u*P2u*P0v*P2v - 2 * Lp2*Lp2*P0u*P2u*P1v*P1v + 2 * Lp2*Lp2*P0u*P2u*P1v*P2v + Lp2*Lp2*P1u*P1u*P0v*P0v - 2 * Lp2*Lp2*P1u*P1u*P0v*P2v + Lp2*Lp2*P1u*P1u*P2v*P2v - 2 * Lp2*Lp2*P1u*P2u*P0v*P0v + 2 * Lp2*Lp2*P1u*P2u*P0v*P1v + 2 * Lp2*Lp2*P1u*P2u*P0v*P2v - 2 * Lp2*Lp2*P1u*P2u*P1v*P2v + Lp2*Lp2*P2u*P2u*P0v*P0v - 2 * Lp2*Lp2*P2u*P2u*P0v*P1v + Lp2*Lp2*P2u*P2u*P1v*P1v - 2 * Lp2*Lq1*P0u*P0u*P1v*P1v + 2 * Lp2*Lq1*P0u*P0u*P1v*P2v + 2 * Lp2*Lq1*P0u*P0u*P1v*PTv - 2 * Lp2*Lq1*P0u*P0u*P2v*PTv + 4 * Lp2*Lq1*P0u*P1u*P0v*P1v - 2 * Lp2*Lq1*P0u*P1u*P0v*P2v - 2 * Lp2*Lq1*P0u*P1u*P0v*PTv - 2 * Lp2*Lq1*P0u*P1u*P1v*P2v - 2 * Lp2*Lq1*P0u*P1u*P1v*PTv + 4 * Lp2*Lq1*P0u*P1u*P2v*PTv - 2 * Lp2*Lq1*P0u*P2u*P0v*P1v + 2 * Lp2*Lq1*P0u*P2u*P0v*PTv + 2 * Lp2*Lq1*P0u*P2u*P1v*P1v - 2 * Lp2*Lq1*P0u*P2u*P1v*PTv - 2 * Lp2*Lq1*P0u*P0v*P1v*PTu + 2 * Lp2*Lq1*P0u*P0v*P2v*PTu + 2 * Lp2*Lq1*P0u*P1v*P1v*PTu - 2 * Lp2*Lq1*P0u*P1v*P2v*PTu - 2 * Lp2*Lq1*P1u*P1u*P0v*P0v + 2 * Lp2*Lq1*P1u*P1u*P0v*P2v + 2 * Lp2*Lq1*P1u*P1u*P0v*PTv - 2 * Lp2*Lq1*P1u*P1u*P2v*PTv + 2 * Lp2*Lq1*P1u*P2u*P0v*P0v - 2 * Lp2*Lq1*P1u*P2u*P0v*P1v - 2 * Lp2*Lq1*P1u*P2u*P0v*PTv + 2 * Lp2*Lq1*P1u*P2u*P1v*PTv + 2 * Lp2*Lq1*P1u*P0v*P0v*PTu - 2 * Lp2*Lq1*P1u*P0v*P1v*PTu - 2 * Lp2*Lq1*P1u*P0v*P2v*PTu + 2 * Lp2*Lq1*P1u*P1v*P2v*PTu - 2 * Lp2*Lq1*P2u*P0v*P0v*PTu + 4 * Lp2*Lq1*P2u*P0v*P1v*PTu - 2 * Lp2*Lq1*P2u*P1v*P1v*PTu + 4 * Lp2*P0u*P0u*P1v*P2v - 4 * Lp2*P0u*P0u*P1v*PTv - 4 * Lp2*P0u*P0u*P2v*PTv + 4 * Lp2*P0u*P0u*PTv*PTv - 4 * Lp2*P0u*P1u*P0v*P2v + 4 * Lp2*P0u*P1u*P0v*PTv + 4 * Lp2*P0u*P1u*P2v*PTv - 4 * Lp2*P0u*P1u*PTv*PTv - 4 * Lp2*P0u*P2u*P0v*P1v + 4 * Lp2*P0u*P2u*P0v*PTv + 4 * Lp2*P0u*P2u*P1v*PTv - 4 * Lp2*P0u*P2u*PTv*PTv + 4 * Lp2*P0u*P0v*P1v*PTu + 4 * Lp2*P0u*P0v*P2v*PTu - 8 * Lp2*P0u*P0v*PTu*PTv - 8 * Lp2*P0u*P1v*P2v*PTu + 4 * Lp2*P0u*P1v*PTu*PTv + 4 * Lp2*P0u*P2v*PTu*PTv + 4 * Lp2*P1u*P2u*P0v*P0v - 8 * Lp2*P1u*P2u*P0v*PTv + 4 * Lp2*P1u*P2u*PTv*PTv - 4 * Lp2*P1u*P0v*P0v*PTu + 4 * Lp2*P1u*P0v*P2v*PTu + 4 * Lp2*P1u*P0v*PTu*PTv - 4 * Lp2*P1u*P2v*PTu*PTv - 4 * Lp2*P2u*P0v*P0v*PTu + 4 * Lp2*P2u*P0v*P1v*PTu + 4 * Lp2*P2u*P0v*PTu*PTv - 4 * Lp2*P2u*P1v*PTu*PTv + 4 * Lp2*P0v*P0v*PTu*PTu - 4 * Lp2*P0v*P1v*PTu*PTu - 4 * Lp2*P0v*P2v*PTu*PTu + 4 * Lp2*P1v*P2v*PTu*PTu + Lq1*Lq1*P0u*P0u*P1v*P1v - 2 * Lq1*Lq1*P0u*P0u*P1v*PTv + Lq1*Lq1*P0u*P0u*PTv*PTv - 2 * Lq1*Lq1*P0u*P1u*P0v*P1v + 2 * Lq1*Lq1*P0u*P1u*P0v*PTv + 2 * Lq1*Lq1*P0u*P1u*P1v*PTv - 2 * Lq1*Lq1*P0u*P1u*PTv*PTv + 2 * Lq1*Lq1*P0u*P0v*P1v*PTu - 2 * Lq1*Lq1*P0u*P0v*PTu*PTv - 2 * Lq1*Lq1*P0u*P1v*P1v*PTu + 2 * Lq1*Lq1*P0u*P1v*PTu*PTv + Lq1*Lq1*P1u*P1u*P0v*P0v - 2 * Lq1*Lq1*P1u*P1u*P0v*PTv + Lq1*Lq1*P1u*P1u*PTv*PTv - 2 * Lq1*Lq1*P1u*P0v*P0v*PTu + 2 * Lq1*Lq1*P1u*P0v*P1v*PTu + 2 * Lq1*Lq1*P1u*P0v*PTu*PTv - 2 * Lq1*Lq1*P1u*P1v*PTu*PTv + Lq1*Lq1*P0v*P0v*PTu*PTu - 2 * Lq1*Lq1*P0v*P1v*PTu*PTu + Lq1*Lq1*P1v*P1v*PTu*PTu) + 2 * P0u*P1v - 2 * P1u*P0v - 2 * P0u*PTv + 2 * P0v*PTu + 2 * P1u*PTv - 2 * P1v*PTu - 2 * Lp1*P0u*P1v + 2 * Lp1*P1u*P0v + Lp1*P0u*P2v - Lp1*P2u*P0v - Lp2*P0u*P1v + Lp2*P1u*P0v - 2 * Lp1*P1u*P2v + 2 * Lp1*P2u*P1v + Lp2*P0u*P2v - Lp2*P2u*P0v - Lp2*P1u*P2v + Lp2*P2u*P1v + Lq1*P0u*P1v - Lq1*P1u*P0v + Lp1*P0u*PTv - Lp1*P0v*PTu - Lp1*P2u*PTv + Lp1*P2v*PTu - Lq1*P0u*PTv + Lq1*P0v*PTu + Lq1*P1u*PTv - Lq1*P1v*PTu) / (2 * (P0u*P1v - P1u*P0v - P0u*PTv + P0v*PTu + P1u*PTv - P1v*PTu - Lp1*P0u*P1v + Lp1*P1u*P0v + Lp1*P0u*P2v - Lp1*P2u*P0v - Lp1*P1u*P2v + Lp1*P2u*P1v));
	}

	double level;
	if (Rp1 < 0) {
		level = (Rp1*dR + Rp2) / (dR + Rq1);
	}
	else {
		level = (Rp1*(1 - dR) + Rp2) / ((1 - dR) + Rq1);
	}

	return level;
}
