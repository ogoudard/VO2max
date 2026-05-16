"""
VO2max Report Generator
Generates a PDF report similar to Medi soft Exp'air format
Usage: python generate_vo2max_report.py <csv_file> [output_pdf]
"""

import sys
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.patches import FancyBboxPatch
import io
import os
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import cm, mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor

# ── Patient info (à adapter ou passer en paramètre) ──────────────────────────
PATIENT = {
    "nom": "Oliv",
    "age": 48,
    "taille_cm": 183,
    "poids_kg": 76,
    "genre": "M",
    "date": "10/05/2026",
}

# ── Colors ────────────────────────────────────────────────────────────────────
C_VO2   = "#E05C3A"   # red-orange
C_VCO2  = "#4A90D9"   # blue
C_VE    = "#2ECC71"   # green
C_FC    = "#E74C3C"   # red
C_RR    = "#E05C3A"   # orange
C_VT    = "#4A90D9"   # blue
C_GRID  = "#DDDDDD"
C_BG    = "#F8F8F8"

def find_column(df, candidates):
    """Find first matching column name (case-insensitive)."""
    cols_lower = {c.lower().strip(): c for c in df.columns}
    for cand in candidates:
        if cand.lower() in cols_lower:
            return cols_lower[cand.lower()]
    return None

def load_and_resample(csv_path):
    df = pd.read_csv(csv_path)

    # Print columns for debug
    print(f"Colonnes trouvees: {df.columns.tolist()}")

    # Auto-detect time column
    time_col = find_column(df, ['timestamp', 'time', 'temps', 'Time', 'Timestamp', 'T', 't'])
    if time_col is None:
        # Use first column as time
        time_col = df.columns[0]
        print(f"Colonne temps non trouvee, utilisation de: '{time_col}'")
    else:
        print(f"Colonne temps: '{time_col}'")
    df['timestamp'] = pd.to_numeric(df[time_col], errors='coerce')

    # Auto-detect VO2 column
    vo2_col = find_column(df, ['VO2', 'vo2', 'Vo2'])
    if vo2_col is None:
        raise ValueError(f"Colonne VO2 introuvable. Colonnes disponibles: {df.columns.tolist()}")
    if vo2_col != 'VO2':
        df['VO2'] = df[vo2_col]

    # Auto-detect other columns with fallback
    col_map = {
        'VO2max':               ['VO2max', 'vo2max', 'VO2 max'],
        'VCO2':                 ['VCO2', 'vco2', 'Vco2'],
        'Respiratory Rate':     ['Respiratory Rate', 'RR', 'FR', 'respiratory_rate', 'resp_rate'],
        'Flow':                 ['Flow', 'flow', 'VE', 've', 'V.E'],
        'Expiratory Flow':      ['Expiratory Flow', 'expiratory_flow', 'ExpFlow'],
        'Respiratory Quotient': ['Respiratory Quotient', 'RQ', 'rq', 'QR'],
        'O2':                   ['O2', 'o2', 'FiO2'],
        'CO2':                  ['CO2', 'co2', 'FeCO2'],
    }
    for target, candidates in col_map.items():
        if target not in df.columns:
            found = find_column(df, candidates)
            if found:
                df[target] = df[found]
                print(f"  '{found}' -> '{target}'")
            else:
                df[target] = np.nan

    df = df[df['VO2'].notna()].copy()
    df['time_s'] = df['timestamp'].round(0).astype(int)
    d = df.groupby('time_s').mean(numeric_only=True).reset_index()
    d['time_min'] = d['time_s'] / 60.0
    # Rolling smooth (5s window)
    for col in ['VO2','VO2max','VCO2','Respiratory Rate','Flow','Expiratory Flow','Respiratory Quotient','O2','CO2']:
        if col in d.columns:
            d[col] = d[col].rolling(5, min_periods=1, center=True).mean()
    return d

