from maix import camera, display, image, nn, app, uart, pinmap, err, time, video
import os

# ================= 用户配置 =================

MODEL_PATH = "/mnt/data/models/model_314093.mud"

# 可填写："yolo11"、"yolov8"、"yolov5"
MODEL_TYPE = "yolov5"

# 钢球类别编号
BALL_CLASS_ID = 0

CONF_THRESHOLD = 0.45
IOU_THRESHOLD = 0.45

# 从画面中心到左右边缘的实际距离
HALF_LENGTH_CM = 13.5

# 滤波系数
SMOOTH_ALPHA = 0.35

UART_DEVICE = "/dev/ttyS1"
UART_BAUDRATE = 115200
UART_TX_PIN = "A19"
UART_RX_PIN = "A18"

UART_PERIOD_MS = 20
PRINT_PERIOD_MS = 100

# 全屏分辨率（16:9，适配屏幕）
CAM_FULL_W = 640
CAM_FULL_H = 360

# ========== 视频录制配置 ==========
RECORD_SEC = 0                     # 录制时长（秒），设为0则一直录制直到按Ctrl+C
SAVE_DIR = "/maixapp/share/video"   # 保存目录
FILE_NAME = "detect_record.h265"    # 裸流文件名

def create_detector():
    model_type = MODEL_TYPE.lower()
    if model_type == "yolo11":
        return nn.YOLO11(model=MODEL_PATH, dual_buff=True)
    if model_type == "yolov8":
        return nn.YOLOv8(model=MODEL_PATH, dual_buff=True)
    if model_type == "yolov5":
        return nn.YOLOv5(model=MODEL_PATH, dual_buff=True)
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
    err.check_raise(pinmap.set_pin_function(UART_TX_PIN, "UART1_TX"), "UART1 TX初始化失败")
    err.check_raise(pinmap.set_pin_function(UART_RX_PIN, "UART1_RX"), "UART1 RX初始化失败")
    return uart.UART(UART_DEVICE, UART_BAUDRATE)

def find_ball(objects):
    best_ball = None
    for obj in objects:
        if obj.class_id != BALL_CLASS_ID:
            continue
        if best_ball is None or obj.score > best_ball.score:
            best_ball = obj
    return best_ball

def pixel_to_position_cm(pixel_x, image_width):
    screen_center_x = image_width / 2.0
    total_length_cm = HALF_LENGTH_CM * 2.0
    position_cm = (pixel_x - screen_center_x) * total_length_cm / image_width
    return limit(position_cm, -HALF_LENGTH_CM, HALF_LENGTH_CM)

