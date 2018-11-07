/*
 * File:     BTO.h
 * Modified  July 23, 2018, 01:46 AM
 */

#ifndef BTO_H
#define	BTO_H
#include "tbb/concurrent_hash_map.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tbb_allocator.h"
#include "STM.cpp"
#include <iterator>
#include <vector>


using namespace tbb;
using namespace std;

//timestamps: structure which holds the max_read,max_write, read_list, and write_list of each data item
struct timestamps
{
	int max_read;
	int max_write;
	list<int> *r_list; //readers list local to dataitem:: stores Transaction Id	
	list<int> *w_list; //writers list local to dataitem:: stores Transaction Id
};


struct BTO_tOB : public common_tOB
{
	void* BTO_data;   //for adding protocol specific data
	BTO_tOB()
	{
		BTO_data=NULL;
	}
	~BTO_tOB()
	{
		delete [] ((char*)BTO_data);
	}
};


/*
	used for local buffer of transactions
	buffer_prameter value 0 => only read
	1 => written
*/
struct local_tOB : public BTO_tOB
{
	int buffer_param;
};



//Function for comparing tOBs based on their IDs. Used for sorting write_buffer
bool compare_local_tOB(const common_tOB* first, const common_tOB* second)
{
	return (first->ID < second->ID);
}


class BTO : public virtual STM 
{
	private:
		mutex create_new_lock;                                         //lock used for atomicity of create_new method
	
	public:
		BTO();                                                         //default Constructor
		int initialize();                                              //default memory initializer
		int initialize(vector<int> sizes);                             //takes vector of data object sizes as input and allots memory
		int initialize(vector<int> initial_tobs,vector<int> sizes);    //input => sizes and IDs of initial data objects
		BTO(const BTO& orig);                                          //Copy constructor
		virtual ~BTO();                                                //Destructor
		trans_state* begin();
    
		int read(trans_state* trans,common_tOB* tb);
		int write(trans_state* trans,common_tOB* tb);
		int try_commit(trans_state* trans,long long& error_tOB);
    
		/*----------------------------------------------------------*/
		int get_RItr(trans_state* trans, common_tOB* tb, vector<int> &read_list);
		int get_WItr(trans_state* trans, common_tOB* tb, vector<int> &write_list);
		int try_commit_conflict(trans_state* trans, long long& error_tOB, list<int> &conf_list);
		/*----------------------------------------------------------*/
    
		void try_abort(trans_state* trans);
		int create_new(long long ID,int size) ;
		int create_new(int size,int& ID) ;
		int create_new(int& ID);
};

#endif	/* BTO_H */
