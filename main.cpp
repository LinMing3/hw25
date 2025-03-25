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
    int replica[REP_NUM + 5];     // 副本,存储该副本在哪个磁盘,replica[j]表示副本j存储在哪个磁盘
    int *unit[REP_NUM + 5];       // 对象块,存储该块在磁盘的位置,unit[j][k]表示副本j的第k个对象块存储的磁盘单元位置
    std::deque<int> request_list; // 当前对象的读取请求,记录每个对象块的待读取次数,read_count[i]表示第i个request的id
    int size;                     // 大小
    int tag;                      // 标签
    bool is_delete;               // 是否删除
} Object;

// 请求数组,request[request_id]表示请求request_id
Request request[MAX_REQUEST_NUM];
// 对象数组
Object object[MAX_OBJECT_NUM];

// T：代表本次数据有𝑇 + 105个时间片，后续输入第二阶段将循环交互𝑇 + 105次。 时间片编号为1 ~𝑇 + 105。输入数据保证1≤𝑇≤86400。对于第𝑇 + 1 ~𝑇 + 105个时间分片，输入数据 保证没有删除、写入和读取请求。
// M：代表对象标签数。对象标签编号为1 ~𝑀。输入数据保证1≤𝑀≤16。
// N：代表存储系统中硬盘的个数，硬盘编号为1 ~𝑁。输入数据保证3≤𝑁 ≤10。
// V：代表存储系统中每个硬盘的存储单元个数。存储单元编号为1 ~𝑉。输入数据保证1≤𝑉≤ 16384，任何时间存储系统中空余的存储单元数占总存储单元数的至少10 %。
// G：代表每个磁头每个时间片最多消耗的令牌数。输入数据保证64≤𝐺 ≤1000。
int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];                   // 磁盘,disk[i][j]表示磁盘i的第j个存储单元存储的对象id
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

void initialize_tag_disk_map()
{
    for (int i = 1; i <= M; i++)
    {
        tag_disk_map[i] = (i - 1) % N + 1; // 让相邻标签尽可能映射到相邻磁盘
    }
}

// 更新每个磁盘中每个标签占用的空间
// TODO:一个个扫描太浪费时间，可以改进，每次写入对象时更新
// void update_tag_disk_usage(int disk_id, int tag)
// {
//     int count = 0; // 用于统计属于标签tag的对象块数量
//     for (int j = V; j >= 1; j--)
//     {
//         int obj_id = disk[disk_id][j];
//         if (object[obj_id].tag == tag)
//         {
//             count++; // 如果对象的标签是指定的标签，增加计数
//         }
//     }
//     tag_disk_usage[disk_id][tag] = count;
// }

// 更新每个磁盘中尾部连续位置长度
void update_tail_empty(int disk_id)
{
    int count = 0;
    for (int j = V; j >= 1; j--)
    {
        if (disk[disk_id][j] == 0)
            count++;
        else
            tail_start[disk_id] = j + 1;
        break;
    }
    tail_empty[disk_id] = count;
}

// 在该标签区域内进行连续写入
// TODO:只允许写入该标签分配的空间?没想好
bool try_write_continuous_tail(int *object_unit, int *disk_unit, int size, int object_id, int disk_id)
{
    int start = tail_start[disk_id];
    int end = start + size - 1;

    // 检查是否有足够的连续空间
    if (end > V)
    {
        return false; // 超出磁盘范围
    }

    // 写入数据
    for (int i = start; i <= end; i++)
    {
        disk_unit[i] = object_id;
        object_unit[i - start + 1] = i;
    }

    // 更新尾部起始位置
    tail_start[disk_id] = end + 1;
    // 更新尾连续长度
    tail_empty[disk_id] -= size;

    return true;
}

// 在该标签区域内进行碎片写入
// TODO:只允许写入该标签分配的空间
bool try_write_fragmented(int *object_unit, int *disk_unit, int size, int object_id, int disk_id)
{
    int written = 0;

    for (int i = 1; i <= V && written < size; i++)
    {
        if (disk_unit[i] == 0)
        {
            disk_unit[i] = object_id;
            object_unit[++written] = i;
        }
    }

    // 更新尾部数组维护
    update_tail_empty(disk_id);

    return written == size;
}

