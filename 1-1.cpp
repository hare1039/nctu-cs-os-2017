#include <iostream>
#include <list>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>

#if __cplusplus < 201103L
    #error("Please compile using -std=c++11 or higher")
#endif

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
    Process_manager(std::function<void(void)> f): context(f) {}
    Process_manager(std::function<void(void)> f, std::list<std::unique_ptr<Process_manager>> &l):
        context(f),
        child_processes(std::move(l)){}
    ~Process_manager()
    {
        wait(NULL);
    }
    std::list<std::unique_ptr<Process_manager> > child_processes;
    std::function<void(void)> context;
    
    Process_manager& register_child(std::function<void(void)> child_func)
    {
        child_processes.push_back(std::unique_ptr<Process_manager>(new Process_manager(child_func)));
        return *this;
    }
    
    Process_manager& register_child(std::unique_ptr<Process_manager> &p)
    {
        child_processes.push_back(std::move(p));
        return *this;
    }
    
    void exec_all_child()
    {
        for(auto &i : child_processes)
        {
            pid_t x = fork();
            if(x < 0)
            {
                std::cerr << "fork error";
            }
            else if(x == 0)
            {
                i->exec_all_child();
                exit(0);
            }
        }
		this->context();
		long pending = child_processes.size();
		while(pending)
		{
			wait(NULL);
			pending--;
		}
    }
};


int main(int argc, char *argv[])
{
    Process_manager this_main([](){
        std::cout << "Main Process ID: " << getpid() << std::endl;
    });
    
    std::unique_ptr<Process_manager> fork1(new Process_manager([](){
		std::cout << "Fork1. I'm child " << getpid() << ", my parent is " << getppid() << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork20(new Process_manager([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::cout << "Fork2. I'm child " << getpid() << ", my parent is " << getppid() << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork21(new Process_manager([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::cout << "Fork2. I'm child " << getpid() << ", my parent is " << getppid() << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork30(new Process_manager([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::cout << "Fork3. I'm child " << getpid() << ", my parent is " << getppid() << "." << std::endl;
    }));
    
    std::unique_ptr<Process_manager> fork31(new Process_manager([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        std::cout << "Fork3. I'm child " << getpid() << ", my parent is " << getppid() << "." << std::endl;
    }));
    
    fork20->register_child(fork30);
    fork21->register_child(fork31);
    fork1->register_child(fork20).register_child(fork21);
    
    
    this_main.register_child(fork1);
    this_main.exec_all_child();
    return 0;
}

