#!/usr/bin/env python3
import sys
import threading
import collections

import csv
import datetime

import serial
import serial.tools.list_ports

import matplotlib
matplotlib.use("QtAgg")  # High performance backend

import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.style as mplstyle

from matplotlib.widgets import CheckButtons

# ╔══════════════════════════════════════════════════════════════════╗
# ║                            CONFIG                                ║
# ╚══════════════════════════════════════════════════════════════════╝

CHANNELS = {
    "0":  {"label": "Temperature",        "unit": "°C"},
    "1":  {"label": "Humidity",           "unit": "%"},
    "2":  {"label": "Pressure",           "unit": "hPa"},
    "3":  {"label": "Altitude",           "unit": "m"},
    "4":  {"label": "Flow",               "unit": "L/s"},
    "5":  {"label": "Cycle Exhaled Vol.", "unit": "L"},
    "6":  {"label": "Total Exhaled Vol.", "unit": "L"},
    "7":  {"label": "O2",                 "unit": "%"},
    "8":  {"label": "CO2",                "unit": "ppm"},
    "9":  {"label": "VO2",                "unit": "mL/kg/min"},
    "10": {"label": "VO2max",             "unit": "mL/kg/min"},
    "11": {"label": "VCO2",               "unit": "mL/kg/min"},
    "12": {"label": "RQ",                 "unit": ""},
    "13": {"label": "RR",                 "unit": "breaths/min"},
    "14": {"label": "Rho",                "unit": "kg/m3"},
    "15": {"label": "Expiratory Flow",    "unit": "L/s"},
}

PANELS = [
    {"title": "Environmental (Temp / Humidity / Pressure / Altitude / Rho)",
     "channels": ["0", "1", "2", "3", "14"],
     "axes_groups": [["0"], ["1"], ["2"], ["3"], ["14"]],
     "left_groups": [0, 1]},  # groups 0,1 on left; rest on right
    {"title": "Flow / Cycle Exhaled Vol. / Total Exhaled Vol.",
     "channels": ["4", "5", "6"], "multi_yaxis": True},
    {"title": "O2 / CO2",            "channels": ["7", "8"], "multi_yaxis": True},
    {"title": "VO2 / VO2max / VCO2 / Expiratory Flow", "channels": ["9", "10", "11", "15"],
     "axes_groups": [["9", "10", "11"], ["15"]]},  # VO2+VCO2+VO2max share left axis, Exp.Flow gets its own
    {"title": "RQ / RR",             "channels": ["12", "13"], "multi_yaxis": True},
]

DEFAULT_PORT     = "COM6"
DEFAULT_BAUDRATE = 921600
UPDATE_INTERVAL  = 33
MAX_POINTS       = 10000

# ╔══════════════════════════════════════════════════════════════════╗
# ║                            STYLE                                 ║
# ╚══════════════════════════════════════════════════════════════════╝

COLORS = [
    "#00d4ff", "#ff6b6b", "#51cf66", "#ffd43b",
    "#cc5de8", "#ff922b", "#74c0fc", "#f783ac",
    "#20c997", "#ff8787", "#a9e34b", "#ffec99",
    "#e599f7", "#ffa94d", "#4dabf7", "#f9a8d4",
]

BG         = "#0d1117"
PANEL_BG   = "#161b22"
SPINE_COL  = "#30363d"
TICK_COL   = "#8b949e"

# ╔══════════════════════════════════════════════════════════════════╗
# ║                         DATA STORAGE                             ║
# ╚══════════════════════════════════════════════════════════════════╝

channel_data = {
    ch_id: {
        "t": collections.deque(maxlen=MAX_POINTS),
        "y": collections.deque(maxlen=MAX_POINTS)
    }
    for ch_id in CHANNELS
}

lock           = threading.Lock()
serial_running = True
parse_errors   = 0
t0             = None
csv_file       = None
csv_writer     = None
current_row    = {}   # latest known value per channel, for CSV fill-in

# ╔══════════════════════════════════════════════════════════════════╗
# ║                        SERIAL FUNCTIONS                          ║
# ╚══════════════════════════════════════════════════════════════════╝