def detect_thresholds(d):
    """Detect SV1 and SV2 from VE/VCO2 inflection (simplified heuristic)."""
    rq = d['Respiratory Quotient'].dropna()
    t = d['time_min']
    # SV1: first time RQ crosses 0.85
    sv1_idx = (rq > 0.85).idxmax() if (rq > 0.85).any() else len(d)//3
    sv1_t = t.iloc[sv1_idx] if sv1_idx < len(t) else t.iloc[len(t)//3]
    # SV2: first time RQ crosses 1.0
    sv2_idx = (rq > 1.0).idxmax() if (rq > 1.0).any() else 2*len(d)//3
    sv2_t = t.iloc[sv2_idx] if sv2_idx < len(t) else t.iloc[2*len(t)//3]
    return sv1_t, sv2_t

def make_chart_reponse_globale(d, sv1, sv2, figsize=(7, 3.5)):
    fig, ax1 = plt.subplots(figsize=figsize, facecolor='white')
    ax2 = ax1.twinx()

    t = d['time_min']
    ax1.plot(t, d['VO2'],   color=C_VO2,  lw=1.5, label='VO2 [L/min]')
    ax1.plot(t, d['VCO2'],  color=C_VCO2, lw=1.5, label='VCO2 [L/min]')
    ax2.plot(t, d['Flow'],  color=C_VE,   lw=1.2, label='V.E [L/min]', alpha=0.8)

    ax1.set_xlabel('Temps [min]', fontsize=8)
    ax1.set_ylabel('VO2 / VCO2 [L/min]', fontsize=8, color='black')
    ax2.set_ylabel('V.E [L/min]', fontsize=8, color=C_VE)

    ymax = max(d['VO2'].max(), d['VCO2'].max()) * 1.15
    ax1.set_ylim(0, ymax)
    ax2.set_ylim(0, d['Flow'].max() * 1.2)

    for sv, label in [(sv1, 'SV1'), (sv2, 'SV2')]:
        ax1.axvline(sv, color='gray', lw=1, ls='--')
        ax1.text(sv, ymax * 0.97, label, ha='center', fontsize=7, color='gray')

    ax1.set_facecolor(C_BG)
    ax1.grid(color=C_GRID, lw=0.5)
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1+lines2, labels1+labels2, fontsize=6, loc='upper left')
    ax1.set_title('REPONSE GLOBALE', fontsize=9, fontweight='bold', loc='left')
    fig.tight_layout()
    return fig

def make_chart_ve_vco2(d, sv1, sv2, figsize=(3.5, 3.5)):
    fig, ax = plt.subplots(figsize=figsize, facecolor='white')
    ax.plot(d['VCO2'], d['Flow'], color=C_VCO2, lw=1.5)
    ax.set_xlabel('VCO2 [L/min]', fontsize=8)
    ax.set_ylabel('V.E [L/min]', fontsize=8)
    ax.set_facecolor(C_BG)
    ax.grid(color=C_GRID, lw=0.5)
    ax.set_title('VE/VCO2', fontsize=9, fontweight='bold', loc='left')

    # Mark SV1 and SV2 on the curve by time
    for sv, label in [(sv1, 'SV1'), (sv2, 'SV2')]:
        idx = (d['time_min'] - sv).abs().idxmin()
        x = d['VCO2'].iloc[idx]
        y = d['Flow'].iloc[idx]
        ax.axvline(x, color='gray', lw=1, ls='--')
        ax.text(x, y * 1.05, label, fontsize=7, color='gray', ha='center')

    fig.tight_layout()
    return fig

def make_chart_ventilatoire(d, sv1, sv2, figsize=(7, 3.5)):
    fig, ax1 = plt.subplots(figsize=figsize, facecolor='white')
    ax2 = ax1.twinx()

    t = d['time_min']
    ax1.plot(t, d['Respiratory Rate'], color=C_RR, lw=1.5, label='F.R [#/min]')
    if 'Expiratory Flow' in d.columns:
        ax1.plot(t, d['Expiratory Flow'], color=C_VT, lw=1.5, label='Vt [L]', alpha=0.8)
    ax2.plot(t, d['Flow'], color=C_VE, lw=1.2, label='V.E [L/min]', alpha=0.7)

    ax1.set_xlabel('Temps [min]', fontsize=8)
    ax1.set_ylabel('F.R / Vt', fontsize=8)
    ax2.set_ylabel('V.E [L/min]', fontsize=8, color=C_VE)
    ax1.set_facecolor(C_BG)
    ax1.grid(color=C_GRID, lw=0.5)

    ymax1 = d['Respiratory Rate'].max() * 1.2
    ax1.set_ylim(0, ymax1)

    for sv, label in [(sv1, 'SV1'), (sv2, 'SV2')]:
        ax1.axvline(sv, color='gray', lw=1, ls='--')
        ax1.text(sv, ymax1 * 0.97, label, ha='center', fontsize=7, color='gray')

    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1+lines2, labels1+labels2, fontsize=6, loc='upper left')
    ax1.set_title('REPONSE VENTILATOIRE', fontsize=9, fontweight='bold', loc='left')
    fig.tight_layout()
    return fig

