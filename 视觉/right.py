from maix import camera, display, image, nn, app, uart, pinmap, err, time


# ================= 用户配置 =================

MODEL_PATH = "/mnt/data/models/model_314093.mud"

# 可填写："yolo11"、"yolov8"、"yolov5"
MODEL_TYPE = "yolov5"

# 钢球类别编号
BALL_CLASS_ID = 0

CONF_THRESHOLD = 0.45
IOU_THRESHOLD = 0.45

# 从画面中心到左右边缘的实际距离
# 屏幕左端：-13.5cm
# 屏幕中心： 0cm
# 屏幕右端：+13.5cm
HALF_LENGTH_CM = 13.5

# 滤波系数：越大反应越快，越小越平滑
SMOOTH_ALPHA = 0.35

UART_DEVICE = "/dev/ttyS1"
UART_BAUDRATE = 115200

# MaixCAM UART1
UART_TX_PIN = "A19"
UART_RX_PIN = "A18"

# 每20ms向STM32发送一次，即50Hz
UART_PERIOD_MS = 20

# 每100ms向MaixVision终端打印一次，避免刷屏太快
PRINT_PERIOD_MS = 100

# =========【新增配置：相机全屏分辨率】=========
# MaixCAM推荐 640×360（16:9 适配屏幕，完整广角视野）
CAM_FULL_W = 640
CAM_FULL_H = 360


def create_detector():
    model_type = MODEL_TYPE.lower()

    if model_type == "yolo11":
        return nn.YOLO11(
            model=MODEL_PATH,
            dual_buff=True
        )

    if model_type == "yolov8":
        return nn.YOLOv8(
            model=MODEL_PATH,
            dual_buff=True
        )

    if model_type == "yolov5":
        return nn.YOLOv5(
            model=MODEL_PATH,
            dual_buff=True
        )

    raise ValueError("MODEL_TYPE填写错误")


def limit(value, minimum, maximum):
    if value < minimum:
        return minimum

    if value > maximum:
        return maximum

    return value


def get_time_ms():
    try:
        return time.ticks_ms()
    except Exception:
        return int(time.time_s() * 1000)


def uart_init():
    err.check_raise(
        pinmap.set_pin_function(
            UART_TX_PIN,
            "UART1_TX"
        ),
        "UART1 TX初始化失败"
    )

    err.check_raise(
        pinmap.set_pin_function(
            UART_RX_PIN,
            "UART1_RX"
        ),
        "UART1 RX初始化失败"
    )

    return uart.UART(
        UART_DEVICE,
        UART_BAUDRATE
    )


def find_ball(objects):
    """
    从全部识别结果中找到置信度最高的钢球。
    """
    best_ball = None

    for obj in objects:
        if obj.class_id != BALL_CLASS_ID:
            continue

        if best_ball is None:
            best_ball = obj
        elif obj.score > best_ball.score:
            best_ball = obj

    return best_ball


def pixel_to_position_cm(pixel_x, image_width):
    """
    将小球横坐标转换成实际位置。
    屏幕最左端：-13.5cm
    屏幕中心：   0cm
    屏幕最右端：+13.5cm
    """
    screen_center_x = image_width / 2.0
    total_length_cm = HALF_LENGTH_CM * 2.0

    position_cm = (
        (pixel_x - screen_center_x)
        * total_length_cm
        / image_width
    )

    return limit(
        position_cm,
        -HALF_LENGTH_CM,
        HALF_LENGTH_CM
    )


