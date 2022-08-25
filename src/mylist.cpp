#include<iostream>
using namespace std;
typedef int ElemType;
typedef struct LNode{
	ElemType data;
	struct LNode *next;
}LNode,*LinkList;

LinkList List_HeadInsert(LinkList &L){  //头插法建立单链表
	LNode *s;int x;
	L=(LinkList)malloc(sizeof(LNode)); //创建头节点
	L->next=NULL;                      //初始为空链表
	cin>>x;							   //输入节点的值
	while(x!=9999){
		s=(LNode*)malloc(sizeof(LNode));
		s->data=x;
		s->next=L->next;
		L->next=s;
		cin>>x;
	}
	return L;
}
//读入数据的顺序与生成链表的中的元素相反

LinkList List_TailInserrt(LinkList &L){ //尾插法建立单链表
	ElemType x;
	L=(LinkList)malloc(sizeof(LNode));
	LNode *s,*r=L;						//r为表尾指针
	cin>>x;
	while(x!=9999){
		s=(LNode*)malloc(sizeof(LNode)); //先将表尾指针指向插入节点，当前节点插入后再将尾指针向后移动，新插入的节点为尾节点
		s->data=x;
		r->next=s;
		r=s;
		cin>>x;
	}
	r->next=NULL;
	return L;                		//当所有结点插入完后尾节点next指针为空
}

LNode *GetElem(LinkList L,int i){ //因为不改变链表L的数据所以使用L（此时相当于复制了链表L进行操作增加了内存开销），当然也可以使用&L
	int j=1;				//从一开始计数
	LNode *p=L->next;       //将头节点指针赋给P
	if(i==0)
	return L;
	if(i<1)
	return NULL;
	while(p&&j<i){   //&&双真为真有一个假就假  表达式中的P表示P存在为真 while()表达式中为真就循环直到为假
		p=p->next;
		j++;
	}
	return p;
}
//按序号查找


LNode *LocateElem(LinkList L,ElemType e){
	LNode *p=L->next;
	while(p!=NULL&&p->data!=e)		//p不为空，p节点值不为e就一直循环
		p=p->next;
	return p;
}
//按值查找


void Insert(LinkList &L,ElemType x,int i){  //后插法
	LNode *p,*s;
	s=(LNode*)malloc(sizeof(LNode));
	p=GetElem(L,i-1);
	s->data=x;           		//寻找需要插入节点的前驱
	s->next=p->next;
	p->next=s;
}

void PriorInset(LinkList &L,ElemType x,int i){ //前插法，先进行后插法再交换数据域
	LNode *s,*p;
	s=(LNode*)malloc(sizeof(LNode));
	p=GetElem(L,i-1);
	s->next=p->next;
	p->next=s;
	s->data=p->data;
	p->data=x;
}

void Delete(LinkList &L,int i){ //删除i节点
	LNode *p,*q;					//p指针指向被删除节点的前驱，q指向被删除节点
	p=GetElem(L,i-1);
	q=p->next;						//先保证p节点的后继节点指向q，对上号了
	p->next=q->next;				//保证不能断链，将q的后继节点给p（先接上）
	free(q);						//此时可以删除
}
void DeleteRear(LinkList &L,int i){
	LNode *p,*q;
	p=GetElem(L,i-1);
	q=p->next;
	p->next=q->next;
	p->data=q->next->data;
	free(q);
}

int LengthList(LinkList &L){
	int i=0;
	LNode *p;
	p=L->next;
	while(p==NULL)
		return 0;
	while(p){
		i++;
		p=p->next;
	}
	return i;
}