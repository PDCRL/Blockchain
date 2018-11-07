/*
 * File:   BTO.h
 * Created July 23, 2018, 01:46 AM
 */

#include "BTO.h"
#include <iostream>
#include <list>
#include <set>
#include <iterator>
#include <vector>
#include <cstring>
#include <fstream>


using namespace std;
using namespace tbb;

/*default Constructor*/
BTO::BTO() : STM()
{
}

/* -------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! initialize() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* -------------------------------------------------------------------------------------------------------*/
/*                       memory initializer with only data object sizes as parameters                     */
/* -------------------------------------------------------------------------------------------------------*/
int BTO::initialize(vector<int> sizes)
{
	for(int i = 0; i<sizes.size(); i++)
	{
		int ID;
		int result = create_new(sizes[i], ID);
      
		if(result != SUCCESS)
			return result;
	}
	return SUCCESS;
}


/* -------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! initialize() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* -------------------------------------------------------------------------------------------------------*/
/*                memory initializer with data objects' sizes and IDS as parameters                       */
/* -------------------------------------------------------------------------------------------------------*/
int BTO::initialize(vector<int> initial_tobs, vector<int> sizes)
{
	for(int i=0; i<initial_tobs.size(); i++)
	{
		int result = create_new(initial_tobs[i], sizes[i]);
      
		if(result != SUCCESS && result < 0)
			return result;
	}
	return SUCCESS;
}

/* -------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! initialize() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* -------------------------------------------------------------------------------------------------------*/
/*                                 default memory initializer                                             */
/* -------------------------------------------------------------------------------------------------------*/
int BTO::initialize()
{
	for(int i=0; i<INITIAL_DATAITEMS; i++)
	{
		int ID;
		int result = create_new(ID);
		if(result != SUCCESS)
			return result;
	}
	return SUCCESS;
}

/* -------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! begin() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* -------------------------------------------------------------------------------------------------------*/
/*    Begin function creates a new trans_state object and sets the TID value by incrementing atomic       */
/*   variable "sh_count". The trans_state pointer is returned to the user which is used as argument for   */
/*                         later operations like "read" , "write" , "try_commit"                          */
/* -------------------------------------------------------------------------------------------------------*/
trans_state* BTO::begin()
{
	trans_state* trans_temp = new trans_state;
	trans_temp->TID = sh_count++;	//incrementing the atomic variable
	return trans_temp;
}

/* -------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! read() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* -------------------------------------------------------------------------------------------------------*/
/*   Read function takes as arguments trans_state(contains the TID for transaction) and tOB(contains the  */
/*   ID,value and timestamps(BTO_data) for a data_item). The user enters the data_item to be read         */
/*   If the read operation is successful then, value of the data item is entered into tOB argument        */
/* -------------------------------------------------------------------------------------------------------*/
int BTO::read(trans_state* trans, common_tOB* tb)
{
	/*search for data item in local buffer*/
	const int id = tb->ID;
	if((trans->local_buffer).find(id) != (trans->local_buffer).end())
	{
		if(tb->size != (trans->local_buffer[tb->ID])->size)
			return SIZE_MISMATCH;
		copyBytes(tb->value, (trans->local_buffer[tb->ID])->value, (trans->local_buffer[tb->ID])->size);  //Returning the value to the user
		return SUCCESS;
	}
	BTO_tOB* tob_temp;
	
	/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	!!!! const_accessor:: Reader lock - permits shared access with other readers.                        !!!!
	!!!! accessor :: Writer lock - permits exclusive access by a thread. Blocks access by other threads. !!!!
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
	
	hash_STM::accessor a_tOB;
	if(sh_tOB_list.find(a_tOB, tb->ID))                                //Finds the data_item in the shared memory and acquires a read lock on it
		tob_temp = (BTO_tOB*)a_tOB->second;
	else
	{
		cout<<"ERROR!! from read: data Object ID = "<<(tb->ID)<<"Does not Exist"<<endl;
		a_tOB.release();
		try_abort(trans);
		return TOB_ABSENT;                                             //error code 2 indicates that required data object does not exist
	}
	if(tb->size != tob_temp->size)
	{
		a_tOB.release();
		try_abort(trans);
		cout<<"Error:: Size mismatch\n";
		return SIZE_MISMATCH;
	}
	timestamps* timestamp_temp = (timestamps*)(tob_temp->BTO_data);      //Getting the timestamps of the tOB item
	if(timestamp_temp->max_write > trans->TID )                        //Validating the read operation
	{
		a_tOB.release();                                               //If invalid then releasing the lock and aborting the transaction
		//try_abort(trans);
		//cout<<"Error:: read validation  failed\n";
		return INVALID_TRANSACTION;
	}
	else
	{
		if( timestamp_temp->max_read < trans->TID )                    //!!some time lower trns may update the max_read with smaller value!!
			timestamp_temp->max_read = trans->TID;                     //Else update the max_read of the timestamps 
		
		if(tb->value == NULL)                                          //If the application did not allot memory to input
		{
			tb->value = operator new(tob_temp->size);
			tb->size  = tob_temp->size;
		}
		copyBytes(tb->value, tob_temp->value, tob_temp->size);         //Returning the value to the user
		local_tOB* local_tob_temp = new local_tOB;                     //updating local buffer
		local_tob_temp->ID        = tb->ID;
		local_tob_temp->size      = tb->size;
		local_tob_temp->value     = operator new(tb->size);
		copyBytes(local_tob_temp->value, tb->value, tb->size);
		local_tob_temp->buffer_param  = READ_PERFORMED;
		(trans->local_buffer[tb->ID]) = local_tob_temp;
		
		a_tOB.release();
		return SUCCESS;
	}
}


