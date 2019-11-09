#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <vector>
#include <set>
#include <unordered_map>
#include <algorithm>
using namespace std;

// 类型声明
// 文件类型
enum FILE_TYPE
{
    NORMAL_FILE,
    DIR
};
// 颜色
enum COLOR
{
    COLOR_NULL = 0,
    COLOR_RED = 1
};
// 取消结构体对齐（重要，艹）
#pragma pack(push)
#pragma pack(1)
// 无符号char, 0-255
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
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
// 链表实现的文件树
struct FileNode
{
    FILE_TYPE type = NORMAL_FILE; // 结点类型
    string file_name;             // 当前文件名
    int child_dir_num = 0;        // 直接子文件夹数量
    int child_file_num = 0;       // 直接子文件数量
    int file_size = 0;            // 文件大小
    string data;                  //数据
    vector<FileNode *> children;  // 子节点
    FileNode(string file_name) : file_name(file_name) {}
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
        {"NOT_A_FILE", "Not a file."},
        {"BAD_CLUSTER", "Bad Cluster."},
    };
// 文件属性
enum FileAttributes
{
    NORMAL = 0x00,
    READ_ONLY = 0x01,
    HIDDEN = 0x02,
    SYSTEM = 0x04,
    VOLUME_ID = 0x08,
    DIRECTORY = 0x10,
    ARCHIVE = 0x20,
    LFN = READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID
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
    /**
     * 输出一个字符串
     * @param c 颜色
     * @param s 待输出字符串
     */
    void nout(COLOR c, const char *s);
}

/**
 * 分割字符串
 * @param src 待分割字符串
 * @param sep 字符串形式的分隔符
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
 * 判断字符串是否以另一个字符串结尾
 * @param str 源字符串
 * @param end 结尾
 * @return 是 / 否
 */
