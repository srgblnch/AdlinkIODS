
#pragma once

#include <vector>
#include <string>

class StatsSettings {
	public:
		StatsSettings();
		~StatsSettings() {}

		void set_config(const std::vector<std::string>& conf_vec);
		void set_config(const std::string &conf);
		std::string get_config() const;

		bool last_mean_enabled;
		bool last_std_dev_enabled;
		bool last_quadratic_mean_enabled;

		bool buf_mean_enabled;
		bool buf_std_dev_enabled;
		bool buf_quadratic_mean_enabled;

		bool event_last_mean_enabled;
		bool event_last_std_dev_enabled;
		bool event_last_quadratic_mean_enabled;

		bool event_buf_mean_enabled;
		bool event_buf_std_dev_enabled;
		bool event_buf_quadratic_mean_enabled;

		bool event_buf_mean_data_ready_enabled;
		bool buf_mean_data_ready_enabled;


		bool event_any_enabled() const
		{
			return event_last_mean_enabled
				|| event_last_std_dev_enabled
				|| event_last_quadratic_mean_enabled
				|| event_buf_mean_enabled
				|| event_buf_std_dev_enabled
				|| event_buf_quadratic_mean_enabled
				|| event_buf_mean_data_ready_enabled;
		}

		bool any_enabled() const
		{
			return last_mean_enabled
				|| last_std_dev_enabled
				|| last_quadratic_mean_enabled
				|| buf_mean_enabled
				|| buf_std_dev_enabled
				|| buf_quadratic_mean_enabled;
		}
};


