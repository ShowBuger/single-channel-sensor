#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import builtins
from PyQt6.QtCore import Qt, QTimer, QPropertyAnimation, QEasingCurve, pyqtProperty
from PyQt6.QtGui import QPixmap, QPainter, QColor
from PyQt6.QtWidgets import QWidget, QLabel, QVBoxLayout


class SplashScreen(QWidget):
    """启动画面窗口，显示logo动画"""

    def __init__(self, duration=2000):
        super().__init__()

        # 设置窗口属性
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.WindowStaysOnTopHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)

        # 设置窗口大小和居中
        self.setFixedSize(400, 300)

        # 创建布局
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # 创建logo标签
        self.logo_label = QLabel()
        self.logo_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # 加载logo图片
        logo_path = os.path.join(builtins.APP_ROOT_PATH, "resources", "images", "logo.png")
        if os.path.exists(logo_path):
            pixmap = QPixmap(logo_path)
            # 缩放图片以适应窗口大小
            scaled_pixmap = pixmap.scaled(200, 150, Qt.AspectRatioMode.KeepAspectRatio,
                                         Qt.TransformationMode.SmoothTransformation)
            self.logo_label.setPixmap(scaled_pixmap)
        else:
            # 如果找不到logo，使用文本替代
            self.logo_label.setText("单通道传感器系统")
            self.logo_label.setStyleSheet("font-size: 24px; font-weight: bold; color: #50fa7b;")

        layout.addWidget(self.logo_label)

        # 设置背景色
        self.setStyleSheet("background-color: rgba(40, 42, 54, 0.95); border-radius: 15px;")

        # 初始化动画属性
        self._opacity = 0.0

        # 创建淡入动画
        self.fade_in_animation = QPropertyAnimation(self, b"opacity")
        self.fade_in_animation.setDuration(800)
        self.fade_in_animation.setStartValue(0.0)
        self.fade_in_animation.setEndValue(1.0)
        self.fade_in_animation.setEasingCurve(QEasingCurve.Type.InOutCubic)

        # 创建淡出动画
        self.fade_out_animation = QPropertyAnimation(self, b"opacity")
        self.fade_out_animation.setDuration(600)
        self.fade_out_animation.setStartValue(1.0)
        self.fade_out_animation.setEndValue(0.0)
        self.fade_out_animation.setEasingCurve(QEasingCurve.Type.InOutCubic)
        self.fade_out_animation.finished.connect(self.close)

        # 设置定时器
        self.duration = duration
        self.timer = QTimer()
        self.timer.timeout.connect(self.start_fade_out)
        self.timer.setSingleShot(True)

    @pyqtProperty(float)
    def opacity(self):
        """获取透明度"""
        return self._opacity

    @opacity.setter
    def opacity(self, value):
        """设置透明度"""
        self._opacity = value
        self.update()

    def paintEvent(self, event):
        """自定义绘制事件，实现透明效果"""
        painter = QPainter(self)
        painter.setOpacity(self._opacity)
        painter.fillRect(self.rect(), QColor(40, 42, 54, int(240 * self._opacity)))

        # 绘制边框
        painter.setPen(QColor(98, 114, 164, int(255 * self._opacity)))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), 15, 15)

    def showEvent(self, event):
        """显示事件，开始淡入动画"""
        super().showEvent(event)
        self.fade_in_animation.start()
        self.timer.start(self.duration)

    def start_fade_out(self):
        """开始淡出动画"""
        self.fade_out_animation.start()

    def close_immediately(self):
        """立即关闭启动画面"""
        self.timer.stop()
        self.fade_in_animation.stop()
        self.fade_out_animation.stop()
        self.close()