#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>
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
} header;
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
unordered_map<string, string>
    ERR_MSG{
        {"EMPTY_INPUT", "Please enter command."},
        {"INVALID_COMMAND", "Invalid command."},
        {"INVALID_PARAM", "Invalid param."},
        {"FILE_NOT_FOUND", "File not found."},
        {"AMBIGUOUS_FILE", "Please specify file path."},
        {"MORE_THAN_ONE_FILE", "More than one file path."},
        {"BAD_CLUSTER", "Bad Cluster."},
    };
// 文件属性
enum FileAttributes
{
    READ_ONLY = 0x01,
    HIDDEN = 0x02,
    SYSTEM = 0x04,
    VOLUME_ID = 0x08,
    DIRECTORY = 0x10,
    ARCHIVE = 0x20,
    LFN = READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID,
    EMPTY = 0x00
};
// 退出
const string BYE = "Bye!";
// 根目录项大小，32B
const int DIR_ENTRY_SIZE = 32;
// 最后一个数据簇
const int LAST_CLUSTER = 0xFF8;
// 坏簇
const int BAD_CLUSTER = 0xFF7;
// FAT项大小，12B
const int FAT_ENTRY_SIZE = 12;
// 始地址
const int FAT_ADDR = 0x200;
// 这里是默认值，后面还会进行计算
// 根目录区
int DIR_SECTION_ADDR = 0x2600;
// 数据区
int DATA_SECTION_ADDR = 0x4200;

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
 * 去掉字符串右边的空格
 * @param s 字符串
 */
static inline void rtrim(string &s)
{
    s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
}

/**
 * 判断字符串是否是"."或".."
 * 强耦合的方法，不具有移植性
 * @param src 待判断字符串
 * @return 是 / 否
 */
bool is_single_or_double_dot(const char *s)
{
    if (*s != '.')
    {
        return false;
    }
    ++s;
    if (*s != '.' && *s != ' ')
    {
        return false;
    }
    ++s;
    // 这是在特定情况下的终止符
    while (*s != 0x10)
    {
        if (*s != ' ')
        {
            return false;
        }
        ++s;
    }
    return true;
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
 * @param infile 文件流
 */
void readFAT12Header(FAT12Header &rf, ifstream &infile)
{
    // 读取
    infile.read(reinterpret_cast<char *>(&rf), sizeof(rf));
    // 确保字符串结束
    rf.BS_VolLab[10] = '\0';
    rf.BS_FileSysType[7] = '\0';
}

/**
 * 读取FAT12表项内容
 * @param num 第几项
 * @param infile 文件流
 * @return FAT表项内容
 */
int readFAT(int num, ifstream &infile)
{
    uint nextAddr = 0;
    infile.seekg(FAT_ADDR + num / 2 * 3, ios_base::beg);
    infile.read(reinterpret_cast<char *>(&nextAddr), FAT_ENTRY_SIZE * 2 / 8);
    // 偶数拿低位
    // 构造形如00000000000000000000111111111111的掩码
    if (num % 2 == 0)
    {
        nextAddr &= uint(-1) >> (sizeof(uint) * 8 - FAT_ENTRY_SIZE);
    }
    // 奇数拿高位
    // 构造形如00000000111111111111000000000000的掩码，然后右移
    else
    {
        nextAddr &= (uint(-1) << (sizeof(uint) * 8 - FAT_ENTRY_SIZE)) >> (sizeof(uint) * 8 - FAT_ENTRY_SIZE * 2);
        nextAddr = nextAddr >> FAT_ENTRY_SIZE;
    }
    return nextAddr;
}

/**
 * 读取数据簇内容（也就是一个扇区了）
 * @param num 第几项
 * @param infile 文件流
 * @return 数据
 */
string readDataCluster(int num, ifstream &infile)
{
    char buf[512];
    int data_addr = DATA_SECTION_ADDR + num * header.BPB_BytsPerSec;
    infile.seekg(data_addr, ios_base::beg);
    infile.read(reinterpret_cast<char *>(&buf), header.BPB_BytsPerSec);
    buf[512] = '\0';
    string data(buf);
    rtrim(data);
    return data;
}

/**
 * 读取根目录项内容
 * @param addr 地址
 * @param infile 文件流
 * @return 根目录项内容
 */
DirEntry readDirEntryContent(int addr, ifstream &infile)
{
    DirEntry de;
    infile.seekg(addr, ios_base::beg);
    infile.read(reinterpret_cast<char *>(&de), sizeof(de));
    return de;
}

/**
 * 读取FAT12镜像的根目录区并打印
 * @param addr 根目录区地址
 * @param infile 文件流
 */
void readDirEntry(int addr, ifstream &infile)
{
    DirEntry de = readDirEntryContent(addr, infile);
    // 为空，结束
    while (de.attribute != EMPTY)
    {
        // TODO: LFN，暂不考虑
        if (de.attribute == LFN)
        {
            addr += DIR_ENTRY_SIZE;
            de = readDirEntryContent(addr, infile);
            continue;
        }
        // 目录，递归
        else if (de.attribute == DIRECTORY)
        {
            cout << de.file_name << endl;
            // 如果是当前目录或上级目录，直接下一步
            if (is_single_or_double_dot(de.file_name))
            {
                addr += DIR_ENTRY_SIZE;
                de = readDirEntryContent(addr, infile);
                continue;
            }
            // 相当于根目录表放到数据区了
            int dir_addr = DATA_SECTION_ADDR + de.cluster_num * header.BPB_BytsPerSec;
            readDirEntry(dir_addr, infile);
        }
        // 文件
        else
        {
            cout << de.file_name << endl;
            int fat = readFAT(de.cluster_num, infile);
            // 最后一个
            if (fat >= LAST_CLUSTER)
            {
                cout << readDataCluster(de.cluster_num, infile) << endl;
            }
            // 坏簇
            else if (fat == BAD_CLUSTER)
            {
                cout << ERR_MSG["BAD_CLUSTER"] << endl;
                return;
            }
            // 超过一个扇区
            else
            {
                string data = readDataCluster(de.cluster_num, infile);
                // 没结束
                while (fat < LAST_CLUSTER)
                {
                    // 并且没有坏
                    if (fat == BAD_CLUSTER)
                    {
                        cout << ERR_MSG["BAD_CLUSTER"] << endl;
                        return;
                    }
                    data += readDataCluster(fat, infile);
                    fat = readFAT(fat, infile);
                }
                cout << data << endl;
            }
        }
        // 下一项
        addr += DIR_ENTRY_SIZE;
        de = readDirEntryContent(addr, infile);
    }
}

int main()
{
    ifstream infile("a.img", ios::in | ios::binary);
    // 读取FAT12引导扇区
    readFAT12Header(header, infile);

    // 计算根目录区和数据区的始地址
    double dir_sections = header.BPB_RootEntCnt * 32 / header.BPB_BytsPerSec;
    // 不能除尽
    if (dir_sections != static_cast<int>(dir_sections))
    {
        dir_sections = static_cast<int>(dir_sections) + 1;
    }
    // 根目录区始地址
    DIR_SECTION_ADDR = (1 + 9 * 2) * 512;
    // 数据区始地址（-2是为了计算方便）
    DATA_SECTION_ADDR = (1 + 9 * 2 + dir_sections - 2) * 512;

    // vector<vector<DirEntry>> root;
    readDirEntry(DIR_SECTION_ADDR, infile);

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
    // 关闭文件流
    infile.close();
    return 0;
}