#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
using namespace std;

// 类型声明
// 取消结构体对齐（重要，艹）
#pragma pack(push)
#pragma pack(1)
// 无符号char, 0-255
typedef unsigned char uchar;
// FAT12引导扇区
struct FAT12Header
{
    char BS_jmpBoot[3];     // 跳转指令
    char BS_OEMName[8];     // 厂商
    ushort BPB_BytsPerSec;  // 每扇区字节数
    uchar BPB_SecPerClus;   // 每簇占用的扇区数
    ushort BPB_RsvdSecCnt;  // Boot占用的扇区数
    uchar BPB_NumFATs;      // FAT表的记录数
    ushort BPB_RootEntCnt;  // 最大根目录文件数
    ushort BPB_TotSec16;    // 每个FAT占用扇区数
    uchar BPB_Media;        // 媒体描述符
    ushort BPB_FATSz16;     // 每个FAT占用扇区数
    ushort BPB_SecPerTrk;   // 每个磁道扇区数
    ushort BPB_NumHeads;    // 磁头数
    uint BPB_HiddSec;       // 隐藏扇区数
    uint BPB_TotSec32;      // 如果BPB_TotSec16是0，则在这里记录
    uchar BS_DrvNum;        // 中断13的驱动器号
    uchar BS_Reserved1;     // 未使用
    uchar BS_BootSig;       // 扩展引导标志
    uint BS_VolID;          // 卷序列号
    char BS_VolLab[11];     // 卷标，必须是11个字符，不足以空格填充
    char BS_FileSysType[8]; // 文件系统类型，必须是8个字符，不足填充空格
};
// 根目录项
struct DirEntry
{
    char file_name[11];            // 文件名
    uchar attribute;               // 文件属性
    uchar reserve_for_NT;          // 给Windows NT准备的，基本可以忽略
    uchar create_second;           // 创建时间（单位10ms或5ms，取决于系统）
    ushort create_time;            // 创建时间（时5b分6b秒5b）
    ushort create_date;            // 创建日期（年7b月4b日5b）
    ushort last_access_time;       // 最后访问日期（年7b月4b日5b）
    ushort always_zero;            // 在FAT12中一直是0，忽略
    ushort last_modification_time; // 最后修改时间（时5b分6b秒5b）
    ushort last_modification_date; // 最后修改日期（年7b月4b日5b）
    ushort cluster_num;            // 数据簇号
    uint file_size;                // 文件大小
};
// 恢复对齐
#pragma pack(pop)

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

/**
 * 读取FAT12镜像的引导扇区（主要是BPB）并打印
 * @param rf 用于存储读取到的header的结构体
 * @param filepath 镜像路径
 */
void readFAT12Header(FAT12Header &rf, const string &filepath)
{
    // 读取
    ifstream infile(filepath, ios::in | ios::binary);
    infile.read(reinterpret_cast<char *>(&rf), sizeof(rf));
    infile.close();
    // 确保字符串结束
    rf.BS_VolLab[10] = 0;
    rf.BS_FileSysType[7] = 0;
    // 输出
    cout << "BS_OEMName: " << rf.BS_OEMName << endl;
    cout << "BPB_BytsPerSec: " << hex << rf.BPB_BytsPerSec << endl;
    cout << "BPB_SecPerClus: " << hex << (int)rf.BPB_SecPerClus << endl;
    cout << "BPB_RsvdSecCnt: " << hex << rf.BPB_RsvdSecCnt << endl;
    cout << "BPB_NumFATs: " << hex << (int)rf.BPB_NumFATs << endl;
    cout << "BPB_RootEntCnt: " << hex << rf.BPB_RootEntCnt << endl;
    cout << "BPB_TotSec16: " << hex << rf.BPB_TotSec16 << endl;
    cout << "BPB_Media: " << hex << (int)rf.BPB_Media << endl;
    cout << "BPB_FATSz16: " << hex << rf.BPB_FATSz16 << endl;
    cout << "BPB_SecPerTrk: " << hex << rf.BPB_SecPerTrk << endl;
    cout << "BPB_NumHeads: " << hex << rf.BPB_NumHeads << endl;
    cout << "BPB_HiddSec: " << hex << rf.BPB_HiddSec << endl;
    cout << "BPB_TotSec32: " << hex << rf.BPB_TotSec32 << endl;
    cout << "BS_DrvNum: " << hex << (int)rf.BS_DrvNum << endl;
    cout << "BS_Reserved1: " << hex << (int)rf.BS_Reserved1 << endl;
    cout << "BS_BootSig: " << hex << (int)rf.BS_BootSig << endl;
    cout << "BS_VolID: " << hex << rf.BS_VolID << endl;
    cout << "BS_VolLab: " << rf.BS_VolLab << endl;
    cout << "BS_FileSysType: " << rf.BS_FileSysType << endl;
}

/**
 * 读取FAT12镜像的根目录区并打印
 * @param root 用于存储读取到的根目录区的结构体
 * @param addr 根目录区地址
 * @param filepath 镜像路径
 */
void readDirEntry(vector<DirEntry> &root, int addr, const string &filepath)
{
    DirEntry de;
    // 读取
    ifstream infile(filepath, ios::in | ios::binary);
    infile.seekg(addr, ios::beg);
    infile.read(reinterpret_cast<char *>(&de), sizeof(de));
    infile.close();
}

int main()
{
    FAT12Header header;
    DirEntry root;
    readFAT12Header(header, "x.img");
    double dir_sections = header.BPB_RootEntCnt * 32 / header.BPB_BytsPerSec;
    // 不能除尽
    if (dir_sections != static_cast<int>(dir_sections))
    {
        dir_sections = static_cast<int>(dir_sections) + 1;
    }
    // 根目录区始地址
    int dir_section_addr = (1 + 9 * 2) * 512;
    cout << hex << dir_section_addr << endl;
    // 数据区始地址
    int data_section_addr = (1 + 9 * 2 + dir_sections) * 512;
    cout << hex << data_section_addr << endl;
    

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