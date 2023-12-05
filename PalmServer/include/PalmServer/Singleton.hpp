#ifndef _SINGLETON_HPP
#define _SINGLETON_HPP

#include <iostream>
#include <mutex>
#include <thread>
#include <memory>

template <class T>
class Singleton
{
public:
    static std::shared_ptr<T> getInstance() {
        std::call_once(_flag, [](){
            _instance.reset(new T);
        });
        return _instance;
    }
protected:
    Singleton() = default;
    ~Singleton() = default;
private:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
    static std::shared_ptr<T> _instance;
    static std::once_flag _flag;
};

template <class T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
template <class T>
std::once_flag Singleton<T>::_flag;

#endif