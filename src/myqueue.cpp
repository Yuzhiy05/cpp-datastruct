#include<iostream>
using namespace std;

typedef struct LinkNode  
{
	int data;
	struct LinkNode *next;
	
}LinkNode;

typedef struct LinkQueues 
{
	LinkNode *front,*rear;
}LinkQueue;

void InitQueue(LinkQueue &Q);
bool IsEmpty(LinkQueue Q);
void Enqueue(LinkQueue &Q,int x);
bool DeQueue(LinkQueue &Q,int x);

void InitQueue(LinkQueue &Q){
	Q.front=Q.rear=(LinkNode *)malloc(sizeof(LinkNode));
	Q.front->next=NULL;
}
bool IsEmpty(LinkQueue Q){
	if(Q.front==Q.rear) {cout<< true;return true;}
	else {cout<<true;return false;}

}

void EnQueue(LinkQueue &Q,int x){
	LinkNode *s=(LinkNode*)malloc(sizeof(LinkNode));
	s->data=x;s->next=NULL;
	Q.rear->next=s;
	Q.rear=s;
}

bool DeQueue(LinkQueue &Q,int x){
	if(IsEmpty(Q)) return false;
	LinkNode *p=Q.front->next;
	x=p->data;
	Q.front->next=p->next;
	if(Q.rear==p)
		Q.rear=Q.front;
	free(p);
	return true;
}


/*int main(){
	LinkQueue S;
	InitQueue(S);
	for (int i = 0; i < 5; ++i)
	{
		EnQueue(S,i);
	}
	IsEmpty(S);
	for (int i = 0; i < 5; ++i)
	{
		cout<<S.front->data<<endl;
	}*/
	/*EnQueue(S,1);
	EnQueue(S,4);
	EnQueue(S,3);
	EnQueue(S,4);
	cout<<S.front->next->next->data;
	return 0;
}*/

