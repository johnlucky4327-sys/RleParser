RLE Dumper
==========

将 RLE 文件解析为 BMP 图片。纯透明的图片和全透明的 rle 文件会被自动跳过。
输入 RLEs\Int，输出 RLEs\Int_dump，目录结构保持一致。
每个 .rle 文件生成一个同名文件夹，里面是 00000.bmp, 00001.bmp, ...

两个版本功能一致，任选其一。

Python 版
---------
无第三方依赖，需要 Python 3.6+

  python rle_dump.py <文件夹> [--ext 后缀名]
  python rle_dump.py data\images --ext .rle

C 版
----
单文件，无第三方依赖，速度快约 70 倍。

编译环境:
  macOS / Linux: 系统自带 gcc 或通过包管理器安装 (brew install gcc / apt install gcc)
  Windows: 安装 Visual Studio Community (免费)，安装时勾选"使用 C++ 的桌面开发"

编译:
  gcc -O2 -o rle_dump rle_dump.c                    (macOS / Linux)
  cl /O2 rle_dump.c                                 (Windows，在 Developer Command Prompt for VS 中执行)

运行:
  ./rle_dump <文件夹> [后缀名]                         (macOS / Linux)
  rle_dump.exe <文件夹> [后缀名]                       (Windows)

示例:
  ./rle_dump data/images .rle                         (macOS / Linux)
  rle_dump.exe data\images .rle                       (Windows)
