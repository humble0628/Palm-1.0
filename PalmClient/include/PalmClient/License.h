#ifndef _LICENSE_H
#define _LICENSE_H

#include <iostream>
#include <string>

#include "AESCoder.h"

/*
类名：CLicense
入口：License文件
出口：激活标志位
*/
class CLicense
{
private:
    std::string license_path;
    enum LicenseStatus{};
public:
    CLicense();
};


#endif