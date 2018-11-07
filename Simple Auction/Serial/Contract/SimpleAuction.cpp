#include "SimpleAuction.h"
/// Bid on the auction with the value sent
/// together with this transaction.
/// The value will only be refunded if the
/// auction is not won.
bool SimpleAuction::bid( int payable, int bidderID, int bidValue )
{
	// No arguments are necessary, all
	// information is already part of
	// the transaction. The keyword payable
	// is required for the function to
	// be able to receive Ether.

	// Revert the call if the bidding
	// period is over.
	
	auto end = high_resolution_clock::now();
	double_t now = duration_cast<milliseconds>( end - start ).count();
//	cout<<"\nNow time in bid () \t\t"+to_string(now);
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
		// Sending back the money by simply using
		// highestBidder.send(highestBid) is a security risk
		// because it could execute an untrusted contract.
		// It is always safer to let the recipients
		// withdraw their money themselves.
		
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
	HighestBidIncreased(bidderID, bidValue);
	return true;
}


/// Withdraw a bid that was overbid.
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

/*!----------------------------------------------------------------*/
//! This fun can also be implemented as method call of other smart !
//! contract. Here we assue that this is always successfull.	   !
/*!----------------------------------------------------------------*/
int SimpleAuction::send(int bidderID, int amount)
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

	// we assue this fun always returns true.
	return 1;
}

/// End the auction and send the highest bid
/// to the beneficiary.
bool SimpleAuction::auction_end()
{
	// It is a good guideline to structure functions that interact
	// with other contracts (i.e. they call functions or send Ether)
	// into three phases:
	// 1. checking conditions
	// 2. performing actions (potentially changing conditions)
	// 3. interacting with other contracts
	// If these phases are mixed up, the other contract could call
	// back into the current contract and modify the state or cause
	// effects (ether payout) to be performed multiple times.
	// If functions called internally include interaction with external
	// contracts, they also have to be considered interaction with
	// external contracts.


	// 1. Conditions
	auto end     = high_resolution_clock::now();
	double_t now = duration_cast<milliseconds>( end - start ).count();
//	cout<<"\nNow time in auction_end () \t"+to_string(now);
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

void SimpleAuction::HighestBidIncreased(int bidder, int amount)
{
	highestBidder = bidder;
	highestBid    = amount;
}
void SimpleAuction::AuctionEnded( )
{
	cout<<"\n======================================";
	cout<<"\n| Auction Winer ID "+to_string(highestBidder)
			+" |  Amount "+to_string(highestBid);
	cout<<"\n======================================\n";	
}
