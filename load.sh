#!/bin/bash

SRC_DIR="./src"
BIN_DIR="../bin"

cd "$SRC_DIR"

mkdir -p "$BIN_DIR"

g++ -std=c++11 -o trace_test trace_test_load.cc -luring
if [ $? -ne 0 ]; then
    echo "Compile Error!"
    exit 1
fi
mv trace_test "$BIN_DIR/"

# 运行程序
# "$BIN_DIR/trace_test" > ./log.txt

for ((i=1; i<=10; i++))
do
    output_file="./output_$i.txt"
    echo "Running ./trace_test, output saved to $output_file"
    "$BIN_DIR/trace_test" > $output_file
done

# 如果需要传递参数，可以像下面这样使用：
# "$BIN_DIR/trace_test" 参数1 参数2 ...

# 返回到脚本所在的目录
cd -
