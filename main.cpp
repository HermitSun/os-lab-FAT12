#include <iostream>
#include <string>
#include <vector>
#include <set>
using namespace std;

// 函数声明
extern "C"
{
    int maxofthree(int, int, int);
}

/**
 * 分割字符串的函数
 * @param src 待分割字符串
 * @param sep 分隔符（可以是字符串）
 * @return 分割后的字符串组成的vector
 */
vector<string> split_string(const string &src, const string &sep)
{
    vector<string> v;
    string::size_type pos1, pos2;
    pos2 = src.find(sep);
    pos1 = 0;
    while (string::npos != pos2)
    {
        v.push_back(src.substr(pos1, pos2 - pos1));
        pos1 = pos2 + sep.size();
        pos2 = src.find(sep, pos1);
    }
    if (pos1 != src.length())
    {
        v.push_back(src.substr(pos1));
    }
    return v;
}

/**
 * 判断字符串是否全是小写l
 * @param src 待判断字符串
 * @return 是 / 否
 */
bool is_all_l(const string &src)
{
    int length = src.length();
    // 空串显然为false
    if (length == 0)
    {
        return false;
    }
    bool res = true;
    for (int i = 0; i < length; ++i)
    {
        if (src[i] != 'l')
        {
            res = false;
            break;
        }
    }
    return res;
}

/**
 * 去重，连续的多个l视作一个l，顺便判断格式是否正确
 * @param vec 待处理的vector<string>
 * @return 是 / 否是正确的命令
 */
bool remove_duplicate(vector<string> &vec)
{
    set<string> non_duplicate;
    // 是否有多个文件路径
    int count = 0;
    for (auto &s : vec)
    {
        // 只有一个l
        if (s == "-l")
        {
            non_duplicate.insert(s);
        }
        // 属于参数
        else if (s[0] == '-')
        {
            // 错误参数
            if (s.size() == 1 ||
                !is_all_l(s.substr(1)))
            {
                cout << "Invalid param." << endl;
                return false;
            }
            // 超过一个l当做不存在
            else
            {
                continue;
            }
        }
        // 属于路径名
        // 注意：输入多个重复路径也视为错误
        else
        {
            non_duplicate.insert(s);
            ++count;
        }
    }
    // 超过一个路径
    if (count > 1)
    {
        cout << "More than one file path." << endl;
        return false;
    }
    vec.assign(non_duplicate.begin(), non_duplicate.end());
    return true;
}

int main()
{
    // cout << maxofthree(1, -4, -7) << endl;
    // cout << maxofthree(2, -6, 1) << endl;
    // cout << maxofthree(2, 3, 1) << endl;
    // cout << maxofthree(-2, 4, 3) << endl;
    // cout << maxofthree(2, -6, 5) << endl;
    // cout << maxofthree(2, 4, 6) << endl;
    string input;
    while (getline(cin, input))
    {
        // 退出
        if (input == "exit")
        {
            cout << "Bye!" << endl;
            break;
        }
        // 输入为空，重试
        if (input == "")
        {
            cout << "Please enter command." << endl;
            continue;
        }
        // 分割输入内容
        vector<string> splitted_input = split_string(input, " ");
        // 获取命令
        string command = splitted_input[0];
        splitted_input.erase(splitted_input.begin());
        // ls命令
        if (command == "ls")
        {
            // 去重，连续的多个l视作一个l
            bool is_valid = remove_duplicate(splitted_input);
            // 命令错误
            if (!is_valid)
            {
                continue;
            }
            // 此时应该可以确认输入里只剩参数和文件路径了
            // TODO: 进行ls处理
            string param = splitted_input[0];
            string filename = splitted_input[1];
        }
        // cat命令
        else if (command == "cat")
        {
            // 未指定文件路径
            if (splitted_input.size() == 1)
            {
                cout << "Please specify file path." << endl;
                continue;
            }
            // TODO: 待输出文件路径
            string filename = splitted_input[1];
            cout << filename << endl;
        }
        // 命令不存在
        else
        {
            cout << "Invalid command." << endl;
            continue;
        }
    }
    return 0;
}