def detect_port():
    ports = serial.tools.list_ports.comports()
    if not ports: return None
    for p in ports:
        if any(k in p.device for k in ("USB", "ACM", "COM")):
            return p.device
    return ports[0].device

def serial_reader(port, baudrate):
    global serial_running, parse_errors, t0, current_row
    print(f"[serial] Connecting to {port} @ {baudrate}")
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
    except Exception as e:
        print(f"[serial] ERROR: {e}")
        serial_running = False
        return

    ser.reset_input_buffer()
    while serial_running:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line: continue
        parts = line.split(",")
        if len(parts) != 3:
            parse_errors += 1
            continue

        ch_id, val_str, ts_str = parts[0].strip(), parts[1].strip(), parts[2].strip()
        val, ts_ms = float(val_str), float(ts_str)

        if ch_id not in CHANNELS:
            continue

        with lock:
            if t0 is None: t0 = ts_ms
            t_rel = (ts_ms - t0) / 1000.0
            channel_data[ch_id]["t"].append(t_rel)
            channel_data[ch_id]["y"].append(val)

            # Update the latest known value for this channel
            current_row[ch_id] = val

            # Write a full row immediately on every sample, filling in the
            # last known value for every other channel (empty if never received)
            if csv_writer:
                row = [f"{t_rel:.3f}"] + [current_row.get(cid, "") for cid in CHANNELS]
                csv_writer.writerow(row)
                csv_file.flush()


# ╔══════════════════════════════════════════════════════════════════╗
# ║                        BUILD LAYOUT                              ║
# ╚══════════════════════════════════════════════════════════════════╝

def style_ax(ax):
    ax.set_facecolor(PANEL_BG)
    ax.tick_params(colors=TICK_COL, labelsize=7)
    for spine in ax.spines.values():
        spine.set_edgecolor(SPINE_COL)
    ax.grid(True, color=SPINE_COL, linestyle="--", linewidth=0.5, alpha=0.6)

