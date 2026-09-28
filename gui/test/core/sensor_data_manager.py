#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import time
import os
import csv
import json
import numpy as np
from PyQt6.QtCore import QObject, pyqtSignal, pyqtSlot
from datetime import datetime


class SensorData:
    """传感器数据类，用于存储单个传感器的数据"""

    def __init__(self, sensor_id):
        self.sensor_id = sensor_id
        self.timestamp = []
        self.data1 = []  # x轴
        self.data2 = []  # y轴
        self.data3 = []  # z轴
        self.latest_values = [0, 0, 0]  # 最新的三轴值

    def add_data(self, d1, d2, d3):
        """添加一组数据"""
        # 添加时间戳
        self.timestamp.append(time.time())

        # 添加数据
        self.data1.append(float(d1))
        self.data2.append(float(d2))
        self.data3.append(float(d3))

        # 更新最新值
        self.latest_values = [float(d1), float(d2), float(d3)]

    def clear_data(self):
        """清空数据"""
        self.timestamp.clear()
        self.data1.clear()
        self.data2.clear()
        self.data3.clear()
        self.latest_values = [0, 0, 0]

    def get_latest_values(self):
        """获取最新的数据值"""
        return self.latest_values

    def get_data_count(self):
        """获取数据点数量"""
        return len(self.timestamp)


