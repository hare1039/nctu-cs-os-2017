#include <iostream>
#include <list>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>
#include <cstring>

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
//		for(auto i: matrix)
//		{
//			std::cout << "[no-fork] " << i << "\n";
//		}
		std::cout << "Time spent: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock()).count()
				  << " ms, check sum: " << static_cast<ulli>(std::accumulate(matrix.begin(), matrix.end(), 0)) << std::endl;

	}

	__start__ = std::chrono::steady_clock::now();
	// fork & using System V IPC system
	{
		key_t key = ftok("Hi, I'm key", 'R'); // Please don't conflict
		int shm_id = shmget( key, 4 * sizeof(ulli)/* 1 parent, 3 for children return partial sum*/, IPC_CREAT | 0660);
		void * shm = shmat(shm_id, NULL, 0);
		std::memset(shm, 0, 4 * sizeof(ulli));
		Process_manager master([&key, &size, &pos_of]
	    	{
	 		    int shm_id = shmget( key, 4 * sizeof(ulli), S_IRUSR | 0660);
	 		    void * shm = shmat(shm_id, NULL, 0);
	 		    ulli * return_arr = static_cast<ulli *>(shm);
				std::vector<ulli> matrix(size * size / 2);
	 		    for(int i(0); i < size/4; i++)
					for(int j(0); j < size; j++)	
						for(int interator(0); interator < size; interator++)
							matrix.at(pos_of(i, j)) += pos_of(interator, j) * pos_of(i, interator);
//				for(auto i: matrix)
//				{
//					std::cout << "[0~3] " << i << "\n";
//				}
				return_arr[0] = std::accumulate(matrix.begin(), matrix.end(), 0);
	    	}
		);
		master.register_child([&key, &size, &pos_of]
		    {
				int shm_id = shmget( key, 4 * sizeof(ulli), S_IRUSR | 0660);
				void * shm = shmat(shm_id, NULL, 0);
				ulli * return_arr = static_cast<ulli *>(shm);
				std::vector<ulli> matrix(size * size / 2);
				for(int i(size/4); i < size/2; i++)
					for(int j(0); j < size; j++)	
						for(int interator(0); interator < size; interator++)
							matrix.at(pos_of(i - size/4, j)) += pos_of(interator, j) * pos_of(i, interator);
// 				for(auto i: matrix)
// 				{
// 					std::cout << "[4~7] " << i << "\n";
// 				}
				return_arr[1] = std::accumulate(matrix.begin(), matrix.end(), 0);
		    }
		);
		master.register_child([&key, &size, &pos_of]
		    {
				int shm_id = shmget( key, 4 * sizeof(ulli), S_IRUSR | 0660);
				void * shm = shmat(shm_id, NULL, 0);
				ulli * return_arr = static_cast<ulli *>(shm);
				std::vector<ulli> matrix(size * size / 2);
				for(int i(size/2); i < size*3/4; i++)
					for(int j(0); j < size; j++)	
						for(int interator(0); interator < size; interator++)
							matrix.at(pos_of(i - size/2, j)) += pos_of(interator, j) * pos_of(i, interator);
// 				for(auto i: matrix)
// 				{
// 					std::cout << "[8~11] " << i << "\n";
// 				}
				return_arr[2] = std::accumulate(matrix.begin(), matrix.end(), 0);
		    }
		);
		master.register_child([&key, &size, &pos_of]
		    {
				int shm_id = shmget( key, 4 * sizeof(ulli), S_IRUSR | 0660);
				void * shm = shmat(shm_id, NULL, 0);
				ulli * return_arr = static_cast<ulli *>(shm);
				std::vector<ulli> matrix(size * size / 2);
				for(int i(size*3/4); i < size; i++)
					for(int j(0); j < size; j++)	
						for(int interator(0); interator < size; interator++)
							matrix.at(pos_of(i - size*3/4, j)) += pos_of(interator, j) * pos_of(i, interator);
//				for(auto i: matrix)
//				{
//					std::cout << "[12~15] " << i << "\n";
//				}
				return_arr[3] = std::accumulate(matrix.begin(), matrix.end(), 0);
		    }
		);

		master.exec_all_child();	


		ulli * result = static_cast<ulli *>(shm), sum(0);
		for(int i(0); i<4; i++)		
			sum += result[i];
		
			
		shmdt(shm);
	
		std::cout << "Time spent: " << std::chrono::duration_cast<std::chrono::milliseconds>(clock()).count()
				  << " ms, check sum: " << sum << std::endl;
	}

    return 0;
}


	//int shm_fd = shm_open("the_matrix", O_CREAT | O_RDWR, 0666);
	//std::vector<int> matrix_shared(size * size);
	//ftruncate(shm_fd, matrix_shared.size());
	//void *ptr = mmap(0, matrix_shared.size(), PROT_WRITE, MAP_SHARED, shm_fd, 0);
