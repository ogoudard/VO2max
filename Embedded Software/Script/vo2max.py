#!/usr/bin/env python3
"""
Serial Plotter
- Each panel is a QWidget row containing one pg.PlotWidget (canvas)
- Extra Y axes are real Qt widgets placed beside the canvas in a
  QHBoxLayout — no pyqtgraph scene layout manipulation whatsoever
- All channels in a panel share the X axis; each has its own Y axis
  that auto-scales independently every frame
- Exception: channels listed in SHARED_AXES share a single Y axis
"""

import sys, threading, collections, struct
import serial, serial.tools.list_ports
from PyQt6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg
import numpy as np

pg.setConfigOptions(useOpenGL=True, antialias=False,
                    background="#0d1117", foreground="#c9d1d9")

# ╔══════════════════════════════════════════════════════════════════╗
# ║ PROTOCOL                                                        ║
# ╚══════════════════════════════════════════════════════════════════╝
SYNC_BYTE   = 0xAA
PACKET_SIZE = 14
FMT         = "<BfQ"

# ╔══════════════════════════════════════════════════════════════════╗
# ║ CONFIG                                                          ║
# ╚══════════════════════════════════════════════════════════════════╝
CHANNELS = {
    "0":  {"label": "Temperature",           "unit": "°C"},
    "1":  {"label": "Humidity",              "unit": "%"},
    "2":  {"label": "Pressure",              "unit": "Pa"},
    "3":  {"label": "Altitude",              "unit": "m"},
    "4":  {"label": "Flow",                  "unit": "L/s"},
    "5":  {"label": "Cycle Vol",             "unit": "L"},
    "6":  {"label": "Total Vol",             "unit": "L"},
    "7":  {"label": "O2",                    "unit": "%"},
    "8":  {"label": "CO2",                   "unit": "ppm"},
    "9":  {"label": "VO2",                   "unit": "mL/kg/min"},
    "10": {"label": "VO2max",                "unit": "mL/kg/min"},
    "11": {"label": "VCO2",                  "unit": "mL/kg/min"},
    "12": {"label": "RQ",                    "unit": ""},
    "13": {"label": "RR",                    "unit": "breaths/min"},
    "14": {"label": "Rho",                   "unit": "kg/m³"},
    "15": {"label": "Exp Flow",              "unit": "L/s"},
    "16": {"label": "Differential Pressure", "unit": "Pa"},
}

PANELS = [
    {"title": "Environmental",          "channels": ["0","1","2","3","14"]},
    {"title": "Flow",                   "channels": ["4","5","6","16"]},
    {"title": "Gas",                    "channels": ["7","8"]},
    {"title": "VO2 / VCO2 / Exp Flow", "channels": ["9","10","11","15"]},
    {"title": "RQ / RR",               "channels": ["12","13"]},
]

# Groups of channels that share a single Y axis widget.
SHARED_AXES = [
    ["9", "10", "11"],   # VO2, VO2max and VCO2 share one Y axis
]

COLORS = [
    "#00d4ff","#ff6b6b","#51cf66","#ffd43b",
    "#cc5de8","#ff922b","#74c0fc","#f783ac",
    "#20c997","#ff8787","#a9e34b","#ffec99",
    "#e599f7","#ffa94d","#4dabf7","#f9a8d4",
    "#a5d8ff",
]

DEFAULT_ON    = {"9","11","15"}
UPDATE_MS     = 40
RENDER_WINDOW = 300.0  # 5 minutes
MAX_POINTS    = 200_000
DEFAULT_PORT  = "COM6"
DEFAULT_BAUD  = 921_600
AXIS_WIDTH    = 52   # px per Y axis widget
Y_PAD         = 0.05 # 5% padding above and below autoscale

# ╔══════════════════════════════════════════════════════════════════╗
# ║ SHARED-AXIS HELPERS                                             ║
# ╚══════════════════════════════════════════════════════════════════╝
def build_axis_map():
    axis_map = {}
    for group in SHARED_AXES:
        key = frozenset(group)
        for ch in group:
            axis_map[ch] = key
    return axis_map

