#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtGui import QIcon, QPixmap
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QSpacerItem,
    QSizePolicy, QFrame
)

class LeftMenu(QWidget):
    """左侧菜单栏"""

    def __init__(self, parent=None):
        super().__init__(parent)

        # 设置最小宽度
        self.setMinimumWidth(60)
        self.setMaximumWidth(60)

        # 设置对象名称以便应用样式
        self.setObjectName("leftMenu")

        # 设置UI
        self.setup_ui()

    def setup_ui(self):
        """设置UI布局"""
        # 创建主布局
        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(0, 0, 0, 0)
        self.main_layout.setSpacing(0)

        # 创建顶部按钮区域
        self.top_frame = QFrame()
        self.top_layout = QVBoxLayout(self.top_frame)
        self.top_layout.setContentsMargins(0, 10, 0, 0)
        self.top_layout.setSpacing(5)

        # 应用图标
        self.app_icon = QLabel()
        self.app_icon.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.app_icon.setMinimumHeight(50)

        # 尝试加载应用图标
        icon_path = os.path.join(APP_ROOT_PATH, "resources", "images", "icon.png")
        if os.path.exists(icon_path):
            pixmap = QPixmap(icon_path).scaled(32, 32, Qt.AspectRatioMode.KeepAspectRatio,
                                               Qt.TransformationMode.SmoothTransformation)
            self.app_icon.setPixmap(pixmap)
        else:
            # 如果图标不存在，显示文本
            self.app_icon.setText("S")
            self.app_icon.setStyleSheet("""
                font-size: 20px;
                font-weight: bold;
                color: #ffffff;
            """)

        self.top_layout.addWidget(self.app_icon)

        # 添加分隔线
        self.separator = QFrame()
        self.separator.setFrameShape(QFrame.Shape.HLine)
        self.separator.setFrameShadow(QFrame.Shadow.Sunken)
        self.separator.setObjectName("menuSeparator")
        self.top_layout.addWidget(self.separator)

        # 添加菜单按钮
        self.serial_btn = self.create_menu_button("串口", "serial")
        self.top_layout.addWidget(self.serial_btn)

        # 添加传感器数据按钮
        self.data_btn = self.create_menu_button("数据", "chart")
        self.top_layout.addWidget(self.data_btn)

        # 添加一个spacer
        self.top_layout.addItem(QSpacerItem(20, 40, QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Expanding))

        # 添加顶部区域到主布局
        self.main_layout.addWidget(self.top_frame)

    def create_menu_button(self, tooltip, icon_name):
        """创建菜单按钮"""
        btn = QPushButton()
        btn.setToolTip(tooltip)
        btn.setMinimumHeight(50)
        btn.setObjectName("menuButton")

        # 尝试加载图标
        icon_path = os.path.join(APP_ROOT_PATH, "resources", "images", f"{icon_name}.png")
        if os.path.exists(icon_path):
            btn.setIcon(QIcon(icon_path))
            btn.setIconSize(QSize(24, 24))
        else:
            # 如果图标不存在，显示文本
            # 获取首字母
            if tooltip:
                btn.setText(tooltip[0])
            else:
                btn.setText("?")

        # 返回按钮
        return btn

    def select_menu_button(self, btn):
        """选择菜单按钮"""
        # 清除所有按钮的选中状态
        for button in self.findChildren(QPushButton):
            if button.objectName() == "menuButton":
                button.setStyleSheet("")

        # 设置当前按钮的选中状态
        btn.setStyleSheet("""
            QPushButton {
                border-left: 3px solid #bd93f9;
                background-color: #44475a;
            }
        """)