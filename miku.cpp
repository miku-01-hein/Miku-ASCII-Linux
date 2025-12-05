/*
 * 彩色ASCII视频转换器
 * 将普通视频转换为ASCII字符艺术风格的彩色视频
 * 编译命令：g++ -O3 -march=native -std=c++17 -o miku miku.cpp `pkg-config --cflags --libs opencv4` -lpthread
 * 使用示例：./miku input.mp4 output.mp4 80
 *
 * 作者: miku-01-hein + GPT
 *
 * 工作原理：
 * 1. 读取输入视频的每一帧
 * 2. 将每帧图像缩放到指定大小的ASCII网格
 * 3. 将每个像素的亮度映射到ASCII字符
 * 4. 使用原始像素颜色绘制对应字符
 * 5. 将所有处理后的帧写入输出视频
 */

#include <opencv2/opencv.hpp>   // OpenCV库，用于图像和视频处理
#include <iostream>              // 输入输出流
#include <vector>                // 向量容器
#include <string>                // 字符串处理
#include <algorithm>             // 算法函数（如min、max、clamp等）
#include <cmath>                 // 数学函数
#include <iomanip>               // 输出格式化

/*
 * ASCII视频转换器命名空间
 * 包含所有程序使用的常量，避免全局命名空间污染
 * 使用constexpr确保这些常量在编译时确定，提高性能
 */
namespace ASCIIVideoConstants {
    // ASCII字符集：按照从暗到亮的顺序排列的字符
    // 字符越暗对应的亮度值越低，字符越亮对应的亮度值越高
    constexpr const char* ASCII_CHARS = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

    // 默认ASCII宽度：每行显示的ASCII字符数量
    constexpr int DEFAULT_ASCII_WIDTH = 80;

    // 最小ASCII宽度：确保输出视频有足够的清晰度
    constexpr int MIN_ASCII_WIDTH = 20;

    // 最大ASCII宽度：避免创建过大的输出文件
    constexpr int MAX_ASCII_WIDTH = 300;

    // ASCII字符宽度：每个字符在输出图像中占据的像素宽度
    // 这个值影响最终输出视频的分辨率
    constexpr int ASCII_CHAR_WIDTH = 6;

    // ASCII字符高度：每个字符在输出图像中占据的像素高度
    // 通常字符高度大于宽度，因为字符通常是纵向延伸的
    constexpr int ASCII_CHAR_HEIGHT = 12;

    // ASCII字体大小：绘制字符时使用的字体缩放因子
    // 较小的字体大小可以使字符更加紧凑
    constexpr double ASCII_FONT_SIZE = 0.3;

    // 亮度权重常量：用于将RGB颜色转换为亮度（灰度）值
    // 这些权重基于人眼对不同颜色的敏感度
    // 红色权重：0.299，人眼对红色最敏感
    constexpr double RED_WEIGHT = 0.299;
    // 绿色权重：0.587，人眼对绿色最敏感
    constexpr double GREEN_WEIGHT = 0.587;
    // 蓝色权重：0.114，人眼对蓝色最不敏感
    constexpr double BLUE_WEIGHT = 0.114;
}

/*
 * EnhancedASCIIConverter类
 * 主转换器类，负责将视频转换为ASCII艺术风格
 * 使用RAII（资源获取即初始化）原则管理资源
 */
class EnhancedASCIIConverter {
private:
    // 当前使用的字符集字符串
    std::string currentCharset;

    // 已处理的帧计数器
    int frameCount;

public:
    /*
     * 构造函数
     * 初始化帧计数器和字符集
     */
    EnhancedASCIIConverter() : frameCount(0) {
        // 从常量命名空间复制ASCII字符集
        // 使用字符串复制而不是指针引用，以便后续可能修改字符集
        currentCharset = ASCIIVideoConstants::ASCII_CHARS;
    }

