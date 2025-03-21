#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cmath>     // 包含 ceil 函数
#include <algorithm> // 包含 std::max 函数
#include <deque>     // 包含 std::deque
#include <vector>

using namespace std;

#define MAX_DISK_NUM (10 + 1)          // 磁盘数
#define MAX_DISK_SIZE (16384 + 1)      // 磁盘大小
#define MAX_REQUEST_NUM (30000000 + 1) // 请求数
#define MAX_OBJECT_NUM (100000 + 1)    // 对象数
#define REP_NUM (3)                    // 副本数
#define FRE_PER_SLICING (1800)         // 每个切片的频率
#define EXTRA_TIME (105)               // 额外时间
const int OBJECT_BLOCKS = 5;

#define MAX_TAG (16 + 1) // 标签数量上限（tag编号 1~16）
int tag_disk_counter[MAX_TAG][MAX_DISK_NUM] = {0};

#define PASS 1
#define READ 2
#define JUMP 3

// 请求结构体
// typedef struct Request_
// {
//     int object_id;                           // 请求的对象id
//     int prev_id;                             // 当前对象的前一个请求的id
//     bool is_done;                            // 是否完成
//     bool object_block_id[OBJECT_BLOCKS + 1]; // 对象块id
// } Request;
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
    int replica[REP_NUM + 1];     // 副本,存储该副本在哪个磁盘,replica[j]表示副本j存储在哪个磁盘
    int *unit[REP_NUM + 1];       // 对象块,存储该块在磁盘的位置,unit[j][k]表示副本j的第k个对象块存储的磁盘单元位置
    std::deque<int> request_list; // 当前对象的读取请求,记录每个对象块的待读取次数,read_count[i]表示第i个request的id
    int size;                     // 大小
    int tag;                      // 标签
    // int last_request_point;       // 最近请求id
    bool is_delete;               // 是否删除

} Object;

// 请求数组,request[request_id]表示请求request_id
Request request[MAX_REQUEST_NUM];
// 对象数组
Object object[MAX_OBJECT_NUM];

// T：代表本次数据有𝑇 + 105个时间片，后续输入第二阶段将循环交互𝑇 + 105次。 时间片编号为1 ~𝑇 + 105。输入数据保证1≤𝑇≤86400。对于第𝑇 + 1 ~𝑇 + 105个时间分片，输入数据 保证没有删除、写入和读取请求。
// • M：代表对象标签数。对象标签编号为1 ~𝑀。输入数据保证1≤𝑀≤16。
// • N：代表存储系统中硬盘的个数，硬盘编号为1 ~𝑁。输入数据保证3≤𝑁 ≤10。
// • V：代表存储系统中每个硬盘的存储单元个数。存储单元编号为1 ~𝑉。输入数据保证1≤𝑉≤ 16384，任何时间存储系统中空余的存储单元数占总存储单元数的至少10 %。
// • G：代表每个磁头每个时间片最多消耗的令牌数。输入数据保证64≤𝐺 ≤1000。
int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘,disk[i][j]表示磁盘i的第j个存储单元存储的对象id
int disk_point[MAX_DISK_NUM];          // 磁头,disk_point[i]表示磁盘i的磁头指向的存储单元位置
int disk_pre_move[MAX_DISK_NUM];       // 磁头,disk_pre_move[i]表示磁盘i的磁头上一次动作
int disk_pre_token[MAX_DISK_NUM];      // 磁头,disk_pre_token[i]表示磁盘i的磁头上一次动作消耗的令牌数

int tag_disk_map[MAX_DISK_NUM]; // 记录每个标签优先存储的磁盘
void initialize_tag_disk_map()
{
    for (int i = 1; i <= M; i++)
    {
        tag_disk_map[i] = (i - 1) % N + 1; // 让相邻标签尽可能映射到相邻磁盘
    }
}

int current_occupation[MAX_DISK_NUM] = {0}; // 当前占用对象数

int tail_empty[MAX_DISK_NUM] = {0}; // 记录每块磁盘尾部连续空闲长度
void update_tail_empty(int disk_id)
{
    int count = 0;
    for (int j = V; j >= 1; j--)
    {
        if (disk[disk_id][j] == 0)
            count++;
        else
            break;
    }
    tail_empty[disk_id] = count;
}

// 大对象优先顺序写入
bool try_write_continuous(int *object_unit, int *disk_unit, int size, int object_id, int disk_id)
{

    if (V - current_occupation[disk_id] < size)
    {
        return false;
    }

    for (int i = 1; i <= V - size + 1; i++)
    {
        bool ok = true;
        for (int j = 0; j < size; j++)
        {
            if (disk_unit[i + j] != 0)
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            for (int j = 0; j < size; j++)
            {
                disk_unit[i + j] = object_id;
                object_unit[j + 1] = i + j;
            }
            return true;
        }
    }
    return false;
}
// 小对象优先碎片化写入
bool try_write_fragmented(int *object_unit, int *disk_unit, int size, int object_id)
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
    return written == size;
}

