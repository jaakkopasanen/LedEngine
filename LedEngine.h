#ifndef LedEngineH
#define LedEngineH

/**
 * Data structure for CIE 1976 Luv coordinates
 */
struct Luv {
	float L;
	float u;
	float v;
};

/**
 * Data structure for RGB color
 */
struct RGB {
	float R;
	float G;
	float B;
};

/**
 * LedEngine class
 */
class LedEngine {
public:
	/**
	 * Constructor
	 *
	 * \param redPin GPIO pin number for red LED
	 * \param greenPin GPIO pin number for green LED
	 * \param bluePin GPIO pin number for blue LED
	 * \param warmPin GPIO pin number for warm white LED
	 * \param coldPin GPIO pin number for cold white LED
	 * \param pwmRange PWM bit width as a maximum possible value e.g. 255 or 1023
	 */
	LedEngine(const uint8_t redPin, const uint8_t greenPin, const uint8_t bluePin, const uint8_t warmPint, const uint8_t coldPin, uint16_t pwmRange);

	/**
	 * Is the light on?
	 *
	 * \return Is the light on
	 */
	bool getOnOff();

	/**
	 * Sets light on or off
	 *
	 * \param onOff Turn light on or off
	 */
	void setOnOff(const bool onOff);

	/**
	 * Get raw PWM values
	 *
	 * \return Raw PWM values normalized in the range 0..1
	 */
	RGB getRaw();

	/**
	 * Set raw PWM values
	 *
	 * \param raw Raw PWM values normalized in the range 0..1
	 */
	void setRaw(const RGB raw);

	/**
	 * Get CIE 1976 UCS color coordinates
	 *
	 * \return CIE 1976 UCS coordinates and lightness
	 */
	Luv getCie1976Ucs();

	/**
	 * Set CIE 1976 UCS color coordinates
	 *
	 * \param luv CIE 1976 UCS coordinates and lightness
	 */
	void setCie1976Ucs(const Luv luv);

	/**
	 * Get color temperature in Kelvins
	 *
	 * \return Color temperature in Kelvins
	 */
	uint16_t getColorTemperature();

	/**
	 * Sets ligth by color temperature
	 *
	 * \param L CIE 1976 lightness
	 * \param T Color temperature in Kelvins
	 */
	void setColorTemperature(const float L, const uint16_t T);

	/**
	 * Get red LED CIE 1976 UCS coordinates
	 *
	 * \return CIE 1976 UCS coordinates for red LED
	 */
	Luv getRedUv();

	/**
	 * Get green LED CIE 1976 UCS coordinates
	 *
	 * \return CIE 1976 UCS coordinates for blue LED
	 */
	Luv getGreenUv();

	/**
	 * Get blue LED CIE 1976 UCS coordinates
	 *
	 * \return CIE 1976 UCS coordinates for green LED
	 */
	Luv getBlueUv();

	/**
	 * Get red LED luminous flux
	 *
	 * \return Luminous flux for red LED as given in calibrate function
	 */
	float getRedLum();

	/**
	 * Get green LED luminous flux
	 *
	 * \return Luminous flux for green LED as given in calibrate function
	 */
	float getGreenLum();

	/**
	 * Get blue LED luminous flux
	 *
	 * \return Luminous flux for blue LED as given in calibrate function
	 */
	float getBlueLum();

	/**
	 * Get rational function coefficients for red level vs normalized red-to-green distance
	 *
	 * \return Pointer to rational function coefficients for red LED level vs normalized red-to-green distance
	 */
	float * getRedToGreenFit();

	/**
	 * Get rational function coefficients for green level vs normalized green-to-blue distance
	 *
	 * \return Pointer to rational function coefficients for green LED level vs normalized green-to-blue distance
	 */
	float * getGreenToBlueFit();

	/**
	 * Get rational function coefficients for blue level vs normalized blue-to-red distance
	 *
	 * \return Pointer to rational function coefficients for blue LED level vs normalized blue-to-red distance
	 */
	float * getBlueToRedFit();

