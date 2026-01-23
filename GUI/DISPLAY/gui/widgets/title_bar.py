#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from PyQt6.QtCore import Qt, QPoint
from PyQt6.QtGui import QIcon, QPixmap
from PyQt6.QtWidgets import (
    QWidget, QHBoxLayout, QPushButton,
    QLabel, QSizePolicy
)


class TitleBar(QWidget):
    """自定义标题栏"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.parent = parent

        # 初始鼠标位置
        self.drag_pos = QPoint()

        # 设置对象名
        self.setObjectName("titleBar")

        # 固定高度
        self.setFixedHeight(40)

        # 设置UI
        self.setup_ui()

    def setup_ui(self):
        """设置UI布局"""
        # 创建主布局
        self.main_layout = QHBoxLayout(self)
        self.main_layout.setContentsMargins(10, 0, 10, 0)
        self.main_layout.setSpacing(0)

        # 创建标题标签
        self.title_label = QLabel("三轴力传感器上位机")
        self.title_label.setObjectName("titleLabel")

        # 创建窗口操作按钮
        self.minimize_btn = QPushButton("")
        self.minimize_btn.setObjectName("minimizeButton")
        self.minimize_btn.setFixedSize(16, 16)
        self.minimize_btn.setToolTip("最小化")
        self.minimize_btn.clicked.connect(self.parent.showMinimized)

        self.maximize_btn = QPushButton("")
        self.maximize_btn.setObjectName("maximizeButton")
        self.maximize_btn.setFixedSize(16, 16)
        self.maximize_btn.setToolTip("最大化")
        self.maximize_btn.clicked.connect(self.toggle_maximize)

        self.close_btn = QPushButton("")
        self.close_btn.setObjectName("closeButton")
        self.close_btn.setFixedSize(16, 16)
        self.close_btn.setToolTip("关闭")
        self.close_btn.clicked.connect(self.parent.close)

        # 加载系统按钮图标
        self.load_system_buttons()

        # 将组件添加到布局
        self.main_layout.addWidget(self.title_label)
        # 添加伸缩项
        self.main_layout.addStretch()
        self.main_layout.addWidget(self.minimize_btn)
        self.main_layout.addSpacing(8)
        self.main_layout.addWidget(self.maximize_btn)
        self.main_layout.addSpacing(8)
        self.main_layout.addWidget(self.close_btn)

    def load_system_buttons(self):
        """加载系统按钮图标"""
        # 图标路径
        icons_path = os.path.join(APP_ROOT_PATH, "resources", "images")

        # 尝试加载图标
        min_icon_path = os.path.join(icons_path, "minimize.png")
        max_icon_path = os.path.join(icons_path, "maximize.png")
        close_icon_path = os.path.join(icons_path, "close.png")

        if os.path.exists(min_icon_path):
            self.minimize_btn.setIcon(QIcon(min_icon_path))

        if os.path.exists(max_icon_path):
            self.maximize_btn.setIcon(QIcon(max_icon_path))

        if os.path.exists(close_icon_path):
            self.close_btn.setIcon(QIcon(close_icon_path))

    def toggle_maximize(self):
        """切换最大化/还原窗口"""
        if self.parent.isMaximized():
            self.parent.showNormal()
            if os.path.exists(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                           "resources", "images", "maximize.png")):
                self.maximize_btn.setIcon(
                    QIcon(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                       "resources", "images", "maximize.png")))
            self.maximize_btn.setToolTip("最大化")
        else:
            self.parent.showMaximized()
            if os.path.exists(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                           "resources", "images", "restore.png")):
                self.maximize_btn.setIcon(
                    QIcon(os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                       "resources", "images", "restore.png")))
            self.maximize_btn.setToolTip("还原")

    def mousePressEvent(self, event):
        """鼠标按下事件，用于移动窗口"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.drag_pos = event.globalPosition().toPoint() - self.parent.pos()
            event.accept()

    def mouseMoveEvent(self, event):
        """鼠标移动事件，用于移动窗口"""
        if event.buttons() == Qt.MouseButton.LeftButton and not self.parent.isMaximized():
            self.parent.move(event.globalPosition().toPoint() - self.drag_pos)
            event.accept()

    def mouseDoubleClickEvent(self, event):
        """鼠标双击事件，用于最大化/还原窗口"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.toggle_maximize()