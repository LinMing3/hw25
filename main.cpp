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
    int prev_id;   // 前一个请求的id
    bool is_done;  // 是否完成
} Request;

// 对象结构体
typedef struct Object_
{
    int replica[REP_NUM + 1]; // 副本
    int *unit[REP_NUM + 1];   // 对象块
    int size;                 // 大小
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
// object_unit:对象块
// disk_unit:磁盘指针
// size:大小
// object_id:对象id
void do_object_write(int *object_unit, int *disk_unit, int size, int object_id)
{
    // 当前写入点
    int current_write_point = 0;
    // 遍历磁盘
    for (int i = 1; i <= V; i++)
    {
        if (disk_unit[i] == 0)
        {
            disk_unit[i] = object_id;
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
void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++)
    {
        int id, size;
        scanf("%d%d%*d", &id, &size);
        object[id].last_request_point = 0;
        for (int j = 1; j <= REP_NUM; j++)
        {
            object[id].replica[j] = (id + j) % N + 1;
            object[id].unit[j] = static_cast<int *>(malloc(sizeof(int) * (size + 1)));
            object[id].size = size;
            object[id].is_delete = false;
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
        }

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

void read_action()
{
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++)
    {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
    }

    static int current_request = 0;
    static int current_phase = 0;
    if (!current_request && n_read > 0)
    {
        current_request = request_id;
    }
    if (!current_request)
    {
        for (int i = 1; i <= N; i++)
        {
            printf("#\n");
        }
        printf("0\n");
    }
    else
    {
        current_phase++;
        object_id = request[current_request].object_id;
        for (int i = 1; i <= N; i++)
        {
            if (i == object[object_id].replica[1])
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
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++)
    {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++)
        {
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    for (int i = 1; i <= N; i++)
    {
        disk_point[i] = 1;
    }

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