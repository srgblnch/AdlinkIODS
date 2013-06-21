
#pragma once

#include "AdlinkCommon.h"
#include <sstream>

///
/// OutputBehaviour is not really expecting any special interface from
/// OutputDeviceInterface other than function handler(), because it uses
/// the Adlink APIs directly.
/// 
/// This class is defined just to make clear the purpose of DevicePartInterface.
/// And may be in the future OutputBehaviour will be ported to an extended
/// OutputDeviceInterface...
///
class OutputDeviceInterface : public DevicePartInterface
{
	public:
	OutputDeviceInterface(CommonDeviceInterface* commonDevice) :
		DevicePartInterface(commonDevice)
	{}


        inline void __adlink_assert(I16 r, const char* fnCall, const char* fn) const
        {
                if (r != NoError) {
                        std::stringstream ss;
                        ss      << "+ " << fnCall << " in " << fn
                                << " FAILED with code " << r << ": "
                                << pcisdask_error_text(r) << std::endl;
                        // status_warn() << ss.str() << std::endl; NO need to do so, it's
                        // done by throw_error itself
                        throw_error("Adlink ERROR", ss.str(), fn);
                }
        }

  virtual bool uses_wfm_generation() const
  {	return false; }

  virtual void set_wfm_group(U16* chanBufIDs, U16 nchannels)
  {
	  assert(false);
  }

  virtual bool requires_wfm_trick() const
  {
	return false;
  }


virtual void wfm_group_update(U16* pwBuffer)
{ throw "todo error"; }



	virtual bool supports_continuous_output() const = 0;











  virtual void cont_buffer_compose(U16 wChannel, U32 dwUpdatecount, void* conBuffer, void* buffer) =  0;


  virtual U32 initial_memory_allocated_kb() =  0;

  virtual U16 cont_buffer_setup(void* buffer, U32 writeCnt) =  0;

  virtual void config(U16 configCtrl, U16 trgCtrl, U32 reTrgCnt, U32 DLY1Cnt, U32 DLY2Cnt, bool autoResetBuf) =  0;




  virtual void async_check(bool &isStopped, U32 &accesCnt) =  0;

  virtual U32 async_clear() =  0;

  virtual void reset_buffers() =  0;

  virtual void configure_channel_range(U16 channel, U16 range_id) =  0;

  virtual void write_channel(U16 channel, U16 value) =  0;

  virtual void cont_write_multi_channels(U16 nchannels, U16* chanBufIDs, U16 bufId, U32 writeCnt, U32 iterations, U32 CHUI, bool infinite, U16 syncMode) =  0;

};

