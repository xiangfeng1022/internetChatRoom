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
#define BUFFER_SIZE 300

#define MIN_THREADS     10
#define MAX_THREADS     20
#define MAX_QUEUE_CAPACITY  10

#define DEFAULT_LOGIN_NAME  20
#define DEFAULT_LOGIN_PAWD  16
#define BUFFER_SQL          100

/* 创建数据库句柄 */
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
    char *buf1 = (char *)val1;
    char *buf2 = (char *)val2;
    if(strlen(buf1) != strlen(buf2))
    {
        return -1;
    }
    else
    {
        return strncmp(buf1, buf2, strlen(buf1));
    }
}

/* AVL打印器 */
int printfFunc(void * val)
{

}

/* 聊天内容插入数据库函数 */
static void databaseChatContentInsert(sqlite3 *db, char *filename, char *username, char *word);

/* 注册信息插入数据库函数 */
static void databaseRegesiterContentInsert(sqlite3 *db, char *id, char *password);

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



    int flag = 0;
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
        /* 当前客户端聊天文件名 */
        char clientHostBuffer[BUFFER_SIZE];
        bzero(clientHostBuffer, sizeof(clientHostBuffer));
        /* 对方客户端聊天文件名 */
        char clientGuestBuffer[BUFFER_SIZE];
        bzero(clientGuestBuffer, sizeof(clientGuestBuffer));
        /* 客户端用户名 */
        char clientUsernameBuffer[BUFFER_SIZE];
        bzero(clientUsernameBuffer, sizeof(clientUsernameBuffer));

        switch (choice)
        {
        /* 登录功能 */
        case LOG_IN:
            /* code */

            /* 测试功能用例 */
            #if 1
            int choose = 0;
            printf("选择账户：\n");
            scanf("%d", &choose);
            if(choose == 1)
            {
                balanceBinarySearchTreeInsert(onlineList, "aa");
                strncat(clientHostBuffer, "aa", strlen("aa") + 1);
                strncpy(clientUsernameBuffer, "aa", strlen("aa") + 1);
            }
            else if(choose == 2)
            {
                balanceBinarySearchTreeInsert(onlineList, "bb");
                strncat(clientHostBuffer, "bb", strlen("bb") + 1);
                strncpy(clientUsernameBuffer, "bb", strlen("bb") + 1);
            }
            #endif
            
            flag = 1;
            break;
        
        /* 注册功能 */
        case REGISTER:
            // 读取用户输入的登录名和密码
            ssize_t readBytes = read(acceptfd, loginName, sizeof(loginName));
            if (readBytes < 0)
            {
                perror("read error");
                close(acceptfd);
                exit(-1);
            }
            readBytes = read(acceptfd, loginPawd, sizeof(loginPawd));
            if (readBytes < 0)
            {
                perror("read error");
                close(acceptfd);
                exit(-1);
            }

            // 加锁以保证在线列表的互斥访问
            pthread_mutex_lock(&g_mutex);
            
            /* 打开数据库 */
            sqlite3 * chatRoomDB;
            int ret = sqlite3_open("chatRoom.db", &chatRoomDB);
            if(ret != SQLITE_OK)
            {
                perror("sqlite open error");
                exit(-1);
            }

            // 检查用户是否已经存在
            char * ermsg = NULL;
            char sql[BUFFER_SQL];
            memset(sql, 0, sizeof(sql));
            sprintf(sql, "select * from user WHERE id='%s' and password='%s'", loginName, loginPawd);
            ret = sqlite3_exec(chatRoomDB, sql, NULL ,NULL, &ermsg);
            if(ret == SQLITE_OK)
            {
                // 用户已存在
                write(acceptfd, "REGISTER_FAIL", sizeof("REGISTER_FAIL"));
            }
            else
            {
                // 将新用户添加到数据库
                databaseRegesiterContentInsert(chatRoomDB, loginName, loginPawd);

                // 注册成功
                write(acceptfd, "REGISTER_SUCCESS", sizeof("REGISTER_SUCCESS"));
            }

            // 解锁
            pthread_mutex_unlock(&g_mutex);
            break;

        default:
            break;
        }

        /* 接收缓冲区 */
        char recvBuffer[BUFFER_SIZE];
        bzero(recvBuffer, sizeof(recvBuffer));
        
        while(flag)
        {
            sqlite3 *chat1db = NULL;
            sqlite3 *chat2db = NULL;
            int readBytes = read(acceptfd, recvBuffer, sizeof(recvBuffer) - 1);
            int ret = 0;
            if(balanceBinarySearchTreeIsContainAppointVal(onlineList, &recvBuffer))
            {
                write(acceptfd, "对方在线", strlen("对方在线"));
                /* 存储当前客户端聊天记录文件名 */
                strncat(clientHostBuffer, "->", strlen("->") + 1);
                strncat(clientHostBuffer, recvBuffer, sizeof(recvBuffer));
                strncat(clientHostBuffer, ".db", strlen(".db") + 1);
                /* 存储对方客户端聊天记录文件名 */
                strncat(clientGuestBuffer, recvBuffer, sizeof(recvBuffer));
                strncat(clientGuestBuffer, "->", strlen("->") + 1);
                strncat(clientGuestBuffer, clientUsernameBuffer, sizeof(clientUsernameBuffer));
                strncat(clientGuestBuffer, ".db", strlen(".db") + 1);
            }
            else if(!balanceBinarySearchTreeIsContainAppointVal(onlineList, &recvBuffer))
            {
                write(acceptfd, "对方不在线", strlen("对方不在线"));
                break;
            }
            while(1)
            {
                /* 从客户端读取输入的文字 */
                readBytes = read(acceptfd, recvBuffer, sizeof(recvBuffer) - 1);
                if(readBytes < 0)
                {
                    perror("read error");
                    break;
                }
                else if(readBytes == 0)
                {
                    printf("客户端下线\n");
                    break;
                }
                else if(readBytes > 1)
                {
                    databaseChatContentInsert(chat1db, clientHostBuffer, clientUsernameBuffer, recvBuffer);
                    databaseChatContentInsert(chat2db, clientGuestBuffer, clientUsernameBuffer, recvBuffer);
                }
            }
            /* 关闭数据库链接 */
            sqlite3_close(chat1db);
            sqlite3_close(chat2db);
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
        // taskQueueInsert(&pool, chatHander, (void *)&chat);
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

/* 聊天内容插入数据库函数 */
static void databaseChatContentInsert(sqlite3 *db, char *filename, char *username, char *word)
{
    const char *sql = NULL;
    
    /* 打开数据库：如果数据库不存在，那么就创建 */
    int ret = sqlite3_open(filename, &db);
    if(ret != SQLITE_OK)
    {
        perror("sqlite open error");
        exit(-1);
    }
    /* 预编译的SQL语句对象 */
    sqlite3_stmt *stmt;
    sql = "insert into chat values (?, ?)";
    
    /* 准备SQL语句 */
    ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) 
    {
        printf("sqlite3_prepare_v2 error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    /* 绑定文本参数 */
    /* 用户名 */
    ret = sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (ret != SQLITE_OK) 
    {
        printf("sqlite3_bind_text error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }
    /* 输入内容 */
    ret = sqlite3_bind_text(stmt, 2, word, -1, SQLITE_STATIC);
    if (ret != SQLITE_OK) 
    {
        printf("sqlite3_bind_text error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    /* 执行SQL语句 */
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) 
    {
        printf("sqlite3_step error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }
    else 
    {
        printf("success\n");
    }

    /* 清理资源 */
    sqlite3_finalize(stmt);
}

/* 注册信息插入数据库函数 */
static void databaseRegesiterContentInsert(sqlite3 *db, char *id, char *password)
{
    const char *sql = NULL;
    
    /* 预编译的SQL语句对象 */
    sqlite3_stmt *stmt;
    sql = "insert into user values (?, ?)";
    
    /* 准备SQL语句 */
    int ret = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (ret != SQLITE_OK) 
    {
        printf("sqlite3_prepare_v2 error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    /* 绑定文本参数 */
    /* 用户名 */
    ret = sqlite3_bind_text(stmt, 1, id, -1, SQLITE_STATIC);
    if (ret != SQLITE_OK) 
    {
        printf("sqlite3_bind_text error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }
    /* 输入内容 */
    ret = sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
    if (ret != SQLITE_OK) 
    {
        printf("sqlite3_bind_text error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }

    /* 执行SQL语句 */
    ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE) 
    {
        printf("sqlite3_step error\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(-1);
    }
    else 
    {
        printf("success\n");
    }

    /* 清理资源 */
    sqlite3_finalize(stmt);
}