#!/bin/bash

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
LIB_DIR="$PROJECT_ROOT/lib"
BIN_DIR="$PROJECT_ROOT/bin"

# 打印带颜色的消息
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo "WebServer 自动构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  clean     清理构建目录"
    echo "  build     构建项目"
    echo "  install   安装到系统"
    echo "  package   打包发布"
    echo "  all       执行完整构建流程 (clean + build)"
    echo ""
    echo "运行选项:"
    echo "  run       运行主程序 (webserver)"
    echo "  run-test  运行单元测试 (webserver_test)"
    echo "  run-bench 运行压力测试 (webserver_bench，带benchmark参数)"
    echo ""
    echo "帮助:"
    echo "  help      显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0 build        # 仅构建"
    echo "  $0 all          # 完整构建流程"
    echo "  $0 clean build  # 清理后构建"
    echo "  $0 run          # 运行主程序"
    echo "  $0 run-test     # 运行单元测试"
    echo "  $0 run-bench    # 运行压力测试"
}

# 检查依赖
check_dependencies() {
    print_info "检查构建依赖..."
    
    local missing_deps=()
    
    # 检查必要的命令
    local commands=("cmake" "make" "g++" "ldd")
    for cmd in "${commands[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done
    
    # 检查必要的库 
    local libs=("yaml-cpp" "jwt-cpp" "OpenSSL" "benchmark" "GTest")
    for lib in "${libs[@]}"; do
        # 尝试多种方式检查库是否存在
        if ! pkg-config --exists "$lib" 2>/dev/null && \
           ! find /usr -name "*$lib*" -type f 2>/dev/null | grep -q . && \
           ! find /usr/local -name "*$lib*" -type f 2>/dev/null | grep -q .; then
            missing_deps+=("$lib")
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_warning "可能缺少以下依赖:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        print_info "如果构建失败，请安装缺少的依赖"
        print_info "Ubuntu/Debian: sudo apt install libyaml-cpp-dev libjwt-cpp-dev libssl-dev libbenchmark-dev libgtest-dev"
        print_info "CentOS/RHEL: sudo yum install yaml-cpp-devel jwt-cpp-devel openssl-devel benchmark-devel gtest-devel"
        echo ""
    else
        print_success "所有依赖检查通过"
    fi
}

