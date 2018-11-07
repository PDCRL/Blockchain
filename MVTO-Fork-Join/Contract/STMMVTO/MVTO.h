#ifndef MVTO_H
#define MVTO_H
#include "tbb/concurrent_hash_map.h"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tbb_allocator.h"
#include "STM.cpp"
#include <fstream>
#include <set>
using namespace tbb;
using namespace std;


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Structure to store conf_list corresponding to each transaction. !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
struct tID_confList
{
	int TID;
	list<int> conf_list;
};

/*defines the hash function and compare function for cList table used*/
struct HashComp
{
	static size_t hash(const int& x)                                  // hash function
	{
		size_t h = 0;
		h = x;
		return h;
	}
	static bool equal(const int& x,const int& y)                      // "==" operator over loading
	{
		if(x == y) return true;
		else return false;
	}
};
typedef concurrent_hash_map< int, list<int>, HashComp > c_LIST;



struct MVTO_version_data
{
	void* value;
	int max_read;
	list<int> read_list;
	MVTO_version_data()
	{
		value = NULL;
	}
	~MVTO_version_data()
	{
		delete [] ((char*)value);
		read_list.clear();
	}
};

struct MVTO_tOB : public common_tOB
{
	/*! Version list for each data item stored using hash map,
	 * where key is ID and value is value of data item. */
	map<int, MVTO_version_data*> MVTO_version_list;

	//!frees memory allocated to version list
	~MVTO_tOB()
	{
		map<int,MVTO_version_data*>::iterator itr = MVTO_version_list.begin();
		for(; itr != MVTO_version_list.end(); itr++)
			delete(itr->second);//!invokes mvto_version_data destructor

		map< int, MVTO_version_data* >::iterator begin_itr = MVTO_version_list.begin();
		map< int, MVTO_version_data* >::iterator end_itr   = MVTO_version_list.end();
		MVTO_version_list.erase(begin_itr, end_itr);
	}
};


/*! Used for local buffer of transactions buffer_prameter
 *  value 0 => only read; 1 => written
*/
struct local_tOB : public common_tOB
{
	int buffer_param;
};


//! Function for comparing tOBs based on their IDs. Used for sorting write_buffer.
bool compare_local_tOB (const common_tOB* first, const common_tOB* second)
{
	return (first->ID < second->ID);
}


class MVTO : public virtual STM 
{
private:
	//! lock used for atomicity of create_new method.
	mutex create_new_lock;
	
	//! lock used for safe access to live set.
	mutex live_set_lock;
	
	//! set of live transactions.
	set<int> sh_live_set;

public:
	std::fstream log;
	
	//! c_List is list of all t_id corresponding conflict lists.
	list<tID_confList>cList;
	
	//! Shared table of conflict list of all the commited transactions.
	c_LIST gConfList;
	
	//! used to start and stop garbage collection.
	bool gc_active;
	
	//! default Constructor.
	MVTO();
	
	//! constructor with garbage collection frequency as user input.
	MVTO(int freq);
	
	//! default memory initializer.
	int initialize();
	
	//! takes vector of data object sizes as input and allots memory.
	int initialize( vector<int> sizes);
	
	//! input => sizes and IDs of initial data objects.
	int initialize( vector<int> initial_tobs,vector<int> sizes);
	
	//! Copy constructor.
	MVTO(const MVTO& orig);
	
	//! Destructor.
	virtual ~MVTO();
	
	trans_state* begin();

	/*----------------------------------------------------------*/
	int try_commit_conflict(trans_state* trans, long long& error_tOB, list<int> &conf_list);
	list<int> get_conf(long long TID);
	/*----------------------------------------------------------*/

	int read (trans_state* trans,common_tOB* tb);
	int write(trans_state* trans,common_tOB* tb);
	int try_commit(trans_state* trans,long long& error_tOB);
	void try_abort(trans_state* trans);
	int create_new(long long ID,int size) ;
	int create_new(int size,long long& ID) ;
	int create_new(long long& ID);
	void end_library();
	void gc();
	void activate_gc();
};

#endif /* MVTO_H */
