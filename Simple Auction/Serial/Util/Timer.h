/********************************************************************
|  *****     File:   Timer.h                                 *****  |
|  *****  Created:   July 23, 2018, 01:46 AM                 *****  |
********************************************************************/

#pragma once
#include <sys/time.h>
#include <random>

using namespace std::chrono;

class Timer
{
	public:
		Timer() : total_us(0), total_clocks(0), counting(false) {};//constructor

		void start();
		void stop();
		unsigned long real_ms_total();
		unsigned long real_ms_current();	
		unsigned long cpu_ms_total();	
		unsigned long cpu_ms_current();	
		void add(Timer &other);
		double timeReq();
		auto _timeStart( );
		
		float_t _timeStop( auto start_time );	

		~Timer(){};
	protected:
		std::chrono::time_point<std::chrono::system_clock> start_time;
	 	unsigned long total_us;
	 	clock_t total_clocks;
	 	clock_t start_clocks;
	 	bool counting;
};

