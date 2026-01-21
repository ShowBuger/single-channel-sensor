#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from PyQt6.QtCore import QSize, QEasingCurve, QPropertyAnimation, QRect


class UIFunctions:
    """UI辅助函数类"""

    @staticmethod
    def resize_grips(window):
        """调整窗口边框大小"""
        # 更新边框位置
        window.left_grip.setGeometry(0, 10, 10, window.height() - 20)
        window.right_grip.setGeometry(window.width() - 10, 10, 10, window.height() - 20)
        window.top_grip.setGeometry(10, 0, window.width() - 20, 10)
        window.bottom_grip.setGeometry(10, window.height() - 10, window.width() - 20, 10)

    @staticmethod
    def toggle_menu(window, widget, max_width, enable):
        """切换菜单展开/收起动画效果"""
        # 如果启用
        if enable:
            # 获取当前宽度
            width = widget.width()
            # 最大扩展
            if width == 0:
                width_extended = max_width
            else:
                width_extended = 0

            # 创建动画
            window.animation = QPropertyAnimation(widget, b"minimumWidth")
            window.animation.setDuration(300)
            window.animation.setStartValue(width)
            window.animation.setEndValue(width_extended)
            window.animation.setEasingCurve(QEasingCurve.Type.InOutQuart)
            window.animation.start()

    @staticmethod
    def apply_button_style(widget, object_name, bg_color, border_color="#000000", text_color="#FFFFFF",
                           border_radius=8):
        """应用按钮样式"""
        widget.setObjectName(object_name)
        widget.setStyleSheet(f"""
            QPushButton#{object_name} {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: {border_radius}px;
                padding: 5px;
            }}
            QPushButton#{object_name}:hover {{
                background-color: {bg_color};
                border: 2px solid {border_color};
            }}
            QPushButton#{object_name}:pressed {{
                background-color: {bg_color};
                border: 2px solid {border_color};
            }}
        """)

    @staticmethod
    def set_app_theme(app, theme):
        """设置应用主题"""
        if theme == "dark":
            stylesheet = """
            /* 黑色主题 */
            QWidget {
                background-color: #1a1a1a;
                color: #f0f0f0;
                font-family: "Segoe UI", Arial, sans-serif;
            }
            /* 其他样式... */
            """
        else:
            stylesheet = """
            /* 亮色主题 */
            QWidget {
                background-color: #f0f0f0;
                color: #212121;
                font-family: "Segoe UI", Arial, sans-serif;
            }
            /* 其他样式... */
            """
        app.setStyleSheet(stylesheet)