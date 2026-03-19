RLE Dumper
==========

将 RLE 文件解析为 BMP 图片。纯透明的图片和全透明的 rle 文件会被自动跳过。
输出按文件名前3字母分组: prefix/stem/00000.bmp
每个 .rle 文件生成一个同名文件夹, 里面是 00000.bmp, 00001.bmp, ...

两个版本功能一致, 任选其一。

Python 版
---------
无第三方依赖, 需要 Python 3.6+ (https://www.python.org/)

  python rle_dump.py <文件夹> [--ext 后缀名]
  python rle_dump.py data\images --ext .rle

C 版
----
单个文件, 无第三方依赖, 速度快约 70 倍。

编译环境:
  macOS / Linux: 系统自带 gcc 或通过包管理器安装 (brew install gcc / apt install gcc)

编译:
  gcc -O2 -o rle_dump rle_dump.c                    (macOS / Linux)

运行:
  ./rle_dump <文件夹> [后缀名]                         (macOS / Linux)
  rle_dump.exe <文件夹> [后缀名]                       (Windows)

示例:
  ./rle_dump data/images .rle                         (macOS / Linux)
  rle_dump.exe data\images .rle                       (Windows)

Windows 编译 (Visual Studio Community)
--------------------------------------
下载地址: https://visualstudio.microsoft.com/zh-hans/vs/community/

  1. 下载并运行安装程序
  2. 在工作负荷页面勾选"使用 C++ 的桌面开发", 点击安装
  3. 安装完成后, 打开开始菜单, 找到 Developer Command Prompt for VS 2022 (或对应版本)
  4. 在命令行中 cd 到 rle_dump.c 所在目录:
       cd D:\RleParser
       cl /O2 rle_dump.c
       rle_dump.exe data\images .rle

  注意: cl 是 Visual Studio 自带的 C/C++ 编译器, 只能在 Developer Command Prompt 中使用,
  普通 cmd 找不到该命令。
