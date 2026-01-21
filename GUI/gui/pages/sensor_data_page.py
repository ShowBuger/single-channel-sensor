#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from PyQt6.QtCore import Qt, pyqtSlot, QTimer, pyqtSignal
from PyQt6.QtGui import QColor
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QPushButton, QLabel, QComboBox, QCheckBox,
    QGroupBox, QFrame, QScrollArea, QListWidget,
    QListWidgetItem, QSizePolicy, QSpinBox, QDoubleSpinBox,
    QFileDialog
)
import pyqtgraph as pg
import numpy as np
from datetime import datetime
import os
import matplotlib
matplotlib.use('QtAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from matplotlib import cm
import platform


class SensorDataPage(QWidget):
    """传感器数据页面，用于展示传感器数据曲线"""

    # 定义信号：传感器选择变化
    sensor_selected_signal = pyqtSignal(int)

    def __init__(self, main_window, sensor_data_manager):
        super().__init__()

        self.main_window = main_window
        self.sensor_data_manager = sensor_data_manager
        
        # 获取串口管理器引用，用于发送标定命令
        if hasattr(main_window, 'serial_manager'):
            self.serial_manager = main_window.serial_manager
        else:
            self.serial_manager = None

        # 选中的传感器ID列表
        self.selected_sensors = []
        
        # 当前选中的单一传感器ID（用于与3D力场可视化同步）
        self.current_selected_sensor = 0

        # 传感器控制相关变量
        self.available_sensors = []  # 扫描后可用的传感器列表
        self.currently_plotting_sensor = None  # 当前正在绘制的传感器ID
        self.is_started = False  # 是否已启动传感器数据接收

        # 创建图表数据和曲线字典
        self.plot_curves = {}  # {sensor_id: {'data1': curve, 'data2': curve, 'data3': curve}}
        self.plot_data = {}  # {sensor_id: {'data1': [], 'data2': [], 'data3': []}}

        # 最大数据点数量，用于限制绘图数据量
        self.max_data_points = 500
        # 示波器滚动显示：可见窗口的数据点数量
        self.visible_points = 50  # 屏幕上可见的数据点数量

        # 3D力变形网格相关变量
        self.force_data = [0, 0, 0]
        self.force_history = np.zeros((10, 3))
        self.resolution = 20
        self.grid_x_range = 5
        self.grid_y_range = 5
        self.last_force_data = [0, 0, 0]
        self.force_change_threshold = 0.01
        self.skip_frame_count = 0
        self.max_skip_frames = 2
        self.surf = None
        self.force_arrow = None
        self.origin_point = None
        self.cbar = None
        # 使用柔和明亮的配色
        self.current_colormap = 'Spectral_r'
        self.colormap_options = {
            'Spectral_r': cm.Spectral_r,  # 柔和的光谱色（反转）
            'RdYlBu_r': cm.RdYlBu_r,      # 红黄蓝柔和过渡（反转）
            'YlOrRd': cm.YlOrRd,          # 黄橙红柔和过渡
            'plasma': cm.plasma,          # 明亮的紫红黄
            'viridis': cm.viridis,        # 经典科学配色
            'cividis': cm.cividis,        # 色盲友好配色
            'coolwarm': cm.coolwarm,      # 冷暖色过渡
        }
        self.scientific_colors = {
            # 适中的透明度、使用柔和明亮的配色
            'surface_alpha': 0.75,
            'arrow_color': '#ff6b6b',      # 柔和的红色箭头
            'origin_color': '#4ecdc4',     # 柔和的青色原点
            'grid_color': '#6c7aa6',
            'text_color': '#f8f8f2',
        }
        self.axis_config = {
            'x_min': -5.0,
            'x_max': 5.0,
            'y_min': -5.0,
            'y_max': 5.0,
            'z_min': -5.0,
            'z_max': 5.0,
            'show_grid': True,
            'grid_alpha': 0.3,
            'tick_color': '#f8f8f2',
            'label_color': '#f8f8f2',
        }

        # 数据映射范围设置（将传感器数据映射到-5到5范围内）
        # 从设置中读取映射范围
        settings = main_window.settings if hasattr(main_window, 'settings') else None
        mapping_settings = settings.get_setting("sensor_mapping") if settings else None

        # 基础范围（用户设置的范围）
        if mapping_settings:
            self.base_mapping_range = {
                'x_min': mapping_settings.get('x_min', -10.0),
                'x_max': mapping_settings.get('x_max', 10.0),
                'y_min': mapping_settings.get('y_min', -10.0),
                'y_max': mapping_settings.get('y_max', 10.0),
                'z_min': mapping_settings.get('z_min', -10.0),
                'z_max': mapping_settings.get('z_max', 10.0),
            }
        else:
            self.base_mapping_range = {
                'x_min': -10.0,
                'x_max': 10.0,
                'y_min': -10.0,
                'y_max': 10.0,
                'z_min': -10.0,
                'z_max': 10.0,
            }

        # 当前实际使用的映射范围（可能因自动缩放而变化）
        self.mapping_range = {
            'x_min': self.base_mapping_range['x_min'],
            'x_max': self.base_mapping_range['x_max'],
            'y_min': self.base_mapping_range['y_min'],
            'y_max': self.base_mapping_range['y_max'],
            'z_min': self.base_mapping_range['z_min'],
            'z_max': self.base_mapping_range['z_max'],
        }

        # 2D曲线图Y轴自动跟随范围设置
        self.curve_y_axis_base_range = {
            'x_min': -10.0,
            'x_max': 10.0,
            'y_min': -10.0,
            'y_max': 10.0,
            'z_min': -10.0,
            'z_max': 10.0,
        }
        # 当前Y轴实际范围（可能因自动跟随而变化）
        self.curve_y_axis_range = {
            'x_min': self.curve_y_axis_base_range['x_min'],
            'x_max': self.curve_y_axis_base_range['x_max'],
            'y_min': self.curve_y_axis_base_range['y_min'],
            'y_max': self.curve_y_axis_base_range['y_max'],
            'z_min': self.curve_y_axis_base_range['z_min'],
            'z_max': self.curve_y_axis_base_range['z_max'],
        }

        # 创建基础网格
        self.create_base_grid()

        # 创建UI
        self.setup_ui()

        # 连接信号
        self.connect_signals()

        # 创建定时器用于定期更新图表
        self.update_timer = QTimer(self)
        self.update_timer.timeout.connect(self.update_plots)
        self.update_timer.start(100)  # 每100ms更新一次

        # 创建定时器用于更新3D可视化
        self.force_viz_timer = QTimer(self)
        self.force_viz_timer.timeout.connect(self.update_force_visualization)
        self.force_viz_timer.start(50)  # 每50ms更新一次3D可视化

    def setup_ui(self):
        """设置UI布局 - 三列设计：左侧（串口连接+传感器控制）、中间（3D网格）、右侧（三个单轴曲线）"""
        # 设置尺寸策略，使组件能够正确分配空间
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        # 创建主布局
        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(5, 5, 5, 5)
        self.main_layout.setSpacing(5)

        # 创建三列布局容器
        self.content_layout = QHBoxLayout()
        self.content_layout.setContentsMargins(0, 0, 0, 0)
        self.content_layout.setSpacing(8)

        # ========== 左侧面板：串口连接 + 传感器控制 ==========
        self.left_panel = QFrame()
        self.left_panel.setObjectName("leftPanel")
        self.left_panel.setMinimumWidth(240)
        self.left_panel.setMaximumWidth(280)
        self.left_panel_layout = QVBoxLayout(self.left_panel)
        self.left_panel_layout.setContentsMargins(12, 12, 12, 12)
        self.left_panel_layout.setSpacing(10)

        # 串口连接组
        self.serial_group = QGroupBox("串口连接")
        self.serial_group.setObjectName("serialGroup")
        self.serial_group_layout = QVBoxLayout(self.serial_group)
        self.serial_group_layout.setContentsMargins(8, 12, 8, 8)
        self.serial_group_layout.setSpacing(8)

        # 端口号选择
        port_layout = QHBoxLayout()
        port_label = QLabel("端口号:")
        port_label.setStyleSheet("background-color: transparent;")
        port_layout.addWidget(port_label)
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(120)
        port_layout.addWidget(self.port_combo)
        self.refresh_port_btn = QPushButton("刷新")
        self.refresh_port_btn.setObjectName("secondaryButton")
        self.refresh_port_btn.setMaximumWidth(60)
        port_layout.addWidget(self.refresh_port_btn)
        self.serial_group_layout.addLayout(port_layout)

        # 波特率选择
        baudrate_layout = QHBoxLayout()
        baudrate_label = QLabel("波特率:")
        baudrate_label.setStyleSheet("background-color: transparent;")
        baudrate_layout.addWidget(baudrate_label)
        self.baudrate_combo = QComboBox()
        self.baudrate_combo.addItems(["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "500000", "921600", "1000000"])
        self.baudrate_combo.setCurrentText("115200")
        baudrate_layout.addWidget(self.baudrate_combo)
        self.serial_group_layout.addLayout(baudrate_layout)

        # 连接/断开按钮
        self.connect_btn = QPushButton("连接")
        self.connect_btn.setObjectName("primaryButton")
        self.connect_btn.setMinimumHeight(36)
        self.serial_group_layout.addWidget(self.connect_btn)

        self.left_panel_layout.addWidget(self.serial_group)

        # 传感器控制组
        self.control_group = QGroupBox("传感器控制")
        self.control_group.setObjectName("controlGroup")
        self.control_group_layout = QVBoxLayout(self.control_group)
        self.control_group_layout.setContentsMargins(8, 12, 8, 8)
        self.control_group_layout.setSpacing(8)

        # 启动按钮（只保留启动功能，使用sensor0）
        self.start_btn = QPushButton("启动")
        self.start_btn.setObjectName("primaryButton")
        self.start_btn.setMinimumHeight(36)
        self.control_group_layout.addWidget(self.start_btn)

        self.left_panel_layout.addWidget(self.control_group)

        # 标定按钮组
        self.calibrate_group = QGroupBox("传感器标定")
        self.calibrate_group.setObjectName("calibrateGroup")
        self.calibrate_group_layout = QVBoxLayout(self.calibrate_group)
        self.calibrate_group_layout.setContentsMargins(8, 12, 8, 8)
        self.calibrate_group_layout.setSpacing(8)

        # 标定按钮
        self.calibrate_btn = QPushButton("标定传感器")
        self.calibrate_btn.setObjectName("primaryButton")
        self.calibrate_btn.setMinimumHeight(36)
        self.calibrate_group_layout.addWidget(self.calibrate_btn)

        self.left_panel_layout.addWidget(self.calibrate_group)

        # 数据操作组
        self.operation_group = QGroupBox("数据操作")
        self.operation_group.setObjectName("operationGroup")
        self.operation_group_layout = QVBoxLayout(self.operation_group)
        self.operation_group_layout.setContentsMargins(8, 12, 8, 8)
        self.operation_group_layout.setSpacing(8)

        self.data_buttons_layout = QHBoxLayout()
        self.data_buttons_layout.setSpacing(8)
        self.clear_btn = QPushButton("清空数据")
        self.clear_btn.setObjectName("secondaryButton")
        self.clear_btn.setMinimumHeight(36)
        self.data_buttons_layout.addWidget(self.clear_btn)

        self.save_btn = QPushButton("保存数据")
        self.save_btn.setObjectName("primaryButton")
        self.save_btn.setMinimumHeight(36)
        self.data_buttons_layout.addWidget(self.save_btn)

        self.operation_group_layout.addLayout(self.data_buttons_layout)

        self.left_panel_layout.addWidget(self.operation_group)

        # 添加弹性空间
        self.left_panel_layout.addStretch()

        # 状态标签
        self.status_label = QLabel("就绪")
        self.status_label.setObjectName("statusLabel")
        self.status_label.setWordWrap(True)
        self.status_label.setMinimumHeight(40)
        self.status_label.setStyleSheet("padding: 8px; background-color: #1e1e1e; border-radius: 4px; border-left: 3px solid #50fa7b;")
        self.left_panel_layout.addWidget(self.status_label)

        self.content_layout.addWidget(self.left_panel)

        # ========== 中间面板：3D网格 ==========
        self.middle_panel = QFrame()
        self.middle_panel.setObjectName("middlePanel")
        self.middle_panel.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.middle_panel_layout = QVBoxLayout(self.middle_panel)
        self.middle_panel_layout.setContentsMargins(5, 5, 5, 5)
        self.middle_panel_layout.setSpacing(0)

        # 3D力变形网格
        self.force_canvas = self.create_force_canvas()
        self.middle_panel_layout.addWidget(self.force_canvas)

        self.content_layout.addWidget(self.middle_panel, 3)  # 中间列占据更多空间（拉伸因子3）

        # ========== 右侧面板：三个单轴曲线（垂直排列）==========
        self.right_panel = QFrame()
        self.right_panel.setObjectName("rightPanel")
        self.right_panel.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.right_panel.setMinimumWidth(250)  # 减小最小宽度
        self.right_panel.setMaximumWidth(350)  # 设置最大宽度限制
        self.right_panel_layout = QVBoxLayout(self.right_panel)
        self.right_panel_layout.setContentsMargins(5, 5, 5, 5)
        self.right_panel_layout.setSpacing(8)

        # X轴图表
        self.plot_widget_x = pg.PlotWidget()
        self.plot_widget_x.setBackground('#282a36')
        self.plot_widget_x.setLabel('left', 'X轴力值', color='#f8f8f2', size='11pt')
        self.plot_widget_x.setLabel('bottom', '采样点', color='#f8f8f2', size='11pt')
        self.plot_widget_x.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget_x.setTitle("<span style='color: #ff5555; font-size: 12pt; font-weight: bold;'>X轴传感器数据</span>")
        self.plot_widget_x.addLegend(offset=(10, 10))
        self.plot_widget_x.getPlotItem().getViewBox().setYRange(
            self.curve_y_axis_base_range['x_min'],
            self.curve_y_axis_base_range['x_max'],
            padding=0.1
        )
        # 设置示波器滚动模式：初始X轴范围
        self.plot_widget_x.getPlotItem().getViewBox().setXRange(0, self.visible_points, padding=0)
        # 设置坐标轴颜色
        self.plot_widget_x.getPlotItem().getAxis('left').setPen('#f8f8f2')
        self.plot_widget_x.getPlotItem().getAxis('bottom').setPen('#f8f8f2')
        self.plot_widget_x.getPlotItem().getAxis('left').setTextPen('#f8f8f2')
        self.plot_widget_x.getPlotItem().getAxis('bottom').setTextPen('#f8f8f2')
        # 添加X轴数值显示文本
        self.x_value_text = pg.TextItem(text="X: --", color='#ff5555', anchor=(0, 0))
        self.x_value_text.setFont(pg.QtGui.QFont("Arial", 12, pg.QtGui.QFont.Weight.Bold))
        self.plot_widget_x.addItem(self.x_value_text)
        self.right_panel_layout.addWidget(self.plot_widget_x, 1)

        # Y轴图表
        self.plot_widget_y = pg.PlotWidget()
        self.plot_widget_y.setBackground('#282a36')
        self.plot_widget_y.setLabel('left', 'Y轴力值', color='#f8f8f2', size='11pt')
        self.plot_widget_y.setLabel('bottom', '采样点', color='#f8f8f2', size='11pt')
        self.plot_widget_y.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget_y.setTitle("<span style='color: #50fa7b; font-size: 12pt; font-weight: bold;'>Y轴传感器数据</span>")
        self.plot_widget_y.addLegend(offset=(10, 10))
        self.plot_widget_y.getPlotItem().getViewBox().setYRange(
            self.curve_y_axis_base_range['y_min'],
            self.curve_y_axis_base_range['y_max'],
            padding=0.1
        )
        # 设置示波器滚动模式：初始X轴范围
        self.plot_widget_y.getPlotItem().getViewBox().setXRange(0, self.visible_points, padding=0)
        # 设置坐标轴颜色
        self.plot_widget_y.getPlotItem().getAxis('left').setPen('#f8f8f2')
        self.plot_widget_y.getPlotItem().getAxis('bottom').setPen('#f8f8f2')
        self.plot_widget_y.getPlotItem().getAxis('left').setTextPen('#f8f8f2')
        self.plot_widget_y.getPlotItem().getAxis('bottom').setTextPen('#f8f8f2')
        # 添加Y轴数值显示文本
        self.y_value_text = pg.TextItem(text="Y: --", color='#50fa7b', anchor=(0, 0))
        self.y_value_text.setFont(pg.QtGui.QFont("Arial", 12, pg.QtGui.QFont.Weight.Bold))
        self.plot_widget_y.addItem(self.y_value_text)
        self.right_panel_layout.addWidget(self.plot_widget_y, 1)

        # Z轴图表
        self.plot_widget_z = pg.PlotWidget()
        self.plot_widget_z.setBackground('#282a36')
        self.plot_widget_z.setLabel('left', 'Z轴力值', color='#f8f8f2', size='11pt')
        self.plot_widget_z.setLabel('bottom', '采样点', color='#f8f8f2', size='11pt')
        self.plot_widget_z.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget_z.setTitle("<span style='color: #8be9fd; font-size: 12pt; font-weight: bold;'>Z轴传感器数据</span>")
        self.plot_widget_z.addLegend(offset=(10, 10))
        self.plot_widget_z.getPlotItem().getViewBox().setYRange(
            self.curve_y_axis_base_range['z_min'],
            self.curve_y_axis_base_range['z_max'],
            padding=0.1
        )
        # 设置示波器滚动模式：初始X轴范围
        self.plot_widget_z.getPlotItem().getViewBox().setXRange(0, self.visible_points, padding=0)
        # 设置坐标轴颜色
        self.plot_widget_z.getPlotItem().getAxis('left').setPen('#f8f8f2')
        self.plot_widget_z.getPlotItem().getAxis('bottom').setPen('#f8f8f2')
        self.plot_widget_z.getPlotItem().getAxis('left').setTextPen('#f8f8f2')
        self.plot_widget_z.getPlotItem().getAxis('bottom').setTextPen('#f8f8f2')
        # 添加Z轴数值显示文本
        self.z_value_text = pg.TextItem(text="Z: --", color='#8be9fd', anchor=(0, 0))
        self.z_value_text.setFont(pg.QtGui.QFont("Arial", 12, pg.QtGui.QFont.Weight.Bold))
        self.plot_widget_z.addItem(self.z_value_text)
        self.right_panel_layout.addWidget(self.plot_widget_z, 1)

        self.content_layout.addWidget(self.right_panel)

        # 将内容布局添加到主布局
        self.main_layout.addLayout(self.content_layout, 1)

    def create_base_grid(self):
        """创建基础网格"""
        x = np.linspace(-self.grid_x_range, self.grid_x_range, self.resolution)
        y = np.linspace(-self.grid_y_range, self.grid_y_range, self.resolution)
        self.X, self.Y = np.meshgrid(x, y)
        self.Z = np.zeros_like(self.X)

    def create_force_canvas(self):
        """创建3D力变形网格画布（只保留表面，无标题、无colorbar、无坐标轴）"""
        # 配置matplotlib字体
        matplotlib.rcParams['axes.unicode_minus'] = False
        if platform.system() == 'Windows':
            matplotlib.rcParams['font.sans-serif'] = ['SimHei', 'Microsoft YaHei', 'DejaVu Sans']
        
        # 创建Figure和Canvas
        fig = Figure(figsize=(4, 3), dpi=100)
        fig.patch.set_facecolor('#282a36')
        canvas = FigureCanvas(fig)
        canvas.setMinimumSize(400, 300)
        
        # 创建3D子图
        ax = fig.add_subplot(111, projection='3d')
        ax.set_facecolor('#282a36')
        
        # 隐藏所有坐标轴
        ax.set_axis_off()
        
        # 设置坐标轴范围（虽然隐藏了，但需要设置范围以正确显示）
        ax.set_xlim(self.axis_config['x_min'], self.axis_config['x_max'])
        ax.set_ylim(self.axis_config['y_min'], self.axis_config['y_max'])
        ax.set_zlim(self.axis_config['z_min'], self.axis_config['z_max'])
        ax.view_init(elev=30, azim=45)
        
        # 不显示网格（因为坐标轴已隐藏）
        ax.grid(False)
        
        # 绘制初始平面
        self.surf = ax.plot_surface(
            self.X, self.Y, self.Z,
            cmap=self.colormap_options[self.current_colormap],
            linewidth=0.3,
            antialiased=False,
            alpha=self.scientific_colors['surface_alpha'],
            rcount=self.resolution,
            ccount=self.resolution,
            vmin=-5.0,  # 固定颜色映射范围最小值
            vmax=5.0    # 固定颜色映射范围最大值
        )
        
        # 不添加颜色条（删除colorbar）
        self.cbar = None
        
        # 保存ax引用
        canvas.ax = ax
        canvas.fig = fig
        
        return canvas

    def map_sensor_data(self, values):
        """将传感器数据映射到-5到5的范围内，支持自动缩放

        Args:
            values: [x, y, z] 原始传感器数据

        Returns:
            [x_mapped, y_mapped, z_mapped] 映射后的数据
        """
        x, y, z = values

        # 自动缩放逻辑：如果数据超出基础范围，扩大映射范围
        # X轴
        if x > self.base_mapping_range['x_max']:
            self.mapping_range['x_max'] = max(self.mapping_range['x_max'], x * 1.1)
        elif x < self.base_mapping_range['x_min']:
            self.mapping_range['x_min'] = min(self.mapping_range['x_min'], x * 1.1)
        else:
            # 数据回到基础范围内，逐渐恢复到基础范围
            if self.mapping_range['x_max'] > self.base_mapping_range['x_max']:
                self.mapping_range['x_max'] = max(self.base_mapping_range['x_max'],
                                                   self.mapping_range['x_max'] * 0.98)
            if self.mapping_range['x_min'] < self.base_mapping_range['x_min']:
                self.mapping_range['x_min'] = min(self.base_mapping_range['x_min'],
                                                   self.mapping_range['x_min'] * 0.98)

        # Y轴
        if y > self.base_mapping_range['y_max']:
            self.mapping_range['y_max'] = max(self.mapping_range['y_max'], y * 1.1)
        elif y < self.base_mapping_range['y_min']:
            self.mapping_range['y_min'] = min(self.mapping_range['y_min'], y * 1.1)
        else:
            if self.mapping_range['y_max'] > self.base_mapping_range['y_max']:
                self.mapping_range['y_max'] = max(self.base_mapping_range['y_max'],
                                                   self.mapping_range['y_max'] * 0.98)
            if self.mapping_range['y_min'] < self.base_mapping_range['y_min']:
                self.mapping_range['y_min'] = min(self.base_mapping_range['y_min'],
                                                   self.mapping_range['y_min'] * 0.98)

        # Z轴
        if z > self.base_mapping_range['z_max']:
            self.mapping_range['z_max'] = max(self.mapping_range['z_max'], z * 1.1)
        elif z < self.base_mapping_range['z_min']:
            self.mapping_range['z_min'] = min(self.mapping_range['z_min'], z * 1.1)
        else:
            if self.mapping_range['z_max'] > self.base_mapping_range['z_max']:
                self.mapping_range['z_max'] = max(self.base_mapping_range['z_max'],
                                                   self.mapping_range['z_max'] * 0.98)
            if self.mapping_range['z_min'] < self.base_mapping_range['z_min']:
                self.mapping_range['z_min'] = min(self.base_mapping_range['z_min'],
                                                   self.mapping_range['z_min'] * 0.98)

        # X轴映射
        x_mapped = np.clip(
            (x - self.mapping_range['x_min']) / (self.mapping_range['x_max'] - self.mapping_range['x_min']) * 10 - 5,
            -5.0, 5.0
        )

        # Y轴映射
        y_mapped = np.clip(
            (y - self.mapping_range['y_min']) / (self.mapping_range['y_max'] - self.mapping_range['y_min']) * 10 - 5,
            -5.0, 5.0
        )

        # Z轴映射
        z_mapped = np.clip(
            (z - self.mapping_range['z_min']) / (self.mapping_range['z_max'] - self.mapping_range['z_min']) * 10 - 5,
            -5.0, 5.0
        )

        return [x_mapped, y_mapped, z_mapped]

    def update_force_visualization(self):
        """更新3D力变形网格可视化"""
        if not hasattr(self, 'force_canvas') or self.force_canvas is None:
            return
        
        if self.currently_plotting_sensor is None:
            return
        
        # 获取当前传感器的数据
        sensor_data = self.sensor_data_manager.get_sensor_data(self.currently_plotting_sensor)
        if not sensor_data:
            return
        
        latest_values = sensor_data.get_latest_values()
        self.force_data = latest_values

        # 将原始数据映射到-5到5的范围内
        mapped_values = self.map_sensor_data(latest_values)

        # 更新力历史记录（使用映射后的数据）
        self.force_history = np.roll(self.force_history, 1, axis=0)
        self.force_history[0] = mapped_values

        # 检查力值是否有显著变化
        current_force = np.array(mapped_values)
        last_force = np.array(self.last_force_data)
        force_change = np.linalg.norm(current_force - last_force)

        if force_change < self.force_change_threshold:
            self.skip_frame_count += 1
            if self.skip_frame_count < self.max_skip_frames:
                return
            else:
                self.skip_frame_count = 0
        else:
            self.skip_frame_count = 0
            self.last_force_data = current_force.copy().tolist()

        # 计算平均力（平滑动画效果）
        avg_force = np.mean(self.force_history[:3], axis=0)
        fx, fy, fz = avg_force
        
        # 计算到原点的距离
        distance = np.sqrt(self.X ** 2 + self.Y ** 2)
        
        # 衰减函数 - 使用高斯衰减
        sigma = 3.0
        decay = np.exp(-(distance ** 2) / (2 * sigma ** 2))
        
        # XY平面变形（基于X和Y分量）
        X_new = self.X + fx * decay * 0.5
        Y_new = self.Y + fy * decay * 0.5
        
        # Z方向变形（基于Z分量）
        Z_new = fz * decay * 1.0
        
        # 更新表面
        if self.surf is not None:
            self.surf.remove()
        
        ax = self.force_canvas.ax
        self.surf = ax.plot_surface(
            X_new, Y_new, Z_new,
            cmap=self.colormap_options[self.current_colormap],
            linewidth=0.3,
            antialiased=False,
            alpha=self.scientific_colors['surface_alpha'],
            rcount=self.resolution,
            ccount=self.resolution,
            vmin=-5.0,  # 固定颜色映射范围最小值
            vmax=5.0    # 固定颜色映射范围最大值
        )
        
        # 隐藏力向量箭头
        # if self.force_arrow is not None:
        #     self.force_arrow.remove()
        #
        # scale = 2.0
        # self.force_arrow = ax.quiver(
        #     0, 0, 0,
        #     fx * scale,
        #     fy * scale,
        #     fz * scale,
        #     color=self.scientific_colors['arrow_color'],
        #     linewidth=2,
        #     arrow_length_ratio=0.15
        # )
        
        # 更新原点
        if self.origin_point is not None:
            self.origin_point.remove()
        
        self.origin_point = ax.scatter([0], [0], [0],
                                     color=self.scientific_colors['origin_color'], s=30)
        
        # 不再更新颜色条（已删除）
        # if self.cbar is not None:
        #     self.cbar.update_normal(self.surf)
        
        # 绘制画布
        self.force_canvas.draw_idle()

    def connect_signals(self):
        """连接信号和槽"""
        # 按钮事件
        self.start_btn.clicked.connect(self.on_start_clicked)
        self.clear_btn.clicked.connect(self.clear_data)
        self.save_btn.clicked.connect(self.save_data)
        self.calibrate_btn.clicked.connect(self.send_calibration_command)

        # 串口连接相关事件
        if self.serial_manager:
            self.refresh_port_btn.clicked.connect(self.refresh_ports)
            self.connect_btn.clicked.connect(self.toggle_serial_connection)
            # 监听串口连接状态变化
            self.serial_manager.connected_signal.connect(self.on_serial_connection_changed)
            # 连接串口接收数据信号
            self.serial_manager.received_data_signal.connect(self.on_serial_data_received)

        # 传感器数据更新事件
        self.sensor_data_manager.data_updated_signal.connect(self.on_sensor_data_updated)

        # 初始化时刷新端口列表
        if self.serial_manager:
            self.refresh_ports()


    def update_current_values_display(self):
        """更新当前传感器数值显示在图表左上角的固定区域"""
        if not self.selected_sensors or self.currently_plotting_sensor is None:
            # 重置所有数值文本
            self.x_value_text.setText("X: --")
            self.y_value_text.setText("Y: --")
            self.z_value_text.setText("Z: --")
            return

        # 获取当前正在绘制的传感器数据
        sensor_id = self.currently_plotting_sensor
        sensor_data = self.sensor_data_manager.get_sensor_data(sensor_id)

        if sensor_data:
            latest_values = sensor_data.get_latest_values()
            # 更新每个图表上的数值显示
            self.x_value_text.setText(f"X: {latest_values[0]:.2f}")
            self.y_value_text.setText(f"Y: {latest_values[1]:.2f}")
            self.z_value_text.setText(f"Z: {latest_values[2]:.2f}")

            # 将文本固定在每个图表的左上角
            # 获取图表的视图范围以设置文本位置
            try:
                x_range = self.plot_widget_x.getPlotItem().getViewBox().viewRange()
                y_range_x = x_range[1]
                self.x_value_text.setPos(x_range[0][0] + (x_range[0][1] - x_range[0][0]) * 0.05, 
                                        y_range_x[1] - (y_range_x[1] - y_range_x[0]) * 0.15)

                y_range = self.plot_widget_y.getPlotItem().getViewBox().viewRange()
                y_range_y = y_range[1]
                self.y_value_text.setPos(y_range[0][0] + (y_range[0][1] - y_range[0][0]) * 0.05,
                                        y_range_y[1] - (y_range_y[1] - y_range_y[0]) * 0.15)

                z_range = self.plot_widget_z.getPlotItem().getViewBox().viewRange()
                y_range_z = z_range[1]
                self.z_value_text.setPos(z_range[0][0] + (z_range[0][1] - z_range[0][0]) * 0.05,
                                        y_range_z[1] - (y_range_z[1] - y_range_z[0]) * 0.15)
            except:
                # 如果获取范围失败，使用默认位置
                pass
        else:
            # 没有数据时显示默认值
            self.x_value_text.setText("X: --")
            self.y_value_text.setText("Y: --")
            self.z_value_text.setText("Z: --")

    def update_plot_curves(self):
        """更新图表曲线"""
        # 清空当前曲线
        self.plot_widget_x.clear()
        self.plot_widget_x.addLegend()
        self.plot_widget_y.clear()
        self.plot_widget_y.addLegend()
        self.plot_widget_z.clear()
        self.plot_widget_z.addLegend()

        # 重新添加文本项（因为clear()会删除所有项）
        self.x_value_text = pg.TextItem(text="X: --", color='#ff5555', anchor=(0, 0))
        self.x_value_text.setFont(pg.QtGui.QFont("Arial", 12, pg.QtGui.QFont.Weight.Bold))
        self.plot_widget_x.addItem(self.x_value_text)

        self.y_value_text = pg.TextItem(text="Y: --", color='#50fa7b', anchor=(0, 0))
        self.y_value_text.setFont(pg.QtGui.QFont("Arial", 12, pg.QtGui.QFont.Weight.Bold))
        self.plot_widget_y.addItem(self.y_value_text)

        self.z_value_text = pg.TextItem(text="Z: --", color='#8be9fd', anchor=(0, 0))
        self.z_value_text.setFont(pg.QtGui.QFont("Arial", 12, pg.QtGui.QFont.Weight.Bold))
        self.plot_widget_z.addItem(self.z_value_text)

        # 颜色列表，用于区分不同传感器
        colors = [
            (255, 0, 0),  # 红色
            (0, 255, 0),  # 绿色
            (0, 0, 255),  # 蓝色
            (255, 255, 0),  # 黄色
            (0, 255, 255),  # 青色
            (255, 0, 255),  # 洋红色
            (128, 128, 0),  # 橄榄色
            (128, 0, 128),  # 紫色
            (0, 128, 128),  # 蓝绿色
            (255, 128, 0)  # 橙色
        ]

        # 重新创建曲线字典
        self.plot_curves = {}

        # 为每个选中的传感器创建曲线
        for i, sensor_id in enumerate(self.selected_sensors):
            color_index = i % len(colors)
            color = colors[color_index]

            # 创建数据结构
            if sensor_id not in self.plot_data:
                self.plot_data[sensor_id] = {
                    'data1': [],
                    'data2': [],
                    'data3': []
                }

            # 获取数据
            data1 = np.array(self.plot_data[sensor_id]['data1'])
            data2 = np.array(self.plot_data[sensor_id]['data2'])
            data3 = np.array(self.plot_data[sensor_id]['data3'])

            # 创建X轴数据
            x_data = list(range(len(data1)))

            # 创建曲线 - 分别添加到三个图表中
            self.plot_curves[sensor_id] = {
                'data1': self.plot_widget_x.plot(
                    x_data, data1,
                    pen=pg.mkPen(color=color, width=3),  # 加粗曲线
                    name=f"传感器{sensor_id}"
                ),
                'data2': self.plot_widget_y.plot(
                    x_data, data2,
                    pen=pg.mkPen(color=color, width=3),  # 加粗曲线
                    name=f"传感器{sensor_id}"
                ),
                'data3': self.plot_widget_z.plot(
                    x_data, data3,
                    pen=pg.mkPen(color=color, width=3),  # 加粗曲线
                    name=f"传感器{sensor_id}"
                )
            }

        # 更新状态标签
        if self.selected_sensors:
            self.status_label.setText(f"监测中: {', '.join([f'传感器{id}' for id in self.selected_sensors])}")
        else:
            self.status_label.setText("未选择传感器")

    @pyqtSlot(int, list)
    def on_sensor_data_updated(self, sensor_id, values):
        """接收到传感器数据更新时调用"""
        # 确保传感器ID在数据字典中
        if sensor_id not in self.plot_data:
            self.plot_data[sensor_id] = {
                'data1': [],
                'data2': [],
                'data3': []
            }

        # 添加数据
        self.plot_data[sensor_id]['data1'].append(values[0])
        self.plot_data[sensor_id]['data2'].append(values[1])
        self.plot_data[sensor_id]['data3'].append(values[2])

        # 限制数据点数量
        if len(self.plot_data[sensor_id]['data1']) > self.max_data_points:
            self.plot_data[sensor_id]['data1'] = self.plot_data[sensor_id]['data1'][-self.max_data_points:]
            self.plot_data[sensor_id]['data2'] = self.plot_data[sensor_id]['data2'][-self.max_data_points:]
            self.plot_data[sensor_id]['data3'] = self.plot_data[sensor_id]['data3'][-self.max_data_points:]

        # 更新传感器当前值显示（只有当该传感器被选中时才更新显示）
        if sensor_id in self.selected_sensors:
            self.update_current_values_display()

    def update_plots(self):
        """更新图表 - 示波器滚动模式"""
        # 只更新选中的传感器图表
        for sensor_id in self.selected_sensors:
            if sensor_id in self.plot_curves and sensor_id in self.plot_data:
                # 获取数据
                data1 = np.array(self.plot_data[sensor_id]['data1'])
                data2 = np.array(self.plot_data[sensor_id]['data2'])
                data3 = np.array(self.plot_data[sensor_id]['data3'])

                # 创建X轴数据（样本索引）
                x_data = list(range(len(data1)))

                # 更新曲线
                if len(data1) > 0:
                    self.plot_curves[sensor_id]['data1'].setData(x_data, data1)
                    self.plot_curves[sensor_id]['data2'].setData(x_data, data2)
                    self.plot_curves[sensor_id]['data3'].setData(x_data, data3)

                    # 示波器滚动效果：当数据超过可见范围时，自动滚动视图
                    current_length = len(data1)
                    if current_length > self.visible_points:
                        # 计算新的X轴范围，显示最新的visible_points个数据点
                        x_min = current_length - self.visible_points
                        x_max = current_length
                        # 更新三个图表的X轴范围，实现滚动效果
                        self.plot_widget_x.getPlotItem().getViewBox().setXRange(x_min, x_max, padding=0)
                        self.plot_widget_y.getPlotItem().getViewBox().setXRange(x_min, x_max, padding=0)
                        self.plot_widget_z.getPlotItem().getViewBox().setXRange(x_min, x_max, padding=0)
                    else:
                        # 数据点少于可见范围时，显示从0开始
                        self.plot_widget_x.getPlotItem().getViewBox().setXRange(0, self.visible_points, padding=0)
                        self.plot_widget_y.getPlotItem().getViewBox().setXRange(0, self.visible_points, padding=0)
                        self.plot_widget_z.getPlotItem().getViewBox().setXRange(0, self.visible_points, padding=0)

                    # Y轴自动跟随逻辑
                    # X轴数据的Y轴范围跟随
                    if len(data1) > 0:
                        x_min_val = np.min(data1)
                        x_max_val = np.max(data1)
                        # 如果数据超出基础范围，扩大Y轴范围
                        if x_max_val > self.curve_y_axis_base_range['x_max']:
                            self.curve_y_axis_range['x_max'] = max(self.curve_y_axis_range['x_max'], x_max_val * 1.1)
                        if x_min_val < self.curve_y_axis_base_range['x_min']:
                            self.curve_y_axis_range['x_min'] = min(self.curve_y_axis_range['x_min'], x_min_val * 1.1)
                        # 如果数据回到基础范围内，逐渐恢复
                        if x_max_val <= self.curve_y_axis_base_range['x_max'] and x_min_val >= self.curve_y_axis_base_range['x_min']:
                            if self.curve_y_axis_range['x_max'] > self.curve_y_axis_base_range['x_max']:
                                self.curve_y_axis_range['x_max'] = max(self.curve_y_axis_base_range['x_max'],
                                                                        self.curve_y_axis_range['x_max'] * 0.98)
                            if self.curve_y_axis_range['x_min'] < self.curve_y_axis_base_range['x_min']:
                                self.curve_y_axis_range['x_min'] = min(self.curve_y_axis_base_range['x_min'],
                                                                        self.curve_y_axis_range['x_min'] * 0.98)
                        # 更新X轴图表的Y轴范围
                        self.plot_widget_x.getPlotItem().getViewBox().setYRange(
                            self.curve_y_axis_range['x_min'],
                            self.curve_y_axis_range['x_max'],
                            padding=0.1
                        )

                    # Y轴数据的Y轴范围跟随
                    if len(data2) > 0:
                        y_min_val = np.min(data2)
                        y_max_val = np.max(data2)
                        if y_max_val > self.curve_y_axis_base_range['y_max']:
                            self.curve_y_axis_range['y_max'] = max(self.curve_y_axis_range['y_max'], y_max_val * 1.1)
                        if y_min_val < self.curve_y_axis_base_range['y_min']:
                            self.curve_y_axis_range['y_min'] = min(self.curve_y_axis_range['y_min'], y_min_val * 1.1)
                        if y_max_val <= self.curve_y_axis_base_range['y_max'] and y_min_val >= self.curve_y_axis_base_range['y_min']:
                            if self.curve_y_axis_range['y_max'] > self.curve_y_axis_base_range['y_max']:
                                self.curve_y_axis_range['y_max'] = max(self.curve_y_axis_base_range['y_max'],
                                                                        self.curve_y_axis_range['y_max'] * 0.98)
                            if self.curve_y_axis_range['y_min'] < self.curve_y_axis_base_range['y_min']:
                                self.curve_y_axis_range['y_min'] = min(self.curve_y_axis_base_range['y_min'],
                                                                        self.curve_y_axis_range['y_min'] * 0.98)
                        self.plot_widget_y.getPlotItem().getViewBox().setYRange(
                            self.curve_y_axis_range['y_min'],
                            self.curve_y_axis_range['y_max'],
                            padding=0.1
                        )

                    # Z轴数据的Y轴范围跟随
                    if len(data3) > 0:
                        z_min_val = np.min(data3)
                        z_max_val = np.max(data3)
                        if z_max_val > self.curve_y_axis_base_range['z_max']:
                            self.curve_y_axis_range['z_max'] = max(self.curve_y_axis_range['z_max'], z_max_val * 1.1)
                        if z_min_val < self.curve_y_axis_base_range['z_min']:
                            self.curve_y_axis_range['z_min'] = min(self.curve_y_axis_range['z_min'], z_min_val * 1.1)
                        if z_max_val <= self.curve_y_axis_base_range['z_max'] and z_min_val >= self.curve_y_axis_base_range['z_min']:
                            if self.curve_y_axis_range['z_max'] > self.curve_y_axis_base_range['z_max']:
                                self.curve_y_axis_range['z_max'] = max(self.curve_y_axis_base_range['z_max'],
                                                                        self.curve_y_axis_range['z_max'] * 0.98)
                            if self.curve_y_axis_range['z_min'] < self.curve_y_axis_base_range['z_min']:
                                self.curve_y_axis_range['z_min'] = min(self.curve_y_axis_base_range['z_min'],
                                                                        self.curve_y_axis_range['z_min'] * 0.98)
                        self.plot_widget_z.getPlotItem().getViewBox().setYRange(
                            self.curve_y_axis_range['z_min'],
                            self.curve_y_axis_range['z_max'],
                            padding=0.1
                        )

        # 每次更新图表时也更新当前值显示
        self.update_current_values_display()

    def save_data(self):
        """保存传感器数据到CSV文件"""
        # 检查是否有选中的传感器
        if not self.currently_plotting_sensor:
            self.status_label.setText("错误：未选择要保存的传感器")
            return

        sensor_id = self.currently_plotting_sensor

        # 检查传感器是否有数据
        sensor_data = self.sensor_data_manager.get_sensor_data(sensor_id)
        if not sensor_data or sensor_data.get_data_count() == 0:
            self.status_label.setText("错误：传感器没有数据可保存")
            return

        # 弹出文件保存对话框
        now = datetime.now()
        timestamp = now.strftime("%Y%m%d_%H%M%S")
        default_filename = f"sensor{sensor_id}_data_{timestamp}.csv"

        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "保存传感器数据",
            default_filename,
            "CSV文件 (*.csv);;所有文件 (*.*)"
        )

        if file_path:
            # 调用传感器数据管理器的保存方法
            success, saved_path = self.sensor_data_manager.save_to_csv(
                sensor_id=sensor_id,
                filepath=file_path
            )

            if success:
                data_count = sensor_data.get_data_count()
                self.status_label.setText(f"数据已保存到: {os.path.basename(saved_path)} ({data_count}个数据点)")
            else:
                self.status_label.setText("保存数据失败")
        else:
            self.status_label.setText("取消保存")

    def clear_data(self):
        """清空数据"""
        # 如果有选中的传感器，清空它们的数据
        if self.selected_sensors:
            for sensor_id in self.selected_sensors:
                if sensor_id in self.plot_data:
                    self.plot_data[sensor_id]['data1'].clear()
                    self.plot_data[sensor_id]['data2'].clear()
                    self.plot_data[sensor_id]['data3'].clear()

                # 清空传感器管理器中的数据
                self.sensor_data_manager.clear_sensor_data(sensor_id)

        # 清空三个图表
        self.plot_widget_x.clear()
        self.plot_widget_x.addLegend()
        self.plot_widget_y.clear()
        self.plot_widget_y.addLegend()
        self.plot_widget_z.clear()
        self.plot_widget_z.addLegend()
        self.plot_curves.clear()

        # 更新状态标签
        self.status_label.setText("数据已清空")

    def reset_ui_to_initial_state(self):
        """恢复所有UI到初始状态（用于接收System_Reset后重置界面）"""
        # 停止内部状态
        self.is_started = False
        self.currently_plotting_sensor = None
        self.selected_sensors.clear()
        self.available_sensors.clear()
        self.current_selected_sensor = 0

        # 关闭所有激活的传感器并清空数据
        if self.sensor_data_manager:
            self.sensor_data_manager.set_active_sensors([])
            self.sensor_data_manager.clear_sensor_data()  # 清空所有传感器数据

        # 重置传感器选择按钮
        for btn in self.sensor_select_buttons.values():
            btn.setEnabled(False)
            btn.setChecked(False)
            btn.setStyleSheet("")

        # 重置启动按钮
        self.start_btn.setEnabled(False)
        self.start_btn.setText("启动")

        # 清空本地缓存数据
        self.plot_curves.clear()
        self.plot_data.clear()

        # 重新初始化三个图表（重新添加TextItem和图例）
        self.update_plot_curves()

        # 重置当前数值文本
        self.x_value_text.setText("X: --")
        self.y_value_text.setText("Y: --")
        self.z_value_text.setText("Z: --")

        # 重置3D力场可视化
        self.force_data = [0, 0, 0]
        self.force_history = np.zeros((10, 3))
        self.last_force_data = [0, 0, 0]
        self.create_base_grid()
        # 重新创建3D画布
        try:
            if hasattr(self, "force_canvas") and self.force_canvas is not None:
                self.middle_panel_layout.removeWidget(self.force_canvas)
                self.force_canvas.deleteLater()
        except Exception:
            pass
        self.force_canvas = self.create_force_canvas()
        self.middle_panel_layout.addWidget(self.force_canvas)

        # 重置状态标签
        self.status_label.setText("已接收 System_Reset，界面已恢复初始状态")

    # 删除数据保存功能，保留清空功能
    # 映射范围设置已移至设置页面的"传感器映射"选项卡

    def send_calibration_command(self):
        """发送标定命令到下位机"""
        if not self.serial_manager or not self.serial_manager.is_connected():
            self.status_label.setText("错误：串口未连接，无法发送标定命令")
            return

        # 发送标定命令 ML CALIB
        success = self.serial_manager.send_data("ML CALIB\r\n")

        if success:
            self.status_label.setText("已发送标定命令到下位机")
        else:
            self.status_label.setText("发送标定命令失败")

    def on_start_clicked(self):
        """启动/停止按钮点击事件 - 直接使用sensor0"""
        if not self.serial_manager or not self.serial_manager.is_connected():
            self.status_label.setText("错误：串口未连接，请先连接串口")
            return

        if not self.is_started:
            # 当前是停止状态，点击后启动
            success = self.serial_manager.send_data("ML START\r\n")

            if success:
                self.is_started = True
                self.start_btn.setText("停止")
                # 直接激活sensor0
                self.currently_plotting_sensor = 0
                self.current_selected_sensor = 0
                self.sensor_data_manager.set_active_sensors([0])

                # 更新选中的传感器列表（用于绘图）
                self.selected_sensors = [0]
                self.update_plot_curves()

                self.status_label.setText("传感器0已启动，正在接收数据...")
            else:
                self.status_label.setText("发送启动命令失败")
        else:
            # 当前是启动状态，点击后停止
            success = self.serial_manager.send_data("ML STOP\r\n")

            if success:
                self.is_started = False
                self.start_btn.setText("启动")
                self.currently_plotting_sensor = None

                # 清空选中列表
                self.selected_sensors.clear()

                # 清空数据和图表
                self.clear_data()

                # 停用所有传感器（不再接收任何传感器数据）
                self.sensor_data_manager.set_active_sensors([])

                self.status_label.setText("传感器已停止")
            else:
                self.status_label.setText("发送停止命令失败")

    @pyqtSlot(bytes)
    def on_serial_data_received(self, data):
        """接收到串口数据时的处理 - 简化版，不处理扫描响应"""
        # 此方法保留但不做处理，数据解析由sensor_data_manager处理
        pass

    def refresh_ports(self):
        """刷新串口列表"""
        if not self.serial_manager:
            return

        # 保存当前选中的端口
        current_port = self.port_combo.currentData()

        # 清空列表
        self.port_combo.clear()

        # 获取可用端口
        ports = self.serial_manager.get_available_ports()

        # 添加到下拉框
        for port in ports:
            port_name = port.device
            port_description = port.description if port.description else ""
            display_text = f"{port_name} - {port_description}" if port_description else port_name
            self.port_combo.addItem(display_text, port_name)

        # 如果之前有选中的端口，尝试恢复
        if current_port:
            for i in range(self.port_combo.count()):
                if self.port_combo.itemData(i) == current_port:
                    self.port_combo.setCurrentIndex(i)
                    break

    def toggle_serial_connection(self):
        """切换串口连接状态"""
        if not self.serial_manager:
            self.status_label.setText("错误：串口管理器未初始化")
            return

        if self.serial_manager.is_connected():
            # 当前已连接，执行断开
            self.serial_manager.disconnect()
        else:
            # 当前未连接，执行连接
            port = self.port_combo.currentData()
            if not port:
                self.status_label.setText("错误：请选择串口")
                return

            baud_rate = int(self.baudrate_combo.currentText())

            # 连接串口
            success = self.serial_manager.connect(
                port=port,
                baud_rate=baud_rate,
                data_bits=8,
                parity='N',
                stop_bits=1,
                timeout=1,
                flow_control=None
            )

            if not success:
                self.status_label.setText("连接失败，请检查串口设置")

    @pyqtSlot(bool)
    def on_serial_connection_changed(self, connected):
        """串口连接状态变化时的处理"""
        if connected:
            self.connect_btn.setText("断开")
            self.status_label.setText("串口已连接")
            # 禁用端口和波特率选择
            self.port_combo.setEnabled(False)
            self.baudrate_combo.setEnabled(False)
            self.refresh_port_btn.setEnabled(False)
        else:
            self.connect_btn.setText("连接")
            self.status_label.setText("串口已断开")
            # 启用端口和波特率选择
            self.port_combo.setEnabled(True)
            self.baudrate_combo.setEnabled(True)
            self.refresh_port_btn.setEnabled(True)