def main():
    detector = create_detector()
    # 获取模型正方形输入尺寸（如320×320）
    model_in_w = detector.input_width()
    model_in_h = detector.input_height()

    # =========【修改1】相机初始化：使用全屏长方形分辨率！=========
    cam = camera.Camera(
        CAM_FULL_W,
        CAM_FULL_H,
        image.Format.FMT_RGB888
    )

    disp = display.Display()
    serial = uart_init()

    smooth_x = None
    smooth_y = None

    last_send_ms = 0
    last_print_ms = 0

    print("================================")
    print("MaixCAM钢球位置检测开始【全屏视野版本】")
    print("屏幕左方（车后方）：负数")
    print("屏幕右方（车前方）：正数")
    print(f"位置范围：-{HALF_LENGTH_CM}cm ~ +{HALF_LENGTH_CM}cm")
    print("================================")

    while not app.need_exit():
        # full_img：原始完整长方形画面（全屏视野！）
        full_img = cam.read()
        full_w = full_img.width()
        full_h = full_img.height()

        # =========【修改2】缩放大图到模型正方形尺寸用于推理 =========
        infer_img = full_img.resize(model_in_w, model_in_h)
        objects = detector.detect(
            infer_img,
            conf_th=CONF_THRESHOLD,
            iou_th=IOU_THRESHOLD
        )

        screen_center_x = int(full_w / 2)

        # 蓝色边框覆盖完整画面
        full_img.draw_rect(
            0,
            0,
            full_w - 1,
            full_h - 1,
            color=image.COLOR_BLUE,
            thickness=2
        )

        # 绘制画面中心线，对应实际位置0cm
        full_img.draw_line(
            screen_center_x,
            0,
            screen_center_x,
            full_h - 1,
            color=image.COLOR_GREEN,
            thickness=1
        )

        # 标记左右方向
        full_img.draw_string(
            4,
            full_h - 24,
            f"-{HALF_LENGTH_CM}cm BACK",
            color=image.COLOR_BLUE
        )

        full_img.draw_string(
            full_w - 130,
            full_h - 24,
            f"FRONT +{HALF_LENGTH_CM}cm",
            color=image.COLOR_BLUE
        )

        ball = find_ball(objects)

        now_ms = get_time_ms()

        if ball is not None:
            # =========【修改3】坐标映射：模型正方形坐标 → 原始大图坐标 =========
            # 模型图上小球中心
            model_ball_cx = ball.x + ball.w / 2.0
            model_ball_cy = ball.y + ball.h / 2.0
            # 换算到全屏大图
            raw_x = model_ball_cx * full_w / model_in_w
            raw_y = model_ball_cy * full_h / model_in_h

            # 一阶低通滤波，减小位置跳动
            if smooth_x is None:
                smooth_x = raw_x
                smooth_y = raw_y
            else:
                smooth_x = smooth_x + SMOOTH_ALPHA * (raw_x - smooth_x)
                smooth_y = smooth_y + SMOOTH_ALPHA * (raw_y - smooth_y)

            ball_x = int(smooth_x + 0.5)
            ball_y = int(smooth_y + 0.5)

            # 防止绘制坐标超出画面
            ball_x = int(limit(ball_x, 0, full_w - 1))
            ball_y = int(limit(ball_y, 0, full_h - 1))

            # 根据完整屏幕宽度换算实际位置
            position_cm = pixel_to_position_cm(smooth_x, full_w)
            # 转换成毫米发送给STM32
            position_mm = int(position_cm * 10.0)

            # =========【重要】检测框也要映射回大图（可选，可视化用）=========
            box_x1 = ball.x * full_w / model_in_w
            box_y1 = ball.y * full_h / model_in_h
            box_x2 = (ball.x + ball.w) * full_w / model_in_w
            box_y2 = (ball.y + ball.h) * full_h / model_in_h
            box_w = box_x2 - box_x1
            box_h = box_y2 - box_y1

            # 绘制YOLO钢球检测框（映射到全屏图）
            full_img.draw_rect(
                int(box_x1),
                int(box_y1),
                int(box_w),
                int(box_h),
                color=image.COLOR_RED,
                thickness=2
            )

            # 绘制滤波后的小球中心点
            full_img.draw_cross(
                ball_x,
                ball_y,
                color=image.COLOR_RED,
                size=8,
                thickness=2
            )

            # 从画面中心画线到小球位置
            full_img.draw_line(
                screen_center_x,
                ball_y,
                ball_x,
                ball_y,
                color=image.COLOR_YELLOW,
                thickness=2
            )

            # 屏幕显示小球位置
            full_img.draw_string(
                4,
                4,
                "Ball:{:+.2f}cm".format(position_cm),
                color=image.COLOR_RED
            )

            full_img.draw_string(
                4,
                28,
                "UART:{:+d}mm".format(position_mm),
                color=image.COLOR_RED
            )

            full_img.draw_string(
                4,
                52,
                "Score:{:.2f}".format(ball.score),
                color=image.COLOR_RED
            )

            # 50Hz发送给STM32
            if now_ms - last_send_ms >= UART_PERIOD_MS:
                packet = "$BALL,{},1\r\n".format(position_mm)
                serial.write_str(packet)
                last_send_ms = now_ms

            # 返回位置到MaixVision终端
            if now_ms - last_print_ms >= PRINT_PERIOD_MS:
                print(
                    "Ball position: {:+.2f} cm, "
                    "UART: {:+d} mm, "
                    "Pixel X: {}, "
                    "Score: {:.2f}".format(
                        position_cm,
                        position_mm,
                        ball_x,
                        ball.score
                    )
                )
                last_print_ms = now_ms

        else:
            # 丢失目标后清除滤波数据
            smooth_x = None
            smooth_y = None

            full_img.draw_string(
                4,
                4,
                "BALL NOT FOUND",
                color=image.COLOR_RED
            )

            # 向STM32发送无效标志
            if now_ms - last_send_ms >= UART_PERIOD_MS:
                serial.write_str("$BALL,0,0\r\n")
                last_send_ms = now_ms

            # MaixVision终端提示未识别到钢球
            if now_ms - last_print_ms >= PRINT_PERIOD_MS:
                print("Ball not found")
                last_print_ms = now_ms

        # 全屏图像推送到屏幕显示
        disp.show(full_img)


if __name__ == "__main__":
    main()