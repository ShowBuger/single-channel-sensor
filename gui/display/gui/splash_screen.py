#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
from PyQt6.QtCore import Qt, QTimer, QPropertyAnimation, QEasingCurve, pyqtProperty
from PyQt6.QtGui import QPixmap, QPainter, QColor, QFont
from PyQt6.QtWidgets import QWidget, QLabel, QVBoxLayout


class SplashScreen(QWidget):
    """启动画面窗口，显示logo动画"""

    def __init__(self, duration=3000):
        super().__init__()

        # 设置窗口属性
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint |
                          Qt.WindowType.WindowStaysOnTopHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)

        # 设置窗口大小
        self.setFixedSize(250, 150)

        # 创建logo标签
        self.logo_label = QLabel(self)
        self.logo_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # 获取资源路径
        try:
            base_path = sys._MEIPASS  # 打包环境
        except AttributeError:
            base_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # 向上2层到DISPLAY目录

        # 尝试加载logo图片
        logo_path = os.path.join(base_path, "resources", "images", "logo.png")
        icon_path = os.path.join(base_path, "resources", "images", "icon.png")

        if os.path.exists(logo_path):
            self.original_pixmap = QPixmap(logo_path)
            # 缩放图片
            scaled_pixmap = self.original_pixmap.scaled(200, 120, Qt.AspectRatioMode.KeepAspectRatio,
                                                      Qt.TransformationMode.SmoothTransformation)
            self.logo_label.setPixmap(scaled_pixmap)
            self.logo_label.setGeometry(25, 15, 200, 120)

            # 设置窗口图标
            if os.path.exists(icon_path):
                from PyQt6.QtGui import QIcon
                self.setWindowIcon(QIcon(icon_path))
        else:
            # 如果找不到logo，使用文本替代
            self.logo_label.setText("单通道传感器系统")
            self.logo_label.setGeometry(25, 50, 200, 50)

        # 设置初始透明样式
        self.logo_label.setStyleSheet("""
            QLabel {
                background-color: transparent;
                color: rgba(80, 250, 123, 0);
            }
        """)

        # 初始化动画属性
        self._opacity = 0.0

        # 创建淡入动画
        self.fade_in_animation = QPropertyAnimation(self, b"opacity")
        self.fade_in_animation.setDuration(1000)  # 1秒淡入
        self.fade_in_animation.setStartValue(0.0)
        self.fade_in_animation.setEndValue(1.0)
        self.fade_in_animation.setEasingCurve(QEasingCurve.Type.InOutCubic)

        # 创建淡出动画
        self.fade_out_animation = QPropertyAnimation(self, b"opacity")
        self.fade_out_animation.setDuration(800)  # 0.8秒淡出
        self.fade_out_animation.setStartValue(1.0)
        self.fade_out_animation.setEndValue(0.0)
        self.fade_out_animation.setEasingCurve(QEasingCurve.Type.InOutCubic)

        # 设置定时器
        self.duration = duration
        self.timer = QTimer()
        self.timer.timeout.connect(self.start_fade_out)
        self.timer.setSingleShot(True)

        # 回调函数
        self.fade_callback = None

        # 添加进度值属性以兼容main.py中的进度设置
        self._progress_value = 0

    @pyqtProperty(float)
    def opacity(self):
        """获取透明度"""
        return self._opacity

    @opacity.setter
    def opacity(self, value):
        """设置透明度"""
        self._opacity = value
        # 直接设置logo标签的透明度
        if hasattr(self, 'logo_label'):
            # 创建半透明效果的样式
            opacity_value = int(self._opacity * 255)
            if opacity_value < 0:
                opacity_value = 0
            elif opacity_value > 255:
                opacity_value = 255

            # 设置标签的透明度样式
            self.logo_label.setStyleSheet(f"""
                QLabel {{
                    background-color: transparent;
                    color: rgba(80, 250, 123, {opacity_value});
                }}
            """)
            self.logo_label.update()

    @property
    def progress_value(self):
        """获取进度值（兼容性属性）"""
        return self._progress_value

    @progress_value.setter
    def progress_value(self, value):
        """设置进度值（兼容性属性）"""
        self._progress_value = value

    def paintEvent(self, event):
        """自定义绘制事件 - 完全透明背景"""
        # 不绘制任何背景，让logo自己处理透明度
        pass

    def showEvent(self, event):
        """显示事件，开始淡入动画"""
        super().showEvent(event)
        self.fade_in_animation.start()
        self.timer.start(self.duration)

    def start_fade_out(self):
        """开始淡出动画（定时器触发的自动淡出）"""
        # 断开之前的所有连接
        try:
            self.fade_out_animation.finished.disconnect()
        except:
            pass

        # 连接自动关闭
        self.fade_out_animation.finished.connect(self.close)

        # 开始淡出动画
        self.fade_out_animation.start()

    def fade_out(self, callback):
        """淡出并执行回调函数"""
        # 停止定时器
        self.timer.stop()

        # 保存回调函数
        self.fade_callback = callback

        # 断开之前的所有连接
        try:
            self.fade_out_animation.finished.disconnect()
        except:
            pass

        # 连接新的完成处理
        self.fade_out_animation.finished.connect(self._on_fade_out_finished)

        # 开始淡出动画
        self.fade_out_animation.start()

    def _on_fade_out_finished(self):
        """淡出动画完成后的处理"""
        # 先执行回调函数
        if hasattr(self, 'fade_callback') and callable(self.fade_callback):
            try:
                self.fade_callback()
            except Exception as e:
                print(f"执行启动界面回调时出错: {e}")

        # 然后关闭启动界面
        self.close()

    def close_immediately(self):
        """立即关闭启动画面"""
        self.timer.stop()
        self.fade_in_animation.stop()
        self.fade_out_animation.stop()
        self.close()
