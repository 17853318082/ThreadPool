# 动态线程池
描述：该线程池为一个动态线程池，实现了以下几点
    （1）对线程进行任务分配执行
    （2）线程个数自增长，与自减少，条件代码中已给出，可自行更改
    （3）创建任务类，使用函数指针实现任务调用
其他：代码中给出了详细的注释，如果有错误或bug可私信我，多谢帮忙修改

# 构建与运行
1. 创建 build 文件夹
2. cd build     
3. ../cmake
4. make
5. ./ThreadPool