def main():
    detector = create_detector()
    model_in_w = detector.input_width()
    model_in_h = detector.input_height()

    # 摄像头（RGB888）
    cam = camera.Camera(CAM_FULL_W, CAM_FULL_H, image.Format.FMT_RGB888)
    disp = display.Display()
    serial = uart_init()

    # ---------- 初始化视频编码器 ----------
    # 确保保存目录存在
    if not os.path.exists(SAVE_DIR):
        os.makedirs(SAVE_DIR)
    output_path = os.path.join(SAVE_DIR, FILE_NAME)

    # 编码器只支持 NV21（YVU420SP），宽高与摄像头一致
    encoder = video.Encoder(width=CAM_FULL_W, height=CAM_FULL_H)
    f = open(output_path, 'wb')
    record_start_ms = get_time_ms()
    # 如果 RECORD_SEC 为 0，则一直录制直到程序退出
    record_end_ms = record_start_ms + RECORD_SEC * 1000 if RECORD_SEC > 0 else None

    # ---------- 检测变量 ----------
    smooth_x = None
    smooth_y = None
    last_send_ms = 0
    last_print_ms = 0

    print("================================")
    print("MaixCAM 钢球检测 + 视频录制")
    print(f"视频保存至: {output_path}")
    print(f"录制时长: {RECORD_SEC}秒" if RECORD_SEC > 0 else "录制将持续到程序退出")
    print("================================")

    while not app.need_exit():
        full_img = cam.read()
        full_w = full_img.width()
        full_h = full_img.height()

        # ----- 检测部分 -----
        infer_img = full_img.resize(model_in_w, model_in_h)
        objects = detector.detect(infer_img, conf_th=CONF_THRESHOLD, iou_th=IOU_THRESHOLD)

        screen_center_x = int(full_w / 2)

        # 绘制蓝色边框（画面边缘）
        full_img.draw_rect(0, 0, full_w - 1, full_h - 1, color=image.COLOR_BLUE, thickness=2)
        # 绘制中心线
        full_img.draw_line(screen_center_x, 0, screen_center_x, full_h - 1, color=image.COLOR_GREEN, thickness=1)
        # 左右文字标识
        full_img.draw_string(4, full_h - 24, f"-{HALF_LENGTH_CM}cm BACK", color=image.COLOR_BLUE)
        full_img.draw_string(full_w - 130, full_h - 24, f"FRONT +{HALF_LENGTH_CM}cm", color=image.COLOR_BLUE)

        ball = find_ball(objects)
        now_ms = get_time_ms()

        if ball is not None:
            # 坐标映射
            model_cx = ball.x + ball.w / 2.0
            model_cy = ball.y + ball.h / 2.0
            raw_x = model_cx * full_w / model_in_w
            raw_y = model_cy * full_h / model_in_h

            if smooth_x is None:
                smooth_x, smooth_y = raw_x, raw_y
            else:
                smooth_x += SMOOTH_ALPHA * (raw_x - smooth_x)
                smooth_y += SMOOTH_ALPHA * (raw_y - smooth_y)

            ball_x = int(limit(smooth_x, 0, full_w - 1))
            ball_y = int(limit(smooth_y, 0, full_h - 1))
            position_cm = pixel_to_position_cm(smooth_x, full_w)
            position_mm = int(position_cm * 10.0)

            # 检测框映射
            box_x1 = ball.x * full_w / model_in_w
            box_y1 = ball.y * full_h / model_in_h
            box_x2 = (ball.x + ball.w) * full_w / model_in_w
            box_y2 = (ball.y + ball.h) * full_h / model_in_h
            full_img.draw_rect(int(box_x1), int(box_y1), int(box_x2 - box_x1), int(box_y2 - box_y1),
                               color=image.COLOR_RED, thickness=2)
            full_img.draw_cross(ball_x, ball_y, color=image.COLOR_RED, size=8, thickness=2)
            full_img.draw_line(screen_center_x, ball_y, ball_x, ball_y, color=image.COLOR_YELLOW, thickness=2)

            full_img.draw_string(4, 4, "Ball:{:+.2f}cm".format(position_cm), color=image.COLOR_RED)
            full_img.draw_string(4, 28, "UART:{:+d}mm".format(position_mm), color=image.COLOR_RED)
            full_img.draw_string(4, 52, "Score:{:.2f}".format(ball.score), color=image.COLOR_RED)

            # 串口发送
            if now_ms - last_send_ms >= UART_PERIOD_MS:
                serial.write_str(f"$BALL,{position_mm},1\r\n")
                last_send_ms = now_ms

            # 终端打印
            if now_ms - last_print_ms >= PRINT_PERIOD_MS:
                print(f"Ball: {position_cm:+.2f} cm, UART: {position_mm:+d} mm, Score: {ball.score:.2f}")
                last_print_ms = now_ms

        else:
            smooth_x = None
            smooth_y = None
            full_img.draw_string(4, 4, "BALL NOT FOUND", color=image.COLOR_RED)
            if now_ms - last_send_ms >= UART_PERIOD_MS:
                serial.write_str("$BALL,0,0\r\n")
                last_send_ms = now_ms
            if now_ms - last_print_ms >= PRINT_PERIOD_MS:
                print("Ball not found")
                last_print_ms = now_ms

        # ----- 视频录制：将当前帧编码并写入文件 -----
        # 将 RGB888 转换为 NV21（编码器需要）
        nv21_img = full_img.to_format(image.Format.FMT_YVU420SP)
        frame_data = encoder.encode(nv21_img)
        f.write(frame_data.to_bytes())

        # 显示到屏幕
        disp.show(full_img)

        # 检查录制时长是否结束
        if record_end_ms is not None and now_ms >= record_end_ms:
            print("录制时间到，停止录制...")
            break

    # ----- 释放资源 -----
    f.close()
    del encoder
    del cam
    print(f"录制完成，视频文件: {output_path}")
    print(f"文件大小: {os.path.getsize(output_path) / 1024:.1f} KB")

    # 可选：使用 ffmpeg 转换为 MP4
    mp4_path = output_path.replace(".h265", ".mp4")
    os.system(f'ffmpeg -loglevel quiet -i {output_path} -c:v copy {mp4_path} -y')
    if os.path.exists(mp4_path):
        print(f"已转换为 MP4: {mp4_path}")
        print(f"MP4 大小: {os.path.getsize(mp4_path) / 1024:.1f} KB")

if __name__ == "__main__":
    main()