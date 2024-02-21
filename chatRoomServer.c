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

/* 主界面选择 */
enum CLIENT_CHOICE
{
    REGISTER = 1,
    LOG_IN,
    EXIT,
};

/* 功能界面选择 */
enum FUNC_CHOICE
{
    PRIVATE_CHAT = 1,
    FRIEND_ADD,
    INTERNAL_EXIT,
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
    int communicateFd;
} clientNode;


/* 锁 */
pthread_mutex_t g_mutex;

/* AVL比较器：以登录名做比较 */
int compareFunc(void * val1, void * val2)
{
    clientNode *node1 = (clientNode *)val1;
    clientNode *node2 = (clientNode *)val2;

    int ret = strncmp(node1->loginName, node2->loginName, sizeof(node1->loginName));
    if(ret > 0)
    {
        return 1;
    }
    else if(ret < 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

/* AVL打印器 */
int printfFunc(void * val)
{
    clientNode * client = (clientNode *)val;
    printf("%s\n", client->loginName);
}


/* 信息写入判断函数 */
static void writeMessage(int fd, char *message, int messageSize);
/* 信息读取判断函数 */
static void readMessage(int fd, char *message, int messageSize);
/* 数据读取判断函数 */
static void readData(int fd, int *data, int dataSize);
/* 聊天内容插入数据库函数 */
static void ChatContentInsert(sqlite3 *db, char *filename, char *username, char *word);
/* 注册信息插入数据库函数 */
static void RegesiterContentInsert(sqlite3 *db, char *id, char *password);

/* 回调函数，用于处理查询结果 */
int callback(void* data, int argc, char** argv, char** azColName) 
{
    if (argc == 0) 
    {
        *(int *)data = 0;
    } 
    else 
    {
        *(int *)data = 1;
    }
    return 0;
}

void * chatHander(void * arg)
{
    /* 接收传递过来的结构体 */
    chatRoom * chat = (chatRoom *)arg;
    /* 通信句柄 */
    int acceptfd = chat->communicateFd;
    /* 在线列表 */
    BalanceBinarySearchTree * onlineList = chat->online;

    /* 新建用户结点 */
    char loginName[DEFAULT_LOGIN_NAME];
    bzero(loginName, sizeof(loginName));
    char loginPawd[DEFAULT_LOGIN_PAWD];
    bzero(loginPawd, sizeof(loginPawd));
    /* 创建客户端结点 */
    clientNode client;
    bzero(&client, sizeof(client));
    bzero(client.loginName, sizeof(client.loginName));

    int flag = 0;
    int choice = 0;
    /* 程序运行 */
    while (1)
    {
        /* 定义指向数据库的指针 */
        sqlite3 * chatRoomDB;
        char * ermsg = NULL;
        char sql[BUFFER_SQL];
        bzero(sql, sizeof(sql));
        int found= 0;
        int ret = 0;
        /* 循环接收选择的功能 */
        int readBytes = read(acceptfd, &choice, sizeof(choice));
        if(readBytes < 0)
        {
            printf("read error\n");
            close(acceptfd);
            break;
        }
        else if(readBytes == 0)
        {
            printf("客户端下线\n");
            close(acceptfd);
            break;
        }
        /* 当前客户端聊天文件名 */
        char clientHostBuffer[BUFFER_SIZE];
        bzero(clientHostBuffer, sizeof(clientHostBuffer));
        /* 对方客户端聊天文件名 */
        char clientGuestBuffer[BUFFER_SIZE];
        bzero(clientGuestBuffer, sizeof(clientGuestBuffer));

        switch (choice)
        {
        /* 注册功能 */
        case REGISTER:
            /* 读取用户输入的登录名和密码 */
            readMessage(acceptfd, loginName, sizeof(loginName) - 1);
            readMessage(acceptfd, loginPawd, sizeof(loginPawd) - 1);

            /* 加锁以保证在线列表的互斥访问 */
            pthread_mutex_lock(&g_mutex);
            
            /* 打开数据库 */
            ret = sqlite3_open("chatRoom.db", &chatRoomDB);
            if(ret != SQLITE_OK)
            {
                perror("sqlite open error");
                pthread_exit(NULL);
            }

            /* 检查用户是否已经存在 */
            sprintf(sql, "select * from user where id = '%s'", loginName);
            ret = sqlite3_exec(chatRoomDB, sql, callback, &found, &ermsg);
            if(ret != SQLITE_OK)
            {
                perror("sqlite open error");
                pthread_exit(NULL);
            }
            if(found == 1)
            {
                /* 用户已存在 */
                writeMessage(acceptfd, "注册失败，用户已存在", sizeof("注册失败，用户已存在"));
            }
            else
            {
                /* 将新用户添加到数据库 */
                RegesiterContentInsert(chatRoomDB, loginName, loginPawd);
                /* 注册成功 */
                writeMessage(acceptfd, "注册成功", sizeof("注册成功"));
            }

            /* 关闭数据库 */
            sqlite3_close(chatRoomDB);

            /* 解锁 */
            pthread_mutex_unlock(&g_mutex);
            break;

        /* 登录功能 */
        case LOG_IN:
            /* 读取用户输入的登录名和密码 */
            readMessage(acceptfd, loginName, sizeof(loginName) - 1);
            readMessage(acceptfd, loginPawd, sizeof(loginPawd) - 1);

            /* 写入客户端结点 */
            strncpy(client.loginName, loginName, sizeof(loginName));
            client.communicateFd = acceptfd;
            
            /* 打开数据库连接 */
            ret = sqlite3_open("chatRoom.db", &chatRoomDB);
            if (ret != SQLITE_OK)
            {
                perror("sqlite open error");
                close(acceptfd);
                pthread_exit(NULL);
            }
    
            /* 查询数据库，验证用户名和密码 */
            sprintf(sql, "select * from user where id ='%s'", loginName);
            ret = sqlite3_exec(chatRoomDB, sql, callback, &found, &ermsg);
            if (ret != SQLITE_OK)
            {
                printf("Login failed: %s\n", ermsg);
                close(acceptfd);
                pthread_exit(NULL);
            }
            /* 用户存在 */
            if(found == 1)
            {
                found = 0;
                sprintf(sql, "select * from user where id ='%s' and password='%s'", loginName, loginPawd);
                ret = sqlite3_exec(chatRoomDB, sql, callback, &found, &ermsg);
                if (ret != SQLITE_OK)
                {
                    printf("Login failed: %s\n", ermsg);
                    close(acceptfd);
                    pthread_exit(NULL);
                }
                /* 密码正确 */
                if(found == 1)
                {
                    /* 加锁，保证在线列表的互斥访问 */
                    pthread_mutex_lock(&g_mutex);
                    /* 在线列表中查询账号 */
                    int existance = balanceBinarySearchTreeIsContainAppointVal(onlineList, (void *)&client);
                    if(existance == 0)
                    {
                        /* 将用户添加到在线列表中 */
                        balanceBinarySearchTreeInsert(onlineList, (void *)&client);
                        balanceBinarySearchTreeLevelOrderTravel(onlineList);
                        /* 登录成功 */
                        writeMessage(acceptfd, "登录成功", sizeof("登录成功"));
                        /* 登录标志位置1 */
                        flag = 1;
                    }
                    else if(existance == 1)
                    {
                        /* 用户已在别处登录 */
                        writeMessage(acceptfd, "用户已在别处登录", sizeof("用户已在别处登录"));
                    }
                    /* 解锁 */
                    pthread_mutex_unlock(&g_mutex);
                }
                /* 密码错误 */
                else
                {
                    writeMessage(acceptfd, "密码错误", sizeof("密码错误"));
                }
            }
            /* 用户不存在 */
            else
            {
                writeMessage(acceptfd, "用户不存在", sizeof("用户不存在"));
            }

            /* 关闭数据库 */
            sqlite3_close(chatRoomDB);
            break;
        
        /* 退出功能 */
        case EXIT:
                writeMessage(acceptfd, "客户端退出", sizeof("客户端退出"));
            break;

        default:
            break;
        }

        /* 接收缓冲区 */
        char recvBuffer[BUFFER_SIZE];
        bzero(recvBuffer, sizeof(recvBuffer));
        
        int mode = 0;
        while(flag)
        {
            /* 循环接收选择的功能 */
            int readBytes = read(acceptfd, &mode, sizeof(mode));
            if(readBytes < 0)
            {
                printf("read error\n");
                close(acceptfd);
                break;
            }
            else if(readBytes == 0)
            {
                printf("客户端下线\n");
                close(acceptfd);
                break;
            }

            switch(mode)
            {
            /* 私聊功能 */
            case PRIVATE_CHAT:
                
                break;

            /* 加好友功能 */
            case FRIEND_ADD:
                
                break;
            
            /* 退出功能 */
            case INTERNAL_EXIT:
                /* 加锁，保证在线列表的互斥访问 */
                pthread_mutex_lock(&g_mutex);
                /* 从在线列表中删除用户结点 */
                balanceBinarySearchTreeDelete(onlineList, (void *)&client);
                if(balanceBinarySearchTreeIsContainAppointVal(onlineList, (void *)&client))
                {
                    /* 未删除结点 */
                    writeMessage(acceptfd, "账户退出失败", sizeof("账户退出失败"));
                    printf("账户退出失败");
                }
                else
                {
                    /* 已删除结点 */
                    writeMessage(acceptfd, "账户退出", sizeof("账户退出"));
                    printf("账户退出");
                    flag = 0;
                }
                /* 解锁 */
                pthread_mutex_unlock(&g_mutex);
                flag = 0;
                break;

            default:
                break;
            }
        }
#if 0
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
                // strncat(clientGuestBuffer, clientUsernameBuffer, sizeof(clientUsernameBuffer));
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
                    // databaseChatContentInsert(chat1db, clientHostBuffer, clientUsernameBuffer, recvBuffer);
                    // databaseChatContentInsert(chat2db, clientGuestBuffer, clientUsernameBuffer, recvBuffer);
                }
            }
            /* 关闭数据库链接 */
            sqlite3_close(chat1db);
            sqlite3_close(chat2db);
#endif
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

/* 信息写入判断函数 */
static void writeMessage(int fd, char *message, int messageSize)
{
    int ret = write(fd, message, messageSize);
    if (ret < 0)
    {
        perror("write error");
        close(fd);
        exit(-1);
    }
}

/* 信息读取判断函数 */
static void readMessage(int fd, char *message, int messageSize)
{
    int ret = read(fd, message, messageSize);
    if (ret < 0)
    {
        perror("read error");
        close(fd);
        exit(-1);
    }
}

/* 数据写入判断函数 */
static void readData(int fd, int *data, int dataSize)
{
    int ret = read(fd, data, dataSize);
    if (ret < 0)
    {
        perror("read error");
        close(fd);
        exit(-1);
    }
}

/* 聊天内容插入数据库函数 */
static void ChatContentInsert(sqlite3 *db, char *filename, char *username, char *word)
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
static void RegesiterContentInsert(sqlite3 *db, char *id, char *password)
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
    /* 密码 */
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