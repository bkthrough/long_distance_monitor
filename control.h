#ifndef _CONTROL_H
#define _CONTROL_H

#include "utili.h"
#include <pthread.h>
#include <vector>
#include <map>

template <typename TYPE, void (TYPE::*thread_func)()>
void *out_thread(void *para);

class control
{
public:
    control(int pt):m_port(pt){};
    ~control();
public:
    void init_sock();
    int  choose();
    void thread_fun();
    void close_vec_map();
private:
    void find_computer();
    void take_photo(int fd);
    void get_file(int fd, const char *path);
    void exe_cmd(int fd, const char *cmd);
    void exe_file(int fd, const char *file);
    void deal_event();
    void deal_data(sock_data *d, int fd, size_t sz);
    const int find_fd(const char *addr);
    const int get_fd();
private:
    std::vector<int>      m_v_fd;
    std::map<int, char *> m_map_addr;
    const int             m_port;
    int                   m_sockfd;
    int                   m_udpfd;
    int                   m_epoll_fd;
    pthread_t             m_tid;
};

#endif
