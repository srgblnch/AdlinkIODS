
import sys

devName = "LT01/DI/ADC-BCM-01"
attrName = "C01_ChannelValues"
resDir = 'results'

if len(sys.argv) < 3:
	print 'You must specify 2 parameters: tango/device/name attributeName'
	sys.exit(-1)

devName = sys.argv[1]
attrName = sys.argv[2]

import PyTango
import pickle
import traceback
import os
from datetime import datetime





class XCallback:
	"""
		To get change events form tango we need a class with a 'push_event'
		method. We just want simpler callbacks, no a new class for each, so
		that's what XCallback is for: Generate meta-callback classes.
	"""
	def __init__(self, fn):
		self._fn = fn
	def push_event(self, event):
		self._fn(event)

class CallbackSubscriptions:
	"""
		Just to manage (subscribe and unsuscribe) events, making sure
		we can unsubscribe from all. Provides a simplified callback
		interface receiving a function instead of a class thanks to
		XCallback.
	"""
	def __init__(self):
		self._empty()

	def _empty(self):
		self._callbacks = []
		self._events = []
		self._eventDevs = []

	def add(self, dev, attrName, fn):
		cb = XCallback(fn)
		ev = dev.subscribe_event(attrName, PyTango.EventType.CHANGE, cb, [])
		self._callbacks.append(cb)
		self._events.append(ev)
		self._eventDevs.append(dev)

	def delete_all(self):
		for x in xrange(len(self._events)):
			self._eventDevs[x].unsubscribe_event(self._events[x])
		self._empty()

def on_adc_volts_change(event):
	global resDir
	try:
		v = event.attr_value
		#print dir(v.time)
		#print v.time.tv_sec, v.time.tv_usec, v.time.tv_nsec
		t = v.time.tv_sec + (v.time.tv_usec*1e-6)
		t = datetime.fromtimestamp(t)
		#fn = resDir + "/%010d%06d" % ( v.time.tv_sec, v.time.tv_usec )
		fn = '%02d:%02d:%02d.%06d'%(t.hour, t.minute, t.second, t.microsecond)
		print fn
		f = file(resDir + '/' + fn, 'w')
		pickle.dump(v.value[:v.dim_x], f)
		f.close()
	except:
		print 'error'
		print traceback.format_exc()
		pass
	#if event.err:
		#print 'error!'
		#return
	#else:
		#v = event.attr_value.value[:event.attr_value.dim_x]
		#try:
			#charge = self.volts_list_2_mean_charge(event.attr_value.value)
			#self.push_change_event("Charge", charge)
			#self._cantPushAlarm = False
			#self.update_state()
		#except:
			#msg = "Unable to push change event on Charge"
			#msg += "\n\n" + traceback.format_exc()
			#self._cantPushAlarm = True
			#self.go_alarm(msg)



os.system("mkdir -p " + resDir )
cs = CallbackSubscriptions()
d = PyTango.DeviceProxy(devName)
cs.add(d, attrName, on_adc_volts_change)

while True:
	pass
