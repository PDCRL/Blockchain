/*
 * File:    STM.cpp
 * Modified July 23, 2018, 01:46 AM
 */
 
#include "STM.h"
using namespace std;

	STM::STM()
	{
		gc_interval = MEDIUM_FREQ; // garbage collection frequency is set to default medium frequency
		sh_count    = 1;           //shared variable
		max_tob_id  =-1;
	}
	/*STM constructor
	freq is the required frequency of garbage collection
	*/
	STM::STM(int freq)
	{
		/*
		if(freq==HIGH_FREQ || freq==MEDIUM_FREQ ||   freq==LOW_FREQ)
		{
			gc_interval=freq;
		}
		else // for any other invalid input, frequency is set to default medium frequency
		{
			gc_interval=MEDIUM_FREQ;
		}*/
		gc_interval =  freq;
		sh_count    =  0;          //shared variable
		max_tob_id  = -1;
	}
  
	int STM::initialize() { }                                          //default memory initializer
  
	int STM::initialize(vector<int> sizes){ }                          //takes vector of data object sizes as input and allots memory
 
	int STM::initialize(vector<int> initial_tobs,vector<int> sizes) {} //input => sizes and IDs of initial data objects

	trans_state* STM::begin() { }

	int STM::read(trans_state* trans,common_tOB* tb) { }               //return int denotes success or failure , interchange parameters for convention
  
	int STM::write(trans_state* trans,common_tOB* tb) { }

	int STM::try_commit(trans_state* trans,long long& error_tOB) { }
  
	/*----------------------------------------------------------*/
	int STM::get_RItr(trans_state* trans, common_tOB* tb, vector<int> &read_list) { }

	int STM::get_WItr(trans_state* trans, common_tOB* tb, vector<int> &write_list) { }

	int STM::try_commit_conflict(trans_state* trans, long long& error_tOB, list<int> &conf_list) { }
	/*----------------------------------------------------------*/
  
	void STM::try_abort(trans_state* trans) { }
  
	int STM::create_new(long long ID,int size) { }                     //creates new data item in shared memory with specified ID,size and default value
  
	int STM::create_new(int size,long long& ID) { }                    //creates new data item in shared memory with specified ID and default value

	int STM::create_new(long long& ID) { }                             //creates default data item in shared memory
  
	void STM::gc() { }
  
	void STM::end_library() { }
  
	STM::STM( const STM& orig ) { }

	STM::~STM(){ }
