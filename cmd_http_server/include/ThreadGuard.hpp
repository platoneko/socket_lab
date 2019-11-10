#include <thread>

class ThreadGuard{  
    public:  
        explicit ThreadGuard(std::thread &t):_t(t){}  
        ~ThreadGuard(){  
            if(_t.joinable()){  // 检测是很有必要的，因为thread::join只能调用一次，要防止其它地方意外join了  
               _t.join();  
            }  
        }  
        ThreadGuard(const ThreadGuard &)=delete;  // c++11中这样声明表示禁用copy constructor需要-std=c++0x支持  
        ThreadGuard &operator=(const ThreadGuard &)=delete;  
    private:  
        std::thread &_t;  
}; 