/********************************************************************
|  *****     File:   FILEOPR.h                               *****  |
|  *****  Created:   July 24, 2018, 01:46 AM                 *****  |
********************************************************************/

#include "FILEOPR.h"

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!! RANDOM NUMBER GENERATER FOR ACCOUNT BALANCE !!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
float FILEOPR::getRBal( ) 
{
	//Random seed.
	random_device rd;
	
	//Initialize Mersenne Twister 
	//pseudo-random number generator.
	mt19937 gen(rd());
	
	//Uniformly distributed in range (1, 1000).
	uniform_int_distribution<> dis( 1, 1000 );
	int num = dis(gen);
	return num;
}
	
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!! RANDOM NUMBER GENERATER FOR ID !!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int FILEOPR::getRId( int numSObj) 
{
	random_device rd; 
	mt19937 gen(rd());
	uniform_int_distribution<> dis(1, numSObj); 
	int num = dis(gen);
	return num;
}
	
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!! RANDOM NUMBER GENERATER FOR FUNCTION CALL !!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
int FILEOPR::getRFunC( int nCFun ) 
{
	std::random_device rd;          
	std::mt19937 gen(rd());        
	std::uniform_int_distribution<> dis(1, nCFun);
	int num = dis(gen);
	return num;
}

	
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!! genAUs() generate and store the Atomic Unites    ! 
!!! (transactions to be executed by miner/validator) !
!!! in a list & file, nFunC: parallel fun's (AUs)    !
!!! in smart contract, numAUs: number of AUs         !
!!! to be requested by client to execute.            !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
void FILEOPR::genAUs(int numAUs, int nBidder,
					 int nFunC, vector<string>& ListAUs)
{
	int auCount = 1;
	ofstream out_file;
	out_file.open("inp-output/listAUs.txt");
	ListAUs.clear();
	
	while(auCount <= numAUs)
	{
		// Gives Smart contract func: 
		// 1 = "bid()"
		// 2 = "withdraw()"
		// 3 = "auction_end()"
		int funName = getRFunC( nFunC );
		if(funName == 1)
		{
			int payable  = getRBal( );
			int bidderID = getRId(nBidder);
			int bidValue = getRBal( );
			string t = to_string(auCount)+" bid "
						+to_string(payable)+" "
						+to_string(bidderID)+" "
						+to_string(bidValue)+"\n";
			out_file << t;
			ListAUs.push_back(t);
			auCount++;
		}
		else if (funName == 2)
		{
			int bidderID = getRId(nBidder);
			string t = to_string(auCount)+" withdraw "
						+to_string(bidderID)+"\n";

			out_file << t;
			ListAUs.push_back(t);
			auCount++;
		}
		else if (funName == 3)
		{
			string t = to_string(auCount)+" auction_end\n";
			
			out_file << t;
			ListAUs.push_back(t);
			auCount++;
		}
	}
	out_file.close ( );
	return;
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!! getInp() reads #Shared Objects,  #Threads,        !
!!! #AUs, & random delay seed "Lemda" from input file !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
void FILEOPR::getInp(int* nBidder, int *bidEndTime, 
					 int *nThreads, int* nAUs,
					 double* lemda) 
{
	//stores input from file
	string ipBuffer[5];
	ifstream inputFile;
	inputFile.open ( "inp-output/inp-params.txt" );
	while(!inputFile)
	{
		cerr<<"Error!! Unable to open <inp-params.txt>\n";
		exit(1); //call system to stop
	}
	int i = 0;
	while( !inputFile.eof( ) )
	{
		inputFile >> ipBuffer[i];
		i++;
	}
	
	// nBidder: # of Bidder;
	*nBidder = stoi(ipBuffer[0]);
	
	// bidEndTime: Time in milliseconds to stop the bid;
	*bidEndTime = stoi(ipBuffer[1]);
	
	// nThreads: # of threads;
	*nThreads = stoi(ipBuffer[2]);
	
	// nAUs: Total # of AUs or Transactions;
	*nAUs = stoi(ipBuffer[3]);
	
	// λ: random delay
	*lemda = stof(ipBuffer[4]);

	inputFile.close( );
	return;
}


/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
!!! writeOpt() stores the Time taken        !
!!! by algorithm in output file "Time.txt"  !
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
void FILEOPR::writeOpt( int nBidder, int nThreads, 
						int nAUs, double TTime[], 
						float_t mTTime[], 
						float_t vTTime[], int aCount[], 
						int vAUs, list<double>&mIT,
						list<double>&vIT)
{
	ofstream out;
	
	out.open("inp-output/Time.txt");
	
	float_t t_Time[2];
	t_Time[0] = 0;//total time miner thread
	t_Time[1] = 0;//total time validator thred

	out <<"\nTime Taken By Miner Threads:\n";
	for(int i = 0; i < nThreads; i++) 
	{
		out <<"THREAD "<< i << " = "
			<< mTTime[i] <<" microseconds\n";
		t_Time[0] = t_Time[0] + mTTime[i];
	}

	out <<"\nTime Taken By Validator Threads:\n";
	for(int i = 0; i < nThreads; i++) 
	{
		out <<"THREAD "<< i << " = "
			<< vTTime[i] <<" microseconds\n";
		t_Time[1] = t_Time[1] + vTTime[i];
	}
	
	out <<"\n[ # Proposal Shared Objects = "
		<< nBidder<<" ]\n[ # Threads = "
		<< nThreads << " ]\n[ # Total AUs = " 
		<< nAUs << " ]\n";


	int total_Abort = 0;	
	for(int i = 0; i < nThreads; i++)
		total_Abort = total_Abort + aCount[i];
	
	//	cout<<" Total Aborts = "<<total_Abort;
	out <<"[ # Total Aborts = "<<total_Abort<<" ]\n\n";
	
	//AvG Time Taken by Miner Thread = Total Time/#Threads
	out <<"\n\nAverage Time Taken by a Miner Thread = "
		<<t_Time[0]/nThreads << " microseconds\n";
	
	cout<<"\n    Avg Miner = "<<t_Time[0]/nThreads
		<<" microseconds\n";
	mIT.push_back(t_Time[0]/nThreads);

	//AvG Time Taken by Validator Thread = Total Time/#Threads
	out <<"Average Time Taken by a Validator Thread = "
		<<t_Time[1]/nThreads<<" microseconds\n";
	
	cout<<"Avg Validator = "<<t_Time[1]/nThreads
		<<" microseconds\n";
	vIT.push_back(t_Time[1]/nThreads);

	//Total Avg Time Taken by Both Algorithm Threads
	cout<<"Total (M + V) = "
		<<(t_Time[0]/nThreads + t_Time[1]/nThreads)
		<<" microseconds\n";
	
	out <<"\nTotal Average Time (Miner + Validator)  = "
		<<(t_Time[0]/nThreads + t_Time[1]/nThreads)
		<<" microseconds\n";	
	
	out.close( );
	return;
}
