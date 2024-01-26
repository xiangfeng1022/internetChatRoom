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

#define SERVER_PORT 8080
#define SERVER_ADDR "172.30.149.120"
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
            break;
        
        /* 注册 */
        case REGISTER:
            /* code */
            break;

        default:
            break;
        }
        
    }
    

    return 0;
}