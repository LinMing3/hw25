#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cmath>     // 包含 ceil 函数
#include <algorithm> // 包含 std::max 函数
#include <deque>    // 包含 std::deque

using namespace std;

#define MAX_DISK_NUM (10 + 1)          // 磁盘数
#define MAX_DISK_SIZE (16384 + 1)      // 磁盘大小
#define MAX_REQUEST_NUM (30000000 + 1) // 请求数
#define MAX_OBJECT_NUM (100000 + 1)    // 对象数
#define REP_NUM (3)                    // 副本数
#define FRE_PER_SLICING (1800)         // 每个切片的频率
#define EXTRA_TIME (105)               // 额外时间
const int OBJECT_BLOCKS = 5;


#define PASS 1
#define READ 2
#define JUMP 3

// 请求结构体
typedef struct Request_
{
    int object_id; // 请求的对象id
    int prev_id;   // 当前对象的前一个请求的id
    bool is_done;  // 是否完成
    bool object_block_id[OBJECT_BLOCKS + 1]; // 对象块id
} Request;

// TODO:实现标签tag的管理
// 对象结构体
typedef struct Object_
{
    int replica[REP_NUM + 1]; // 副本,存储该副本在哪个磁盘,replica[j]表示副本j存储在哪个磁盘
    int *unit[REP_NUM + 1];   // 对象块,存储该块在磁盘的位置,unit[j][k]表示副本j的第k个对象块存储的磁盘单元位置
    std:: deque<int> request_list;   //当前对象的读取请求,记录每个对象块的待读取次数,read_count[i]表示第i个request的id
    int size;                 // 大小
    int tag;                  // 标签
    int last_request_point;   // 最近请求id
    bool is_delete;           // 是否删除

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

struct BestThree
{
    int best1;
    int best2;
    int best3;
};
BestThree find_best_disks_for_tag(int tag)
{
    BestThree best_disks = {0, 0, 0};
    int main_disk = tag_disk_map[tag]; // 该标签优先存放的磁盘

    int candidate_disks[REP_NUM]; // 存储候选磁盘
    candidate_disks[0] = main_disk;
    candidate_disks[1] = (main_disk == N) ? 1 : (main_disk + 1);
    candidate_disks[2] = (main_disk == 1) ? N : (main_disk - 1); 
    // 相邻磁盘（顺/逆时针）

    // **Step 1: 在候选磁盘中寻找负载最轻的 3 个磁盘**
    for (int i = 0; i < REP_NUM; i++)
    {
        int disk_id = candidate_disks[i];

        //TODO: 优化选择磁盘的策略
        if (disk_id < 1 || disk_id > N) {//不要continue,否则会导致best_disks.best1=0,先暂时放在1号磁盘
            disk_id = 1;
        }

        if (best_disks.best1 == 0 || current_occupation[disk_id] < current_occupation[best_disks.best1])
        {
            best_disks.best3 = best_disks.best2;
            best_disks.best2 = best_disks.best1;
            best_disks.best1 = disk_id;
        }
        else if (best_disks.best2 == 0 || current_occupation[disk_id] < current_occupation[best_disks.best2])
        {
            best_disks.best3 = best_disks.best2;
            best_disks.best2 = disk_id;
        }
        else if (best_disks.best3 == 0 || current_occupation[disk_id] < current_occupation[best_disks.best3])
        {
            best_disks.best3 = disk_id;
        }
    }

    if (best_disks.best1 == 0 || best_disks.best2 == 0 || best_disks.best3 == 0) {
        printf("Error: Invalid disk selection! best1: %d, best2: %d, best3: %d\n",
               best_disks.best1, best_disks.best2, best_disks.best3);
        exit(1);
    }

    return best_disks;
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
    int abort_num = 0;              // 取消读取数
    static int _id[MAX_OBJECT_NUM]; // 对象id

    // 读取删除数
    scanf("%d", &n_delete);
    // 读取删除对象id
    for (int i = 1; i <= n_delete; i++)
    {
        scanf("%d", &_id[i]);
    }

    // 取消读取数
    for (int i = 1; i <= n_delete; i++)
    {
        int id = _id[i];
        int current_id = object[id].last_request_point;
        while (current_id != 0)
        {
            if (request[current_id].is_done == false)
            {
                abort_num++;
            }
            // 当前对象的前一个请求id
            current_id = request[current_id].prev_id;
        }
    }

    // 打印取消读取数
    printf("%d\n", abort_num);
    for (int i = 1; i <= n_delete; i++)
    {
        int id = _id[i];
        // 当前对象的最近请求id
        int current_id = object[id].last_request_point;
        while (current_id != 0)
        {
            if (request[current_id].is_done == false)
            {
                printf("%d\n", current_id);
            }
            current_id = request[current_id].prev_id;
        }
        // 删除对象
        for (int j = 1; j <= REP_NUM; j++)
        {                                                                 // 删除副本j
            current_occupation[object[id].replica[j]] -= object[id].size; // 更新磁盘占用
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}

// 写入对象
// object_unit:对象块(存储该块在磁盘的位置)
// disk_unit:磁盘指针(存储该块存储的对象id)
// size:大小(对象块大小)
// object_id:对象id
// : 优化写入算法
// void do_object_write(int *object_unit, int *disk_unit, int size, int object_id)
// {

//     int current_write_point = 0; // 当前写入的对象块
//     // 遍历磁盘
//     for (int i = 1; i <= V; i++)
//     {
//         // 磁盘单元为空
//         if (disk_unit[i] == 0)
//         {
//             // 写入对象块
//             disk_unit[i] = object_id;
//             current_occupation[i] += size; // 更新磁盘占用
//             // unit[j][current_write_point]表示副本j的第current_write_point个对象块存储的磁盘单元位置
//             object_unit[++current_write_point] = i;
//             if (current_write_point == size)
//             {
//                 break;
//             }
//         }
//     }

//     assert(current_write_point == size);
// }

int find_best_fit_space(int disk_id, int size)
{
    int best_start = -1;
    int min_gap = INT_MAX;

    for (int i = 1; i <= V - size + 1; i++)
    {
        if (disk[disk_id][i] == 0) // 发现空闲块
        {
            int gap_size = 0;
            for (int j = 0; j < size; j++)
            {
                if (disk[disk_id][i + j] == 0)
                    gap_size++;
                else
                    break;
            }

            if (gap_size == size) // 完美匹配
            {
                return i;
            }
            else if (gap_size > size && gap_size < min_gap)
            {
                min_gap = gap_size;
                best_start = i;
            }
        }
    }

    return best_start; // 返回找到的最合适的存储位置
}


void do_object_write(int *object_unit, int *disk_unit, int size, int object_id, int disk_id)
{
    int best_start = find_best_fit_space(disk_id, size);

    if (best_start == -1)
    {
        printf("Error: No available space on disk %d for object %d\n", disk_id, object_id);
        exit(1);
    }

    for (int j = 0; j < size; j++)
    {
        disk_unit[best_start + j] = object_id;//disk_unit[i]表示磁盘i的第i个存储单元存储的对象id
        object_unit[j + 1] = best_start + j;
    }
}




// 写入操作
// n_write：代表这一时间片写入对象的个数。输入数据保证总写入次数小于等于100000。
// 接下来n_write 行，每行三个数obj_id[i]、obj_size[i]、obj_tag[i]，代表当前时间片写入的对象编号，对象大小，对象标签编号。输入数据保证obj_id 为1开始每次递增1的整数，且1≤𝑜𝑏𝑗_𝑠𝑖𝑧𝑒[𝑖]≤5， 1≤𝑜𝑏𝑗_𝑡𝑎𝑔 [𝑖]≤𝑀
// 输出包含4∗𝑛_𝑤𝑟𝑖𝑡𝑒行，每4行代表一个对象：
// 第一行一个整数obj_id[i]，表示该对象的对象编号。
// 接下来一行，第一个整数rep[1] 表示该对象的第一个副本写入的硬盘编号，接下来对象大小(obj_size) 个整数unit[1][j]，代表第一个副本第𝑗个对象块写入的存储单元编号。
// 第三行，第四行格式与第二行相同，为写入第二，第三个副本的结果。
// void write_action()
// {
//     int n_write;
//     scanf("%d", &n_write);
//     for (int i = 1; i <= n_write; i++)
//     {
//         int id, size, tag;
//         scanf("%d%d%%d", &id, &size, &tag);
//         object[id].last_request_point = 0;
//         object[id].tag = tag;
//         object[id].is_delete = false;
//         object[id].size = size;
//         for (int j = 1; j <= REP_NUM; j++)
//         { // 副本j存储在磁盘(id+j)%N+1
//             object[id].replica[j] = (id + j) % N + 1;
//             // 分配存储空间,unit[j]现在指向一块可以存储(size + 1)个整数的内存区域
//             object[id].unit[j] = static_cast<int *>(malloc(sizeof(int) * (size + 1)));
//             do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
//         }

//         printf("%d\n", id);
//         for (int j = 1; j <= REP_NUM; j++) // 输出副本j的存储情况
//         {
//             printf("%d", object[id].replica[j]); // 输出副本j存储在哪个磁盘
//             for (int k = 1; k <= size; k++)      // 输出副本j的unit[j][k],副本j第k个对象块写入的存储单元编号
//             {
//                 printf(" %d", object[id].unit[j][k]);
//             }
//             printf("\n");
//         }
//     }

//     fflush(stdout);
// }

void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    
    for (int i = 1; i <= n_write; i++)
    {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        object[id].last_request_point = 0;
        object[id].tag = tag;
        object[id].is_delete = false;
        object[id].size = size;

        // **Step 1: 选择最佳存储磁盘**
        BestThree best_disks = find_best_disks_for_tag(tag);
        int chosen_disks[REP_NUM] = {best_disks.best1, best_disks.best2, best_disks.best3};

        // **Step 2: 在选定磁盘中查找最佳存储单元并写入**
        for (int j = 1; j <= REP_NUM; j++)
        {
            object[id].replica[j] = chosen_disks[j - 1];
            object[id].unit[j] = static_cast<int *>(malloc(sizeof(int) * (size + 1)));

            do_object_write(object[id].unit[j], disk[chosen_disks[j - 1]], size, id, chosen_disks[j - 1]);
        }

        // **Step 3: 输出存储位置**
        printf("%d\n", id);
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




//优化读取算法

//磁头跳跃,磁头disk_id跳跃到的存储单元编号,0表示不执行jump
int jump_to(int disk_id)
{
    int current_position = disk_point[disk_id];
    int count = 0;
    int position = current_position;
    // 从当前磁头位置向前遍历磁盘存储单元
    for (int i = 1; i <= V/3; i++)
    {
        position = (current_position + i) % V; // 循环计算位置
        if (disk[disk_id][position] != 0)                  // 找到最近的非空位置
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

// 最早的请求
int get_earliest_request(int object_id)
{
    if(object[object_id].request_list.empty()){
        return 0;
    }
    int current_id = object[object_id].request_list.front();
    return current_id;
}

//磁头是pass还是read,磁头disk_id,PASS表示pass,READ表示read,0表示不执行
int pass_or_read(int disk_id,int& re_id)
{
    int obj_id = disk[disk_id][disk_point[disk_id]];
    if(obj_id==0){
        return PASS;
    }
    re_id = get_earliest_request(obj_id);
    if(re_id==0){
         return PASS;
    }
    int rep_id = 0;
    for(int i=1;i<=REP_NUM;i++){
        if(object[obj_id].replica[i]==disk_id){
            rep_id=i;
            break;
        }
    }
    if (rep_id > 0)
    {
        int obj_block_id=0;
        for(int i=1;i<=object[obj_id].size;i++){
            if(object[obj_id].unit[rep_id][i]==disk_point[disk_id]){
                obj_block_id=i;
                break;
            }
        }
        if (obj_block_id > 0)
        {
            for (int req_id : object[obj_id].request_list)
            {
                if(request[req_id].object_block_id[obj_block_id]==false){
                    request[req_id].object_block_id[obj_block_id] = true;
                    request[req_id].is_done = true;
                    for(int i=1;i<=object[obj_id].size;i++){
                        if(request[req_id].object_block_id[i]==false){
                            request[req_id].is_done = false;
                            re_id = req_id;
                            return READ;
                        }
                    }                    
                    return READ;
                }
            }
            

            
        }
    }
    return PASS;
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

// 磁头移动,磁头disk_id表示第disk_id个磁头
void disk_move(int disk_id, int &n_request_complete, int *request_complete)
{
    int left_G = G;
    int jump = jump_to(disk_id);
    if (jump)
    {
        printf("j %d\n", jump);
        disk_point[disk_id] = jump;
        disk_pre_move[disk_id] = JUMP;
        disk_pre_token[disk_id] = G;
        return;
    }
    while (left_G > 0)
    {
        int re_id = 0;
        int pass = pass_or_read(disk_id, re_id);
        if (pass == PASS)
        {
            printf("p");
            left_G--;
            disk_point[disk_id]++;
            if(disk_point[disk_id]>V){
                disk_point[disk_id]=1;
            }
            disk_pre_move[disk_id] = PASS;
            disk_pre_token[disk_id] = 1;
        }
        else if (pass == READ)
        {
            printf("r");
            if (request[re_id].is_done = true)
            {
                n_request_complete++;
                request_complete[n_request_complete]=re_id;
                object[request[re_id].object_id].request_list.pop_front();
            }
            int consume = read_consume(disk_id);
            left_G -= consume;
            disk_point[disk_id]++;
            if (disk_point[disk_id] > V)
            {
                disk_point[disk_id] = 1;
            }
            disk_pre_move[disk_id] = READ;
            disk_pre_token[disk_id] = consume;
        }
        else
        {
            printf("#\n");
            return;
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
        request[request_id].object_id = object_id;                          // 请求request_id请求的对象id
        request[request_id].prev_id = object[object_id].last_request_point; // 请求request_id请求的对象的前一个请求的id
        object[object_id].last_request_point = request_id;                  // 对象object_id的最近请求id
        request[request_id].is_done = false;                                // 请求request_id是否完成
        object[object_id].request_list.push_back(request_id);                    // 对象object_id的读取请求
    }

    int n_rsp;
    int rsp_id[MAX_REQUEST_NUM];

    // 前𝑁行是磁头的运动输出，第𝑖行action[i] 代表编号为𝑖的硬盘所对应的磁头的运动方式：
    //  1. 该磁头执行了 "Jump"动作：这一行输出空格隔开的两个字符串，第一个字符串固定为"j"；第 二个字符串为一个整数，表示跳跃到的存储单元编号。
    //  2. 该磁头没有执行 "Jump"动作：这一行输出一个字符串，仅包含字符 'p'、 'r'、 '#'，代表该磁头 在当前时间片的所有动作，每一个字符代表一个动作。其中 'p'字符代表 "Pass"动作， 'r'字符代表 "Read"动作。运动结束用字符 '#'表示。
    // 每个磁头的运动输出
    for (int i = 1; i <= N; i++)
    {
        disk_move(i, n_rsp, rsp_id);
    }

    // 读取完成的请求个数
    //   n_rsp：代表当前时间片上报读取完成的请求个数。
    //  接下来n_rsp 行，每行1 个数req_id[i]，代表本时间片上报读取完成的读取请求编号。
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