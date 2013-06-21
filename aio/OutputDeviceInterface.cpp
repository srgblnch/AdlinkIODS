
#include "AdlHelp.h"
#include "InputDeviceInterface.h"
#include "AdlDeviceFactorySingleton.h"
#include <vector>
#include <cassert>
#include <cmath>

#include "OutputDeviceInterface.h"

#define _OCLASSNAME "InputDevicePSDASK"
#include "odebug.h"

///@todo fix tabs/spaces mess
///@todo interrupt checks...


class OutputDeviceD2K : public OutputDeviceInterface
{
public:
  OutputDeviceD2K(CommonDeviceInterface* commonDevice):
    OutputDeviceInterface(commonDevice)
  {
  }
  
	virtual bool supports_continuous_output() const
	{
		return true;
	}

  virtual void cont_buffer_compose(U16 wChannel, U32 dwUpdateCount, void* conBuffer, void* buffer)
  {
	this->_d2k_cont_buffer_compose(wChannel, dwUpdateCount, 0, conBuffer, buffer, false);
  }

  void _d2k_cont_buffer_compose(U16 wChannel, U32 dwUpdateCount, U16 group, void* conBuffer, void* buffer, bool fifoload)
  {
	printf("D2K_AO_ContBufferCompose(%d, %d, %d, %d, %d, %d);\n",
						handler(),            // U16 wCardNumber
						group,                // U16 group
						wChannel,             // U16 wChannel
						dwUpdateCount,        // U32 dwUpdateCount
						conBuffer,            // void* ConBuffer
						buffer,               // void* Buffer
						(BOOLEAN)fifoload ); // BOOLEAN fifoload

	adlink_assert(D2K_AO_ContBufferCompose(
						handler(),            // U16 wCardNumber
						group,                // U16 group
						wChannel,             // U16 wChannel
						dwUpdateCount,        // U32 dwUpdateCount
						conBuffer,            // void* ConBuffer
						buffer,               // void* Buffer
						(BOOLEAN)fifoload )); // BOOLEAN fifoload
  }

  virtual U32 initial_memory_allocated_kb()
  {
		U32 memSize;
		adlink_assert(D2K_AO_InitialMemoryAllocated(this->handler(), &memSize));
		return memSize;
  }

  virtual U16 cont_buffer_setup(void* buffer, U32 writeCnt)
  {
		printf("D2K_AO_ContBufferSetup (%d, %d, %d, bufferId)\n", handler(), buffer, writeCnt);
		U16 bufId;
		adlink_assert( D2K_AO_ContBufferSetup(this->handler(), buffer, writeCnt, &bufId));
		printf("  -> bufferId = %d", bufId);
		return bufId;
  }

  virtual void config(U16 configCtrl, U16 trgCtrl, U32 reTrgCnt, U32 DLY1Cnt, U32 DLY2Cnt, bool autoResetBuf)
  {
	// for D2K DLYXCnt are U16; For 6202 are U32

	const BOOLEAN _autoResetBuf = BOOLEAN(autoResetBuf);
	adlink_assert(D2K_AO_Config( this->handler(),
							  configCtrl,
							  trgCtrl,
							  reTrgCnt, 
							  DLY1Cnt,
							  DLY2Cnt,
							  _autoResetBuf ));
  }




  virtual void async_check(bool &isStopped, U32 &accesCnt)
  {
	BOOLEAN _isStopped;
	adlink_assert(D2K_AO_AsyncCheck(handler(), &_isStopped, &accesCnt));
	isStopped = _isStopped;
  }

  virtual U32 async_clear()
  {
	U32 count;
	adlink_assert(D2K_AO_AsyncClear(this->handler(), &count, DAQ2K_DA_TerminateImmediate ));
	return count;
  }

  virtual void reset_buffers()
  {
	adlink_assert( D2K_AO_ContBufferReset(this->handler() ));
  }