def make_chart_cardio(d, sv1, sv2, figsize=(7, 3.5)):
    fig, ax1 = plt.subplots(figsize=figsize, facecolor='white')
    ax2 = ax1.twinx()

    t = d['time_min']
    # VO2/FC proxy : VO2 * 1000 (mL) — on n'a pas FC donc on affiche VO2 et VCO2
    ax1.plot(t, d['VO2'] * 1000, color=C_VO2, lw=1.5, label='VO2 [mL/min]')
    ax2.plot(t, d['VCO2'],       color=C_VCO2, lw=1.2, label='VCO2 [L/min]', alpha=0.8)

    ax1.set_xlabel('Temps [min]', fontsize=8)
    ax1.set_ylabel('VO2 [mL/min]', fontsize=8, color=C_VO2)
    ax2.set_ylabel('VCO2 [L/min]', fontsize=8, color=C_VCO2)
    ax1.set_facecolor(C_BG)
    ax1.grid(color=C_GRID, lw=0.5)

    ymax1 = d['VO2'].max() * 1000 * 1.2
    ax1.set_ylim(0, ymax1)

    for sv, label in [(sv1, 'SV1'), (sv2, 'SV2')]:
        ax1.axvline(sv, color='gray', lw=1, ls='--')
        ax1.text(sv, ymax1 * 0.97, label, ha='center', fontsize=7, color='gray')

    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1+lines2, labels1+labels2, fontsize=6, loc='upper left')
    ax1.set_title('REPONSE CARDIO-CIRCULATOIRE', fontsize=9, fontweight='bold', loc='left')
    fig.tight_layout()
    return fig

def fig_to_bytes(fig):
    buf = io.BytesIO()
    fig.savefig(buf, format='png', dpi=150, bbox_inches='tight')
    plt.close(fig)
    buf.seek(0)
    return buf

