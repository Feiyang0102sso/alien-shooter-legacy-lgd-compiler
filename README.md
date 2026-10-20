# Legacy LGD Compiler

[English](#english) | [中文](#中文)

## English

Compiles `.lgc` scripts into **Legacy LGD**, the lgd format shipped with Sigma's old PC engines.

### Usage

Double-click the `.bat` to compile automatically.

### Targets

This repository provides compilers for four variants: **AS1 / AS2 / AS2-Addon / ZS2**.

> [!WARNING]
>
> Only the **AS1** version is recommended. The AS2-generation engines (AS2 / AS2-Addon / ZS2) have a loader bug: after reading an lgd they never rebuild the symbol hashes, so not even `main` can be found and the script never runs.

### Documentation

See [docs/legacy_lgd_doc_EN.md](docs/legacy_lgd_doc_EN.md) for the file format.

### Credits

* **[Roman](https://github.com/Romalu86)** — thanks for his Alien Shooter series decompilation project. This project uses part of the code from those repositories; see the corresponding `vendor` folders.
* **[Tachyon Project](https://github.com/TachyonProject0099)** — thanks for discovering the Crazy Launch lgd and for a lot of early research, including the binary template and 010 Editor scripts.

---

## 中文

把 `.lgc` 脚本编译成 **Legacy LGD**，即 Sigma PC 老引擎自带的 lgd 格式。

### 用法

双击 `.bat` 自动编译。

### 目标

本仓库提供 **AS1 / AS2 / AS2-Addon / ZS2** 这四种变体的编译器。

> [!WARNING]
>
> 仅建议使用 **AS1** 版本。AS2 一代的引擎（AS2 / AS2-Addon / ZS2）加载器有缺陷：读入 lgd 后不会重建符号哈希，连 `main` 都找不到，脚本根本不会运行。

### 文档

格式说明见 [docs/legacy_lgd_doc_CN.md](docs/legacy_lgd_doc_CN.md)。

### 致谢

* **[Roman](https://github.com/Romalu86)**：感谢他的 Alien Shooter 系列反编译项目。本项目取用了对应仓库的部分代码，参见对应的 `vendor` 文件夹。
* **[Tachyon Project](https://github.com/TachyonProject0099)**：感谢他发现了 Crazy Launch 的 lgd，以及诸多前期研究，包括 binary template 和 010 Editor 脚本。
