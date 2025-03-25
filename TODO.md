# 提交方式
压缩包.zip
格式
cmakelist.txt
main.cpp

# 配环境
- [x] vscode的C++环境
- [x] cmakelist
- [x] g++

# 需要修改
- [x] main.cpp
- [x] cmakelist.txt

# 连接主机有什么用
检验linux环境编译效果

# run.py
本地运行判题器
```
g++ demos\cpp\main.cpp -o demos\cpp\main.exe
python run.py interactor\windows\interactor.exe data\sample.in demos\cpp\main.exe
```

# debug 模式
```
python run.py [interactor_path] [data_path] [player_path] -d
```

# replay 模式
```
python run.py [interactor_path] [data_path] [player_path] -r 5 10
```

# 其他
- [x] 安装todotree插件
- [x] 分任务
- [x] 第一次提交(demo)
- [ ] 第二次提交
- [ ] 读:tag . size . 权重
- [ ] 设计数据结构
  - [ ] 存入: 分别将大小为1-5的object分为若干size为1或2的block，block可以分开存储在磁盘（但是尽量存在一起
    - [ ] 1
    - [ ] 1+1
    - [ ] 1+1+1 1+2*
    - [ ] 1+1+2 2+2*
    - [ ] 1+2+2
    - [ ] 根据存储的tag决定磁盘（即每个磁盘有对应的tag
    - [ ] 选择最接近tag，最空的三个磁盘
    - [ ] 寻找空隙插入（优先插入空隙，再插入连续大块空间
    - 补充说明: tag决定所属磁盘，再按照空隙插入相应空间
  - [x] 读取:
    - [x] 记录读取状态 ,某对象的某对象块是否被读取
    - [x] 当前磁头的读取请求
    - [x] 若其他磁盘已读取该对象,则pass;未读取则读取
    - [x] 遇到大片空白max_gap,jump至需要读取的第一个