
#include "Utils.h"
#include "AdlinkCommon.h"
#include <cmath>
#include <cassert>
#include <cstdlib>


bool operator==(const struct timeval& a, const struct timeval& b)
{
    return (a.tv_usec == b.tv_usec) && (a.tv_sec == b.tv_sec);
}
 
bool operator!=(const struct timeval& a, const struct timeval& b)
{
    return (a.tv_usec != b.tv_usec) || (a.tv_sec != b.tv_sec);
}

unsigned int TriggerMode::get_trigger_mode(bool isInput) const
{
	if (isInput) {
		switch (this->m_nextTriggerMode)
		{
			case DAQ2K_AI_TRGMOD_POST: return 0;
			case DAQ2K_AI_TRGMOD_DELAY: return 1;
			case DAQ2K_AI_TRGMOD_PRE: return 2;
			case DAQ2K_AI_TRGMOD_MIDL: return 3;
		};
	} else {
		switch (this->m_nextTriggerMode)
		{
			case DAQ2K_DA_TRGMOD_POST: return 0;
			case DAQ2K_DA_TRGMOD_DELAY: return 1;
		};
	}
	return 0;
}

bool TriggerMode::set_trigger_mode(unsigned int tm, bool isInput)
{
	unsigned int mode = 0;
	switch (tm)
	{
		case 0:
			mode = isInput? DAQ2K_AI_TRGMOD_POST : DAQ2K_DA_TRGMOD_POST;
			break;
		case 1:
			mode = isInput? DAQ2K_AI_TRGMOD_DELAY : DAQ2K_DA_TRGMOD_DELAY;
			break;
		case 2:
			if (!isInput)
				return false;
			mode = DAQ2K_AI_TRGMOD_PRE;
			break;
		case 3:
			if (!isInput)
				return false;
			mode = DAQ2K_AI_TRGMOD_MIDL;
			break;
		default:
			return false;
			break;
	}
	this->m_nextTriggerMode = mode;
	return true;
}


