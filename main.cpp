#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cmath>     // 包含 ceil 函数
#include <algorithm> // 包含 std::max 函数
#include <deque>     // 包含 std::deque
#include <vector>
#include <queue>     // 包含 std::priority_queue
#include <stdexcept> // 包含runtime_error
#include <cstdlib>   // 用于rand()
#include <ctime>     // 用于time()
#include <iostream>  // 用于cin和cout
#include <stack>

using namespace std;

#define MAX_DISK_NUM (10 + 5)          // 磁盘数
#define MAX_DISK_SIZE (16384 + 5)      // 磁盘大小
#define MAX_REQUEST_NUM (30000000 + 5) // 请求数
#define MAX_OBJECT_NUM (100000 + 5)    // 对象数
#define REP_NUM (3)                    // 副本数
#define FRE_PER_SLICING (1800)         // 每个切片的频率
#define EXTRA_TIME (105)               // 额外时间
const int OBJECT_BLOCKS = 5;

#define MAX_TAG (16 + 5) // 标签数量上限（tag编号 1~16）
int tag_disk_counter[MAX_TAG][MAX_DISK_NUM] = {0};

#define PASS 1
#define READ 2
#define JUMP 3
#define STOP 4

typedef struct Request_
{
    int object_id;                           // 请求的对象 ID
    bool object_block_id[OBJECT_BLOCKS + 1]; // 对象块读取状态，`true` 表示已读取
    bool is_done;                            // 请求是否完成
} Request;

// TODO:实现标签tag的管理
// 对象结构体
typedef struct Object_
{
    int replica[REP_NUM + 5];             // 副本,存储该副本在哪个磁盘,replica[j]表示副本j存储在哪个磁盘
    bool is_used_disk[MAX_DISK_NUM] = {}; // 已经有哪些磁盘用于存储该对象了，从1开始奥
    int *unit[REP_NUM + 5];               // 对象块,存储该块在磁盘的位置,unit[j][k]表示副本j的第k个对象块存储的磁盘单元位置
    std::deque<int> request_list;         // 当前对象的读取请求,记录每个对象块的待读取次数,read_count[i]表示第i个request的id
    int size;                             // 大小
    int tag;                              // 标签
    bool is_delete;                       // 是否删除
} Object;

// 请求数组,request[request_id]表示请求request_id
Request request[MAX_REQUEST_NUM];
// 对象数组
Object object[MAX_OBJECT_NUM];

typedef struct Disk_
{
    int object_id;
    int pending_requests; // 当前磁盘单元的待读取请求数
} Disk;

Disk disk[MAX_DISK_NUM][MAX_DISK_SIZE] = {0}; // 磁盘,disk[i][j]表示磁盘i的第j个存储单元存储的对象id
// T：代表本次数据有𝑇 + 105个时间片，后续输入第二阶段将循环交互𝑇 + 105次。 时间片编号为1 ~𝑇 + 105。输入数据保证1≤𝑇≤86400。对于第𝑇 + 1 ~𝑇 + 105个时间分片，输入数据 保证没有删除、写入和读取请求。
// M：代表对象标签数。对象标签编号为1 ~𝑀。输入数据保证1≤𝑀≤16。
// N：代表存储系统中硬盘的个数，硬盘编号为1 ~𝑁。输入数据保证3≤𝑁 ≤10。
// V：代表存储系统中每个硬盘的存储单元个数。存储单元编号为1 ~𝑉。输入数据保证1≤𝑉≤ 16384，任何时间存储系统中空余的存储单元数占总存储单元数的至少10 %。
// G：代表每个磁头每个时间片最多消耗的令牌数。输入数据保证64≤𝐺 ≤1000。
int T, M, N, V, G;

int current_timestamp = 0;                               // 全局变量，记录当前时间片编号
int disk_point[MAX_DISK_NUM];                            // 磁头,disk_point[i]表示磁盘i的磁头指向的存储单元位置
int disk_pre_move[MAX_DISK_NUM];                         // 磁头,disk_pre_move[i]表示磁盘i的磁头上一次动作
int disk_pre_token[MAX_DISK_NUM];                        // 磁头,disk_pre_token[i]表示磁盘i的磁头上一次动作消耗的令牌数
int tag_disk_request_count[MAX_TAG][MAX_DISK_NUM] = {0}; // 磁盘-标签-请求计数,tag_disk_request_count[tag][disk_id]表示标签tag在磁盘disk_id上的请求计数

int tag_disk_map[MAX_TAG]; // 记录每个标签优先存储的磁盘

// 对磁盘空间的维护
//  int space_per_tag = M / N;  // 每个标签分配一个固定的空间，M是标签数量,N是磁盘数量
int current_occupation[MAX_DISK_NUM] = {0};      // 每个磁盘中存放的对象的大小之和（占用的单元块数量）
int tag_disk_usage[MAX_DISK_NUM][MAX_TAG] = {0}; // 记录每个磁盘中，单个个标签的已使用空间
int tag_disk_gap[MAX_DISK_NUM][MAX_TAG] = {0};   // 记录每个磁盘单个标签的剩余，也就是说还有多少空间可以写入
int tail_empty[MAX_DISK_NUM] = {0};              // 记录每块磁盘尾部连续空闲长度
int tail_start[MAX_DISK_NUM] = {0};              // 记录每块磁盘尾部连续空闲的起始位置
int disk_available[MAX_DISK_NUM];

// 存储每个标签的磁盘信息
struct tag_store_disk
{
    int disk_unit[MAX_DISK_NUM]; // 考虑到每个磁盘都可能存储该标签//!!!且不是按照磁盘编号顺序存储，是按照先后顺序
    // TODO:刚开始时已经均分好位置
    int disk_start[MAX_DISK_NUM];
};

