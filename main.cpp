#include <iostream>
#include <list>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>

extern "C"
{
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
}

class Process_manager
{
public:
    Process_manager(std::function<void(int)> f): context(f) {}
    Process_manager(std::function<void(int)> f, std::list<std::unique_ptr<Process_manager>> &l):
        context(f),
        child_processes(std::move(l)){}
    ~Process_manager()
    {
        wait(NULL);
    }
    std::list<std::unique_ptr<Process_manager> > child_processes;
    std::function<void(int)> context;
    
    Process_manager& register_child(std::function<void(int)> child_func)
    {
        child_processes.push_back(std::unique_ptr<Process_manager>(new Process_manager(child_func)));
        return *this;
    }
    
    Process_manager& register_child(std::unique_ptr<Process_manager> &p)
    {
        child_processes.push_back(std::move(p));
        return *this;
    }

    
    void exec_all_child(pid_t parent)
    {
        bool is_not_child = true;
        pid_t p = getpid();
        for(auto &i : child_processes)
        {
            pid_t x = fork();
            if(x < 0)
            {
                std::cerr << "fork error";
            }
            else if(x == 0)
            {
                i->exec_all_child(p);
                is_not_child = false;
                break;
            }
        }
        if(is_not_child)
            this->context(parent);
    }
};

int main(int argc, char *argv[])
{
    Process_manager this_main([](pid_t){
        std::cout << "Main Process ID: " << getpid() << std::endl;
    });
    
    std::unique_ptr<Process_manager> fork1(new Process_manager([](pid_t parent){
        std::cout << "Fork1. I'm child " << getpid() << ", my parent is " << parent << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork20(new Process_manager([](pid_t parent){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::cout << "Fork2. I'm child " << getpid() << ", my parent is " << parent << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork21(new Process_manager([](pid_t parent){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::cout << "Fork2. I'm child " << getpid() << ", my parent is " << parent << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork30(new Process_manager([](pid_t parent){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::cout << "Fork3. I'm child " << getpid() << ", my parent is " << parent << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork31(new Process_manager([](pid_t parent){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::cout << "Fork3. I'm child " << getpid() << ", my parent is " << parent << "." << std::endl;
    }));
    
    fork20->register_child(fork30);
    fork21->register_child(fork31);
    fork1->register_child(fork20).register_child(fork21);
    
    
    this_main.register_child(fork1);
    this_main.exec_all_child(-1);
    return 0;
}