/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!   try_commit()   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*  This function takes as arguments trans_state and returns an int depending on the success or failure   */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::try_commit(trans_state* trans, long long& error_tOB)
{
	list<common_tOB*>::iterator tb_itr;
	list<common_tOB*> write_set;
	map<int,common_tOB*>::iterator hash_itr=(trans->local_buffer).begin();
	for(;hash_itr!=(trans->local_buffer).end();hash_itr++)
	{
		if(((local_tOB*)((*hash_itr).second))->buffer_param == WRITE_PERFORMED)//write to shared memory only if write was performed by transaction
		write_set.push_back((*hash_itr).second);
	}
	write_set.sort(compare_local_tOB);                                 //Write buffer is soted based on ID's to impose an order on locking
	//!for validation phase we are required to check if lock can be acquired on all data_item in write buffer
	
	hash_STM::accessor a_tOB[write_set.size()];
	int acc_index = 0;
	for (tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)//Validating whole write buffer
	{
		BTO_tOB* tob_temp;
		//!check if required data object exists in shared memory
		if(!sh_tOB_list.find(a_tOB[acc_index], (*tb_itr)->ID))         //Acquiring a lock on all data items of write buffer if found
		{
			for(int j = 0; j <= acc_index; j++)                        //release all the locks acquired till now before abort
			{
				a_tOB[j].release();
			}
			error_tOB = (*tb_itr)->ID;
			try_abort(trans);
			return TOB_ABSENT;
		}
		tob_temp = (BTO_tOB*)a_tOB[acc_index]->second;
		timestamps* findval = (timestamps*)(tob_temp->BTO_data);
		
		//!Validating the write operation using the timestamps obtained in above step
		
		if(findval->max_read > trans->TID || findval->max_write > trans->TID )
		{
			for(int j=0; j <= acc_index; j++)                          //release all the locks acquired till now before abort
			{
				a_tOB[j].release();
			}
			error_tOB = (*tb_itr)->ID;
			try_abort(trans);
			return INVALID_TRANSACTION;
		}
		//!checking if sizes of data objects in shared memory and write set match
		if((*tb_itr)->size != tob_temp->size)
		{
			for(int j = 0; j <= acc_index; j++)                        //release all the locks acquired till now before abort
			{
				a_tOB[j].release();
			}
			error_tOB = (*tb_itr)->ID;
			try_abort(trans);
			return SIZE_MISMATCH;                                      //3 indicates that sizes of existing and required data objects do not match
		}
		acc_index++;
	}
	//!After validation is successful updating the shared memory with the values stored in write buffer(given by the user)
	acc_index = 0;
	for (tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)
	{
		BTO_tOB* tob_temp;
		tob_temp=(BTO_tOB*)a_tOB[acc_index]->second;
		timestamps* findval = (timestamps*)(tob_temp->BTO_data);
		findval->max_write  = trans->TID;                              //Updating the max_write value of the timestamps of the particular data item
		copyBytes(tob_temp->value, (*tb_itr)->value, (*tb_itr)->size); //Updating the shared memory with new value
		acc_index++;
	}
	for(int j = 0; j < write_set.size(); j++)
	{
		a_tOB[j].release();
	}
	delete(trans); //freeing memory allocated to trans_state of committing transaction
	return SUCCESS;
}