    /*
     * 主转换函数
     * 将输入视频转换为彩色ASCII艺术视频
     *
     * 参数：
     *   inputPath: 输入视频文件的路径
     *   outputPath: 输出视频文件的路径
     *   asciiWidth: ASCII网格的宽度（每行字符数），默认80
     *   quality: 质量参数（当前版本未使用，保留用于未来扩展）
     *
     * 返回值：
     *   bool: 转换成功返回true，失败返回false
     *
     * 工作流程：
     *   1. 打开输入视频文件
     *   2. 获取视频信息（分辨率、帧率、总帧数）
     *   3. 计算输出视频参数
     *   4. 创建视频写入器
     *   5. 逐帧处理视频
     *   6. 释放资源并输出结果
     */
    bool convertToColorASCII(const std::string& inputPath, const std::string& outputPath,
                             int asciiWidth = ASCIIVideoConstants::DEFAULT_ASCII_WIDTH, double quality = 1.0) {
        // 步骤1：打开输入视频文件
        cv::VideoCapture cap(inputPath);
        if (!cap.isOpened()) {
            std::cerr << "无法打开视频文件: " << inputPath << std::endl;
            return false;
        }

        // 步骤2：获取视频基本信息
        double fps = cap.get(cv::CAP_PROP_FPS);  // 帧率（每秒帧数）
        int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));  // 总帧数
        int originalWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));  // 原始宽度
        int originalHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));  // 原始高度

        // 显示视频信息，让用户了解处理的是什么视频
        std::cout << "视频信息: " << originalWidth << "x" << originalHeight
        << ", " << fps << "fps, " << totalFrames << "帧" << std::endl;

        // 步骤3：计算输出视频参数
        // 计算ASCII网格高度，保持原始视频的宽高比
        // 乘以0.5是因为字符通常比像素高，需要调整纵横比
        int asciiHeight = static_cast<int>((asciiWidth * originalHeight / originalWidth) * 0.5);

        // 计算输出视频的实际分辨率
        // 每个ASCII字符占据固定像素大小，所以总分辨率 = 字符数 × 字符像素大小
        cv::Size frameSize(asciiWidth * ASCIIVideoConstants::ASCII_CHAR_WIDTH,
                           asciiHeight * ASCIIVideoConstants::ASCII_CHAR_HEIGHT);

        std::cout << "输出尺寸: " << frameSize.width << "x" << frameSize.height << std::endl;
        std::cout << "ASCII网格: " << asciiWidth << "x" << asciiHeight << " 字符" << std::endl;
        std::cout << "使用字符集: " << currentCharset.length() << " 个字符" << std::endl;

        // 步骤4：创建视频写入器
        cv::VideoWriter writer;
        bool writerOpened = false;

        // 尝试多种视频编码器，不同系统和环境可能支持不同的编码器
        // 按顺序尝试直到找到一个可用的编码器
        std::vector<std::vector<int>> codecList = {
            {cv::VideoWriter::fourcc('m', 'p', '4', 'v')},  // MP4V编码器
            {cv::VideoWriter::fourcc('a', 'v', 'c', '1')},  // AVC1编码器
            {cv::VideoWriter::fourcc('X', '2', '6', '4')},  // H.264编码器
            {cv::VideoWriter::fourcc('H', '2', '6', '4')}   // 另一种H.264编码器
        };

        for (const auto& codec : codecList) {
            writer.open(outputPath, codec[0], fps, frameSize);
            if (writer.isOpened()) {
                writerOpened = true;
                std::cout << "使用编码器: " << std::hex << codec[0] << std::dec << std::endl;
                break;
            }
        }

        if (!writerOpened) {
            std::cerr << "无法创建输出视频文件: " << outputPath << std::endl;
            cap.release();
            return false;
        }

        // 步骤5：逐帧处理视频
        cv::Mat frame, resized;  // 原始帧和调整大小后的帧
        frameCount = 0;  // 重置帧计数器

        std::cout << "开始转换视频..." << std::endl;

        // 显示字符集信息，帮助用户理解亮度到字符的映射关系
        testCharacterDisplay();

        // 主处理循环：读取、处理、写入每一帧
        while (true) {
            cap >> frame;  // 从视频捕获对象读取下一帧
            if (frame.empty()) {
                break;  // 如果读取到空帧，说明视频已结束
            }

            // 5.1 调整帧大小到ASCII网格尺寸
            // 使用INTER_AREA插值方法，适合缩小图像
            cv::resize(frame, resized, cv::Size(asciiWidth, asciiHeight), 0, 0, cv::INTER_AREA);

            // 5.2 将调整大小后的帧转换为ASCII艺术帧
            cv::Mat asciiFrame = generateColorASCIIFrame(resized);

            // 5.3 将ASCII艺术帧写入输出视频
            writer.write(asciiFrame);

            // 5.4 更新帧计数器并显示进度
            frameCount++;
            if (frameCount % 30 == 0) {  // 每处理30帧显示一次进度
                double progress = (frameCount * 100.0) / totalFrames;
                std::cout << "进度: " << frameCount << "/" << totalFrames
                << " 帧 (" << std::fixed << std::setprecision(1) << progress << "%)" << std::endl;
            }
        }

        // 步骤6：释放资源
        cap.release();   // 释放视频捕获对象
        writer.release(); // 释放视频写入对象

        std::cout << "转换完成! 总帧数: " << frameCount << std::endl;
        std::cout << "输出文件: " << outputPath << std::endl;
        return true;  // 转换成功
                             }

