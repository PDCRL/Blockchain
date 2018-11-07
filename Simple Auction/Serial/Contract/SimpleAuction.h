#pragma once
#include <chrono>

using namespace std;
class SimpleAuction
{
	public:
		// Parameters of the auction. Times are either
		// absolute unix timestamps (seconds since 1970-01-01)
		// or time periods in seconds.
		
		//------------------------------------------------------------------------
		// \'now'\ we will get using out util Timer class, will be in millisecond,
		// and \'_biddingTime'\ can be given as second. when timer start it will be gloabal time here in header file.
		// int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		//------------------------------------------------------------------------
		
		std::chrono::time_point<std::chrono::system_clock> now, start, end;
		
		int beneficiary;
		int beneficiaryAmount;
		uint auctionEnd = 0;

		// Current state of the auction.
		int highestBidder;
		int highestBid;

		struct PendReturn
		{
			int ID;
			int value;
		};
		// Allowed withdrawals of previous bids.
		// mapping(address => uint) pendingReturns;
///		int *pendingReturns;
		list<PendReturn>pendingReturns;

		// Set to true at the end, disallows any change.
		bool ended;

		// The following is a so-called natspec comment,
		// recognizable by the three slashes.
		// It will be shown when the user is asked to
		// confirm a transaction.
		/// Create a simple auction with \`_biddingTime`\
		/// seconds bidding time on behalf of the
		/// beneficiary address \`_beneficiary`\.
		
		// constructor.
		SimpleAuction( int _biddingTime, int _beneficiary, int numBidder)
		{
			for(int i = 0; i <= numBidder; i++)
			{
				PendReturn pret;
				pret.ID = i;
				pret.value = 0;
				pendingReturns.push_back(pret);
			}
			beneficiary    = _beneficiary;
			start = std::chrono::system_clock::now();
					
			cout<<"\nAuction [Start Time "<<0;
			auctionEnd     = _biddingTime;
			cout<<"] [End Time "<<auctionEnd<<"] microseconds\n";
			
			highestBidder  = 0;
			highestBid     = 0;
		};


		/// Bid on the auction with the value sent
		/// together with this transaction.
		/// The value will only be refunded if the
		/// auction is not won.
		bool bid( int payable, int bidderID, int bidValue );
		/// Withdraw a bid that was overbid.
		bool withdraw(int bidderID);
		int send(int bidderID, int amount);
		/// End the auction and send the highest bid
		/// to the beneficiary.
		bool auction_end();
				
		void HighestBidIncreased(int bidder, int amount);
		void AuctionEnded( );
	
	~SimpleAuction(){ };//destructor
};
