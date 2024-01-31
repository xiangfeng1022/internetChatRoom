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
#include <sqlite3.h>
#include <string.h>

#define SERVER_PORT     8080
#define LISTEN_MAX      128

#define MIN_THREADS     10
#define MAX_THREADS     20
#define MAX_QUEUE_CAPACITY  10

#define DEFAULT_LOGIN_NAME  20
#define DEFAULT_LOGIN_PAWD  16
#define BUFFER_SQL          100

/* 创建数据库句柄*/
sqlite3 * chatRoomDB = NULL;

enum CLIENT_CHOICE
{
    LOG_IN = 1,
    REGISTER,
    PRIVATE_CHAT,
};

typedef struct chatRoom
{
    BalanceBinarySearchTree * online;
    int communicateFd;
    pthread_mutex_t mutex;
    sqlite3 * ppDb;
} chatRoom;

#if 0
typedef struct clientNode
{
    char loginName[DEFAULT_LOGIN_NAME];
    char loginPawd[DEFAULT_LOGIN_PAWD];
} clientNode;
#endif

/* 锁 */
pthread_mutex_t g_mutex;

/* AVL比较器：以登录名做比较 */
int compareFunc(void * val1, void * val2)
{

}

/* AVL打印器 */
int printfFunc(void * val)
{

}