/*static*/ void TriggerMode::trigger_source_string(std::string & trgSource)
{
	// We assume Analog Input constants are valid for analog output
	// If it changes someday, we will need to rewrite some functions
	assert(DAQ2K_AI_TRGSRC_SOFT == DAQ2K_DA_TRGSRC_SOFT);
	assert(DAQ2K_AI_TRGSRC_ANA == DAQ2K_DA_TRGSRC_ANA);
	assert(DAQ2K_AI_TRGSRC_ExtD == DAQ2K_DA_TRGSRC_ExtD);
	assert(DAQ2K_AI_TRSRC_SSI == DAQ2K_DA_TRSRC_SSI);
	
	/// @bug Wee are using just a simple string when there's in fact lots of
	/// params to set about trigger source and mode:
	// (2) Trigger Mode Selection  ==>> TriggerMode[0-5]
	// 	DAQ2K_AI_TRGMOD_POST : Post Trigger Mode (default)
	// 	DAQ2K_AI_TRGMOD_DELAY : Delay Trigger Mode
	// 	DAQ2K_AI_TRGMOD_PRE : Pre-Trigger Mode
	// 	DAQ2K_AI_TRGMOD_MIDL : Middle-Trigger Mode
	// (3) Delay Source Selection (only available for Delay Trigger Mode)
	// 	DAQ2K_AI_Dly1InSamples: delay in samples
	// 	DAQ2K_AI_Dly1InTimebase: delay in time base (default)
	// (4) Re-Trigger Mode Enable (only available for Delay and Post Trigger Mode) ==>> TriggerMode[4,5]
	// 	DAQ2K_AI_ReTrigEn: Re-trigger in an acquisition is enabled
	// (5) MCounter Enable (only available for Pre- and Middle Trigger Mode)
	// 	This constant is only valid for Pre-trigger and Middle trigger mode
	// 	DAQ2K_AI_MCounterEn: Mcounter is enabled and then the trigger signal is
	// 	ignore before M terminal count is reached.
	// (6) External Digital Trigger Polarity  ==>> TriggerSource[ExtD:+/-]
	// 	DAQ2K_AI_TrgPositive: Trigger positive edge active (default)
	// 	DAQ2K_AI_TrgNegative: Trigger negative edge active

	static const U32 mask =	DAQ2K_AI_TRGSRC_SOFT | DAQ2K_AI_TRGSRC_ANA
						|	DAQ2K_AI_TRGSRC_ExtD | DAQ2K_AI_TRSRC_SSI;
	const U32 bit = this->m_nextTriggerSource & mask;
	std::stringstream r;


	switch (bit) {
		case DAQ2K_AI_TRGSRC_ExtD: {
				r << "ExtD";
				static const U32 mask = DAQ2K_AI_TrgPositive | DAQ2K_AI_TrgNegative;
				const U32 a = this->m_nextTriggerSource & mask;
				if(a == DAQ2K_AI_TrgNegative)
					r << ":-";
				else
					r << ":+";
			}
			break;
 		case DAQ2K_AI_TRSRC_SSI:
			r << "SSI";
			break;
		case DAQ2K_AI_TRGSRC_ANA: {
				r << "ANA";
				static const U32 mask =	Below_Low_level | Above_High_Level
									|	Inside_Region
									|	Low_Hysteresis | High_Hysteresis;
				const U32 a = this->m_anaTrigAIOConf & mask;
				if (!m_usingDeprecatedAnalogTrigger) {
					switch(a) {
						case Below_Low_level:
							r << ":below-low";
							break;
						case Above_High_Level:
							r << ":above-high";
							break;
						case Inside_Region:
							r 	<< ":inside-region";
							break;
						case Low_Hysteresis:
							r 	<< ":low-hysteresis";
							break;
						case High_Hysteresis:
							r 	<< ":high-hysteresis";
							break;
					};
				} else {
					r << "(DEPRECATED USAGE! Please See TriggerLowLimit, TriggerHighLimit and the new info about TriggerSources)";
					switch(a) {
						case Below_Low_level:
							r << ":low=" << this->m_deprecatedAnaTrigLow;
							break;
						case Above_High_Level:
							r << ":high=" << this->m_deprecatedAnaTrigHigh;
							break;
						case Inside_Region:
							assert(false);
							break;
						case Low_Hysteresis:
							r 	<< ":low=" << this->m_deprecatedAnaTrigLow
								<< ";high=" << this->m_deprecatedAnaTrigHigh;
							break;
						case High_Hysteresis:
							// Low and High are switched, like they are
							// in parse_trigger_source()
							r << ":low=" << this->m_deprecatedAnaTrigHigh;
							r << ";high=" << this->m_deprecatedAnaTrigLow;
							break;
					};
				}
			}
			break;
		case DAQ2K_AI_TRGSRC_SOFT:
		default:
			r << "SOFT";
			break;
	};

	trgSource = r.str();
}

///
/// editable_low_high(true): Does it make sense to edit 'low' now?
/// editable_low_high(false): Does it make sense to edit 'high' now?
/// 
/// I mean, they only make sense in ANAlog trigger mode, and even then
/// only in certain submodes (ex: In ANA:below-low we don't neeed 'high')
/// 
/// Also if configured using m_usingDeprecatedAnalogTrigger then we don't
/// set the settings through this so no reason to letting you do so.
///
bool TriggerMode::editable_low_high(bool checkLow) const
{
	if (this->m_usingDeprecatedAnalogTrigger)
		return false;
	
	if ( !(this->m_nextTriggerSource & DAQ2K_AI_TRGSRC_ANA) )
		return false;
	
	if ( !(this->m_anaTrigAIOConf & EXTATRIG) )
		return false;

	if (this->m_anaTrigAIOConf & Below_Low_level)
		return checkLow;

	if (this->m_anaTrigAIOConf & Above_High_Level)
		return !checkLow;

	if (this->m_anaTrigAIOConf & Inside_Region)
		return true;

	if (this->m_anaTrigAIOConf & Low_Hysteresis)
		return true;

	if (this->m_anaTrigAIOConf & High_Hysteresis)
		return true;
}

