#!/bin/bash

# ========================================
# 彩色ASCII视频转换器启动脚本
# 功能：自动检测系统、安装依赖、编译程序、转换视频并播放
# 作者：miku-01-hein
# 版本：2.0
# ========================================

# 彩色输出函数定义
# 使用ANSI转义码为终端输出添加颜色，提高可读性
RED='\033[0;31m'       # 红色：用于错误信息
GREEN='\033[0;32m'     # 绿色：用于成功信息
YELLOW='\033[1;33m'    # 黄色：用于警告信息
BLUE='\033[0;34m'      # 蓝色：用于普通信息
NC='\033[0m'           # 无颜色：重置终端颜色

# 打印信息函数：输出绿色信息文本，用于普通状态提示
print_info() {
    echo -e "${NC}[${GREEN} OK ${NC}] $1"
}

# 打印成功函数：输出蓝色成功文本，用于操作成功提示
print_success() {
    echo -e "${NC}[${BLUE} SUCCESS ${NC}] $1"
}

# 打印警告函数：输出黄色警告文本，用于需要注意但非错误的情况
print_warning() {
    echo -e "${NC}[${YELLOW} WARNING ${NC}] $1"
}

# 打印错误函数：输出红色错误文本，用于操作失败或严重问题
print_error() {
    echo -e "${NC}[${RED} ERROR ${NC}] $1"
}

# 检测系统类型函数
# 通过检查系统文件或使用lsb_release命令确定当前Linux发行版
detect_os() {
    # 首先检查/etc/os-release文件（大多数现代Linux发行版都有此文件）
    if [ -f /etc/os-release ]; then
        # 加载文件中的变量，获取操作系统信息
        . /etc/os-release
        OS=$ID          # 系统ID（如：arch, ubuntu, fedora等）
        OS_NAME=$NAME   # 系统全名
    # 如果没有os-release文件，尝试使用lsb_release命令
    elif type lsb_release >/dev/null 2>&1; then
        # 使用lsb_release命令获取系统信息
        OS=$(lsb_release -si | tr '[:upper:]' '[:lower:]')  # 系统ID（转为小写）
        OS_NAME=$(lsb_release -sd)                          # 系统描述
    else
        # 如果都无法检测，则报错退出
        print_error "无法检测操作系统类型"
        exit 1
    fi

    # 显示检测到的操作系统信息
    print_info "检测到操作系统: $OS_NAME"
}

# 安装依赖函数
# 根据检测到的操作系统类型安装相应的依赖包
install_dependencies() {
    print_info "检查并安装必要的依赖..."

    # 根据操作系统类型设置包管理器命令
    case $OS in
        ubuntu|debian|linuxmint)
            # Debian系发行版（Ubuntu、Debian、Linux Mint等）
            PKG_MANAGER="apt-get"                              # 包管理器命令
            PKG_UPDATE="sudo $PKG_MANAGER update"              # 更新包列表命令
            PKG_INSTALL="sudo $PKG_MANAGER install -y"         # 安装包命令（-y自动确认）
            PKGS="g++ pkg-config libopencv-dev mpv"           # 需要安装的包列表
            ;;
        fedora|centos|rhel|rocky)
            # RedHat系发行版（Fedora、CentOS、RHEL、Rocky Linux等）
            if command -v dnf >/dev/null 2>&1; then
                PKG_MANAGER="dnf"                              # Fedora使用dnf
            else
                PKG_MANAGER="yum"                              # 老版本使用yum
            fi
            PKG_UPDATE="sudo $PKG_MANAGER update -y"           # 更新包列表命令
            PKG_INSTALL="sudo $PKG_MANAGER install -y"         # 安装包命令
            PKGS="gcc-c++ pkg-config opencv-devel mpv"        # 需要安装的包列表
            ;;
        arch|manjaro)
            # Arch系发行版（Arch Linux、Manjaro等）
            PKG_MANAGER="pacman"                               # 包管理器命令
            PKG_UPDATE="sudo $PKG_MANAGER -Sy"                 # -S同步数据库，-y下载最新数据库
            PKG_INSTALL="sudo $PKG_MANAGER -S --noconfirm"     # --noconfirm自动确认安装
            PKGS="gcc pkg-config opencv mpv"                  # 需要安装的包列表
            ;;
        *)
            # 不支持的发行版
            print_error "不支持的Linux发行版: $OS"
            print_info "请手动安装以下依赖: g++, pkg-config, opencv, mpv"
            exit 1
            ;;
    esac

    # 更新包管理器数据库，确保获取最新的包信息
    print_info "更新包列表..."
    $PKG_UPDATE

    # 安装所需的所有依赖包
    print_info "安装依赖: $PKGS"
    $PKG_INSTALL $PKGS

    # 检查安装是否成功
    if [ $? -eq 0 ]; then
        print_success "依赖安装完成"
    else
        print_error "依赖安装失败"
        exit 1
    fi
}