# 清理构建目录
clean_build() {
    print_info "清理构建目录..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "构建目录已清理"
    else
        print_warning "构建目录不存在，跳过清理"
    fi
    
    # 清理生成的文件
    if [ -d "$LIB_DIR" ]; then
        rm -f "$LIB_DIR"/*.so*
        print_success "库文件已清理"
    fi
    
    if [ -d "$BIN_DIR" ]; then
        rm -f "$BIN_DIR"/webserver*
        print_success "可执行文件已清理"
    fi
}

# 构建项目
build_project() {
    print_info "开始构建项目..."
    
    # 创建构建目录
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # 配置项目
    print_info "配置CMake项目..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    
    # 编译项目
    print_info "编译项目..."
    make -j$(nproc)
    
    print_success "项目构建完成"
}

# 运行测试
run_tests() {
    print_info "运行测试..."
    
    if [ ! -f "$BIN_DIR/webserver_test" ]; then
        print_error "测试程序不存在，请先构建项目"
        exit 1
    fi
    
    cd "$BIN_DIR"
    
    # 运行测试
    print_info "执行单元测试..."
    if ./webserver_test; then
        print_success "所有测试通过"
    else
        print_error "测试失败"
        exit 1
    fi
}

# 检查构建结果
check_build_result() {
    print_info "检查构建结果..."
    
    local missing_files=()
    
    # 检查动态库
    if [ ! -f "$LIB_DIR/libwebserver.so.1.0.0" ]; then
        missing_files+=("动态库")
    fi
    
    # 检查可执行文件
    if [ ! -f "$BIN_DIR/webserver" ]; then
        missing_files+=("主程序")
    fi
    
    if [ ! -f "$BIN_DIR/webserver_test" ]; then
        missing_files+=("测试程序")
    fi
    
    if [ ! -f "$BIN_DIR/webserver_bench" ]; then
        missing_files+=("基准测试程序")
    fi
    
    if [ ${#missing_files[@]} -ne 0 ]; then
        print_error "缺少以下文件:"
        for file in "${missing_files[@]}"; do
            echo "  - $file"
        done
        return 1
    fi
    
    print_success "所有文件生成成功"
    
    # 显示文件信息
    echo ""
    print_info "生成的文件:"
    echo "  动态库: $LIB_DIR/libwebserver.so.1.0.0 ($(du -h "$LIB_DIR/libwebserver.so.1.0.0" | cut -f1))"
    echo "  主程序: $BIN_DIR/webserver ($(du -h "$BIN_DIR/webserver" | cut -f1))"
    echo "  测试程序: $BIN_DIR/webserver_test ($(du -h "$BIN_DIR/webserver_test" | cut -f1))"
    echo "  基准测试: $BIN_DIR/webserver_bench ($(du -h "$BIN_DIR/webserver_bench" | cut -f1))"
}

# 安装到系统
install_system() {
    print_info "安装到系统..."
    
    # 检查权限
    if [ "$EUID" -ne 0 ]; then
        print_error "需要root权限进行系统安装"
        print_info "请使用: sudo $0 install"
        exit 1
    fi
    
    # 安装动态库
    install -m 755 "$LIB_DIR/libwebserver.so.1.0.0" /usr/local/lib/
    ln -sf libwebserver.so.1.0.0 /usr/local/lib/libwebserver.so.1
    ln -sf libwebserver.so.1 /usr/local/lib/libwebserver.so
    
    # 更新动态链接器缓存
    ldconfig
    
    # 安装可执行文件
    install -m 755 "$BIN_DIR/webserver" /usr/local/bin/
    install -m 755 "$BIN_DIR/webserver_test" /usr/local/bin/
    install -m 755 "$BIN_DIR/webserver_bench" /usr/local/bin/
    
    # 安装头文件
    mkdir -p /usr/local/include/webserver
    cp -r "$PROJECT_ROOT/include/"* /usr/local/include/webserver/
    
    print_success "安装完成"
}

# 打包发布
package_release() {
    print_info "打包发布版本..."
    
    local version="1.0.0"
    local package_name="webserver-${version}"
    local package_dir="$PROJECT_ROOT/$package_name"
    
    # 创建打包目录
    mkdir -p "$package_dir"
    
    # 复制文件
    cp -r "$LIB_DIR" "$package_dir/"
    cp -r "$BIN_DIR" "$package_dir/"
    cp -r "$PROJECT_ROOT/include" "$package_dir/"
    cp -r "$PROJECT_ROOT/configs" "$package_dir/"
    cp -r "$PROJECT_ROOT/web_static" "$package_dir/"
    cp "$PROJECT_ROOT/README.md" "$package_dir/"
    
    # 创建安装脚本
    cat > "$package_dir/install.sh" << 'EOF'
#!/bin/bash
set -e

# 安装动态库
sudo install -m 755 lib/libwebserver.so.1.0.0 /usr/local/lib/
sudo ln -sf libwebserver.so.1.0.0 /usr/local/lib/libwebserver.so.1
sudo ln -sf libwebserver.so.1 /usr/local/lib/libwebserver.so
sudo ldconfig

# 安装可执行文件
sudo install -m 755 bin/webserver /usr/local/bin/
sudo install -m 755 bin/webserver_test /usr/local/bin/
sudo install -m 755 bin/webserver_bench /usr/local/bin/

# 安装头文件
sudo mkdir -p /usr/local/include/webserver
sudo cp -r include/* /usr/local/include/webserver/

echo "安装完成"
EOF
    chmod +x "$package_dir/install.sh"
    
    # 创建压缩包
    cd "$PROJECT_ROOT"
    tar -czf "${package_name}.tar.gz" "$package_name"
    rm -rf "$package_dir"
    
    print_success "打包完成: ${package_name}.tar.gz"
}

# 运行主程序
run_main() {
    print_info "运行主程序..."
    
    if [ ! -f "$BIN_DIR/webserver" ]; then
        print_error "主程序不存在，请先构建项目"
        print_info "使用: $0 build"
        exit 1
    fi
    
    print_info "启动 WebServer 主程序..."
    print_info "按 Ctrl+C 停止程序"
    print_info "工作目录: $PROJECT_ROOT"
    echo ""
    
    # 在项目根目录下运行主程序
    cd "$PROJECT_ROOT"
    "$BIN_DIR/webserver"
}

# 运行单元测试
run_unit_test() {
    print_info "运行单元测试..."
    
    if [ ! -f "$BIN_DIR/webserver_test" ]; then
        print_error "测试程序不存在，请先构建项目"
        print_info "使用: $0 build"
        exit 1
    fi
    
    print_info "执行单元测试..."
    print_info "工作目录: $PROJECT_ROOT"
    echo ""
    
    # 在项目根目录下运行测试
    cd "$PROJECT_ROOT"
    if "$BIN_DIR/webserver_test"; then
        print_success "所有测试通过"
    else
        print_error "测试失败"
        exit 1
    fi
}

# 运行压力测试
run_benchmark() {
    print_info "运行压力测试..."
    
    if [ ! -f "$BIN_DIR/webserver_bench" ]; then
        print_error "基准测试程序不存在，请先构建项目"
        print_info "使用: $0 build"
        exit 1
    fi
    
    print_info "启动压力测试程序..."
    print_info "参数: --benchmark_min_time=5s --benchmark_repetitions=3 --benchmark_report_aggregates_only=true"
    print_info "程序将在测试完成后自动退出"
    print_info "工作目录: $PROJECT_ROOT"
    echo ""
    
    # 在项目根目录下运行基准测试，带完整参数
    cd "$PROJECT_ROOT"
    
    # 使用timeout命令，设置最大运行时间为5分钟，确保程序能退出
    timeout 300 "$BIN_DIR/webserver_bench" --benchmark_min_time=5s --benchmark_repetitions=3 --benchmark_report_aggregates_only=true
    
    # 检查退出状态
    local exit_code=$?
    if [ $exit_code -eq 124 ]; then
        print_warning "压力测试超时（5分钟），已强制退出"
    elif [ $exit_code -eq 0 ]; then
        print_success "压力测试完成"
    else
        print_error "压力测试异常退出，退出码: $exit_code"
    fi
}

# 主函数
main() {
    print_info "WebServer 自动构建脚本启动"
    print_info "项目根目录: $PROJECT_ROOT"
    
    # 如果没有参数，显示帮助
    if [ $# -eq 0 ]; then
        show_help
        exit 0
    fi
    
    # 处理命令行参数
    for arg in "$@"; do
        case $arg in
            clean)
                clean_build
                ;;
            build)
                check_dependencies
                build_project
                check_build_result
                ;;
            install)
                install_system
                ;;
            package)
                package_release
                ;;
            all)
                check_dependencies
                clean_build
                build_project
                check_build_result
                ;;
            run)
                run_main
                ;;
            run-test)
                run_unit_test
                ;;
            run-bench)
                run_benchmark
                ;;
            help|--help|-h)
                show_help
                exit 0
                ;;
            *)
                print_error "未知选项: $arg"
                show_help
                exit 1
                ;;
        esac
    done
    
    print_success "所有操作完成"
}

# 执行主函数
main "$@" 