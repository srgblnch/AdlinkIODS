

#ifndef _____ADLINK_DEVICE_THREAD___H__SDDFGgf_Ferjr93fDF___
#define _____ADLINK_DEVICE_THREAD___H__SDDFGgf_Ferjr93fDF___
#include <d2kdask.h>
#include <omnithread.h>

class InputBehaviour;

class AdlinkDeviceThread : private omni_thread
{
	void * run_undetached();

	/// Device doing the work
	InputBehaviour*		device;
	/// Until the thread has been created
	/// AND device setting up is finished to
	/// get continuous input, start_thread() method
	/// wont return. This is intended to
	/// synchronize this
	omni_semaphore		initDone;
	/// Error given by the initialization method.
	I16					error;
	/// The thread loop must look at this value
	/// and stop when set to true
	bool				stopThread;

	/// Private constructor/destructor, we want objects to
	/// be build using start_thread() and
	/// stop_thread() static methods
	AdlinkDeviceThread() : initDone(0) {}
	~AdlinkDeviceThread() {}

	/// Entry point for the threading code
	/// Asks the device to prepare and informs
	/// start_thread() when its finished. Then,
	/// asks the device man thread loop to run
	void* run_undetached(void *data);
public:

	/// Stop a thread
	static void stop_thread(AdlinkDeviceThread *& thread);

	/// Create a thread to perform continuous adquisition
	/// over a device
	static AdlinkDeviceThread* start_thread(InputBehaviour* device);
};


#endif //_____ADLINK_DEVICE_THREAD___H__SDDFGgf_Ferjr93fDF___