/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! try_commit_conflict() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*  This function takes as arguments trans_state and returns an int depending on the success or failure   */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::try_commit_conflict(trans_state* trans, long long& error_tOB, list<int> &conf_list)
{
	list< common_tOB* >::iterator tb_itr;
	list< common_tOB* > write_set;
	list< common_tOB* > read_set;
	
	map<int, common_tOB*>::iterator hash_itr = (trans->local_buffer).begin();
	
	for(;hash_itr != (trans->local_buffer).end(); hash_itr++)
	{
		if(((local_tOB*)((*hash_itr).second))->buffer_param == WRITE_PERFORMED)//write to shared memory only if write was performed by transaction
			write_set.push_back((*hash_itr).second);
			
		if(((local_tOB*)((*hash_itr).second))->buffer_param == READ_PERFORMED) //get the read-set of this::transaction
			read_set.push_back((*hash_itr).second); 
	}
  
	write_set.sort(compare_local_tOB);                                 //write buffer is sorted based on ID's to impose an order on locking
	read_set.sort (compare_local_tOB);                                 //read buffer is soted based on ID's to impose an order on locking
	
	/*!for validation phase we are required to check if lock can be acquired on all data_item in write buffer!*/
	hash_STM::accessor a_tOB[write_set.size()];
	int acc_index = 0;
	for (tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++)  //validating whole write buffer
	{
		BTO_tOB* tob_temp;
		/*!check if required data object exists in shared memory!*/
		if( !sh_tOB_list.find(a_tOB[acc_index], (*tb_itr)->ID) )       //acquiring a write lock on data item of write buffer if found
		{
			for(int j = 0;j <= acc_index; j++)                         //release all the locks acquired till now before abort
			{
				a_tOB[j].release();
			}
			error_tOB = (*tb_itr)->ID;
			try_abort(trans);
			return TOB_ABSENT;
		}
		
		tob_temp            = (BTO_tOB*)a_tOB[acc_index]->second;
		timestamps* findval = (timestamps*)(tob_temp->BTO_data);
	  
		//!validating the write operation using the timestamps obtained in above step!
		if(findval->max_read > trans->TID || findval->max_write > trans->TID )
		{
			for(int j=0; j<=acc_index; j++)                            //release all the locks acquired till now before abort
			{
				a_tOB[j].release();
			}
			error_tOB = (*tb_itr)->ID;
			try_abort(trans);
			return INVALID_TRANSACTION;
		}
		//!checking if sizes of data objects in shared memory and write set match
		if((*tb_itr)->size != tob_temp->size)
		{
			for(int j=0; j<=acc_index; j++)                            //release all the locks acquired till now before abort
			{
				a_tOB[j].release();
			}
			error_tOB = (*tb_itr)->ID;
			try_abort(trans);
			return SIZE_MISMATCH;                                      //3 indicates that sizes of existing and required data objects do not match
		}
		acc_index++;
	}
	//!After validation is successful updating the shared memory with the values stored in write buffer(given by the user)
	acc_index = 0;
	for( tb_itr = (write_set).begin(); tb_itr != (write_set).end(); tb_itr++ )
	{
		
		BTO_tOB* tob_temp;
		tob_temp            = (BTO_tOB*)a_tOB[acc_index]->second;
		timestamps *findval = (timestamps*)(tob_temp->BTO_data);
		findval->max_write  = trans->TID;                              //Updating the max_write value of the timestamps of the particular data item
		/*----------------------------------------------------------------------------------------------------*/
		//Here update <This::transaction-id> in respective <shared object> <write-list> in sorted order
		/*----------------------------------------------------------------------------------------------------*/

		//for each shared object in write-set of current trns, write_list is added in conf_list
		list<int>::iterator it;
		for(it = findval->w_list->begin(); it != findval->w_list->end();it++)
			conf_list.push_back(*it);
		
		//for each shared object in write-set of current trns, read_list is added in conf_list
		for(it = findval->r_list->begin(); it != findval->r_list->end();it++)
			conf_list.push_back(*it);
			  	
		findval->w_list->push_back(trans->TID);
		copyBytes( tob_temp->value, (*tb_itr)->value, (*tb_itr)->size );//Updating the shared memory with new value
		acc_index++;
	}
	for(int j=0; j<write_set.size(); j++)
	{
		a_tOB[j].release();
	}
  
	/*----------------------------------------------------------------------------------------------------*/
	//Here update <This::transaction-id in respective <shared object> <read-list> in sorted order
	/*----------------------------------------------------------------------------------------------------*/
	
	list<common_tOB*>::iterator tb_itr1;
	read_set.unique(compare_local_tOB);                                //to avoide locking on same data item multiple time for read operation
	
	hash_STM::accessor a_tOB1[read_set.size()];
	int acc_index1 = 0;
	for (tb_itr1 = (read_set).begin(); tb_itr1 != (read_set).end(); tb_itr1++)//To obtain read lock on read-set shared objects
	{
		BTO_tOB* tob_temp1;
		//!check if required data object exists in shared memory
		if(!sh_tOB_list.find(a_tOB1[acc_index1], (*tb_itr1)->ID))      //find and acquiring read locks on data-item of read buffer
		{
			for(int j = 0; j <= acc_index1; j++)                       //release all the locks acquired till now in case if transaction not found a data item is aborted
			{
				a_tOB1[j].release();
			}
		}
		tob_temp1            = (BTO_tOB*)a_tOB1[acc_index1]->second;
		timestamps *findval1 = (timestamps*)(tob_temp1->BTO_data);	  

		//!for each shared object in read-set of current trns, write_list is added to conf_list
		for(list<int>::iterator it = findval1->w_list->begin(); it != findval1->w_list->end(); it++)
			conf_list.push_back(*it);
		
		findval1->r_list->push_back(trans->TID);                       //add this transaction id in read-list of shared-object locally
		a_tOB1[acc_index1].release();                                  //release read locks
		acc_index1++;
	}

	conf_list.sort();
	conf_list.unique();
	
	delete(trans); //freeing memory allocated to trans_state of committing transaction
	return SUCCESS;
}

