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

#define SERVER_PORT 8080
// #define SERVER_ADDR "172.30.149.120"
#define SERVER_ADDR "172.27.105.168"
#define BUFFER_SIZE 300

enum CLIENT_CHOICE
{
    LOG_IN = 1,
    REGISTER,
    PRIVATE_CHAT,
};

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
    read(mainMenu, mainMenuBuffer, sizeof(mainMenuBuffer) - 1);

    /* 登录后打开功能页面 */
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
    read(funcMenu, funcMenuBuffer, sizeof(funcMenuBuffer) - 1);

    int choice = 0;
    char c = '0';
    /* 登录标志位 */
    int flag = 0;
    /* 开始执行功能 */
    while (1)
    {
        printf("%s\n", mainMenuBuffer);
        printf("请选择你需要的功能：\n");
        scanf("%d", &choice);
        /* 清空输入缓冲区 */
        write(socketfd, &choice, sizeof(choice));
        while ((c = getchar()) != EOF && c != '\n');

        switch (choice)
        {

        /* 登录 */
        case LOG_IN:
            /* code */
            flag = 1;
            break;
        
        /* 注册 */
        case REGISTER:
            /* code */
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
            printf("选择聊天模式：\n");
            printf("    1.私聊\n");
            printf("    2.群聊\n");
            scanf("%d", &mode);
            printf("mode:%d\n", mode);
            if(mode == 1)
            {
                printf("选择私聊好友：\n");
                scanf("%s", cliRecvBuffer);
                write(socketfd, cliRecvBuffer, sizeof(cliRecvBuffer));
                int readBytes = read(socketfd, serRecvBuffer, sizeof(serRecvBuffer) - 1);
                if(strncmp(serRecvBuffer, "对方在线", readBytes) == 0)
                {
                    printf("%s\n", serRecvBuffer);
                    connect = 1;
                }
                else
                {
                    printf("%s", serRecvBuffer);
                    connect = 0;
                }
                while(connect)
                {
                    printf("请输入文字：\n");
                    scanf("%s", cliRecvBuffer);
                    write(socketfd, cliRecvBuffer, sizeof(cliRecvBuffer));
                }
            }
            else if(mode == 2)
            {
                
            }
            else
            {
                break;
            }
        }
    }
    

    return 0;
}