private:
    /*
     * 测试字符显示函数
     * 显示当前使用的字符集及其亮度映射关系
     *
     * 作用：
     *   1. 让用户了解使用哪些字符进行转换
     *   2. 显示字符的亮度映射关系，帮助理解字符选择逻辑
     *   3. 调试目的：验证字符集是否正确加载
     */
    void testCharacterDisplay() {
        std::cout << "测试字符显示:" << std::endl;
        std::cout << "基本字符集: " << currentCharset << std::endl;
        std::cout << "字符数量: " << currentCharset.length() << std::endl;

        std::cout << "字符亮度映射:" << std::endl;

        // 遍历字符集中的每个字符
        for (size_t i = 0; i < currentCharset.length(); ++i) {
            // 计算字符对应的亮度值
            // 假设字符在字符集中的位置线性对应亮度
            // 第一个字符对应亮度0（最暗），最后一个字符对应亮度1（最亮）
            double brightness = static_cast<double>(i) / (currentCharset.length() - 1);

            // 格式化输出字符和对应的亮度值
            std::cout << "'" << currentCharset[i] << "' -> " << std::fixed << std::setprecision(2) << brightness;

            // 每显示8个字符换行一次，使输出更整洁
            if (i % 8 == 7) {
                std::cout << std::endl;
            } else {
                std::cout << " | ";
            }
        }
        std::cout << std::endl;
    }

    /*
     * 生成彩色ASCII帧函数
     * 将彩色图像帧转换为ASCII艺术图像帧
     *
     * 参数：
     *   colorFrame: 输入彩色图像帧（已调整到ASCII网格大小）
     *
     * 返回值：
     *   cv::Mat: 包含ASCII字符的彩色图像帧
     *
     * 工作原理：
     *   1. 创建黑色背景图像
     *   2. 遍历输入图像的每个像素
     *   3. 计算像素亮度
     *   4. 根据亮度选择ASCII字符
     *   5. 使用像素原始颜色绘制字符
     */
    cv::Mat generateColorASCIIFrame(const cv::Mat& colorFrame) {
        // 获取输入图像的尺寸（ASCII网格尺寸）
        int width = colorFrame.cols;   // 列数 = ASCII宽度
        int height = colorFrame.rows;  // 行数 = ASCII高度

        // 创建输出图像（ASCII艺术帧）
        // 尺寸：每个ASCII字符占据固定像素大小
        // 类型：CV_8UC3 表示8位无符号整数，3通道（BGR彩色图像）
        // 初始颜色：黑色背景（Scalar(0, 0, 0)）
        cv::Mat asciiFrame(height * ASCIIVideoConstants::ASCII_CHAR_HEIGHT,
                           width * ASCIIVideoConstants::ASCII_CHAR_WIDTH,
                           CV_8UC3, cv::Scalar(0, 0, 0));

        // 双重循环遍历ASCII网格中的每个位置
        for (int y = 0; y < height; y++) {         // 行循环
            for (int x = 0; x < width; x++) {      // 列循环
                // 获取当前像素的颜色值（BGR格式）
                cv::Vec3b pixel = colorFrame.at<cv::Vec3b>(y, x);

                // 计算像素亮度（灰度值）
                // 使用加权平均公式：亮度 = (0.299*R + 0.587*G + 0.114*B) / 255
                // 除以255将亮度归一化到[0, 1]范围
                double brightness = (ASCIIVideoConstants::RED_WEIGHT * pixel[2] +   // 红色通道
                ASCIIVideoConstants::GREEN_WEIGHT * pixel[1] + // 绿色通道
                ASCIIVideoConstants::BLUE_WEIGHT * pixel[0]) / 255.0; // 蓝色通道

                // 根据亮度选择对应的ASCII字符
                char asciiChar = getASCIIChar(brightness);

                // 使用像素的原始颜色作为字符颜色
                // OpenCV使用BGR格式：Scalar(blue, green, red)
                cv::Scalar textColor(pixel[0], pixel[1], pixel[2]);

                // 计算字符绘制位置
                // x方向：字符索引 × 字符宽度
                // y方向：(字符索引+1) × 字符高度 - 2（微调使字符垂直居中）
                cv::Point textPos(x * ASCIIVideoConstants::ASCII_CHAR_WIDTH,
                                  (y + 1) * ASCIIVideoConstants::ASCII_CHAR_HEIGHT - 2);

                // 在输出图像上绘制ASCII字符
                cv::putText(asciiFrame,                    // 目标图像
                            std::string(1, asciiChar),     // 要绘制的文本（单个字符）
                            textPos,                       // 绘制位置
                            cv::FONT_HERSHEY_SIMPLEX,      // 字体类型
                            ASCIIVideoConstants::ASCII_FONT_SIZE, // 字体大小
                            textColor,                     // 文字颜色
                            1,                             // 线条粗细
                            cv::LINE_AA);                  // 抗锯齿

                // 调试输出：只在第一帧的前6个像素显示亮度到字符的映射关系
                // 帮助理解字符选择过程，实际运行时只执行一次
                if (x < 3 && y < 2 && frameCount == 0) {
                    std::cout << "像素(" << x << "," << y << "): 亮度=" << std::fixed
                    << std::setprecision(3) << brightness << ", 字符='"
                    << asciiChar << "'" << std::endl;
                }
            }
        }

        return asciiFrame;  // 返回生成的ASCII艺术帧
    }

    /*
     * 获取ASCII字符函数
     * 根据亮度值选择对应的ASCII字符
     *
     * 参数：
     *   brightness: 归一化的亮度值，范围应为[0, 1]
     *
     * 返回值：
     *   char: 对应亮度的ASCII字符
     *
     * 映射原理：
     *   1. 确保亮度值在有效范围[0, 1]内
     *   2. 将亮度线性映射到字符集索引
     *   3. 通过索引从字符集中选择字符
     */
    char getASCIIChar(double brightness) {
        // 步骤1：确保亮度值在有效范围内
        // 使用std::min和std::max将亮度限制在[0, 1]区间
        brightness = std::max(0.0, std::min(1.0, brightness));

        // 步骤2：计算字符索引
        // 将亮度线性映射到字符集索引范围[0, charset.length()-1]
        int index = static_cast<int>(brightness * (currentCharset.length() - 1));

        // 步骤3：确保索引在有效范围内
        // 再次使用std::min和std::max防止索引越界
        index = std::max(0, std::min(static_cast<int>(currentCharset.length() - 1), index));

        // 步骤4：返回对应的ASCII字符
        return currentCharset[index];
    }
};