struct BestThree
{
    int best1;
    int best2;
    int best3;
};
BestThree find_best_disks_for_tag(int tag, int obj_size)
{
    int best1 = -1, best2 = -1, best3 = -1;
    int score1 = INT_MIN, score2 = INT_MIN, score3 = INT_MIN;

    for (int i = 1; i <= N; i++)
    {
        int free_space = V - current_occupation[i];
        if (free_space < obj_size)
            continue;

        int score = 0;

        // ① 当前占用越少越好（负载均衡）
        score -= current_occupation[i]; // 越空闲越优

        // ② 标签聚合加分：tag_disk_counter 已统计该标签在此盘已写入了多少
        score += tag_disk_counter[tag][i] * 5;

        // ③ 顺序写优先：尾部连续空块越多越优（阈值优化）
        int tail_free = 0;
        for (int j = V; j >= V - 10 && j >= 1; j--)
        { // 只扫后10个块，快速判断
            if (disk[i][j] == 0)
                tail_free++;
            else
                break;
        }
        if (tail_free >= obj_size)
            score += 20; // 有连续空区域就给 bonus

        // Top 3 排名更新
        if (score > score1)
        {
            score3 = score2;
            best3 = best2;
            score2 = score1;
            best2 = best1;
            score1 = score;
            best1 = i;
        }
        else if (score > score2)
        {
            score3 = score2;
            best3 = best2;
            score2 = score;
            best2 = i;
        }
        else if (score > score3)
        {
            score3 = score;
            best3 = i;
        }
    }

    if (best1 <= 0 || best2 <= 0 || best3 <= 0)
    {
        printf("Error: Cannot find 3 valid disks for tag %d\n", tag);
        exit(1);
    }

    return {best1, best2, best3};
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
// object_unit:对象块
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
    int unfinished_count = 0;// 取消读取数
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
        }
        // 删除对象
        for (int j = 1; j <= REP_NUM; j++)
        {                                                                 // 删除副本j
            current_occupation[object[id].replica[j]] -= object[id].size; // 更新磁盘占用
            int disk_id = object[id].replica[j];
            tag_disk_counter[object[id].tag][disk_id] -= object[id].size; // 更新标签磁盘占用
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}


