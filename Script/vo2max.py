#!/usr/bin/env python3
"""
Serial Plotter
- Each panel is a QWidget row containing one pg.PlotWidget (canvas)
- Extra Y axes are real Qt widgets placed beside the canvas in a
  QHBoxLayout — no pyqtgraph scene layout manipulation whatsoever
- All channels in a panel share the X axis; each has its own Y axis
  that auto-scales independently every frame
- Exception: channels listed in SHARED_AXES share a single Y axis
- CSV logging: every new packet triggers a full-snapshot row written
  to a timestamped CSV file via a background writer thread
- A dashed zero line is drawn on each panel to make bipolar signals
  (e.g. Flow) easy to read
"""

import sys, threading, collections, struct, csv, queue
from datetime import datetime
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
    "4":  {"label": "Rho",                   "unit": "kg/m³"},
    "5":  {"label": "O2",                    "unit": "%"},
    "6":  {"label": "CO2",                   "unit": "ppm"},
    "7":  {"label": "Differential Pressure", "unit": "Pa"},
    "8":  {"label": "Flow",                  "unit": "L/s"},
    "9":  {"label": "Expiratory Flow",       "unit": "L/s"},
    "10": {"label": "Cycle Vol",             "unit": "L"},
    "11": {"label": "Total Vol",             "unit": "L"},
    "12": {"label": "Respiratory Rate",      "unit": "breaths/min"},
    "13": {"label": "VO2",                   "unit": "mL/kg/min"},
    "14": {"label": "VO2max",                "unit": "mL/kg/min"},
    "15": {"label": "VCO2",                  "unit": "mL/kg/min"},
    "16": {"label": "Respiratory Quotient",  "unit": ""}
}

# Ordered channel list for consistent CSV column ordering
CHANNEL_ORDER = [str(i) for i in range(17)]

PANELS = [
    {"title": "Environmental",                              "channels": ["0","1","2","3","4"]},
    {"title": "Flow",                                       "channels": ["7","8","10","11"]},
    {"title": "Gas",                                        "channels": ["5","6"]},
    {"title": "Expiratory Flow / VO2 / VO2max / VCO2",     "channels": ["9","13","14","15"]},
    {"title": "Respiratory Rate / Respiratory Quotient",    "channels": ["12","16"]},
]

# Groups of channels that share a single Y axis widget.
SHARED_AXES = [
    ["13", "14", "15"],   # VO2, VO2max and VCO2 share one Y axis
]

COLORS = [
    "#00d4ff","#ff6b6b","#51cf66","#ffd43b",
    "#cc5de8","#ff922b","#74c0fc","#f783ac",
    "#20c997","#ff8787","#a9e34b","#ffec99",
    "#e599f7","#ffa94d","#4dabf7","#f9a8d4",
    "#a5d8ff",
]

DEFAULT_ON    = {"9", "13", "14", "15"}
UPDATE_MS     = 40
RENDER_WINDOW = 300.0   # seconds
MAX_POINTS    = 200_000
DEFAULT_PORT  = "COM6"
DEFAULT_BAUD  = 921_600
AXIS_WIDTH    = 52      # px per Y axis widget
Y_PAD         = 0.05    # 5 % padding above and below autoscale

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

# Latest known value per channel for CSV snapshot rows (None = not yet received)
latest_values = {ch: None for ch in CHANNELS}

running = True
t0      = None

# ╔══════════════════════════════════════════════════════════════════╗
# ║ CSV LOGGING                                                     ║
# ╚══════════════════════════════════════════════════════════════════╝
csv_queue: queue.Queue = queue.Queue(maxsize=50_000)

def csv_header():
    return ["timestamp"] + [CHANNELS[ch]["label"] for ch in CHANNEL_ORDER]

