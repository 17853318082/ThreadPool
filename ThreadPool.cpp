/*
静态线程池，用于配合epoll反应堆完成高并发
*/
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <list>

using  namespace std;
#ifndef _THREAD_POOL_
#define _THREAD_POOL_

// 创建任务类
class Task{
public:
    Task(){};
    Task(int n):n(n){};
    Task(void (*fun)(int,int,void*),int fd,int events,void* arg)      // 构造函数，三个参数
    :fun(fun),fd(fd),events(events),arg(arg){};
    // void run(){
    //     fun(fd,events,arg);
    // }
    void run(){
        this_thread::sleep_for(1s);
        cout<<"线程："<<this_thread::get_id()<<"  任务:"<<n<<"执行"<<endl;
    }
private:
    void (*fun)(int,int,void*);   // 任务函数指针
    // 指针所需要的三个参数
    int fd;
    int events;
    void* arg;
    int n;
    // 任务格式
};

// 创建线程池类
class ThreadPool
{
public:
    ThreadPool();
    ThreadPool(int num);
    ThreadPool(int num,int max_thread_num);
    ThreadPool(int num,int max_thread_num,int add_num);
    void ThreadWork();         // 线程工作
    void AddTask(Task task);   // 向线程池添加任务
    void ManageWorker();       // 管理线程工作方法
    void Exit();
    ~ThreadPool();
private:
    int thread_num;                          // 线程池,核心线程数
    int max_thread_num;                      // 线程池最大线程数
    std::list<std::thread> workers;          // 工作线程队列，用于保存工作线程
    std::queue<Task> tasks;                  // 任务队列

    std::mutex queue_mutex;            // 任务队列锁
    std::mutex thread_queue_mutex;     // 线程队列锁
    std::mutex thread_busy_mutex;      // 正在执行的线程数量锁
    std::condition_variable condition; // 条件变量

    bool is_exit;                      // 是否退出
    std::thread manage_tid;            // 管理者线程id
    int busy_thread_num;               // 记录繁忙线程数量，即现在正在执行任务的线程数量
    
    int add_num;                   // 每次增加线程的数量，暂定为5
};

/*初始化线程池，输入参数核心线程数：thread_num*/
ThreadPool::ThreadPool(int num) : thread_num(num),is_exit(false)
{
    max_thread_num = 200;   // 最大线程数量暂定为200
    busy_thread_num = 0;    // 繁忙线程初始为0
    add_num = 5;
    // 构造函数创建num个线程
    for (int i = 0; i < thread_num; i++)
    {
        workers.emplace_back(&ThreadPool::ThreadWork, this); // 将线程加入工作队列，线程全部都执行ThreadWork方法
    }
    // 开启管理者线程
    manage_tid = std::thread(&ThreadPool::ManageWorker,this);
}
/*初始化线程池，输入两个参数，核心线程数与最大线程数*/
ThreadPool::ThreadPool(int num,int max_thread_num) : thread_num(num),max_thread_num(max_thread_num),is_exit(false)
{
    busy_thread_num = 0;    // 繁忙线程初始为0
    add_num = 5;
    // 构造函数创建num个线程
    for (int i = 0; i < thread_num; i++)
    {
        workers.emplace_back(&ThreadPool::ThreadWork, this); // 将线程加入工作队列，线程全部都执行ThreadWork方法
    }
    // 开启管理者线程
    manage_tid = std::thread(&ThreadPool::ManageWorker,this);
}
/*初始化线程池：输入三个参数：核心线程数，最大线程数和每次线程增加与减少的数量*/
ThreadPool::ThreadPool(int num,int max_thread_num,int add_num) 
                      : thread_num(num),max_thread_num(max_thread_num),add_num(add_num),is_exit(false)
{
    busy_thread_num = 0;    // 繁忙线程初始为0
    // 构造函数创建num个线程
    for (int i = 0; i < thread_num; i++)
    {
        workers.emplace_back(&ThreadPool::ThreadWork, this); // 将线程加入工作队列，线程全部都执行ThreadWork方法
    }
    // 开启管理者线程
    manage_tid = std::thread(&ThreadPool::ManageWorker,this);
}


