RLE Dumper
==========

将 RLE 文件解析为 BMP 图片。纯透明的图片和全透明的 rle 文件会被自动跳过。
输入 RLEs\Int，输出 RLEs\Int_dump，目录结构保持一致。
每个 .rle 文件生成一个同名文件夹，里面是 00000.bmp, 00001.bmp, ...

两个版本功能一致，任选其一。

Python 版
---------
无第三方依赖，需要 Python 3.6+ (https://www.python.org/)

  python rle_dump.py <文件夹> [--ext 后缀名]
  python rle_dump.py data\images --ext .rle

C 版
----
单文件，无第三方依赖，速度快约 70 倍。

编译环境:
  macOS / Linux: 系统自带 gcc 或通过包管理器安装 (brew install gcc / apt install gcc)
  Windows: 安装 Dev-C++ (https://github.com/royqh1979/Dev-CPP/releases)
    1. 下载最新版 Dev-Cpp.*.Setup.exe 并安装
    2. 打开 Dev-C++，菜单 文件 → 打开，选择 rle_dump.c
    3. 菜单 运行 → 编译 (或按 F9)，生成 rle_dump.exe
    4. 打开命令提示符 (cmd)，cd 到 rle_dump.exe 所在目录，执行:
       rle_dump.exe data\images .rle

  也可以直接用命令行编译 (Dev-C++ 自带 gcc):
    1. 将 Dev-C++ 安装目录下的 MinGW64\bin 添加到系统 PATH 环境变量
    2. 打开命令提示符:
       gcc -O2 -o rle_dump.exe rle_dump.c
       rle_dump.exe data\images .rle

编译:
  gcc -O2 -o rle_dump rle_dump.c                    (macOS / Linux)

运行:
  ./rle_dump <文件夹> [后缀名]                         (macOS / Linux)
  rle_dump.exe <文件夹> [后缀名]                       (Windows)

示例:
  ./rle_dump data/images .rle                         (macOS / Linux)
  rle_dump.exe data\images .rle                       (Windows)
