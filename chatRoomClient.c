#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <string.h>
#include <sqlite3.h>

#define SERVER_PORT 8080
// #define SERVER_ADDR "172.30.149.120"
#define SERVER_ADDR "172.27.105.168"

#define DEFAULT_LOGIN_NAME  20
#define DEFAULT_LOGIN_PAWD  16
#define BUFFER_SIZE 300

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

/* 信息写入判断函数 */
static void writeMessage(int fd, char *message, int messageSize);
/* 信息读取判断函数 */
static void readMessage(int fd, char *message, int messageSize);
/* 数据写入判断函数 */
static void writeData(int fd, int *data, int dataSize);
/* 用户注册函数 */
static void registerUser(int socketfd);
/* 用户登录函数 */
static int loginUser(int socketfd);
/* 聊天记录显示函数 */
static void chatContentDisplay();

int main()
{
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1)
    {
        perror("socket error");
        exit(-1);
    }

    struct sockaddr_in localAddress;
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(SERVER_PORT);

    int ret = inet_pton(AF_INET, SERVER_ADDR, &localAddress.sin_addr.s_addr);
    if (ret == -1)
    {
        perror("inet_pton error");
        close(socketfd);
        exit(-1);
    }

    socklen_t localAddressLen = sizeof(localAddress);

    ret = connect(socketfd, (struct sockaddr *)&localAddress, localAddressLen);
    if (ret == -1)
    {
        perror("connect error");
        close(socketfd);
        exit(-1);
    }

    /* 打开主菜单文件 */
    int mainMenu = open("mainMenu", O_RDONLY);
    if (mainMenu == -1)
    {
        perror("open mainMenu error");
        close(socketfd);
        exit(-1);
    }
    char mainMenuBuffer[BUFFER_SIZE];
    bzero(mainMenuBuffer, sizeof(mainMenuBuffer));
    readMessage(mainMenu, mainMenuBuffer, sizeof(mainMenuBuffer) - 1);

    /* 打开功能页面文件 */
    int funcMenu = open("funcMenu", O_RDONLY);
    if (funcMenu == -1)
    {
        perror("open funcMenu error");
        close(mainMenu);
        close(socketfd);
        exit(-1);
    }
    char funcMenuBuffer[BUFFER_SIZE];
    bzero(funcMenuBuffer, sizeof(funcMenuBuffer));
    readMessage(funcMenu, funcMenuBuffer, sizeof(funcMenuBuffer) - 1);

    /* 登录标志位 */
    int flag = 0;
    /* 开始执行功能 */
    int choice = 0;
    while(1)
    {
        choice = 0;
        char input[BUFFER_SIZE];
        bzero(input, sizeof(input));
        printf("%s\n", mainMenuBuffer);
        printf("请选择你需要的功能：\n");
        scanf("%d", &choice);
        fgets(input, sizeof(input), stdin);
        sscanf(input, "%d", &choice);
        if(choice >= 1 && choice <= 3)
        {
            writeData(socketfd, &choice, sizeof(choice));
        }
        /* 清空输入缓冲区 */
        system("clear");

        switch (choice)
        {
        /* 注册 */
        case REGISTER:
            /* 用户注册并创建好友列表 */
            registerUser(socketfd);
            sleep(2);
            system("clear");
            break;
        
        /* 登录 */
        case LOG_IN:
            /* 用户登录函数 */
            flag = loginUser(socketfd);
            sleep(2);
            system("clear");
            break;
        
        /* 退出 */
        case EXIT:
            char response[BUFFER_SIZE];
            bzero(response, sizeof(response));
            /* 接收服务器的响应 */
            readMessage(socketfd, response, sizeof(response) - 1);

            /* 解析服务器的响应并进行处理 */
            if(strncmp(response, "客户端退出", sizeof("客户端退出")) == 0)
            {
                printf("客户端退出\n");
                sleep(1);
                close(mainMenu);
                close(funcMenu);
                exit(-1);
            }
            else
            {
                printf("客户端退出失败\n");
            }
            break;

        default:
            break;
        }

        /* 客户端缓冲区 */
        char cliRecvBuffer[BUFFER_SIZE];
        bzero(cliRecvBuffer, sizeof(cliRecvBuffer));
        /* 服务器缓冲区 */
        char serRecvBuffer[BUFFER_SIZE];
        bzero(serRecvBuffer, sizeof(serRecvBuffer));

        int connect = 0;
        int readBytes = 0;
        /* 模式选择变量 */
        int mode = 0;
        while(flag)
        {
            mode = 0;
            char input2[BUFFER_SIZE];
            bzero(input2, sizeof(input2));
            printf("%s\n", funcMenuBuffer);
            printf("请选择你需要的功能：\n");
            fgets(input2, sizeof(input2), stdin);
            sscanf(input2, "%d", &mode);
            if(mode >= 1 && mode <= 3)
            {
                writeData(socketfd, &mode, sizeof(mode));
            }
            /* 清空输入缓冲区 */
            system("clear");

            switch(mode)
            {
            /* 私聊 */
            case PRIVATE_CHAT:
                printf("选择私聊好友：\n");
                scanf("%s", cliRecvBuffer);
                write(socketfd, cliRecvBuffer, sizeof(cliRecvBuffer));
                int readBytes = read(socketfd, serRecvBuffer, sizeof(serRecvBuffer) - 1);
                if(strncmp(serRecvBuffer, "对方在线", readBytes) == 0)
                {
                    printf("%s\n", serRecvBuffer);
                    sleep(3);
                    system("clear");
                    connect = 1;
                }
                else
                {
                    printf("%s\n", serRecvBuffer);
                    sleep(3);
                    system("clear");
                    connect = 0;
                }
                while(connect)
                {
                    chatContentDisplay();
                    printf("请输入文字：\n");
                    scanf("%s", cliRecvBuffer);
                    write(socketfd, cliRecvBuffer, sizeof(cliRecvBuffer));
                    sleep(1);
                    system("clear");
                }
                break;

            /* 加好友 */
            case FRIEND_ADD:
                
                break;

            /* 退出 */
            case INTERNAL_EXIT:
                char response[BUFFER_SIZE];
                bzero(response, sizeof(response));
                /* 接收服务器的响应 */
                readMessage(socketfd, response, sizeof(response) - 1);

                /* 解析服务器的响应并进行处理 */
                if(strncmp(response, "账户退出", sizeof("账户退出")) == 0)
                {
                    printf("账户退出\n");
                    sleep(1);
                    system("clear");
                    flag = 0;
                }
                else if(strncmp(response, "账户退出失败", sizeof("账户退出失败")) == 0)
                {
                    printf("账户退出失败\n");
                    sleep(1);
                    system("clear");
                }
                break;

            default:
                break;
            }
        }
    }
    return 0;
}