tag_store_disk tag_disk_info[MAX_TAG] = {}; // 每个tag存储在哪些磁盘上//预存储
bool tag_disk_have_stored[MAX_TAG][MAX_DISK_NUM] = {}; //记录每个tag在哪些磁盘上有存放//不考虑删除带来的tag消失情况，只是一味地开空间

// 用于记录磁盘上某个标签段的信息
struct DiskTagSegment
{
    int tag_id;           // 该段标签编号
    int start_index;      // 该段的起始位置
    int usage_end_index;  // 该段已使用到的位置//!!!没有考虑删除导致的回缩，可以优化
    int gap;              // 该段已使用位置到下一段起始位置的距离
    int available;        // 该段还剩总位置//!!!目前来说还没有什么用
    DiskTagSegment *prev; // 前一段
    DiskTagSegment *next; // 后一段

    DiskTagSegment(int tag, int start, int end, int gap)
            : tag_id(tag), start_index(start), usage_end_index(end),
            prev(nullptr), next(nullptr), gap(gap) {}
}; //!!!要记得更新gap和available等动态值哦

// 定义一个二维数组，存储每个磁盘的标签信息,该磁盘要放入哪个标签就对数组的哪个位置进行DiskTagInfo的赋值
// 对于每个磁盘，维护一个双向循环链表的头指针
DiskTagSegment *disk_head[MAX_DISK_NUM] = {};
// 当前每个磁盘已分配的标签数量
int disk_seg_tag_count[MAX_DISK_NUM] = {0};

// 时间片
void timestamp_action()
{
    // 时间片
    int timestamp;
    // 读取时间片
    scanf("%*s%d", &timestamp);
    // 更新当前时间片编号
    current_timestamp = timestamp;
    // 打印时间片
    printf("TIMESTAMP %d\n", timestamp);

    // 刷新缓冲区
    fflush(stdout);
}

// 删除对象
// object_unit:传入对象的第j个副本在磁盘中存储位置的数组
// disk_unit:磁盘指针
// size:大小
void do_object_delete(const int *object_unit, Disk *disk_unit, int size)
{
    for (int i = 1; i <= size; i++)
    {
        disk_unit[object_unit[i]].object_id = 0;
        disk_unit[object_unit[i]].pending_requests = 0;
    }
}

