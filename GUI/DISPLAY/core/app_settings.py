#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import os
from PyQt6.QtCore import QObject, pyqtSignal


class Settings(QObject):
    """应用程序设置类，负责管理和保存应用设置"""

    # 定义信号
    settings_changed = pyqtSignal(dict)

    def __init__(self):
        super().__init__()

        # 设置默认值
        self.settings = {
            # 主题设置
            "theme": "dracula",

            # 串口默认设置
            "serial": {
                "baud_rate": 9600,
                "data_bits": 8,
                "parity": "N",
                "stop_bits": 1,
                "flow_control": None,
                "last_port": "",
                "auto_connect": False,
                "reconnect_attempts": 3
            },

            # 数据显示设置
            "display": {
                "text_font": "Consolas",
                "text_size": 10,
                "show_timestamp": False,
                "hex_display": False,
                "auto_scroll": True,
                "max_lines": 1000
            }
        }

        # 配置文件路径
        self.config_path = os.path.join(APP_ROOT_PATH, "config", "settings.json")

    def load_settings(self):
        """加载设置"""
        try:
            # 检查配置文件是否存在
            if os.path.exists(self.config_path):
                with open(self.config_path, 'r') as f:
                    loaded_settings = json.load(f)

                    # 递归更新设置，保留默认值
                    self._update_settings_recursive(self.settings, loaded_settings)

                    # 发出设置变更信号
                    self.settings_changed.emit(self.settings)

                    return True
            else:
                # 如果配置文件不存在，创建默认配置
                self.save_settings()
        except Exception as e:
            print(f"加载设置错误: {e}")
            # 使用默认设置
            # 保存默认设置
            self.save_settings()

        return False

    def save_settings(self):
        """保存设置"""
        try:
            # 确保配置目录存在
            config_dir = os.path.dirname(self.config_path)
            if not os.path.exists(config_dir):
                os.makedirs(config_dir)

            # 保存设置到文件
            with open(self.config_path, 'w') as f:
                json.dump(self.settings, f, indent=4)

            return True
        except Exception as e:
            print(f"保存设置错误: {e}")
            return False

    def get_setting(self, category, key=None):
        """获取设置值"""
        if category in self.settings:
            if key is None:
                return self.settings[category]
            elif key in self.settings[category]:
                return self.settings[category][key]
        return None

    def set_setting(self, category, key, value):
        """设置值"""
        if category in self.settings:
            if key in self.settings[category]:
                if self.settings[category][key] != value:
                    self.settings[category][key] = value
                    self.save_settings()
                    # 发出设置变更信号
                    self.settings_changed.emit(self.settings)
                    return True
        return False

    def update_settings(self, new_settings):
        """更新多个设置"""
        updated = False

        for category in new_settings:
            if category in self.settings:
                if isinstance(new_settings[category], dict):
                    for key, value in new_settings[category].items():
                        if key in self.settings[category] and self.settings[category][key] != value:
                            self.settings[category][key] = value
                            updated = True
                elif self.settings[category] != new_settings[category]:
                    self.settings[category] = new_settings[category]
                    updated = True

        if updated:
            self.save_settings()
            # 发出设置变更信号
            self.settings_changed.emit(self.settings)

        return updated

    def reset_to_defaults(self):
        """重置为默认设置"""
        # 创建默认设置的副本
        default_settings = {
            # 主题设置
            "theme": "dracula",

            # 串口默认设置
            "serial": {
                "baud_rate": 9600,
                "data_bits": 8,
                "parity": "N",
                "stop_bits": 1,
                "flow_control": None,
                "last_port": "",
                "auto_connect": False,
                "reconnect_attempts": 3
            },

            # 数据显示设置
            "display": {
                "text_font": "Consolas",
                "text_size": 10,
                "show_timestamp": False,
                "hex_display": False,
                "auto_scroll": True,
                "max_lines": 1000
            }
        }

        # 更新设置
        self.settings = default_settings

        # 保存设置
        self.save_settings()

        # 发出设置变更信号
        self.settings_changed.emit(self.settings)

        return True

    def _update_settings_recursive(self, target_dict, source_dict):
        """递归更新设置字典，保留原始结构"""
        for key, value in source_dict.items():
            if key in target_dict:
                if isinstance(value, dict) and isinstance(target_dict[key], dict):
                    # 递归更新子字典
                    self._update_settings_recursive(target_dict[key], value)
                else:
                    # 更新值
                    target_dict[key] = value