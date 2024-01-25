#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadPool.h"
#include "balanceBinarySearchTree.h"
#include <strings.h>

#define SERVER_PORT     8080
#define LISTEN_MAX      128

#define MIN_THREADS     10
#define MAX_THREADS     20
#define MAX_QUEUE_CAPACITY  10

#define DEFAULT_LOGIN_NAME  20
#define DEFAULT_LOGIN_PAWD  16

enum CLIENT_CHOICE
{
    REGISTER = 1,
    LOG_IN,
    PRIVATE_CHAT,
};

typedef struct chatRoom
{
    BalanceBinarySearchTree * online;
    int communicateFd;
    pthread_mutex_t mutex;
} chatRoom;

typedef struct clientNode
{
    char loginName[DEFAULT_LOGIN_NAME];
    char loginPawd[DEFAULT_LOGIN_PAWD];
} clientNode;

/* 锁 */
pthread_mutex_t g_mutex;

/* AVL比较器：以登录名做比较 */
void * compareFunc(void * val1, void * val2)
{

}

/* AVL打印器 */
void * printfFunc(void * val)
{

}

void * chatHander(void * arg)
{
    chatRoom * chat = (chatRoom *)arg;

    int acceptfd = chat->communicateFd;
    BalanceBinarySearchTree * onlineList = chat->online;

    int choice = 0;
    ssize_t readBytes = read(acceptfd, &choice, sizeof(choice));
    if (readBytes < 0)
    {
        perror("read error");
        close(acceptfd);
        pthread_exit(-1);
    }

    /* 新建用户结点 */
    clientNode client;
    bzero(&client, sizeof(client));

    /* 程序运行 */
    while (1)
    {
        switch (choice)
        {
        /* 注册功能 */
        case REGISTER:
            /* code */
            break;
        
        /* 登录功能 */
        case LOG_IN:
            /* code */
            break;
        
        /* 私聊功能 */
        case PRIVATE_CHAT:
            /* code */
            break;
        

        default:
            break;
        }

    }

    pthread_exit(NULL);
}

int main()
{
    /* 初始化线程池 */
    threadPool pool;

    threadPoolInit(&pool, MIN_THREADS, MAX_THREADS, MAX_QUEUE_CAPACITY);

    /* 初始化锁：用来实现对在线列表的互斥访问 */
    pthread_mutex_init(&g_mutex, NULL);

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1)
    {
        perror("socket error");
        exit(-1);
    }

    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;

    /* 存储服务器信息 */
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    socklen_t serverAddressLen = sizeof(serverAddress);
    socklen_t clientAddressLen = sizeof(clientAddress);

    /* 设置端口复用 */
    int enableOpt = 1;
    int ret = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (void *) &enableOpt, sizeof(enableOpt));
    if (ret == -1)
    {
        perror("setsockopt error");
        close(socketfd);
        exit(-1);
    }

    /* 绑定服务器端口信息 */
    int ret = bind(socketfd, (struct sockaddr *)&serverAddress, serverAddressLen);
    if (ret == -1)
    {
        perror("bind error");
        close(socketfd);
        exit(-1);
    }

    ret = listen(socketfd, LISTEN_MAX);
    if (listen == -1)
    {
        perror("listen error");
        close(socketfd);
        exit(-1);
    }

    /* 建立在线链表 */
    BalanceBinarySearchTree * onlineList;

    ret = balanceBinarySearchTreeInit(&onlineList, compareFunc, printfFunc);
    if (ret != 0)
    {
        perror("create online list error");
        close(socketfd);
        exit(-1);
    }

    chatRoom chat;
    bzero(&chat, sizeof(chat));

    chat.communicateFd = onlineList;
    
    int acceptfd = 0;
    /* 建立连接 */
    while (1)
    {
        acceptfd = accept(socketfd, (struct sockaddr *)&clientAddress, &clientAddressLen);
        if (acceptfd == -1)
        {
            perror("accept error");
            break;
        }

        chat.communicateFd = acceptfd;

        /* 向线程池中插入线程执行函数 */
        taskQueueInsert(&pool, chatHander, (void *)&chat);
    }

    /* 销毁资源 */
    close(acceptfd);
    close(socketfd);
    
    pthread_mutex_destroy(&g_mutex);
    balanceBinarySearchTreeDestroy(&onlineList);
    threadPoolDestroy(&pool);

    return 0;
}