void * chatHander(void * arg)
{
    /* 接收传递过来的结构体 */
    chatRoom * chat = (chatRoom *)arg;

    /* 通信句柄 */
    int acceptfd = chat->communicateFd;
    /* 在线列表 */
    BalanceBinarySearchTree * onlineList = chat->online;
    /* 数据库句柄 */
    sqlite3 * charRoomDB = chat->ppDb;

    int choice = 0;

    /* 新建用户结点 */
    char loginName[DEFAULT_LOGIN_NAME];
    bzero(loginName, sizeof(loginName));
    char loginPawd[DEFAULT_LOGIN_PAWD];
    bzero(loginPawd, sizeof(loginPawd));

    /* 程序运行 */
    while (1)
    {
        /* 循环接收选择的功能 */
        ssize_t readChoice = read(acceptfd, &choice, sizeof(choice));
        if (readChoice < 0)
        {
            perror("read error");
            close(acceptfd);
            pthread_exit(NULL);
        }
        switch (choice)
        {
        /* 注册功能 */
        case REGISTER:
            /* code */

            break;
        
        /* 登录功能 */
        case LOG_IN:
            /* 接收客户端发送的用户名和密码 */
            ssize_t readName = read(acceptfd, loginName, sizeof(loginName));
            ssize_t readPawd = read(acceptfd, loginPawd, sizeof(loginPawd));
            if (readName < 0 || readPawd < 0)
            {
                perror("read error");
                close(acceptfd);
                pthread_exit(NULL);
            }

            /* 打开数据库连接 */
            sqlite3 * chatRoomDB;
            int ret = sqlite3_open("chatRoom.db", &chatRoomDB);
            if (ret != SQLITE_OK)
            {
                perror("sqlite open error");
                close(acceptfd);
                pthread_exit(NULL);
            }
    
            /* 查询数据库，验证用户名和密码 */
            char * ermsg = NULL;
            char sql[BUFFER_SQL];
            memset(sql, 0, sizeof(sql));
            sprintf(sql, "select * from user WHERE id='%s' and password='%s'", loginName, loginPawd);
            // ret = sqlite3_exec(chatRoomDB, sql, NULL ,NULL, &ermsg);
            // if (ret != SQLITE_OK)
            // {
            //     printf("Login failed: %s\n", ermsg);
            //     close(acceptfd);
            //     pthread_exit(NULL);
            // }

            /* 查询数据库并存储查询结果到result中 */
            char ** result = NULL;
            int row = 0;
            int column = 0;
            ret = sqlite3_get_table(chatRoomDB, sql, &result, &row, &column, &ermsg);
            if (ret != SQLITE_OK)
            {
                printf("sqlite3_get_table error:%s\n", ermsg);
                pthread_exit(NULL);
            }

            char sql1[BUFFER_SQL];
            memset(sql1, 0, sizeof(sql1));
            int row1 = 0;
            int column1 = 0;
            sprintf(sql1, "select * from user WHERE id='%s'", loginName);
            ret = sqlite3_get_table(chatRoomDB, sql1, &result, &row1, &column1, &ermsg);

            if (row == 1)
            {
                /* 登录成功，将用户添加到在线列表中 */
                pthread_mutex_lock(&g_mutex);  // 加锁，保证在线列表的互斥访问
                balanceBinarySearchTreeInsert(onlineList, loginName);  // 将用户名插入在线列表
                //ret = balanceBinarySearchTreeIsContainAppointVal(onlineList, loginName);// 在线列表中查询账号
                pthread_mutex_unlock(&g_mutex);  // 解锁

                /* 发送登录 成功 消息给客户端 */
                char loginSuccess[BUFFER_SQL] = "登陆成功";
                ssize_t writeSuccess = write(acceptfd, loginSuccess, strlen(loginSuccess) + 1);
                if (writeSuccess < 0)
                {
                    perror("write error");
                    close(acceptfd);
                    pthread_exit(NULL);
                }
            }
            else 
            {
                /* 判断账号是否存在 */
                if (row1 == 0) 
                {
                    /* 发送登录失败消息给客户端，并提醒注册账号 */
                    char loginFailure[BUFFER_SQL] = "没有账号,请注册";
                    ssize_t writeFailure = write(acceptfd, loginFailure, strlen(loginFailure) + 1);
                    if (writeFailure < 0)
                    {
                        perror("write error");
                        close(acceptfd);
                        pthread_exit(NULL);
                    }
                }
                else 
                {
                    /* 发送登录失败消息给客户端 */
                    char loginFailure[BUFFER_SQL] = "密码或账号错误,登录失败";
                    ssize_t writeFailure = write(acceptfd, loginFailure, strlen(loginFailure) + 1);
                    if (writeFailure < 0)
                    {
                        perror("write error");
                        close(acceptfd);
                        sqlite3_free_table(result);
                        pthread_exit(NULL);
                    }
                }
            }
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

    /* 创建数据库和表 */
    sqlite3 * chatRoomDB = NULL;
    /* 打开数据库 */
    int ret = sqlite3_open("chatRoom.db", &chatRoomDB);
    if (ret != SQLITE_OK)
    {
        perror("sqlite open error");
        exit(-1);
    }

    /* 创建储存所有用户的表 */
    char * ermsg = NULL;
    const char * sql = "create table if not exists user (id text primary key not null, password text not null)";
    ret = sqlite3_exec(chatRoomDB, sql, NULL, NULL, &ermsg);
    if (ret != SQLITE_OK)
    {
        printf("create table error:%s\n", ermsg);
        sqlite3_close(chatRoomDB);
        pthread_mutex_destroy(&g_mutex);
        threadPoolDestroy(&pool);
        exit(-1);
    }

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1)
    {
        perror("socket error");
        sqlite3_close(chatRoomDB);
        pthread_mutex_destroy(&g_mutex);
        threadPoolDestroy(&pool);
        exit(-1);
    }

    /* todo */
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
    ret = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (void *) &enableOpt, sizeof(enableOpt));
    if (ret == -1)
    {
        perror("setsockopt error");
        close(socketfd);
        sqlite3_close(chatRoomDB);
        pthread_mutex_destroy(&g_mutex);
        threadPoolDestroy(&pool);
        exit(-1);
    }

    /* 绑定服务器端口信息 */
    ret = bind(socketfd, (struct sockaddr *)&serverAddress, serverAddressLen);
    if (ret == -1)
    {
        perror("bind error");
        close(socketfd);
        sqlite3_close(chatRoomDB);
        pthread_mutex_destroy(&g_mutex);
        threadPoolDestroy(&pool);
        exit(-1);
    }

    ret = listen(socketfd, LISTEN_MAX);
    if (ret == -1)
    {
        perror("listen error");
        close(socketfd);
        sqlite3_close(chatRoomDB);
        pthread_mutex_destroy(&g_mutex);
        threadPoolDestroy(&pool);
        exit(-1);
    }

    /* 建立在线链表 */
    BalanceBinarySearchTree * onlineList;

    ret = balanceBinarySearchTreeInit(&onlineList, compareFunc, printfFunc);
    if (ret != 0)
    {
        perror("create online list error");
        close(socketfd);
        sqlite3_close(chatRoomDB);
        pthread_mutex_destroy(&g_mutex);
        threadPoolDestroy(&pool);
        exit(-1);
    }

    chatRoom chat;
    bzero(&chat, sizeof(chat));

    chat.online = onlineList;
    
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
        pthread_t tid;
        pthread_create(&tid, NULL, chatHander, (void *)&chat);
        //taskQueueInsert(&pool, chatHander, (void *)&chat);
    }

    /* 销毁资源 */
    close(acceptfd);
    close(socketfd);
    
    pthread_mutex_destroy(&g_mutex);
    balanceBinarySearchTreeDestroy(onlineList);
    sqlite3_close(chatRoomDB);
    threadPoolDestroy(&pool);

    return 0;
}