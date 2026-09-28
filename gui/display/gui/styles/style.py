#!/usr/bin/env python3
# -*- coding: utf-8 -*-

class Style:
    """样式表类，包含各种主题样式"""

    @staticmethod
    def get_style(theme="dracula"):
        """根据主题名获取对应的样式表"""
        if theme == "dracula":
            return Style.dracula_style()
        else:
            # 默认为Dracula主题
            return Style.dracula_style()

    @staticmethod
    def dracula_style():
        """Dracula主题样式表"""
        return """
        /* Dracula Theme */

        /* 主窗口 */
        QMainWindow {
            background-color: #282a36;
            border: 1px solid #44475a;
        }

        /* 标题栏 */
        #titleBar {
            background-color: #282a36;
            border-bottom: 1px solid #44475a;
        }

        #titleLabel {
            color: #f8f8f2;
            font-weight: bold;
            font-size: 12px;
        }

        #minimizeButton, #maximizeButton, #closeButton {
            background-color: transparent;
            border: none;
        }

        #minimizeButton:hover, #maximizeButton:hover {
            background-color: #44475a;
        }

        #closeButton:hover {
            background-color: #ff5555;
        }

        /* 左侧菜单 */
        #leftMenu {
            background-color: #2a2d3d;
            border-right: 1px solid #44475a;
        }

        #menuButton {
            color: #f8f8f2;
            border: none;
            border-radius: 4px;
            margin: 2px 5px;
            background-color: transparent;
            font-size: 16px;
        }

        #menuButton:hover {
            background-color: #44475a;
        }

        #menuSeparator {
            background-color: #44475a;
            height: 1px;
        }

        /* 内容区域 */
        QWidget {
            background-color: #282a36;
            color: #f8f8f2;
        }

        /* 标签 */
        QLabel {
            color: #f8f8f2;
        }

        #pageTitle {
            font-size: 18px;
            font-weight: bold;
            color: #bd93f9;
            margin-bottom: 10px;
        }

        #statusLabel {
            color: #8be9fd;
            font-size: 12px;
        }

        /* 按钮 */
        QPushButton {
            background-color: #44475a;
            color: #f8f8f2;
            border: 1px solid #6272a4;
            border-radius: 4px;
            padding: 5px 10px;
            font-size: 12px;
        }

        QPushButton:hover {
            background-color: #6272a4;
            border: 1px solid #bd93f9;
        }

        QPushButton:pressed {
            background-color: #bd93f9;
            color: #282a36;
        }

        #primaryButton {
            background-color: #bd93f9;
            color: #282a36;
            border: 1px solid #bd93f9;
        }

        #primaryButton:hover {
            background-color: #bd93f9;
            border: 1px solid #f8f8f2;
        }

        #primaryButton:pressed {
            background-color: #a173e8;
        }

        #secondaryButton {
            background-color: #44475a;
            color: #f8f8f2;
            border: 1px solid #6272a4;
        }

        #secondaryButton:hover {
            background-color: #6272a4;
        }

        #secondaryButton:pressed {
            background-color: #44475a;
        }

        /* 下拉框 */
        QComboBox {
            background-color: #44475a;
            color: #f8f8f2;
            border: 1px solid #6272a4;
            border-radius: 4px;
            padding: 5px;
            min-width: 6em;
        }

        QComboBox:hover {
            border: 1px solid #bd93f9;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 15px;
            border-left: 1px solid #6272a4;
        }

        QComboBox QAbstractItemView {
            background-color: #44475a;
            color: #f8f8f2;
            border: 1px solid #6272a4;
            selection-background-color: #6272a4;
            selection-color: #f8f8f2;
        }

        /* 文本框 */
        QTextEdit {
            background-color: #1e1f29;
            color: #f8f8f2;
            border: 1px solid #44475a;
            border-radius: 4px;
            padding: 5px;
            font-family: "Consolas", "Courier New", monospace;
        }

        QTextEdit:focus {
            border: 1px solid #bd93f9;
        }

        #receiveText {
            background-color: #1e1f29;
            color: #f8f8f2;
        }

        #sendText {
            background-color: #1e1f29;
            color: #f8f8f2;
        }

        /* 单行文本框 */
        QLineEdit {
            background-color: #1e1f29;
            color: #f8f8f2;
            border: 1px solid #44475a;
            border-radius: 4px;
            padding: 5px;
        }

        QLineEdit:focus {
            border: 1px solid #bd93f9;
        }

        /* 选项卡 */
        QTabWidget {
            background-color: #282a36;
        }

        QTabWidget::pane {
            border: 1px solid #44475a;
            background-color: #282a36;
        }

        QTabBar::tab {
            background-color: #282a36;
            color: #f8f8f2;
            border: 1px solid #44475a;
            border-bottom: none;
            padding: 5px 10px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        QTabBar::tab:selected {
            background-color: #44475a;
            border: 1px solid #bd93f9;
            border-bottom: none;
        }

        QTabBar::tab:hover:!selected {
            background-color: #44475a;
        }

        /* 分组框 */
        QGroupBox {
            background-color: #282a36;
            color: #bd93f9;
            border: 1px solid #44475a;
            border-radius: 4px;
            margin-top: 10px;
            font-weight: bold;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 5px;
        }

        /* 复选框 */
        QCheckBox {
            color: #f8f8f2;
            spacing: 5px;
        }

        QCheckBox::indicator {
            width: 15px;
            height: 15px;
        }

        QCheckBox::indicator:unchecked {
            background-color: #44475a;
            border: 1px solid #6272a4;
            border-radius: 3px;
        }

        QCheckBox::indicator:checked {
            background-color: #bd93f9;
            border: 1px solid #bd93f9;
            border-radius: 3px;
        }

        /* 滚动条 */
        QScrollBar:vertical {
            background-color: #282a36;
            width: 10px;
            margin: 0;
        }

        QScrollBar::handle:vertical {
            background-color: #44475a;
            min-height: 20px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #6272a4;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            background: none;
            height: 0px;
        }

        QScrollBar:horizontal {
            background-color: #282a36;
            height: 10px;
            margin: 0;
        }

        QScrollBar::handle:horizontal {
            background-color: #44475a;
            min-width: 20px;
            border-radius: 5px;
        }

        QScrollBar::handle:horizontal:hover {
            background-color: #6272a4;
        }

        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            background: none;
            width: 0px;
        }

        /* 控制框架 */
        #controlFrame {
            background-color: #1e1f29;
            border: 1px solid #44475a;
            border-radius: 4px;
        }

        /* 自定义边框 */
        #customGrip {
            background-color: transparent;
        }
 /* 表格样式 */
    QTableWidget {
        background-color: #282a36;
        color: #f8f8f2;
        gridline-color: #44475a;
        border: 1px solid #44475a;
        border-radius: 4px;
    }
    
    /* 表头样式 */
    QHeaderView::section {
        background-color: #282a36;
        color: #f8f8f2;
        padding: 4px;
        border: 1px solid #44475a;
    }
    
    /* 表格第一列样式 */
    QTableWidget::item:first-column {
        background-color: #282a36;
        color: #f8f8f2;
    }
    
    /* 表格选中项样式 */
    QTableWidget::item:selected {
        background-color: #44475a;
        color: #f8f8f2;
    }
    
    /* 表格悬停项样式 */
    QTableWidget::item:hover {
        background-color: #383a4c;
    }
    
    /* 进度条样式 */
    QProgressBar {
        border: 1px solid #44475a;
        border-radius: 3px;
        background-color: #1e1f29;
        text-align: center;
        color: #f8f8f2;
    }
    
    QProgressBar::chunk {
        background-color: #bd93f9;
        width: 1px;
    }
    
    /* 执行状态组框样式 */
    QGroupBox[title="执行状态"] {
        background-color: #282a36;
        color: #f8f8f2;
    }
    """

