#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from PyQt6.QtCore import Qt, QPoint
from PyQt6.QtWidgets import QSizeGrip


class CustomGrip(QSizeGrip):
    """自定义窗口大小调整边框"""

    def __init__(self, parent, edge):
        super().__init__(parent)
        self.parent = parent
        self.edge = edge
        self.setObjectName("customGrip")

        # 设置鼠标追踪，以便于mouseMoveEvent触发
        self.setMouseTracking(True)

    def mousePressEvent(self, event):
        """鼠标按下事件"""
        # 记录鼠标位置和窗口大小
        if event.button() == Qt.MouseButton.LeftButton:
            self.mouse_pos = event.globalPosition().toPoint()
            self.window_rect = self.parent.geometry()
            event.accept()
        else:
            super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        """鼠标移动事件，动态调整窗口大小"""
        if event.buttons() == Qt.MouseButton.LeftButton:
            # 计算移动的偏移量
            delta = event.globalPosition().toPoint() - self.mouse_pos
            new_rect = self.window_rect.translated(0, 0)  # 创建一个窗口矩形的副本

            # 根据边缘调整窗口大小
            if self.edge == Qt.Edge.LeftEdge:
                left = self.window_rect.left() + delta.x()
                new_rect.setLeft(left)
            elif self.edge == Qt.Edge.RightEdge:
                right = self.window_rect.right() + delta.x()
                new_rect.setRight(right)
            elif self.edge == Qt.Edge.TopEdge:
                top = self.window_rect.top() + delta.y()
                new_rect.setTop(top)
            elif self.edge == Qt.Edge.BottomEdge:
                bottom = self.window_rect.bottom() + delta.y()
                new_rect.setBottom(bottom)

            # 应用新的窗口大小
            if new_rect.width() >= self.parent.minimumWidth() and new_rect.height() >= self.parent.minimumHeight():
                self.parent.setGeometry(new_rect)

            event.accept()
        else:
            super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event):
        """鼠标释放事件"""
        if event.button() == Qt.MouseButton.LeftButton:
            event.accept()
        else:
            super().mouseReleaseEvent(event)