/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! write() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*      Since the BTO implementation is deferred write, the only job of the protocol is to               **/
/*        add to write_set which is later validated and updated in the try commit phase                  **/
/*--------------------------------------------------------------------------------------------------------*/
int BTO::write(trans_state* trans, common_tOB* tb)
{
	//!search for data item in local buffer
	const int id = tb->ID;
	if( trans->local_buffer.find(id) != (trans->local_buffer).end() )
	{
		( (local_tOB*) (trans->local_buffer[tb->ID]) )->buffer_param = WRITE_PERFORMED;

		if( tb->size != (trans->local_buffer[tb->ID])->size )
		{
			cout<<"Error!!! SIZE MISMATCH\n";
			return SIZE_MISMATCH;                                      //3 indicates that sizes of existing and required data objects do not match
		}
		copyBytes((trans->local_buffer[tb->ID])->value, tb->value, tb->size);//Returning the value to the user
		return SUCCESS;
	}

	/*adding new write to local_buffer*/
	local_tOB* tob_temp = new local_tOB;
	tob_temp->ID        = tb->ID;
	tob_temp->size      = tb->size;
	tob_temp->value     = operator new(tb->size);
  
	copyBytes(tob_temp->value, tb->value, tb->size);
  
	tob_temp->buffer_param        = WRITE_PERFORMED;
	
	(trans->local_buffer[tb->ID]) = tob_temp;
	
	return SUCCESS;
}


/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!   try_abort()     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*       Aborting the transaction called by all the functions in case of a failure in validation step     */
/**********************************************************************************************************/
void BTO::try_abort( trans_state* trans )
{
	//cout<<"Transaction "+to_string(trans->TID)+" Aborted\n";
	delete(trans); //calls destructor of trans_state which frees the local_buffer
}

