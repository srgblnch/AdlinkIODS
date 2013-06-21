
#ifndef RANGE_CHECKER_H____DDKdsf39ed
#define RANGE_CHECKER_H____DDKdsf39ed

#include "AdlHelp.h"

#include <map>
#include <set>
#include <cassert>

class AdlinkCommon;
class BaseBehaviour;

bool operator==(const struct timeval& a, const struct timeval& b);
bool operator!=(const struct timeval& a, const struct timeval& b);

struct Range {
	Range(double _min, double _max) : min(_min), max(_max) {}
	Range() : min(0.0), max(0.0) {}
	double min;
	double max;
};
typedef std::map<unsigned, Range> RangeMap;
typedef std::set<unsigned> RangeIdSet;


class RangeChecker {
	RangeMap m_ranges;
	RangeIdSet m_validRanges;
	double m_nextMinRange;
	double m_nextMaxRange;

	double m_currentMin;
	double m_currentMax;

	U16 adl_range; /// adl_range as defined for DAQ-2000 Cards: AD_B_10_V, AD_B_5_V...

	bool set_min_max_range(double min, double max);
public:
	bool get_min_max_range(double &min, double &max) const;

	RangeChecker();

	bool apply_range();
	bool get_next_min_max_range(double &min, double &max) const;
	bool set_next_min_range(double min);
	bool set_next_max_range(double max);

	void set_valid_ranges(const RangeIdSet & vr);
	void set_all_valid_ranges();

	U16 range_id() const { return adl_range; }

	double current_min() const
	{	return m_currentMin;	}

	double current_max() const
	{	return m_currentMax;	}

	double next_min() const
	{	return m_nextMinRange;	}

	double next_max() const
	{	return m_nextMaxRange;	}

	bool set_range_id(U16 range);
};


class TriggerMode {
	/// Trigger mode, in the same format as parameter to D2K_AI_Config
	int m_triggerMode;
	U32 m_nextTriggerMode;
	U32 m_nextTriggerSource;

	double m_anaTrigLow;
	double m_anaTrigHigh;

	/// @name Deprecated analog trigger
	/// Two ways to configure analog trigger: By TriggerSource(deprecated)
	/// or by different attributes. This should be removed someday.
	/// @{
	bool	m_usingDeprecatedAnalogTrigger;
	int		m_deprecatedAnaTrigHigh;
	int		m_deprecatedAnaTrigLow;
	/// @}

public:
	U32		m_anaTrigAIOConf;

	TriggerMode() :
		m_triggerMode(0),
		m_nextTriggerMode(0),
		m_nextTriggerSource(0),
		m_deprecatedAnaTrigHigh(0),
		m_deprecatedAnaTrigLow(0),
		m_anaTrigAIOConf(0),
		m_usingDeprecatedAnalogTrigger(false)
	{}


	bool editable_low_high(bool checkLow) const;

	/// @todo delete when deleting m_usingDeprecatedAnalogTrigger
	/// @{
 	short analog_low(double min, double max) const;
 	short analog_high(double min, double max) const;
	/// @}

	void set_analog_low(double low) {
		this->m_anaTrigLow = low;
	}

	void set_analog_high(double high) {
		this->m_anaTrigHigh = high;
	}
	
	inline U32 analog_conf() const {
		return this->m_anaTrigAIOConf;
	}

	inline double analog_low() const
	{	return this->m_anaTrigLow; }

	inline double analog_high() const
	{	return this->m_anaTrigHigh; }

	inline U32 get_next_mode() const
	{
		return m_nextTriggerMode | m_nextTriggerSource;
	}

	inline U32 get_mode() const
	{
		return m_triggerMode;
	}

	inline void apply()
	{
		m_triggerMode = m_nextTriggerMode | m_nextTriggerSource;
	}

	bool set_trigger_mode(unsigned int tm, bool isInput);
	unsigned int get_trigger_mode(bool isInput) const;

	bool parse_trigger_source(const std::string & trgSource);
	void trigger_source_string(std::string & trgSource);

	inline bool is_software()
	{
		assert(DAQ2K_AI_TRGSRC_SOFT == 0); // that's why we need this trick!

		static const U32 mask = 	DAQ2K_AI_TRGSRC_SOFT | DAQ2K_AI_TRGSRC_ANA
								|	DAQ2K_AI_TRGSRC_ExtD | DAQ2K_AI_TRSRC_SSI;
		return (this->m_triggerMode & mask) == 0;
	}

	inline bool is_analog() const
	{
		assert(DAQ2K_AI_TRGSRC_ANA == DAQ2K_DA_TRGSRC_ANA );
		return m_triggerMode & DAQ2K_AI_TRGSRC_ANA;
	}
};

#endif // RANGE_CHECKER_H____DDKdsf39ed
