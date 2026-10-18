# Sơ đồ thuật toán Bed Tramming (wizard trên màn CYD)

Nguồn: `Marlin/src/feature/tbt_run.cpp`, `tbt_report.cpp`, `tbt_protocol.h`,
`cydlcd/src/ui/pages/ui_page_tbt.cpp`.

## 1. Luồng chính

```mermaid
flowchart TD
    A(["HOME + PROBE<br/>TBT_CMD_RESTART"]) --> B["G28 - home toàn bộ trục<br/>set_all_unhomed"]
    B --> C{"all_axes_homed ?"}
    C -- chưa --> C
    C -- xong --> D["ĐO 4 GÓC — TR_PROBE<br/>probe_xy từng góc một"]

    D --> D1["probe_xy cho 1 góc:<br/>1. Z về 5mm<br/>2. TẮT bed mesh<br/>3. probe_at_point<br/>4. BẬT lại bed mesh<br/>5. in z_measured"]
    D1 --> E{"step_index >= 4 ?"}
    E -- chưa --> D
    E -- xong --> F["set_ref<br/>chốt z_ref_min = góc thấp nhất<br/>z_ref_max = góc cao nhất"]

    F --> G{"any_need_adjust 0.05<br/>có góc lệch > 0.05mm ?"}
    G -- không --> H["ĐẠT - all_dat = true<br/>màn hình hiện DAT<br/>không di chuyển, không bắt vặn"]
    G -- có --> I["TR_GOTO - goto_worst<br/>chọn góc có |z_measured − ref| lớn nhất<br/>di XY tới góc đó"]

    I --> J["TR_IDLE - chờ người dùng<br/>worst_index = góc đang đứng"]
    J --> K{"Người dùng bấm gì ?"}

    K -- "PROBE" --> L["probe_here → probe_here_raw<br/>đo lại ĐÚNG góc đang đứng<br/>z_measured[worst] = z"]
    L --> M{"need_adjust 0.02<br/>|z_measured − ref| <= 0.02 ?"}
    M -- "chưa đạt" --> J
    M -- "đạt + AUTO ON" --> N["TR_AUTO"]
    M -- "đạt + AUTO OFF" --> J

    K -- "DONE" --> O["done<br/>z_accepted[worst] = true"]
    O --> P{"còn góc khác cần chỉnh ?"}
    P -- "còn + AUTO ON" --> Q["goto_index góc kế tiếp<br/>di XY + probe"]
    P -- "còn + AUTO OFF" --> J
    P -- "hết" --> R["restart<br/>home lại + đo lại 4 góc"]
    Q --> J
    R --> B

    K -- "nhãn BR / BL / FL / FR" --> S["goto_corner i<br/>di XY tới góc i + probe"]
    S --> J

    K -- "MAX/MIN" --> T["Hộp XÁC NHẬN<br/>OK / CANCEL"]
    T -- OK --> U["toggle_mode_restart<br/>đổi MAX ↔ MIN rồi restart"]
    T -- CANCEL --> J
    U --> B

    K -- "AUTO" --> V["toggle_auto<br/>bật/tắt tự động di chuyển"]
    V --> J

    N --> W{"còn góc cần chỉnh ?"}
    W -- "còn" --> Q
    W -- "hết" --> R
```

## 2. Mốc so sánh và ngưỡng

```mermaid
flowchart LR
    subgraph DISPLAY["4 số hiển thị trên màn hình"]
        D1["display_ref = 0"] --> D2["số hiện = z_measured<br/>giá trị probe ĐO ĐƯỢC, không trừ mốc"]
    end
    subgraph CALC["Tính khoảng cách và chiều vặn"]
        C1["ref = z_ref_max ở chế độ MAX<br/>ref = z_ref_min ở chế độ MIN"] --> C2["delta = z_measured[i] − ref"]
        C2 --> C3["khoảng cách = |delta|"]
        C2 --> C4["chiều: delta < 0 → CW<br/>delta > 0 → CCW"]
    end
    subgraph TOL["Ngưỡng đạt"]
        T1["sau khi đo 4 góc: 0.05mm<br/>TBT_DONE_INIT_MM"] --> T3["chỉ dùng ở TR_PROBE"]
        T2["khi đang chỉnh: 0.02mm<br/>TBT_DONE_MM"] --> T4["dùng ở need_adjust / next_need_adjust"]
    end
```

