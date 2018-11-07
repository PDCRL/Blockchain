/********************************************************************
|  *****     File:   coarse.cpp                              *****  |
|  *****  Created:   Aug 18, 2018, 11:46 AM                  *****  |
*********************************************************************
|  *****  COMPILE:: g++ serial.cpp -pthread -std=c++14 -ltbb *****  |
********************************************************************/

#include <iostream>
#include <thread>
#include <list>
#include <atomic>
#include "Util/Timer.cpp"
#include "Contract/SimpleAuction.cpp"
#include "Util/FILEOPR.cpp"

#define maxThreads 128
#define maxBObj 5000
#define maxbEndT 100 //seconds
#define funInContract 6
#define pl "=============================\n"

using namespace std;
using namespace std::chrono;

int beneficiary = 0;       //! fixed beneficiary id to 0, it can be any unique address/id.
int    nBidder  = 2;       //! nBidder: number of bidder shared objects.
int    nThread  = 1;       //! nThread: total number of concurrent threads; default is 1.
int    numAUs;             //! numAUs: total number of Atomic Unites to be executed.
double lemda;              //! λ: random delay seed.
int    bidEndT;            //! time duration for auction.
double totalT[2];          //! total time taken by miner and validator algorithm respectively.
SimpleAuction *auction;    //! smart contract for miner.
SimpleAuction *auctionV;   //! smart contract for Validator.
int    *aCount;            //! aborted transaction count.
float_t*mTTime;            //! time taken by each miner Thread to execute AUs (Transactions).
float_t*vTTime;            //! time taken by each validator Thread to execute AUs (Transactions).
vector<string>listAUs;     //! holds AUs to be executed on smart contract: "listAUs" index+1 represents AU_ID.
std::atomic<int>currAU;    //! used by miner-thread to get index of Atomic Unit to execute.
std::atomic<int>eAUCount;  //! used by validator threads to keep track of how many valid AUs executed by validator threads.



/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!    Class "Miner" CREATE & RUN "n" miner-THREAD CONCURRENTLY           !
!"concMiner()" CALLED BY miner-THREAD TO PERFROM oprs of RESPECTIVE AUs !
! THREAD 0 IS CONSIDERED AS MINTER-THREAD (SMART CONTRACT DEPLOYER)     !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
class Miner
{
	public:
	Miner( )
	{
		//! initialize the counter used to execute the numAUs to
		//! 0, and graph node counter to 0 (number of AUs added
		//! in graph, invalid AUs will not be part of the grpah).
		currAU     = 0;

		//! index location represents respective thread id.
		mTTime = new float_t [nThread];
		aCount = new int [nThread];
		
		for(int i = 0; i < nThread; i++) 
		{
			mTTime[i] = 0;
			aCount[i] = 0;
		}
		auction = new SimpleAuction(bidEndT, beneficiary, nBidder);
	}

	//!-------------------------------------------- 
	//!!!!!! MAIN MINER:: CREATE MINER THREADS !!!!
	//!--------------------------------------------
	void mainMiner()
	{
		Timer mTimer;
		thread T[nThread];

		//!-------------------------------------
		//!!!!!! Create one Miner threads  !!!!!
		//!-------------------------------------
//		cout<<"!!!!!!!   Serial Miner Thread Started         !!!!!!\n";
		//! Start clock to get time taken by miner algorithm.
		double start = mTimer.timeReq();
		for(int i = 0; i < nThread; i++)
		{
			T[i] = thread(concMiner, i, numAUs);
		}
		//! miner thread join
		for(auto& th : T)
		{
			th.join();
		}
		//! Stop clock
		totalT[0] = mTimer.timeReq() - start;
//		cout<<"!!!!!!!   Serial Miner Thread Join            !!!!!!\n";
		

		//! print the final state of the shared objects.
//		finalState();
		cout<<"\n Number of Valid   AUs = "+to_string(numAUs-aCount[0])
				+" (AUs Executed Successfully)\n";
		cout<<" Number of Invalid AUs = "+to_string(aCount[0])+"\n";
		
		 auction->AuctionEnded( );
	}


