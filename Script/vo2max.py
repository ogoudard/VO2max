#!/usr/bin/env python3
"""
Serial Plotter — white background, thin curves, fixed Y-axis labels,
                  crosshair with X (time) and Y (real-unit) readout.
"""

import sys, threading, collections, struct, csv, queue
from datetime import datetime
import serial, serial.tools.list_ports
from PyQt6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg
import numpy as np

pg.setConfigOptions(useOpenGL=True, antialias=False,
                    background="#ffffff", foreground="#333333")

# ╔══════════════════════════════════════════════════════════════════╗
# ║ PROTOCOL                                                        ║
# ╚══════════════════════════════════════════════════════════════════╝
SYNC_BYTE   = 0xAA
PACKET_SIZE = 18
FMT         = "<BdQ"

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
    "16": {"label": "Respiratory Quotient",  "unit": ""},
    "17": {"label": "Power",                 "unit": "W"}
}

CHANNEL_ORDER = [str(i) for i in range(17)]

PANELS = [
    {"title": "Environmental",                              "channels": ["0","1","2","3","4"]},
    {"title": "Flow",                                       "channels": ["7","8","10","11"]},
    {"title": "Gas",                                        "channels": ["5","6"]},
    {"title": "Expiratory Flow / VO2 / VO2max / VCO2",      "channels": ["9","13","14","15"]},
    {"title": "Respiratory Rate / Respiratory Quotient",    "channels": ["12","16"]},
    {"title": "Power / Cadence / Torque",                   "channels": ["17"]}
]

SHARED_AXES = [
    ["13", "14", "15"],
]

COLORS = [
    "#0077cc", "#cc2200", "#007733", "#cc8800",
    "#7700cc", "#cc5500", "#0055aa", "#aa0077",
    "#007766", "#993300", "#336600", "#886600",
    "#660099", "#994400", "#004488", "#882255",
    "#003377",
]

DEFAULT_ON    = {"9", "13", "14", "15", "17"}
UPDATE_MS     = 40
RENDER_WINDOW = 300.0
MAX_POINTS    = 200_000
DEFAULT_BAUD  = 921_600
AXIS_WIDTH    = 64
Y_PAD         = 0.05

# ╔══════════════════════════════════════════════════════════════════╗
# ║ SHARED-AXIS HELPERS                                             ║
# ╚══════════════════════════════════════════════════════════════════╝
def build_axis_map():
    m = {}
    for group in SHARED_AXES:
        key = frozenset(group)
        for ch in group:
            m[ch] = key
    return m

AXIS_MAP = build_axis_map()

def group_key_for(ch):
    return AXIS_MAP.get(ch, frozenset({ch}))

def axis_label_for(gk):
    return " / ".join(CHANNELS[ch]["label"] for ch in sorted(gk, key=int))

def axis_color_for(gk):
    return COLORS[int(min(gk, key=int)) % len(COLORS)]

# ╔══════════════════════════════════════════════════════════════════╗
# ║ DATA                                                            ║
# ╚══════════════════════════════════════════════════════════════════╝
lock          = threading.Lock()
channel_data  = {ch: {"t": collections.deque(), "y": collections.deque()} for ch in CHANNELS}
latest_values = {ch: None for ch in CHANNELS}
running       = True
t0            = None

# ╔══════════════════════════════════════════════════════════════════╗
# ║ CSV LOGGING                                                     ║
# ╚══════════════════════════════════════════════════════════════════╝
csv_queue: queue.Queue = queue.Queue(maxsize=50_000)

def csv_header():
    return ["timestamp"] + [CHANNELS[ch]["label"] for ch in CHANNEL_ORDER]