/*管理者线程,用于管理线程，调节线程的数量*/
void ThreadPool::ManageWorker()
{
    /*
    1. 循环检测线程池内现有的线程是否够用
    2. 够用的条件，目前等待任务数小于等于线程池中的线程数
    3. 线程过量条件，任务队列已无任务，且当前繁忙的线程数较少，此时应该去除部分线程
    */
    while(!is_exit){
        this_thread::sleep_for(5s);   // 每五秒检测一次，正在执行的线程数量
        cout<<"当前线程数:"<<workers.size()<<endl;
        // 如果任务队列里有任务未执行,且剩余任务数量大于当前线程池中的线程数时，说明缺少线程，线程已不够用，并且线程数量还未到最大线程数，此时应该添加线程
        if(tasks.size()>workers.size()&&workers.size()<max_thread_num){
            cout<<"还有"<<tasks.size()<<"个任务未被处理,"<<"线程数量不够，需要添加线程！！！"<<endl;
            int add = 0;
            // 每次添加规定的线程数
            while(add<add_num&&workers.size()<max_thread_num){
                cout<<"正在添加线程数量"<<endl;
                add++;
                thread_queue_mutex.lock();   // 线程队列加锁
                workers.push_back(thread(&ThreadPool::ThreadWork,this)); // 添加线程
                thread_queue_mutex.unlock(); // 线程队列解锁
            }
        }
        // 任务队列中没有任务，且繁忙线程的数量小于最小的线程数量时，减少线程数量
        if(tasks.size()==0&&workers.size()>thread_num&&busy_thread_num<thread_num){
            cout<<"还有"<<workers.size()-thread_num<<"多余线程存活"<<endl;
            int reduce = 0;
            // 每次减少的数量与添加的数量相同,空的时候每次最多唤醒5个线程进行销毁
            while(reduce<add_num&&workers.size()>thread_num){
                cout<<"正在减少线程数量"<<endl;
                reduce++;
                condition.notify_one();    // 唤醒一个线程进行销毁
            }
        }
    }
}
/*线程池执行任务方法：用于调用线程执行任务*/
void ThreadPool::ThreadWork()
{
    // 线程任务方法,退出时自动退出
    while (!is_exit)
    {
        // 添加线程销毁的,只有任务队列为空时才进入线程销毁模式
        Task task;
        {
            // 创建唯一锁对象，这个锁是用来锁住任务队列
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            // 条件阻塞，当任务队列非空，或者是线程池销毁时加锁
            this->condition.wait(lock, [this]    
                                { return this->is_exit || !this->tasks.empty()||
                                this->tasks.empty()&&this->workers.size()>this->thread_num    
                                &&busy_thread_num<thread_num; });
            // 清除线程池多余线程
            if(tasks.empty()&&this->workers.size()>this->thread_num&&busy_thread_num<thread_num)
            {
                {   
                    thread_queue_mutex.lock();  // 锁住线程队列，清除队列中的多余线程
                    // 当任务队列为空，并且有空余线程数大于核心线程数的时候，并且繁忙的线程数小于最小线程数的时候，说明有多余未参与工作的线程，进入释放线程
                    // 从线程队列中移除当前线程
                    auto current_thread_id = this_thread::get_id();
                    cout<<"准备移除："<<current_thread_id<<endl;
                    for(auto it=workers.begin();it!=workers.end();it++){
                        if(it->get_id()==current_thread_id){
                            it->detach();           // 从主线程分离，准备退出线程，由系统进行回收
                            workers.erase(it);
                            cout<<"线程："<<std::this_thread::get_id()<<" 已移除"<<endl;
                            break;
                        }
                    }
                    thread_queue_mutex.unlock();   // 解锁
                }
                return;      // 执行return，退出该线程并释放线程资源
            }
            // 如果是线程池销毁条件，则退出该线程
            if (this->is_exit && this->tasks.empty())
            {
                return;
            }
            // 创建一个任务对象，接收任务并处理
            task = std::move(this->tasks.front());
            // 任务出队列
            this->tasks.pop();
        }
        // 在执行任务之前，增加线程繁忙线程的数量
        thread_busy_mutex.lock();
        busy_thread_num++;
        thread_busy_mutex.unlock();
        // 执行任务
        task.run();
        // 执行任务之后，将繁忙线程减一，此线程标记为不繁忙
        thread_busy_mutex.lock();
        busy_thread_num--;
        thread_busy_mutex.unlock();
        // 到此，该线程任务执行完毕，进入空闲状态
    }
}
// 函数指针的形式导入任务
// 向任务队列中添加任务 -- 这里只针对于
void ThreadPool::AddTask(Task task){
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if(is_exit){
            throw std::runtime_error("threadpool stopped");
        }
        // 任务入列
        tasks.emplace(task);
    }
    condition.notify_one(); // 唤醒一个线程执行任务
}
/*设置退出线程池*/
void ThreadPool::Exit(){
    this->is_exit = true;    // 主动退出，则释放强制释放所有线程
    cout<<"正在退出线程池,清理所有线程--------------------------"<<endl;
}

// 销毁线程池，释放线程资源
ThreadPool::~ThreadPool(){
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        is_exit = true;
    }
    condition.notify_all();   // 唤醒所有线程池进行销毁
    manage_tid.join();   // 关闭管理者线程
    for(std::thread &worker:this->workers){
        worker.join();                // 挂载在主线程上 ---资源回收
    }
}

/*测试函数*/
int main(){
    // 创建一个线程池，线程数为5
    ThreadPool tp(10);     // 仅设置核心线程数
    // ThreadPool tp(5,20);      // 设置线程上限
    // ThreadPool tp(5,100,20);  // 设置上限+每次递增递减线程数
    // 向线程池添加任务
    for(int i=0;i<1000;i++){
        Task task(i+1);
        tp.AddTask(task);
    }
    getchar();
    cout<<"----------------30秒后线程池退出------------------"<<endl;
    this_thread::sleep_for(30s);
    tp.Exit();               // 主动退出线程池
    return 0;
}

#endif