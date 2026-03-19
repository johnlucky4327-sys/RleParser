RedMoon RLE Dumper
==================

将 RLE 文件解析为 BMP 图片。纯 Python，无第三方依赖。

用法
----
python rle_dump.py <RLE文件夹> [--ext 后缀名]

示例
----
python rle_dump.py RLEs\Int --ext .rle

输入 RLEs\Int，输出 RLEs\Int_dump，目录结构保持一致。
每个 .rle 文件生成一个同名文件夹，里面是 00000.bmp, 00001.bmp, ...
纯透明的图片和全透明的 rle 文件会被自动跳过。

要求
----
Python 3.6+
