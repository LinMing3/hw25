#include <cstdio>
#include <cassert>
#include <cstdlib>

#define MAX_DISK_NUM (10 + 1)          // 磁盘数
#define MAX_DISK_SIZE (16384 + 1)      // 磁盘大小
#define MAX_REQUEST_NUM (30000000 + 1) // 请求数
#define MAX_OBJECT_NUM (100000 + 1)    // 对象数
#define REP_NUM (3)                    // 副本数
#define FRE_PER_SLICING (1800)         // 每个切片的频率
#define EXTRA_TIME (105)               // 额外时间

// 请求结构体
typedef struct Request_
{
    int object_id; // 请求的对象id
    int prev_id;   // 当前对象的前一个请求的id
    bool is_done;  // 是否完成

} Request;

// TODO:实现标签tag的管理
// 对象结构体
typedef struct Object_
{
    int replica[REP_NUM + 1]; // 副本,存储该副本在哪个磁盘
    int *unit[REP_NUM + 1];   // 对象块,存储该块在磁盘的位置
    int size;                 // 大小
    int tag;                  // 标签
    int last_request_point;   // 最后请求的点
    bool is_delete;           // 是否删除
} Object;

// 请求数组
Request request[MAX_REQUEST_NUM];
// 对象数组
Object object[MAX_OBJECT_NUM];

// T：代表本次数据有𝑇 + 105个时间片，后续输入第二阶段将循环交互𝑇 + 105次。 时间片编号为1 ~𝑇 + 105。输入数据保证1≤𝑇≤86400。对于第𝑇 + 1 ~𝑇 + 105个时间分片，输入数据 保证没有删除、写入和读取请求。
// • M：代表对象标签数。对象标签编号为1 ~𝑀。输入数据保证1≤𝑀≤16。
// • N：代表存储系统中硬盘的个数，硬盘编号为1 ~𝑁。输入数据保证3≤𝑁 ≤10。
// • V：代表存储系统中每个硬盘的存储单元个数。存储单元编号为1 ~𝑉。输入数据保证1≤𝑉≤ 16384，任何时间存储系统中空余的存储单元数占总存储单元数的至少10 %。
// • G：代表每个磁头每个时间片最多消耗的令牌数。输入数据保证64≤𝐺 ≤1000。
int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘
int disk_point[MAX_DISK_NUM];          // 磁头

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