AXIS_MAP = build_axis_map()

def group_key_for(ch):
    return AXIS_MAP.get(ch, frozenset({ch}))

def axis_label_for(group_key):
    labels = [CHANNELS[ch]["label"] for ch in sorted(group_key, key=int)]
    return " / ".join(labels)

def axis_color_for(group_key):
    ch = min(group_key, key=int)
    return COLORS[int(ch) % len(COLORS)]

# ╔══════════════════════════════════════════════════════════════════╗
# ║ DATA                                                            ║
# ╚══════════════════════════════════════════════════════════════════╝
lock = threading.Lock()
channel_data = {
    ch: {"t": collections.deque(), "y": collections.deque()}
    for ch in CHANNELS
}
running = True
t0      = None

# ╔══════════════════════════════════════════════════════════════════╗
# ║ SERIAL                                                          ║
# ╚══════════════════════════════════════════════════════════════════╝
def detect_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if any(k in p.device for k in ("USB","ACM","COM")):
            return p.device
    return ports[0].device if ports else None

def serial_thread(port, baud):
    global running, t0
    ser = serial.Serial(port, baud, timeout=1)
    ser.reset_input_buffer()
    buf = bytearray()
    while running:
        data = ser.read(256)
        if not data:
            continue
        buf.extend(data)
        while len(buf) >= PACKET_SIZE:
            if buf[0] != SYNC_BYTE:
                buf.pop(0)
                continue
            pkt = bytes(buf[1:PACKET_SIZE])
            del buf[:PACKET_SIZE]
            try:
                ch, val, ts = struct.unpack(FMT, pkt)
            except Exception:
                continue
            ch = str(ch)
            if ch not in CHANNELS:
                continue
            with lock:
                if t0 is None:
                    t0 = ts
                t = (ts - t0) / 1e6
                d = channel_data[ch]
                d["t"].append(t)
                d["y"].append(val)
                if len(d["t"]) > MAX_POINTS:
                    d["t"].popleft()
                    d["y"].popleft()