/// @todo: Delete with m_usingDeprecatedAnalogTrigger
static short volts_to_255(double volts, double min, double max)
{
	double r = round(255 * (volts - min)/(max - min));
	if (r < 0)
		return 0;
	if (r > 255)
		return 255;
	return short(r);
}

/// @todo: Delete with m_usingDeprecatedAnalogTrigger
short TriggerMode::analog_low(double min, double max) const
{
	if (this->m_usingDeprecatedAnalogTrigger)
		return this->m_deprecatedAnaTrigLow;
	return volts_to_255(this->m_anaTrigLow, min, max);
}

/// @todo: Delete with m_usingDeprecatedAnalogTrigger
short TriggerMode::analog_high(double min, double max) const
{
	if (this->m_usingDeprecatedAnalogTrigger)
		return this->m_deprecatedAnaTrigHigh;
	return volts_to_255(this->m_anaTrigHigh, min, max);
}

bool TriggerMode::parse_trigger_source(const std::string & trgSource)
{
	// 	DAQ2K_AI_TRGSRC_SOFT : software (default)
	// 	DAQ2K_AI_TRGSRC_ANA : From analog trigger pin
	// 	DAQ2K_AI_TRGSRC_ExtD: From external digital trigger pin
	// 	DAQ2K_AI_TRSRC_SSI : From SSI source


	// SOFT, SSI...
	//  Analog BelowLowLevel Trigger:
	// 			ANA:below-low
	//  Analog AboveHighLevel Trigger:
	// 			ANA:above-high
	//  Analog InsideRegion:
	// 			ANA:inside-region
	//  Analog LowHysteresis:
	// 			ANA:low-hysteresis
	//  Analog HighHysteresis:
	// 			ANA:high-hysteresis
	//  Digital trigger positive polarity:
	//			ExtD:+
	//  Digital trigger negative polarity:
	//			Extd:-

	static const U32 mask =		DAQ2K_AI_TRGSRC_SOFT | DAQ2K_AI_TRGSRC_ANA
							|	DAQ2K_AI_TRGSRC_ExtD | DAQ2K_AI_TRSRC_SSI
							|	DAQ2K_AI_TrgPositive | DAQ2K_AI_TrgNegative;

	double low=-1, high=-1;
	bool useLow = false, useHigh = false;
	U32 r = 0;
	if ( (trgSource == "SOFT") || (trgSource == "") ) {
		r |= DAQ2K_AI_TRGSRC_SOFT;
	} else if (trgSource == "ExtD:-") {
			r |= DAQ2K_AI_TRGSRC_ExtD | DAQ2K_AI_TrgNegative;
	} else if ( (trgSource == "ExtD") || (trgSource == "ExtD:+") ) {
			r |= DAQ2K_AI_TRGSRC_ExtD | DAQ2K_AI_TrgPositive;
	} else if (trgSource == "SSI") {
		r |= DAQ2K_AI_TRSRC_SSI;
	} else if (trgSource.find("ANA:below-low") == 0) {
		r |= DAQ2K_AI_TRGSRC_ANA;
		this->m_anaTrigAIOConf = EXTATRIG | Below_Low_level;
		this->m_usingDeprecatedAnalogTrigger = false;
	} else if (trgSource.find("ANA:above-high") == 0) {
		r |= DAQ2K_AI_TRGSRC_ANA;
		this->m_anaTrigAIOConf = EXTATRIG | Above_High_Level;
		this->m_usingDeprecatedAnalogTrigger = false;
	} else if (trgSource.find("ANA:inside-region") == 0) {
		r |= DAQ2K_AI_TRGSRC_ANA;
		this->m_anaTrigAIOConf = EXTATRIG | Inside_Region;
		this->m_usingDeprecatedAnalogTrigger = false;
	} else if (trgSource.find("ANA:low-hysteresis") == 0) {
		r |= DAQ2K_AI_TRGSRC_ANA;
		this->m_anaTrigAIOConf = EXTATRIG | Low_Hysteresis;
		this->m_usingDeprecatedAnalogTrigger = false;
	} else if (trgSource.find("ANA:high-hysteresis") == 0) {
		r |= DAQ2K_AI_TRGSRC_ANA;
		this->m_anaTrigAIOConf = EXTATRIG | High_Hysteresis;
		this->m_usingDeprecatedAnalogTrigger = false;
	} else if ((trgSource.find("ANA:low=") == 0) || (trgSource.find("ANA:high=") == 0)) {
		/// @todo deprecated form of setting ANA, say it somehow, somewhere...
		/// @todo In not much time, delete support for this stuff completely
		// This is the old declaration for analog trigger. Note that it
		// is deprecated and should not be used anymore.
		//
		// Values of low and high here are not in board units but in a range
		// 0-255. If the board is configured in (-10~10V), to put a threshold
		// at 0V you should set 127 (255/2) and if it is (0~10) what you
		// should use is 0. So if you change the range, you must change
		// the value here too. It's not needed anymore with the new
		// syntax. Also note that InsideRegion is not supported using this
		// deprecated syntax.
		// 	Analog BelowLowLevel Trigger:
		// 			ANA:low=3
		// 	Analog AboveHighLevel Trigger:
		// 			ANA:high=3
		//  Analog InsideRegion: UNSUPORTED
		// 	Analog LowHysteresis:
		// 			ANA:low=3;high=4
		// 	Analog HighHysteresis:
		// 			ANA:low=4;high=3  (note low > high)
		this->m_usingDeprecatedAnalogTrigger = true;
		int low=-1, high=-1;
		bool useLow = false, useHigh = false;
	
		if ( (trgSource.size() > 3) && (trgSource[3] != ':') )
			return false;
		r |= DAQ2K_AI_TRGSRC_ANA;
		size_t lows = trgSource.find("low=");
		if(lows != -1) {
			useLow = true;
			size_t lowe = trgSource.find(";", lows+4);
			low = atoi(trgSource.substr(lows+4, lowe).c_str());
		}
		size_t hs = trgSource.find("high=");
		if(hs != -1) {
			useHigh = true;
			size_t he = trgSource.find(";", hs+5);
			high = atoi(trgSource.substr(hs+5, he).c_str());
		}
		U32 a = 0;
		this->m_deprecatedAnaTrigHigh = high;
		this->m_deprecatedAnaTrigLow = low;
		if(useLow && !useHigh)
			a = Below_Low_level;
		else if(useHigh && !useLow)
			a = Above_High_Level;
		else if(useHigh && useLow) {
			if(low < high)
				a = Low_Hysteresis;
			else {
				a = High_Hysteresis;
				this->m_deprecatedAnaTrigHigh = low;
				this->m_deprecatedAnaTrigLow = high;
			}
		}
		this->m_anaTrigAIOConf = EXTATRIG | a;
	} else {
		return false;
	}
	
	this->m_nextTriggerSource &= ~mask;
	this->m_nextTriggerSource |= r;
	
	return true;
}




