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
- [ ] main.cpp
- [ ] cmakelist.txt

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
- [ ] 安装todotree插件
- [ ] 分任务
- [ ] 第一次提交