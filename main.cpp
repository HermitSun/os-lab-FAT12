#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
using namespace std;

// 错误信息
unordered_map<string, string> ERR_MSG{
    {"EMPTY_INPUT", "Please enter command."},
    {"INVALID_COMMAND", "Invalid command."},
    {"INVALID_PARAM", "Invalid param."},
    {"FILE_NOT_FOUND", "File not found."},
    {"AMBIGUOUS_FILE", "Please specify file path."},
    {"MORE_THAN_ONE_FILE", "More than one file path."},
};
// 退出
const string BYE = "Bye!";

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
            bool non_param = s.size() == 1;
            bool is_not_l = !is_all_l(s.substr(1));
            if (non_param || is_not_l)
            {
                cout << ERR_MSG["INVALID_PARAM"] << endl;
                return false;
            }
            // 超过一个l就当成只有一个-l塞进去
            else
            {
                non_duplicate.insert("-l");
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
        cout << ERR_MSG["MORE_THAN_ONE_FILE"] << endl;
        return false;
    }
    vec.assign(non_duplicate.begin(), non_duplicate.end());
    return true;
}

int main()
{
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
            cout << ERR_MSG["EMPTY_INPUT"] << endl;
            continue;
        }
        // 分割输入内容
        vector<string> splitted_input = split_string(input, " ");
        // 获取命令，然后从输入中移除命令
        string command = splitted_input[0];
        splitted_input.erase(splitted_input.begin());
        for (auto &s : splitted_input)
        {
            cout << s << endl;
        }
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
            string filename;
            // 不指定路径则默认根目录
            if (splitted_input.size() == 1)
            {
                filename = "/";
            }
            else
            {
                filename = splitted_input[1];
            }
            cout << param << " " << filename << endl;
        }
        // cat命令
        else if (command == "cat")
        {
            int file_num = splitted_input.size();
            // 未指定路径
            if (file_num == 0)
            {
                cout << ERR_MSG["AMBIGUOUS_FILE"] << endl;
                continue;
            }
            // 路径超过一个
            else if (file_num > 1)
            {
                cout << ERR_MSG["MORE_THAN_ONE_FILE"] << endl;
                continue;
            }
            // TODO: 待输出文件路径
            string filename = splitted_input[0];
            cout << filename << endl;
        }
        // 命令不存在
        else
        {
            cout << ERR_MSG["INVALID_COMMAND"] << endl;
            continue;
        }
    }
    return 0;
}