  virtual void configure_channel_range(U16 channel, U16 range_id)
  {
	std::stringstream ss;
	ss << "D2K_AO_CH_Config(" << this->handler() << ", " << channel << ", ";
	if (range_id == AD_U_10_V) {
		ss << "DAQ2K_DA_UniPolar, DAQ2K_DA_Int_REF, 10.0);";
		adlink_assert(D2K_AO_CH_Config(this->handler(), channel, DAQ2K_DA_UniPolar, DAQ2K_DA_Int_REF, 10.0));
	} else if (range_id == AD_B_10_V) {
		ss << "DAQ2K_DA_BiPolar, DAQ2K_DA_Int_REF, 10.0);";
		adlink_assert(D2K_AO_CH_Config(this->handler(), channel, DAQ2K_DA_BiPolar, DAQ2K_DA_Int_REF, 10.0));
	} else {
		throw_error( "Adlink ERROR",
					 "Unsupported range.\n"
					 "Valid ones are only (0,10) and (-10,10)",
					 _CFN_);
	}
	std::cout << ss.str() << std::endl;
  }

  virtual void write_channel(U16 channel, U16 value)
  {
	info() << "D2K_AO_WriteChannel(" << channel << ", " << reinterpret_cast<I16&>(value) << ")" << std::endl;
	adlink_assert(D2K_AO_WriteChannel(handler(), channel, reinterpret_cast<I16&>(value)));
  }

  virtual void cont_write_multi_channels(U16 nchannels, U16* chanBufIDs, U16 bufId, U32 writeCnt, U32 iterations, U32 CHUI, bool infinite, U16 syncMode)
  {
		assert(chanBufIDs[0] == 0);
		assert(chanBufIDs[1] == 1);

		U16 definite = infinite? 0 : 1;
		printf("D2K_AO_ContWriteMultiChannels (%d,%d,%d,%d,%d,%d,%d,%d, %d)\n",
						this->handler(), // U16 wCardNumber
						nchannels,       // U16 wNumChans
						chanBufIDs,      // U16 *pwChans
						bufId,           // U16 bufId
						writeCnt,        // U32 dwUpdateCount
						iterations,      // U32 wIterations
						CHUI,            // U32 dwCHUI
						definite,        // U16 definite
						syncMode );      // U16 wSyncMode
		adlink_assert(D2K_AO_ContWriteMultiChannels(
						this->handler(), // U16 wCardNumber
						nchannels,       // U16 wNumChans
						chanBufIDs,      // U16 *pwChans
						bufId,           // U16 bufId
						writeCnt,        // U32 dwUpdateCount
						iterations,      // U32 wIterations
						CHUI,            // U32 dwCHUI
						definite,        // U16 definite
						syncMode ));     // U16 wSyncMode
  }
};

class OutputDeviceD2KWaveform : public OutputDeviceD2K
{

public:
  OutputDeviceD2KWaveform(CommonDeviceInterface* commonDevice):
    OutputDeviceD2K(commonDevice)
  {
	m_wfmDAGroup = DA_Group_AB;
  }

  /// In waveform generation mode, the group. 
	U16 m_wfmDAGroup;

  virtual void cont_buffer_compose(U16 wChannel, U32 dwUpdateCount, void* conBuffer, void* buffer)
  {
	this->_d2k_cont_buffer_compose(wChannel, dwUpdateCount, m_wfmDAGroup, conBuffer, buffer, false);
  }

  virtual void config(U16 configCtrl, U16 trgCtrl, U32 reTrgCnt, U32 DLY1Cnt, U32 DLY2Cnt, bool autoResetBuf)
  {
	  OutputDeviceD2K::config(configCtrl | m_wfmDAGroup, trgCtrl, reTrgCnt, DLY1Cnt, DLY2Cnt, autoResetBuf);
  }