	/**
	 * Save calibration parameters
	 *
	 * \param redUv CIE 1976 UCS coordinates for red LED
	 * \param greenUv CIE 1976 UCS coordinates for green LED
	 * \param blueUv CIE 1976 UCS coordinates for blue LED
	 * \param redLum Luminous flux for red LED
	 * \param redLum Luminous flux for green LED
	 * \param redLum Luminous flux for blue LED
	 * \param redToGreenFit Rational function coefficients for red LED level vs normalized red-to-green distance
	 * \param redToGreenFit Rational function coefficients for green LED level vs normalized green-to-blue distance
	 * \param redToGreenFit Rational function coefficients for blue LED level vs normalized blue-to-red distance
	 */
	void calibrate(const Luv redUv, const Luv greenUv, const Luv blueUv, const float redLum,
		const float greenLum, const float blueLum, const float redToGreenFit[3], const float greenToBlueFit[3],
		const float blueToRedFit[3]);

private:
	/**
	 * GPIO pin number for red LED
	 */
	uint8_t redPin_;

	/**
	 * GPIO pin number for green LED
	 */
	uint8_t greenPin_;

	/**
	 * GPIO pin number for blue LED
	 */
	uint8_t bluePin_;

	/**
	 * GPIO pin number for warm white LED
	 */
	uint8_t warmPin_;

	/**
	 * GPIO pin number for cold white LED
	 */
	uint8_t coldPin_;

	/**
	 * PWM bit width as a maximum number, e.g. 255 or 1023
	 */
	uint16_t pwmRange_;

	/**
	 * Is the light on?
	 */
	bool onOff_;

	/**
	 * Raw PWM values for the RGB LED normalized in the range 0..1
	 */
	RGB raw_;

	/**
	 * CIE 1976 UCS coordinates and lightness
	 */
	Luv luv_;

	/**
	 * Color temperature
	 */
	uint16_t T_;

	/**
	 * CIE 1976 UCS coordinates for red LED
	 */
	Luv redUv_ = { 100, 0.5535, 0.5170 };

	/**
	 * CIE 1976 UCS coordinates for green LED
	 */
	Luv greenUv_ = { 100, 0.0373, 0.5856 };

	/**
	 * CIE 1976 UCS coordinates for blue LED
	 */
	Luv blueUv_{ 100, 0.1679, 0.1153 };

	/**
	 * Luminous flux for red LED
	 */
	float redLum_ = 0.5;

	/**
	 * Luminous flux for green LED
	 */
	float greenLum_ = 1.0;

	/**
	 * Luminous flux for blue LED
	 */
	float blueLum_ = 0.75;

	/**
	 * Luminous flux which yields CIE 1976 lightness of 100
	 */
	float maxLum_ = 2.25;

	/**
	 * Rational function coefficients for red LED level vs normalized red-to-green distance
	 */
	float redToGreenFit_[3] = { 2.9658, 0.0, 1.9658 };

	/**
	 * Rational function coefficients for green LED level vs normalized green-to-blue distance
	 */
	float greenToBlueFit_[3] = { 1.3587, 0.0, 0.3587 };

	/**
	 * Rational function coefficients for blue LED level vs normalized blue-to-red distance
	 */
	float blueToRedFit_[3] = { -0.2121, 0.2121, 0.2121 };

	/**
	 * Finds coefficient for LED needed to produce target CIE 1976 UCS coordinates
	 *
	 * \param PT CIE 1976 UCS coordinates for target point
	 * \param P0 CIE 1976 UCS coordinates for the source LED whose level is to be searched, e.g. redUv
	 * \param P1 CIE 1976 UCS coordinates for the next LED counter-clockwise, e.g. greenUv
	 * \param P2 CIE 1976 UCS coordinates for the last RGB LED, e.g. blueUv
	 * \param rightHandFit Rational function coefficients for source LED level vs normalized right hand side distance
	 * \param leftHandFit Rational function coefficients for normalized left hand side distance vs normalized right hand side distance
	 */
	float findCoefficient_(const Luv PT, const Luv P0, const Luv P1, const Luv P2, const float rightHandFit[3], const float leftHandFit[3]);
};

#endif
