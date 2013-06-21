
#include "AdlinkDeviceThread.h"
#include "InputBehaviour.h"


/// Entry point for the threading code
/// Asks the device to prepare and informs
/// start_thread() when its finished. Then,
/// asks the device man thread loop to run
void* AdlinkDeviceThread::run_undetached(void *data)
{
	this->error = this->device->thread_init();
	initDone.post(); // Initialization process done
	if (this->error != NoError)
		return 0;

	this->stopThread = false;
	this->device->thread_body(this->stopThread);
	return 0;
}


/// Stop a thread
/*static*/ void AdlinkDeviceThread::stop_thread(AdlinkDeviceThread *& thread)
{
	assert(thread);
	void *foo;

	// Tell the thread loop to stop iterating.
	thread->stopThread = true;
	// Maybe the thread loop is not runnign but waiting for
	// a semaphore/mutex? If this is the case, this function
	// should wake him up, so it can realize that we told
	// him to stop.
	thread->device->thread_stopping();
	// No need for delete, omnithread does this for us,
	// after waiting for the thread end.
	thread->join(&foo);
	thread = 0;
}

/// Create a thread to perform continuous adquisition
/// over a device
/*static*/ AdlinkDeviceThread* AdlinkDeviceThread::start_thread(InputBehaviour* device)
{
	assert(device);

	// Build the thread object...
	AdlinkDeviceThread * thread = new AdlinkDeviceThread();
	thread->device = device;
	thread->error = NoError;
	// Run the thread
	thread->start_undetached();
	// Wait until init thread method is completed
	thread->initDone.wait();
	// if the initialization finished without sucess,
	// destroy the thread and return 0
	if (thread->error != NoError) {
		void *foo;
		thread->join(&foo);
		return 0;
	}
	return  thread;
}