def build_layout(fig, ch_enabled):
    # Remove ONLY plot axes, preserving the checkbox_panel
    for ax in fig.axes[:]:
        if ax.get_label() != "checkbox_panel":
            fig.delaxes(ax)

    active_panels = [
        (idx, p) for idx, p in enumerate(PANELS)
        if any(ch_enabled[ch] for ch in p["channels"])
    ]

    n = len(active_panels)
    if n == 0: return {}, {}, {}, {}

    # One host axes per panel; twin axes are added manually below
    axes = fig.subplots(nrows=n, ncols=1, sharex=True, squeeze=False).flatten()
    fig.subplots_adjust(left=0.10, right=0.75, top=0.96, bottom=0.06, hspace=0.3)

    # p_axes  : orig_idx -> host ax  (used for xlim / title)
    # p_lines : orig_idx -> {ch_id: Line2D}
    # p_vtxts : orig_idx -> {ch_id: Text}
    # p_chaxes: orig_idx -> {ch_id: Axes}  (per-channel axes, for ylim)
    p_axes, p_lines, p_vtxts, p_chaxes = {}, {}, {}, {}

    for plot_idx, ((orig_idx, panel), host_ax) in enumerate(zip(active_panels, axes)):
        style_ax(host_ax)
        host_ax.set_title(panel["title"], color="white", fontsize=8, fontweight="bold", loc="left")

        if plot_idx != n - 1:
            host_ax.tick_params(labelbottom=False)
        else:
            host_ax.set_xlabel("Time (s)", color=TICK_COL, fontsize=9)

        visible_channels = [ch for ch in panel["channels"] if ch_enabled[ch]]

        # Build axis groups: channels in the same group share a y-axis.
        # axes_groups overrides multi_yaxis for fine-grained control.
        if "axes_groups" in panel:
            # Only keep groups that have at least one visible channel
            raw_groups = [[ch for ch in g if ch in visible_channels]
                          for g in panel["axes_groups"]]
            groups = [g for g in raw_groups if g]
            # Any visible channel not mentioned in axes_groups gets its own group
            grouped = {ch for g in groups for ch in g}
            for ch in visible_channels:
                if ch not in grouped:
                    groups.append([ch])
        elif panel.get("multi_yaxis", False):
            groups = [[ch] for ch in visible_channels]  # every channel its own axis
        else:
            groups = [visible_channels]  # all channels share the host axis

        ch_lines, ch_vtxts, ch_axes = {}, {}, {}
        group_ax = {}   # group index -> axes

        # left_groups: set of group indices to place on the left spine
        left_groups = set(panel.get("left_groups", []))

        left_slot  = 0   # counts extra left axes added (after the host)
        right_slot = 0   # counts extra right axes added

        for g_idx, group in enumerate(groups):
            color = COLORS[int(group[0]) % len(COLORS)]
            if g_idx == 0 and g_idx not in left_groups:
                # First group always uses host (left) axis
                ax = host_ax
            elif g_idx == 0:
                ax = host_ax
            elif g_idx in left_groups:
                # Extra left axis: twinx then flip to left spine
                ax = host_ax.twinx()
                ax.autoscale(False)
                ax.spines["right"].set_visible(False)
                ax.yaxis.set_ticks_position("left")
                ax.yaxis.set_label_position("left")
                offset = 40 * (left_slot + 1)
                ax.spines["left"].set_position(("outward", offset))
                style_ax(ax)
                ax.spines["left"].set_edgecolor(color)
                ax.tick_params(axis="y", colors=color, labelsize=7)
                left_slot += 1
            else:
                # Extra right axis
                ax = host_ax.twinx()
                ax.autoscale(False)
                offset = 40 * right_slot
                ax.spines["right"].set_position(("outward", offset))
                style_ax(ax)
                ax.spines["right"].set_edgecolor(color)
                ax.tick_params(axis="y", colors=color, labelsize=7)
                right_slot += 1
            group_ax[g_idx] = ax

        for slot, ch_id in enumerate(visible_channels):
            color = COLORS[int(ch_id) % len(COLORS)]
            unit  = f" ({CHANNELS[ch_id]['unit']})" if CHANNELS[ch_id]['unit'] else ""
            label = f"{CHANNELS[ch_id]['label']}{unit}"

            # Find which group this channel belongs to
            g_idx = next(i for i, g in enumerate(groups) if ch_id in g)
            ax = group_ax[g_idx]
            ax.tick_params(axis="y", colors=COLORS[int(groups[g_idx][0]) % len(COLORS)], labelsize=7)
            ax.yaxis.label.set_color(COLORS[int(groups[g_idx][0]) % len(COLORS)])

            line, = ax.plot([], [], color=color, lw=1.3, animated=True, label=label)
            ch_lines[ch_id] = line
            ch_axes[ch_id]  = ax

            # Live-value overlay anchored to the host axes so coords are stable
            txt = host_ax.text(
                0.99, 0.97 - slot * 0.16, "—",
                transform=host_ax.transAxes,
                ha="right", va="top", fontsize=8, color=color, fontweight="bold",
                animated=True,
                bbox=dict(boxstyle="round", facecolor=BG, edgecolor=color, alpha=0.7)
            )
            ch_vtxts[ch_id] = txt

        # Shared legend on the host axis
        if len(visible_channels) > 1:
            handles = [ch_lines[ch] for ch in visible_channels]
            labels  = [ch_lines[ch].get_label() for ch in visible_channels]
            host_ax.legend(handles, labels, fontsize=6, facecolor=BG,
                           edgecolor=SPINE_COL, loc="upper left")

        p_axes[orig_idx]   = host_ax
        p_lines[orig_idx]  = ch_lines
        p_vtxts[orig_idx]  = ch_vtxts
        p_chaxes[orig_idx] = ch_axes

    return p_axes, p_lines, p_vtxts, p_chaxes

# ╔══════════════════════════════════════════════════════════════════╗
# ║                             MAIN                                 ║
# ╚══════════════════════════════════════════════════════════════════╝