  virtual void set_wfm_group(U16* chanBufIDs, U16 nchannels)
  {
	{
		bool a = false, b = false;
		for (U16 i=0; i<nchannels; i++) {
			if (chanBufIDs[i]<4) a = true;
			if (chanBufIDs[i]>3) b = true;
		}
		if (a && b)         m_wfmDAGroup = DA_Group_AB;
		else if (a && !b)   m_wfmDAGroup = DA_Group_A;
		else if (b && !a)   m_wfmDAGroup = DA_Group_B;
		else                m_wfmDAGroup = DA_Group_AB;
	}

	printf("DA_Group set to %s\n",
		(m_wfmDAGroup==DA_Group_AB? "DA_Group_AB" :
		(m_wfmDAGroup==DA_Group_A? "DA_Group_A" : "DA_Group_B")));

	printf("D2K_AO_Group_Setup(%d,%d,%d:",
		handler(), m_wfmDAGroup, nchannels, chanBufIDs);
	for (int h=0;h<nchannels;h++)
		printf("%d,",chanBufIDs[h]);
	printf(")\n");

	adlink_assert(D2K_AO_Group_Setup(this->handler(), m_wfmDAGroup, nchannels, chanBufIDs));
  }

  virtual bool uses_wfm_generation() const
  {	return true; }

  virtual void async_check(bool &isStopped, U32 &accesCnt)
  {
	U8 _isStopped;
	adlink_assert(D2K_AO_Group_WFM_AsyncCheck(handler(), this->m_wfmDAGroup, &_isStopped, &accesCnt));
	isStopped = (_isStopped != 0);
  }

  virtual U32 async_clear()
  {
	U32 count;
	D2K_AO_Group_WFM_AsyncClear(this->handler(), DA_Group_AB, &count, DAQ2K_DA_TerminateImmediate);
	return count;
  }

  virtual void write_channel(U16 channel, U16 value)
  {
	throw_error( "Adlink ERROR",
				 "Error, 2502, 2501 waveform boards do not support D2K_AO_WriteChannel!!",
				 _CFN_);
  }

  virtual void wfm_group_update(U16* pwBuffer)
  {
	adlink_assert(D2K_AO_Group_Update(this->handler(), m_wfmDAGroup, reinterpret_cast<I16*>(pwBuffer)));
  }

  virtual void cont_write_multi_channels(U16 nchannels, U16* chanBufIDs, U16 bufId, U32 writeCnt, U32 iterations, U32 CHUI, bool infinite, U16 syncMode)
  {

		// 2502, 2501 waveform boards do not support D2K_AO_ContWriteMultiChannels!!

		/// @todo potser acceptar aixo nomes si syncMode = ASYNCH_OP?
		/// I amb nchannels i chanBufIDs puc cridar set_wfm_group,
		/// pero com que el necessito abans per l'ugly_trick, merda,
		/// de moment...
		// ignored: nchannels
		// ignored: chanBufIDs
		// ignored: syncMode

		U16 definite = infinite? 0 : 1;
		// Perform the acquisition
		printf("D2K_AO_Group_WFM_Start (%d,%d,%d,%d,%d,%d,%d,%d)\n",
					this->handler(),  // U14 cardNumber
					m_wfmDAGroup,     // U16 group
					bufId,            // U16 fstBufIdOrNotUsed
					0,                // U16 sndBufId
					writeCnt,         // U32 dwUpdateCount
					iterations,       // U32 wIterations
					CHUI,             // U32 dwCHUI
					definite);        // U16 definite

		adlink_assert(
			D2K_AO_Group_WFM_Start(
					this->handler(),  // U14 cardNumber
					m_wfmDAGroup,     // U16 group
					bufId,            // U16 fstBufIdOrNotUsed
					0,                // U16 sndBufId
					writeCnt,         // U32 dwUpdateCount
					iterations,       // U32 wIterations
					CHUI,             // U32 dwCHUI
					definite));       // U16 definite
  }




};

class OutputDevicePSDASK : public OutputDeviceInterface
{
public:
  OutputDevicePSDASK(CommonDeviceInterface* commonDevice):
    OutputDeviceInterface(commonDevice)
  {
  }

  virtual void cont_buffer_compose(U16 wChannel, U32 dwUpdateCount, void* _conBuffer, void* _buffer)
  {
	// Based on D2K_AO_ContBufferCompose, it's just that PSDASK does not
	// provide an equivalent!
	assert(wChannel < this->type_params()->aoChannels);

	U16* inBuf = reinterpret_cast<U16*>(_buffer);
	U16* outBuf = reinterpret_cast<U16*>(_conBuffer);

	U16 total_channels = this->type_params()->aoChannels;
	
   	for(U32 k=0; k<dwUpdateCount; ++k) {
	   //((U16*) ConBuffer)[k*2+wChannel] = ((U16 *) pwBuffer)[k];
	   outBuf[k*total_channels + wChannel] = inBuf[k];
  	}
  }