bool endswith(const string &str, const string &end)
{
    int srclen = str.size();
    int endlen = end.size();
    if (srclen >= endlen)
    {
        string temp = str.substr(srclen - endlen, endlen);
        if (temp == end)
            return true;
    }
    return false;
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
 * 去掉文件名字符串右边的空格
 * 之前的函数不生效是因为结束的时候可能有个0x10……服了
 * @param s 字符串
 */
void file_name_rtrim(string &s)
{
    // 如果有神秘的0x10，先去掉
    int length = s.length();
    if (s[length - 1] == 0x10)
    {
        s = s.substr(0, length - 1);
    }
    rtrim(s);
}

/**
 * 字符串转大写
 * @param s 待转换字符串
 */
void to_upper_ASCII(string &s)
{
    for (auto &c : s)
    {
        c = toupper(c);
    }
}

/**
 * 判断字符串是否全为大写
 * @param s 字符串
 * @return 是 / 否
 */
bool is_upper_case(const string &s)
{
    string temp = s;
    to_upper_ASCII(const_cast<string &>(s));
    return s == temp;
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
    // 就一个'.'
    if (!*s)
    {
        return true;
    }
    if (*s != '.' && *s != ' ')
    {
        return false;
    }
    ++s;
    // 这是在特定情况下的终止符
    while (*s && *s != 0x10)
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
        nextAddr &= static_cast<uint>(-1) >> (sizeof(uint) * 8 - FAT_ENTRY_SIZE);
    }
    // 奇数拿高位
    // 构造形如00000000111111111111000000000000的掩码，然后右移
    else
    {
        nextAddr &= (static_cast<uint>(-1) << (sizeof(uint) * 8 - FAT_ENTRY_SIZE)) >> (sizeof(uint) * 8 - FAT_ENTRY_SIZE * 2);
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
 * 读取FAT12镜像的文件
 * @param addr 根目录区地址
 * @param infile 文件流
 * @param parent 父文件
 */
void readDirEntry(int addr, ifstream &infile, FileNode *parent)
{
    DirEntry de = readDirEntryContent(addr, infile);
    // 为空，结束
    while (de.create_time != 0)
    {
        // 暂不考虑LFN
        if (de.attribute == LFN)
        {
            addr += DIR_ENTRY_SIZE;
            de = readDirEntryContent(addr, infile);
            continue;
        }
        // 目录，递归
        else if (de.attribute == DIRECTORY)
        {
            // 加进文件树，不需要考虑文件大小
            // 加入的时候把多余的空格去掉，看着膈应
            string name(de.file_name);
            file_name_rtrim(name);
            FileNode *node = new FileNode(name);
            // 显式声明类型为目录
            node->type = DIR;
            parent->children.push_back(node);
            // 如果是当前目录或上级目录，直接下一步
            // 这两个不计入子目录
            if (is_single_or_double_dot(de.file_name))
            {
                addr += DIR_ENTRY_SIZE;
                de = readDirEntryContent(addr, infile);
                continue;
            }
            // 父节点子目录数 + 1
            ++(parent->child_dir_num);
            // 相当于根目录表放到数据区了
            int dir_addr = DATA_SECTION_ADDR + de.cluster_num * header.BPB_BytsPerSec;
            readDirEntry(dir_addr, infile, node);
        }
        // 文件
        else
        {
            // 加进文件树
            // 加入的时候把多余的空格去掉，看着膈应
            string name(de.file_name);
            file_name_rtrim(name);
            vector<string> splitted_names = split_string(name, " ");
            // 加上类型名
            name = splitted_names[0] + "." + splitted_names[splitted_names.size() - 1];
            FileNode *node = new FileNode(name);
            node->file_size = de.file_size;
            // 父节点子文件数 + 1
            parent->children.push_back(node);
            ++(parent->child_file_num);
            // 父节点大小增加
            parent->file_size += de.file_size;
            int fat = readFAT(de.cluster_num, infile);
            // 最后一个
            if (fat >= LAST_CLUSTER)
            {
                node->data = readDataCluster(de.cluster_num, infile);
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
                node->data = data;
            }
        }
        // 下一项
        addr += DIR_ENTRY_SIZE;
        de = readDirEntryContent(addr, infile);
    }
}

/**
 * 查询路径对应的文件结点
 * TODO: 假设不会出现同名的文件和目录
 * @param root 文件树
 * @param path 文件路径
 * @return 找到返回目标结点，未找到返回nullptr
 */
FileNode *findNode(FileNode *root, const string &path)
{
    // 如果是根目录，直接返回
    if (path == "/")
    {
        return root;
    }
    vector<string> splitted_path = split_string(path, "/");
    int length = splitted_path.size();
    // 从根目录的下一级开始
    FileNode *spare = root;
    // 类似引用计数的机制来确保搜索到了真正的目标
    int count = 0;
    for (int i = 1; i < length; ++i)
    {
        for (auto node : root->children)
        {
            // 文件名相同
            if (node->file_name == splitted_path[i])
            {
                root = node;
                ++count;
                break;
            }
        }
    }
    // 没找到，或者没有真正找到
    return root == spare || count != length - 1 ? nullptr : root;
}

/**
 * 全部输出，但只输出文件名，对应于ls
 * @param root 文件树根结点
 * @param parent 父节点名称
 */
void printSummary(FileNode *root, const string &parent)
{
    // 单文件，不输出直接返回，因为父级已经处理过了
    if (root->type == NORMAL_FILE)
    {
        return;
    }
    // 目录
    else
    {
        // 当前目录和上级目录，不输出直接返回，因为父级已经处理过了
        if (is_single_or_double_dot((root->file_name).c_str()))
        {
            return;
        }
        // 其他目录输出，因为需要满足层级关系的格式
        else
        {
            nout(COLOR_NULL, (parent == "" ? "/" : parent.c_str()));
            nout(COLOR_NULL, (root->file_name == "/" ? "" : (root->file_name).c_str()));
            nout(COLOR_NULL, (root->file_name == "/" ? "" : "/"));
            nout(COLOR_NULL, ":");
            nout(COLOR_NULL, "\n");
        }
        // 每次先遍历输出每个目录里的内容
        for (auto node : root->children)
        {
            if (node->type == NORMAL_FILE)
            {
                nout(COLOR_NULL, (node->file_name).c_str());
                nout(COLOR_NULL, " ");
            }
            else
            {
                nout(COLOR_RED, (node->file_name).c_str());
                nout(COLOR_NULL, " ");
            }
        }
        nout(COLOR_NULL, "\n");
        // 然后对每个子目录进行递归
        for (auto node : root->children)
        {
            printSummary(node, parent + root->file_name + (parent == "" ? "" : "/"));
        }
    }
}

/**
 * 全部输出，对应于ls -l
 * @param root 文件树根结点
 * @param parent 父节点名称
 */
void printDetail(FileNode *root, const string &parent)
{
    // 单文件，不输出直接返回，因为父级已经处理过了
    if (root->type == NORMAL_FILE)
    {
        return;
    }
    // 目录
    else
    {
        // 当前目录和上级目录，不输出直接返回，因为父级已经处理过了
        if (is_single_or_double_dot((root->file_name).c_str()))
        {
            return;
        }
        // 其他目录输出，因为需要满足层级关系的格式
        else
        {
            nout(COLOR_NULL, (parent == "" ? "/" : parent.c_str()));
            nout(COLOR_NULL, (root->file_name == "/" ? "" : (root->file_name).c_str()));
            nout(COLOR_NULL, (root->file_name == "/" ? "" : "/"));
            nout(COLOR_NULL, " ");
            nout(COLOR_NULL, to_string(root->child_dir_num).c_str());
            nout(COLOR_NULL, " ");
            nout(COLOR_NULL, to_string(root->child_file_num).c_str());
            nout(COLOR_NULL, ":");
            nout(COLOR_NULL, "\n");
        }
        // 每次先遍历输出每个目录里的内容
        for (auto node : root->children)
        {
            if (node->type == NORMAL_FILE)
            {
                nout(COLOR_NULL, (node->file_name).c_str());
                nout(COLOR_NULL, " ");
                nout(COLOR_NULL, to_string(node->file_size).c_str());
                nout(COLOR_NULL, "\n");
            }
            else
            {
                if (is_single_or_double_dot(node->file_name.c_str()))
                {
                    nout(COLOR_RED, (node->file_name).c_str());
                    nout(COLOR_NULL, "\n");
                }
                else
                {
                    nout(COLOR_RED, (node->file_name).c_str());
                    nout(COLOR_NULL, " ");
                    nout(COLOR_NULL, to_string(node->child_dir_num).c_str());
                    nout(COLOR_NULL, " ");
                    nout(COLOR_NULL, to_string(node->child_file_num).c_str());
                    nout(COLOR_NULL, "\n");
                }
            }
        }
        nout(COLOR_NULL, "\n");
        // 然后对每个子目录进行递归
        for (auto node : root->children)
        {
            printDetail(node, parent + root->file_name + (parent == "" ? "" : "/"));
        }
    }
}

/**
 * 输出文件内容，对应于cat
 * @param node 需要输出的文件结点
 */
void printContent(FileNode *node)
{
    if (node->type == NORMAL_FILE)
    {
        cout << node->data << endl;
    }
    else
    {
        cout << ERR_MSG["NOT_A_FILE"] << endl;
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

    // 新建根目录，显式指定为目录类型
    FileNode *root = new FileNode("/");
    root->type = DIR;
    // 构建文件树
    readDirEntry(DIR_SECTION_ADDR, infile, root);

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
            // 此时输入要么为空，要么只剩参数和文件路径
            string param;
            string filename;
            // 只有一个命令
            if (splitted_input.size() == 0)
            {
                filename = "/";
            }
            // 可能只有参数或命令
            else if (splitted_input.size() == 1)
            {
                // 假设开头是-的都是参数
                if (splitted_input[0][0] == '-')
                {
                    param = splitted_input[0];
                    filename = "/";
                }
                // 是文件名
                else
                {
                    filename = splitted_input[0];
                }
            }
            // 既有参数又有命令
            else
            {
                param = splitted_input[0];
                filename = splitted_input[1];
            }
            // 只支持大写
            if (!is_upper_case(filename))
            {
                cout << ERR_MSG["FILE_NOT_FOUND"] << endl;
                continue;
            }
            // 如果没有以/开头，补上
            if (filename[0] != '/')
            {
                filename = "/" + filename;
            }
            // 相当于当前目录
            if (endswith(filename, "/."))
            {
                filename = filename.substr(0, filename.length() - 2);
            }
            // 相当于上级目录
            else if (endswith(filename, "/.."))
            {
                filename = filename.substr(0, filename.length() - 3);
                vector<string> splitted_filename = split_string(filename, "/");
                int parent_dirname_length = splitted_filename[splitted_filename.size() - 1].length();
                filename = filename.substr(0, filename.length() - parent_dirname_length);
            }
            FileNode *res = findNode(root, filename);
            // 找不到文件
            if (!res)
            {
                cout << ERR_MSG["FILE_NOT_FOUND"] << endl;
                continue;
            }
            vector<string> splitted_name = split_string(filename, "/");
            string parent;
            int length = splitted_name.size();
            for (int i = 0; i < length - 1; ++i)
            {
                parent += splitted_name[i] + "/";
            }
            // 只要有参数，就可以确认是-l，因为其他参数不合法
            if (param != "")
            {
                printDetail(res, parent);
            }
            // 无参数
            else
            {
                printSummary(res, parent);
            }
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
            string filename = splitted_input[0];
            // 只支持大写
            if (!is_upper_case(filename))
            {
                cout << ERR_MSG["FILE_NOT_FOUND"] << endl;
                continue;
            }
            // 如果没有以/开头，补上
            if (filename[0] != '/')
            {
                filename = "/" + filename;
            }
            FileNode *res = findNode(root, filename);
            // 找不到文件
            if (!res)
            {
                cout << ERR_MSG["FILE_NOT_FOUND"] << endl;
                continue;
            }
            printContent(res);
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