//: 删除操作
void delete_action()
{
    int n_delete;                   // 删除数
    static int _id[MAX_OBJECT_NUM]; // 对象id

    // 读取删除数
    scanf("%d", &n_delete);
    // 读取删除对象id
    for (int i = 1; i <= n_delete; i++)
    {
        scanf("%d", &_id[i]);
    }

    // 取消读取数
    int unfinished_count = 0; // 取消读取数
    for (int i = 1; i <= n_delete; i++)
    {
        // 遍历对象的请求队列
        for (int request_id : object[_id[i]].request_list)
        {
            if (!request[request_id].is_done)
            {
                unfinished_count++; // 统计未完成的请求
            }
        }
    }
    // 打印取消读取数
    printf("%d\n", unfinished_count);
    for (int i = 1; i <= n_delete; i++)
    {
        int id = _id[i];
        // 输出取消读取请求
        while (object[id].request_list.size() > 0)
        {
            int current_id = object[id].request_list.front();
            if (request[current_id].is_done == false)
            {
                printf("%d\n", current_id);
            }
            object[id].request_list.pop_front();
            // 更新请求计数
            int object_id = request[current_id].object_id;
            for (int i = 1; i <= REP_NUM; i++)
            {
                int disk_id = object[object_id].replica[i];
                int tag = object[object_id].tag;
                if (tag_disk_request_count[tag][disk_id] > 0)
                {
                    tag_disk_request_count[tag][disk_id]--;
                }
            }
        }
        // 删除对象
        for (int j = 1; j <= REP_NUM; j++)
        { // 删除副本j
            // current_occupation[object[id].replica[j]] -= object[id].size;             // 更新磁盘占用
            int disk_id = object[id].replica[j];
            int tag = object[id].tag;
            int size = object[id].size;
            tag_disk_usage[disk_id][tag] -= size; // 更新标签磁盘占用
            // update_tail_empty(object[id].replica[j]);                                 // 更新该磁盘尾部连续空块
            tag_disk_counter[tag][disk_id] -= size; // 更新标签磁盘占用
            disk_available[disk_id] += size;        // 更新磁盘剩余空间
            do_object_delete(object[id].unit[j], disk[disk_id], size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}

void initialize_disks()
{
    for (int i = 1; i <= N; i++)
    {
        for (int j = 1; j <= V; j++)
        {
            disk[i][j].object_id = 0;
            disk[i][j].pending_requests = 0;
        }
    }
}

// TODO:help
//  谁能帮我实现给定磁盘位置,返回当前位置tag的函数
//  最好磁盘状态是连续tag1-连续blank-连续tag2-...
//  **然后删除导致的小空白不算在blank里**
// 简单模拟(前后10个格子中出现次数最多的tag)
int get_tag(int disk_id, int pos)
{
    // 定义一个数组来统计标签的出现次数
    int tag_count[MAX_TAG] = {0};

    // 遍历周围 10 个格子
    for (int i = -5; i <= 5; i++)
    {
        int current_pos = pos + i;

        // 处理循环边界
        if (current_pos < 1)
        {
            current_pos += V; // 回绕到磁盘末尾
        }
        else if (current_pos > V)
        {
            current_pos -= V; // 回绕到磁盘开头
        }

        // 获取当前位置的对象 ID
        int obj_id = disk[disk_id][current_pos].object_id;

        // 如果当前位置为空（blank），跳过
        if (obj_id == 0)
        {
            continue;
        }

        // 获取对象的标签
        int tag = object[obj_id].tag;

        // 增加标签的计数
        tag_count[tag]++;
    }

    // 找到计数最多的标签
    int most_frequent_tag = 0;
    int max_count = 0;
    for (int tag = 1; tag <= M; tag++)
    {
        if (tag_count[tag] > max_count)
        {
            max_count = tag_count[tag];
            most_frequent_tag = tag;
        }
    }

    return most_frequent_tag;
}

// TODO:选择空余空间最多的磁盘，加入标签链表的连续位置，并且修改有哪些tag存放的磁盘
int allocate_each_object(int object_id, int size, int tag, int replica_num)
{
    // 得到最开始分配的磁盘
    int origin_disk = tag_disk_info[tag].disk_unit[replica_num];

    // 如果原磁盘有足够的空间且该磁盘没有被此对象副本使用
    if (disk_available[origin_disk] >= size && !object[object_id].is_used_disk[origin_disk])
    {
        object[object_id].is_used_disk[origin_disk] = true; // 标记该磁盘已被使用
        return origin_disk;
    }
    else
    {
        // 先查找除了origin_disk之外含有该标签的disk，就判断它的空闲位置是否足够，如果够的话就返回，
        // 如果都不够就再扫一次，返回空闲位置最多的磁盘
        // TODO：选择剩余空间最多/磁盘上标签段种类最少
        for (int index = 1; index <= N; index++)
        {
            if(index==origin_disk) continue;//如果是排除的origin，就继续进行扫射
            if (!tag_disk_have_stored[tag][index])//该磁盘如果没有存储该标签
            {
                continue;
            }
            if (disk_available[index] >= size && !object[object_id].is_used_disk[index])
            {
                return index;
            }
        }
        // 到这里就发现没有磁盘有该标签，顺序扫，选择有空的磁盘//TODO:可优化哈
        for (int i = 1; i <= N; i++)
        {
            if (disk_available[i] >= size && !object[object_id].is_used_disk[i])
            {
                return i;
            }
        }
    }

    // 如果没有可用磁盘则返回-1表示无法分配
    return -1;
}

// 将 new_seg 插入到磁盘 disk_id 的环形链表中 (示例：插在尾部)
void insert_segment(int disk_id, DiskTagSegment *new_seg)
{
    // 如果该磁盘还没有任何段，则自己形成环
    if (!disk_head[disk_id])
    {
        disk_head[disk_id] = new_seg;
        new_seg->prev = new_seg;
        new_seg->next = new_seg;
        return;
    }
    // 否则插在尾部
    DiskTagSegment *head = disk_head[disk_id];
    DiskTagSegment *tail = head->prev; // 环形链表最后一个节点
    // tail <-> new_seg <-> head
    tail->next = new_seg;
    new_seg->prev = tail;
    new_seg->next = head;
    head->prev = new_seg;
}

int available_place(int a, int b)
{ // ab是磁盘中的点//V是单个磁盘的总块数//计算的是gap哦,在从小到大顺序下a在b前
    if (a <= b)
    {
        return b - a - 1;
    }
    else
    {
        return V - a + b - 1;
    }
}

DiskTagSegment *insert_middle_segment(int disk_id, DiskTagSegment *current_seg, int need_size, int tag)
{
    // 1) 计算插入位置
    int gap = current_seg->gap;
    int gap_start = current_seg->usage_end_index + 1;
    int offset = (gap - need_size) / 2;
    int new_start = gap_start + offset;
    if (new_start > V)
        new_start -= V;
    int new_end = new_start + need_size - 1;
    if (new_end > V)
        new_end -= V;
    int new_gap = available_place(new_end, current_seg->next->start_index);

    //更新一下前一段的gap
    current_seg->gap= available_place(current_seg->usage_end_index,new_start);

    // 2) 创建新段
    DiskTagSegment *new_seg = new DiskTagSegment(tag, new_start, new_end, new_gap);

    // 3) 插入环形链表 (new_seg 在 current_seg 和 current_seg->next 之间)
    current_seg->next->prev = new_seg;
    new_seg->prev = current_seg;
    new_seg->next = current_seg->next;
    current_seg->next = new_seg;

    return new_seg;
}

// 假设你在全局有：DiskTagSegment* disk_head[MAX_DISK_NUM] 表示每个磁盘的环形链表头
DiskTagSegment *find_tag_segment(int disk_id, int tag_id)
{
    // 获取该磁盘的链表头
    DiskTagSegment *head = disk_head[disk_id];
    if (!head)
    {
        // 如果磁盘上还没有任何标签段
        return nullptr;
    }

    // 指针从 head 开始
    DiskTagSegment *current = head;

    do
    {
        if (current->tag_id == tag_id)
        {
            // 找到对应标签
            return current;
        }
        current = current->next;
    } while (current != head);

    // 如果遍历一圈没找到，返回 nullptr
    return nullptr;
}

DiskTagSegment *find_tag_gap_max(int disk_id)
{
    // 获取该磁盘的链表头
    DiskTagSegment *head = disk_head[disk_id];

    DiskTagSegment *current = head;
    DiskTagSegment *max_gap_node = nullptr;

    int max_gap = -1; // 初始化为最小值

    do
    {
        // 找到对应标签，检查 gap
        if (current->gap > max_gap)
        {
            max_gap = current->gap;
            max_gap_node = current;
        }
        current = current->next;
    } while (current != head);

    // 返回 gap 最大的节点（如果没有匹配的 tag_id，返回 nullptr）
    return max_gap_node;
}

// 为每个磁盘和标签分配相等大小的存储空间
void allocate_space_per_tag(int tags_per_disk)
{
    int space_per_tag = V / tags_per_disk; // 每个标签在每个副本上的空间大小

    // 当前每个磁盘的写入位置指针（用于确定 start_index 和 end_index）
    int disk_pos[MAX_DISK_NUM] = {0};

    // M=标签总数, N=磁盘总数, 每个标签3个副本
    // tag_disk_info[i].disk_unit[r] 表示"标签i、第r个副本"存在哪个磁盘
    for (int tag_id = 1; tag_id <= M; tag_id++) {
        for (int replica = 1; replica <= 3; replica++) {
            // 计算磁盘编号
            int disk_id = ((tag_id - 1) * 3 + (replica - 1)) % N + 1;

            tag_disk_info[tag_id].disk_unit[replica] = disk_id;//记录下来

            int start_index = disk_pos[disk_id] + 1; // 最开始的就是1
            tag_disk_info[tag_id].disk_start[replica] = start_index;//TODO 可以优化，这个没用上

            int end_index=start_index-1;
            if(end_index<1){
                end_index=V;
            }
            // 新建一个磁盘段
            DiskTagSegment *new_seg = new DiskTagSegment(tag_id, start_index, end_index, space_per_tag - 1); // 现在的end就是start，gap就是end和下一个start中间还能插入几个
            // 将新段插入该磁盘的环形链表
            insert_segment(disk_id, new_seg);

            // 更新指针
            disk_seg_tag_count[disk_id]++;
            disk_pos[disk_id] = disk_pos[disk_id] + space_per_tag - 1;
        }
    }
}



stack<DiskTagSegment *> build_segment_stack(int disk_id, int tag)
{
    stack<DiskTagSegment *> seg_stack;
    DiskTagSegment *head = disk_head[disk_id];
    if (!head)
        return seg_stack;

    // 假设通过环形遍历:
    DiskTagSegment *current = head;
    do
    {
        // 如果是当前标签，就压栈
        if (current->tag_id == tag)
        {
            seg_stack.push(current);
        }
        current = current->next;
    } while (current != head);

    return seg_stack;
}

void do_object_write(int *object_unit, DiskTagSegment *disk_head, Disk *disk_unit, int size, int object_id, int tag, int disk_num, int replica_num, int tags_per_disk)
{
    // int start_index = disk_tag_info[disk_num][tag].start_index;//标签的起始位置
    // int end_index= disk_tag_info[disk_num][tag].usage_end_index;//标签存储的结束位置

    // int next_tag = disk_tag_info[disk_num][tag].tag_id_next;//下一个标签
    // int next_start = disk_tag_info[disk_num][next_tag].start_index;//下一个标签的起始位置

    // int prev_tag=disk_tag_info[disk_num][tag].tag_id_prev;//前一个标签
    // int prev_end=disk_tag_info[disk_num][prev_tag].usage_end_index;//前一个标签用到的位置

    // int tag_disk_gap[MAX_DISK_NUM][MAX_TAG]; // 空余空间

    // 先检查当前时间片是多少
    // 写到空隙占比了,要维护空隙数组,就是要在每次删除对象时从三个磁盘里增加空袭,再在20个时间片之后在空隙中插入对象时减去空袭,并且两个都要更新占比,或者要用的时候再更新占比
    // 由此想到要维护每个磁盘的连续空白块，也可以根据end来维护,用于没位置的时候寻找位置
    // 或者就是尾部没位置了就填空隙

    // 这里需要将对象块写入指定的磁盘位置，确保对象的每个块存储在磁盘的可用位置
    // 假设我们有以下记录每个磁盘的标签空余空间的数组
    // 记录每个磁盘标签的空余空间范围

    // 当前对象还需要写入的数量
    int need_write = size;
    int written_count = 0;

    // 直接拿到栈
    //TODO 判断对象的大小，大的就加到尾部，小的就填补空隙
    stack<DiskTagSegment *> seg_stack = build_segment_stack(disk_num, tag);
    DiskTagSegment *pos_seg;

    while (!seg_stack.empty() && need_write > 0)
    {
        DiskTagSegment *now_seg = seg_stack.top();
        pos_seg = now_seg;
        seg_stack.pop();

        // 前后都延伸
        while (now_seg->gap > 0 && need_write > 0)
        {            
            now_seg->usage_end_index++;                                                // TODO:可以扫完一遍之后再更新
            if (now_seg->usage_end_index > V)
            {
                now_seg->usage_end_index = 1;
            }
            disk_unit[now_seg->usage_end_index].object_id = object_id;         // 修改磁盘存储状态
            object_unit[++written_count] = now_seg->usage_end_index; // 对象的存储状态
            need_write--;
            //!!!要确保环绕，数据合理
            now_seg->available--;
            now_seg->gap--;
        }

        if (need_write == 0)
        {
            return;
        }

        while (now_seg->prev->gap > 0 && need_write > 0)
        {
            now_seg->start_index -= 1;
            if (now_seg->start_index < 1)
            {
                now_seg->start_index = V;
            } // 确保当前存入位置合理

            disk_unit[now_seg->start_index].object_id = object_id;         // 修改磁盘存储状态
            object_unit[++written_count] = now_seg->start_index; // 对象的存储状态
            need_write--;
            now_seg->prev->gap--;
        }

        if (need_write == 0)
        {
            return;
        }

        int this_pos=now_seg->start_index;
        int stop=now_seg->usage_end_index+1;
        if(stop>V){
            stop=1;
        }
        while (this_pos != stop && need_write>0)//如果没有扫到尾部或者没有存满
        {
            if(disk_unit[this_pos].object_id==0){//如果有空就存进来
                disk_unit[this_pos].object_id=object_id;
                object_unit[++written_count] = this_pos; // 对象的存储状态
                need_write--;//存完了
            }
            //现在我要移动this_pos了
            this_pos++;
            if(this_pos>V){
                this_pos=1;
            }
        }

        if (need_write == 0)
        {
            return;
        }
    }

    // 走到这里说明还没满，如果一直不满就一直开新tag
    while (need_write > 0)
    {
        // 从大到小依次查找gap
        DiskTagSegment *current_seg = find_tag_gap_max(disk_num);
        pos_seg=current_seg;
        // 开个新的
        if(current_seg->gap){
            DiskTagSegment *new_seg;
            if(need_write<current_seg->gap){
                new_seg = insert_middle_segment(disk_num, current_seg, need_write, tag);
            }else{
                new_seg = insert_middle_segment(disk_num, current_seg, current_seg->gap, tag);
            }//TODO 对end和start维护得更仔细
            
            //更新对象和磁盘的状态
            int store_pos = new_seg->start_index;
            int stop=new_seg->usage_end_index+1;
            if(stop>V){
                stop=1;
            }
            
            do{
                disk_unit[store_pos].object_id = object_id;
                object_unit[++written_count] = store_pos;
                store_pos++;
                need_write--;
                if(store_pos>V){
                    store_pos=1;
                }
            }while(store_pos!=stop);
            
            disk_seg_tag_count[disk_num]++;
            
        }else{
            break;
        }

        if(need_write==0){
            return;
        }
    }

    // 走到这里还没存完，只有一种情况，所有gap都不够存这个对象了，所有tag的usage和下一个start都紧贴
    // 从当前pos_seg的下一个节点开始，把链表中的每一块扫完，填满
    //TODO 如果后期优化，就存储哪个空隙在哪里开始，连续多少 而且一个对象的块在哪里都存储了，删除的时候可以查找块所在的位置
    int start_pos=pos_seg->next->start_index;
    int all_pos=start_pos;
    bool first_loop = true;
    while (need_write > 0) {
        // 若当前位置空，就写
        if (disk_unit[all_pos].object_id == 0) {
            disk_unit[all_pos].object_id = object_id;
            object_unit[++written_count] = all_pos;
            need_write--;
            if (need_write <= 0) break;
        }

        // 前进一个位置
        all_pos++;
        if (all_pos > V) {
            all_pos = 1; // 环绕回磁盘开头
        }

        // 如果回到起始位置 => 说明一圈写完了
        if (all_pos == start_pos) {
            break;
        }
    }

    if(need_write){
        throw std::runtime_error("错误：磁盘 " + std::to_string(disk_num) + " 没有位置");
    }

}

//!!!检查环绕
void write_action(int tags_per_disk)
{
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++)
    {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        // object[id].last_request_point = 0;
        object[id].tag = tag;
        for (int j = 1; j <= REP_NUM; j++)
        {
            // 根据标签和可用空间更新副本分配逻辑
            int disk_num;

            // object[id].replica[j] = tag_disk_info[tag].disk_unit[j - 1];
            // int object_id,int size,int tag,int replica_num
            object[id].replica[j] = allocate_each_object(id, size, tag, j);

            disk_num = object[id].replica[j];
            if (disk_num == -1)
            {
                printf("Error: allocate disks\n");
            }
            disk_available[disk_num] -= size;
            object[id].is_used_disk[disk_num] = true;


            // 为对象分配存储空间
            object[id].unit[j] = static_cast<int *>(calloc(size + 5, sizeof(int))); // 从1开始存，存到size，把每个都初始化为0
            object[id].size = size;
            object[id].is_delete = false;

            //更新该标签有没有存储到新磁盘上//TODO 维护每个磁盘上该标签的对象总数,这样就可以更富集了
            tag_disk_have_stored[tag][disk_num]=true;//TODO 删除的时候可能需要维护一下

            // 使用新方法查找最佳存储空间
            // int start_index = tag_disk_info[tag].disk_start[j-1];       // 从标签分配信息中获取起始索引
            // int end_index = tag_disk_info[tag].disk_start[j-1] + N * V / (M * 3); // TODO: 使用邻居的start_index
            //int *object_unit, DiskTagSegment *disk_head, int *disk_unit, int size, int object_id, int tag, int disk_num, int replica_num, int tags_per_disk
            do_object_write(object[id].unit[j], disk_head[disk_num], disk[disk_num], size, id, tag, disk_num, j, tags_per_disk);
        }

        printf("%d\n", id);
        for (int j = 1; j <= REP_NUM; j++) // 输出存储信息
        {
            printf("%d", object[id].replica[j]);
            for (int k = 1; k <= size; k++)
            {
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

// ************************************读取请求****************************************************************

int get_most_req_tag(int disk_id)
{
    int most_req_tag = 0;
    int max_requests = 0;

    for (int tag = 1; tag <= M; tag++)
    {
        if (tag_disk_request_count[tag][disk_id] > max_requests)
        {
            max_requests = tag_disk_request_count[tag][disk_id];
            most_req_tag = tag;
        }
    }

    // 如果无请求,返回0
    return most_req_tag;
}

// TODO:获取最多请求的位置
int get_most_req_position(int disk_id, int tag)
{
    // 获取磁盘的标签段链表头
    DiskTagSegment *head = disk_head[disk_id];
    if (!head)
    {
        // 如果磁盘上没有任何标签段，返回 -1
        return 0;
    }

    // 遍历链表，查找指定标签
    DiskTagSegment *current = head;
    do
    {
        if (current->tag_id == tag)
        {
            // 找到指定标签，返回其起始位置
            return current->start_index;
        }
        current = current->next;
    } while (current != head);

    // 如果未找到指定标签，返回 -1
    return 0;
    // int window_size = 100;
    // int window_num = V / window_size + 1;
    // int max_requests = 0;
    // int max_window = 0;
    // int max_position = 0;
    // for (int i = 0; i < window_num - 1; i++)
    // {
    //     int req_win = 0;
    //     for (int j = 1; j <= window_size; j++)
    //     {
    //         if (disk[disk_id][i * window_size + j].pending_requests != 0)
    //         {
    //             req_win++;
    //             if (req_win > max_requests)
    //             {
    //                 max_requests = req_win;
    //                 max_window = i;
    //             }
    //         }
    //     }
    // }

    // int req_win = 0;
    // for (int j = (window_num - 1) * window_size + 1; j <= V; j++)
    // {
    //     if (disk[disk_id][j].pending_requests != 0)
    //     {
    //         req_win++;
    //         if (req_win > max_requests)
    //         {
    //             max_requests = req_win;
    //             max_window = window_num;
    //         }
    //     }
    // }

    // for (int j = max_window * window_size + 1; j <= (max_window + 1) * window_size; j++)
    // {
    //     if (disk[disk_id][j].pending_requests != 0)
    //     {
    //         max_position = j;
    //         break;
    //     }
    // }

    // return max_position;
    // 注意: 爷爷手写，可能有bug

    // int most_req_position = 0;
    // for (int i = 1; i <= V; i++)
    // {
    //     int pos = (disk_point[disk_id] + i - 1) % V + 1; // 循环遍历磁盘存储单元
    //     int obj_id = disk[disk_id][pos].object_id;

    //     // 如果当前位置有对象且标签匹配
    //     if (obj_id != 0 && object[obj_id].tag == tag)
    //     {
    //         most_req_position = pos;
    //         break;
    //     }
    // }
    // // 如果无请求,返回0
    // return most_req_position;
}

// 磁头跳跃,磁头disk_id跳跃到的存储单元编号,0表示不执行jump
int jump_to(int disk_id)
{
    int current_position = disk_point[disk_id];
    int pos_tag = get_tag(disk_id, current_position);
    int most_req_tag = get_most_req_tag(disk_id);
    int most_req_position = get_most_req_position(disk_id, most_req_tag);
    // 如果无请求,返回0
    if (most_req_tag == 0 || most_req_position == 0)
    {
        return 0;
    }

    if (pos_tag) // 如果当前位置是tag
    {
        // 如果当前位置的tag不是最多请求的tag,且距离最多请求的tag的距离大于G
        if (pos_tag != most_req_tag && (most_req_position - pos_tag + V) % V > G)
        {
            return most_req_position;
        }
    }
    else // 如果当前位置是blank
    {
        if ((most_req_position - pos_tag + V) % V > G)
        {
            return most_req_position;
        }
    }
    return 0;
}
// 更新请求状态
void update_request_status(int request_id, int block_id, int object_size)
{
    request[request_id].object_block_id[block_id] = true;

    // 检查是否所有块都已读取
    bool all_blocks_read = true;
    for (int i = 1; i <= object_size; i++)
    {
        if (!request[request_id].object_block_id[i])
        {
            all_blocks_read = false;
            break;
        }
    }

    // 如果所有块都已读取，标记请求为完成
    if (all_blocks_read)
    {
        request[request_id].is_done = true;
        // 更新请求计数
        int object_id = request[request_id].object_id;
        for (int i = 1; i <= REP_NUM; i++)
        {
            int disk_id = object[object_id].replica[i];
            int tag = object[object_id].tag;
            if (tag_disk_request_count[tag][disk_id] > 0)
            {
                tag_disk_request_count[tag][disk_id]--;
            }
        }
    }
}
// 最早当前块未读取的请求
int get_earliest_request(int object_id, int block_id)
{
    for (int request_id : object[object_id].request_list)
    {
        // 检查请求的某块是否未完成
        if (!request[request_id].object_block_id[block_id])
        {
            return request_id;
        }
    }
    return 0;
}

// 获取下一个读取位置
int get_next_read_position(int disk_id)
{
    int current_position = disk_point[disk_id];
    for(int i=1;i<=V;i++){
        int pos = (current_position+i-1)%V;
        if(disk[disk_id][pos].pending_requests!=0){
            return pos;
        }
    }
    return 0;
}

// read的消耗
int read_consume(int disk_id)
{
    if (disk_pre_move[disk_id] != READ)
    {
        return 64;
    }
    int ceilValue = static_cast<int>(std::ceil(disk_pre_token[disk_id] * 0.8));
    return std::max(16, ceilValue);
}

// TODO:优化pass_read_decision
//  确定执行多少次pass和read,并更新action和times,确保left_G足够
// magic number :10
void pass_read_decision(int disk_id, int &action, int &times, int left_G)
{
    int next_read_position = get_next_read_position(disk_id);
    int distance_to_next_position = (next_read_position - disk_point[disk_id] + V) % V;
    int req = disk[disk_id][disk_point[disk_id]].pending_requests;
    if (req > 0)
    {
        action = READ;
        times = 1;
        return;
    }

    // 动态计算跳过的阈值
    int dynamic_threshold = 10; // 默认阈值
    if (left_G > 0.8 * G)
    {
        // 如果剩余令牌较多，可以跳过更多的存储单元
        dynamic_threshold = 20;
    }
    else if (left_G < 0.2 * G)
    {
        // 如果剩余令牌较少，则跳过的阈值应更小
        dynamic_threshold = 5;
    }

    // 如果到下一个读取位置的距离大于动态阈值，我们选择PASS
    if (distance_to_next_position > dynamic_threshold)
    {
        action = PASS;
        times = next_read_position-disk_point[disk_id]-1;
        return;
    }else{
        action = READ;
        times = 1;
        return;
    }
}
// void pass_read_decision(int disk_id, int &action, int &times, int left_G)
// {
//     int next_read_position = get_next_read_position(disk_id);
//     int distance_to_next_position = (next_read_position - disk_point[disk_id] + V) % V;
//     int req = disk[disk_id][disk_point[disk_id]].pending_requests;
//     if (req > 0)
//     {
//         action = READ;
//         times = 1;
//         return;
//     }
//     // 动态计算跳过的阈值
//     int dynamic_threshold = 10; // 默认阈值
//     if (left_G > 0.8 * G)
//     {
//         // 如果剩余令牌较多，可以跳过更多的存储单元
//         dynamic_threshold = 20;
//     }
//     else if (left_G < 0.2 * G)
//     {
//         // 如果剩余令牌较少，则跳过的阈值应更小
//         dynamic_threshold = 5;
//     }

//     // 如果到下一个读取位置的距离大于动态阈值，我们选择PASS
//     if (distance_to_next_position > dynamic_threshold)
//     {
//         action = PASS;
//         times = distance_to_next_position - 1; // 尽可能跳过，但留一个位置进行读取
//     }
//     else
//     {
//         action = READ;
//         times = 1; // 在当前位置执行读取操作
//     }
// }

// 模拟连续读取策略与跳过策略的令牌消耗
int evaluate_token_consumption(int disk_id)
{
    int current_position = disk_point[disk_id];
    int next_position = get_next_read_position(disk_id);
    int distance_to_next_position = (next_position - current_position + V) % V;

    // 策略1: 连续读取策略
    int total_tokens_read = 0;
    int total_tokens_skipped = 0;

    // 执行连续读取，假设每个读取动作根据上一次的令牌消耗递减
    int last_token_consumed = 64; // 假设第一次读取消耗64令牌
    while (current_position != next_position)
    {
        total_tokens_read += last_token_consumed;
        last_token_consumed = std::max(16, static_cast<int>(std::ceil(last_token_consumed * 0.8))); // 更新令牌消耗
        current_position = (current_position + 1) % V;
    }

    // 策略2: 跳过策略
    int total_tokens_skip = 0;
    if (distance_to_next_position > 10)
    {
        total_tokens_skip = distance_to_next_position - 1; // 每跳过一个位置消耗1个令牌
    }

    // 比较两种策略的令牌消耗，返回最小的消耗
    return std::min(total_tokens_read, total_tokens_skip);
}

void pass(int disk_id, int &left_G)
{
    printf("p");
    left_G--;              // Pass 消耗 1 个令牌
    disk_point[disk_id]++; // 磁头移动到下一个存储单元
    if (disk_point[disk_id] > V)
    {
        disk_point[disk_id] = 1; // 磁头循环回到起点
    }
    disk_pre_token[disk_id] = 1;
    disk_pre_move[disk_id] = PASS;
}

void read(int disk_id, int &left_G, int &n_request_complete, std::vector<int> &request_complete)
{
    if (read_consume(disk_id) <= left_G && disk[disk_id][disk_point[disk_id]].pending_requests > 0)
    {

        int obj_id = disk[disk_id][disk_point[disk_id]].object_id;
        int rep_id = 0, block_id = 0;
        for (int i = 1; i <= REP_NUM; i++)
        {
            if (object[obj_id].replica[i] == disk_id)
            {
                rep_id = i;
                break;
            }
        }
        for (int i = 1; i <= object[obj_id].size; i++)
        {
            if (object[obj_id].unit[rep_id][i] == disk_point[disk_id])
            {
                block_id = i;
                break;
            }
        }

        int request_id = get_earliest_request(obj_id, block_id);
        if(request_id==0){
            pass(disk_id, left_G);
            return;
        }
        // 更新请求状态
        for (int i = 1; i <= REP_NUM; i++)
        {
            int disk_id = object[obj_id].replica[i];
            disk[disk_id][object[obj_id].unit[i][block_id]].pending_requests--;
        }

        update_request_status(request_id, block_id, object[obj_id].size);

        if (request[request_id].is_done)
        {
            n_request_complete++;
            request_complete.push_back(request_id);
            object[obj_id].request_list.pop_front();
        }
        printf("r");
        left_G -= read_consume(disk_id);
        disk_point[disk_id]++;
        if (disk_point[disk_id] > V)
        {
            disk_point[disk_id] = 1; // 磁头循环回到起点
        }
        disk_pre_token[disk_id] = read_consume(disk_id);
        disk_pre_move[disk_id] = READ;
    }
    else
    {
        pass(disk_id, left_G);
        return;
    }
}
// 磁头移动
void disk_move(int disk_id, int &n_request_complete, std::vector<int> &request_complete)
{
    int left_G = G; // 剩余令牌数
    // 为了避免磁头在一个tag内多次跳跃，设置一个标志位
    static bool ready_to_jump = true;
    // 尝试执行 Jump 动作
    if (ready_to_jump)
    {
        int jump = jump_to(disk_id);
        if (jump)
        {
            printf("j %d\n", jump);
            disk_point[disk_id] = jump;    // 更新磁头位置
            disk_pre_token[disk_id] = G;   // Jump 消耗所有令牌
            disk_pre_move[disk_id] = JUMP; // 更新磁头上一次动作

            // ready_to_jump = false;
            return;
        }
    }

    // 循环处理 Pass,直到到达tag
    while (left_G > 0 && !get_tag(disk_id, disk_point[disk_id]))
    {
        // 执行动作,并更新left_G,n_request_complete,request_complete
        pass(disk_id, left_G);
        // ready_to_jump = false;
    }

    // 循环处理 Pass 和 Read 动作，直到令牌耗尽
    while (left_G > 0)
    {
        int action;
        int times;
        pass_read_decision(disk_id, action, times, left_G); // 决定执行多少次pass和read,并更新action和times
        for (int i = 0; i < times; i++)
        {
            if (left_G <= 0)
            {
                break;
            }
            if (action == PASS)
            {
                pass(disk_id, left_G);
            }
            else
            {
                read(disk_id, left_G, n_request_complete, request_complete);
            }//TODO:stop
        }
    }
    printf("#\n");
}
// 初始化请求
void initialize_request(int request_id, int object_id, int object_size)
{
    request[request_id].object_id = object_id;
    request[request_id].is_done = false;
    for (int i = 1; i <= object_size; i++)
    {
        request[request_id].object_block_id[i] = false;
    }
    object[object_id].request_list.push_back(request_id);
    // 更新请求计数
    for (int i = 1; i <= REP_NUM; i++)
    {
        int disk_id = object[object_id].replica[i];
        tag_disk_request_count[object[object_id].tag][object[object_id].replica[i]]++;
        for (int j = 1; j <= object_size; j++)
        {
            int position = object[object_id].unit[i][j];
            disk[disk_id][position].pending_requests++; // 增加待读取数目
        }
    }
}

// 读取操作
void read_action()
{
    // n_read：代表这一时间片读取对象的个数。输入数据保证总读取次数小于等于30000000。
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    // 接下来n_read 行，每行两个数req_id[i]、obj_id[i]，代表当前时间片读取的请求编号和请求的对象编号。输入数据保证读请求编号为1 开始每次递增1 的整数，读取的对象在请求到来的时刻一定在存储系统中
    for (int i = 1; i <= n_read; i++)
    {
        scanf("%d%d", &request_id, &object_id);
        initialize_request(request_id, object_id, object[object_id].size);
    }

    int n_rsp = 0;
    std::vector<int> rsp_id;
    rsp_id.push_back(0);

    // 每个磁头的运动输出
    for (int i = 1; i <= N; i++)
    {
        disk_move(i, n_rsp, rsp_id);
    }

    //   n_rsp：代表当前时间片上报读取完成的请求个数。
    //  接下来n_rsp 行，每行1 个数req_id[i]，代表本时间片上报读取完成的读取请求编号。
    printf("%d\n", n_rsp);
    for (int i = 1; i <= n_rsp; i++)
    {
        printf("%d\n", rsp_id[i]);
    }

    fflush(stdout);
}

// 清理
void clean()
{
    for (auto &obj : object)
    {
        for (int i = 1; i <= REP_NUM; i++)
        {
            if (obj.unit[i] == nullptr)
                continue;
            delete[] obj.unit[i]; // 使用 delete[] 释放数组内存
            obj.unit[i] = nullptr;
        }
    }
}

int main()
{
    // 读取输入
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);

    // 前𝑚行中，第𝑖行第𝑗个数𝑓𝑟𝑒_𝑑𝑒𝑙[𝑖][𝑗]表示时间片编号𝑖𝑑满足 (𝑗−1)∗1800+1≤𝑖𝑑 ≤𝑗∗1800的情况下，所有删除操作中对象标签为𝑖的对象大小之和。
    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }
    // 接下来𝑚行，第𝑖行第𝑗个数𝑓𝑟𝑒_𝑤𝑟𝑖𝑡𝑒[𝑖][𝑗] 表示时间片编号𝑖𝑑满足(𝑗−1)∗1800 + 1≤𝑖𝑑 ≤𝑗∗ 1800的情况下，所有写入操作中对象标签为𝑖的对象大小之和。
    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }
    // 接下来𝑚行，第𝑖行第𝑗个数𝑓𝑟𝑒_𝑟𝑒𝑎𝑑[𝑖][𝑗] 表示时间片编号𝑖𝑑满足(𝑗−1)∗1800 + 1≤𝑖𝑑 ≤𝑗∗ 1800的情况下，所有读取操作中对象标签为𝑖的对象大小之和，同一个对象的多次读取会重复 计算。
    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    initialize_disks();
    // 初始化磁盘//初始化每个磁盘的剩余空间
    for (int i = 1; i <= N; i++)
    {
        disk_point[i] = 1;
        disk_pre_move[i] = PASS;
        disk_pre_token[i] = 0;
        disk_available[i] = V;
    }

    // TODO:维护available，维护完之后给每个副本分磁盘（和给每个标签分磁盘是不同的）

    // 给每个磁盘分配标签空间
    int tags_per_disk = ceil(static_cast<double>(M) * 3 / N); // 每个磁盘负责的标签数量
    allocate_space_per_tag(tags_per_disk);
    // initialize_tag_id_next(tags_per_disk);

    // 主循环
    for (int t = 1; t <= T + EXTRA_TIME; t++)
    {
        timestamp_action();
        delete_action();
        write_action(tags_per_disk);
        read_action();
    }
    clean();

    return 0;
}