// TODO:可以把标签也放入这个结构体的成员中
struct BestThree
{
    int best1;
    int best2;
    int best3;
    bool best1_tail;
    bool best2_tail;
    bool best3_tail;
    // 添加构造函数
    BestThree(int b1, int b2, int b3, bool b1_tail, bool b2_tail, bool b3_tail)
        : best1(b1), best2(b2), best3(b3),
          best1_tail(b1_tail), best2_tail(b2_tail), best3_tail(b3_tail) {}
};

BestThree find_best_disks_for_tag(int tag, int obj_size)
{
    // TODO：计算每个磁盘的每个标签的空间大小
    // TODO:每个磁盘都要放16个标签？感觉很碎啊，看别人的每个磁盘约放七到十个标签，看你最开始写的每个磁盘要么放3个要么放十几个，那个逻辑是什么
    // TODO:和best1无关,只要不相同就行,每个best不能相同
    int space_per_tag = M / N;  // 每个标签分配一个固定的空间，M是标签数量,N是磁盘数量
    int tail_score_times = 1.5; // 尾部空闲块决定的得分倍数
    int V_percent = 0.2;        // 把对象块大小和V的percent比较
    int L1 = 1.5;               // 超出tag_space的惩罚参数

    const double MAX_SCORE = 200.0;      // 得分上限
    const double RESERVED_RATIO = 0.1;   // 保留空间比例
    const double BASE_SCALE = 100.0;     // 基础分缩放
    const double OVERFLOW_PENALTY = 1.8; // 超限惩罚
    const double TAIL_BONUS = 1.2;       // 尾部奖励
    const double BALANCE_FACTOR = 0.2;   // 负载均衡因子

    struct DiskInfo
    {
        int id;
        double score;
        int free_space;
        bool tail_place;
        bool operator<(const DiskInfo &o) const
        {
            return score > o.score; // 降序排列
        }
    };
    priority_queue<DiskInfo> candidates;

    // 预检查：确保系统至少有一个可用磁盘
    bool has_available = false;
    for (int i = 1; i <= N; i++)
    {
        int actual_free = V - current_occupation[i] - ceil(V * RESERVED_RATIO);
        if (actual_free >= obj_size)
        {
            has_available = true;
            break;
        }
    }
    if (!has_available)
    {
        throw runtime_error("No disk has enough reserved space for object size " + to_string(obj_size));
    }

    // 主评分逻辑
    for (int i = 1; i <= N; i++)
    {
        bool can_put_tail = false;
        // 计算实际可用空间（扣除保留空间）
        int real_free = V - current_occupation[i] - ceil(V * RESERVED_RATIO);
        if (real_free < obj_size)
            continue;

        double score = 0;
        int new_tag_usage = tag_disk_usage[i][tag] + obj_size;

        // 1. 标签空间得分（0~100分）
        double tag_ratio = min(1.0, new_tag_usage / (double)space_per_tag);
        if (new_tag_usage <= space_per_tag)
        {
            score += BASE_SCALE * tag_ratio;
        }
        else
        {
            score += BASE_SCALE * (1 - pow(tag_ratio - 1, OVERFLOW_PENALTY));
        }

        // 2. 空间利用率得分（0~50分）
        double free_ratio = real_free / (double)V;
        score += 50 * min(1.0, free_ratio);

        // 3. 尾部连续奖励（0~30分）
        // TODO 直接放入尾部
        int tail_free = 0;
        for (int j = V; j >= 1 && disk[i][j] == 0; j--)
        {
            tail_free++;
        }
        if (tail_free >= obj_size)
        {
            score += 30 * min(1.0, obj_size / (double)tail_free);
            if (obj_size >= V * V_percent)
            {
                can_put_tail = true;
            }
        }

        // 4. 负载均衡修正
        double usage_ratio = current_occupation[i] / (double)V;
        score *= (1 - BALANCE_FACTOR * usage_ratio);

        // 确保得分合法
        score = max(0.0, min(MAX_SCORE, score));
        candidates.push({i, score, real_free, can_put_tail});
    }

    // 结果处理
    if (candidates.empty())
    {
        throw runtime_error("No valid disk after reserved space check");
    }

    struct DiskOut
    {
        int id;
        bool tail_place;
    };

    vector<DiskOut> res;
    while (!candidates.empty() && res.size() < 3)
    {
        auto disk = candidates.top();
        // 二次检查可用空间（并发安全）
        int recheck_free = V - current_occupation[disk.id] - ceil(V * RESERVED_RATIO);
        if (recheck_free >= obj_size)
        {
            res.push_back(DiskOut{disk.id, disk.tail_place});
        }
        candidates.pop();
    }

    // 补全结果（确保返回3个磁盘）
    // TODO如果还是选不出三个盘，需要改进放置策略
    if (res.size() < 3)
    {
        cout << "Error: Not enough disks, randomizing..." << endl;
    }

    // 使用构造函数初始化
    return BestThree(
        res[0].id, res[1].id, res[2].id,
        res[0].tail_place, res[1].tail_place, res[2].tail_place);
}

