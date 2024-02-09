/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct { // represents a pool of connected descriptors
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

int byte_cnt = 0; //count total bytes recieved by server

void init_pool(int listenfd, pool* p);
void add_client(int connfd, pool* p);
void check_clients(pool* p);


/* 주식 기능 구현 */
char printbuf[MAXLINE]; /* 명령 수행결과 출력하기 위한 배열 */

typedef struct item { /* 주식 item */
    int ID;
    int left_stock;
    int price;
    int readcnt;
}item;

typedef struct node* node_pointer;
typedef struct node { /* 이진트리의 노드 */
    item stock;
    node_pointer left_child;
    node_pointer right_child;
}node;

node_pointer root = NULL; /* 이진트리 */

node_pointer create_node(item stock); /* 노드 생성 */
node_pointer insert_node(node_pointer root, item stock); /* 이진트리에 노드 삽입 */
node_pointer read_stock(); /* stock.txt를 읽어 binary tree 형태로 저장한다 */
void write_inorder(node_pointer root, FILE* fp);
void write_stock(node_pointer root); /* 현재 주식 상태를 stock.txt에 쓴다 */
void show_stock(int connfd, node_pointer root); /* 현재 주식 상태를 보여준다 */
struct node* tree_search(node_pointer root, int id); /* 트리에서 id를 가진 노드를 찾아 반환 */
void buy_stock(int connfd, int id, int num, node_pointer root); /* 주식 구매 */
void sell_stock(int connfd, int id, int num, node_pointer root); /* 주식 판매 */
void free_tree(node_pointer root); /* 트리에 할당된 메모리 해제 */


/****************함수 정의****************/

node_pointer create_node(item stock)
{
    node_pointer new_node = (node_pointer)malloc(sizeof(node));
    new_node->stock = stock;
    new_node->left_child = new_node->right_child = NULL;
    return new_node;
}

node_pointer insert_node(node_pointer root, item stock)
{
    if (root == NULL)
        return create_node(stock);

    if (stock.ID < root->stock.ID)
        root->left_child = insert_node(root->left_child, stock);

    else if (stock.ID > root->stock.ID)
        root->right_child = insert_node(root->right_child, stock);

    return root;
}

node_pointer read_stock()
{
    node_pointer root = NULL;
    FILE* fp = fopen("stock.txt", "r");
    if (fp == NULL)
    {
        fprintf(stderr, "fopen error\n");
        exit(1);
    }

    char line[100];
    while (fgets(line, sizeof(line), fp))
    {
        item stock;
        sscanf(line, "%d %d %d", &stock.ID, &stock.left_stock, &stock.price);
        root = insert_node(root, stock);
    }

    fclose(fp);
    return root;
}

void write_inorder(node_pointer root, FILE* fp)
{
    if (root == NULL)
        return;

    write_inorder(root->left_child, fp);
    fprintf(fp, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    write_inorder(root->right_child, fp);
}

void write_stock(node_pointer root)
{
    FILE* fp = fopen("stock.txt", "w");
    if (fp == NULL)
    {
        fprintf(stderr, "fopen error\n");
        exit(1);
    }

    write_inorder(root, fp);

    fclose(fp);
}

void init_pool(int listenfd, pool* p) 
{
    //Initially, no connected descripotrs 
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++) {
        p->clientfd[i] = -1;
    }

    // Initially, listenfd is only member of select read set
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool* p) 
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)
    {
        if (p->clientfd[i] < 0) //비어있는 슬롯을 찾으면
        {
            p->clientfd[i] = connfd; //connfd를 pool에 추가한다
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set); //connfd를 descriptor set에 추가한다

            // update max descriptor and pool high water mark 
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
       
    if (i == FD_SETSIZE) //비어있는 슬롯이 없으면
        app_error("add_client error: Too many clients");
}

void show_stock(int connfd, node_pointer root) 
{
    if (root == NULL)
        return;

    char tmpbuf[MAXLINE];
   
    show_stock(connfd, root->left_child);
    sprintf(tmpbuf, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    strcat(printbuf, tmpbuf);
    show_stock(connfd, root->right_child);
}

struct node* tree_search(node_pointer root, int id) {
    if (root == NULL || root->stock.ID == id)
        return root;

    if (id < root->stock.ID)
        return tree_search(root->left_child, id);

    else
        return tree_search(root->right_child, id);
}

void buy_stock(int connfd, int id, int num, node_pointer root) 
{
    if (root == NULL)
        return;

    node_pointer target = tree_search(root, id);
   
    if (target->stock.left_stock < num) 
    {
        sprintf(printbuf, "Not enough left stock\n");
    }
    else 
    {
        target->stock.left_stock -= num;
        sprintf(printbuf, "[buy] success\n");
    }
}

void sell_stock(int connfd, int id, int num, node_pointer root) 
{
    if (root == NULL)
        return;

    node_pointer target = tree_search(root, id);
    target->stock.left_stock += num;
    sprintf(printbuf, "[sell] success\n");
}
 
void check_clients(pool* p) 
{
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) 
    {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) //if connfd is ready
        { 
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) //line 읽음
            {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n",
                    n, byte_cnt, connfd);
                
                char command[5]; command[0] = 0;
                int id = 0;
                int num = 0;
                sscanf(buf, "%s %d %d", command, &id, &num);
                //printf("com:%s id:%d num:%d\n", command, id, num);
 
                /* 각 명령어에 따라 printbuf를 준비한다 */
                if (strcmp(command, "show") == 0) 
                {
                    show_stock(connfd, root);
                    //printf("printbuf:: %s\n", printbuf); /*임시*/
                }

                else if (strcmp(command, "buy") == 0) 
                {
                    buy_stock(connfd, id, num, root);
                    //printf("printbuf:: %s\n", printbuf); /*임시*/
                }

                else if (strcmp(command, "sell") == 0) 
                {
                    sell_stock(connfd, id, num, root);
                    //printf("printbuf:: %s\n", printbuf); /*임시*/
                }

                else if (strcmp(command, "exit") == 0)
                {
                    sprintf(printbuf, "exit the stock server\n");
                   
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                    write_stock(root);
                }

                /* 준비한 printbuf를 connfd에 보낸다 */
                Rio_writen(connfd, printbuf, sizeof(printbuf));
                memset(printbuf, 0, sizeof(printbuf));
            }

            else //EOF 읽음
            {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                write_stock(root);
            }
        }
    }
}

/* 임시 */
void printStockTree(struct node* root) {
    if (root == NULL) {
        return;
    }
    printStockTree(root->left_child);
    printf("ID: %d, Left Stock: %d, Price: %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
    printStockTree(root->right_child);
}


/**************** main ****************/
int main(int argc, char ** argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    static pool pool;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    root = read_stock();
    //printStockTree(root); /* 임시 */


    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        //listenfd 혹은 connfd가 준비되기를 기다림
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

        // If listenfd is ready, add new client to pool
        if (FD_ISSET(listenfd, &pool.ready_set)) 
        {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);

            Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, 
                client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);

            add_client(connfd, &pool);
        }

        // 각 ready connfd로부터 텍스트라인을 읽어 처리한다
        check_clients(&pool);
    }
    exit(0);
}
/* $end echoserverimain */