def csv_writer_thread(filepath: str):
    """
    Reads snapshot rows from csv_queue and writes them to disk.
    Runs as a daemon thread; exits when it receives None as a sentinel.
    """
    with open(filepath, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(csv_header())
        while True:
            row = csv_queue.get()
            if row is None:      # sentinel — shut down
                break
            writer.writerow(row)
            csv_queue.task_done()

def enqueue_snapshot(timestamp_s: float):
    """
    Build a full-snapshot CSV row from latest_values and push it onto
    the queue. Called from the serial thread (already holding `lock`).
    Values not yet received are written as empty strings.
    Raw values are stored unchanged.
    """
    row = [f"{timestamp_s:.6f}"]
    for ch in CHANNEL_ORDER:
        v = latest_values[ch]
        row.append("" if v is None else repr(v))
    try:
        csv_queue.put_nowait(row)
    except queue.Full:
        pass   # drop rather than stall the serial thread

# ╔══════════════════════════════════════════════════════════════════╗
# ║ SERIAL                                                          ║
# ╚══════════════════════════════════════════════════════════════════╝
def detect_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if any(k in p.device for k in ("USB", "ACM", "COM")):
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
                # Update latest snapshot and log a CSV row (raw values)
                latest_values[ch] = val
                enqueue_snapshot(t)

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
        self._color = QtGui.QColor(color)
        self._label = label
        self._side  = side
        self._lo    = 0.0
        self._hi    = 1.0
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
            val      = lo + (hi - lo) * i / n_ticks
            y        = int(h - (val - lo) / rng * h)
            y        = max(1, min(h - 1, y))
            tick_len = 5
            if self._side == "left":
                p.drawLine(w - 1, y, w - 1 - tick_len, y)
                txt = f"{val:.3g}"
                tw  = fm.horizontalAdvance(txt)
                p.drawText(w - 1 - tick_len - tw - 6,
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

        # Zero reference line — dashed grey, hidden until data arrives.
        # Drawn in normalised [0,1] Y space; repositioned each frame.
        self._zero_line = pg.InfiniteLine(
            pos=0.5, angle=0,
            pen=pg.mkPen("#444c56", width=1,
                         style=QtCore.Qt.PenStyle.DashLine),
        )
        self._zero_line.setVisible(False)
        self.pw.addItem(self._zero_line)

        # Map group_key → zero-line (one shared line; last writer wins,
        # which is fine because all groups share the same normalised axis)
        self._group_ranges: dict = {}

        self.live_overlay = LiveLabelOverlay(channels, self.pw.viewport())
        self.live_overlay.show()

        self.pw.setYRange(0, 1, padding=0)

    def get_plot_item(self):
        return self.pw.getPlotItem()

    def update(self, snap: dict):
        # ── 1. Compute autoscale range per group ─────────────────────
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

        # ── 2. Update curves, axis labels, live overlay ───────────────
        zero_norm = None   # will be set by whichever group contains ch 0
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

            # Compute normalised position of zero for this group.
            # Use the last group processed; all share the same plot space.
            zero_norm = (0.0 - lo) / rng

        # ── 3. Position the zero reference line ───────────────────────
        if zero_norm is not None and 0.0 <= zero_norm <= 1.0:
            self._zero_line.setPos(zero_norm)
            self._zero_line.setVisible(True)
        else:
            # Zero is outside the visible range — hide the line so it
            # doesn't clutter a panel where all values are, say, > 100.
            self._zero_line.setVisible(False)


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

        # CSV path label at the bottom of the sidebar
        self.csv_label = QtWidgets.QLabel()
        self.csv_label.setWordWrap(True)
        self.csv_label.setStyleSheet("color:#8b949e;font-size:9px;")
        sl.addWidget(self.csv_label)

        root.addWidget(side)

        self.panel_widgets: list[PanelWidget] = []
        self.build()

        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_plot)
        self.timer.start(UPDATE_MS)

    def set_csv_path(self, path: str):
        self.csv_label.setText(f"📄 {path}")

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

    # Start CSV writer thread
    session_time = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = f"serial_log_{session_time}.csv"
    writer_t = threading.Thread(
        target=csv_writer_thread, args=(csv_path,), daemon=True)
    writer_t.start()
    print(f"[csv]    logging to {csv_path}")

    threading.Thread(target=serial_thread, args=(port, baud),
                     daemon=True).start()

    app = QtWidgets.QApplication(sys.argv)
    win = MainWindow()
    win.set_csv_path(csv_path)
    win.showMaximized()
    try:
        sys.exit(app.exec())
    finally:
        running = False
        csv_queue.put(None)      # signal writer to flush and exit
        writer_t.join(timeout=5)

if __name__ == "__main__":
    main()