class SensorDataManager(QObject):
    """传感器数据管理器，负责解析和存储多个传感器的数据"""

    # 定义信号
    data_updated_signal = pyqtSignal(int, list)  # 数据更新信号 (传感器ID, [data1, data2, data3])
    data_parsed_signal = pyqtSignal(bool)  # 数据解析状态信号

    def __init__(self):
        super().__init__()

        # 传感器数据字典 {sensor_id: SensorData对象}
        self.sensors = {}

        # 激活的传感器ID（只有激活的传感器才会接收和存储数据）
        self.active_sensors = set()  # 集合类型，可以包含多个激活的传感器

        # 数据正则表达式匹配模式
        self.pattern = re.compile(r'sensor(\d+):\s*([-+]?\d*\.?\d+),\s*([-+]?\d*\.?\d+),\s*([-+]?\d*\.?\d+)')

        # 数据文件目录
        self.data_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")
        if not os.path.exists(self.data_dir):
            os.makedirs(self.data_dir)

    def parse_data(self, data_str):
        """解析传感器数据字符串"""
        try:
            data_str = data_str.strip()
            # 查找匹配模式
            match = self.pattern.search(data_str)
            if match:
                # 提取传感器ID和数据
                sensor_id = int(match.group(1))
                data1 = match.group(2)
                data2 = match.group(3)
                data3 = match.group(4)

                # 只处理激活的传感器数据
                if sensor_id not in self.active_sensors:
                    # 发射数据解析状态信号 - 成功但被过滤
                    self.data_parsed_signal.emit(True)
                    return True

                # 确保传感器实例存在
                if sensor_id not in self.sensors:
                    self.sensors[sensor_id] = SensorData(sensor_id)

                # 添加数据
                self.sensors[sensor_id].add_data(data1, data2, data3)

                # 发射数据更新信号
                self.data_updated_signal.emit(sensor_id, [float(data1), float(data2), float(data3)])

                # 发射数据解析状态信号 - 成功
                self.data_parsed_signal.emit(True)

                return True
            else:
                # 发射数据解析状态信号 - 失败
                self.data_parsed_signal.emit(False)
                return False
        except Exception as e:
            print(f"数据解析错误: {e}")
            # 发射数据解析状态信号 - 失败
            self.data_parsed_signal.emit(False)
            return False

    def get_sensor_data(self, sensor_id):
        """获取指定传感器的数据对象"""
        if sensor_id in self.sensors:
            return self.sensors[sensor_id]
        return None

    def get_all_sensors(self):
        """获取所有传感器ID列表"""
        return list(self.sensors.keys())

    def clear_sensor_data(self, sensor_id=None):
        """清空指定传感器的数据，如果sensor_id为None则清空所有传感器数据"""
        if sensor_id is not None:
            if sensor_id in self.sensors:
                self.sensors[sensor_id].clear_data()
        else:
            for sensor in self.sensors.values():
                sensor.clear_data()

    def set_active_sensors(self, sensor_ids):
        """设置激活的传感器列表，只有激活的传感器才会接收数据
        
        Args:
            sensor_ids: 传感器ID列表或单个ID，如果为空列表/None则停用所有传感器
        """
        # 获取之前的激活传感器集合
        previous_active = self.active_sensors.copy()
        
        # 处理输入
        if sensor_ids is None:
            self.active_sensors = set()
        elif isinstance(sensor_ids, (list, tuple, set)):
            self.active_sensors = set(sensor_ids)
        else:
            # 单个传感器ID
            self.active_sensors = {sensor_ids}
        
        # 清空现在未激活的传感器数据
        no_longer_active = previous_active - self.active_sensors
        for sensor_id in no_longer_active:
            if sensor_id in self.sensors:
                self.sensors[sensor_id].clear_data()

    def add_active_sensor(self, sensor_id):
        """添加一个激活的传感器"""
        self.active_sensors.add(sensor_id)

    def remove_active_sensor(self, sensor_id):
        """移除一个激活的传感器并清空其数据"""
        if sensor_id in self.active_sensors:
            self.active_sensors.discard(sensor_id)
        if sensor_id in self.sensors:
            self.sensors[sensor_id].clear_data()

    def get_active_sensors(self):
        """获取当前激活的传感器列表"""
        return list(self.active_sensors)

    def clear_all_inactive_sensors(self):
        """清空所有未激活传感器的数据"""
        all_sensors = set(self.sensors.keys())
        inactive = all_sensors - self.active_sensors
        for sensor_id in inactive:
            if sensor_id in self.sensors:
                self.sensors[sensor_id].clear_data()

    def save_to_csv(self, sensor_id=None, filename=None, filepath=None):
        """将传感器数据保存为CSV文件

        Args:
            sensor_id: 要保存的传感器ID，如果为None则保存所有传感器
            filename: 文件名，如果为None则自动生成
            filepath: 完整文件路径，如果提供则直接使用该路径保存，忽略 filename 参数

        Returns:
            bool: 保存是否成功，返回实际保存的文件路径
        """
        try:
            if not filepath:
                # 如果没有指定文件名，自动生成
                if filename is None:
                    now = datetime.now()
                    timestamp = now.strftime("%Y%m%d_%H%M%S")
                    if sensor_id is not None:
                        filename = f"sensor{sensor_id}_data_{timestamp}.csv"
                    else:
                        filename = f"all_sensors_data_{timestamp}.csv"

                # 确保文件路径
                filepath = os.path.join(self.data_dir, filename)
            
            # 确保目录存在
            save_dir = os.path.dirname(filepath)
            if not os.path.exists(save_dir):
                os.makedirs(save_dir)

            # 写入CSV文件
            with open(filepath, 'w', newline='') as csvfile:
                writer = csv.writer(csvfile)

                # 写入表头
                if sensor_id is not None:
                    # 单个传感器
                    writer.writerow(['Timestamp', 'Data1', 'Data2', 'Data3'])
                    sensor_data = self.get_sensor_data(sensor_id)
                    if sensor_data:
                        for i in range(sensor_data.get_data_count()):
                            # 将时间戳转换为可读格式
                            timestamp_readable = datetime.fromtimestamp(sensor_data.timestamp[i]).strftime(
                                "%Y-%m-%d %H:%M:%S.%f")[:-3]
                            writer.writerow([
                                timestamp_readable,
                                sensor_data.data1[i],
                                sensor_data.data2[i],
                                sensor_data.data3[i]
                            ])
                else:
                    # 所有传感器
                    sensor_ids = self.get_all_sensors()
                    header = ['Timestamp']
                    for sid in sensor_ids:
                        header.extend([f'Sensor{sid}_Data1', f'Sensor{sid}_Data2', f'Sensor{sid}_Data3'])
                    writer.writerow(header)

                    # 找出最大数据点数
                    max_data_points = 0
                    for sid in sensor_ids:
                        sensor_data = self.get_sensor_data(sid)
                        if sensor_data:
                            max_data_points = max(max_data_points, sensor_data.get_data_count())

                    # 写入数据行
                    for i in range(max_data_points):
                        row = []
                        # 使用第一个传感器的时间戳
                        first_sensor = self.get_sensor_data(sensor_ids[0])
                        if first_sensor and i < first_sensor.get_data_count():
                            # 将时间戳转换为可读格式
                            timestamp_readable = datetime.fromtimestamp(first_sensor.timestamp[i]).strftime(
                                "%Y-%m-%d %H:%M:%S.%f")[:-3]
                            row.append(timestamp_readable)
                        else:
                            row.append('')

                        # 添加所有传感器的数据
                        for sid in sensor_ids:
                            sensor_data = self.get_sensor_data(sid)
                            if sensor_data and i < sensor_data.get_data_count():
                                row.extend([
                                    sensor_data.data1[i],
                                    sensor_data.data2[i],
                                    sensor_data.data3[i]
                                ])
                            else:
                                row.extend(['', '', ''])
                        writer.writerow(row)

            return True, filepath
        except Exception as e:
            print(f"保存CSV文件错误: {e}")
            return False, None

    def get_sensor_statistics(self, sensor_id):
        """获取传感器数据的统计信息"""
        sensor_data = self.get_sensor_data(sensor_id)
        if not sensor_data or sensor_data.get_data_count() == 0:
            return None

        # 计算统计数据
        stats = {
            'data1': {
                'min': min(sensor_data.data1),
                'max': max(sensor_data.data1),
                'mean': np.mean(sensor_data.data1),
                'std': np.std(sensor_data.data1)
            },
            'data2': {
                'min': min(sensor_data.data2),
                'max': max(sensor_data.data2),
                'mean': np.mean(sensor_data.data2),
                'std': np.std(sensor_data.data2)
            },
            'data3': {
                'min': min(sensor_data.data3),
                'max': max(sensor_data.data3),
                'mean': np.mean(sensor_data.data3),
                'std': np.std(sensor_data.data3)
            }
        }

        return stats