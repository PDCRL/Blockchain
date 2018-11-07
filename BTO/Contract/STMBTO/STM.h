/*
 * File:     STM.h
 * Modified: July 23, 2018, 01:46 AM
 */


#ifndef STM_H
#define	STM_H
#include <list>
#include "tbb/concurrent_hash_map.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include <atomic>
#include <map>
#include <vector>
#include <mutex>
using namespace std;
using namespace tbb;

const int INITIAL_DATAITEMS   = 10; // used by default initializer to create data objects initially
const int DEFAULT_DATA_SIZE   = 4;  // default size of data objects
const int WRITE_PERFORMED     = 1;  // value given to buffer parameter of local buffer to indicate that write occured on the data object
const int READ_PERFORMED      = 0;  // value given to buffer parameter of local buffer to indicate that only read occured on the data object

//error codes for different errors
const int SUCCESS             =  0; // indicates success
const int FAILURE             = -1; // indicates failure
const int MEMORY_INSUFFICIENT = -2; // indicates that memory is insufficient for new data objects creation
const int INVALID_TRANSACTION = -3; // indicates that protocol specific validation failed for given transaction
const int TOB_ABSENT          = -4; // indicates that required data object does not exist in shared memory
const int SIZE_MISMATCH       = -5; // indicates that sizes of existing and required data objects does not match
const int TOB_ID_CLASH        = -6; // indicates that the id with which we want to create new data object already exists in shared memory

//garbage collection frequency parameters
const int LOW_FREQ    = 100000;     // (run gc once in every 100000 microseconds)
const int MEDIUM_FREQ = 50000;
const int HIGH_FREQ   = 1000;



//Transaction object
struct common_tOB
{
	long long ID; // unique identifier
	void* value;  // holds the value
	int size;     // size of data object
	common_tOB()
	{
	value = NULL;
	}
	~common_tOB()
	{
	delete [] ((char*)value);
	}
};


//Transaction class
struct trans_state
{
	int TID;                                   //unique ID
	map<int, common_tOB*> local_buffer;        //local buffer for storing local writes
	int read_flag;                             //To check if transaction is write only
	
	~trans_state()                             //trans_state destructor frees memory allocated to write buffer
	{
		map<int, common_tOB*>::iterator tb;
		for (tb = (local_buffer).begin(); tb != (local_buffer).end(); tb++)
		{
			delete(tb->second);                // invokes tOB destructor
		}
		map<int,common_tOB*>::iterator begin_itr = local_buffer.begin();
		map<int,common_tOB*>::iterator end_itr   = local_buffer.end();
		local_buffer.erase(begin_itr, end_itr);
	}
};


/*defines the hash function and compare function for hash table used*/
struct MyHashCompare
{
	static size_t hash(const int& x)           // hash function
	{
		size_t h = 0;
		h = x;
		return h;
	}
   static bool equal(const int& x,const int& y)// "==" operator over loading
   {
     if(x==y)
	 return true;
	 else
	 return false;
   }
};


typedef concurrent_hash_map< int, common_tOB*, MyHashCompare > hash_STM;


//Interface for STM library. Methods are all implemented in protocols which inherit this library
class STM 
{
	public:
	int gc_interval;                                                   //garbage collection interval; set in STM constructor depending on user input
	std::atomic<int> sh_count;                                         //keeps count of transactions for allocating new transaction IDs
	long long max_tob_id;                                              //keeps track of maximum tob id created
	hash_STM  sh_tOB_list;                                             //shared memory
	virtual int initialize();                                          //default memory initializer
	virtual int initialize(vector<int> sizes);                         //takes vector of data object sizes as input and allots memory
	virtual int initialize(vector<int> initial_tobs,vector<int> sizes);//input => sizes and IDs of initial data objects
	virtual trans_state* begin();
	virtual int read(trans_state* trans,common_tOB* tb);
	virtual int write(trans_state* trans,common_tOB* tb);
	virtual int try_commit(trans_state* trans,long long& error_tOB);
    
	/*----------------------------------------------------------*/
	virtual int get_RItr(trans_state* trans, common_tOB* tb, vector<int>&read_list);
	virtual int get_WItr(trans_state* trans, common_tOB* tb, vector<int>&write_list);
	virtual int try_commit_conflict(trans_state* trans, long long& error_tOB, list<int> &conf_list);
	/*----------------------------------------------------------*/
    
	virtual void try_abort(trans_state* trans);
	virtual int  create_new(long long ID,int size);
	virtual int  create_new(int size,long long& ID);
	virtual int  create_new(long long& ID);
	virtual void end_library();
	virtual void gc();
	STM();
	STM(int freq);                                                     //freq is the required frequency of garbage collection
	STM(const STM& orig);                                              //copy constructor
    
	virtual ~STM();
};

//mutex copyBytes_lock;
void copyBytes( void *a, void *b, int howMany )
{
	//copyBytes_lock.lock();
	int i;
	char* x = (char*) a;
	char* y = (char*) b;
	for( i  = 0; i<howMany; i++)
	{
		*(x+i) = *(y+i);
	}
	//copyBytes_lock.unlock();
}

#endif	/* STM_H */
