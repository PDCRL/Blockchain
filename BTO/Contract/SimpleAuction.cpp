#include "SimpleAuction.h"

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*!!!FUNCTIONS FOR VALIDATOR !!!*/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
//! RESETING TIMEPOIN FOR VALIDATOR.
void SimpleAuction::reset()
{
	beneficiaryAmount = 0;
	start = std::chrono::system_clock::now();
	cout<<"AUCTION [Start Time = "<<0;
	cout<<"] [End Time = "<<auctionEnd<<"] milliseconds";
}


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*! VALIDATOR:: Bid on the auction with the value sent together with this  !*/
/*! transaction. The value will only be refunded if the auction is not won.!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
bool SimpleAuction::bid( int payable, int bidderID, int bidValue )
{
	// No arguments are necessary, all information is already part of trans
	// -action. The keyword payable is required for the function to be able
	// to receive Ether. Revert the call if the bidding period is over.
	
	auto end = high_resolution_clock::now();
	double_t now = duration_cast<milliseconds>( end - start ).count();

	if( now > auctionEnd)
	{
//		cout<<"\nAuction already ended.";
		return false;
	}
	// If the bid is not higher, send the
	// money back.
	if( bidValue <= highestBid)
	{
//		cout<<"\nThere already is a higher bid.";
		return false;
	}
	if (highestBid != 0) 
	{
		// Sending back the money by simply using highestBidder.send(highestBid)
		// is a security risk because it could execute an untrusted contract.
		// It is always safer to let recipients withdraw their money themselves.
		//pendingReturns[highestBidder] += highestBid;
		
		list<PendReturn>::iterator pr = pendingReturns.begin();
		for(; pr != pendingReturns.end(); pr++)
		{
			if( pr->ID == highestBidder)
				break;
		}
		if(pr == pendingReturns.end() && pr->ID != highestBidder)
		{
			cout<<"\nError:: Bidder "+to_string(highestBidder)+" not found.\n";
		}
		pr->value = highestBid;
	}
	//HighestBidIncreased(bidderID, bidValue);
	highestBidder = bidderID;
	highestBid    = bidValue;

	return true;
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*! VALIDATOR:: Withdraw a bid that was overbid. !*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
bool SimpleAuction::withdraw(int bidderID)
{
	list<PendReturn>::iterator pr = pendingReturns.begin();
	for(; pr != pendingReturns.end(); pr++)
	{
		if( pr->ID == bidderID)
			break;
	}
	if(pr == pendingReturns.end() && pr->ID != bidderID)
	{
		cout<<"\nError:: Bidder "+to_string(bidderID)+" not found.\n";
	}

///	int amount = pendingReturns[bidderID];
	int amount = pr->value;
	if (amount > 0) 
	{
		// It is important to set this to zero because the recipient
		// can call this function again as part of the receiving call
		// before `send` returns.
///		pendingReturns[bidderID] = 0;
		pr->value = 0;
		if ( !send(bidderID, amount) )
		{
			// No need to call throw here, just reset the amount owing.
///			pendingReturns[bidderID] = amount;
			pr->value = amount;
			return false;
		}
	}
	return true;
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* VALIDATOR:: this fun can also be impelemted !*/
/* as method call to other smart contract. we  !*/
/* assume this fun always successful in send.  !*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int SimpleAuction::send(int bidderID, int amount)
{
//	bidderAcount[bidderID] += amount;
	return 1;
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* VALIDATOR:: End the auction and send the highest bid to the beneficiary. !*/
/*!_________________________________________________________________________!*/
/*! It's good guideline to structure fun that interact with other contracts !*/
/*! (i.e. they call functions or send Ether) into three phases: 1.checking  !*/
/*! conditions, 2.performing actions (potentially changing conditions), 3.  !*/
/*! interacting with other contracts. If these phases mixed up, other cont- !*/
/*! -ract could call back into current contract & modify state or cause     !*/
/*! effects (ether payout) to be performed multiple times. If fun called    !*/
/*! internally include interaction with external contracts, they also have  !*/ 
/*! to be considered interaction with external contracts.                   !*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
bool SimpleAuction::auction_end()
{
	// 1. Conditions
	auto end     = high_resolution_clock::now();
	double_t now = duration_cast<milliseconds>( end - start ).count();

	if(now < auctionEnd)
    {
//    	cout<< "\nAuction not yet ended.";
    	return false;
    }
	if(!ended)
    {
//    	AuctionEnded(highestBidder, highestBid);
//		cout<<"\nAuctionEnd has already been called.";
    	return true;
	}
	// 2. Effects
	ended = true;
//	AuctionEnded( );

	// 3. Interaction
	///beneficiary.transfer(highestBid);
	beneficiaryAmount = highestBid;
	return true;
}


void SimpleAuction::AuctionEnded( )
{
	cout<<"\n======================================";
	cout<<"\n| Auction Winer ID "+to_string(highestBidder)
			+" |  Amount "+to_string(highestBid);
	cout<<"\n======================================\n";	
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*!! FUNCTIONS FOR MINER !!!*/
/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*! MINER:: Bid on the auction with the value sent together with this      !*/
/*! transaction. The value will only be refunded if the auction is not won.!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int SimpleAuction::bid_m( int payable, int bidderID, int bidValue, 
							int *ts, list<int> &cList)
{
	auto end = high_resolution_clock::now();
	double_t now = duration_cast<milliseconds>( end - start ).count();

	if( now > auctionEnd)
	{
		return -1;//! invalid AUs: Auction already ended.
	}
	
	long long error_id;
	trans_state *T   = lib->begin();     //! Transactions begin.
	*ts              = T->TID;           //! return time_stamp to user.

	common_tOB *higestBid = new common_tOB;
	higestBid->size       = sizeof(int);
	higestBid->ID         = -2;           //! highestBid SObj id is -2. 

	int hbR  = lib->read(T, higestBid);   //! read from STM Shared Memoy.
	if(hbR != SUCCESS)
	{
		return 0; //! AU aborted.
	}

	common_tOB *hBidder = new common_tOB;
	hBidder->size       = sizeof(int);
	hBidder->ID         = -1;             //! highestBidder SObj id is -1. 
	int hbrR = lib->read(T, hBidder);     //! read from STM Shared Memoy.
	if(hbrR != SUCCESS)
	{
		return 0; //! AU aborted.
	}

	if( bidValue <= *(int*)higestBid->value )
	{
		lib->try_abort( T );
		return -1;//! invalid AUs: There already is a higher bid.
	}	
		
	// If the bid is no longer higher, send the money back to old bidder.
	if (*(int*)higestBid->value != 0) 
	{
		int hBiderID = *(int*)hBidder->value; //! higestBidder in STM memory. 
		common_tOB *bidr = new common_tOB;
		bidr->size       = sizeof(int);
		bidr->ID         = hBiderID;

		int bR = lib->read(T, bidr);              //! read from STM Shared Memoy. 
		if(bR != SUCCESS)
		{
			return 0; //! AU aborted.
		}
		//pendingReturns[highestBidder] += highestBid;
		*(int*)bidr->value += *(int*)higestBid->value;
		
		int bW = lib->write(T, bidr);
		if( bW != SUCCESS)
		{
			lib->try_abort( T );
			return 0;//AU aborted.
		}
	}
	// increase the highest bid.
	*(int*)hBidder->value   = bidderID;
	*(int*)higestBid->value = bidValue;

	int hbrW = lib->write(T, hBidder);
	int hbW  = lib->write(T, higestBid);
	if(hbrW != SUCCESS && hbW != SUCCESS)
	{
		lib->try_abort( T );
		return 0;//AU aborted.
	}
	int tc = lib->try_commit_conflict(T, error_id, cList);
	if( tc == SUCCESS)
	{
		return 1;//bid successfully done; AU execution successful.
	}
	else
	{
		return 0;//AU aborted.
	}
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*! MINER:: Withdraw a bid that was overbid. !*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int SimpleAuction::withdraw_m(int bidderID, int *ts, list<int> &cList)
{
	long long error_id;
	trans_state *T   = lib->begin();     //! Transactions begin.
	*ts              = T->TID;           //! return time_stamp to user.

	common_tOB *bidr = new common_tOB;
	bidr->size       = sizeof(int);
	bidr->ID         = bidderID;        //! Bidder SObj in STM memory. 

	int bR = lib->read(T, bidr);        //! read from STM Shared Memoy. 
	if(bR != SUCCESS) 
	{
		return 0;         //! AU aborted.
	}
	//int amount = pendingReturns[bidderID];
	int amount = *(int*)bidr->value;
	if (amount > 0) 
	{
		//pendingReturns[bidderID] = 0;
		*(int*)bidr->value = 0;
		int bW = lib->write(T, bidr);
		if( bW != SUCCESS )
		{
			lib->try_abort( T );
			return 0;//AU aborted.
		}
		
		if ( !send(bidderID, amount) )
		{
			// No need to call throw here, just reset the amount owing.
			*(int*)bidr->value = amount;
			int bW = lib->write(T, bidr);
			if( bW != SUCCESS )
			{
				lib->try_abort( T );
				return 0;//AU aborted.
			}
			int tc = lib->try_commit_conflict(T, error_id, cList);
			if( tc == SUCCESS)
			{
				return -1;//AU invalid.
			}
			else
			{
				return 0; //AU aborted.
			}
		}
	}
	int tc = lib->try_commit_conflict(T, error_id, cList);
	if( tc == SUCCESS)
	{
		return 1;//withdraw successfully done; AU execution successful.
	}
	else
	{ 
		return 0; //AU aborted.
	}
}


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* MINER:: End the auction and send the highest bid to the beneficiary. !*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int SimpleAuction::auction_end_m(int *ts, list<int> &cList)
{
	auto end     = high_resolution_clock::now();
	double_t now = duration_cast<milliseconds>( end - start ).count();

	if(now < auctionEnd)
    {
    	//! Auction not yet ended.
    	return -1;
    }
    
    long long error_id;
    trans_state *T   = lib->begin();     //! Transactions begin.
	*ts              = T->TID;           //! return time_stamp to user.
	common_tOB *bEnd = new common_tOB;
	bEnd->size       = sizeof(bool);
	bEnd->ID         = 0;                //! end SObj id is 0 in STM memory. 

	int bR = lib->read(T, bEnd);         //! read from STM Shared Memoy. 
	if(bR != SUCCESS) 
	{
		return 0; //! AU aborted.
	}
	bool ended = *(bool*)bEnd->value;
	if( !ended )
    {
		//! AuctionEnd has already been called.
		lib->try_abort( T );
    	return 1;
	}
//	ended = true;
	*(bool*)bEnd->value = true;
	int bW = lib->write(T, bEnd);
	if( bW != SUCCESS )
	{
		lib->try_abort( T );
		return 0;//AU aborted.
	}

	common_tOB *hBidder = new common_tOB;
	hBidder->size       = sizeof(int);
	hBidder->ID         = -1;             //! highestBidder SObj id is -1. 
	
	int hbrR = lib->read(T, hBidder);     //! read from STM Shared Memoy. 
	if(hbrR != SUCCESS)
	{
		return 0; //! AU aborted.
	}
	beneficiaryAmount = *(int*)hBidder->value;
	
	int tc = lib->try_commit_conflict(T, error_id, cList);
	if( tc == SUCCESS)
	{
		return 1;//auction_end successfully done; AU execution successful.
	}
	else
	{ 
		return 0; //AU aborted.
	}
}

bool SimpleAuction::AuctionEnded_m( )
{
	long long error_id;
	trans_state *T   = lib->begin();     //! Transactions begin.
	
	common_tOB *higestBid = new common_tOB;
	higestBid->size       = sizeof(int);
	higestBid->ID         = -2;           //! highestBid SObj id is -2. 

	int hbR  = lib->read(T, higestBid);   //! read from STM Shared Memoy.
	if(hbR != SUCCESS)
	{
		return false; //! AU aborted.
	}

	
	common_tOB *hBidder = new common_tOB;
	hBidder->size       = sizeof(int);
	hBidder->ID         = -1;             //! highestBidder SObj id is -1.	
	int hbrR = lib->read(T, hBidder);     //! read from STM Shared Memoy. 
	if(hbrR != SUCCESS)
	{
		return false; //! AU aborted.
	}
	cout<<"\n======================================";
	cout<<"\n| Auction Winer ID "+to_string(*(int*)hBidder->value)
			+" |  Amount "+to_string(*(int*)higestBid->value);
	cout<<"\n======================================\n";	
	
	int tc = lib->try_commit(T, error_id);
	if( tc == SUCCESS)
	{
		return true;//auction_end successfully done; AU execution successful.
	}
	else
	{ 
		return false; //AU aborted.
	}
}