RangeChecker::RangeChecker() : 
  adl_range(AD_B_10_V),
  m_nextMinRange(-10),
  m_nextMaxRange(10)
{
	m_ranges[AD_B_10_V] =		Range(	-10.0,		10.0	);
	m_ranges[AD_B_5_V] =		Range(	-5.0,		5.0		);
	m_ranges[AD_B_2_5_V] =		Range(	-2.5,		2.5		);
	m_ranges[AD_B_1_25_V] =		Range(	-1.25,		1.25	);
	m_ranges[AD_B_0_625_V] =	Range(	-0.625,		0.625	);
	m_ranges[AD_B_0_3125_V] =	Range(	-0.3125,	0.3125	);
	m_ranges[AD_B_0_5_V] =		Range(	-0.5,		0.5		);
	m_ranges[AD_B_0_05_V] =		Range(	-0.05,		0.05	);
	m_ranges[AD_B_0_005_V] =	Range(	-0.005,		0.005	);
	m_ranges[AD_B_1_V] =		Range(	-1.0,		1.0		);
	m_ranges[AD_B_0_1_V] =		Range(	-0.1,		0.1		);
	m_ranges[AD_B_0_01_V] =		Range(	-0.01,		0.01	);
	m_ranges[AD_B_0_001_V] =	Range(	-0.001,		0.001	);
	
	m_ranges[AD_U_20_V] =		Range(	0.0,		20.0	);
	m_ranges[AD_U_10_V] =		Range(	0.0,		10.0	);
	m_ranges[AD_U_5_V] =		Range(	0.0,		5.0		);
	m_ranges[AD_U_2_5_V] =		Range(	0.0,		2.5		);
	m_ranges[AD_U_1_25_V] =		Range(	0.0,		1.25	);
	m_ranges[AD_U_1_V] =		Range(	0.0,		1.0		);
	m_ranges[AD_U_0_1_V] =		Range(	0.0,		0.1		);
	m_ranges[AD_U_0_01_V] =		Range(	0.0,		0.01	);
	m_ranges[AD_U_0_001_V] =	Range(	0.0,		0.001	);

	m_ranges[AD_B_2_V] =		Range(	-2.0,		2.0		);
	m_ranges[AD_B_0_25_V] =		Range(	-0.25,		0.25	);
	m_ranges[AD_B_0_2_V] =		Range(	-0.2,		0.2		);

	m_ranges[AD_U_4_V] =		Range(	0.0,		4.0		);
	m_ranges[AD_U_2_V] =		Range(	0.0,		2.0		);
	m_ranges[AD_U_0_5_V] =		Range(	0.0,		0.5		);
	m_ranges[AD_U_0_4_V] =		Range(	0.0,		0.4		);

	this->set_all_valid_ranges();
	this->apply_range();
}