	virtual bool supports_continuous_output() const
	{
		return false;
	}

  virtual void config(U16 d2k_configCtrl, U16 d2k_trgCtrl, U32 reTrgCnt, U32 DLY1Cnt, U32 DLY2Cnt, bool autoResetBuf)
  {
	if ( (d2k_trgCtrl!=0) || (d2k_configCtrl!=0) || (reTrgCnt!=0) || (DLY1Cnt!=0) || (DLY2Cnt!=0))
		throw_error( "Adlink ERROR",
					 "Current configuration is not supported by this board.",
					 _CFN_);
  }

  virtual U32 initial_memory_allocated_kb()
  {
		U32 memSize;
		adlink_assert(AO_InitialMemoryAllocated(this->handler(), &memSize));
		return memSize;
  }

  /// Even though now the only PSDASK board supporting continuous mode is 6202
  /// (OutputDevice6202) most of it's functionality is defined here, so it will
  /// already be common in case some new card arrives.

  virtual U16 cont_buffer_setup(void* buffer, U32 writeCnt)
  {
		writeCnt = writeCnt/2; /// @todo Try to understand why, not just "because it's the only way it works" Same in cont_write_multi_channels
		
		printf("AO_ContBufferSetup (%d, %d, %d, bufferId)\n", handler(), buffer, writeCnt);
		U16 bufId;
		adlink_assert( AO_ContBufferSetup(this->handler(), buffer, writeCnt, &bufId));
		printf("  -> bufferId = %d", bufId);
		return bufId;
  }

  virtual void async_check(bool &isStopped, U32 &accesCnt)
  {
	BOOLEAN _isStopped;
	adlink_assert( AO_AsyncCheck(handler(), &_isStopped, &accesCnt) );
	isStopped = _isStopped;
  }

  virtual U32 async_clear()
  {
	U32 count;
	AO_AsyncClear(this->handler(), &count, 0);
	return count;
  }

  virtual void reset_buffers()
  {
	adlink_assert(AO_ContBufferReset(this->handler()));
  }

  virtual void configure_channel_range(U16 channel, U16 range_id)
  {
	if (range_id != AD_B_10_V) {
		throw_error( "Adlink ERROR",
					 "Unsupported range. Only valid value is (-10,10)",
					 _CFN_);
	}
  }

  virtual void write_channel(U16 channel, U16 value)
  {
	info() << "AO_WriteChannel(" << channel << ", " << value << ")" << std::endl;
	adlink_assert(AO_WriteChannel(handler(), channel, value));
  }

  virtual void cont_write_multi_channels(U16 nchannels, U16* chanBufIDs, U16 bufId, U32 writeCnt, U32 iterations, U32 CHUI, bool infinite, U16 syncMode)
  {
		/// @todo Try to understand why I put these /2, not just "because it's the only way it works" Same in cont_buffer_setup
		U16 definite = infinite? 0 : 1;
		printf("AO_ContWriteMultiChannels (%d,%d,%d,%d,%d,%d,%d,%d, %d)\n",
						this->handler(), // U16 wCardNumber
						nchannels,       // U16 wNumChans
						chanBufIDs,      // U16 *pwChans
						bufId,           // U16 bufId
						writeCnt/2,      // U32 dwUpdateCount
						iterations,      // U32 wIterations
						CHUI*nchannels/2,// U32 dwCHUI
						definite,        // U16 definite
						syncMode );      // U16 wSyncMode
		adlink_assert(AO_ContWriteMultiChannels(
						this->handler(), // U16 wCardNumber
						nchannels,       // U16 wNumChans
						chanBufIDs,      // U16 *pwChans
						bufId,           // U16 bufId
						writeCnt/2,      // U32 dwUpdateCount
						iterations,      // U32 wIterations
						CHUI*nchannels/2,// U32 dwCHUI
						definite,        // U16 definite
						syncMode ));     // U16 wSyncMode
  }

};

