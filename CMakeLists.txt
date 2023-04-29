cmake_minimum_required(VERSION 3.22) # 指明cmake版本
PROJECT(THREADPOOL)

SET(SRC_LIST ThreadPool.cpp)

MESSAGE("开始构建...........")

ADD_EXECUTABLE(ThreadPool ${SRC_LIST})

