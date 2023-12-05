#include <iostream>
#include <typeinfo>
#include <memory>

#include "Palm.h"

bool inputCheck(int& input)
{
    std::set<int> valid_inputs = {0, 1, 2, 3};
    if (valid_inputs.count(input) > 0) return true;
    return false;
}

void displayImageThread(CPalm& palm)
{
    cv::Mat frame;
    cv::namedWindow("EasyPalm", cv::WINDOW_NORMAL);
    cv::resizeWindow("EasyPalm", 480, 640);
    while (true)
    {
        frame = palm.getFrameWithPoints();
        cv::imshow("EasyPalm", frame);
        int key = cv::waitKey(10);
        if (key == 27)
        {
            cv::destroyWindow("EasyPalm");
            break;
        }
    }
}

int main()
{
    enum Userinput{Register, Recognize, Database, Quit};
    boost::asio::io_context ioc;
    std::unique_ptr<CPalm> palm(new CPalm(ioc));
    int input = -1;

    // std::ref确保多线程共用同一个对象，保证线程安全
    std::thread displayThread(displayImageThread, std::ref(*palm));

    do
    {
        std::cout << "\n选择模式：[0]注册 [1]识别 [2]查看数据库 [3]退出" << std::endl;
        while(true)
        {
            std::cout << ">>";
            if (std::cin >> input)
            {
                if (inputCheck(input)) break;
            }
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\n模式选择无效，请重新输入" << std::endl;
        }
        Userinput user_input = static_cast<Userinput>(input);
        switch (user_input)
        {
            case Userinput::Register:
                palm->userRegister();
                break;
            case Userinput::Recognize:
                palm->userRecognize();
                break;
            case Userinput::Database:
                palm->userDatabase();
                break;
            case Userinput::Quit:
                palm->userQuit();
                break;
            default:
                break;
        }
    } while (true);
    
    // 等待支线程执行完毕
    displayThread.join();

    return 0;
}