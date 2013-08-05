
#include "CounterDeviceInterface.h"
#include <cassert>

void CounterDeviceInterface::read(unsigned long & value)
{
	adlink_assert(
		D2K_GCTR_Read(
			this->handler(),
			this->m_counterID,
			(U32*)&value
		));
}

void CounterDeviceInterface::reset()
{
	adlink_assert(
		D2K_GCTR_Reset(
			this->handler(),
			this->m_counterID
		));
}

unsigned short CounterDeviceInterface::status_register()
{
	U16 value;
	adlink_assert(
		D2K_GCTR_Status(
			this->handler(),
			this->m_counterID,
			&value
		));
	return value;
}

void CounterDeviceInterface::software_control_gate(unsigned short value)
{
	adlink_assert(
		D2K_GCTR_Control(
			this->handler(),
			this->m_counterID,
			GPTC_IntGATE,
			value
		));
}
void CounterDeviceInterface::software_control_updown(unsigned short value)
{
	adlink_assert(
		D2K_GCTR_Control(
			this->handler(),
			this->m_counterID,
			GPTC_IntGATE,
			value
		));
}
void CounterDeviceInterface::software_control_enable(unsigned short value)
{
	adlink_assert(
		D2K_GCTR_Control(
			this->handler(),
			this->m_counterID,
			GPTC_IntGATE,
			value
		));
}

void CounterDeviceInterface::setup()
{
	assert( (this->m_mode > 0) && (this->m_mode < 9) );
	assert( SimpleGatedEventCNT == 1);
	assert( SinglePeriodMSR == 2);
	assert( SinglePulseWidthMSR == 3);
	assert( SingleGatedPulseGen == 4);
	assert( SingleTrigPulseGen == 5);
	assert( RetrigSinglePulseGen == 6);
	assert( SingleTrigContPulseGen == 7);
	assert( ContGatedPulseGen == 8);

	U8 srcCtrl=0, polCtrl=0;
	U16 lreg1=0, lreg2=0;

	srcCtrl |= this->m_sourceTimeBaseExternal ? D2K_GPTC_CLKSRC_EXT : D2K_GPTC_CLKSRC_INT;
	srcCtrl |= this->m_sourceGateExternal ? D2K_GPTC_GATESRC_EXT : D2K_GPTC_GATESRC_INT;
	srcCtrl |= this->m_sourceUpDownExternal ? D2K_GPTC_UPDOWN_SEL_EXT : D2K_GPTC_UPDOWN_SEL_INT;

	polCtrl |= this->m_polarityLowGate ? D2K_GPTC_GATE_LACTIVE : D2K_GPTC_GATE_HACTIVE;
	polCtrl |= this->m_polarityLowUpDown ? D2K_GPTC_UPDOWN_LACTIVE : D2K_GPTC_UPDOWN_HACTIVE;
	polCtrl |= this->m_polarityLowClockEn ? D2K_GPTC_CLKEN_LACTIVE : D2K_GPTC_CLKEN_HACTIVE;
	polCtrl |= this->m_polarityLowOutput ? D2K_GPTC_OUTPUT_LACTIVE : D2K_GPTC_OUTPUT_HACTIVE;

	lreg1 = (this->m_mode < 4) ? this->m_initialCount : this->m_pulseDelay;
	lreg2 = (this->m_mode < 4) ? 0 : this->m_pulseWidth;
	
	adlink_assert(
		D2K_GCTR_Setup(
			this->handler(),
			this->m_counterID,
			this->m_mode,
			srcCtrl,
			polCtrl,
			lreg1,
			lreg2
		));
}

/*static*/ DevicePartInterface* AdlDeviceFactorySingleton::create_counter_device(CommonDeviceInterface* commonDevice)
{
	return new CounterDeviceInterface( commonDevice );
}
