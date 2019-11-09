#include <thread>

class ThreadGuard{  
    public:  
        explicit ThreadGuard(std::thread& t):_t(t){}  
        ~ThreadGuard(){  
            if(_t.joinable()){//检测是很有必要的，因为thread::join只能调用一次，要防止其它地方意外join了  
               _t.join();  
            }  
        }  
        //thread_guard(const thread_guard&)=delete;//c++11中这样声明表示禁用copy constructor需要-std=c++0x支持，这里采用boost::noncopyable已经禁止了拷贝和复制  
        //thread_guard& operator=(const thread_guard&)=delete;  
    private:  
        std::thread& _t;  
}; 