def main():
    global serial_running
    port = sys.argv[1] if len(sys.argv) > 1 else detect_port()
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_BAUDRATE

    if not port:
        print("No serial port found"); sys.exit(1)

    # Open timestamped CSV file
    global csv_file, csv_writer
    csv_filename = datetime.datetime.now().strftime("vo2max_%Y%m%d_%H%M%S.csv")
    csv_file = open(csv_filename, "w", newline="")
    csv_writer = csv.writer(csv_file)
    header = ["timestamp_s"] + [CHANNELS[cid]["label"] for cid in CHANNELS]
    csv_writer.writerow(header)
    csv_file.flush()
    print(f"[csv] Logging to {csv_filename}")

    threading.Thread(target=serial_reader, args=(port, baud), daemon=True).start()

    mplstyle.use("dark_background")
    fig = plt.figure(figsize=(16, 9), facecolor=BG)
    fig.canvas.manager.set_window_title("Serial Plotter")
    fig.canvas.manager.window.showMaximized()

    DEFAULT_ON = {"9", "11", "15"}  # VO2, VCO2, Expiratory Flow
    ch_enabled = {ch: (ch in DEFAULT_ON) for ch in CHANNELS}
    state = {
        "axes":   {},
        "lines":  {},
        "vtxts":  {},
        "chaxes": {},   # per-channel axes for ylim
        "needs_rebuild": True,
        "widget": None,
    }

    # PERSISTENT CHECKBOX AXIS
    checkbox_ax = fig.add_axes([0.78, 0.05, 0.20, 0.90], facecolor=BG, label="checkbox_panel")
    labels = [f"ID {cid} {CHANNELS[cid]['label']}" for cid in CHANNELS]

    # Store the widget in 'state' so it isn't garbage collected
    state["widget"] = CheckButtons(checkbox_ax, labels,
                                   [ch in DEFAULT_ON for ch in CHANNELS])

    checkbox_ax.set_title("Channels", color="cyan", fontsize=10, fontweight="bold")
    for spine in checkbox_ax.spines.values(): spine.set_visible(False)
    for txt in state["widget"].labels:
        txt.set_color("#c9d1d9")
        txt.set_fontsize(8)

    def on_toggle(label):
        cid = label.split()[1]
        ch_enabled[cid] = not ch_enabled[cid]
        state["needs_rebuild"] = True

    state["widget"].on_clicked(on_toggle)

    def animate(_frame):
        if state["needs_rebuild"]:
            a, l, v, ca = build_layout(fig, ch_enabled)
            state.update({"axes": a, "lines": l, "vtxts": v, "chaxes": ca, "needs_rebuild": False})
            return []

        artists = []
        with lock:
            if t0 is None: return []
            snapshot = {ch: (list(channel_data[ch]["t"]), list(channel_data[ch]["y"])) for ch in CHANNELS}

        curr_t = 0
        for p_idx, host_ax in state["axes"].items():
            # Accumulate y-values per axis object so shared axes scale correctly
            ax_yvals = {}

            for ch_id, line in state["lines"][p_idx].items():
                t_vals, y_vals = snapshot[ch_id]
                if not t_vals: continue

                line.set_data(t_vals, y_vals)
                artists.append(line)
                curr_t = max(curr_t, t_vals[-1])

                # Update live-value text overlay
                txt = state["vtxts"][p_idx][ch_id]
                unit = CHANNELS[ch_id]['unit']
                txt.set_text(f"{y_vals[-1]:.4g} {unit}")
                artists.append(txt)

                # Group y-values by the actual axis object (shared axes accumulate together)
                ch_ax = state["chaxes"][p_idx][ch_id]
                ax_yvals.setdefault(id(ch_ax), (ch_ax, []))
                ax_yvals[id(ch_ax)][1].extend(y_vals)

            # Set ylim once per unique axis; emit=False stops limits
            # propagating to other axes in the same twinx share group
            for ch_ax, y_vals_combined in ax_yvals.values():
                mi, ma = min(y_vals_combined), max(y_vals_combined)
                margin = (ma - mi) * 0.15 or 0.5
                ch_ax.set_ylim(mi - margin, ma + margin, emit=False)

            host_ax.set_xlim(0, curr_t + 1)  # Show all data since launch

        return artists

    ani = animation.FuncAnimation(fig, animate, interval=UPDATE_INTERVAL, blit=True, cache_frame_data=False)

    try:
        plt.show()
    finally:
        serial_running = False
        if csv_file:
            csv_file.close()
            print(f"[csv] Saved {csv_filename}")
        print(f"[plotter] Exiting. Errors: {parse_errors}")

if __name__ == "__main__":
    main()