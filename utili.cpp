#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <iostream>
#include "utili.h"

using namespace std;

void epoll_add(int epoll_fd, int sockfd, int state)
{
    struct epoll_event ev;

    ev.events = state;
    ev.data.fd = sockfd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);
}

void epoll_del(int epoll_fd, int sockfd, int state)
{
    struct epoll_event ev;

    ev.events = state;
    ev.data.fd = sockfd;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, &ev);
}

void upload_file(int sockfd, const char *path, int len, int state)
{
    int file_fd = 0;
    FILE *fp = NULL;
    struct stat st;
    char *file_start = NULL;
    off_t offset = 0;
    size_t left_sz = 0;

    if(path == NULL)
        return;
    if(NULL == (fp = fopen(path, "r"))){
        cout << "this doc not found, please check and then choose file!" << endl;
        return ;
    }
    file_fd = fileno(fp);
    fstat(file_fd, &st);
    left_sz = st.st_size;
    //判断文件如果大于3G，那么就需要切分
    //使用do_while解决文件为空的情况
    do{
        if(left_sz < g_max_content)
            len = left_sz;
        else
            len = g_max_content;
        file_start = (char *)mmap(NULL, len, PROT_READ, MAP_SHARED, file_fd, 0);
        //通过send_file具体来做上传的工作
        send_file(sockfd, path, file_start, len, state);
        left_sz -= len;
        munmap(file_start, len);
    }while(left_sz > 0);
}

char *get_local_ip(int sockfd)
{
    struct ifconf ifconf;
    char buf[512];
    struct ifreq *ifreq;

    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;
    ioctl(sockfd, SIOCGIFCONF, &ifconf);
    ifreq = (struct ifreq *)buf;
    for(int i = (ifconf.ifc_len/sizeof(struct ifreq)); i > 0; --i){
        if(ifreq->ifr_flags == AF_INET){
            //不是回环地址，就返回
            if(0 != strcmp(ifreq->ifr_name, "lo")){
                return inet_ntoa(((struct sockaddr_in *)&(ifreq->ifr_addr))->sin_addr);
            }
            ++ifreq;
        }
    }
}

void send_file(int sockfd, const char *path, const char *start, int len, int state)
{
    int left_sz = 0;
    sock_data *d;
    socklen_t data_len = 0;
    char *ip;
    char name[255] = {0};
    int path_len = 0;
    int i = 0;

    //获取本机ip
    ip = get_local_ip(sockfd);
    left_sz = len;
    path_len = strlen(path);
    //把basename赋值给name
    for(i = path_len-1; i >= 0; --i){
        if(path[i] == '/')
            break;
    }
    for(int j = i+1; j < path_len; ++j){
        name[j-i-1] = path[j];
    }
    //如果文件为空
    if(len == 0){
        d = (struct sock_data *)malloc(sizeof(sock_data));
        d->state = state;
        sprintf(d->filename, "%s-%s", ip, name);
        data_len = sizeof(*d);
        send(sockfd, d, data_len, 0);
    }
    while(left_sz > 0){
        if(left_sz <= g_max_send_sz){
            d = (struct sock_data *)malloc(sizeof(sock_data)+left_sz);
            memset(d, 0, sizeof(d));
            d->len = left_sz;
            memcpy(d->buf, start, d->len);
            d->state = state;
        }else{
            d = (struct sock_data *)malloc(sizeof(sock_data)+g_max_send_sz);
            memset(d, 0, sizeof(*d)+g_max_send_sz);
            d->len = g_max_send_sz;
            memcpy(d->buf, start, d->len);
            start += d->len;
            d->state = state;
        }
        sprintf(d->filename, "%s-%s", ip, name);
        data_len = sizeof(*d) + d->len;
        send(sockfd, d, data_len, 0);
        left_sz -= d->len;
    }
}