	//!--------------------------------------------------------
	//! The function to be executed by all the miner threads. !
	//!--------------------------------------------------------
	static void concMiner( int t_ID, int numAUs)
	{
		Timer thTimer;

		//! get the current index, and increment it.
		int curInd = currAU++;

		//! statrt clock to get time taken by this.AU
		auto start = thTimer._timeStart();

		while( curInd < numAUs )
		{

			//!get the AU to execute, which is of string type.
			istringstream ss(listAUs[curInd]);

			string tmp;
			//! AU_ID to Execute.
			ss >> tmp;

			int AU_ID = stoi(tmp);

			//! Function Name (smart contract).
			ss >> tmp;

			if(tmp.compare("bid") == 0)
			{
				ss >> tmp;
				int payable = stoi(tmp);//! payable
				ss >> tmp;
				int bID = stoi(tmp);//! Bidder ID
				ss >> tmp;
				int bAmt = stoi(tmp);//! Bidder value
				bool v = auction->bid(payable, bID, bAmt);
				if(v != true)
				{
					aCount[0]++;
				}
			}
			if(tmp.compare("withdraw") == 0)
			{
				ss >> tmp;
				int bID = stoi(tmp);//! Bidder ID

				bool v = auction->withdraw(bID);
				if(v != true)
				{
					aCount[0]++;
				}
			}
			if(tmp.compare("auction_end") == 0)
			{
				bool v = auction->auction_end( );
				if(v != true)
				{
					aCount[0]++;
				}
			}
			//! get the current index to execute, and increment it.
			curInd = currAU++;
		}
		mTTime[t_ID] = mTTime[t_ID] + thTimer._timeStop(start);
	}


	//!-------------------------------------------------
	//!FINAL STATE OF ALL THE SHARED OBJECT. Once all  |
	//!AUs executed. we are geting this using state_m()|
	//!-------------------------------------------------
	void finalState()
	{
		
		for(int id = 1; id <= nBidder; id++) 
		{
//			ballot->state(id, false);//for voter state
		}
		cout<<"\n Number of Valid   AUs = "+to_string(numAUs - aCount[0])
				+" (AUs Executed Successfully)\n";
		cout<<" Number of Invalid AUs = "+to_string(aCount[0])+"\n";
	}

	~Miner() { };
};



/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
! Class "Validator" CREATE & RUN "1" validator-THREAD CONCURRENTLY BASED ON CONFLICT GRPAH!
! GIVEN BY MINER. "concValidator()" CALLED BY validator-THREAD TO PERFROM OPERATIONS of   !
! RESPECTIVE AUs. THREAD 0 IS CONSIDERED AS MINTER-THREAD (SMART CONTRACT DEPLOYER)       !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
class Validator
{
public:

	Validator(int chairperson)
	{
		//! initialize the counter used to execute the numAUs to
		//! 0, and graph node counter to 0 (number of AUs added
		//! in graph, invalid AUs will not be part of the grpah).
		currAU     = 0;

		//! index location represents respective thread id.
		vTTime = new float_t [nThread];
		aCount = new int [nThread];
		
		for(int i = 0; i < nThread; i++) 
		{
			vTTime[i] = 0;
			aCount[i] = 0;
		}
		auctionV = new SimpleAuction(bidEndT, beneficiary, nBidder);
	}

	/*!---------------------------------------
	| create n concurrent validator threads  |
	| to execute valid AUs in conflict graph.|
	----------------------------------------*/
	void mainValidator()
	{
		Timer vTimer;
		thread T[nThread];

		//!--------------------------_-----------
		//!!!!! Create one Validator threads !!!!
		//!--------------------------------------
//		cout<<"!!!!!!!   Serial Validator Thread Started     !!!!!!\n";
		//!Start clock
		double start = vTimer.timeReq();
		for(int i = 0; i<nThread; i++)
		{
			T[i] = thread(concValidator, i);
		}
		//!validator thread join.
		for(auto& th : T)
		{
			th.join( );
		}
		//!Stop clock
		totalT[1] = vTimer.timeReq() - start;
//		cout<<"!!!!!!!   Serial Validator Thread Join        !!!!!!\n\n";

		//!print the final state of the shared objects by validator.
//		finalState();
		auctionV->AuctionEnded( );
	}