/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! get_RItr() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*  get_RItr:: to get the read-list of a shared object by supplying object id,  3rd refrence a vector     */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::get_RItr(trans_state* trans, common_tOB* tb, vector<int> &read_list)
{
	const int id = tb->ID;
	BTO_tOB* tob_temp;
	hash_STM::const_accessor a_tOB;
	if( sh_tOB_list.find(a_tOB, tb->ID) )                              //Finds the data_item in the shared memory and acquires a lock on it
		tob_temp = (BTO_tOB*) a_tOB->second;
	else
	{
		cout<<"ERROR!! from read: data Object ID = "<<(tb->ID)<<" Does not Exist"<<endl;
		a_tOB.release();
		return TOB_ABSENT;                                             //error code 2 indicates that required data object does not exist
	}
	if(tb->size != tob_temp->size)
	{
		a_tOB.release();
		return SIZE_MISMATCH;
	}
	else
	{
		timestamps *findval = (timestamps*)(tob_temp->BTO_data);       //Getting the timestamps of the tOB item
		
		//cout<<"\nSHARED OBJECT [ "<<to_string(tob_temp->ID)<<" ] Read-list  (Transaction-ID's):: [ ";
		list<int>::iterator it;
		for(it = findval->r_list->begin(); it != findval->r_list->end();it++)
		{
			//cout<<(*it)<<" ";
			read_list.push_back(*it);	
		}
		//cout<<"]\n";
		
		a_tOB.release();//release lock
	}
	return SUCCESS;
}


/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! get_WItr() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*  get_WItr:: to get the write-list of a shared object by supplying object id,  3rd refrence a vector    */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::get_WItr(trans_state* trans, common_tOB* tb, vector<int> &write_list)
{
	const int id = tb->ID;
	BTO_tOB* tob_temp;
	hash_STM::const_accessor a_tOB;
	if( sh_tOB_list.find(a_tOB, tb->ID) )                              //Finds the data_item in the shared memory and acquires a lock on it
		tob_temp = (BTO_tOB*) a_tOB->second;
	else
	{
		cout<<"ERROR!! from read: data Object ID = "<<(tb->ID)<<" Does not Exist"<<endl;
		a_tOB.release();
		return TOB_ABSENT;                                             //error code 2 indicates that required data object does not exist
	}
	if(tb->size != tob_temp->size)
	{
		a_tOB.release();
		return SIZE_MISMATCH;
	}
	else
	{
		timestamps *findval = (timestamps*)(tob_temp->BTO_data);       //timestamps of the tOB item
		
		//cout<<"\nSHARED OBJECT [ "<<to_string(tob_temp->ID)<<" ] Write-list  (Transaction-ID's):: [ ";
		list<int>::iterator it;
		for(it = findval->w_list->begin(); it != findval->w_list->end();it++)
		{
			//cout<<(*it)<<" ";
			write_list.push_back(*it);	
		}
		//cout<<"]\n";
		
		a_tOB.release();//release lock
	}
	return SUCCESS;
}

/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! create_new() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*             creates new data item in shared memory with specified ID,size and default value            */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::create_new( long long ID, int size)
{
	BTO_tOB *tob_temp = new BTO_tOB;
	if(!tob_temp)
		return MEMORY_INSUFFICIENT;                                    //unable to create new data object
    
	create_new_lock.lock();                                            //to make data object creation and max_tob_id update operations atomic
  
	hash_STM::const_accessor ca_tOB;
	if(sh_tOB_list.find(ca_tOB, ID))                                   //checking if data object with required id already exists
	{
		ca_tOB.release();
		cout<<"Data Object with specified ID already Exists"<<endl;
		create_new_lock.unlock();
		return TOB_ID_CLASH;
	}
	ca_tOB.release();
  
	tob_temp->ID    = ID;
	tob_temp->value = operator new(size);
	memset((char*)(tob_temp->value), 0, size);
	tob_temp->size  = size;
	timestamps* t   = new timestamps;                                  //Creating new timestamps object
  
	if(!t)
	{
		create_new_lock.unlock();
		return MEMORY_INSUFFICIENT;                                    //unable to allocate memory for new timestamps
	}
	t->max_read = -1;                                                  //Initializing the values
	t->max_write= -1;
  
  /*-------------------------------*/
	t->r_list = new list<int>;
	t->w_list = new list<int>;
  /*-------------------------------*/
  
  
	tob_temp->BTO_data = (void*)t;                                     //type casting to (void*) i.e. to its original data type
  
	hash_STM::accessor a_tOB;
	sh_tOB_list.insert(a_tOB, ID);
  
	a_tOB->second = tob_temp;
	//cout<<"Created Shared Object with ID = "<<ID<<" Value = "<<*((int*)(tob_temp->value))<<endl;
	a_tOB.release();
	if(ID > max_tob_id)
		max_tob_id = ID;
	create_new_lock.unlock();                                          //releasing mutex lock
	return SUCCESS;                                                    //returning of newly created data object
}




