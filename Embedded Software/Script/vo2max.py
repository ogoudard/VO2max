#!/usr/bin/env python3

import sys
import csv
import time
import threading
import datetime
import collections
import struct

import serial
import serial.tools.list_ports

from PyQt6 import QtWidgets, QtCore
import pyqtgraph as pg

# ╔══════════════════════════════════════════════════════════════════╗
# ║ GPU MODE                                                        ║
# ╚══════════════════════════════════════════════════════════════════╝

pg.setConfigOptions(
    useOpenGL=True,
    antialias=False,
    background="w",
    foreground="k",
)

# ╔══════════════════════════════════════════════════════════════════╗
# ║ PROTOCOL                                                        ║
# ╚══════════════════════════════════════════════════════════════════╝

SYNC_BYTE = 0xAA
PACKET_SIZE = 14
FMT = "<BfQ"   # channel, float32, uint64 (after sync byte)

# ╔══════════════════════════════════════════════════════════════════╗
# ║ CONFIG                                                          ║
# ╚══════════════════════════════════════════════════════════════════╝

CHANNELS = {
    "0": {"label": "Temperature"},
    "1": {"label": "Humidity"},
    "2": {"label": "Pressure"},
    "3": {"label": "Altitude"},
    "4": {"label": "Flow"},
    "5": {"label": "Cycle Vol"},
    "6": {"label": "Total Vol"},
    "7": {"label": "O2"},
    "8": {"label": "CO2"},
    "9": {"label": "VO2"},
    "10": {"label": "VO2max"},
    "11": {"label": "VCO2"},
    "12": {"label": "RQ"},
    "13": {"label": "RR"},
    "14": {"label": "Rho"},
    "15": {"label": "Exp Flow"},
    "16": {"label": "dP"},
}

COLORS = [
    "#0077cc", "#d62728", "#2ca02c", "#ff7f0e",
    "#9467bd", "#8c564b", "#17becf", "#e377c2",
    "#bcbd22", "#1f77b4", "#ff9896", "#98df8a",
    "#c5b0d5", "#c49c94", "#9edae5", "#f7b6d2",
    "#dbdb8d",
]

UPDATE_MS = 40
RENDER_WINDOW = 20.0
MAX_POINTS = 200000

# ╔══════════════════════════════════════════════════════════════════╗
# ║ DATA                                                            ║
# ╚══════════════════════════════════════════════════════════════════╝

lock = threading.Lock()

channel_data = {
    ch: {"t": collections.deque(), "y": collections.deque()}
    for ch in CHANNELS
}

running = True
t0 = None

# ╔══════════════════════════════════════════════════════════════════╗
# ║ PORT DETECTION                                                 ║
# ╚══════════════════════════════════════════════════════════════════╝

def detect_port():

    ports = serial.tools.list_ports.comports()

    for p in ports:
        if any(k in p.device for k in ("USB", "ACM", "COM")):
            return p.device

    return ports[0].device if ports else None

# ╔══════════════════════════════════════════════════════════════════╗
# ║ SERIAL (SYNC BYTE PARSER)                                      ║
# ╚══════════════════════════════════════════════════════════════════╝

def serial_thread(port, baud):

    global running, t0

    ser = serial.Serial(port, baud, timeout=1)
    ser.reset_input_buffer()

    buffer = bytearray()

    while running:

        data = ser.read(256)
        if not data:
            continue

        buffer.extend(data)

        # ─────────────────────────────────────────────
        # SYNC SEARCH + FRAME EXTRACTION
        # ─────────────────────────────────────────────

        while len(buffer) >= PACKET_SIZE:
            # find sync byte
            if buffer[0] != SYNC_BYTE:
                buffer.pop(0)
                continue

            packet = buffer[1:PACKET_SIZE]
            buffer = buffer[PACKET_SIZE:]

            try:
                ch, val, ts = struct.unpack(FMT, packet)
            except:
                continue

            ch = str(ch)

            if ch not in CHANNELS:
                continue

            with lock:

                if t0 is None:
                    t0 = ts

                t = (ts - t0) / 1e6

                channel_data[ch]["t"].append(t)
                channel_data[ch]["y"].append(val)

                if len(channel_data[ch]["t"]) > MAX_POINTS:
                    channel_data[ch]["t"].popleft()
                    channel_data[ch]["y"].popleft()

# ╔══════════════════════════════════════════════════════════════════╗
# ║ GUI                                                             ║
# ╚══════════════════════════════════════════════════════════════════╝

class MainWindow(QtWidgets.QMainWindow):

    def __init__(self):

        super().__init__()

        self.setWindowTitle("GPU Sync UART Plotter")

        self.resize(1800, 1000)

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)

        layout = QtWidgets.QHBoxLayout(central)

        # plots
        self.graph = pg.GraphicsLayoutWidget()
        layout.addWidget(self.graph, 1)

        # side panel
        side = QtWidgets.QWidget()
        side.setFixedWidth(260)
        sl = QtWidgets.QVBoxLayout(side)
        layout.addWidget(side)

        self.checks = {}

        for ch in CHANNELS:

            cb = QtWidgets.QCheckBox(f"{ch} {CHANNELS[ch]['label']}")
            cb.setChecked(ch in {"9", "11", "15"})
            cb.stateChanged.connect(self.rebuild)
            sl.addWidget(cb)

            self.checks[ch] = cb

        sl.addStretch()

        self.curves = {}

        self.build()

        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(UPDATE_MS)

    # ─────────────────────────────────────────────

    def rebuild(self):
        self.build()

    def build(self):

        self.graph.clear()
        self.curves.clear()

        row = 0
        first = None

        for ch in CHANNELS:

            if not self.checks[ch].isChecked():
                continue

            plot = self.graph.addPlot(row=row, col=0)

            plot.showGrid(x=True, y=True, alpha=0.2)

            if first is None:
                first = plot
            else:
                plot.setXLink(first)

            curve = pg.PlotCurveItem(
                pen=pg.mkPen(COLORS[int(ch) % len(COLORS)], width=1.2)
            )

            plot.addItem(curve)

            self.curves[ch] = curve

            row += 1

    # ─────────────────────────────────────────────

    def update(self):

        with lock:

            snap = {
                ch: (
                    list(channel_data[ch]["t"]),
                    list(channel_data[ch]["y"]),
                )
                for ch in self.curves
            }

        for ch, curve in self.curves.items():

            t, y = snap[ch]

            if not t:
                continue

            t_max = t[-1]
            t_min = t_max - RENDER_WINDOW

            start = 0
            for i in range(len(t) - 1, -1, -1):
                if t[i] < t_min:
                    start = i
                    break

            curve.setData(t[start:], y[start:])

# ╔══════════════════════════════════════════════════════════════════╗
# ║ MAIN                                                            ║
# ╚══════════════════════════════════════════════════════════════════╝

def main():

    global running

    port = detect_port()
    baud = 921600

    if not port:
        print("No serial port found")
        return

    print("Using port:", port)

    threading.Thread(
        target=serial_thread,
        args=(port, baud),
        daemon=True
    ).start()

    app = QtWidgets.QApplication(sys.argv)

    win = MainWindow()
    win.showMaximized()

    try:
        sys.exit(app.exec())
    finally:
        running = False

if __name__ == "__main__":
    main()