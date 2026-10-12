# docs/issues — Issue nháp của repo này

Các file ở đây là **bản nháp issue** để tạo trên GitHub (`tungbuivn/Marlin`) khi máy không có
`gh` CLI / token. Mỗi file ghi rõ: tiêu đề, nhãn gợi ý, nội dung, và lệnh tạo issue.

| # | File | Nội dung |
|---|---|---|
| 1 | `0001-panel-serial-ui-tat-custom-menu.md` | Bật `PANEL_SERIAL_UI` (màn hình riêng) phải tắt `CUSTOM_MENU_MAIN` + `LED_CONTROL_MENU` → cần cơ chế đẩy danh sách menu tuỳ biến từ Marlin xuống panel để thêm lại |

Tạo issue từ nháp:

```powershell
winget install --id GitHub.cli
gh auth login
gh issue create --repo tungbuivn/Marlin `
  --title "<tiêu đề trong file>" --body-file docs/issues/<file>.md --label enhancement
```
