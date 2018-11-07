/********************************************************************
|  *****     File:   Graph.h                                 *****  |
|  *****  Created:   on Aug 01, 2018,05:40 PM                *****  |
********************************************************************/
#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <atomic>

class Graph
{
	public: 
		struct EdgeNode                          //Structure for conflicting edge list of current AUs.
		{
			int   au_id;                         // au_id = denotes conflictig atomic unit ID
			std::atomic<EdgeNode*> next;
			void *ref;                           // used by validator to decement in_degree of Grpah_Node pointed by this ref 
		};

		//Structure for graph of Atomic Units.
		struct Graph_Node
		{
			int  AU_ID;                         // AU_ID    = Atomic unit ID
			int  ts;                            // ts       = Begining timestamp of AU
			bool marked     = false;            // marked   = logical field used for validator in parallel execultion
			std::atomic<int> in_count;          // in_count = denotes # of incident edges on node
			EdgeNode *edgeHead  = new EdgeNode; // edgeHead = head of list of AUs on which edge is incident from current node
			EdgeNode *edgeTail  = new EdgeNode;
			std::atomic<Graph_Node*> next;

		}*g_start, *verHead, *verTail;
		
		Graph()          //constructor
		{
			verHead        = new Graph_Node;
			verTail        = new Graph_Node;
			verHead->AU_ID = -1;                // vertex initial sentianl node
			verTail->AU_ID = 2147483647;        // vertex end sentinal node
			verHead->next  = verTail;
			
			verHead->edgeHead->au_id = -1;
			verHead->edgeTail->au_id = 2147483647;
			verHead->edgeHead->next  = verHead->edgeTail;
			
			verTail->edgeHead->au_id = -1;
			verTail->edgeTail->au_id = 2147483647;
			verTail->edgeHead->next  = verTail->edgeTail;
		}; 

		
		void add_node(int A_ID, int ts_AU, Graph_Node **ref);
		void add_edge(int from, int to, int from_ts, int to_ts );         //logic to add edges between atomic unit in graph

		void findEWind(EdgeNode* edgeTail, EdgeNode **edg_pre, EdgeNode **edg_curr, int key);//g_temp is current vertex node
		void findVWind(Graph_Node **ver_pre, Graph_Node **ver_curr, int key);

		void print_grpah();
		//destructor
		~Graph() 
		{
			verHead = NULL;
			delete verHead;
			verTail = NULL;
			delete verTail;
		 };
};
