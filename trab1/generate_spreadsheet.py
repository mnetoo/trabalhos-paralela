import openpyxl
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.chart import LineChart, BarChart, Reference, Series
from openpyxl.utils import get_column_letter

def build_spreadsheet():
    wb = openpyxl.Workbook()
    wb.remove(wb.active)  # remove default sheet

    # Fonts
    font_main_title = Font(name="Calibri", size=12, bold=True, color="1F497D")
    font_sub_title = Font(name="Calibri", size=10, bold=False, color="333333")
    font_header_bold = Font(name="Calibri", size=10, bold=True, color="000000")
    font_regular = Font(name="Calibri", size=10, color="000000")
    font_bold = Font(name="Calibri", size=10, bold=True, color="000000")
    font_italic = Font(name="Calibri", size=9, italic=True, color="555555")
    font_yellow_bar = Font(name="Calibri", size=11, bold=True, color="000000")

    # Fills
    fill_yellow = PatternFill(start_color="FFFF00", end_color="FFFF00", fill_type="solid")
    fill_col_header = PatternFill(start_color="E9EDF4", end_color="E9EDF4", fill_type="solid")
    fill_subtle_header = PatternFill(start_color="F2F2F2", end_color="F2F2F2", fill_type="solid")
    fill_highlight = PatternFill(start_color="FFF2CC", end_color="FFF2CC", fill_type="solid")

    # Borders
    border_cell = Border(
        left=Side(style="thin", color="D9D9D9"),
        right=Side(style="thin", color="D9D9D9"),
        top=Side(style="thin", color="D9D9D9"),
        bottom=Side(style="thin", color="D9D9D9")
    )
    border_header = Border(
        left=Side(style="thin", color="BFBFBF"),
        right=Side(style="thin", color="BFBFBF"),
        top=Side(style="thin", color="BFBFBF"),
        bottom=Side(style="medium", color="808080")
    )
    border_summary = Border(
        left=Side(style="thin", color="BFBFBF"),
        right=Side(style="thin", color="BFBFBF"),
        top=Side(style="thin", color="BFBFBF"),
        bottom=Side(style="thin", color="BFBFBF")
    )

    align_center = Alignment(horizontal="center", vertical="center")
    align_left = Alignment(horizontal="left", vertical="center")
    align_right = Alignment(horizontal="right", vertical="center")

    # Data Exp A (6 runs)
    times_expA = {
        1: [0.032267749, 0.032218977, 0.032242474, 0.032260989, 0.032292278, 0.032310836],
        2: [0.019212458, 0.019325847, 0.019276299, 0.019123083, 0.019385711, 0.019062344],
        3: [0.019986378, 0.019358638, 0.019686825, 0.019412884, 0.019356753, 0.019275464],
        4: [0.016208124, 0.016182014, 0.016114157, 0.016521499, 0.015860184, 0.016200289],
        5: [0.017755442, 0.018391661, 0.017751973, 0.018440259, 0.017572462, 0.018055285],
        6: [0.015998459, 0.017205048, 0.015936956, 0.016394474, 0.016094427, 0.016590235],
        7: [0.016698526, 0.017225442, 0.016745068, 0.016764102, 0.016337159, 0.016976966],
        8: [0.018221943, 0.015510678, 0.018309891, 0.015446252, 0.019075269, 0.015464591]
    }
    mops_expA = {
        1: [247.925566, 248.300869, 248.119915, 247.977516, 247.737244, 247.594952],
        2: [416.396489, 413.953407, 415.017427, 418.342595, 412.675087, 419.675572],
        3: [400.272624, 413.252220, 406.363146, 412.097448, 413.292464, 415.035411],
        4: [493.579641, 494.376045, 496.457854, 484.217565, 504.407776, 493.818346],
        5: [450.566098, 434.979736, 450.654144, 433.833385, 455.257771, 443.083567],
        6: [500.048158, 464.979808, 501.977917, 487.969305, 497.066484, 482.211362],
        7: [479.084211, 464.429304, 477.752617, 477.210179, 489.681216, 471.226716],
        8: [439.031120, 515.773701, 436.922312, 517.924997, 419.391196, 517.310807]
    }

    # Data Exp B (5 runs)
    times_expB = {
        1: [0.045059507, 0.044938027, 0.044761922, 0.044835491, 0.044850187],
        2: [0.026533094, 0.026362574, 0.026627346, 0.026426605, 0.026426306],
        3: [0.028144837, 0.027890521, 0.027933090, 0.028259471, 0.027964368],
        4: [0.024010886, 0.023699174, 0.023996244, 0.024113293, 0.023726011],
        5: [0.026877486, 0.026699620, 0.026689536, 0.026655214, 0.026634773],
        6: [0.024147825, 0.024711576, 0.025272006, 0.024005308, 0.023895905],
        7: [0.025705520, 0.025428657, 0.025637612, 0.025611468, 0.025604146],
        8: [0.024181130, 0.024831267, 0.024264394, 0.024100790, 0.023984823]
    }
    mops_expB = {
        1: [177.543001, 178.022948, 178.723336, 178.430073, 178.371607],
        2: [301.510256, 303.460508, 300.443009, 302.725228, 302.728652],
        3: [284.243958, 286.835805, 286.398679, 283.090937, 286.078338],
        4: [333.182212, 337.564504, 333.385515, 331.767218, 337.182684],
        5: [297.646890, 299.629732, 299.742944, 300.128902, 300.359231],
        6: [331.292777, 323.734915, 316.555801, 333.259623, 334.785391],
        7: [311.217198, 314.605677, 312.041541, 312.360076, 312.449402],
        8: [330.836481, 322.174455, 329.701203, 331.939322, 333.544257]
    }

    cols = ["B", "C", "D", "E", "F", "G", "H", "I"]

    def create_sheet(ws, title_text, subtitle_text, total_elements, chunk_size, times_data, mops_data, n_runs, is_chunked):
        ws.views.sheetView[0].showGridLines = True

        # Titles exactly matching the professor's layout (Image 1)
        ws["C1"] = title_text
        ws["C1"].font = font_main_title
        ws["C1"].alignment = align_left

        ws["C2"] = subtitle_text
        ws["C2"].font = font_sub_title
        ws["C2"].alignment = align_left

        ws["H2"] = "total elements:"
        ws["H2"].font = font_header_bold
        ws["H2"].alignment = align_right

        ws["I2"] = total_elements
        ws["I2"].font = font_header_bold
        ws["I2"].alignment = align_center

        if is_chunked:
            ws["H3"] = "chunkSize:"
            ws["H3"].font = font_italic
            ws["H3"].alignment = align_right
            ws["I3"] = chunk_size
            ws["I3"].font = font_italic
            ws["I3"].alignment = align_center

        # Thread column headers (Row 4)
        for idx, th in enumerate(range(1, 9)):
            col_l = cols[idx]
            c = ws[f"{col_l}4"]
            c.value = f"{th} threads:"
            c.font = font_header_bold
            c.alignment = align_center
            c.fill = fill_col_header
            c.border = border_header

        # Data rows (seconds)
        for r in range(n_runs):
            row_idx = 5 + r
            ws.row_dimensions[row_idx].height = 19
            for th_idx, th in enumerate(range(1, 9)):
                col_l = cols[th_idx]
                c = ws[f"{col_l}{row_idx}"]
                c.value = times_data[th][r]
                c.number_format = "0.000000"
                c.font = font_regular
                c.alignment = align_center
                c.border = border_cell

        # Row: Tempo médio (Yellow bar)
        row_yellow_tm = 5 + n_runs + 1
        ws.merge_cells(f"B{row_yellow_tm}:I{row_yellow_tm}")
        c_ytm = ws[f"B{row_yellow_tm}"]
        c_ytm.value = "Tempo médio"
        c_ytm.font = font_yellow_bar
        c_ytm.alignment = align_center
        c_ytm.fill = fill_yellow
        for col_l in cols:
            ws[f"{col_l}{row_yellow_tm}"].border = border_summary

        row_th_tm = row_yellow_tm + 1
        row_tm = row_yellow_tm + 2
        row_ac_tm = row_yellow_tm + 3

        ws[f"A{row_th_tm}"] = "threads"
        ws[f"A{row_th_tm}"].font = font_header_bold
        ws[f"A{row_th_tm}"].alignment = align_center
        ws[f"A{row_th_tm}"].border = border_summary

        ws[f"A{row_tm}"] = "tempo médio (ms)"
        ws[f"A{row_tm}"].font = font_header_bold
        ws[f"A{row_tm}"].alignment = align_left
        ws[f"A{row_tm}"].border = border_summary

        ws[f"A{row_ac_tm}"] = "Aceleração"
        ws[f"A{row_ac_tm}"].font = font_header_bold
        ws[f"A{row_ac_tm}"].alignment = align_left
        ws[f"A{row_ac_tm}"].border = border_summary

        for th_idx, th in enumerate(range(1, 9)):
            col_l = cols[th_idx]
            # Thread number
            c_th = ws[f"{col_l}{row_th_tm}"]
            c_th.value = th
            c_th.font = font_header_bold
            c_th.alignment = align_center
            c_th.border = border_summary

            # Tempo médio (ms) formula: =AVERAGE(B5:B10)*1000
            c_tm = ws[f"{col_l}{row_tm}"]
            c_tm.value = f"=AVERAGE({col_l}5:{col_l}{4+n_runs})*1000"
            c_tm.number_format = "0.00"
            c_tm.font = font_bold
            c_tm.alignment = align_center
            c_tm.border = border_summary

            # Aceleração formula: =$B$row_tm / col_l{row_tm}
            c_ac = ws[f"{col_l}{row_ac_tm}"]
            c_ac.value = f"=$B${row_tm}/{col_l}{row_tm}"
            c_ac.number_format = "0.00"
            c_ac.font = font_bold
            c_ac.alignment = align_center
            c_ac.border = border_summary

        # Row: VAZAO MEDIA: (Yellow bar) - Matching Image 3
        row_yellow_vz = row_ac_tm + 2
        ws.merge_cells(f"B{row_yellow_vz}:I{row_yellow_vz}")
        c_yvz = ws[f"B{row_yellow_vz}"]
        c_yvz.value = "VAZAO MEDIA:"
        c_yvz.font = font_yellow_bar
        c_yvz.alignment = align_center
        c_yvz.fill = fill_yellow
        for col_l in cols:
            ws[f"{col_l}{row_yellow_vz}"].border = border_summary

        row_th_vz = row_yellow_vz + 1
        row_vz = row_yellow_vz + 2
        row_ac_vz = row_yellow_vz + 3

        ws[f"A{row_th_vz}"] = "threads"
        ws[f"A{row_th_vz}"].font = font_header_bold
        ws[f"A{row_th_vz}"].alignment = align_center
        ws[f"A{row_th_vz}"].border = border_summary

        ws[f"A{row_vz}"] = "vazao média (MOPS)"
        ws[f"A{row_vz}"].font = font_header_bold
        ws[f"A{row_vz}"].alignment = align_left
        ws[f"A{row_vz}"].border = border_summary

        ws[f"A{row_ac_vz}"] = "Aceleração"
        ws[f"A{row_ac_vz}"].font = font_header_bold
        ws[f"A{row_ac_vz}"].alignment = align_left
        ws[f"A{row_ac_vz}"].border = border_summary

        # Raw MOPS Table below
        row_raw_mops_header = row_ac_vz + 3
        ws[f"A{row_raw_mops_header}"] = "Vazão por Execução (MOPS):"
        ws[f"A{row_raw_mops_header}"].font = font_header_bold

        for idx, th in enumerate(range(1, 9)):
            col_l = cols[idx]
            c = ws[f"{col_l}{row_raw_mops_header}"]
            c.value = f"{th} threads:"
            c.font = font_header_bold
            c.alignment = align_center
            c.fill = fill_subtle_header
            c.border = border_header

        mops_start = row_raw_mops_header + 1
        mops_end = row_raw_mops_header + n_runs

        for r in range(n_runs):
            row_idx = mops_start + r
            ws.row_dimensions[row_idx].height = 19
            for th_idx, th in enumerate(range(1, 9)):
                col_l = cols[th_idx]
                c = ws[f"{col_l}{row_idx}"]
                c.value = mops_data[th][r]
                c.number_format = "0.00"
                c.font = font_regular
                c.alignment = align_center
                c.border = border_cell

        # Fill vazao media values
        for th_idx, th in enumerate(range(1, 9)):
            col_l = cols[th_idx]
            # Thread
            c_th = ws[f"{col_l}{row_th_vz}"]
            c_th.value = th
            c_th.font = font_header_bold
            c_th.alignment = align_center
            c_th.border = border_summary

            # Vazão média = AVERAGE(mops_start:mops_end)
            c_vz = ws[f"{col_l}{row_vz}"]
            c_vz.value = f"=AVERAGE({col_l}{mops_start}:{col_l}{mops_end})"
            c_vz.number_format = "0.00"
            c_vz.font = font_bold
            c_vz.alignment = align_center
            c_vz.border = border_summary

            # Aceleração da vazão = col_l / $B
            c_ac = ws[f"{col_l}{row_ac_vz}"]
            c_ac.value = f"={col_l}{row_vz}/$B${row_vz}"
            c_ac.number_format = "0.00"
            c_ac.font = font_bold
            c_ac.alignment = align_center
            c_ac.border = border_summary

        # Observation note (Matching Image 3)
        ws[f"J{row_vz}"] = "OBS: Como nesse caso uma OP é a soma de prefixos de um elemento, a vazão é expressa em MOPS (Milhões de Operações por Segundo)"
        ws[f"J{row_vz}"].font = font_italic

        # Column widths
        ws.column_dimensions["A"].width = 20
        for col_l in cols:
            ws.column_dimensions[col_l].width = 15
        ws.column_dimensions["J"].width = 45

        # ----------------------------------------------------
        # CHARTS (Matching Image 2 exactly)
        # ----------------------------------------------------
        # Chart 1: Tempo médio (ms) vs Aceleração
        chart_title = "Soma de prefixos paralela com Pthreads (long long)"
        
        c1 = LineChart()
        c1.title = chart_title
        c1.style = 13
        c1.y_axis.title = "Tempo médio (ms)"
        c1.x_axis.title = "n threads"
        c1.width = 16
        c1.height = 10.5
        c1.y_axis.scaling.min = 0

        data_tm = Reference(ws, min_col=1, min_row=row_tm, max_col=9, max_row=row_tm)
        cats = Reference(ws, min_col=2, min_row=row_th_tm, max_col=9, max_row=row_th_tm)
        c1.add_data(data_tm, titles_from_data=True, from_rows=True)
        c1.set_categories(cats)

        s1 = c1.series[0]
        s1.marker.symbol = "square"
        s1.marker.size = 7
        s1.graphicalProperties.line.solidFill = "004C87"
        s1.graphicalProperties.line.width = 25000

        c1_sec = LineChart()
        data_ac1 = Reference(ws, min_col=1, min_row=row_ac_tm, max_col=9, max_row=row_ac_tm)
        c1_sec.add_data(data_ac1, titles_from_data=True, from_rows=True)
        c1_sec.y_axis.title = "Aceleração"
        c1_sec.y_axis.axId = 200
        c1_sec.y_axis.crosses = "max"
        c1_sec.y_axis.scaling.min = 0

        s1_sec = c1_sec.series[0]
        s1_sec.marker.symbol = "diamond"
        s1_sec.marker.size = 7
        s1_sec.graphicalProperties.line.solidFill = "ED7D31"
        s1_sec.graphicalProperties.line.width = 25000

        c1 += c1_sec

        # Chart 2: Vazão média (MOPS) vs Aceleração
        c2 = LineChart()
        c2.title = chart_title
        c2.style = 13
        c2.y_axis.title = "vazao média (MOPS)"
        c2.x_axis.title = "n threads"
        c2.width = 16
        c2.height = 10.5
        c2.y_axis.scaling.min = 0

        data_vz = Reference(ws, min_col=1, min_row=row_vz, max_col=9, max_row=row_vz)
        c2.add_data(data_vz, titles_from_data=True, from_rows=True)
        c2.set_categories(cats)

        s2 = c2.series[0]
        s2.marker.symbol = "square"
        s2.marker.size = 7
        s2.graphicalProperties.line.solidFill = "004C87"
        s2.graphicalProperties.line.width = 25000

        c2_sec = LineChart()
        data_ac2 = Reference(ws, min_col=1, min_row=row_ac_vz, max_col=9, max_row=row_ac_vz)
        c2_sec.add_data(data_ac2, titles_from_data=True, from_rows=True)
        c2_sec.y_axis.title = "Aceleração"
        c2_sec.y_axis.axId = 300
        c2_sec.y_axis.crosses = "max"
        c2_sec.y_axis.scaling.min = 0

        s2_sec = c2_sec.series[0]
        s2_sec.marker.symbol = "diamond"
        s2_sec.marker.size = 7
        s2_sec.graphicalProperties.line.solidFill = "ED7D31"
        s2_sec.graphicalProperties.line.width = 25000

        c2 += c2_sec

        # Positioning side by side below data
        chart_row = mops_end + 3
        ws.add_chart(c1, f"B{chart_row}")
        ws.add_chart(c2, f"K{chart_row}")

        return row_tm, row_vz, row_ac_tm

    # 1. Sheet: Exp A
    ws_a = wb.create_sheet(title="Exp A (Com Chunks L2)")
    row_tm_a, row_vz_a, row_ac_a = create_sheet(
        ws_a,
        title_text="ParallelPrefixSum rodando no wz10 - Com Chunks (1/4 L2 = 196608 elementos)",
        subtitle_text="Executando 6 vezes com 8000000 elementos long long (8 bytes)",
        total_elements=8000000,
        chunk_size=196608,
        times_data=times_expA,
        mops_data=mops_expA,
        n_runs=6,
        is_chunked=True
    )

    # 2. Sheet: Exp B
    ws_b = wb.create_sheet(title="Exp B (Sem Chunks)")
    row_tm_b, row_vz_b, row_ac_b = create_sheet(
        ws_b,
        title_text="ParallelPrefixSum rodando no wz10 - SEM Chunks (particionamento direto)",
        subtitle_text="Executando 5 vezes com 8000000 elementos long long (8 bytes)",
        total_elements=8000000,
        chunk_size=0,
        times_data=times_expB,
        mops_data=mops_expB,
        n_runs=5,
        is_chunked=False
    )

    # 3. Sheet: Comparativo A vs B
    ws_comp = wb.create_sheet(title="Comparativo A vs B")
    ws_comp.views.sheetView[0].showGridLines = True

    ws_comp["A1"] = "Comparativo de Desempenho: Exp A (Com Chunks L2) vs Exp B (Sem Chunks)"
    ws_comp["A1"].font = font_main_title
    ws_comp["A2"] = "Ambiente de teste: Cluster host wz10 (8 CPUs alocadas pelo SLURM, 8.000.000 elementos long long)"
    ws_comp["A2"].font = font_sub_title

    headers_comp = [
        "Threads",
        "Tempo Exp A (ms)",
        "Tempo Exp B (ms)",
        "Vazão Exp A (MOPS)",
        "Vazão Exp B (MOPS)",
        "Ganho Chunks L2 (%)",
        "Speedup Exp A",
        "Speedup Exp B",
        "Speedup Linear Ideal"
    ]

    for col_idx, h_text in enumerate(headers_comp, start=1):
        c = ws_comp.cell(row=4, column=col_idx, value=h_text)
        c.font = font_header_bold
        c.alignment = align_center
        c.fill = fill_col_header
        c.border = border_header

    for idx, th in enumerate(range(1, 9)):
        row_num = 5 + idx
        col_l = cols[idx]

        ws_comp.cell(row=row_num, column=1, value=th).alignment = align_center

        # Tempo A
        c_ta = ws_comp.cell(row=row_num, column=2, value=f"='Exp A (Com Chunks L2)'!{col_l}{row_tm_a}")
        c_ta.number_format = "0.00"
        c_ta.alignment = align_center

        # Tempo B
        c_tb = ws_comp.cell(row=row_num, column=3, value=f"='Exp B (Sem Chunks)'!{col_l}{row_tm_b}")
        c_tb.number_format = "0.00"
        c_tb.alignment = align_center

        # Vazao A
        c_va = ws_comp.cell(row=row_num, column=4, value=f"='Exp A (Com Chunks L2)'!{col_l}{row_vz_a}")
        c_va.number_format = "0.00"
        c_va.alignment = align_center

        # Vazao B
        c_vb = ws_comp.cell(row=row_num, column=5, value=f"='Exp B (Sem Chunks)'!{col_l}{row_vz_b}")
        c_vb.number_format = "0.00"
        c_vb.alignment = align_center

        # Ganho Chunks (%) = (Vazao A / Vazao B - 1) * 100
        c_gain = ws_comp.cell(row=row_num, column=6, value=f"=(D{row_num}/E{row_num}-1)*100")
        c_gain.number_format = "0.00\"%\""
        c_gain.alignment = align_center
        c_gain.fill = fill_highlight

        # Speedup A = Vazao A / Vazao A (1T)
        c_spa = ws_comp.cell(row=row_num, column=7, value=f"=D{row_num}/$D$5")
        c_spa.number_format = "0.00"
        c_spa.alignment = align_center

        # Speedup B = Vazao B / Vazao B (1T)
        c_spb = ws_comp.cell(row=row_num, column=8, value=f"=E{row_num}/$E$5")
        c_spb.number_format = "0.00"
        c_spb.alignment = align_center

        # Speedup Ideal = th
        c_ideal = ws_comp.cell(row=row_num, column=9, value=th)
        c_ideal.number_format = "0.00"
        c_ideal.alignment = align_center

        for c_i in range(1, 10):
            ws_comp.cell(row=row_num, column=c_i).border = border_cell
            ws_comp.cell(row=row_num, column=c_i).font = font_regular

    for col_idx in range(1, 10):
        col_name = get_column_letter(col_idx)
        ws_comp.column_dimensions[col_name].width = 19

    # Charts Comparativo
    # Bar Chart: Vazão Exp A vs Exp B
    comp_c1 = BarChart()
    comp_c1.type = "col"
    comp_c1.style = 10
    comp_c1.title = "Comparativo de Vazão Média: Exp A (Com Chunks L2) vs Exp B (Sem Chunks)"
    comp_c1.y_axis.title = "Vazão média (MOPS)"
    comp_c1.x_axis.title = "Número de Threads"
    comp_c1.width = 16
    comp_c1.height = 10.5

    data_vazao = Reference(ws_comp, min_col=4, min_row=4, max_col=5, max_row=12)
    cats_comp = Reference(ws_comp, min_col=1, min_row=5, max_row=12)
    comp_c1.add_data(data_vazao, titles_from_data=True)
    comp_c1.set_categories(cats_comp)
    comp_c1.series[0].graphicalProperties.solidFill = "004C87"
    comp_c1.series[1].graphicalProperties.solidFill = "ED7D31"

    # Line Chart: Speedup Exp A vs Exp B vs Ideal
    comp_c2 = LineChart()
    comp_c2.title = "Comparativo de Aceleração (Speedup) vs Speedup Linear Ideal"
    comp_c2.style = 13
    comp_c2.y_axis.title = "Speedup (Tp / T1)"
    comp_c2.x_axis.title = "Número de Threads"
    comp_c2.width = 16
    comp_c2.height = 10.5
    comp_c2.y_axis.scaling.min = 0

    data_sp = Reference(ws_comp, min_col=7, min_row=4, max_col=9, max_row=12)
    comp_c2.add_data(data_sp, titles_from_data=True)
    comp_c2.set_categories(cats_comp)

    # Series 1: Speedup Exp A
    comp_c2.series[0].marker.symbol = "square"
    comp_c2.series[0].marker.size = 7
    comp_c2.series[0].graphicalProperties.line.solidFill = "004C87"
    comp_c2.series[0].graphicalProperties.line.width = 25000

    # Series 2: Speedup Exp B
    comp_c2.series[1].marker.symbol = "triangle"
    comp_c2.series[1].marker.size = 7
    comp_c2.series[1].graphicalProperties.line.solidFill = "ED7D31"
    comp_c2.series[1].graphicalProperties.line.width = 25000

    # Series 3: Speedup Ideal
    comp_c2.series[2].marker.symbol = "none"
    comp_c2.series[2].graphicalProperties.line.dashStyle = "dash"
    comp_c2.series[2].graphicalProperties.line.solidFill = "70AD47"
    comp_c2.series[2].graphicalProperties.line.width = 15000

    ws_comp.add_chart(comp_c1, "B15")
    ws_comp.add_chart(comp_c2, "K15")

    # 4. Sheet: Hardware e SLURM
    ws_hw = wb.create_sheet(title="Hardware e SLURM")
    ws_hw.views.sheetView[0].showGridLines = True
    ws_hw.column_dimensions["A"].width = 30
    ws_hw.column_dimensions["B"].width = 55

    ws_hw["A1"] = "Informações do Ambiente de Execução (Cluster SLURM)"
    ws_hw["A1"].font = font_main_title

    hw_info = [
        ("Nó do Cluster (Hostname):", "wz10"),
        ("Partição SLURM:", "debug"),
        ("CPUs alocadas por nó:", "8"),
        ("Job ID Exp A (Com Chunks):", "18977 (t1-expA)"),
        ("Job ID Exp B (Sem Chunks):", "18980 (t1-expB)"),
        ("Tipo de Dado:", "long long (8 bytes)"),
        ("Tamanho do Vetor (N):", "8.000.000 elementos (~61 MB)"),
        ("Tamanho da Cache L2:", "6.291.456 bytes (6 MB / 6144 KB)"),
        ("Tamanho da Cache L3:", "0 bytes (Não detectada / compartilhada)"),
        ("Chunk Size Exp A:", "196.608 elementos (1/4 da L2 = 1.572.864 bytes)"),
        ("Chunk Size Exp B:", "0 (Sem chunks - partição global direta)"),
        ("Repetições por thread (Exp A):", "6 execuções válidas"),
        ("Repetições por thread (Exp B):", "5 execuções válidas"),
        ("Script de Execução Exp A:", "roda-exp-A-chunk-196608-slurm.sh"),
        ("Script de Execução Exp B:", "roda-exp-B-chunk-0-slurm.sh"),
        ("Binário Utilizado:", "./prefixSumPth-v3-longlong")
    ]

    for idx, (label, val) in enumerate(hw_info, start=3):
        cell_lbl = ws_hw.cell(row=idx, column=1, value=label)
        cell_lbl.font = font_header_bold
        cell_lbl.fill = fill_subtle_header
        cell_lbl.border = border_cell

        cell_val = ws_hw.cell(row=idx, column=2, value=val)
        cell_val.font = font_regular
        cell_val.border = border_cell

    output_file = "Planilha-PrefixSum-Pthreads-WZ-ExpA-ExpB.xlsx"
    wb.save(output_file)
    print(f"Spreadsheet generated successfully: {output_file}")

if __name__ == "__main__":
    build_spreadsheet()