# 检查命令是否存在函数
# 验证指定命令是否在系统PATH中可用
check_command() {
    if ! command -v $1 &> /dev/null; then
        # 命令不存在，返回错误
        print_error "$1 未安装"
        return 1
    else
        # 命令存在，显示命令路径
        print_info "$1 已安装: $(command -v $1)"
        return 0
    fi
}

# 检查OpenCV开发库函数
# 验证OpenCV开发库是否已安装并可被pkg-config识别
check_opencv() {
    # 首先检查opencv4（OpenCV 4.x版本）
    if ! pkg-config --exists opencv4; then
        # 如果没有opencv4，检查opencv（OpenCV 3.x或更早版本）
        if ! pkg-config --exists opencv; then
            # 都没有找到，返回错误
            print_error "OpenCV开发库未找到"
            return 1
        else
            # 找到opencv（旧版本）
            print_info "找到OpenCV库 (opencv)"
            return 0
        fi
    else
        # 找到opencv4（新版本）
        print_info "找到OpenCV库 (opencv4)"
        return 0
    fi
}

# 主函数：脚本的主要执行流程
main() {
    # 显示脚本开始执行的提示
    print_info "开始执行ASCII视频转换脚本"
    echo "========================================"

    # 第一步：检测操作系统类型
    detect_os

    # 第二步：检查系统依赖
    print_info "检查系统依赖..."

    # 初始化是否需要安装依赖的标志
    NEED_INSTALL=false

    # 检查g++编译器是否存在
    if ! check_command g++; then
        NEED_INSTALL=true
    fi

    # 检查pkg-config工具是否存在
    if ! check_command pkg-config; then
        NEED_INSTALL=true
    fi

    # 检查OpenCV开发库是否存在
    if ! check_opencv; then
        NEED_INSTALL=true
    fi

    # 检查mpv播放器是否存在
    if ! check_command mpv; then
        print_warning "mpv未安装，将自动安装"
        NEED_INSTALL=true
    fi

    # 第三步：如果需要安装依赖，则执行安装
    if [ "$NEED_INSTALL" = true ]; then
        install_dependencies
    else
        print_success "所有依赖已满足"
    fi

    # 第四步：再次验证OpenCV是否可用
    if ! check_opencv; then
        print_error "OpenCV开发库安装失败，请手动安装"
        exit 1
    fi

    echo "========================================"
    print_info "开始编译程序..."

    # 第五步：编译C++程序
    # 显示编译命令给用户看
    print_info "执行编译命令: g++ -O3 -march=native -std=c++17 -o miku miku.cpp \`pkg-config --cflags --libs opencv4\` -lpthread"

    # 根据OpenCV版本选择不同的编译命令
    if pkg-config --exists opencv4; then
        # 使用OpenCV 4.x版本编译
        # 编译参数说明：
        # -O3：最高级别的编译优化，提高程序运行速度
        # -march=native：为当前CPU架构优化，利用所有可用的CPU特性
        # -std=c++17：使用C++17标准编译
        # -o miku：指定输出文件名为miku
        # `pkg-config --cflags --libs opencv4`：自动获取OpenCV的编译和链接参数
        # -lpthread：链接POSIX线程库，支持多线程
        g++ -O3 -march=native -std=c++17 -o miku miku.cpp `pkg-config --cflags --libs opencv4` -lpthread
    else
        # 使用OpenCV 3.x或更早版本编译
        print_warning "未找到opencv4，尝试使用opencv"
        g++ -O3 -march=native -std=c++17 -o miku miku.cpp `pkg-config --cflags --libs opencv` -lpthread
    fi

    # 第六步：检查编译是否成功
    if [ $? -eq 0 ] && [ -f "miku" ]; then
        print_success "编译成功！生成可执行文件: miku"
    else
        # 编译失败，显示错误信息
        print_error "编译失败！"
        print_info "请检查:"
        print_info "1. miku.cpp 文件是否存在"
        print_info "2. OpenCV开发库是否正确安装"
        print_info "3. 编译错误信息"
        exit 1
    fi

    echo "========================================"
    print_info "运行ASCII视频转换..."

    # 第七步：运行视频转换程序
    if [ -f "miku.mp4" ]; then
        # 显示转换命令
        print_info "执行转换命令: ./miku miku.mp4 ascii.mp4 150 1.5"
        # 执行转换命令，参数说明：
        # miku.mp4：输入视频文件
        # ascii.mp4：输出ASCII视频文件
        # 150：ASCII宽度（每行150个字符）
        # 1.5：质量参数
        ./miku miku.mp4 ascii.mp4 150 1.5

        # 检查转换是否成功
        if [ $? -eq 0 ] && [ -f "ascii.mp4" ]; then
            print_success "视频转换完成！"
        else
            print_error "视频转换失败！"
            exit 1
        fi
    else
        print_error "输入视频文件 miku.mp4 不存在"
        print_info "请确保 miku.mp4 文件在当前目录"
        exit 1
    fi

    echo "========================================"
    print_info "启动视频播放器..."

    # 第八步：播放视频
    # 同时播放原始视频和ASCII视频，方便对比
    print_info "执行播放命令: mpv miku.mp4 & mpv ascii.mp4"

    # 启动mpv播放器播放两个视频
    # & 符号表示在后台运行命令
    mpv miku.mp4 & mpv ascii.mp4
    MPV1_PID=$!  # 获取第一个mpv进程的PID

    # 等待1秒，让播放器有时间启动
    sleep 1

    # 检查进程是否在运行
    # kill -0 命令用于检查进程是否存在，不发送任何信号
    if kill -0 $MPV1_PID 2>/dev/null; then
        print_success "两个视频播放器已启动"
        print_info "原始视频 PID: $MPV1_PID"
        print_info "ASCII视频 PID: $MPV2_PID"
    else
        print_warning "部分播放器可能启动失败"
    fi

    echo "========================================"
    print_success "所有步骤执行完成！"
    print_info "脚本执行完毕"
}

# 执行主函数，传递所有命令行参数
main "$@"

# ========================================
# 脚本使用说明：
# 1. 确保脚本有执行权限：chmod +x start.sh
# 2. 确保当前目录有miku.cpp和miku.mp4文件
# 3. 运行脚本：./start.sh
#
# 脚本执行流程：
# 1. 检测操作系统类型
# 2. 检查并安装必要依赖
# 3. 编译C++程序
# 4. 转换视频为ASCII艺术风格
# 5. 同时播放原始视频和转换后的视频
#
# 注意事项：
# 1. 需要管理员权限安装依赖，脚本会提示输入密码
# 2. 视频转换可能需要较长时间，取决于视频大小和系统性能
# 3. 按q键可以退出mpv播放器
# 4. 如果转换失败，请检查编译错误信息和系统日志
# ========================================