void RangeChecker::set_valid_ranges(const RangeIdSet & vr)
{
	this->m_validRanges.clear();
	RangeIdSet::iterator i, e = vr.end();
	for (i = vr.begin(); i != e; ++i) {
		if (this->m_ranges.count(*i))
			this->m_validRanges.insert(*i);
		else
			std::cout << "WARN: Trying to set an unknown range" << std::endl;
	}
}

void RangeChecker::set_all_valid_ranges()
{
	this->m_validRanges.clear();
	RangeMap::iterator i, e = this->m_ranges.end();
	for (i = this->m_ranges.begin(); i != e; ++i)
		this->m_validRanges.insert(i->first);
}


bool RangeChecker::set_min_max_range(double min, double max)
{
	RangeMap::const_iterator i, e = this->m_ranges.end();
	for (i = this->m_ranges.begin(); i != e; ++i) {
		const Range & r = i->second;
		if ( ( fabs(min - r.min) < 1e-6 ) && ( fabs(max - r.max) < 1e-6 ) ) {
			if (this->m_validRanges.count(i->first)) {
				this->adl_range = i->first;
				return true;
			}
		}
	}
	return false;
}

bool RangeChecker::get_min_max_range(double &min, double &max) const
{
	RangeMap::const_iterator i, e = this->m_ranges.end();
	i = this->m_ranges.find(this->adl_range);
	if (i != e) {
		const Range & r = i->second;
		min = r.min;
		max = r.max;
		return true;
	}
	return false;
}

bool RangeChecker::set_range_id(U16 range)
{
	if (!this->m_validRanges.count(range))
		return false;
	U16 old = this->adl_range;
	this->adl_range = range;
	if(this->get_min_max_range(this->m_nextMinRange, this->m_nextMaxRange))
		return true;
	this->adl_range = old;
	return false;
}



bool RangeChecker::apply_range()
{
	bool r = this->set_min_max_range(this->m_nextMinRange, this->m_nextMaxRange);
	r = r && this->get_min_max_range(this->m_currentMin, this->m_currentMax);
	return r;
}


bool RangeChecker::set_next_min_range(double min)
{
	this->m_nextMinRange = min;
}

bool RangeChecker::set_next_max_range(double max)
{
	this->m_nextMaxRange = max;
}

bool RangeChecker::get_next_min_max_range(double &min, double &max) const
{
	min = this->m_nextMinRange;
	max = this->m_nextMaxRange;
	return true;
}