// 时间片
void timestamp_action()
{
    // 时间片
    int timestamp;
    // 读取时间片
    scanf("%*s%d", &timestamp);
    // 打印时间片
    printf("TIMESTAMP %d\n", timestamp);

    // 刷新缓冲区
    fflush(stdout);
}

// 删除对象
// object_unit:传入对象的第j个副本在磁盘中存储位置的数组
// disk_unit:磁盘指针
// size:大小
void do_object_delete(const int *object_unit, int *disk_unit, int size)
{
    for (int i = 1; i <= size; i++)
    {
        disk_unit[object_unit[i]] = 0;
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
        {                                                                             // 删除副本j
            current_occupation[object[id].replica[j]] -= object[id].size;             // 更新磁盘占用
            tag_disk_usage[object[id].replica[j]][object[id].tag] -= object[id].size; // 更新标签磁盘占用
            update_tail_empty(object[id].replica[j]);                                 // 更新该磁盘尾部连续空块
            int disk_id = object[id].replica[j];
            tag_disk_counter[object[id].tag][disk_id] -= object[id].size; // 更新标签磁盘占用
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}

// TODO:help
//  谁能帮我实现给定磁盘位置,返回当前位置tag的函数
//  最好磁盘状态是连续tag1-连续blank-连续tag2-...
//  **然后删除导致的小空白不算在blank里**
int get_tag(int disk_id, int pos)
{
    int tag = 0;

    // 如果是blank,返回0
    if (disk[disk_id][pos] == 0)
    {
        return tag;
    }
    else
    {
        return object[disk[disk_id][pos]].tag;
    }
}

// TODO:优化写入算法
//  尝试写入对象
// do_object_write(object[id].unit[j], disk[disk_id], size, id, disk_id,If_tail);
void do_object_write(int *object_unit, int *disk_unit, int size, int object_id, int disk_id, bool If_tail)
{

    // 如果传入的bool为true，先直接顺序写入尾部
    if (If_tail)
    {
        try_write_continuous_tail(object_unit, disk_unit, size, object_id, disk_id);
        return;
    }

    bool ok = false;

    // 如果不能直接写入尾部，从空隙开始填入，一直填到填完为止
    if (!ok && tag_disk_gap[disk_id][object[object_id].tag] >= size)
    {
        ok = try_write_fragmented(object_unit, disk_unit, size, object_id, disk_id);
    }

    if (!ok)
    {
        printf("Error: disk %d cannot store object %d (size %d)\n", disk_id, object_id, size);
        exit(1);
    }

    // 更新已使用空间
    tag_disk_usage[disk_id][object[object_id].tag] += size;
}

// 写入操作////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO:任何时间空余至少10%，初步设想给每个磁盘空余10%，向上取整
// TODO:可以尝试检测曾经被删除对象留下来的空隙
// TODO：每个标签固定写入三个盘
// TODO:如果可以的话，可以尝试在写入时更新occupication和tag_disk_counter
void write_action()
{
    int n_write;
    scanf("%d", &n_write);

    for (int i = 1; i <= n_write; i++)
    {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);

        // object[id].last_request_point = 0;
        object[id].tag = tag;
        object[id].is_delete = false;
        object[id].size = size;

        // Step 1: 选择最佳副本磁盘（三个磁盘）
        BestThree best_disks = find_best_disks_for_tag(tag, size);

        int chosen_disks[REP_NUM + 5] = {best_disks.best1, best_disks.best2, best_disks.best3};
        bool if_tail[REP_NUM + 5] = {best_disks.best1_tail, best_disks.best2_tail, best_disks.best3_tail};

        printf("%d\n", id); // 输出对象编号

        // Step 2: 在磁盘上为每个副本写入
        for (int j = 1; j <= REP_NUM; j++)
        {
            int disk_id = chosen_disks[j - 1];
            bool If_tail = if_tail[j - 1];

            object[id].replica[j] = disk_id;
            object[id].unit[j] = new int[size + 1]; // 使用 new 分配内存

            // 智能写入（根据对象大小自动碎片写或连续写）
            do_object_write(object[id].unit[j], disk[disk_id], size, id, disk_id, If_tail);

            // 写入成功后更新全局信息
            current_occupation[disk_id] += size;    // 更新磁盘占用
            tag_disk_counter[tag][disk_id] += size; // 更新标签磁盘占用
            update_tail_empty(disk_id);             // 更新该磁盘尾部连续空块
        }

        // Step 3: 输出写入位置（4行格式）
        for (int j = 1; j <= REP_NUM; j++)
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
    int most_req_position = 0;
    for (int i = 1; i <= V; i++)
    {
        if (disk[disk_id][i] == tag)
        {
            most_req_position = i;
            break;
        }
    }
    // 如果无请求,返回0
    return most_req_position;
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
int get_earliest_request(int object_id)
{
    for (int request_id : object[object_id].request_list)
    {
        // 检查请求是否未完成
        if (!request[request_id].is_done)
        {
            // 遍历请求的对象块，检查是否有未被读取的块
            for (int i = 1; i <= object[object_id].size; i++)
            {
                if (!request[request_id].object_block_id[i])
                {
                    // 找到符合条件的请求，返回请求 ID
                    return request_id;
                }
            }
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
    if (read_consume(disk_id) <= left_G)
    {
        action = READ;
        times = 1;
        return;
    }
    else
    {
        action = PASS;
        times = 1;
        return;
    }
}

//  执行动作pass或read,并更新left_G,n_request_complete,request_complete
void do_move(int disk_id, int action, int &left_G, int &n_request_complete, std::vector<int> &request_complete)
{
    if (action == PASS)
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
    else if (action == READ)
    {
        int obj_id = disk[disk_id][disk_point[disk_id]];
        // 如果当前位置有对象
        if (obj_id != 0)
        {

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
            int request_id = get_earliest_request(obj_id);
            // 如果当前位置有请求
            if (request_id != 0)
            {
                update_request_status(request_id, block_id, object[obj_id].size);
                if (request[request_id].is_done)
                {
                    n_request_complete++;
                    request_complete.push_back(request_id);
                    object[obj_id].request_list.pop_front();
                }
            }
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
        printf("Error: Unknown action %d\n", action);
        exit(1);
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
        do_move(disk_id, PASS, left_G, n_request_complete, request_complete);
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
            do_move(disk_id, action, left_G, n_request_complete, request_complete); // 执行动作,并更新left_G,n_request_complete,request_complete
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
        tag_disk_request_count[object[object_id].tag][object[object_id].replica[i]]++;
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

    // 初始化磁盘
    for (int i = 1; i <= N; i++)
    {
        disk_point[i] = 1;
        disk_pre_move[i] = PASS;
        disk_pre_token[i] = 0;
    }
    // 初始化map
    initialize_tag_disk_map();

    // 初始化tail_start
    for (int i = 1; i <= N; i++)
    {
        tail_start[i] = 1;
    }

    // 主循环
    for (int t = 1; t <= T + EXTRA_TIME; t++)
    {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}