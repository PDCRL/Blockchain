#include <iostream>
#include <list> 
#include <set>
#include "MVTO.h"

using namespace std;
using namespace tbb;

/*!!!!!!!!!!!!!!!!!!!!!
! Default Constructor !
!!!!!!!!!!!!!!!!!!!!!*/
MVTO::MVTO() : STM()
{
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! constructor with garbage collection frequency as parameter !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
MVTO::MVTO(int freq) : STM(freq)
{
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! garbage collection thread that runs as long as gc_active variable is set !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
void* garbage_collector(void *lib)  
{
	while( ( (MVTO*)lib )->gc_active )
	{
		//cout<<"gc active\n";
		((MVTO*)lib)->gc();
		usleep(((MVTO*)lib)->gc_interval);
	}
	pthread_exit(NULL);
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! starts garbage collection thread !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
void MVTO::activate_gc()
{
	gc_active = true;
	pthread_t gc_thread = 0;
	pthread_create( &gc_thread, NULL, garbage_collector, (void*)this);
}

/*------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!! initialize() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*------------------------------------------------------------------*/
/*!   memory initializer with only data object sizes as parameters  */
/*------------------------------------------------------------------*/
int MVTO::initialize(vector<int> sizes)
{
	activate_gc();
	for(int i = 0; i < sizes.size(); i++)
	{
		long long ID;
		int result = create_new(sizes[i], ID);
		if(result != SUCCESS)
			return result;
	}
	return SUCCESS;
}
/*------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!! initialize() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*------------------------------------------------------------------*/
/*!Memory initializer with data objects' sizes and IDS as parameters*/
/*------------------------------------------------------------------*/
int MVTO::initialize(vector<int> initial_tobs, vector<int> sizes)
{
	activate_gc();
	for(int i = 0; i < initial_tobs.size(); i++)
	{
		int result = create_new(initial_tobs[i], sizes[i]);
		if(result != SUCCESS && result < 0)
			return result;
	}
	return SUCCESS;
}
/*----------------------------*/
/*!!!!!!! initialize() !!!!!!!*/
/*----------------------------*/
/* Default memory initializer */
/*----------------------------*/
int MVTO::initialize()
{
	activate_gc();
	log.flush();
	log.open("log_file.txt",ios_base::out);                          //ios::out, filebuf::sh_write;
	log<<"initialize successful"<<endl;
	for(int i = 0; i < INITIAL_DATAITEMS; i++)
	{
		long long ID;
		int result = create_new(ID);
		if(result != SUCCESS)
			return result;
	}
	return SUCCESS;
}

/*--------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! begin() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------*/
/*! Begin function creates a new trans_state object and sets the TID value by increme-  */
/*! -nting atomic variable "sh_count". The trans_state pointer is returned to the user  */
/*! which is used as argument for later operations like "read" , "write" , "try_commit" */
/*--------------------------------------------------------------------------------------*/
trans_state* MVTO::begin()
{
	trans_state* trans_temp = new trans_state;
	trans_temp->TID         = sh_count++;                            //incrementing the atomic variable.

	live_set_lock.lock();
	sh_live_set.insert(trans_temp->TID);
	live_set_lock.unlock(); 

	//log<<"succesfully began transaction "<<trans_temp->TID<<endl;
	return trans_temp;
}

/*------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! read() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*------------------------------------------------------------------------------------------*/
/*! Read function takes as arguments trans_state(contains TID for transaction) & common_tOB */
/*! (contains ID, value for a data_item). The user enters data_item to be read. If the read */
/*! operation is successful then, value of the data item is entered into tOB argument.      */ 
/*------------------------------------------------------------------------------------------*/
int MVTO::read( trans_state* trans, common_tOB* tb )
{
	log<<"transaction: "<<trans->TID<<" thread: "<<pthread_self()<<" entered read func and data item is"<<tb->ID<<endl;
	//!search for data item in local buffer
	const int id = tb->ID;
	if( (trans->local_buffer).find(id) != (trans->local_buffer).end() )
	{
		//!---------------------------------------------------------
		//!enters this block if data item is found in local buffer !
		//!---------------------------------------------------------
		if( tb->size != ((trans->local_buffer)[tb->ID])->size )
			return SIZE_MISMATCH;                                    //sizes of the existing and input data item did not match.
		if(tb->value == NULL)                                        //if the application did not allot memory to input.
		{
			tb->value = operator new(tb->size);
		}
		copyBytes( tb->value, ((trans->local_buffer)[tb->ID])->value, tb->size );//Returning the value to the user.
		return SUCCESS;
	}
	MVTO_tOB* tob_temp;
//	hash_STM::const_accessor a_tOB;
	hash_STM::accessor a_tOB;	
	//!Finds the data_item in the shared memory and acquires a lock on it.
	if( sh_tOB_list.find(a_tOB, tb->ID) ) 
		tob_temp = (MVTO_tOB*)(a_tOB->second);                       //if data object exists in shared memory lock and store pointer into tob_temp
	else
	{
		cout<<"error from read: data object with id= "<<(tb->ID)<<"does not exist"<<endl;
		a_tOB.release();
		return TOB_ABSENT;                                           //error code 2 indicates that required data object does not exist.
	}
	if( tb->size != tob_temp->size )
	{
		cout<<"size mismatch\n";
		a_tOB.release();
		return SIZE_MISMATCH;
	}

	//!----------------------------------------------------------------------
	//!read from latest trans with time stamp less than current transaction !
	//!----------------------------------------------------------------------
	//! lower_bound returns pointer to version greter then or equal to trans->TID in map.
	auto itr = (tob_temp->MVTO_version_list).lower_bound(trans->TID);

	//!just privious version (created by largest trans smaller then itself)
	itr--;
	//!--------------------------------------------------------------
	//!read the data object from shared memory and return its value !
	//!--------------------------------------------------------------
	if(tb->value == NULL) //! if the application did not allot memory to input.
		tb->value = operator new(tob_temp->size);

	if(itr == (tob_temp->MVTO_version_list).end())
	{
		log.flush();
		log<<"error here 1\n";
	}
	else if((itr->second)->value == NULL)
	{
		log.flush();
		log<<"error here 2\n";
	}

	//_________________________ Modification Start _____________________________

	//! instead read list update max_read here, read list (of all shared object 
	//! in read_set of this TID) will be updated in tryCommit().
	if( (itr->second)->max_read < trans->TID ) 
		(itr->second)->max_read = trans->TID;

	//!store (Tj) that created 'itr' version of shared object, 
	//! in read_confList of trans->TID (Ti).  
	trans->read_conflist.push_back(itr->first);

	//_________________________ Modification End _______________________________

	//! copying value from shared memory to user input.
	copyBytes(tb->value, (itr->second)->value, tb->size);

	a_tOB.release();

	//!updating local buffer.
	local_tOB* local_tob_temp     = new local_tOB;
	local_tob_temp->ID            = tb->ID;
	local_tob_temp->size          = tb->size;
	local_tob_temp->value         = operator new(tb->size);
	copyBytes(local_tob_temp->value, tb->value, tb->size );
	local_tob_temp->buffer_param  = READ_PERFORMED;
	(trans->local_buffer)[tb->ID] = local_tob_temp;
	
	
	//! updating read_buffer
	local_tOB* r_tob_temp        = new local_tOB;
	r_tob_temp->ID               = tb->ID;
	r_tob_temp->size             = tb->size;
	r_tob_temp->value            = operator new(tb->size);
	copyBytes(r_tob_temp->value, tb->value, tb->size);
	(trans->read_buffer)[tb->ID] = r_tob_temp;

	return SUCCESS;
}

/*-----------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! try_commit() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*-----------------------------------------------------------------------------------*/
/*! Takes arguments as trans_state and returns an int depending on success or failure*/
/*-----------------------------------------------------------------------------------*/
int MVTO::try_commit(trans_state* trans, long long& error_tOB)
{
	//log.flush();
	//log<<"transaction: "<<trans->TID<<" thread: "<<pthread_self()<<" entered try_commit func"<<endl;                
	list<common_tOB*>::iterator tb_itr; 
	list<common_tOB*> write_set;
	list<common_tOB*> read_set;

	
	for(auto it = (trans->local_buffer).begin(); it != (trans->local_buffer).end(); it++)
	{
		//! write to shared memory only if write was performed by transaction.
		if(((local_tOB*)(it->second))->buffer_param == WRITE_PERFORMED)
			write_set.push_back(it->second);

		//if(((local_tOB*)(hash_itr->second))->buffer_param == READ_PERFORMED)
		//	read_set.push_back(hash_itr->second);
	}

	for(auto it = (trans->read_buffer).begin(); it != (trans->read_buffer).end(); it++)
		read_set.push_back(it->second);

	if(write_set.size() != 0)
	{
		//!---------------------------------------------------------
		//! for validation phase we are required to check if lock  !
		//! can be acquired on all data_item in write buffer.      !
		//!---------------------------------------------------------
		
		//! Write buffer is soted based on ID's to impose an order on locking.
		write_set.sort(compare_local_tOB);
		write_set.unique(compare_local_tOB);

		//! stores pointer to shared objects in write set.
		hash_STM::accessor a_tOB[write_set.size()];
		int acc_index = 0;
		//! Obtain lock on data items of whole write buffer.
		for(tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			//! check if required data object exists in shared memory.
			//! acquiring a lock on all data items of write buffer if found.
			if(!sh_tOB_list.find(a_tOB[acc_index], (*tb_itr)->ID))
			{
				//! release all the locks acquired till now before abort,
				//! if a data object of write buffer not found.
				for(int j = 0; j <= acc_index; j++)
				{
					a_tOB[j].release();
				} 
				error_tOB = (*tb_itr)->ID; 
				try_abort(trans);
				return TOB_ABSENT;
			}
			acc_index++;
		}

		acc_index = 0;
		//! Validating whole write buffer.
		for(tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			MVTO_tOB* tob_temp = (MVTO_tOB*)(a_tOB[acc_index]->second);
			//!--------------------------------------------------
			//! Validating the write operations using MVTO rule.!
			//!--------------------------------------------------
			auto itr = (tob_temp->MVTO_version_list).lower_bound(trans->TID);
			//! go to just before version.
			itr--;
			if(trans->TID < (itr->second)->max_read)
			{
				error_tOB = (*tb_itr)->ID;
				//! release all the locks acquired till now before abort.
				for(int j = 0; j < write_set.size(); j++)
				{
					a_tOB[j].release();
				} 
				try_abort(trans);
				return INVALID_TRANSACTION;
			}
			acc_index++;
		}

		acc_index = 0; 
		for(tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			//!----------------------------------------------------------------
			//! check if sizes of objects in shared memory & write set matchs !
			//!---------------------------------------------------------------- 
			if((*tb_itr)->size != ((MVTO_tOB*)(a_tOB[acc_index]->second))->size)
			{
				//! release all the locks acquired till now before abort.
				for(int j = 0; j < write_set.size(); j++) { a_tOB[j].release();}
				error_tOB = (*tb_itr)->ID;
				try_abort(trans);

				//! sizes of existing & required objects do'not match.
				return SIZE_MISMATCH;
			} 
			acc_index++;
		}

		//!-----------------------------------------------------
		//! After validation is successful updat shared memory !
		//! with the values stored in write buffer.            !
		//!-----------------------------------------------------
		acc_index = 0; 
		for(tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			MVTO_tOB* tob_temp; 
			tob_temp                    = (MVTO_tOB*)(a_tOB[acc_index]->second);
			MVTO_version_data*temp_data = new MVTO_version_data;
			void* value                 = operator new((*tb_itr)->size);
			temp_data->value            = value;
			temp_data->max_read         = trans->TID;  //0;?/

			//! inserting new version into shared memory.
			copyBytes(temp_data->value, (*tb_itr)->value, (*tb_itr)->size);
			(tob_temp->MVTO_version_list)[trans->TID] = temp_data;

		//	a_tOB[acc_index].release();
			acc_index++;
		}
		for( int j = 0; j < write_set.size(); j++ ) 
		{
			a_tOB[j].release();
		}

	}
	
	if(read_set.size() != 0)
	{
		 //! read set is soted based on ID's to impose an order on locking.
		read_set.sort(compare_local_tOB);
		read_set.unique(compare_local_tOB);

		//! allocate array for access shared memory.
		hash_STM::accessor a_tOB[read_set.size()];

		int acc_index = 0;

		//! Obtain, do task, and then release lock on shared object of read set one by one.
		for(tb_itr = (read_set).begin(); tb_itr != (read_set).end(); tb_itr++)
		{
			//! acquiring a lock on data item of read set.
			sh_tOB_list.find(a_tOB[acc_index], (*tb_itr)->ID);

			//! get the pointer in tob_temp for the shared object in read_set.
			MVTO_tOB* tob_temp = (MVTO_tOB*)(a_tOB[acc_index]->second);

			//! lower_bound() returns pointer to trans->TID in map.
			auto itr = (tob_temp->MVTO_version_list).lower_bound(trans->TID);

			//! pointer to privious version.
			itr--;

			//! adding current TID to the read list of the version it has read.
			((itr->second)->read_list).push_back(trans->TID);

			//! release the lock
			a_tOB[acc_index].release();
			acc_index++;
		}
	}

	live_set_lock.lock();
	sh_live_set.erase(trans->TID);
	live_set_lock.unlock();
	delete(trans);//! freeing memory allocated to trans_state of committing trans.
	return SUCCESS; 
}

/*----------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!! try_commit_conflict() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*----------------------------------------------------------------------------*/
/*! Takes argument as trans_state & return an int depending on success/failure*/
/*----------------------------------------------------------------------------*/
int MVTO::try_commit_conflict(trans_state *trans, long long &error_tOB, list<int>&conf_list)
{
	list<common_tOB*>::iterator tb_itr; 
	list<common_tOB*> write_set;
	list<common_tOB*> read_set;

	//! stores respective conflicts.
	list<int> w_confList;
	
	auto hash_itr = (trans->local_buffer).begin();
	for( ; hash_itr != (trans->local_buffer).end(); hash_itr++)
	{
		//! write to shared memory only if write was performed by transaction.
		if(((local_tOB*)(hash_itr->second))->buffer_param == WRITE_PERFORMED)
			write_set.push_back(hash_itr->second);

		//if(((local_tOB*)(hash_itr->second))->buffer_param == READ_PERFORMED)
		//	read_set.push_back(hash_itr->second);
	}
	
	hash_itr = (trans->read_buffer).begin();
	for( ; hash_itr != (trans->read_buffer).end(); hash_itr++)
		read_set.push_back(hash_itr->second);

	if(write_set.size() != 0)
	{
		//! Write buffer is soted based on ID's to impose an order on locking.
		write_set.sort(compare_local_tOB);

		//!--------------------------------------------------------
		//! for validation phase we are required to check if lock !
		//! can  be acquired on all data_item in write buffer     !
		//!--------------------------------------------------------
		hash_STM::accessor a_tOB[write_set.size()];
		int acc_index = 0;
		for(tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			//! check if required data object exists in shared memory
			//! acquiring a lock on all data items of write buffer if found.
			if(!sh_tOB_list.find(a_tOB[acc_index], (*tb_itr)->ID))
			{
				//! release all the locks acquired till now before abort,
				//! if a data object of write buffer not found.
				for(int j = 0; j <= acc_index; j++) a_tOB[j].release();
				error_tOB = (*tb_itr)->ID; 
				try_abort(trans);
				return TOB_ABSENT;
			}
			acc_index++;
		}

		//!--------------------------------------------------
		//! Validating the write operations using MVTO rule !
		//!--------------------------------------------------
		acc_index = 0;
		for (tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			MVTO_tOB* tob_temp = (MVTO_tOB*)(a_tOB[acc_index]->second);
			auto itr = (tob_temp->MVTO_version_list).lower_bound(trans->TID);
			//! go to just before version.
			itr--;
			if(trans->TID < (itr->second)->max_read)
			{
				error_tOB = (*tb_itr)->ID; 
				//! release all the locks acquired till now before abort.
				for(int j = 0; j <= acc_index; j++) a_tOB[j].release();
				try_abort(trans);
				return INVALID_TRANSACTION;
			}
			acc_index++;
		}
		
		//!------------------------------------------------------------------
		//! check if sizes of objects in shared memory and write set matchs !
		//!------------------------------------------------------------------
		acc_index   = 0; 
		for (tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{ 
			if((*tb_itr)->size != ((MVTO_tOB*)(a_tOB[acc_index]->second))->size)
			{
				 //! release all the locks acquired till now before abort.
				for(int j = 0; j <= acc_index; j++) a_tOB[j].release();
				error_tOB = (*tb_itr)->ID;
				try_abort(trans);
				//! 3 indicates that sizes of existing & required objects d'not match.
				return SIZE_MISMATCH;
			} 
			acc_index++;
		}

		//!------------------------------------------------------
		//! After validation is successful updat shared memory  !
		//! with the values stored in write buffer.             !
		//!------------------------------------------------------
		acc_index  = 0; 
		for(tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
		{
			MVTO_tOB* tob_temp = (MVTO_tOB*)(a_tOB[acc_index]->second);		
			//! map "lower_bound" points to version id greater than or equals to the trans->TID.
			auto itr = (tob_temp->MVTO_version_list).lower_bound(trans->TID);
			itr--;//! go to just before version.
	
//			cout<< "Trans "+to_string(trans->TID)+" to create version of SObj X"+to_string(tob_temp->ID)+" Its Privious Version is Xi"+to_string(itr->first)+"\n\n";

		/*
			//! get pointer to this.trans conflist in gConfList table
			c_LIST:: accessor cAccess;
			if(gConfList.find(cAccess, trans->TID))
			{
				if(((itr->second)->read_list).empty()) 	
					cAccess->second.push_back(itr->first);
				else
				{
					//! insert tIDs from readlist of privious version.
					auto rl = ((itr->second)->read_list).begin();
					for(; rl != ((itr->second)->read_list).end(); rl++)
						cAccess->second.push_back(*rl);
				}
				cAccess.release();
			}
		*/
			//! if privious version read list is empty, add conflict as version creator TID.
			if(((itr->second)->read_list).empty()) 
			{
//				cout<< "Trans "+to_string(trans->TID)+" to create version of SObj X"+to_string(tob_temp->ID)+" Its Privious Version RL is Empty addingh TID"+to_string(itr->first)+" into its confList\n\n";
				w_confList.push_back(itr->first);
			}
			else
			{
				//! insert tIDs from reading list of privious version as a conflict in conf list.
				auto rl_itr = ((itr->second)->read_list).begin();
				for(; rl_itr != ((itr->second)->read_list).end(); rl_itr++)
				{
//					cout<< "Trans "+to_string(trans->TID)+" to create version of SObj X"+to_string(tob_temp->ID)+" Its Privious Version RL is nonEmpty addingh TID"+to_string(*rl_itr)+" into its confList\n\n";
					w_confList.push_back(*(rl_itr));
				}
			}

			//!----------------------
			//!find the next version
			//!----------------------
			bool flag = false;
			auto nextVe = (tob_temp->MVTO_version_list).upper_bound(trans->TID);
			
			auto next = (tob_temp->MVTO_version_list).begin();
			for(; next != (tob_temp->MVTO_version_list).end(); next++)
			{
				if(trans->TID < next->first)
				{
					flag = true;
					cout<<"Version - Xi"+to_string(next->first)+" is > trans->TID "+to_string(trans->TID)+"\n";
					break;
				}
//				cout<<"Version - Xi"+to_string(next->first)+" is < trans->TID "+to_string(trans->TID)+"\n";	
			}
			
			
			//cout<<"Trans ID "+to_string(trans->TID)+" next version by upper bound "+to_string(next->first)+"\n";
			if(nextVe->first > trans->TID)
			{
				cout<< "Trans "+to_string(trans->TID)+" to create version of SObj X"+to_string(tob_temp->ID)+" Its Next Version Xi"+to_string(nextVe->first)+" into its confList\n\n";
				w_confList.push_back(nextVe->first);
			}
			
			
			//! Finally add new version in version list
			MVTO_version_data* temp_data = new MVTO_version_data;
			void* value                  = operator new((*tb_itr)->size);
			temp_data->value             = value;
			temp_data->max_read          = trans->TID; //0;?/
			//! inserting new version into shared memory
			copyBytes(temp_data->value, (*tb_itr)->value, (*tb_itr)->size);
			(tob_temp->MVTO_version_list)[trans->TID] = temp_data;

			
			//! release lock on write set current shared object
	//		a_tOB[acc_index].release();
			acc_index++;
		}
		for( int j = 0; j < write_set.size(); j++ ) 
		{
			a_tOB[j].release();
		}

	}

	if(read_set.size() != 0)
	{
		//! read set is soted based on ID's to impose an order on locking.
		read_set.sort(compare_local_tOB);
		read_set.unique(compare_local_tOB);

		//! allocate array for access shared memory.
		hash_STM::accessor a_tOB[read_set.size()];

		int acc_index = 0;
		//! Obtain, do task, & release lock on shared object of read set one by one.
		for(tb_itr = (read_set).begin(); tb_itr != (read_set).end(); tb_itr++)
		{
			//! search and acquire a lock on shared object of tb_itr object 
			//! in read set (here "find" obtain lock on data item in map).
			sh_tOB_list.find(a_tOB[acc_index], (*tb_itr)->ID);
			//! get the pointer in tob_temp for the shared object in read_set.
			MVTO_tOB* tob_temp = (MVTO_tOB*)(a_tOB[acc_index]->second);
			//! lower_bound() returns pointer to trans->TID in map.
			auto itr = (tob_temp->MVTO_version_list).lower_bound(trans->TID);
			//! pointer to privious version.
			itr--;
			
			//! adding current TID to the read list of the version it has read.
			((itr->second)->read_list).push_back(trans->TID);

//			cout<< "Trans "+to_string(trans->TID)+ " for read version of SObj X"+to_string(tob_temp->ID) +" adding itself into RL of Xi"+to_string(itr->first)+"\n\n";

			w_confList.push_back(itr->first);
//			cout<<"Trans "+to_string(trans->TID)+" for read version of SObj X"+to_string(tob_temp->ID) +" update its conflict with version it read Xi"+to_string(itr->first)+"\n\n";

			auto nextVe = (tob_temp->MVTO_version_list).upper_bound(trans->TID);
			if(nextVe->first > trans->TID)
			{
				w_confList.push_back(nextVe->first);
//				cout<<"Trans "+to_string(trans->TID)+" for read version of SObj X"+to_string(tob_temp->ID) +" update its conflict with next version Xi"+to_string(nextVe->first)+"\n\n";
			}
			
			//! release the lock
			a_tOB[acc_index].release();
			acc_index++;
		}
	}
	
	w_confList.sort();
	w_confList.unique();
	tID_confList temp_stuct;
	temp_stuct.TID       = trans->TID;
	temp_stuct.conf_list = w_confList;
	cList.push_back(temp_stuct);
	
	conf_list = w_confList;
	
	live_set_lock.lock();
	sh_live_set.erase(trans->TID);
	live_set_lock.unlock();
	delete(trans);//! freeing memory allocated to trans_state of committing trans.
	return SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! get_conf() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*----------------------------------------------------------------------------*/
/*! Given transaction id (TID) function prints the corresponding conflict list*/
/*----------------------------------------------------------------------------*/
list<int> MVTO::get_conf(long long TID)
{
	list<int>emptyList;
	auto findIter = cList.begin();
	for(; findIter != cList.end(); findIter++)
	{
		if(findIter->TID == TID)
		{
			break;
		}
	}

	if(findIter->TID == TID && findIter != cList.end())
		return (findIter->conf_list);
	else
		return (emptyList);
}

/*-----------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! write() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*-----------------------------------------------------------------------------------*/
/*! Since the MVTO implementation is deferred write, the only job of the protocol is */
/*! to add to write_set which is later validated and updated in the try commit phase */
/*-----------------------------------------------------------------------------------*/
int MVTO::write(trans_state* trans, common_tOB* tb)
{
	log.flush();
	log<<"transaction: "<<trans->TID<<" thread: "<<pthread_self()<<" entered write func"<<endl;
	//!---------------------------------------
	//! search for data item in local buffer !
	//!---------------------------------------
	const int id = tb->ID;
	if(trans->local_buffer.find(id) != (trans->local_buffer).end())
	{
		((local_tOB*)(trans->local_buffer[tb->ID]))->buffer_param = WRITE_PERFORMED; 
		if(tb->size != (trans->local_buffer[tb->ID])->size)
			return SIZE_MISMATCH;                                    //3 indicates that sizes of existing and required data objects do not match.
		copyBytes((trans->local_buffer[tb->ID])->value,tb->value,tb->size);//Returning the value to the user.
		return SUCCESS;
	}

	//!----------------------------------
	//!adding new write to local_buffer !
	//!----------------------------------
	local_tOB* tob_temp           = new local_tOB;
	tob_temp->ID                  = tb->ID;
	tob_temp->size                = tb->size;
	tob_temp->value               = operator new(tb->size);
	copyBytes(tob_temp->value, tb->value,tb->size);
	tob_temp->buffer_param        = WRITE_PERFORMED;
	(trans->local_buffer[tb->ID]) = tob_temp;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!! try_abort() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*-----------------------------------------------------------------------------------*/
/*! Aborting transaction called by functions in case of a failure in validation step */
/*-----------------------------------------------------------------------------------*/
void MVTO::try_abort(trans_state* trans)
{
	log.flush();
	log<<"transaction: "<<trans->TID<<" thread: "<<pthread_self()<<" entered try_abort func"<<endl;
	live_set_lock.lock();
	sh_live_set.erase(trans->TID);
	live_set_lock.unlock();
	delete(trans);                                                   //calls destructor of trans_state which frees the local_buffer.
}


/*--------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!! create_new() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------*/
/*!creates new data item in shared memory with specified ID,size and default value*/
/*--------------------------------------------------------------------------------*/
int MVTO::create_new(long long ID, int size)   
{
	MVTO_tOB *tob_temp = new MVTO_tOB;
	if(!tob_temp)
		return MEMORY_INSUFFICIENT;                                  //unable to create new data object.

	create_new_lock.lock();                                          //to make data object creation and max_tob_id update operations atomic.
	hash_STM::const_accessor ca_tOB;
	if(sh_tOB_list.find(ca_tOB, ID))                                 //checking if data object with required id already exists.
	{
		ca_tOB.release();
		cout<<"Data object with specified ID already exists"<<endl;
		create_new_lock.unlock();
		return TOB_ID_CLASH;
	}
	ca_tOB.release();

	tob_temp->ID                     = ID;
	tob_temp->value                  = NULL;
	
	MVTO_version_data* temp          = new MVTO_version_data;
	(tob_temp->MVTO_version_list)[0] = temp;                         //version 0
	
	(temp)->value                    = operator new(size);           //transaction t0 writes the first version of every data item.
	memset( (char*)(temp->value), 0, size );
	tob_temp->size                   = size;
	temp->max_read                   = 0;

	hash_STM::accessor a_tOB;
	sh_tOB_list.insert( a_tOB, ID );
	a_tOB->second = tob_temp;
	a_tOB.release();
	
	if(ID > max_tob_id)
		max_tob_id = ID;

	create_new_lock.unlock();                                        //releasing mutex lock.

	return SUCCESS;                                                  //returning of newly created data object.
} 
/*--------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!! create_new() !!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------*/
/*! creates new data item in shared memory with specified size !*/
/*--------------------------------------------------------------*/
int MVTO::create_new(int size, long long & ID)
{
	//!-----------------------------------------------------------------------
	//! to make data object creation and max_tob_id update operations atomic !
	//!-----------------------------------------------------------------------
	MVTO_tOB *tob_temp = new MVTO_tOB;
	if(!tob_temp)
		return MEMORY_INSUFFICIENT;                                  //unable to allocate memory to data object.
	create_new_lock.lock();
	max_tob_id++;                                                    //updating max_tob_id.

	tob_temp->ID                     = max_tob_id;                   //using id=previous max_tob_id+1.
	tob_temp->value                  = NULL;
	MVTO_version_data* temp          = new MVTO_version_data;
	(tob_temp->MVTO_version_list)[0] = temp;
	(temp)->value                    = operator new(size);           //transaction t0 writes the first version of every data item.
	memset( (char*)(temp->value), 0, size );
	tob_temp->size                   = size;
	temp->max_read                   = 0;


	hash_STM::accessor a_tOB;
	sh_tOB_list.insert(a_tOB, max_tob_id);
	a_tOB->second = tob_temp;
	a_tOB.release();
	ID = max_tob_id;
	create_new_lock.unlock();                                        //releasing mutex lock.
	return SUCCESS;                                                  //returning newly created id.
}

/*----------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!! create_new() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*----------------------------------------------------------------------------*/
/*! creates new data object with default size and unused id in shared memory !*/
/*----------------------------------------------------------------------------*/
int MVTO::create_new(long long &ID)
{
	log.flush();
	log<<" thread: "<<pthread_self()<<" entered create new func"<<endl;
	create_new_lock.lock();                                          //to make data object creation and max_tob_id update operations atomic.
	MVTO_tOB *tob_temp = new MVTO_tOB;
	if(!tob_temp)
		return MEMORY_INSUFFICIENT;                                  //unable to allocate memory to data object.

	max_tob_id++;                                                    //updating max_tob_id.
	tob_temp->ID            = max_tob_id;                            //using id=previous max_tob_id+1.
	tob_temp->size          = DEFAULT_DATA_SIZE;
	tob_temp->value         = NULL;
	MVTO_version_data* temp = new MVTO_version_data;
	temp->value             = operator new(DEFAULT_DATA_SIZE);       //transaction t0 writes the first version of every data item.
	memset( (char*)(temp->value), 0, DEFAULT_DATA_SIZE );

	//!modificastion
	temp->max_read = 0;

	(temp->read_list).push_back(-1);
	(tob_temp->MVTO_version_list)[0] = temp;
	if( (tob_temp->MVTO_version_list)[0]->value == NULL ) log<<"error here 3!\n";

	hash_STM::accessor a_tOB;
	sh_tOB_list.insert(a_tOB,max_tob_id);
	a_tOB->second = tob_temp;
	a_tOB.release();
	ID = max_tob_id;
	create_new_lock.unlock();                                        //releasing mutex lock.
	return SUCCESS;                                                  //returning newly created id.
}
 
void MVTO::end_library()
{
	gc_active = false;
	log.close();
}


void MVTO::gc()
{
	set<long long > data_items;
	hash_STM::iterator itr = sh_tOB_list.begin();
	for(;itr!=sh_tOB_list.end();itr++)
	{
		data_items.insert(itr->first);
	}
	set<long long >::iterator d_itr;
	for( d_itr = data_items.begin(); d_itr != data_items.end(); d_itr++ )
	{
		hash_STM::const_accessor a_tOB;
		if( sh_tOB_list.find(a_tOB,*d_itr) )
		{
			map<int,MVTO_version_data*>::iterator v_itr=((MVTO_tOB*)(a_tOB->second))->MVTO_version_list.begin();
			v_itr++;     //do not consider t0
			set<int> to_be_deleted;
			for( ; v_itr != ((MVTO_tOB*)(a_tOB->second))->MVTO_version_list.end(); v_itr++)
			{
				v_itr++;
				//!----------------------------------------------------------------------------------------------------------
				//! if no other transaction has written a newer version for this data item, do not delete it (step 1 of gc) !
				//!----------------------------------------------------------------------------------------------------------
				if( v_itr == ((MVTO_tOB*)(a_tOB->second))->MVTO_version_list.end() )
					break;

				int k_TID=v_itr->first;
				v_itr--;
				int i_TID=v_itr->first;
				//!----------------------------------------
				//!verifying step 2 of garbage collection !
				//!----------------------------------------
				live_set_lock.lock();
				set<int>::iterator live_itr = sh_live_set.upper_bound(i_TID);
				if( live_itr == sh_live_set.end() || (*live_itr) > k_TID )//safe to delete version.
				{
					to_be_deleted.insert(i_TID);                     //storing the versions to be deleted in a set.
				} 
				live_set_lock.unlock();
			}
			set<int>::iterator del_set_itr = to_be_deleted.begin();
			for(;del_set_itr != to_be_deleted.end(); del_set_itr++)  //deleting all versions in to_be_deleted set for current data item.
			{
				( (MVTO_tOB*)(a_tOB->second) )->MVTO_version_list.erase(*del_set_itr);
			}
			a_tOB.release();
		}
	}
}

MVTO::MVTO(const MVTO& orig)
{
}

MVTO::~MVTO()
{
}

/*
changes
create t0 transaction
multiple -- for itr
lock versions independently
ask the user for the speed of application to decide the frequency of gc
*/
