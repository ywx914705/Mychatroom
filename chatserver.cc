#include <iostream>
#include <memory>
#include "chatserver.hpp"
using namespace std;
int main(int argc,char*argv[])
{
    if(argc!=2)
    {
        cout<<"请以./chatserver  port的形式输入"<<endl;
    }
    uint16_t port = atoi(argv[1]);
    unique_ptr<Chatserver> us(new Chatserver(port));
    us->initchatserver();
    cout<<"开始调用Dispatch()"<<endl;
    us->Dispatch();
    return 0;
}
