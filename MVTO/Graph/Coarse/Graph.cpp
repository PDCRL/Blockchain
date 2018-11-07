/********************************************************************
|  *****     File:   Graph.h                                 *****  |
|  *****  Created:   on July 31, 2018,01:46 AM               *****  |
********************************************************************/

#include "Graph.h"

/***********************************************************/
/*!   Function used to create and add AU node in graph.    */
/***********************************************************/	
void Graph::add_node(int A_ID, int ts_AU, Graph_Node **ref)
{
	Graph_Node *pred = verHead, *curr;
	findVWind(&pred, &curr, A_ID);
	
	//! if vertex node is not in the grpah.
	if(curr->AU_ID != A_ID)
	{
		//! validation Successful.
		Graph_Node *temp     = new Graph_Node;
		temp->AU_ID          = A_ID;
		temp->ts             = ts_AU;
		temp->in_count       = 0;
		temp->edgeHead->next = temp->edgeTail;
		temp->next           = curr;
		pred->next           = temp;
		*ref = temp;
		return;
	}
	if(curr->AU_ID == A_ID) *ref = curr;
}


/***********************
! Find vertex in graph.!
***********************/
void Graph::findVWind(Graph_Node **ver_pre, Graph_Node **ver_curr, int key)
{
	Graph_Node *pre = *ver_pre, *curr = pre->next;
	while(curr != verTail && key < curr->AU_ID)
	{
		pre  = curr;
		curr = curr->next;
	}
	*ver_pre  = pre;
	*ver_curr = curr;
}

/***********************************************************/
/*!   Function used to create and add Edges in graph.      */
/***********************************************************/
void Graph::add_edge(int from, int to, int from_ts, int to_ts )
{
	Graph_Node *fromRef, *toRef;
	
	//! if vertex node "from" is not present in graph, add "from" node.
	add_node(from, from_ts, &fromRef);
	
	//! if vertex node "to" is not present in graph, add "to" node.
	add_node(to, to_ts, &toRef);

	Graph_Node *ver_pre = verHead, *ver_curr;
	//! find "from" node in Grpah; ver_curr points to "from" node. 
	//! Remove the findVWind() comment to remove the effect of optimization. 
//	findVWind(&ver_pre, &ver_curr, from);

	//! Optimization.
	ver_curr = fromRef;
	
	//! check for duplicate Edge in edge list of "from" vertex node.
	EdgeNode *edg_pre = ver_curr->edgeHead, *edg_curr, *vcurrEdgTail = ver_curr->edgeTail;
	findEWind(vcurrEdgTail, &edg_pre, &edg_curr, to);
	
	//! find duplicate edge, i.e., if edge added eariler return.
	if(edg_curr->au_id == to) return;

	//!-------------------------------------------------------------------
	//! Actuall creation of edge. Find "to" vertex node in graph to::
	//! (1) increment in_count because of edge ("from" node-->"to" node);
	//! (2) to store ref to "to" node in EdgeNode.
	//!-------------------------------------------------------------------
	Graph_Node *refCurr, *refPre = verHead;
	//! Remove the findVWind() comment to remove the effect of optimization.
//	findVWind(&refPre, &refCurr, to);
	
	//! refCurr: point to "this.to" node. Optimization.
	refCurr = toRef;
	
	//! in-count is atomic variable.
	refCurr->in_count++;

	while(true)
	{
		//! validation successfull add edge.
		if(edg_pre->next == edg_curr && ver_curr->AU_ID == from)
		{
			EdgeNode *temp_edg  = new EdgeNode;
			temp_edg->au_id     = to;
			temp_edg->next      = NULL;
			temp_edg->ref       = refCurr;//! refrence to "to" vertex node.
			temp_edg->next      = edg_curr;
			edg_pre ->next      = temp_edg;
			break;
		}
		else
			findEWind(vcurrEdgTail, &edg_pre, &edg_curr, to);
	}
}

/*******************************************
! Find window to add new edge in edge list.!
*******************************************/
void Graph::findEWind(EdgeNode* edgeTail, 
		EdgeNode **edg_pre, EdgeNode **edg_curr, int key)
{
	EdgeNode *pre  = *edg_pre;
	EdgeNode *curr = pre->next;
	while(curr != edgeTail && key < curr->au_id)
	{
		pre   = curr;
		curr  = curr->next;
	}
	*edg_pre  = pre;
	*edg_curr = curr;
}

/**************
! Print grpah.!
**************/
void Graph::print_grpah()
{
	cout<<"==============================================================\n";
	cout<<"  Grpah Node | In-Degree | Time Stamp | Out Edges (AU_IDs)\n";
	cout<<"==============================================================\n";
	Graph_Node* g_temp = verHead->next;

	while( g_temp != verTail )
	{
		EdgeNode *o_temp = g_temp->edgeHead->next;
		cout<<"\t"+to_string(g_temp->AU_ID)+" \t   "+
			to_string(g_temp->in_count)+"\t\t "+to_string(g_temp->ts)+" \t";

		while(o_temp != g_temp->edgeTail)
		{
			cout<< "-->"+to_string(o_temp->au_id)+"";
			o_temp = o_temp->next;
		}
		cout<<endl;
		g_temp = g_temp->next;
	}
	cout<<"==============================================================\n";
}