/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! create_new() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*            creates new data item in shared memory with specified size and default value                */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::create_new(int size, int& ID)
{
	create_new_lock.lock();                                            //to make data object creation and max_tob_id update operations atomic
	BTO_tOB *tob_temp = new BTO_tOB;
	if(!tob_temp)
		return MEMORY_INSUFFICIENT;                                    //unable to allocate memory to data object
	max_tob_id++;                                                      //updating max_tob_id
	tob_temp->ID    = max_tob_id;                                      //using id=previous max_tob_id+1
	tob_temp->value = operator new(size);
	
	memset((char*)(tob_temp->value),0,size);
	
	tob_temp->size  = size;
	timestamps* t   = new timestamps;                                  //Creating new timestamps object
	if(!t)
	{
		create_new_lock.unlock();
		return MEMORY_INSUFFICIENT;                                    //unable to allocate memory for timestamps
	}
	t->max_read  = -1;                                                 //Initializing the values
	t->max_write = -1;
	t->r_list    = new list<int>;
	t->w_list    = new list<int>;

	
	tob_temp->BTO_data = (void*)t;                                     //type casting to (void*) i.e. to its original data type
	hash_STM::accessor a_tOB;
	sh_tOB_list.insert(a_tOB,max_tob_id);
	a_tOB->second = tob_temp;
	
	cout<<"Created Shared Object with ID = "<<max_tob_id<<" Value = "<<*((int*)(tob_temp->value))<<endl;
	a_tOB.release();
	ID=max_tob_id;
	create_new_lock.unlock();                                          //releasing mutex lock
	return SUCCESS;                                                    //returning newly created id
}

/*--------------------------------------------------------------------------------------------------------*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! create_new() !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*--------------------------------------------------------------------------------------------------------*/
/*              creates new data object with default size and unused id in shared memory                  */
/*--------------------------------------------------------------------------------------------------------*/
int BTO::create_new(int& ID)
{

	create_new_lock.lock();                                            //to make data object creation and max_tob_id update operations atomic
	BTO_tOB *tob_temp = new BTO_tOB;
	if(!tob_temp)
		return MEMORY_INSUFFICIENT;                                    //unable to allocate memory to data object
	
	max_tob_id++;                                                      //updating max_tob_id
	tob_temp->ID    = max_tob_id;                                      //using id=previous max_tob_id+1
	tob_temp->value = operator new(DEFAULT_DATA_SIZE);
	
	memset((char*)(tob_temp->value),0,DEFAULT_DATA_SIZE);
	
	tob_temp->size  = DEFAULT_DATA_SIZE;
	timestamps* t   = new timestamps;                                  //Creating new timestamps object
	if(!t)
	{
		create_new_lock.unlock();
		return MEMORY_INSUFFICIENT;                                    //unable to allocate memory for timestamps
	}
	t->max_read  = -1;                                                 //Initializing the values
	t->max_write = -1;
	t->r_list    = new list<int>;
	t->w_list    = new list<int>;
	
	tob_temp->BTO_data = (void*)t;                                     //type casting to (void*) i.e. to its original data type
	hash_STM::accessor a_tOB;
	sh_tOB_list.insert(a_tOB,max_tob_id);
	
	a_tOB->second = tob_temp;
	cout<<"Created Shared Object with ID = "<<max_tob_id<<" Value= "<<*((int*)(tob_temp->value))<<endl;
	
	a_tOB.release();
	ID = max_tob_id;
	create_new_lock.unlock();                                          //releasing mutex lock
	return SUCCESS;                                                    //returning newly created id
}



BTO::BTO(const BTO& orig) { }

BTO::~BTO() 
{
	hash_STM::accessor a_tOB;
	sh_tOB_list.erase(a_tOB);
	sh_tOB_list.clear();
}