class OutputDevice6202 : public OutputDevicePSDASK
{

public:
  OutputDevice6202(CommonDeviceInterface* commonDevice):
    OutputDevicePSDASK(commonDevice)
  {
  }

  	virtual bool supports_continuous_output() const
	{
		return true;
	}

  virtual void config(U16 d2k_configCtrl, U16 d2k_trgCtrl, U32 reTrgCnt, U32 DLY1Cnt, U32 DLY2Cnt, bool autoResetBuf)
  {
	// for D2K DLYXCnt are U16; For 6202 are U32

	U16 configCtrl = 0;
	U16 trgCtrl = d2k_trgCtrl;

	assert(P6202_DA_WRSRC_Int == DAQ2K_DA_WRSRC_Int);
	assert(P6202_DA_WRSRC_AFI0 == DAQ2K_DA_WRSRC_AFI0);
	assert(P6202_DA_WRSRC_SSI == DAQ2K_DA_WRSRC_SSI);

	assert(P6202_DA_TRGMOD_DELAY == DAQ2K_DA_TRGMOD_DELAY);
	assert(P6202_DA_TRGMOD_POST == DAQ2K_DA_TRGMOD_POST);
	assert(P6202_DA_ReTrigEn == DAQ2K_DA_ReTrigEn);
	const U16 d2kValidBits = P6202_DA_TRGMOD_DELAY | P6202_DA_TRGMOD_POST | P6202_DA_ReTrigEn;

	U16 d2ksrc = d2k_trgCtrl & 0x3;
	if (d2ksrc == DAQ2K_DA_TRGSRC_SOFT)
		trgCtrl |= P6202_DA_TRGSRC_SOFT;
	else if (d2ksrc == DAQ2K_DA_TRSRC_SSI)
		trgCtrl |= P6202_DA_TRGSRC_SSI;
	else
		throw_error( "Adlink ERROR",
					 "Unsupported option for trigger source.\n"
					 "This board does not support current configuration.",
					 _CFN_ );
		/// @todo WARNING Rethink this, because we are using D2K as base, and complaining if it is not supported by PCIDASK... but PCIDASK also supports some things that are not in D2K!!

	trgCtrl |= (d2k_trgCtrl & d2kValidBits);

	if (d2k_trgCtrl & ~(d2kValidBits | 0x3))
		throw_error( "Adlink ERROR",
					 "Unexpected option in trigger control.\n"
					 "This board does not support current configuration.",
					 _CFN_);

	adlink_assert(AO_6202_Config( this->handler(),
							  configCtrl,
							  trgCtrl,
							  reTrgCnt,
							  DLY1Cnt,
							  DLY2Cnt,
							  BOOLEAN(autoResetBuf) ));

  }
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Factory:
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/*static*/ DevicePartInterface* AdlDeviceFactorySingleton::create_output_device_d2k(CommonDeviceInterface* commonDevice)
{
	const I16 type = commonDevice->type_params()->type;
	bool wfm = (type == DAQ_2502) || (type == DAQ_2501);

	if (wfm)
	  return new OutputDeviceD2KWaveform(commonDevice);
	return new OutputDeviceD2K(commonDevice);
}

/*static*/ DevicePartInterface* AdlDeviceFactorySingleton::create_output_device_psdask(CommonDeviceInterface* commonDevice)
{
	const I16 type = commonDevice->type_params()->type;

	if ((type == PCI_6208A) || (type == PCI_6208V)) {
		return new OutputDevicePSDASK(commonDevice);
	} else if(type == PCI_6202) {
		return new OutputDevice6202(commonDevice);
	}else
		throw AdlExcept(
			"Unsupported device",
			"Sorry, Adlink PSDASK Device supported for AnalogOutput"
			" by this server are just PCI-6208V, PCI-6208A and PCI-6202",
			"AdlDeviceFactorySingleton::create_output_device_psdask",
			0);
	return 0;
}