def build_pdf(csv_path, output_path):
    print(f"Loading data from {csv_path}...")
    d = load_and_resample(csv_path)
    sv1, sv2 = detect_thresholds(d)

    # Key stats
    vo2max_val   = d['VO2max'].max()
    vo2max_ml    = vo2max_val * 1000 / PATIENT['poids_kg']  # mL/min/kg
    vco2_max     = d['VCO2'].max()
    rr_max       = d['Respiratory Rate'].max()
    ve_max       = d['Flow'].max()
    rq_max       = d['Respiratory Quotient'].dropna().max()
    duration_min = d['time_min'].max()
    imc          = PATIENT['poids_kg'] / (PATIENT['taille_cm']/100)**2

    print(f"VO2max = {vo2max_val:.2f} L/min  ({vo2max_ml:.1f} mL/min/kg)")
    print(f"Generating charts...")

    fig_glob  = make_chart_reponse_globale(d, sv1, sv2, figsize=(8, 3.8))
    fig_vevco = make_chart_ve_vco2(d, sv1, sv2, figsize=(3.8, 3.8))
    fig_vent  = make_chart_ventilatoire(d, sv1, sv2, figsize=(8, 3.8))
    fig_card  = make_chart_cardio(d, sv1, sv2, figsize=(8, 3.8))

    img_glob  = fig_to_bytes(fig_glob)
    img_vevco = fig_to_bytes(fig_vevco)
    img_vent  = fig_to_bytes(fig_vent)
    img_card  = fig_to_bytes(fig_card)

    print(f"Building PDF: {output_path}...")

    PAGE_W, PAGE_H = A4
    c_pdf = canvas.Canvas(output_path, pagesize=A4)
    W, H = PAGE_W, PAGE_H

    # ── Header bar ────────────────────────────────────────────────────────────
    c_pdf.setFillColor(HexColor("#1A3A5C"))
    c_pdf.rect(0, H - 2.2*cm, W, 2.2*cm, fill=1, stroke=0)
    c_pdf.setFillColor(colors.white)
    c_pdf.setFont("Helvetica-Bold", 14)
    c_pdf.drawString(1*cm, H - 1.3*cm, "RAPPORT TEST D'EFFORT - VO2max")
    c_pdf.setFont("Helvetica", 9)
    c_pdf.drawRightString(W - 1*cm, H - 1.3*cm, f"Date : {PATIENT['date']}")

    # ── Patient info box ──────────────────────────────────────────────────────
    y_info = H - 4.2*cm
    c_pdf.setFillColor(HexColor("#EEF4FB"))
    c_pdf.rect(0.8*cm, y_info, W - 1.6*cm, 1.6*cm, fill=1, stroke=0)
    c_pdf.setFillColor(HexColor("#1A3A5C"))
    c_pdf.setFont("Helvetica-Bold", 9)
    c_pdf.drawString(1.2*cm, y_info + 1.0*cm, f"Patient : {PATIENT['nom']}")
    c_pdf.setFont("Helvetica", 8)
    c_pdf.drawString(1.2*cm, y_info + 0.4*cm,
        f"Age : {PATIENT['age']} ans    Taille : {PATIENT['taille_cm']} cm    "
        f"Poids : {PATIENT['poids_kg']} kg    IMC : {imc:.1f}    Genre : {PATIENT['genre']}")

    c_pdf.setFillColor(HexColor("#1A3A5C"))
    c_pdf.setFont("Helvetica-Bold", 9)
    c_pdf.drawRightString(W - 1.2*cm, y_info + 1.0*cm, f"VO2max : {vo2max_val:.2f} L/min")
    c_pdf.setFont("Helvetica", 8)
    c_pdf.drawRightString(W - 1.2*cm, y_info + 0.4*cm, f"({vo2max_ml:.1f} mL/min/kg)    Duree : {duration_min:.1f} min")

    # ── Row 1: Reponse Globale + VE/VCO2 ─────────────────────────────────────
    y_row1 = H - 9.2*cm
    img_w_large = 12.5*cm
    img_h_row   = 4.8*cm
    img_w_small = 5.2*cm

    from reportlab.lib.utils import ImageReader
    c_pdf.drawImage(ImageReader(img_glob),  0.8*cm, y_row1, width=img_w_large, height=img_h_row, preserveAspectRatio=True)
    c_pdf.drawImage(ImageReader(img_vevco), 13.8*cm, y_row1, width=img_w_small, height=img_h_row, preserveAspectRatio=True)

    # ── Row 2: Reponse Ventilatoire ───────────────────────────────────────────
    y_row2 = H - 14.4*cm
    c_pdf.drawImage(ImageReader(img_vent), 0.8*cm, y_row2, width=img_w_large, height=img_h_row, preserveAspectRatio=True)

    # ── Comments box (right of row2) ──────────────────────────────────────────
    cx = 13.8*cm; cy = y_row2; cw = 5.2*cm; ch = img_h_row
    c_pdf.setFillColor(HexColor("#FFF8E7"))
    c_pdf.rect(cx, cy, cw, ch, fill=1, stroke=1)
    c_pdf.setFillColor(HexColor("#C0392B"))
    c_pdf.setFont("Helvetica-Bold", 9)
    c_pdf.drawString(cx + 0.2*cm, cy + ch - 0.5*cm, "Commentaires")
    c_pdf.setFillColor(colors.black)
    c_pdf.setFont("Helvetica", 8)
    comments = [
        f"VO2max : {vo2max_val:.2f} L/min",
        f"           {vo2max_ml:.1f} mL/min/kg",
        f"VCO2max : {vco2_max:.2f} L/min",
        f"QR max : {rq_max:.2f}",
        f"FR max : {rr_max:.0f} /min",
        f"VE max : {ve_max:.1f} L/min",
        f"SV1 : {sv1:.1f} min",
        f"SV2 : {sv2:.1f} min",
    ]
    for i, line in enumerate(comments):
        c_pdf.drawString(cx + 0.3*cm, cy + ch - 1.0*cm - i*0.45*cm, line)

    # ── Row 3: Reponse Cardio ─────────────────────────────────────────────────
    y_row3 = H - 19.6*cm
    c_pdf.drawImage(ImageReader(img_card), 0.8*cm, y_row3, width=img_w_large, height=img_h_row, preserveAspectRatio=True)

    # ── Summary table ─────────────────────────────────────────────────────────
    y_tab = y_row3 - 2.8*cm
    c_pdf.setFillColor(HexColor("#1A3A5C"))
    c_pdf.rect(0.8*cm, y_tab + 1.8*cm, W - 1.6*cm, 0.55*cm, fill=1, stroke=0)
    c_pdf.setFillColor(colors.white)
    c_pdf.setFont("Helvetica-Bold", 8)
    headers = ["Parametre", "Valeur", "Unite"]
    col_x = [1.0*cm, 8.0*cm, 13.0*cm]
    for hdr, x in zip(headers, col_x):
        c_pdf.drawString(x, y_tab + 1.95*cm, hdr)

    rows_data = [
        ("VO2max",            f"{vo2max_val:.3f}",   "L/min"),
        ("VO2max/kg",         f"{vo2max_ml:.2f}",    "mL/min/kg"),
        ("VCO2max",           f"{vco2_max:.3f}",     "L/min"),
        ("Quotient Resp. max",f"{rq_max:.3f}",       ""),
        ("FR max",            f"{rr_max:.1f}",       "#/min"),
        ("VE max",            f"{ve_max:.1f}",       "L/min"),
        ("Duree totale",      f"{duration_min:.1f}", "min"),
    ]
    c_pdf.setFillColor(colors.black)
    c_pdf.setFont("Helvetica", 8)
    for i, (param, val, unit) in enumerate(rows_data):
        row_y = y_tab + 1.3*cm - i*0.38*cm
        if i % 2 == 0:
            c_pdf.setFillColor(HexColor("#F0F4F8"))
            c_pdf.rect(0.8*cm, row_y - 0.05*cm, W - 1.6*cm, 0.38*cm, fill=1, stroke=0)
        c_pdf.setFillColor(colors.black)
        c_pdf.drawString(col_x[0], row_y + 0.08*cm, param)
        c_pdf.drawString(col_x[1], row_y + 0.08*cm, val)
        c_pdf.drawString(col_x[2], row_y + 0.08*cm, unit)

    # ── Footer ────────────────────────────────────────────────────────────────
    c_pdf.setFillColor(HexColor("#888888"))
    c_pdf.setFont("Helvetica", 7)
    c_pdf.drawRightString(W - 1*cm, 0.7*cm, "Generated by VO2max Report Generator  |  Medi soft Exp'air compatible")
    c_pdf.line(0.8*cm, 1.0*cm, W - 0.8*cm, 1.0*cm)

    c_pdf.save()
    print(f"PDF saved: {output_path}")


if __name__ == "__main__":
    csv_file = sys.argv[1] if len(sys.argv) > 1 else "/mnt/user-data/uploads/1778905906522_serial_log_20260515_174950_Arthur.csv"
    out_file = sys.argv[2] if len(sys.argv) > 2 else "/mnt/user-data/outputs/rapport_vo2max_Arthur.pdf"
    os.makedirs(os.path.dirname(out_file), exist_ok=True)
    build_pdf(csv_file, out_file)
