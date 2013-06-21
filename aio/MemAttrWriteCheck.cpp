
namespace AdlinkAIO_ns
{

///
/// This whole file is just a nasty trick to simulate a missing Tango feature.
/// 
/// What I want to accomply is to be able to run some code right after all
/// the memorized attributes have been set on initialization time. More
/// precisely, in this case I am implementing the "AutoStart" property:
/// It means that I want to run "start" command when the device is loaded,
/// but I cannot do it on "init_device" because acquisition parameters depend
/// on memorized attribute values.
/// 
/// In init_device we just call MemAttrWriteCheck::init() which tryes to
/// figure out which attributes are memorized and will be written. This
/// list is written in a list in the device.
/// 
/// Then all write attributes must check this list when written. If they are
/// on the list, delete themselves from it. If the list becomes empty after
/// this deletion, then auto-start should be performed.
/// 
/// Obviously, it should be done at the END of each write_attribute* function.
/// The fact that it can have multiple end points (plus exceptions) made me
/// wonder how to make it always easy to write it. So, all that the write_*
/// function has to do is define an object of class MemAttrWriteCheck at
/// the very begining. It's destructor will handle it all.
/// 
/// Once compiled, in normal operation (when auto-start has been performed) it
/// results in two pointers allocated in the stack (Does the compiler optimize
/// them over? It really can...) and a simple 'if' at the end of all
/// write_* functions, so not much overhead is introduced.
/// 
/// However, I expect tango support for this kind of things some day.
/// 
/// @todo If tango gets support to run a method just after initialization
/// of memorized attributes, use it and get rid of this trick to
/// accomplish "AutoStart".
///
class MemAttrWriteCheck
{
public:
	typedef AdlinkAIO TheDeviceType;
	typedef std::set<std::string> StringSet;

private:
	const Tango::WAttribute & m_attr;
	TheDeviceType* const m_device;	

	static void _start_now(TheDeviceType* const device)
	{
	  // If all expected attributes are already written
		if ( device->m_expectedMemorizedAttrs.empty() ) {
	
		  // The first thing ~MemAttrWriteCheck() does is
		  // check the value of this property. Setting it to false
		  // we can return from there after the very first condition
		  // check. And it does not matter, as it will be set back to 1
		  // on init_device when the properties are read, so
		  // there's no problem in doing it this way here.
			device->autoStart = false;
	
		  // Now start
			try {
				std::cout << "Auto starting device..." << std::endl;
				device->start();
			} catch (...) {
				std::cerr	<< "There has been some problems trying to"
							<< "Auto start the device" << std::endl;
			}
		}
	}

	// We must take note of the attributes at the begining because on the
	// process of of writing its set values... Tango sets them all temporaly
	// as NOT memorized!!
	static void _init_expected_attributes(TheDeviceType* device)
	{
		vector<long> &att_list = device->get_device_attr()->get_w_attr_list();
		
		device->m_expectedMemorizedAttrs.clear();
	
		long nb_wr = 0;
		for (unsigned long j = 0;j < att_list.size();j++)
		{
			Tango::WAttribute &attr =
					device->get_device_attr()->get_w_attr_by_ind(att_list[j]);
	
			if (! ( attr.is_memorized() && attr.is_memorized_init() ) )
				continue;
	
			string &mem_value = attr.get_mem_value();
			if (mem_value == MemNotUsed)
				continue;
	
			device->m_expectedMemorizedAttrs.insert(attr.get_name());
		}
	}
public:
	inline MemAttrWriteCheck(
			TheDeviceType* dev,
			const Tango::WAttribute & attr) :
				m_attr(attr), m_device(dev)
	{
// 		std::cout << " MemAttrWriteCheck( " << ((Tango::WAttribute &)attr).get_name() << ")" << std::endl;
	}

	static void init(TheDeviceType* device)
	{
		if ( device->autoStart ) {
			_init_expected_attributes(device);
			_start_now(device);
		}
	}

	inline ~MemAttrWriteCheck()
	{
		if ( !m_device->autoStart )
			return;
		
	  // If all expected attributes are already written
		if ( m_device->m_expectedMemorizedAttrs.empty() )
			// Process is already finished, start has already being run
			return;

	  // Mark attribute as written
		const std::string attrName = 
							const_cast<Tango::WAttribute &>(m_attr).get_name();
		m_device->m_expectedMemorizedAttrs.erase( attrName);

		_start_now(m_device);
	}
}; // class MemAttrWriteCheck

} // namespace AdlinkAIO_ns