/*
 * 主函数
 * 程序的入口点，处理命令行参数并启动转换过程
 *
 * 参数：
 *   argc: 命令行参数数量
 *   argv: 命令行参数数组
 *
 * 返回值：
 *   int: 程序退出状态码（0表示成功，非0表示错误）
 *
 * 工作流程：
 *   1. 解析命令行参数
 *   2. 验证参数有效性
 *   3. 创建转换器实例
 *   4. 执行转换
 *   5. 输出结果
 */
int main(int argc, char* argv[]) {
    // 步骤1：检查命令行参数数量
    // 至少需要输入文件和输出文件两个参数
    if (argc < 3) {
        std::cout << "用法: " << argv[0] << " <input-video> <output-video> [ASCII宽度]" << std::endl;
        std::cout << "示例: " << argv[0] << " miku.mp4 ascii.mp4" << std::endl;
        std::cout << "示例: " << argv[0] << " miku.mp4 ascii.mp4 120" << std::endl;
        std::cout << "建议ASCII宽度: 60-150 (数值越大越清晰但文件越大)" << std::endl;
        return 1;  // 返回错误码1：参数不足
    }

    // 步骤2：解析命令行参数
    std::string inputPath = argv[1];   // 第一个参数：输入视频文件路径
    std::string outputPath = argv[2];  // 第二个参数：输出视频文件路径
    int asciiWidth = ASCIIVideoConstants::DEFAULT_ASCII_WIDTH;  // 第三个参数：ASCII宽度（可选）

    // 如果提供了第三个参数（ASCII宽度），则使用用户指定的值
    if (argc >= 4) {
        asciiWidth = std::atoi(argv[3]);  // 将字符串转换为整数
    }

    // 步骤3：验证ASCII宽度参数是否在有效范围内
    if (asciiWidth < ASCIIVideoConstants::MIN_ASCII_WIDTH ||
        asciiWidth > ASCIIVideoConstants::MAX_ASCII_WIDTH) {
        std::cerr << "错误: ASCII宽度应在" << ASCIIVideoConstants::MIN_ASCII_WIDTH
        << "-" << ASCIIVideoConstants::MAX_ASCII_WIDTH << "之间" << std::endl;
    return 1;  // 返回错误码1：参数无效
        }

        // 步骤4：创建ASCII转换器实例
        EnhancedASCIIConverter converter;

        // 步骤5：显示程序标题和分隔符
        std::cout << "========================================" << std::endl;
        std::cout << "原彩ASCII视频转换器" << std::endl;
        std::cout << "========================================" << std::endl;

        // 步骤6：执行视频转换
        if (converter.convertToColorASCII(inputPath, outputPath, asciiWidth, 1.0)) {
            // 转换成功：显示成功信息和输出文件路径
            std::cout << "========================================" << std::endl;
            std::cout << "成功创建彩色ASCII视频!" << std::endl;
            std::cout << "输出文件: " << outputPath << std::endl;
            std::cout << "========================================" << std::endl;
            return 0;  // 返回0：程序执行成功
        } else {
            // 转换失败：显示错误信息
            std::cerr << "========================================" << std::endl;
            std::cerr << "转换失败!" << std::endl;
            std::cerr << "========================================" << std::endl;
            return 1;  // 返回1：转换失败
        }
}