def csv_writer_thread(filepath: str):
    with open(filepath, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(csv_header())
        while True:
            row = csv_queue.get()
            if row is None:
                break
            writer.writerow(row)
            csv_queue.task_done()

# Pending row accumulator — groups channels that share the same timestamp.
# Accessed only from the serial thread (no extra lock needed).
_csv_pending_ts:  float | None = None          # timestamp of the open row
_csv_pending_row: dict         = {}            # ch -> val for the open row

def _flush_pending_row():
    """Serialise the current pending row and push it onto the write queue."""
    global _csv_pending_ts, _csv_pending_row
    if _csv_pending_ts is None:
        return
    row = [f"{_csv_pending_ts:.6f}"]
    for c in CHANNEL_ORDER:
        v = _csv_pending_row.get(c)
        row.append("" if v is None else repr(v))
    try:
        csv_queue.put_nowait(row)
    except queue.Full:
        pass
    _csv_pending_ts  = None
    _csv_pending_row = {}

def enqueue_sample(timestamp_s: float, ch: str, val: float):
    """Accumulate ch/val into the pending row; flush when the timestamp changes."""
    global _csv_pending_ts, _csv_pending_row
    if _csv_pending_ts is not None and timestamp_s != _csv_pending_ts:
        _flush_pending_row()
    _csv_pending_ts      = timestamp_s
    _csv_pending_row[ch] = val

# ╔══════════════════════════════════════════════════════════════════╗
# ║ PORT DETECTION                                                  ║
# ╚══════════════════════════════════════════════════════════════════╝
LILYGO_VIDS = {0x10C4, 0x1A86, 0x303A}
LILYGO_DESCRIPTION_KEYWORDS = ("cp210","ch910","ch340","esp32","lilygo","ttgo","uart bridge")

def detect_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if p.vid in LILYGO_VIDS:
            return p.device
    for p in ports:
        if any(kw in (p.description or "").lower() for kw in LILYGO_DESCRIPTION_KEYWORDS):
            return p.device
    return None

# ╔══════════════════════════════════════════════════════════════════╗
# ║ PORT PICKER                                                     ║
# ╚══════════════════════════════════════════════════════════════════╝
class PortPickerDialog(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Select Serial Port")
        self.setMinimumWidth(460)
        self.selected_port = None
        self.selected_baud = DEFAULT_BAUD
        layout = QtWidgets.QVBoxLayout(self)
        lbl = QtWidgets.QLabel(
            "No LilyGo T-Display port detected automatically.\n"
            "Select the correct port from the list below:")
        layout.addWidget(lbl)
        self.port_list = QtWidgets.QListWidget()
        for p in serial.tools.list_ports.comports():
            desc = p.description or "Unknown device"
            vid_str = f"  [VID:{p.vid:04X}]" if p.vid else ""
            item = QtWidgets.QListWidgetItem(f"{p.device}  —  {desc}{vid_str}")
            item.setData(QtCore.Qt.ItemDataRole.UserRole, p.device)
            self.port_list.addItem(item)
        if self.port_list.count():
            self.port_list.setCurrentRow(0)
        else:
            self.port_list.addItem("No serial ports found")
        layout.addWidget(self.port_list)
        baud_row = QtWidgets.QHBoxLayout()
        baud_row.addWidget(QtWidgets.QLabel("Baud rate:"))
        self.baud_box = QtWidgets.QComboBox()
        for b in [9600, 115200, 230400, 460800, 921600]:
            self.baud_box.addItem(str(b))
        self.baud_box.setCurrentText(str(DEFAULT_BAUD))
        baud_row.addWidget(self.baud_box)
        baud_row.addStretch()
        layout.addLayout(baud_row)
        btn_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok |
            QtWidgets.QDialogButtonBox.StandardButton.Cancel)
        btn_box.accepted.connect(self.accept)
        btn_box.rejected.connect(self.reject)
        layout.addWidget(btn_box)

    def accept(self):
        item = self.port_list.currentItem()
        if item:
            self.selected_port = item.data(QtCore.Qt.ItemDataRole.UserRole)
        self.selected_baud = int(self.baud_box.currentText())
        super().accept()

# ╔══════════════════════════════════════════════════════════════════╗
# ║ SERIAL                                                          ║
# ╚══════════════════════════════════════════════════════════════════╝
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
                latest_values[ch] = val
                enqueue_sample(t, ch, val)
    # Flush the last partial row (may never arrive if stream cuts mid-burst)
    _flush_pending_row()

# ╔══════════════════════════════════════════════════════════════════╗
# ║ CROSSHAIR MANAGER  (broadcasts X position to all panels)       ║
# ╚══════════════════════════════════════════════════════════════════╝
class CrosshairManager(QtCore.QObject):
    """Emits the current time-axis X when any panel moves the crosshair."""
    x_changed = QtCore.pyqtSignal(float)   # real time value
    hidden    = QtCore.pyqtSignal()         # mouse left all panels

    def __init__(self, parent=None):
        super().__init__(parent)

# ╔══════════════════════════════════════════════════════════════════╗
# ║ Y-AXIS WIDGET                                                   ║
# ╚══════════════════════════════════════════════════════════════════╝
class YAxisWidget(QtWidgets.QWidget):
    TICK = 5
    GAP  = 3

    def __init__(self, color: str, label: str, side: str = "left", parent=None):
        super().__init__(parent)
        self.setFixedWidth(AXIS_WIDTH)
        self._color = QtGui.QColor(color)
        self._label = label
        self._side  = side
        self._lo    = 0.0
        self._hi    = 1.0
        self.setStyleSheet("background:#ffffff;")

    def set_range(self, lo: float, hi: float):
        if lo != self._lo or hi != self._hi:
            self._lo, self._hi = lo, hi
            self.update()

    def paintEvent(self, _):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, False)

        w, h   = self.width(), self.height()
        lo, hi = self._lo, self._hi
        rng    = hi - lo if hi != lo else 1.0

        num_font = QtGui.QFont("monospace", 7)
        lbl_font = QtGui.QFont("monospace", 7, QtGui.QFont.Weight.Bold)
        num_fm   = QtGui.QFontMetrics(num_font)
        lbl_fm   = QtGui.QFontMetrics(lbl_font)
        ascent   = lbl_fm.ascent()

        col_pen = QtGui.QPen(self._color, 1)
        p.setPen(col_pen)

        spine_x = w - 1 if self._side == "left" else 0
        p.drawLine(spine_x, 0, spine_x, h)

        p.setFont(num_font)
        for i in range(7):
            val = lo + (hi - lo) * i / 6
            y   = int(h - (val - lo) / rng * h)
            y   = max(1, min(h - 1, y))
            txt = f"{val:.3g}"
            tw  = num_fm.horizontalAdvance(txt)
            ty  = y + num_fm.ascent() // 2

            if self._side == "left":
                p.drawLine(spine_x, y, spine_x - self.TICK, y)
                p.drawText(spine_x - self.TICK - self.GAP - tw, ty, txt)
            else:
                p.drawLine(spine_x, y, spine_x + self.TICK, y)
                p.drawText(spine_x + self.TICK + self.GAP, ty, txt)

        p.save()
        p.setFont(lbl_font)
        p.setPen(col_pen)
        lw = lbl_fm.horizontalAdvance(self._label)

        if self._side == "left":
            tx = ascent
        else:
            tx = w - 2 * ascent - 2

        p.translate(tx, h // 2)
        p.rotate(-90)
        p.drawText(-lw // 2, 0, self._label)
        p.restore()
        p.end()


# ╔══════════════════════════════════════════════════════════════════╗
# ║ LIVE LABEL OVERLAY                                              ║
# ╚══════════════════════════════════════════════════════════════════╝
class LiveLabelOverlay(QtWidgets.QWidget):
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
        self._colors  = {ch: QtGui.QColor(COLORS[int(ch) % len(COLORS)]) for ch in channels}
        self._bg      = QtGui.QColor(255, 255, 255, 200)
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
        bh = fm.height() + self.PAD_Y * 2
        x_right = self.width() - self.MARGIN
        y = self.MARGIN
        for ch in self.channels:
            val = self._values[ch]
            if val is None:
                continue
            unit = CHANNELS[ch]['unit']
            txt  = f"{CHANNELS[ch]['label']}: {val:.4g} {unit}".strip()
            bw   = fm.horizontalAdvance(txt) + self.PAD_X * 2
            rect = QtCore.QRectF(x_right - bw, y, bw, bh)
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
                 x_link_target=None, crosshair_mgr: CrosshairManager = None,
                 parent=None):
        super().__init__(parent)
        self.channels    = channels
        self.curves      = {}
        self.y_widgets   = {}
        self.ch_to_group = {}
        self._snap_data  = {}   # ch -> (t_arr, y_arr) for crosshair interpolation
        self._group_ranges: dict = {}  # gk -> (lo, hi)
        self._xhair_mgr  = crosshair_mgr

        # Build local group keys
        seen_groups, seen_set = [], set()
        for ch in channels:
            gk_local = frozenset(c for c in group_key_for(ch) if c in channels)
            self.ch_to_group[ch] = gk_local
            if gk_local not in seen_set:
                seen_set.add(gk_local)
                seen_groups.append(gk_local)

        n_left       = (len(seen_groups) + 1) // 2
        left_groups  = seen_groups[:n_left]
        right_groups = seen_groups[n_left:]

        root = QtWidgets.QHBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        for gk in reversed(left_groups):
            yw = YAxisWidget(axis_color_for(gk), axis_label_for(gk), "left", self)
            root.addWidget(yw)
            self.y_widgets[gk] = yw

        # ── Plot canvas ────────────────────────────────────────────
        self.pw = pg.PlotWidget(background="#ffffff")
        self.pw.showGrid(x=True, y=True, alpha=0.2)
        self.pw.setMouseEnabled(x=True, y=False)
        left_ax = self.pw.getAxis("left")
        left_ax.setTicks([[(i/6, "") for i in range(7)], []])
        left_ax.setStyle(tickLength=0)
        left_ax.setPen(pg.mkPen(None))
        left_ax.setTextPen(pg.mkPen(None))
        self.pw.getAxis("right").hide()
        self.pw.setClipToView(True)
        self.pw.setDownsampling(auto=True, mode='peak')
        pi = self.pw.getPlotItem()
        pi.getAxis("bottom").setPen(pg.mkPen("#aaaaaa"))
        pi.getAxis("bottom").setTextPen(pg.mkPen("#444444"))
        if not show_x:
            pi.getAxis("bottom").setStyle(showValues=False)
        else:
            pi.getAxis("bottom").setLabel("Time (s)")

        if x_link_target is not None:
            pi.setXLink(x_link_target)

        tl = pg.TextItem(title, color="#222222", anchor=(0, 1))
        tl.setFont(QtGui.QFont("monospace", 8, QtGui.QFont.Weight.Bold))
        tl.setParentItem(pi.vb)
        tl.setFlag(tl.GraphicsItemFlag.ItemIgnoresTransformations)
        tl.setPos(6, 18)

        root.addWidget(self.pw, 1)

        for gk in right_groups:
            yw = YAxisWidget(axis_color_for(gk), axis_label_for(gk), "right", self)
            root.addWidget(yw)
            self.y_widgets[gk] = yw

        for ch in channels:
            curve = pg.PlotCurveItem(
                pen=pg.mkPen(COLORS[int(ch) % len(COLORS)], width=0.8),
                skipFiniteCheck=True,
            )
            self.pw.addItem(curve)
            self.curves[ch] = curve

        self._zero_line = pg.InfiniteLine(
            pos=0.5, angle=0,
            pen=pg.mkPen("#bbbbbb", width=1, style=QtCore.Qt.PenStyle.DashLine),
        )
        self._zero_line.setVisible(False)
        self.pw.addItem(self._zero_line)

        self.live_overlay = LiveLabelOverlay(channels, self.pw.viewport())
        self.live_overlay.show()
        self.pw.setYRange(0, 1, padding=0)

        # ── Crosshair lines ────────────────────────────────────────
        self._vline = pg.InfiniteLine(
            angle=90,
            pen=pg.mkPen("#555555", width=1, style=QtCore.Qt.PenStyle.DashLine),
            movable=False,
        )
        self._hline = pg.InfiniteLine(
            angle=0,
            pen=pg.mkPen("#555555", width=1, style=QtCore.Qt.PenStyle.DashLine),
            movable=False,
        )
        self._vline.setVisible(False)
        self._hline.setVisible(False)
        self.pw.addItem(self._vline, ignoreBounds=True)
        self.pw.addItem(self._hline, ignoreBounds=True)

        # Crosshair tooltip: a rounded-rect + text drawn in scene-pixel space
        # so it floats right next to the cursor regardless of zoom/pan.
        scene = self.pw.scene()
        self._tip_bg   = QtWidgets.QGraphicsRectItem()
        self._tip_text = QtWidgets.QGraphicsTextItem()
        self._tip_text.setFont(QtGui.QFont("monospace", 8))
        self._tip_bg.setPen(QtGui.QPen(QtGui.QColor("#888888"), 0.5))
        self._tip_bg.setBrush(QtGui.QBrush(QtGui.QColor(255, 255, 255, 220)))
        self._tip_bg.setZValue(100)
        self._tip_text.setZValue(101)
        scene.addItem(self._tip_bg)
        scene.addItem(self._tip_text)
        self._tip_bg.setVisible(False)
        self._tip_text.setVisible(False)
        self._last_scene_pos: QtCore.QPointF | None = None  # updated on mouse move

        # Throttled mouse-move proxy (50 ms rate limit)
        self._proxy = pg.SignalProxy(
            self.pw.scene().sigMouseMoved,
            rateLimit=50,
            slot=self._on_mouse_moved,
        )

        # Connect leave event by subclassing viewport filter
        self.pw.viewport().installEventFilter(self)

        # Connect to manager signals
        if self._xhair_mgr is not None:
            self._xhair_mgr.x_changed.connect(self._on_x_broadcast)
            self._xhair_mgr.hidden.connect(self._hide_crosshair)

    # ── event filter: hide crosshair when mouse leaves viewport ───
    def eventFilter(self, obj, event):
        if obj is self.pw.viewport():
            if event.type() == QtCore.QEvent.Type.Leave:
                if self._xhair_mgr is not None:
                    self._xhair_mgr.hidden.emit()
                else:
                    self._hide_crosshair()
        return False

    def get_plot_item(self):
        return self.pw.getPlotItem()

    # ── Mouse moved (local panel) ──────────────────────────────────
    def _on_mouse_moved(self, args):
        pos = args[0]
        vb  = self.pw.getPlotItem().vb
        if not self.pw.sceneBoundingRect().contains(pos):
            return
        mouse_point = vb.mapSceneToView(pos)
        x_val  = mouse_point.x()
        y_norm = mouse_point.y()

        # Remember scene position so the tooltip can follow the cursor
        self._last_scene_pos = pos

        # Update horizontal line immediately from local mouse Y
        self._hline.setPos(y_norm)
        self._hline.setVisible(True)

        # Broadcast X to all panels (including self via signal)
        if self._xhair_mgr is not None:
            self._xhair_mgr.x_changed.emit(x_val)
        else:
            self._update_crosshair_at_x(x_val, y_norm)

    # ── Receive broadcast X from manager ──────────────────────────
    def _on_x_broadcast(self, x_val: float):
        self._vline.setPos(x_val)
        self._vline.setVisible(True)
        self._update_crosshair_label(x_val)

    def _hide_crosshair(self):
        self._vline.setVisible(False)
        self._hline.setVisible(False)
        self._tip_bg.setVisible(False)
        self._tip_text.setVisible(False)

    def _update_crosshair_at_x(self, x_val: float, y_norm: float = None):
        """Used when no CrosshairManager is provided (single panel)."""
        self._vline.setPos(x_val)
        self._vline.setVisible(True)
        self._update_crosshair_label(x_val)

    # ── Build the text label at the given time X ───────────────────
    def _update_crosshair_label(self, x_val: float):
        # Only draw the tooltip on the panel the mouse is currently over
        if self._last_scene_pos is None:
            return
        if not self.pw.sceneBoundingRect().contains(self._last_scene_pos):
            self._tip_bg.setVisible(False)
            self._tip_text.setVisible(False)
            return

        # Build text: time on first line, then one line per channel
        lines = [f"t = {x_val:.3f} s"]
        for ch in self.channels:
            if ch not in self._snap_data:
                continue
            t_arr, y_arr = self._snap_data[ch]
            if len(t_arr) < 2:
                continue
            idx = np.searchsorted(t_arr, x_val)
            if idx <= 0 or idx >= len(t_arr):
                continue
            t0_, t1_ = t_arr[idx - 1], t_arr[idx]
            y0_, y1_ = y_arr[idx - 1], y_arr[idx]
            dt   = t1_ - t0_
            frac = (x_val - t0_) / dt if dt != 0 else 0.0
            y_real = float(y0_ + frac * (y1_ - y0_))
            unit  = CHANNELS[ch]["unit"]
            label = CHANNELS[ch]["label"]
            lines.append(f"{label}: {y_real:.4g} {unit}".strip())

        html = "<br>".join(lines)
        self._tip_text.setHtml(
            f'<span style="font-family:monospace;font-size:8pt;">{html}</span>'
        )

        # ── Position tooltip in scene pixels, offset from cursor ──
        OFFSET_X = 12   # px right of cursor
        OFFSET_Y = -8   # px above cursor
        PAD      = 5    # inner padding of background rect

        br    = self._tip_text.boundingRect()
        tw    = br.width()
        th    = br.height()
        scene_rect = self.pw.sceneBoundingRect()

        sx = self._last_scene_pos.x()
        sy = self._last_scene_pos.y()

        # Flip to left if too close to right edge
        if sx + OFFSET_X + tw + PAD * 2 > scene_rect.right():
            tx = sx - OFFSET_X - tw - PAD * 2
        else:
            tx = sx + OFFSET_X

        # Flip downward if too close to top edge
        if sy + OFFSET_Y - th - PAD * 2 < scene_rect.top():
            ty = sy + abs(OFFSET_Y)
        else:
            ty = sy + OFFSET_Y - th - PAD * 2

        bg_rect = QtCore.QRectF(tx, ty, tw + PAD * 2, th + PAD * 2)
        self._tip_bg.setRect(bg_rect)
        self._tip_text.setPos(tx + PAD, ty + PAD)

        self._tip_bg.setVisible(True)
        self._tip_text.setVisible(True)

    # ── Data update ────────────────────────────────────────────────
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
                mask  = t_arr >= (float(t_arr[-1]) - RENDER_WINDOW)
                y_win = y_arr[mask]
                if len(y_win):
                    all_y.append(y_win)
            if not all_y:
                continue
            combined = np.concatenate(all_y)
            ymin, ymax = float(combined.min()), float(combined.max())
            pad = (ymax - ymin if ymax != ymin else 1.0) * Y_PAD
            group_ranges[gk] = (ymin - pad, ymax + pad)

        self._group_ranges = group_ranges

        zero_norm = None
        for ch in self.channels:
            t_all, y_all = snap.get(ch, ([], []))
            if not t_all:
                continue
            t_arr = np.asarray(t_all, dtype=np.float64)
            y_arr = np.asarray(y_all, dtype=np.float32)
            t_max = float(t_arr[-1])
            mask  = t_arr >= (t_max - RENDER_WINDOW)
            t_win, y_win = t_arr[mask], y_arr[mask]
            if not len(y_win):
                continue
            gk = self.ch_to_group[ch]
            if gk not in group_ranges:
                continue
            lo, hi = group_ranges[gk]
            rng    = hi - lo if hi != lo else 1.0
            self.curves[ch].setData(t_win, (y_win - lo) / rng)
            self.y_widgets[gk].set_range(lo, hi)
            self.live_overlay.set_value(ch, float(y_arr[-1]))
            zero_norm = (0.0 - lo) / rng

            # Cache windowed arrays for crosshair interpolation
            self._snap_data[ch] = (t_win, y_win)

        if zero_norm is not None and 0.0 <= zero_norm <= 1.0:
            self._zero_line.setPos(zero_norm)
            self._zero_line.setVisible(True)
        else:
            self._zero_line.setVisible(False)


# ╔══════════════════════════════════════════════════════════════════╗
# ║ MAIN WINDOW                                                     ║
# ╚══════════════════════════════════════════════════════════════════╝
class MainWindow(QtWidgets.QMainWindow):

    def __init__(self):
        super().__init__()
        self.setWindowTitle("VO2max Plotter")
        self.resize(1800, 1000)

        self._xhair_mgr = CrosshairManager(self)

        central = QtWidgets.QWidget()
        central.setStyleSheet("background:#ffffff;")
        self.setCentralWidget(central)
        root = QtWidgets.QHBoxLayout(central)
        root.setContentsMargins(4, 4, 4, 4)
        root.setSpacing(4)

        self.plot_col = QtWidgets.QWidget()
        self.plot_col.setStyleSheet("background:#ffffff;")
        self.plot_vbox = QtWidgets.QVBoxLayout(self.plot_col)
        self.plot_vbox.setContentsMargins(0, 0, 0, 0)
        self.plot_vbox.setSpacing(2)
        root.addWidget(self.plot_col, 1)

        side = QtWidgets.QWidget()
        side.setFixedWidth(230)
        side.setStyleSheet("background:#f0f0f0;")
        sl = QtWidgets.QVBoxLayout(side)
        sl.setContentsMargins(8, 8, 8, 8)
        sl.setSpacing(2)
        hdr = QtWidgets.QLabel("Channels")
        hdr.setStyleSheet("color:#0055aa;font-weight:bold;font-size:13px;")
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
        self.csv_label = QtWidgets.QLabel()
        self.csv_label.setWordWrap(True)
        self.csv_label.setStyleSheet("color:#555555;font-size:9px;")
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

        active = [
            {"title": p["title"],
             "channels": [ch for ch in p["channels"] if self.checks[ch].isChecked()]}
            for p in PANELS
            if any(self.checks[ch].isChecked() for ch in p["channels"])
        ]
        if not active:
            return

        first_pi = None
        for i, pdef in enumerate(active):
            pw = PanelWidget(
                title=pdef["title"],
                channels=pdef["channels"],
                show_x=(i == len(active) - 1),
                x_link_target=first_pi,
                crosshair_mgr=self._xhair_mgr,
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
            snap = {ch: (list(channel_data[ch]["t"]), list(channel_data[ch]["y"]))
                    for ch in all_ch}
        for pw in self.panel_widgets:
            pw.update(snap)


# ╔══════════════════════════════════════════════════════════════════╗
# ║ MAIN                                                            ║
# ╚══════════════════════════════════════════════════════════════════╝
def main():
    global running
    app = QtWidgets.QApplication(sys.argv)
    app.setStyle("Fusion")

    port = sys.argv[1] if len(sys.argv) > 1 else detect_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUD

    if not port:
        dlg = PortPickerDialog()
        if dlg.exec() != QtWidgets.QDialog.DialogCode.Accepted or not dlg.selected_port:
            print("No port selected — exiting.")
            sys.exit(0)
        port, baud = dlg.selected_port, dlg.selected_baud

    print(f"[serial] {port} @ {baud}")

    session_time = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = f"serial_log_{session_time}.csv"
    writer_t = threading.Thread(target=csv_writer_thread, args=(csv_path,), daemon=True)
    writer_t.start()
    print(f"[csv]    logging to {csv_path}")

    threading.Thread(target=serial_thread, args=(port, baud), daemon=True).start()

    win = MainWindow()
    win.set_csv_path(csv_path)
    win.showMaximized()
    try:
        sys.exit(app.exec())
    finally:
        running = False
        csv_queue.put(None)
        writer_t.join(timeout=5)


if __name__ == "__main__":
    main()