## 3. Máy trạng thái

```mermaid
stateDiagram-v2
    [*] --> TR_HOMING : begin / restart
    TR_HOMING --> TR_PROBE : all_axes_homed
    TR_PROBE --> TR_PROBE : probe từng góc, step_index++
    TR_PROBE --> TR_IDLE : đủ 4 góc và all_dat = true
    TR_PROBE --> TR_GOTO : đủ 4 góc, còn góc cần chỉnh, AUTO ON
    TR_PROBE --> TR_IDLE : đủ 4 góc, còn góc cần chỉnh, AUTO OFF
    TR_GOTO --> TR_IDLE : goto_worst xong
    TR_IDLE --> TR_IDLE : PROBE / DONE / bấm nhãn góc
    TR_IDLE --> TR_AUTO : PROBE lại và đã đạt, AUTO ON
    TR_AUTO --> TR_POINT : next_need_adjust còn góc
    TR_POINT --> TR_IDLE : goto_index xong
    TR_POINT --> TR_AUTO : góc vừa tới cũng đạt
    TR_AUTO --> TR_HOMING : hết góc cần chỉnh, restart
    TR_HOMING --> [*] : exit
```

## 4. Đường đi của một con số

```mermaid
flowchart LR
    P["probe_xy<br/>probe_at_point trả về z"] --> Q["z_measured[i] = z"]
    Q --> R["measured i<br/>accessor"]
    R --> S["st.d[i]<br/>tbt_report.cpp"]
    S --> T["dòng TBT<br/>tbt_protocol.h"]
    T --> U["tbt.st.d[i]<br/>CYD parse"]
    U --> V["vẽ lên màn<br/>ui_page_tbt.cpp"]
    R --> W["need_adjust / goto_worst / hint<br/>dùng thêm ref"]
```

## 5. Chặn an toàn và điều kiện biên

```mermaid
flowchart TD
    X["Mọi hàm có di chuyển"] --> Y{"can_move_now<br/>= all_axes_homed ?"}
    Y -- "chưa home" --> Z["TỪ CHỐI<br/>in 'TU CHOI di chuyen - truc CHUA HOME'"]
    Y -- "đã home" --> AA["di chuyển bình thường"]
    AA --> AB{"auto_move ?"}
    AB -- "ON" --> AC["cho phép tự nhảy góc"]
    AB -- "OFF" --> AD["chỉ di chuyển khi người dùng bấm"]
```

## Bảng đối chiếu nhanh

| Khái niệm | Biến / hằng | Ý nghĩa |
|---|---|---|
| Số đo thô | `z_measured[i]` | probe trả về bao nhiêu, lưu bấy nhiêu |
| Mốc tính toán | `ref()` | Z-MAX ở chế độ MAX, Z-MIN ở chế độ MIN |
| Mốc hiển thị | `display_ref()` | luôn 0 → hiện đúng số đo thô |
| Sai lệch | `z_measured[i] − ref()` | dương = cao hơn mốc, âm = thấp hơn mốc |
| Ngưỡng sau 4 góc | `TBT_DONE_INIT_MM` = 0.05 | lệch nhỏ hơn coi như đạt luôn |
| Ngưỡng khi chỉnh | `TBT_DONE_MM` = 0.02 | phải vặn cho tới mức này |
| Chiều vặn | `tramming_turn_cw()` | delta âm → CW, dương → CCW |
| Tự động nhảy góc | `auto_move` | bật/tắt bằng nút AUTO |
| Chuỗi Z trước probe | `Z_CLEARANCE_BETWEEN_PROBES` = 5mm | duy nhất một lệnh, ở mọi đường |
| Bed mesh | tắt/bật quanh `probe_at_point` | đo không bị bù theo toạ độ XY |
| Chặn an toàn | `can_move_now()` | chỉ cho di chuyển khi đã home |
