#include <iostream>
#include <list>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>
#include <cstring>

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
    #include <sys/mman.h>
    #include <sys/stat.h>        /* For mode constants */
    #include <fcntl.h>
    #include <errno.h>
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
    std::list<std::unique_ptr<Process_manager>> child_processes;
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

	pid_t pid()
    {
		return getpid();
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
			pid_t end_pid = wait(NULL);
			child_processes.remove_if([end_pid](std::unique_ptr<Process_manager> &mgr_ptr)
				{
					return mgr_ptr->pid() == end_pid;
				}
			);
			pending--;
		}
    }
};

typedef unsigned int ulli;

int main(int argc, char *argv[])
{
	int size;
	std::cin >> size;
	auto __start__ = std::chrono::steady_clock::now();
	auto clock = [&__start__]{return std::chrono::steady_clock::now() - __start__;};

	auto pos_of = [&size](ulli i, ulli j) {return i * size + j;};
	auto &matrix_at = pos_of;
	// no fork
	{
		std::vector<ulli> matrix(size * size);
		for(int i(0); i < size; i++)
			for(int j(0); j < size; j++)	
				for(int interator(0); interator < size; interator++)
					matrix.at(pos_of(i, j)) += matrix_at(interator, j) * matrix_at(i, interator);
		std::cout << "Time spent: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock()).count()
				  << " ms, check sum: " << static_cast<ulli>(std::accumulate(matrix.begin(), matrix.end(), 0)) << std::endl;

	}

	__start__ = std::chrono::steady_clock::now();
	// fork & using System V IPC system
	{
		//key_t key = ftok("/tmp", 'm'); // Please don't conflict
		//if(key < 0)
		//	std::cerr << "[0] ftok failed, err: " << std::strerror(errno) << std::endl;
		
		int shm_id = shmget( IPC_PRIVATE, size * size * sizeof(ulli)/* matrix */, IPC_CREAT | 0666);
		if(shm_id < 0)
			std::cerr << "[0] shm_id dead, err: " << std::strerror(errno) << std::endl;

		void * shm = shmat(shm_id, NULL, 0);
		if(shm == (void *) -1)
			std::cerr << "[0] shm failed, err: " << std::strerror(errno) << std::endl;

		std::memset(shm, 0, size * size * sizeof(ulli));

		
		Process_manager master([&size, &pos_of, &shm_id]
	    	{		
	 		    void * shm = shmat(shm_id, NULL, 0);
				if(shm == (void *) -1)
					std::cerr << "[1] shm failed, err: " << std::strerror(errno) << std::endl;

	 		    ulli * matrix = static_cast<ulli *>(shm);

				int start_pos = 0;
	 		    for(int i(0); i < size/4; i++)
					for(int j(0); j < size; j++, start_pos++)
						for(int interator(0); interator < size; interator++)
							matrix[start_pos] += pos_of(interator, j) * pos_of(i, interator);
				//std::cout << "[1] "<< start_pos << "\n";
	    	}
		);
		master.register_child([&size, &pos_of, &shm_id]
		    {		    	
				void * shm = shmat(shm_id, NULL, 0);
				if(shm == (void *) -1)
					std::cerr << "[2] shm failed, err: " << std::strerror(errno) << std::endl;

				ulli * matrix = static_cast<ulli *>(shm);
				int start_pos((size / 4) * size); 
				for(int i(size/4); i < size/2; i++)
					for(int j(0); j < size; j++, start_pos++)	
						for(int interator(0); interator < size; interator++)
							matrix[start_pos] += pos_of(interator, j) * pos_of(i, interator);
				//std::cout << "[2] "<< start_pos << "\n";
		    }
		);
		master.register_child([&size, &pos_of, &shm_id]
		    {	
				void * shm = shmat(shm_id, NULL, 0);
				if(shm == (void *) -1)
					std::cerr << "[3] shm failed, err: " << std::strerror(errno) << std::endl;
 

				ulli * matrix = static_cast<ulli *>(shm);
				int start_pos((size / 2) * size);
				for(int i(size/2); i < size*3/4; i++)
					for(int j(0); j < size; j++, start_pos++)	
						for(int interator(0); interator < size; interator++)
							matrix[start_pos] += pos_of(interator, j) * pos_of(i, interator);
				//std::cout << "[3] "<< start_pos << "\n";
		    }
		);
		master.register_child([&size, &pos_of, &shm_id]
		    {
				void * shm = shmat(shm_id, NULL, 0);
				if(shm == (void *) -1)
					std::cerr << "[4] shm failed, err: " << std::strerror(errno) << std::endl;
				
 
				ulli * matrix = static_cast<ulli *>(shm);
				int start_pos((size * 3 / 4) * size);
				for(int i(size*3/4); i < size; i++)
					for(int j(0); j < size; j++, start_pos++)	
						for(int interator(0); interator < size; interator++)
							matrix[start_pos] += pos_of(interator, j) * pos_of(i, interator);
				//std::cout << "[4] "<< start_pos << "\n";
		    }
		);

		master.exec_all_child();	


		ulli * result = static_cast<ulli *>(shm), sum(0);
  		for(int i(0); i < size * size; i++)		
  			sum += result[i];
		shmdt(shm);
	
		std::cout << "Time spent: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock()).count()
				  << " ms, check sum: " << sum << std::endl;
	}

    return 0;
}