	//!-------------------------------------------------------
	//! Function to be executed by all the validator threads.!
	//!-------------------------------------------------------
	static void concValidator( int t_ID )
	{
		Timer thTimer;

		//! get the current index, and increment it.
		int curInd = currAU++;

		//! statrt clock to get time taken by this.AU
		auto start = thTimer._timeStart();

		while( curInd < numAUs )
		{

			//!get the AU to execute, which is of string type.
			istringstream ss(listAUs[curInd]);

			string tmp;
			//! AU_ID to Execute.
			ss >> tmp;

			int AU_ID = stoi(tmp);

			//! Function Name (smart contract).
			ss >> tmp;

			if(tmp.compare("bid") == 0)
			{
				ss >> tmp;
				int payable = stoi(tmp);//! payable
				ss >> tmp;
				int bID = stoi(tmp);//! Bidder ID
				ss >> tmp;
				int bAmt = stoi(tmp);//! Bidder value
				bool v = auctionV->bid(payable, bID, bAmt);
				if(v != true)
				{
					aCount[0]++;
				}
			}
			if(tmp.compare("withdraw") == 0)
			{
				ss >> tmp;
				int bID = stoi(tmp);//! Bidder ID

				bool v = auctionV->withdraw(bID);
				if(v != true)
				{
					aCount[0]++;
				}
			}
			if(tmp.compare("auction_end") == 0)
			{
				bool v = auctionV->auction_end( );
				if(v != true)
				{
					aCount[0]++;
				}
			}
			//! get the current index to execute, and increment it.
			curInd = currAU++;
		}
		vTTime[t_ID] = vTTime[t_ID] + thTimer._timeStop(start);
	}


	//!-------------------------------------------------
	//!FINAL STATE OF ALL THE SHARED OBJECT. Once all  |
	//!AUs executed. we are geting this using get_bel()|
	//!-------------------------------------------------
	void finalState()
	{

		for(int id = 1; id <= nBidder; id++) 
		{
//			ballot_v->state(id, false);//for voter state
		}
	}
	~Validator() { };
};




/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!   main() !!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int main( )
{
	//! list holds the avg time taken by miner and Validator
	//! thread s for multiple consecutive runs.
	list<double>mItrT;
	list<double>vItrT;

	cout<<pl<<" Serial Algorithm \n"<<pl;
	int totalRun = 1;
	for(int numItr = 0; numItr < totalRun; numItr++)
	{
		FILEOPR file_opr;

		//! read from input file:: nBidder = #numProposal; nThread = #threads;
		//! numAUs = #AUs; λ = random delay seed.
		file_opr.getInp(&nBidder, &bidEndT, &nThread, &numAUs, &lemda);
		
		//!------------------------------------------------------------------
		//! Num of threads should be 1 for serial so we are fixing it to 1, !
		//! Whatever be # of threads in inputfile, it will be always one.   !
		//!------------------------------------------------------------------
		nThread = 1;

		//! max Proposal shared object error handling.
		if(nBidder > maxBObj) 
		{
			nBidder = maxBObj;
			cout<<"Max number of Bid Shared Object can be "<<maxBObj<<"\n";
		}
		if(bidEndT > maxbEndT) 
		{
			bidEndT = maxbEndT;
			cout<<"Max Bid End Time can be "<<maxbEndT<<"\n";
		}
		 //! generates AUs (i.e. trans to be executed by miner & validator).
		file_opr.genAUs(numAUs, nBidder, funInContract, listAUs);

		//MINER
		Miner *miner = new Miner();
		miner ->mainMiner();

		//VALIDATOR
		Validator *validator = new Validator(0);
		validator ->mainValidator();

		//! total valid AUs among total AUs executed 
		//! by miner and varified by Validator.
		int vAUs = numAUs-aCount[0];
		
		file_opr.writeOpt(nBidder, nThread, numAUs, 
							totalT, mTTime, vTTime, 
							aCount, vAUs, mItrT, vItrT);
		cout<<"\n===================== Execution "
			<<numItr+1<<" Over =====================\n"<<endl;

		listAUs.clear();
		delete miner;
		delete validator;
	}
	
	double tAvgMinerT = 0, tAvgValidT = 0;//to get total avg miner and validator time after number of totalRun runs.
	auto mit = mItrT.begin();
	auto vit = vItrT.begin();
	for(int j = 0; j < totalRun; j++)
	{
		tAvgMinerT = tAvgMinerT + *mit;
//		cout<<"\n    Avg Miner = "<<*mit;
		tAvgValidT = tAvgValidT + *vit;
//		cout<<"\nAvg Validator = "<<*vit;
		mit++;
		vit++;
	}
	tAvgMinerT = tAvgMinerT/totalRun;
	tAvgValidT = tAvgValidT/totalRun;
	cout<<pl<<"    Total Avg Miner = "<<tAvgMinerT<<" microseconds";
	cout<<"\nTotal Avg Validator = "<<tAvgValidT<<" microseconds";
	cout<<"\n  Total Avg (M + V) = "<<tAvgMinerT+tAvgValidT<<" microseconds";
	cout<<"\n"<<pl;

	mItrT.clear();
	vItrT.clear();

	delete mTTime;
	delete vTTime;
	delete aCount;

return 0;
}
