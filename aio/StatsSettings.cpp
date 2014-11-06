
#include "StatsSettings.h"

#include <sstream>
#include <iostream>

StatsSettings::StatsSettings()
{
	this->set_config("");
}

void StatsSettings::set_config(const std::vector<std::string>& conf_vec)
{
	std::stringstream ss;
	std::vector<std::string>::const_iterator i = conf_vec.begin(), e = conf_vec.end();
	for (; i != e; ++i)
		ss << *i << "\n";
	this->set_config(ss.str());
}

void StatsSettings::set_config(const std::string& conf)
{
	last_mean_enabled = false;
	last_std_dev_enabled = false;
	last_quadratic_mean_enabled = false;

	buf_mean_enabled = false;
	buf_std_dev_enabled = false;
	buf_quadratic_mean_enabled = false;

	event_last_mean_enabled = false;
	event_last_std_dev_enabled = false;
	event_last_quadratic_mean_enabled = false;

	event_buf_mean_enabled = false;
	event_buf_std_dev_enabled = false;
	event_buf_quadratic_mean_enabled = false;

	buf_mean_data_ready_enabled= false;
	event_buf_mean_data_ready_enabled = false;

	std::stringstream ss(conf);
	std::string op, op_old;
	
	while (ss >> op) {
		bool ok = false;
#	define __SS_READ_CONF_(var_name) \
		if (op == #var_name) { \
			var_name##_enabled = true; \
			ok = true;\
		}else if ( (op == "event") && (op_old == #var_name)) {\
			event_##var_name##_enabled = true;\
			ok = true;\
		} else (void)0

			__SS_READ_CONF_(last_mean);
			__SS_READ_CONF_(last_std_dev);
			__SS_READ_CONF_(last_quadratic_mean);

			__SS_READ_CONF_(buf_mean);
			__SS_READ_CONF_(buf_std_dev);
			__SS_READ_CONF_(buf_quadratic_mean);

			__SS_READ_CONF_(buf_mean_data_ready);


#	undef __SS_READ_CONF_

		if (!ok) {
			/// @todo
			std::cerr << "PROBLEMS PARSING StatsSettings" << std::endl;
		}
		op_old = op;
	}
}


std::string StatsSettings::get_config() const
{
	std::stringstream ss;

#	define __SS_WRITE_CONF_(var_name) \
		if (var_name##_enabled) { \
			ss << #var_name; \
			if (event_##var_name##_enabled) \
				ss << " event"; \
			ss << std::endl; \
		} else (void)0

			__SS_WRITE_CONF_(last_mean);
			__SS_WRITE_CONF_(last_std_dev);
			__SS_WRITE_CONF_(last_quadratic_mean);

			__SS_WRITE_CONF_(buf_mean);
			__SS_WRITE_CONF_(buf_std_dev);
			__SS_WRITE_CONF_(buf_quadratic_mean);

			__SS_WRITE_CONF_(buf_mean_data_ready);



#	undef __SS_WRITE_CONF_
	return ss.str();
}

#ifdef __AUTOTEST__StatsSettings__
/*
g++ aio/StatsSettings.cpp -D__AUTOTEST__StatsSettings__ -o StatsSettings
*/
	int main()
	{
		StatsSettings test, test2;

		test.last_mean_enabled = true;
		test.event_last_mean_enabled = true;
		test.buf_quadratic_mean_enabled = true;
		test.event_buf_quadratic_mean_enabled = true;
		test.last_std_dev_enabled = true;

		test2.set_config(test.get_config());

		if ( test.get_config() != test2.get_config() )
			return -1;

		std::cout << "OK!!:" << std::endl;
		std::cout << test.get_config() << std::endl;
		std::cout << "-------\n" << test2.get_config() << std::endl;

		return 0;
	}
#endif // __AUTOTEST__StatsSettings__