void do_object_write(int *object_unit, int *disk_unit, int size, int object_id, int disk_id)
{
    int start_pos = -1;
    bool ok = false;

    if (size >= 3)
    {
        ok = try_write_continuous(object_unit, disk_unit, size, object_id, disk_id);
    }
    else
    {
        // 小对象优先碎片利用
        ok = try_write_fragmented(object_unit, disk_unit, size, object_id);
        if (!ok)
        {
            // fallback 尝试顺序写
            ok = try_write_continuous(object_unit, disk_unit, size, object_id, disk_id);
        }
    }

    if (!ok)
    {
        printf("Error: disk %d cannot store object %d (size %d)\n", disk_id, object_id, size);
        exit(1);
    }
}


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
        int chosen_disks[REP_NUM] = {best_disks.best1, best_disks.best2, best_disks.best3};

        printf("%d\n", id); // 输出对象编号

        // Step 2: 在磁盘上为每个副本写入
        for (int j = 1; j <= REP_NUM; j++)
        {
            int disk_id = chosen_disks[j - 1];

            object[id].replica[j] = disk_id;
            object[id].unit[j] = static_cast<int *>(malloc(sizeof(int) * (size + 1)));

            // 智能写入（根据对象大小自动碎片写或连续写）
            do_object_write(object[id].unit[j], disk[disk_id], size, id, disk_id);

            // 写入成功后更新全局信息
            current_occupation[disk_id] += size;
            tag_disk_counter[tag][disk_id] += size;
            update_tail_empty(disk_id); // 更新该磁盘尾部连续空块
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

// 优化读取算法

// 磁头跳跃,磁头disk_id跳跃到的存储单元编号,0表示不执行jump
int jump_to(int disk_id)
{
    int current_position = disk_point[disk_id];
    int count = 0;
    int position = current_position;
    // 从当前磁头位置向前遍历磁盘存储单元
    for (int i = 1; i <= V / 3; i++)
    {
        position = (current_position + i) % V; // 循环计算位置
        if (disk[disk_id][position] != 0)      // 找到最近的非空位置
        {
            count++;
        }
    }
    if (count >= G)
    {
        return position;
    }
    return 0;
}
void update_request_status(int request_id, int block_id, int object_size) {
    request[request_id].object_block_id[block_id] = true;

    // 检查是否所有块都已读取
    bool all_blocks_read = true;
    for (int i = 1; i <= object_size; i++) {
        if (!request[request_id].object_block_id[i]) {
            all_blocks_read = false;
            break;
        }
    }

    // 如果所有块都已读取，标记请求为完成
    if (all_blocks_read) {
        request[request_id].is_done = true;
    }
}

// 最早的请求
int get_earliest_request(int object_id)
{
    if (object[object_id].request_list.empty())
    {
        return 0;
    }
    int current_id = object[object_id].request_list.front();
    return current_id;
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

// 磁头是pass还是read,磁头disk_id,PASS表示pass,READ表示read,0表示不执行
int pass_or_read(int disk_id, int &n_request_complete, std::vector<int> &request_complete)
{
    int obj_id = disk[disk_id][disk_point[disk_id]];
    if (obj_id == 0)
    {
        return PASS;
    }

    // 获取对象的最早请求
    if (object[obj_id].request_list.empty())
    {
        return PASS; // 没有未完成的请求
    }
    int request_id = object[obj_id].request_list.front();

    // 找到当前块在对象中的位置
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

    if (block_id > 0)
    {
        // 更新请求状态
        update_request_status(request_id, block_id, object[obj_id].size);

        // 如果请求完成，移出请求队列
        if (request[request_id].is_done)
        {
            n_request_complete++;
            request_complete.push_back(request_id);
            object[obj_id].request_list.pop_front();
        }
    }

    return READ;
}



// 磁头移动,磁头disk_id表示第disk_id个磁头
void disk_move(int disk_id, int &n_request_complete, std::vector<int> &request_complete)
{
    int left_G = G; // 剩余令牌数

    // Step 1: 尝试执行 Jump 动作
    int jump = jump_to(disk_id);
    if (jump)
    {
        printf("j %d\n", jump);
        disk_point[disk_id] = jump; // 更新磁头位置
        disk_pre_move[disk_id] = JUMP;
        disk_pre_token[disk_id] = G; // Jump 消耗所有令牌
        return;
    }

    // Step 2: 循环处理 Pass 和 Read 动作，直到令牌耗尽
    while (left_G > 0)
    {
        
        int obj_id = disk[disk_id][disk_point[disk_id]];
        if (obj_id == 0||read_consume(disk_id) > left_G)
        {
            // 磁头指向空闲区域，执行 Pass 动作
            printf("p");
            left_G--;              // Pass 消耗 1 个令牌
            disk_point[disk_id]++; // 磁头移动到下一个存储单元
            if (disk_point[disk_id] > V)
            {
                disk_point[disk_id] = 1; // 磁头循环回到起点
            }
            disk_pre_move[disk_id] = PASS;
            disk_pre_token[disk_id] = 1;
        }
        else
        {
            // 磁头指向对象，执行 Read 动作
            int action = pass_or_read(disk_id, n_request_complete, request_complete);
            if (action == PASS)
            {
                // 执行 Pass 动作
                printf("p");
                left_G--;              // Pass 消耗 1 个令牌
                disk_point[disk_id]++; // 磁头移动到下一个存储单元
                if (disk_point[disk_id] > V)
                {
                    disk_point[disk_id] = 1; // 磁头循环回到起点
                }
                disk_pre_move[disk_id] = PASS;
                disk_pre_token[disk_id] = 1;
            }
            else if (action == READ)
            {
                // 执行 Read 动作
                printf("r");
                
                int consume = read_consume(disk_id); // 计算 Read 动作的令牌消耗
                left_G -= consume;                   // 减少剩余令牌
                disk_point[disk_id]++;               // 磁头移动到下一个存储单元
                if (disk_point[disk_id] > V)
                {
                    disk_point[disk_id] = 1; // 磁头循环回到起点
                }
                disk_pre_move[disk_id] = READ;
                disk_pre_token[disk_id] = consume;
            }
        }
        
    }
    
    printf("#\n");

}
void initialize_request(int request_id, int object_id, int object_size) {
    request[request_id].object_id = object_id;
    request[request_id].is_done = false;
    for (int i = 1; i <= object_size; i++) {
        request[request_id].object_block_id[i] = false;
    }
    object[object_id].request_list.push_back(request_id);
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

    int n_rsp=0;
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
            free(obj.unit[i]);
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

    // 初始化tail_empty
    for (int i = 1; i <= N; i++)
    {
        int count = 0;
        for (int j = V; j >= 1; j--)
        {
            if (disk[i][j] == 0)
                count++;
            else
                break;
        }
        tail_empty[i] = count;
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