# ╔══════════════════════════════════════════════════════════════════╗
# ║ Y-AXIS WIDGET                                                   ║
# ╚══════════════════════════════════════════════════════════════════╝
class YAxisWidget(QtWidgets.QWidget):
    """
    A plain QPainter-drawn Y axis: ticks + labels on a fixed-width strip.
    """

    def __init__(self, color: str, label: str, side: str = "left",
                 parent=None):
        super().__init__(parent)
        self.setFixedWidth(AXIS_WIDTH)
        self._color  = QtGui.QColor(color)
        self._label  = label
        self._side   = side
        self._lo     = 0.0
        self._hi     = 1.0
        self.setStyleSheet("background:#0d1117;")

    def set_range(self, lo: float, hi: float):
        if lo != self._lo or hi != self._hi:
            self._lo = lo
            self._hi = hi
            self.update()

    def paintEvent(self, _):
        p   = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, False)
        w   = self.width()
        h   = self.height()
        lo  = self._lo
        hi  = self._hi
        rng = hi - lo if hi != lo else 1.0

        pen = QtGui.QPen(self._color)
        pen.setWidth(1)
        p.setPen(pen)
        font = QtGui.QFont("monospace", 7)
        p.setFont(font)
        fm   = QtGui.QFontMetrics(font)

        # Spine line
        x_spine = w - 1 if self._side == "left" else 0
        p.drawLine(x_spine, 0, x_spine, h)

        # Ticks and labels
        n_ticks = 6
        for i in range(n_ticks + 1):
            val  = lo + (hi - lo) * i / n_ticks
            y    = int(h - (val - lo) / rng * h)
            y    = max(1, min(h - 1, y))
            tick_len = 5
            if self._side == "left":
                p.drawLine(w - 1, y, w - 1 - tick_len, y)
                txt  = f"{val:.3g}"
                tw   = fm.horizontalAdvance(txt)
                p.drawText(w - 1 - tick_len - tw - 2,
                           y + fm.ascent() // 2, txt)
            else:
                p.drawLine(0, y, tick_len, y)
                txt = f"{val:.3g}"
                p.drawText(tick_len + 2, y + fm.ascent() // 2, txt)

        # Channel label rotated vertically in centre
        p.save()
        p.translate(w // 2, h // 2)
        if self._side == "left":
            p.rotate(-90)
        else:
            p.rotate(90)
        lbl_font = QtGui.QFont("monospace", 7, QtGui.QFont.Weight.Bold)
        p.setFont(lbl_font)
        p.setPen(QtGui.QPen(self._color))
        lw = QtGui.QFontMetrics(lbl_font).horizontalAdvance(self._label)
        p.drawText(-lw // 2, 0, self._label)
        p.restore()

        p.end()


# ╔══════════════════════════════════════════════════════════════════╗
# ║ LIVE LABEL OVERLAY (Qt widget, not pyqtgraph scene item)        ║
# ╚══════════════════════════════════════════════════════════════════╝
class LiveLabelOverlay(QtWidgets.QWidget):
    """
    A transparent Qt widget that sits on top of the PlotWidget and
    draws one pill badge per channel, stacked in the top-right corner.
    """
    PAD_X   = 7
    PAD_Y   = 3
    SPACING = 4
    MARGIN  = 6
    FONT    = QtGui.QFont("monospace", 8, QtGui.QFont.Weight.Bold)

    def __init__(self, channels: list, parent: QtWidgets.QWidget):
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_TransparentForMouseEvents)
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_NoSystemBackground)
        self.channels = channels
        self._values  = {ch: None for ch in channels}
        self._colors  = {ch: QtGui.QColor(COLORS[int(ch) % len(COLORS)])
                         for ch in channels}
        self._bg      = QtGui.QColor(13, 17, 23, 210)
        parent.installEventFilter(self)

    def set_value(self, ch: str, value: float):
        self._values[ch] = value
        self.update()

    def eventFilter(self, obj, event):
        if obj is self.parent() and event.type() == QtCore.QEvent.Type.Resize:
            self.resize(self.parent().size())
        return False

    def showEvent(self, _):
        self.resize(self.parent().size())

    def paintEvent(self, _):
        p  = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
        fm = QtGui.QFontMetrics(self.FONT)
        th = fm.height()
        bh = th + self.PAD_Y * 2

        x_right = self.width() - self.MARGIN
        y       = self.MARGIN

        for ch in self.channels:
            val = self._values[ch]
            if val is None:
                continue
            unit = CHANNELS[ch]['unit']
            txt  = f"{CHANNELS[ch]['label']}: {val:.4g} {unit}".strip()
            tw   = fm.horizontalAdvance(txt)
            bw   = tw + self.PAD_X * 2
            rx   = x_right - bw
            rect = QtCore.QRectF(rx, y, bw, bh)

            color = self._colors[ch]
            p.setPen(QtGui.QPen(color, 1))
            p.setBrush(QtGui.QBrush(self._bg))
            p.drawRoundedRect(rect, 4, 4)

            p.setPen(QtGui.QPen(color))
            p.setFont(self.FONT)
            p.drawText(rect, QtCore.Qt.AlignmentFlag.AlignCenter, txt)

            y += bh + self.SPACING

        p.end()


# ╔══════════════════════════════════════════════════════════════════╗
# ║ PANEL WIDGET                                                    ║
# ╚══════════════════════════════════════════════════════════════════╝
class PanelWidget(QtWidgets.QWidget):

    def __init__(self, title: str, channels: list, show_x: bool,
                 x_link_target=None, parent=None):
        super().__init__(parent)
        self.channels    = channels
        self.curves      = {}
        self.y_widgets   = {}
        self.ch_to_group = {}

        seen_groups = []
        seen_set    = set()
        for ch in channels:
            gk       = group_key_for(ch)
            gk_local = frozenset(c for c in gk if c in channels)
            self.ch_to_group[ch] = gk_local
            if gk_local not in seen_set:
                seen_set.add(gk_local)
                seen_groups.append(gk_local)

        n_axes       = len(seen_groups)
        n_left       = (n_axes + 1) // 2
        left_groups  = seen_groups[:n_left]
        right_groups = seen_groups[n_left:]

        root = QtWidgets.QHBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        for gk in reversed(left_groups):
            color = axis_color_for(gk)
            label = axis_label_for(gk)
            yw    = YAxisWidget(color, label, side="left", parent=self)
            root.addWidget(yw)
            self.y_widgets[gk] = yw

        self.pw = pg.PlotWidget(background="#0d1117")
        self.pw.showGrid(x=True, y=True, alpha=0.15)
        self.pw.setMouseEnabled(x=True, y=False)
        self.pw.getAxis("left").hide()
        self.pw.getAxis("right").hide()
        self.pw.setClipToView(True)
        self.pw.setDownsampling(auto=True, mode='peak')
        pi = self.pw.getPlotItem()
        pi.getAxis("bottom").setPen(pg.mkPen("#30363d"))
        pi.getAxis("bottom").setTextPen(pg.mkPen("#8b949e"))
        if not show_x:
            pi.getAxis("bottom").setStyle(showValues=False)
        else:
            pi.getAxis("bottom").setLabel("Time (s)")

        if x_link_target is not None:
            pi.setXLink(x_link_target)

        tl = pg.TextItem(title, color="#ffffff", anchor=(0, 1))
        tl.setFont(QtGui.QFont("monospace", 8, QtGui.QFont.Weight.Bold))
        tl.setParentItem(self.pw.getPlotItem().vb)
        tl.setFlag(tl.GraphicsItemFlag.ItemIgnoresTransformations)
        tl.setPos(6, 18)

        root.addWidget(self.pw, 1)

        for gk in right_groups:
            color = axis_color_for(gk)
            label = axis_label_for(gk)
            yw    = YAxisWidget(color, label, side="right", parent=self)
            root.addWidget(yw)
            self.y_widgets[gk] = yw

        for ch in channels:
            color = COLORS[int(ch) % len(COLORS)]
            curve = pg.PlotCurveItem(
                pen=pg.mkPen(color, width=1.5),
                skipFiniteCheck=True,
            )
            self.pw.addItem(curve)
            self.curves[ch] = curve

        self.live_overlay = LiveLabelOverlay(channels, self.pw.viewport())
        self.live_overlay.show()

        self.pw.setYRange(0, 1, padding=0)

    def get_plot_item(self):
        return self.pw.getPlotItem()

    def update(self, snap: dict):
        group_ranges = {}
        for gk in set(self.ch_to_group.values()):
            all_y = []
            for ch in gk:
                t_all, y_all = snap.get(ch, ([], []))
                if not t_all:
                    continue
                t_arr = np.asarray(t_all, dtype=np.float64)
                y_arr = np.asarray(y_all, dtype=np.float32)
                t_max = float(t_arr[-1])
                mask  = t_arr >= (t_max - RENDER_WINDOW)
                y_win = y_arr[mask]
                if len(y_win):
                    all_y.append(y_win)
            if not all_y:
                continue
            combined = np.concatenate(all_y)
            ymin = float(combined.min())
            ymax = float(combined.max())
            rng  = ymax - ymin if ymax != ymin else 1.0
            pad  = rng * Y_PAD
            group_ranges[gk] = (ymin - pad, ymax + pad)

        for ch in self.channels:
            t_all, y_all = snap.get(ch, ([], []))
            if not t_all:
                continue

            t_arr = np.asarray(t_all, dtype=np.float64)
            y_arr = np.asarray(y_all, dtype=np.float32)
            t_max = float(t_arr[-1])
            mask  = t_arr >= (t_max - RENDER_WINDOW)
            t_win = t_arr[mask]
            y_win = y_arr[mask]

            if len(y_win) == 0:
                continue

            gk = self.ch_to_group[ch]
            if gk not in group_ranges:
                continue

            lo, hi = group_ranges[gk]
            rng    = hi - lo if hi != lo else 1.0
            y_norm = (y_win - lo) / rng
            self.curves[ch].setData(t_win, y_norm)
            self.y_widgets[gk].set_range(lo, hi)
            self.live_overlay.set_value(ch, float(y_arr[-1]))

# ╔══════════════════════════════════════════════════════════════════╗
# ║ MAIN WINDOW                                                     ║
# ╚══════════════════════════════════════════════════════════════════╝
class MainWindow(QtWidgets.QMainWindow):

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Serial Plotter")
        self.resize(1800, 1000)

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        root = QtWidgets.QHBoxLayout(central)
        root.setContentsMargins(4, 4, 4, 4)
        root.setSpacing(4)

        self.plot_col = QtWidgets.QWidget()
        self.plot_col.setStyleSheet("background:#0d1117;")
        self.plot_vbox = QtWidgets.QVBoxLayout(self.plot_col)
        self.plot_vbox.setContentsMargins(0, 0, 0, 0)
        self.plot_vbox.setSpacing(2)
        root.addWidget(self.plot_col, 1)

        side = QtWidgets.QWidget()
        side.setFixedWidth(230)
        side.setStyleSheet("background:#161b22;")
        sl = QtWidgets.QVBoxLayout(side)
        sl.setContentsMargins(8, 8, 8, 8)
        sl.setSpacing(2)
        hdr = QtWidgets.QLabel("Channels")
        hdr.setStyleSheet("color:cyan;font-weight:bold;font-size:13px;")
        sl.addWidget(hdr)
        self.checks = {}
        for ch in CHANNELS:
            col  = COLORS[int(ch) % len(COLORS)]
            text = f"{ch}  {CHANNELS[ch]['label']}"
            if CHANNELS[ch]['unit']:
                text += f"  ({CHANNELS[ch]['unit']})"
            cb = QtWidgets.QCheckBox(text)
            cb.setChecked(ch in DEFAULT_ON)
            cb.setStyleSheet(f"color:{col};")
            cb.stateChanged.connect(self.build)
            sl.addWidget(cb)
            self.checks[ch] = cb
        sl.addStretch()
        root.addWidget(side)

        self.panel_widgets: list[PanelWidget] = []
        self.build()

        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(UPDATE_MS)

    def build(self):
        for pw in self.panel_widgets:
            self.plot_vbox.removeWidget(pw)
            pw.deleteLater()
        self.panel_widgets.clear()

        active = []
        for pdef in PANELS:
            chs = [ch for ch in pdef["channels"]
                   if self.checks[ch].isChecked()]
            if chs:
                active.append({"title": pdef["title"], "channels": chs})

        n = len(active)
        if n == 0:
            return

        first_pi = None
        for i, pdef in enumerate(active):
            show_x = (i == n - 1)
            pw = PanelWidget(
                title=pdef["title"],
                channels=pdef["channels"],
                show_x=show_x,
                x_link_target=first_pi,
                parent=self.plot_col,
            )
            if first_pi is None:
                first_pi = pw.get_plot_item()
            self.plot_vbox.addWidget(pw)
            self.panel_widgets.append(pw)

    def update_plot(self):
        with lock:
            if t0 is None:
                return
            all_ch = [ch for p in self.panel_widgets for ch in p.channels]
            snap = {
                ch: (list(channel_data[ch]["t"]),
                     list(channel_data[ch]["y"]))
                for ch in all_ch
            }
        for pw in self.panel_widgets:
            pw.update(snap)

# ╔══════════════════════════════════════════════════════════════════╗
# ║ MAIN                                                            ║
# ╚══════════════════════════════════════════════════════════════════╝
def main():
    global running
    port = sys.argv[1] if len(sys.argv) > 1 else detect_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD
    if not port:
        print("No serial port found"); return
    print(f"[serial] {port} @ {baud}")
    threading.Thread(target=serial_thread, args=(port, baud),
                     daemon=True).start()
    app = QtWidgets.QApplication(sys.argv)
    win = MainWindow()
    win.showMaximized()
    try:
        sys.exit(app.exec())
    finally:
        running = False

if __name__ == "__main__":
    main()