// 删除操作
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
        { // 删除副本j
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
//TODO: 优化写入算法
void do_object_write(int *object_unit, int *disk_unit, int size, int object_id)
{
    // 当前写入点
    int current_write_point = 0;
    // 遍历磁盘
    for (int i = 1; i <= V; i++)
    {
        // 磁盘单元为空
        if (disk_unit[i] == 0)
        {
            // 写入对象块
            disk_unit[i] = object_id;
            // 将该unit的位置存储到unit中
            object_unit[++current_write_point] = i;
            if (current_write_point == size)
            {
                break;
            }
        }
    }

    assert(current_write_point == size);
}

// 写入操作
// n_write：代表这一时间片写入对象的个数。输入数据保证总写入次数小于等于100000。
// 接下来n_write 行，每行三个数obj_id[i]、obj_size[i]、obj_tag[i]，代表当前时间片写入的对象编号，对象大小，对象标签编号。输入数据保证obj_id 为1开始每次递增1的整数，且1≤𝑜𝑏𝑗_𝑠𝑖𝑧𝑒[𝑖]≤5， 1≤𝑜𝑏𝑗_𝑡𝑎𝑔 [𝑖]≤𝑀
// 输出包含4∗𝑛_𝑤𝑟𝑖𝑡𝑒行，每4行代表一个对象：
// 第一行一个整数obj_id[i]，表示该对象的对象编号。
// 接下来一行，第一个整数rep[1] 表示该对象的第一个副本写入的硬盘编号，接下来对象大小(obj_size) 个整数unit[1][j]，代表第一个副本第𝑗个对象块写入的存储单元编号。
// 第三行，第四行格式与第二行相同，为写入第二，第三个副本的结果。
    void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++)
    {
        int id, size, tag;
        scanf("%d%d%%d", &id, &size,&tag);
        object[id].last_request_point = 0;
        for (int j = 1; j <= REP_NUM; j++)
        {   //副本j存储在磁盘(id+j)%N+1
            object[id].replica[j] = (id + j) % N + 1;
            // 分配存储空间,unit[j]现在指向一块可以存储(size + 1)个整数的内存区域
            object[id].unit[j] = static_cast<int *>(malloc(sizeof(int) * (size + 1)));
            object[id].size = size;
            object[id].tag = tag;
            object[id].is_delete = false;
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
        }

        printf("%d\n", id);
        for (int j = 1; j <= REP_NUM; j++)//输出副本j的存储情况
        {
            printf("%d", object[id].replica[j]);//输出副本j存储在哪个磁盘
            for (int k = 1; k <= size; k++)     // 输出副本j的unit[j][k],副本j第k个对象块写入的存储单元编号
            {
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

// 读取操作
// n_read：代表这一时间片读取对象的个数。输入数据保证总读取次数小于等于30000000。
// 接下来n_read 行，每行两个数req_id[i]、obj_id[i]，代表当前时间片读取的请求编号和请求的对象编号。输入数据保证读请求编号为1 开始每次递增1 的整数，读取的对象在请求到来的时刻一定在存储系统中
// 
// 前𝑁行是磁头的运动输出，第𝑖行action[i] 代表编号为𝑖的硬盘所对应的磁头的运动方式：
//  1. 该磁头执行了 "Jump"动作：这一行输出空格隔开的两个字符串，第一个字符串固定为"j"；第 二个字符串为一个整数，表示跳跃到的存储单元编号。
//  2. 该磁头没有执行 "Jump"动作：这一行输出一个字符串，仅包含字符 'p'、 'r'、 '#'，代表该磁头 在当前时间片的所有动作，每一个字符代表一个动作。其中 'p'字符代表 "Pass"动作， 'r'字符代表 "Read"动作。运动结束用字符 '#'表示。 
//  n_rsp：代表当前时间片上报读取完成的请求个数。 
// 接下来n_rsp 行，每行1 个数req_id[i]，代表本时间片上报读取完成的读取请求编号。 
// TODO: 优化读取算法
void read_action()
{
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++)
    {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;//请求request_id请求的对象id
        request[request_id].prev_id = object[object_id].last_request_point;//请求request_id请求的对象的前一个请求的id
        object[object_id].last_request_point = request_id;//对象object_id的最近请求id
        request[request_id].is_done = false;//请求request_id是否完成
    }

    static int current_request = 0;
    static int current_phase = 0;
    // 这意味着如果当前没有正在处理的请求且有新的读取请求，则开始处理新的请求。
    if (!current_request && n_read > 0) 
    {
        current_request = request_id;
    }
    // 如果当前没有正在处理的请求且没有新的读取请求，则输出N行“#”和一个“0”。
    if (!current_request)
    {
        for (int i = 1; i <= N; i++)
        {
            printf("#\n");
        }
        printf("0\n");
    }
    // 如果当前有正在处理的请求，则根据请求的处理阶段进行处理。
    else
    {
        current_phase++;
        object_id = request[current_request].object_id;//请求current_request请求的对象id
        for (int i = 1; i <= N; i++)//遍历磁盘
        {
            if (i == object[object_id].replica[1])//如果磁盘i是对象object_id的第一个副本存储的磁盘
            {
                if (current_phase % 2 == 1)
                {
                    printf("j %d\n", object[object_id].unit[1][current_phase / 2 + 1]);
                }
                else
                {
                    printf("r#\n");
                }
            }
            else
            {
                printf("#\n");
            }
        }

        // 如果当前处理阶段为对象的大小的两倍，则输出“1”和请求的编号，并将请求标记为已完成。
        if (current_phase == object[object_id].size * 2)
        {
            if (object[object_id].is_delete)
            {
                printf("0\n");
            }
            else
            {
                printf("1\n%d\n", current_request);
                request[current_request].is_done = true;
            }
            current_request = 0;
            current_phase = 0;
        }
        else
        {
            printf("0\n");
        }
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

    // 前𝑚行中，第𝑖行第𝑗个数𝑓𝑟𝑒_𝑑𝑒𝑙[𝑖][𝑗]表示时间片编号𝑖𝑑满足 (𝑗−1)∗1800+1≤𝑖𝑑 ≤𝑗∗1800的情况下，所有删除操作中对象标签为𝑖的对象大小之和。 for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }
    //接下来𝑚行，第𝑖行第𝑗个数𝑓𝑟𝑒_𝑤𝑟𝑖𝑡𝑒[𝑖][𝑗] 表示时间片编号𝑖𝑑满足(𝑗−1)∗1800 + 1≤𝑖𝑑 ≤𝑗∗ 1800的情况下，所有写入操作中对象标签为𝑖的对象大小之和。 for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }
    //接下来𝑚行，第𝑖行第𝑗个数𝑓𝑟𝑒_𝑟𝑒𝑎𝑑[𝑖][𝑗] 表示时间片编号𝑖𝑑满足(𝑗−1)∗1800 + 1≤𝑖𝑑 ≤𝑗∗ 1800的情况下，所有读取操作中对象标签为𝑖的对象大小之和，同一个对象的多次读取会重复 计算。 for (int i = 1; i <= M; i++)
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