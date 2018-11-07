/********************************************************************
|  *****     File:   Timer.cpp                               *****  |
|  *****  Created:   July 23, 2018, 01:46 AM                 *****  |
********************************************************************/

#include "Timer.h"
#include <cassert>

using namespace std::chrono;


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!! GIVES EXPONENTIALLY DISTRIBUTED RANDOME TIME FOR THREAD WAITING  !!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*chrono::duration <double> Timer::_rand_Time( double lemda )
{
	int seed = chrono::system_clock::now( ).time_since_epoch( ).count( );
	default_random_engine generator ( seed );
	exponential_distribution <double> distribution ( lemda );
	double num = distribution ( generator );
	chrono::duration <double> time ( num );
	return ( time );
}
*/
/*!!!!!!!!!!!!!!!!!!!!*/
/*!!! TO GET TIME  !!!*/
/*!!!!!!!!!!!!!!!!!!!!*/
double Timer::timeReq()
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	double timevalue = tp.tv_sec + (tp.tv_usec/1000000.0);
	return timevalue;
}

/*!!!!!!!!!!!!!!!!!!!*/
/*!!! TO GET TIME !!!*/
/*!!!!!!!!!!!!!!!!!!!*/
auto Timer::_timeStart( )
{
	auto beginCollect = high_resolution_clock::now( ); /*Statrt clock to get time taken by this transaction*/
	return ( beginCollect );
}


/*!!!!!!!!!!!!!!!!!!!*/
/*!!! TO GET TIME !!!*/
/*!!!!!!!!!!!!!!!!!!!*/
float_t Timer::_timeStop( auto start_time )
{
	auto finish_time = high_resolution_clock::now();
	float_t time_taken = duration_cast<microseconds>( finish_time - start_time ).count();/*use for milli sec <::milliseconds>*/
	return ( time_taken );
}



void Timer::start() 
{
	start_clocks = clock();
	start_time  = system_clock::now();
	assert(!counting);
	counting = true;
}

void Timer::stop() 
{
	clock_t end_clocks = clock();
	auto end_time = system_clock::now();
	assert(counting);
	counting = false;

	total_clocks += (end_clocks - start_clocks);
	total_us += duration_cast<microseconds>(end_time - start_time).count();
}

unsigned long Timer::real_ms_current() 
{
	if (!counting) return 0;
	auto current_time = system_clock::now();
	return duration_cast<microseconds>(current_time - start_time).count() / 1000;
}

unsigned long Timer::cpu_ms_current() 
{
	if (!counting) return 0;
	clock_t current_clocks = clock();
	return 1000 * (current_clocks - start_clocks) / CLOCKS_PER_SEC;
}

unsigned long Timer::real_ms_total() 
{
	return total_us / 1000 + real_ms_current();
}

unsigned long Timer::cpu_ms_total() 
{
	return 1000 * total_clocks / CLOCKS_PER_SEC + cpu_ms_current();
}

void Timer::add(Timer &other) 
{
	assert(!other.counting);

	total_clocks += other.total_clocks;
	total_us += other.total_us;
}
