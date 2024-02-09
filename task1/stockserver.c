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


/* �ֽ� ��� ���� */
char printbuf[MAXLINE]; /* ��� ������ ����ϱ� ���� �迭 */

typedef struct item { /* �ֽ� item */
    int ID;
    int left_stock;
    int price;
    int readcnt;
}item;

typedef struct node* node_pointer;
typedef struct node { /* ����Ʈ���� ��� */
    item stock;
    node_pointer left_child;
    node_pointer right_child;
}node;

node_pointer root = NULL; /* ����Ʈ�� */

node_pointer create_node(item stock); /* ��� ���� */
node_pointer insert_node(node_pointer root, item stock); /* ����Ʈ���� ��� ���� */
node_pointer read_stock(); /* stock.txt�� �о� binary tree ���·� �����Ѵ� */
void write_inorder(node_pointer root, FILE* fp);
void write_stock(node_pointer root); /* ���� �ֽ� ���¸� stock.txt�� ���� */
void show_stock(int connfd, node_pointer root); /* ���� �ֽ� ���¸� �����ش� */
struct node* tree_search(node_pointer root, int id); /* Ʈ������ id�� ���� ��带 ã�� ��ȯ */
void buy_stock(int connfd, int id, int num, node_pointer root); /* �ֽ� ���� */
void sell_stock(int connfd, int id, int num, node_pointer root); /* �ֽ� �Ǹ� */
void free_tree(node_pointer root); /* Ʈ���� �Ҵ�� �޸� ���� */


/****************�Լ� ����****************/

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
        if (p->clientfd[i] < 0) //����ִ� ������ ã����
        {
            p->clientfd[i] = connfd; //connfd�� pool�� �߰��Ѵ�
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set); //connfd�� descriptor set�� �߰��Ѵ�

            // update max descriptor and pool high water mark 
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
    }
       
    if (i == FD_SETSIZE) //����ִ� ������ ������
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
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) //line ����
            {
                byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n",
                    n, byte_cnt, connfd);
                
                char command[5]; command[0] = 0;
                int id = 0;
                int num = 0;
                sscanf(buf, "%s %d %d", command, &id, &num);
                //printf("com:%s id:%d num:%d\n", command, id, num);
 
                /* �� ��ɾ ���� printbuf�� �غ��Ѵ� */
                if (strcmp(command, "show") == 0) 
                {
                    show_stock(connfd, root);
                    //printf("printbuf:: %s\n", printbuf); /*�ӽ�*/
                }

                else if (strcmp(command, "buy") == 0) 
                {
                    buy_stock(connfd, id, num, root);
                    //printf("printbuf:: %s\n", printbuf); /*�ӽ�*/
                }

                else if (strcmp(command, "sell") == 0) 
                {
                    sell_stock(connfd, id, num, root);
                    //printf("printbuf:: %s\n", printbuf); /*�ӽ�*/
                }

                else if (strcmp(command, "exit") == 0)
                {
                    sprintf(printbuf, "exit the stock server\n");
                   
                    Close(connfd);
                    FD_CLR(connfd, &p->read_set);
                    p->clientfd[i] = -1;
                    write_stock(root);
                }

                /* �غ��� printbuf�� connfd�� ������ */
                Rio_writen(connfd, printbuf, sizeof(printbuf));
                memset(printbuf, 0, sizeof(printbuf));
            }

            else //EOF ����
            {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                write_stock(root);
            }
        }
    }
}

/* �ӽ� */
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
    //printStockTree(root); /* �ӽ� */


    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        //listenfd Ȥ�� connfd�� �غ�Ǳ⸦ ��ٸ�
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

        // �� ready connfd�κ��� �ؽ�Ʈ������ �о� ó���Ѵ�
        check_clients(&pool);
    }
    exit(0);
}
/* $end echoserverimain */
