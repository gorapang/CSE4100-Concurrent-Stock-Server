/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define NTHREADS 100
#define SBUFSIZE 32
/***** Prethreaded server 구현 *****/

typedef struct {
    int* buf; /* 버퍼 배열 */
    int n; /* 슬롯의 최대 개수 */
    int front; /* 첫번째 item = buf[(front+1)%n] */
    int rear; /* 마지막 item = buf[rear%n] */
    sem_t mutex; /* 버퍼에 대한 접근을 보호한다 */
    sem_t slots; /* available slot의 개수 */
    sem_t items; /* available item의 개수 */
} sbuf_t;

sbuf_t sbuf; /* Shared buffer of connected descriptors */

static int byte_cnt; /* counter for total bytes received by all threads */
static sem_t mutex; /* and the mutex that protects it */

void sbuf_init(sbuf_t* sp, int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp, int item);
int sbuf_remove(sbuf_t* sp);

void* thread(void* vargp);
static void init_echo_cnt(void);
void echo_cnt(int connfd);


/***** 주식 기능 구현 *****/

char printbuf[MAXLINE]; /* 명령 수행결과 출력하기 위한 배열 */

typedef struct item { /* 주식 item */
    int ID;
    int left_stock;
    int price;
    int readcnt; /* Initially = 0 */
    sem_t mutex, w; /* Initially = 1 */
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
void sigint_handler(int signo);

/***********************함수 정의***********************/

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t* sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n; /* Buffer holds max of n items */
    sp->front = sp->rear = 0; /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0); /* Initially, buf has 0 items */
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t* sp)
{
    Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t* sp, int item)
{
    P(&sp->slots); /* Wait for available slot */
    P(&sp->mutex); /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->items); /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t* sp)
{
    int item;
    P(&sp->items); /* Wait for available item */
    P(&sp->mutex); /* Lock the buffer */
    item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->slots); /* Announce available slot */
    return item;
}

/* Worker thread routine */
void* thread(void* vargp)
{
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
        echo_cnt(connfd); /* service client */
        Close(connfd);
    }
}

/* echo_cnt initialization routine */
static void init_echo_cnt(void)
{
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

/* Worker thread service routine */
void echo_cnt(int connfd) {
    int n;
    char buf[MAXLINE];
    rio_t rio;

    static pthread_once_t once = PTHREAD_ONCE_INIT;
    Pthread_once(&once, init_echo_cnt);

    Rio_readinitb(&rio, connfd);

    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        char command[5]; command[0] = 0;
        int id = 0; int num = 0;
        sscanf(buf, "%s %d %d", command, &id, &num);

        /* mutex protects byte_cnt */
        P(&mutex);
        byte_cnt += n;
        printf("thread %d received %d (%d total) bytes on fd %d\n",
            (int)pthread_self(), n, byte_cnt, connfd);
        V(&mutex);

        if (strcmp(command, "show") == 0)
        {
            show_stock(connfd, root);
            Rio_writen(connfd, printbuf, MAXLINE);
            memset(printbuf, 0, sizeof(printbuf));
        }
        else if (strcmp(command, "buy") == 0)
        {
            buy_stock(connfd, id, num, root);
            Rio_writen(connfd, printbuf, MAXLINE);
            memset(printbuf, 0, sizeof(printbuf));
        }
        else if (strcmp(command, "sell") == 0)
        {
            sell_stock(connfd, id, num, root);
            Rio_writen(connfd, printbuf, MAXLINE);
            memset(printbuf, 0, sizeof(printbuf));
        }
        else if (strcmp(command, "exit") == 0)
        {
            sprintf(printbuf, "exit the stock server\n");
            //printf("printbuf:: %s\n", printbuf); /*임시*/
            Rio_writen(connfd, printbuf, MAXLINE);
            memset(printbuf, 0, sizeof(printbuf));
            write_stock(root);
            break;
        }
        else
        {
            write_stock(root);
        }
        memset(printbuf, 0, sizeof(printbuf));
        // write_stock(root);
    }
}

void show_stock(int connfd, node_pointer root)
{
    char tmpbuf[MAXLINE];
    if (root != NULL)
    {
        show_stock(connfd, root->left_child);

        P(&root->stock.mutex);
        root->stock.readcnt++;
        if (root->stock.readcnt == 1) //first in reader
            P(&root->stock.w);
        V(&root->stock.mutex);

        // Writing
        sprintf(tmpbuf, "%d %d %d\n", root->stock.ID, root->stock.left_stock, root->stock.price);
        strcat(printbuf, tmpbuf);

        P(&root->stock.mutex);
        root->stock.readcnt--;
        if (root->stock.readcnt == 0) //last out reader
            V(&root->stock.w);
        V(&root->stock.mutex);

        show_stock(connfd, root->right_child);
    }
}

struct node* tree_search(node_pointer root, int id) {
    if (root == NULL || root->stock.ID == id)
        return root;

    if (id < root->stock.ID)
        return tree_search(root->left_child, id);

    else
        return tree_search(root->right_child, id);
}

void buy_stock(int connfd, int id, int n, node_pointer root)
{
    if (root == NULL) return;

    node_pointer target = tree_search(root, id);

    if (target->stock.left_stock < n)
    {
        sprintf(printbuf, "Not enough left stock\n");
    }
    else
    {
        P(&(target->stock.w));
        target->stock.left_stock -= n; //writing
        V(&(target->stock.w));
        sprintf(printbuf, "[buy] success\n");
    }
}

void sell_stock(int connfd, int id, int n, node_pointer root)
{
    if (root == NULL) return;


    node_pointer target = tree_search(root, id);

    P(&(target->stock.w));
    target->stock.left_stock += n; //writing
    V(&(target->stock.w));

    sprintf(printbuf, "[sell] success\n");
}


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
        stock.readcnt = 0; 
        sem_init(&stock.mutex, 0, 1);
        sem_init(&stock.w, 0, 1);
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

void sigint_handler(int signo) 
{ 
    write_stock(root);
    printf("\nSIGINT detected\n");
    exit(1);
}
/**********************************************************/
int main(int argc, char** argv)
{
    Signal(SIGINT, sigint_handler);

    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    root = read_stock();

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);

    for (i = 0; i < NTHREADS; i++) /* Create worker threads */
        Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */

        Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE,
            client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
    }

    write_stock(root);

    exit(0);
}

/////////////////