/* 用户注册函数 */
static void registerUser(int socketfd)
{
    char username[DEFAULT_LOGIN_NAME];
    bzero(username, sizeof(username));
    char password[DEFAULT_LOGIN_PAWD];
    bzero(password, sizeof(password));
    char response[BUFFER_SIZE];
    bzero(response, sizeof(response));

    printf("请输入用户名：");
    scanf("%s", username);
    printf("请输入密码：");
    scanf("%s", password);

    /* 发送注册信息给服务器 */
    writeMessage(socketfd, username, sizeof(username) - 1);
    writeMessage(socketfd, password, sizeof(password) - 1);

    /* 接收服务器的响应 */
    readMessage(socketfd, response, sizeof(response) - 1);

    /* 解析服务器的响应并进行处理 */
    if(strncmp(response, "注册成功", sizeof("注册成功")) == 0)
    {
        printf("注册成功\n");
    }
    else if(strncmp(response, "注册失败，用户已存在", sizeof("注册失败，用户已存在")) == 0)
    {
        printf("注册失败，用户已存在\n");
    }
    else
    {
        printf("系统出错\n");
        exit(-1);
    }
    /* 创建好友列表 */
    sqlite3 *db = NULL;
    char friendlistname[BUFFER_SIZE];
    bzero(friendlistname, sizeof(friendlistname));
    strncat(friendlistname, username, sizeof(username));
    strncat(friendlistname, "FriendList.db", sizeof("FriendList.db"));
    /* 打开数据库：如果数据库不存在，那么就创建 */
    int ret = sqlite3_open(friendlistname, &db);
    if(ret != SQLITE_OK)
    {
        perror("sqlite open error");
        exit(-1);
    }
    char * errormsg = NULL;
    const char *sql = "create table if not exists friendlist (username text not NULL)";
    ret = sqlite3_exec(db, sql, NULL, NULL, &errormsg);
    if(ret != SQLITE_OK)
    {
        printf("sqlite exec error: %s\n", errormsg);
        exit(-1);
    }
}

/* 用户登录函数 */
static int loginUser(int socketfd)
{
    char username[DEFAULT_LOGIN_NAME];
    bzero(username, sizeof(username));
    char password[DEFAULT_LOGIN_PAWD];
    bzero(password, sizeof(password));
    char response[BUFFER_SIZE];
    bzero(response, sizeof(response));

    printf("请输入用户名：");
    scanf("%s", username);
    printf("请输入密码：");
    scanf("%s", password);

    /* 发送登录信息给服务器 */
    writeMessage(socketfd, username, sizeof(username) - 1);
    writeMessage(socketfd, password, sizeof(password) - 1);

    /* 接收服务器的响应 */
    readMessage(socketfd, response, sizeof(response) - 1);

    /* 解析服务器的响应并进行处理 */
    if(strncmp(response, "登录成功", sizeof("登录成功")) == 0)
    {
        printf("登录成功\n");
        return 1;
    }
    else if(strncmp(response, "用户已在别处登录", sizeof("用户已在别处登录")) == 0)
    {
        printf("用户已在别处登录\n");
        return 0;
    }
    else if(strncmp(response, "密码错误", sizeof("密码错误")) == 0)
    {
        printf("密码错误\n");
        return 0;
    }
    else if(strncmp(response, "用户不存在", sizeof("用户不存在")) == 0)
    {
        printf("用户不存在\n");
        return 0;
    }
    else
    {
        printf("系统出错\n");
        exit(-1);
    }
}

/* 聊天记录显示函数 */
static void chatContentDisplay()
{
    sqlite3 *db = NULL;
    const char *sql = NULL;
    char * errormsg = NULL;
    char **result = NULL;
    int start = 1;
    int row = 0;
    int column = 0;

    int ret = sqlite3_open("aa->bb.db", &db);
    if(ret != SQLITE_OK)
    {
        perror("sqlite open error");
        exit(-1);
    }

    sql = "select *from chat";
    ret = sqlite3_get_table(db, sql, &result, &row, &column, &errormsg);
    if(ret != SQLITE_OK)
    {
        printf("sqlite_exec_error: %s\n", errormsg);
        exit(-1);
    }
    if(row > 10)
    {
        start = row - 9;
    }

    for(int idx = start; idx <= row; idx++)
    {
        printf("%s:", result[idx * column]);
        printf("%s\n", result[idx * column + 1]);
    }

    /* 关闭数据库 */
    sqlite3_close(db);
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
static void writeData(int fd, int *data, int dataSize)
{
    int ret = write(fd, data, dataSize);
    if (ret < 0)
    {
        perror("write error");
        close(fd);
        exit(-1);
    }
}