/*
 * 编译和运行说明：
 *
 * 1. 编译命令：
 *    g++ -O3 -march=native -std=c++17 -o miku miku.cpp \
 *        `pkg-config --cflags --libs opencv4` -lpthread
 *
 *    参数解释：
 *    -O3: 最高级别优化，提高程序运行速度
 *    -march=native: 为当前CPU架构优化，充分利用CPU特性
 *    -std=c++17: 使用C++17标准，支持现代C++特性
 *    -o ascii_video: 指定输出可执行文件名
 *    `pkg-config --cflags --libs opencv4`: 自动获取OpenCV编译选项和链接库
 *    -lpthread: 链接POSIX线程库，提高多线程性能
 *
 * 2. 运行示例：
 *    ./miku input-video.mp4 output-video.mp4 80
 *
 * 3. 参数说明：
 *    第一个参数：输入视频文件路径（必须是系统支持的视频格式）
 *    第二个参数：输出视频文件路径（建议使用.mp4扩展名）
 *    第三个参数：ASCII宽度（可选，默认80，建议值60-150）
 *
 * 4. 性能提示：
 *    - ASCII宽度越大，输出视频越清晰，但处理时间和文件大小也越大
 *    - 处理长视频时可能需要较长时间，请耐心等待
 *    - 程序会显示处理进度，每处理30帧更新一次
 *
 * 5. 输出说明：
 *    - 程序会在控制台显示详细处理信息
 *    - 转换完成后会生成指定路径的输出文件
 *    - 输出视频中的每个像素都被替换为ASCII字符
 *    - 字符颜色保持原始像素颜色
 */
