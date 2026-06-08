#!/usr/bin/env python3
import re
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Preformatted,
                                Table, TableStyle, HRFlowable, Image)
from reportlab.lib.utils import ImageReader

SRC = "phari012_custom_lab_report.md"
OUT = "phari012_custom_lab_report.pdf"

styles = getSampleStyleSheet()
title_style = ParagraphStyle("T", parent=styles["Title"], fontSize=18, spaceAfter=10)
h1 = ParagraphStyle("H1", parent=styles["Heading1"], fontSize=14, spaceBefore=12, spaceAfter=6,
                    textColor=colors.HexColor("#1a3c6e"))
h2 = ParagraphStyle("H2", parent=styles["Heading2"], fontSize=11.5, spaceBefore=8, spaceAfter=4,
                    textColor=colors.HexColor("#244"))
body = ParagraphStyle("B", parent=styles["Normal"], fontSize=10, leading=13, spaceAfter=5)
bullet = ParagraphStyle("Bu", parent=body, leftIndent=14, bulletIndent=2, spaceAfter=3)
code = ParagraphStyle("C", parent=styles["Code"], fontName="Courier", fontSize=7.6, leading=8.8,
                      backColor=colors.HexColor("#f4f4f4"), borderPadding=4)

def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def inline(t):
    t = esc(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"`(.+?)`", r'<font face="Courier">\1</font>', t)
    return t

lines = open(SRC).read().split("\n")
story = []
i = 0
para_buf = []

def flush_para():
    global para_buf
    if para_buf:
        story.append(Paragraph(inline(" ".join(para_buf)), body))
        para_buf = []

while i < len(lines):
    line = lines[i]
    s = line.strip()

    # image:  ![alt](path)
    mimg = re.match(r"!\[.*?\]\((.+?)\)", s)
    if mimg:
        flush_para()
        path = mimg.group(1)
        iw, ih = ImageReader(path).getSize()
        w = min(5.2 * inch, iw)
        h = w * ih / iw
        if h > 5.5 * inch:
            h = 5.5 * inch
            w = h * iw / ih
        img = Image(path, width=w, height=h)
        img.hAlign = "CENTER"
        story.append(img)
        story.append(Spacer(1, 8))
        i += 1
        continue

    # code fence
    if s.startswith("```"):
        flush_para()
        block = []
        i += 1
        while i < len(lines) and not lines[i].strip().startswith("```"):
            block.append(lines[i])
            i += 1
        i += 1
        story.append(Preformatted("\n".join(block), code))
        story.append(Spacer(1, 4))
        continue

    # table
    if s.startswith("|"):
        flush_para()
        rows = []
        while i < len(lines) and lines[i].strip().startswith("|"):
            cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
            if not re.match(r"^[-: ]+$", "".join(cells)):
                rows.append([Paragraph(inline(c), body) for c in cells])
            i += 1
        t = Table(rows, hAlign="LEFT")
        t.setStyle(TableStyle([
            ("GRID", (0,0), (-1,-1), 0.5, colors.grey),
            ("BACKGROUND", (0,0), (-1,0), colors.HexColor("#e8eef6")),
            ("VALIGN", (0,0), (-1,-1), "TOP"),
            ("LEFTPADDING", (0,0), (-1,-1), 5),
            ("RIGHTPADDING", (0,0), (-1,-1), 5),
            ("TOPPADDING", (0,0), (-1,-1), 3),
            ("BOTTOMPADDING", (0,0), (-1,-1), 3),
        ]))
        story.append(t)
        story.append(Spacer(1, 6))
        continue

    if s == "---":
        flush_para()
        story.append(Spacer(1, 4))
        story.append(HRFlowable(width="100%", thickness=0.6, color=colors.grey))
        story.append(Spacer(1, 4))
        i += 1
        continue

    if s.startswith("# "):
        flush_para(); story.append(Paragraph(inline(s[2:]), title_style)); i += 1; continue
    if s.startswith("## "):
        flush_para(); story.append(Paragraph(inline(s[3:]), h1)); i += 1; continue
    if s.startswith("### "):
        flush_para(); story.append(Paragraph(inline(s[4:]), h2)); i += 1; continue

    if s.startswith("- "):
        flush_para()
        story.append(Paragraph(inline(s[2:]), bullet, bulletText="•"))
        i += 1
        continue

    if s == "":
        flush_para()
        i += 1
        continue

    para_buf.append(s)
    i += 1

flush_para()

doc = SimpleDocTemplate(OUT, pagesize=letter,
                        leftMargin=0.7*inch, rightMargin=0.7*inch,
                        topMargin=0.7*inch, bottomMargin=0.7*inch)
doc.build(story)
print("wrote", OUT)
