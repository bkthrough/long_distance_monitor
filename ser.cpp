#include "control.h"
#include <iostream>

using namespace std;
int main()
{
    int port = 8888;
    control *c = new control(port);
    const int QUIT = 1;

    c->init_sock();
    for(;;){
        if(QUIT == c->choose()){
            break;
        }
    }
    delete c;

    return 0;
}
